/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp-core/ng/config/config.h
 *
 * Generic key/value configuration engine.
 *
 * This header is C++17 — no modules, no domain types, no std::function
 * where a plain function pointer suffices in Key<T> itself.  Domain key
 * tokens (Key<T> instances) are defined at higher tiers (zyppng:contextconfig)
 * and registered into Config before the first parse() call.
 *
 * Keys are identified by canonical string names of the form "section/keyname".
 * All internal maps use std::string keys (owned copies) so dynamic key names
 * are safe and no string_view lifetime discipline is required from callers.
 */
#ifndef ZYPP_NG_CONFIG_CONFIG_H
#define ZYPP_NG_CONFIG_CONFIG_H

#include <any>
#include <zypp-core/Pathname.h>
#include <functional>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <zypp-core/ng/base/signals.h>
#include <zypp-core/ng/meta/type_traits.h>
#include <zypp-core/ng/pipelines/expected.h>

namespace zypp::parser { class IniDict; }

namespace zyppng::config {

// Forward declaration — needed by Key<T>::defaultFn signature.
class Config;

// ---------------------------------------------------------------------------
// Key<T> — typed config key token
// ---------------------------------------------------------------------------

/**
 * \brief A compile-time token identifying one typed configuration key.
 *
 * Instances are declared as \c inline \c constexpr globals at the tier that
 * knows about \c T (e.g. \c zyppng::config::SystemArchitecture).  The
 * \c Config engine itself is domain-agnostic and never sees \c T directly
 * after registration.
 *
 * Plain function pointers are used for \c defaultFn and \c parseFn to keep
 * \c Key<T> constexpr-constructible and avoid heap allocation in the token
 * itself.  The registry stores type-erased \c std::function wrappers (one-time
 * cost at \c registerKey() time).
 *
 * \par Fields
 * - \c name         Canonical key name: \c "section/keyname".
 * - \c legacyAlias  Old zypp.conf key (e.g. \c "download.transfer_timeout").
 *                   Empty \c string_view if no alias needed.
 * - \c defaultFn    Returns the compiled-in default.  Receives \c Config& so
 *                   cross-key defaults (e.g. UseDeltarpmAlways masked by
 *                   UseDeltarpm) are expressible.
 * - \c parseFn      Converts a raw zypp.conf string value to \c T, or returns
 *                   a \c zyppng::unexpected wrapping a \c std::exception_ptr
 *                   describing the parse failure.  Parse errors are logged as
 *                   warnings by \c Config::parse(); the key falls through to
 *                   its \c defaultFn — no silent data corruption.
 * - \c serializeFn  Converts a \c T value back to a string.  Must round-trip
 *                   with \c parseFn.  Used by \c Config::serialize() to produce
 *                   the INI blob sent to worker processes.
 */
template<typename T>
struct Key {
    std::string_view  name;
    std::string_view  legacyAlias;
    T (*defaultFn)(const Config &);
    expected<T> (*parseFn)(std::string_view);
    std::string (*serializeFn)(const T &);
};

// ---------------------------------------------------------------------------
// ParseError — one failed key parse during Config::parse()
// ---------------------------------------------------------------------------

/**
 * \brief Describes a single key parse failure encountered during parse().
 *
 * The \c Config is always fully constructed regardless of parse errors —
 * keys that fail to parse fall through to their compiled-in default.
 * The caller decides how to handle the error list.
 */
struct ParseError {
    std::string        key;       ///< Canonical key name, e.g. "main/arch"
    std::string        rawValue;  ///< The raw string from zypp.conf that failed
    std::exception_ptr error;     ///< The exception describing the failure
};

// ---------------------------------------------------------------------------
// Entry — per-key storage cell
// ---------------------------------------------------------------------------

/**
 * \brief Internal storage cell for one config key.
 *
 * Both \c fileValue and \c overrideValue use \c std::any whose default-
 * constructed state (\c has_value()==false) represents "not set".
 * \c std::optional<std::any> would be redundant.
 *
 * \c signal is lazy — the \c sigc::signal object is constructed only the
 * first time \c Config::sigChanged() is called for this key.
 */
struct Entry {
    std::any fileValue;      ///< Populated by parse(). Empty = absent from file.
    std::any overrideValue;  ///< Populated by set(). Empty = no programmatic override.
    mutable std::optional<sigc::signal<void(const std::any &)>> signal; ///< Lazy.
};

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------

/**
 * \brief Generic key/value configuration engine.
 *
 * \par Lifecycle
 * Constructed via one of three static factories.  Non-copyable, move-only.
 * Intended to be owned exclusively by \c zyppng::Context.
 *
 * \par Key registration
 * Domain key tokens (\c Key<T>) are registered via \c registerKey() before
 * the first \c parse() call.  Call \c registerZyppKeys(*this) explicitly
 * before \c parse() — there is no automatic registration.
 * \c Config itself remains domain-agnostic.
 *
 * \par Path keys
 * Filesystem paths are plain \c Key<zypp::Pathname> tokens.  The
 * engine stores and returns them as configured — it never prepends \c root().
 * Callers compose \c root() / \c cfg.get(SomePathKey) themselves when they
 * need a fully-qualified path within a managed root.  This is intentional:
 * the caller knows whether they need the host path or the chroot path.
 * All path values are validated by \c parsePath() which rejects paths that
 * escape the root (normalized \c zypp::Pathname that is \c relativeDotDot(), i.e. starts with \c ./..).
 *
 * \par Lookup order
 * 1. overrideValue  — programmatic \c set() call
 * 2. fileValue      — parsed from zypp.conf
 * 3. defaultFn      — compiled-in default from the key registration
 *                     (may reference other keys via \c Config&)
 *
 * \par Key naming
 * All keys use the canonical form \c "section/keyname" (e.g. \c "main/arch",
 * \c "media/transfer_timeout").  Legacy zypp.conf key names are registered as
 * aliases and resolved transparently during \c parse().
 *
 * \par Change signals
 * \c sigChanged("section/key") returns a lazy \c sigc::signal<void(const std::any&)>
 * emitted by \c set() and \c reset() with the new effective value.
 *
 * \par Dynamic keys
 * Unknown keys encountered during \c parse(), or keys set via the textual
 * \c set(string_view, std::string), are stored as \c std::string values and
 * accessible via \c getRaw().
 *
 * \par Type invariant
 * Registered keys only ever hold their registered type: they are written
 * through \c set(Key<T>,...) (checked against the registry), the textual
 * \c set() (which runs the key's \c parseFn) or \c parse*() (same).
 * Unregistered keys only ever hold \c std::string.  \c serialize() therefore
 * cannot hit a type mismatch.  One documented gap: a key parsed \em before it
 * is registered stays a raw string — always call \c registerKey() first.
 */
class Config {
public:
    // -----------------------------------------------------------------------
    // Constructor
    // -----------------------------------------------------------------------

