/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include <zypp-core/ng/config/config.h>

#include <cstdlib>
#include <map>
#include <sstream>
#include <stdexcept>

#include <zypp-core/base/LogControl.h>
#include <zypp-core/base/inputstream.h>
#include <zypp-core/Pathname.h>
#include <zypp-core/fs/PathInfo.h>
#include <zypp-core/parser/IniDict>
#include <zypp-core/parser/EconfDict>

namespace zyppng::config {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

Config::Config( std::filesystem::path root )
    : _root(std::move(root))
{}

// ---------------------------------------------------------------------------
// reset / resetAll
// ---------------------------------------------------------------------------

void Config::reset(std::string_view sectionKey)
{
    const std::string k(sectionKey);
    auto it = _entries.find(k);
    if (it == _entries.end())
        return;

    it->second.overrideValue.reset();

    if (!it->second.signal)
        return;

    // Compute the new effective value and copy it before emitting — a slot
    // that inserts a new entry into _entries would trigger a rehash and leave
    // a reference into the map dangling for the duration of emit().
    std::any newValue;
    if (it->second.fileValue.has_value()) {
        newValue = it->second.fileValue;
    } else {
        auto rIt = _registry.find(k);
        if (rIt != _registry.end() && rIt->second.defaultFn)
            newValue = rIt->second.defaultFn(*this);
    }
    it->second.signal->emit(newValue);
}

void Config::resetAll()
{
    // Snapshot keys first: reset() emits signals whose slots may insert new
    // entries into _entries, invalidating a range-for iterator over it.
    std::vector<std::string> keys;
    keys.reserve(_entries.size());
    for (const auto & [name, _] : _entries)
        keys.push_back(name);
    for (const auto & name : keys)
        reset(name);
}

// ---------------------------------------------------------------------------
// Raw string access
// ---------------------------------------------------------------------------

std::optional<std::string> Config::getRaw(std::string_view sectionKey) const
{
    const std::string k(sectionKey);
    auto it = _entries.find(k);
    if (it == _entries.end())
        return std::nullopt;

    // Check override first, then file value.
    for (const std::any * v : { &it->second.overrideValue, &it->second.fileValue }) {
        if (!v->has_value())
            continue;
        // Stored as std::string (raw / unknown key).
        if (const auto * s = std::any_cast<std::string>(v))
            return *s;
        // Stored as a typed value — not accessible as raw string.
        return std::nullopt;
    }
    return std::nullopt;
}

void Config::setRaw(std::string_view sectionKey, std::string value)
{
    const std::string k(sectionKey);
    auto & entry = _entries[k];
    entry.overrideValue = std::any(std::move(value));
    if (entry.signal) {
        std::any copy = entry.overrideValue;
        entry.signal->emit(copy);
    }
}

// ---------------------------------------------------------------------------
// Change signals
// ---------------------------------------------------------------------------

sigc::signal<void(const std::any &)> & Config::sigChanged(std::string_view sectionKey)
{
    auto & entry = _entries[std::string(sectionKey)];
    if (!entry.signal)
        entry.signal.emplace();
    return *entry.signal;
}

// ---------------------------------------------------------------------------
// Iteration
// ---------------------------------------------------------------------------

void Config::forEach(std::function<void(std::string_view, const Entry &)> f) const
{
    for (const auto & [name, entry] : _entries) {
        if (entry.fileValue.has_value() || entry.overrideValue.has_value())
            f(name, entry);
    }
}

bool Config::hasRegisteredKey(std::string_view sectionKey) const
{
    return _registry.count(std::string(sectionKey)) != 0;
}
//
// ---------------------------------------------------------------------------
// parseDict() — shared INI-dict-to-entries logic
// ---------------------------------------------------------------------------

std::vector<ParseError> Config::parseDict(const zypp::parser::IniDict & dict)
{
    std::vector<ParseError> errors;

    for (auto sIt = dict.sectionsBegin(); sIt != dict.sectionsEnd(); ++sIt) {
        const std::string & section = *sIt;

        for (auto eIt = dict.entriesBegin(section);
             eIt != dict.entriesEnd(section); ++eIt)
        {
            const std::string & rawKey   = eIt->first;
            const std::string & rawValue = eIt->second;

            // Assemble canonical name: "section/key"
            std::string canonical = section + "/" + rawKey;

            // Check legacy alias remapping.
            // First try the section-qualified form ("main/arch").
            {
                auto aIt = _aliasMap.find(canonical);
                if (aIt != _aliasMap.end()) {
                    canonical = aIt->second;
                } else {
                    // Fall back to the bare key name for legacy flat aliases
                    // (e.g. "arch" → "main/arch" registered without a section prefix).
                    auto aIt2 = _aliasMap.find(rawKey);
                    if (aIt2 != _aliasMap.end())
                        canonical = aIt2->second;
                }
            }

            auto rIt = _registry.find(canonical);
            if (rIt != _registry.end() && rIt->second.parseFn) {
                // Known key — convert via registered parseFn.
                auto result = rIt->second.parseFn(rawValue);
                if ( result ) {
                    _entries[canonical].fileValue = std::move( result.get() );
                } else {
                    // Collect error — fileValue left empty, key falls through
                    // to defaultFn at get() time.
                    errors.push_back( ParseError{ canonical, rawValue, result.error() } );
                }
            } else {
                // Unknown key — store as raw string.
                _entries[canonical].fileValue = std::any(rawValue);
                DBG << "Config::parse — unknown key '" << canonical
                    << "' stored as raw string" << std::endl;
            }
        }
    }

    return errors;
}

// ---------------------------------------------------------------------------
// parse()
// ---------------------------------------------------------------------------

std::vector<ParseError> Config::parse()
{
    // $ZYPP_CONF override — single file, plain IniDict.
    const char * zyppConf = ::getenv("ZYPP_CONF");

    std::unique_ptr<zypp::parser::IniDict> dict;
    if (zyppConf && *zyppConf) {
        const zypp::Pathname confPath = zypp::Pathname(_root.string()) / zyppConf;
        if (zypp::PathInfo(confPath).isFile()) {
            dict = std::make_unique<zypp::parser::IniDict>();
            dict->read(confPath);
        } else {
            WAR << "ZYPP_CONF set but file not found: " << confPath << std::endl;
            dict = std::make_unique<zypp::parser::IniDict>();
        }
    } else {
        // Standard UAPI three-layer merge via EconfDict.
        dict = std::make_unique<zypp::parser::EconfDict>(
            "zypp/zypp.conf", zypp::Pathname(_root.string()));
    }

    return parseDict(*dict);
}

// ---------------------------------------------------------------------------
// parseFromString()
// ---------------------------------------------------------------------------

std::vector<ParseError> Config::parseFromString(std::string_view iniContent)
{
    // InputStream takes std::istream& — must be a named lvalue, not a temporary.
    std::istringstream iss( (std::string(iniContent)) );
    zypp::parser::IniDict dict;
    dict.read( zypp::InputStream(iss) );
    return parseDict(dict);
}

// ---------------------------------------------------------------------------
// serialize()
// ---------------------------------------------------------------------------

std::string Config::serialize() const
{
    // Build a sorted section → sorted key → value map so that:
    // - Each section appears exactly once
    // - Typed and raw keys in the same section are merged together
    // - Output is deterministic (alphabetically sorted)
    std::map<std::string, std::map<std::string, std::string>> sections;

    // Pass 1 — all registered keys: emit effective value via serializeFn.
    for (const auto & [canonicalName, meta] : _registry) {
        if (!meta.serializeFn || !meta.defaultFn)
            continue;

        const auto slash = canonicalName.find('/');
        if (slash == std::string::npos)
            continue;

        const std::string section = canonicalName.substr(0, slash);
        const std::string keyname = canonicalName.substr(slash + 1);

        // Effective value: override → file → default
        std::any effectiveValue;
        auto eIt = _entries.find(canonicalName);
        if (eIt != _entries.end() && eIt->second.overrideValue.has_value())
            effectiveValue = eIt->second.overrideValue;
        else if (eIt != _entries.end() && eIt->second.fileValue.has_value())
            effectiveValue = eIt->second.fileValue;
        else
            effectiveValue = meta.defaultFn(*this);

        sections[section][keyname] = meta.serializeFn(effectiveValue);
    }

    // Pass 2 — raw/unknown keys not in the registry.
    for (const auto & [canonicalName, entry] : _entries) {
        if (_registry.count(canonicalName))
            continue;  // already handled in pass 1

        const std::any & v = entry.overrideValue.has_value()
                             ? entry.overrideValue : entry.fileValue;
        if (!v.has_value() || v.type() != typeid(std::string))
            continue;

        const auto slash = canonicalName.find('/');
        if (slash == std::string::npos)
            continue;

        sections[canonicalName.substr(0, slash)][canonicalName.substr(slash + 1)]
            = std::any_cast<const std::string &>(v);
    }

    // Render to INI text.
    std::ostringstream out;
    for (const auto & [section, keys] : sections) {
        out << '[' << section << "]\n";
        for (const auto & [keyname, value] : keys)
            out << keyname << " = " << value << '\n';
        out << '\n';
    }

    return out.str();
}

} // namespace zyppng::config