    /**
     * Construct a Config for \a root.
     * The registry is empty until registerKey() calls are made.
     * Typical usage:
     * \code
     *   Config cfg("/mnt");
     *   registerZyppKeys(cfg);          // populate registry
     *   auto [cfg2, errs] = cfg.parse(); // NOT right — see parse() below
     * \endcode
     * See contextconfig.cc for the canonical factory pattern.
     */
    explicit Config( zypp::Pathname root );

    Config(const Config &)             = delete;
    Config & operator=(const Config &) = delete;
    Config(Config &&)                  = default;
    Config & operator=(Config &&)      = default;

    // -----------------------------------------------------------------------
    // Root
    // -----------------------------------------------------------------------

    const zypp::Pathname & root() const { return _root; }

    // -----------------------------------------------------------------------
    // Typed access via Key<T> token (compile-time type safety)
    // -----------------------------------------------------------------------

    /**
     * Return the effective value for \a key.
     * Lookup: overrideValue → fileValue → registered defaultFn(*this).
     * Throws std::out_of_range if the key is not registered.
     */
    template<typename T>
    T get(const Key<T> & key) const { return getTyped<T>(key.name); }

    /**
     * Set a programmatic override for \a key.
     * The value parameter is a non-deduced context: \c T comes from the key
     * only, so \c cfg.set(SomeStringKey,"literal") converts the literal.
     * Throws std::logic_error if \a key was never registered (programmer
     * error) — this keeps the invariant that registered keys hold only their
     * registered type and unregistered keys hold only strings.
     * Emits sigChanged with the new value if a signal has been connected.
     */
    template<typename T>
    void set(const Key<T> & key, type_identity_t<T> value) { setTyped<T>(key.name, std::move(value)); }

    template<typename T>
    void reset(const Key<T> & key) { reset(key.name); }

    /**
    * Equivalent to the legacy ZConfig::builtin*Path() accessors.
    * Useful when a caller needs to know "what did zypp.conf say?" independently
    * of any runtime set() call — e.g. to distinguish a standard cache location
    * from a user-specified override before performing auto-cleanup.
    */
   template<typename T>
   T getBaseline(const Key<T> & key) const { return getBaselineTyped<T>(key.name); }

    /**
     * Clear the programmatic override for \a sectionKey.
     * Effective value reverts to fileValue or defaultFn.
     * Emits sigChanged with the new effective value if a signal is connected.
     */
    void reset(std::string_view sectionKey);

    /** Clear all programmatic overrides. */
    void resetAll();

    // -----------------------------------------------------------------------
    // Textual access (preview features, dynamic/unknown keys, CLI overrides)
    // -----------------------------------------------------------------------

    /**
     * Return the raw string value for any key.
     * Returns std::nullopt if no value has been set (file or override).
     * For typed keys this returns the string representation only if the
     * stored value is a std::string (i.e. unknown/raw keys).
     * For preview features: cfg.getRaw("preview/single_rpmtrans").
     */
    std::optional<std::string> getRaw(std::string_view sectionKey) const;

    /**
     * Set a programmatic override from a textual value.
     * \a sectionKey is a canonical "section/key" name or a registered legacy alias.
     *  - Registered key: \a value is converted via the key's parseFn and stored
     *    as the typed override (clamping etc. applies as for zypp.conf values).
     *    On a parse failure the error is returned and the value is unchanged.
     *  - Unregistered key: \a value is stored as a raw string (always succeeds).
     * Unregistered keys must be of the form "section/key".
     * Emits sigChanged with the stored value if a signal has been connected.
     */
    expected<void> set(std::string_view sectionKey, std::string value);

    // -----------------------------------------------------------------------
    // Change signals
    // -----------------------------------------------------------------------

    /**
     * Return the sigc::signal for \a sectionKey, creating it lazily.
     * Emitted by set(), reset() with the new effective value as std::any.
     *
     * Usage:
     * \code
     *   cfg.sigChanged("main/arch").connect(sigc::ptr_fun(mySlot));
     * \endcode
     */
    sigc::signal<void(const std::any &)> & sigChanged(std::string_view sectionKey);

    // -----------------------------------------------------------------------
    // Iteration
    // -----------------------------------------------------------------------

    /**
     * Visit every key that has a file or override value (not default-only keys).
     */
    void forEach(std::function<void(std::string_view, const Entry &)> f) const;

    /** Returns true if \a sectionKey has a registered parseFn (i.e. was registered via registerKey()). */
    bool hasRegisteredKey(std::string_view sectionKey) const;

    // -----------------------------------------------------------------------
    // Key registration
    // -----------------------------------------------------------------------

    /**
     * Register a typed key token.
     * Stores type-erased defaultFn and parseFn in the registry, and
     * registers the legacyAlias → canonical name mapping.
     * Must be called before the first parse() invocation.
     */
    template<typename T>
    void registerKey(const Key<T> & key) {
        KeyMeta meta;
        meta.defaultFn = [fn = key.defaultFn](const Config & c) -> std::any {
            return std::any(fn(c));
        };
        meta.parseFn = [fn = key.parseFn](std::string_view s) -> expected<std::any> {
            auto r = fn(s);
            if ( !r ) return unexpected( r.error() );
            return std::any( std::move(r.get()) );
        };
        meta.serializeFn = [fn = key.serializeFn](const std::any & v) -> std::string {
            return fn(std::any_cast<const T &>(v));
        };
        _registry[std::string(key.name)] = std::move(meta);
        if (!key.legacyAlias.empty())
            _aliasMap[std::string(key.legacyAlias)] = std::string(key.name);
    }

    // -----------------------------------------------------------------------
    // Parse
    // -----------------------------------------------------------------------

    /**
     * Read zypp.conf from root() using EconfDict (UAPI three-layer merge).
     * Respects the $ZYPP_CONF env var (plain IniDict on that single file).
     * Registered parseFn is called per known key; unknown keys stored as
     * raw std::string. Keys that fail to parse fall through to defaultFn.
     * Returns the list of parse errors (empty on full success).
     *
     * Call registerZyppKeys(*this) before parse() — keys not in the registry
     * are stored as raw strings and never type-converted.
     */
    std::vector<ParseError> parse();

    /**
     * Parse config values from an INI string (as produced by serialize()).
     * Equivalent to parse() but reads from a string buffer rather than disk.
     * Used by worker processes to reconstruct the parent's config.
     */
    std::vector<ParseError> parseFromString(std::string_view iniContent);

    /**
     * Serialize all effective values (override → file → defaultFn) to INI
     * text suitable for sending to worker processes or external plugins.
     *
     * Every registered key is emitted using its effective value via serializeFn.
     * Unknown/raw keys (preview features, custom keys) are also emitted as-is.
     * Keys are grouped by section; sections and keys within each section are
     * sorted alphabetically for deterministic output.
     *
     * The output round-trips through parseFromString():
     *   Config worker(root);
     *   registerZyppKeys(worker);
     *   worker.parseFromString(cfg.serialize());
     */
    std::string serialize() const;

    /**
     * Effective value of every registered key plus every raw/unknown key,
     * canonical "section/keyname" -> serialized string. Same value rules as
     * serialize(). Used for transports that cannot carry multi-line INI text
     * (e.g. one STOMP header per key).
     */
    std::map<std::string, std::string> serializeEntries() const;

    /**
     * Parse canonical-keyed entries (as produced by serializeEntries()).
     * Same semantics as parse(): registered keys go through parseFn (errors
     * collected), unknown keys are stored as raw std::string. Keys without a
     * "section/" prefix are skipped.
     */
    std::vector<ParseError> parseEntries(const std::map<std::string, std::string> & entries);

private:
    zypp::Pathname _root;

    // Main value storage — canonical "section/keyname" → Entry.
    std::unordered_map<std::string, Entry> _entries;

    // Key registry — populated by registerKey() before parse().
    struct KeyMeta {
        std::function<std::any(const Config &)>              defaultFn;
        std::function<expected<std::any>(std::string_view)>  parseFn;
        std::function<std::string(const std::any &)>         serializeFn;
    };
    std::unordered_map<std::string, KeyMeta> _registry;  // canonical → meta
    std::unordered_map<std::string, std::string> _aliasMap; // legacy → canonical

    /** Parse all entries from \a dict into _entries. Returns parse errors. */
    std::vector<ParseError> parseDict(const zypp::parser::IniDict & dict);

    /**
     * Parse one entry: alias resolution → registry lookup → parseFn (errors
     * appended to \a errors) or raw string storage for unknown keys.
     */
    void parseEntry(const std::string & section, const std::string & key,
                    const std::string & value, std::vector<ParseError> & errors);

    /**
     * Resolve "section" + "key" to the canonical name: first the section-qualified
     * form in the alias map, then the bare key as a legacy flat alias, else
     * the plain "section/key" concatenation.
     */
    std::string resolveCanonical(const std::string & section, const std::string & key) const;

    /** Store \a value as the override of \a canonical and emit sigChanged if connected. */
    void storeOverride(const std::string & canonical, std::any value);

    // -----------------------------------------------------------------------
    // Typed string-keyed helpers — private: the public typed API is Key<T>
    // only, so a caller cannot store a value of the wrong type. Named
    // differently from the public set()/get() so overload resolution can never
    // select them for e.g. cfg.set("main/x", "literal").
    // -----------------------------------------------------------------------

    /**
     * Effective value: overrideValue → fileValue → registered defaultFn(*this).
     * Throws std::bad_any_cast on a type mismatch, std::out_of_range if unregistered.
     */
    template<typename T>
    T getTyped(std::string_view sectionKey) const {
        const std::string key(sectionKey);
        auto it = _entries.find(key);
        if (it != _entries.end()) {
            if (it->second.overrideValue.has_value())
                return std::any_cast<T>(it->second.overrideValue);
            if (it->second.fileValue.has_value())
                return std::any_cast<T>(it->second.fileValue);
        }
        // Fall through to registered default.
        auto rIt = _registry.find(key);
        if (rIt != _registry.end() && rIt->second.defaultFn)
            return std::any_cast<T>(rIt->second.defaultFn(*this));
        throw std::out_of_range(std::string("Config::get — unknown key: ") + key);
    }

    /** Override value; throws std::logic_error if \a sectionKey is not registered. */
    template<typename T>
    void setTyped(std::string_view sectionKey, T value) {
        const std::string key(sectionKey);
        if (_registry.find(key) == _registry.end())
            throw std::logic_error(std::string("Config::set — key is not registered: ") + key);
        storeOverride(key, std::any(std::move(value)));
    }

    /** fileValue → registered defaultFn(*this); the override is skipped. */
    template<typename T>
    T getBaselineTyped(std::string_view sectionKey) const {
        const std::string key(sectionKey);
        auto it = _entries.find(key);
        if (it != _entries.end() && it->second.fileValue.has_value())
            return std::any_cast<T>(it->second.fileValue);
        auto rIt = _registry.find(key);
        if (rIt != _registry.end() && rIt->second.defaultFn)
            return std::any_cast<T>(rIt->second.defaultFn(*this));
        throw std::out_of_range(std::string("Config::getBaseline — unknown key: ") + key);
    }
};

} // namespace zyppng::config

#endif // ZYPP_NG_CONFIG_CONFIG_H
