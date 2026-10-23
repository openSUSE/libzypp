# 03 – The sat Pool and the Resolvable Object Model

This chapter covers how libzypp wraps libsolv: the single global libsolv pool (`sat::Pool` / `sat::detail::PoolImpl`), the cheap id-based value types built on it (`IdString`, `Capability`, `Edition`, `Arch`, `ResKind`, `sat::Solvable`), and the object layer on top (`ResPool`, `PoolItem`, `ResStatus`, `ResObject` and its subclasses, `ui::Selectable`). It explains how the pool gets filled, how the serial-number "dirty" mechanism keeps the derived layers consistent, and the special cases around locales, locks, vendors, products, retracted packages and PTFs.

Solver behaviour is covered in [Solver](04-solver.md), repo caches in [Repositories](05-repositories-and-services.md), and `zypp.conf` options (multiversion, vendors, locks file) in [Configuration](09-configuration-and-environment.md).

---

## 1. Where the code lives

The "legacy" pool layer is spread over two trees. Always check both.

| Area | Location |
|---|---|
| libsolv C-type aliases, ids (`noId`, `emptyId`, markers, namespaces) | `zypp-logic/zypp/sat/detail/PoolDefines.h` |
| The single libsolv `Pool*` (string pool owner) | `zypp-logic/zypp/ng/sat/stringpool.{h,cc}` (`zyppng::sat::StringPool`, tagged `// LEGACY-REQUIRED`) |
| `IdString`, `IdStringType<>` | `zypp-logic/zypp/IdString.{h,cc}`, `IdStringType.h` |
| `Edition`, `Rel`, `Range`, `RelCompare`, `CapMatch` | `zypp-logic/zypp/` |
| `Arch` (+ compat tables, system arch detection) | `zypp-logic/zypp/Arch.{h,cc}` |
| `ResKind`, `ResTraits` | `zypp-logic/zypp/ResKind.{h,cc}`, `ResTraits.h` |
| `Locale`, `LanguageCode`, `CountryCode`, `CpeId` | `zypp-logic/zypp/` |
| `SolvAttr` | `zypp-logic/zypp/sat/SolvAttr.{h,cc}` |
| `SerialNumber`, `SetTracker`, `StrMatcher` | `zypp-logic/zypp/base/` |
| `sat::Pool`, `PoolImpl`, `PoolMember` | `zypp/zypp/sat/Pool.*`, `zypp/zypp/sat/detail/PoolImpl.*`, `PoolMember.*` |
| `Solvable`, `SolvableSet`, `SolvableSpec`, `SolvableType`, `SolvIterMixin` | `zypp/zypp/sat/` |
| `WhatProvides`, `WhatObsoletes`, `LookupAttr`, `Map`, `Queue`, `Transaction`, `FileConflicts`, `LocaleSupport` | `zypp/zypp/sat/` |
| `Capability`, `CapDetail`, `Capabilities` | `zypp/zypp/Capability.*`, `Capabilities.*` |
| `Repository` (sat repo handle) | `zypp/zypp/Repository.*` |
| `ResPool`, `PoolItem`, `ResStatus`, `ResPoolProxy` | `zypp/zypp/` |
| `pool::PoolImpl`, `PoolTraits`, `ByIdent` | `zypp/zypp/pool/` |
| `Resolvable`, `ResObject`, `Package`, `Patch`, `Pattern`, `Product`, `SrcPackage`, `Application` | `zypp/zypp/` |
| `ui::Selectable`, `ui::Status`, `SelectableTraits` | `zypp/zypp/ui/` |
| `PoolQuery`, `Locks`, `VendorAttr` | `zypp/zypp/` |

Under the tier rules in `AGENTS.md`, everything here is **frozen** legacy/bridge code. You fix bugs here but don't add features. `zypp-logic/zypp/ng/sat/stringpool.*` is the only piece in an `ng/` subtree. It has to stay in place because `IdString`, `Capability` and `Edition` all go through it.

---

## 2. One libsolv pool, two owners

```
                zyppng::sat::StringPool::instance()   (function-local static)
                  └── ::pool_create(); ::pool_setdisttype(DISTTYPE_RPM)
                        ▲                         ▲
   IdString / Capability / Edition         sat::detail::PoolImpl  (global static in PoolMember.cc)
   (string <-> Id, evrcmp, rel2id)         _pool = StringPool::instance().getPool()
                                           nscallback, debug callback, serials, locales,
                                           repoinfos, multiversion, specs ...
                        ▲
            sat::Pool (thin handle, PoolMember::myPool())
                        ▲
            pool::PoolImpl  (ResPool::instance(): PoolItem store)
                        ▲
            ResPool / ResPoolProxy / ui::Selectable
```

* `StringPool` (`zypp-logic/zypp/ng/sat/stringpool.cc`) creates **the** libsolv `Pool`. It calls `pool_setdisttype(DISTTYPE_RPM)` straight away, because without a disttype, `pool_evrcmp_str` would use whatever flavour libsolv was compiled with (libzypp#726: even on Debian, rpm comparison rules must apply). The comment notes that libzypp supports only one pool until libsolv can share a string pool between pools. **Do not assume this pool is also the solving pool forever.**
* `sat::detail::PoolImpl` (`zypp/zypp/sat/detail/PoolImpl.cc`) borrows that same `Pool*`. Its constructor:
  * sets libsolv debugging. `LIBSOLV_DEBUGMASK=<n>` sets the raw mask. Otherwise `ZYPP_LIBSOLV_FULLLOG` / `ZYPP_LIBSAT_FULLLOG` select level 3, `ZYPP_FULLLOG` selects level 2, and the default is `SOLV_DEBUG_JOB|SOLV_DEBUG_STATS`. Output goes through `logSat()` to log group `libsolv` (errors → ERR, stats → DBG, the rest → MIL). Some noisy lines ("job: drop orphaned", "job: multiversion", …) are filtered out.
  * installs the namespace callback `nsCallback` (§9).
  * sets `_retractedSpec.addIdenticalInstalledToo(true)` (§11).
* `PoolMember` (`zypp/zypp/sat/detail/PoolMember.cc`) is a mixin that gives every sat value type a static `myPool()`. The global `PoolImpl` is a function-local static with a guard object. After static destruction, `myPool()` throws `"Global PoolImpl instance used after it was destroyed!"`, and `poolValid()` lets you test for that case. That guard exists because of past crashes at exit, when other statics touched the pool.
* `PoolImpl.cc` uses `BOOST_MPL_ASSERT_RELATION` to check at compile time that libzypp's redefined constants match libsolv's: `noId==STRID_NULL`, `emptyId==STRID_EMPTY`, `systemSolvableId==SYSTEMSOLVABLE`, the prereq/file markers (15/16), `CAP_AND…CAP_ARCH` vs `REL_*`, and `NAMESPACE_MODALIAS/LANGUAGE/FILESYSTEM` (18/20/21). **A libsolv update that renumbers these breaks the build here, which is intended.**

### 2.1 Ids

`PoolDefines.h`:

| Constant | Value | Meaning |
|---|---|---|
| `noId` | 0 | Null string / no capability (`IdString::Null`, `Capability::Null`) |
| `emptyId` | 1 | Empty string `""` (`IdString::Empty`, default `Capability`) |
| `solvablePrereqMarker` | 15 | Separates requires from pre-requires in a dep array |
| `solvableFileMarker` | 16 | Internal file-provides marker |
| `noSolvableId` / `systemSolvableId` | 0 / 1 | `Solvable::noSolvable`, the hidden libsolv "system solvable" |
| `RepoIdType` | `CRepo*` | A repository id is literally the libsolv `Repo*` pointer |

---

## 3. String pooling: `IdString` and `IdStringType`

`IdString` (`zypp-logic/zypp/IdString.cc`) wraps an `int` id in the libsolv string pool. Construction calls `pool_str2id(..., create=true)`, so every string you build becomes permanent pool content. Strings are never freed until the pool is freed (see `pool_freeallrepos(reuseIds=true)` in §5).

* Equality (`compareEQ`) is just an id compare. Ordering (`compare`) uses `strcmp` on the strings. Null sorts before everything.
* `Null` (id 0) and `Empty` (id 1) compare differently, but both are `empty()`. `operator bool` is `!= Null`.
* `IdStringType<Derived>` is a CRTP helper that gives a class all of `IdString`'s comparisons and conversions. A derived class can override `_doCompare`. Examples:
  * `Edition` uses an rpm evr compare.
  * `ResKind` uses `strcasecmp`, and its constructors lower-case the input.
  * Any class can disable comparison by declaring `_doCompare` without defining it.

Because ids are only meaningful inside the one pool, **never persist raw ids**. The one exception is in-process caches that watch `serialIDs()` (§5).

---

## 4. Value types built on the pool

### 4.1 Edition / Rel / Range

`Edition` (`zypp-logic/zypp/Edition.{h,cc}`) is `[epoch:]version[-release]` stored as one `IdString`.

* `epoch()`, `version()` and `release()` are parsed on demand. The release is whatever follows the **last** `-`.
* `Edition::_doCompare` → `pool_evrcmp_str(..., EVRCMP_COMPARE)`. `_doMatch` → `EVRCMP_MATCH`, where an empty version or release acts as a wildcard. **`match` is not transitive**, so never use it to order a container (header warning).
* rpm semantics: alphanumeric segments are compared. A string segment is less than a number segment (`1.a < 1.0`). Separators are ignored, so `1.0 == 1_0`. The disttype must be RPM, otherwise these rules don't apply (§2).
* `Edition::noedition` is `""`, and it is a valid edition.
* `Edition::MatchRange = Range<Edition, Match>` is used when matching capabilities. `CompareRange` uses `Compare`.

`Rel` (`zypp-logic/zypp/Rel.h`) has enumerator values identical to libsolv's `REL_GT=1`, `REL_EQ=2`, `REL_LT=4`. Derived values: `NE = GT|LT`, `ANY = GT|EQ|LT` (the default), `NONE = 0`. It parses `"="/"=="`, `"!="`, `"<"`, `"<="`, `">"`, `">="`, the names `EQ`…`NONE`, and `""` (→ ANY). `overlaps(Rel, Rel, cmp)` in `zypp-logic/zypp/Range.cc` implements range intersection, with special cases for `NE`, `ANY` and `NONE`.

### 4.2 Arch

`zypp-logic/zypp/Arch.cc`:

* **CompatSet singleton** (`ArchCompatSet`). Each built-in arch gets one bit in a `bit::BitField<uint64_t>`. `noarch` is bit value 0. Bit value 1 is reserved for *all non-built-in archs*, which are only compatible with themselves. `defCompatibleWith(target, a0..a9)` defines the table.
  * The order of definitions matters: `Arch::compare` compares by bit value, so later-defined archs count as "better".
  * There are about 61 built-ins (`DEF_BUILTIN`), close to the 64-bit limit. `nextIdBit()` throws "Need more bits to encode architectures" if the table overflows. **Adding new arches (riscv variants, loong64 v2, …) will hit this eventually.**
  * `defCompatibleWith` takes at most 10 compatible archs. The x86_64_v4 line already uses all 10.
* **System arch detection** (`_autoDetectSystemArchitecture`, cached in `Arch::detectSystemArchitecture()`):
  * Starts from `uname().machine`.
  * `x86_64` is upgraded to `x86_64_v2/v3/v4` using `cpuid` checks copied from rpm's `rpmrc.c`.
  * `i686` is downgraded to `i586` if `/proc/cpuinfo` lacks `cx8` or `cmov` (bug #18885).
  * sparc uses `sun4v/u/m` from `/proc/cpuinfo` (bug #566291).
  * `armv{6,7,8}l` is upgraded to `armv{6,7,8}hl` if `/etc/rpm/platform` starts with `armvXhl-`.
  * `ppc64` becomes `ppc64p7` if `getauxval(AT_PLATFORM)` is `powerN` with N > 6.
* `ZConfig::systemArchitecture()` (`zypp/zypp/ZConfig.cc`) can be overridden by `arch=` in `zypp.conf`, and by **`ZYPP_TESTSUITE_FAKE_ARCH`** (legacy test hook). When that variable is set, `ZYppImpl::commit` refuses to run ("Commit not allowed and disabled").
* `Arch::baseArch()` handles the multilib cases (x86_64, sparc64(v), ppc64, s390x) specially. Otherwise it returns the arch just before `noarch` in the compat set.
* libsolv treats `src` / `nosrc` as architectures. libzypp maps them to `ResKind::srcpackage` (§4.4).

### 4.3 ResKind and kind prefixes

`zypp-logic/zypp/ResKind.cc`: built-ins are `package`, `patch`, `pattern`, `product`, `srcpackage`, `application`, plus `nokind`.

In libsolv, a solvable's *name* (libzypp calls it the **ident**) carries the kind as a prefix:

| Kind | sat ident | How it is detected |
|---|---|---|
| package | `foo` | no `:` in name |
| srcpackage | `foo` | arch `src`/`nosrc` (ident has **no** prefix) |
| patch / pattern / product / application | `patch:foo`, `pattern:foo`, … | prefix |

* `ResKind::explicitBuiltin(str)` is a fast switch on `str[3]`. It only returns a kind if the `kind:` prefix is present, and it guarantees the `:` separator; other code relies on that guarantee.
* `ResKind::satIdent(kind, name)` and `Solvable::SplitIdent` (`zypp/zypp/sat/Solvable.cc`, `_doSplit`) convert between `(kind, name)` and ident. `package:` and `srcpackage:` prefixes are stripped from idents.
* `Solvable::kind()` checks, in order: src/nosrc arch → explicit built-in prefix → no `:` means package → otherwise an unknown `ResKind(prefix)`. `Solvable::name()` strips everything up to the first `:`. A package with a `:` in its name would be misdetected ("hopefully" in the comments).

### 4.4 Capability, CapDetail, Capabilities

`Capability` (`zypp/zypp/Capability.cc`) is just a libsolv dependency `Id`: either a plain string id or a "reldep" (`ISRELDEP`). Forms:

```
NAMED       name            or  name.arch          (REL_ARCH reldep)
VERSIONED   name[.arch] op edition                   (REL_GT/EQ/LT bits)
EXPRESSION  rich deps (and/or/if/unless/else/with/without), namespace:..(..), REL_ARCH, ...
```

**String parsing** (`Capability(const std::string&, ResKind prefix = {}, CtorFlag = UNPARSED)`):

1. If the string starts with `(`, try `pool_parserpmrichdep` (via `StringPool`). If that fails, fall back to the classic parser. **In doubt, the classic parser always yields a NAMED cap.** That is why user search strings such as `'/gcc[0-9]+/ >= 8'` work with `PoolQuery`.
2. `splitOpEdition`:
   * First look for a trailing `name op edition` separated by whitespace.
   * Otherwise look for an embedded op (`<`, `<=`, `=`, `==`, `!=`, `>`, `>=`) that is not inside `()`, e.g. `foo>=1.0`.
   * Passing `PARSED` skips this step.
3. `name.arch` is split off only if the suffix is a **built-in** arch or `src`/`nosrc`. So `foo.bar` stays the name `foo.bar`.
4. `srcpackage:foo`, or kind `srcpackage`, is encoded as `foo REL_ARCH src`. Other kinds get the `kind:` prefix (an explicit prefix in the string wins, see the table in `Capability.h`).

Explicit constructors exist for `(name, op, ed)`, `(arch, name, op, ed)`, namespaces (`Capability(ResolverNamespace, IdString)`; an empty value maps to `Null`, which matches every member of the namespace), and expressions (`Capability(CapRel, lhs, rhs)`).

**Printing**: `c_str()` renders reldeps with `cap2strHelper` into a **round-robin buffer of 5 strings** (`TempStrings<5>`).

> ⚠️ The `const char*` returned by `Capability::c_str()` for reldeps is only valid until five more reldep `c_str()` calls have happened, and the buffer is not thread-safe. Copy it into a `std::string` if you need to keep it.

Brackets follow rpm boolean-dependency rules (`needsBrace`). `srcpackage` caps are printed as `srcpackage:name`. `dumpOn` uses libsolv's `pool_dep2str` instead. `dumpCap` prints the expression tree.

**`CapDetail`** decomposes an id into kind / name / arch / op / ed / lhs / rhs / capRel. It maps libsolv's `src`/`nosrc` pseudo-arch back to a `srcpackage:` name.

**Matching**: `Capability::matches` / `_doMatch` only compares *simple* caps:
* Expressions return `CapMatch::irrelevant`.
* Names must be equal.
* If both caps have an arch, the arches must be equal.
* A NAMED cap matches any edition.
* Otherwise the result is `overlaps(MatchRange, MatchRange)`.

This is libzypp's own matcher. It is **not** what the solver uses; libsolv's whatprovides is authoritative.

`Capability::guessPackageSpec("libzypp-1.2.3-4.5")` rewrites `name-ver-rel` to `name = ver-rel` when nothing provides the literal string, but only if some pool item actually has that name (not just a provides). It tries the last and then the second-to-last `-`. zypper uses this for user input.

**`Capabilities`** (`zypp/zypp/Capabilities.{h,cc}`) is a read-only view into a solvable's `idarraydata`. It skips the marker ids on iteration and in `size()`.
* Pre-requires come *after* `solvablePrereqMarker` inside the requires array. `Solvable::dep_prerequires()` uses the `skip_r` constructor. **If the marker is absent, that constructor yields an empty list.**
* `dep_requires()` contains **both** normal requires and prerequires. The iterator's `tagged()` is `true` for entries after the marker.

---

## 5. `sat::Pool`, repositories and the serial/dirty mechanism

`sat::Pool` (`zypp/zypp/sat/Pool.{h,cc}`) is a handle with no state; everything forwards to `PoolImpl`. Main API:
* repository management: `reposInsert`, `reposFind`, `reposErase`, `reposEraseAll`, `systemRepo()`, `findSystemRepo()`, `addRepoSolv(file, RepoInfo)`, `addRepoHelix`
* solvable iteration (skips invalid entries; `capacity()` ≠ `solvablesSize()`)
* locales, multiversion, autoinstalled, needreboot spec
* `rootDir()` (used by file-conflict checks)
* `whatMatchesDep`, `whatContainsDep`, `whatMatchesSolvable` (thin `pool_whatmatchesdep` and related wrappers)

### 5.1 Adding and removing repos (`PoolImpl`)

Every mutating call starts with `setDirty(__FUNCTION__, name)`:
* `_createRepo` calls `repo_create`. If the name is `@System`, it also calls `pool_set_installed`.
* `_addSolv`, `_addHelix` and `_addTesttags` read the file, then call `_postRepoAdd`.
  * `_postRepoAdd` **removes solvables whose arch is not in `Arch::compatSet(systemArchitecture)`** (plus `src`/`nosrc`), in blocks via `repo_free_solvable_block`. The system repo is exempt. So foreign-arch packages from a repo are never in the pool, and changing the arch requires reloading the repos.
  * Note: `_addHelix` and `_addTesttags` always return 0, even when the underlying call fails.
* `_addSolvables` calls `repo_add_solvable_block` (used by testcases and code that builds a pool by hand).
* `_deleteRepo`:
  * calls `repo_free(..., reuseIds=false)`;
  * clears `_autoinstalled` if it was the system repo;
  * erases the repo's `RepoInfo`;
  * **if no repos are left**, calls `pool_freeallrepos(reuseIds=true)` and bumps `_serialIDs`, so solvable ids can be reused.
* `setRepoInfo` maps the yum-style priority (1 = highest … 99) to libsolv `repo->priority = -priority`. `repo->subpriority` comes from `media::MediaPriority(url)` (prefer http over DVD when priorities are equal). It only dirties the pool if one of the values changed.
* `Pool::addRepoSolv` uses an `AutoDispose<Repository>` with `EraseFromPool`, so a failed load does not leave a half-created repo behind.
* `Pool::reposInsert("@System")` attaches a dummy `RepoInfo` (enabled, autorefresh).

### 5.2 Serial numbers and lazy housekeeping

```
setDirty()  ─► _serial.setDirty()            (pool content changed)
             ├ reset available-locales cache, multiversion list
             ├ needreboot/retracted/ptf specs setDirty()
             └ depSetDirty() ─► pool_freewhatprovides()

localeSetDirty() ─► reset tracked locale ids + depSetDirty()

prepare()  (called lazily, e.g. by whatProvides(), ResPool::checkSerial())
   ├ /etc/sysconfig/storage changed (WatchFile)?  → drop USED_FS_LIST cache, depSetDirty
   ├ _serial changed since last prepare?           → pool_setarch(ZConfig::systemArchitecture())
   ├ !pool->whatprovides                           → pool_addfileprovides + pool_createwhatprovides
   └ !pool->languages                              → setTextLocale(ZConfig::textLocale())
```

* `serial()` changes whenever the content changes. `serialIDs()` changes only when ids are reused (last repo removed). Both are `SerialNumber`s, observed by `SerialNumberWatcher::remember()`.
* **Anything that caches pool-derived data must watch `serial()`. Anything that caches `PoolItem`s or ids must also watch `serialIDs()`.** `pool::PoolImpl::store()` does exactly that.
* `setDirty` lazily initialises the retracted/PTF specs, because `IdString` can't be used inside the `PoolImpl` constructor (static-init-order problem; see the CAVEAT comment).

### 5.3 `@System` and how the pool is filled

* **Installed system**: `target::TargetImpl::buildCache()` / `load()` in `zypp/zypp/target/TargetImpl.cc`:
  1. The cache is `<root>/<repoSolvfilesPath>/@System/solv` with a `cookie` file. The cookie holds a `RepoStatus` built from the rpmdb plus `<root>/etc/products.d`.
  2. If the cookie is stale, it runs `rpmdb2solv -r <root> -D <dbpath> -X -p <root>/etc/products.d [oldsolv] -o tmp`. `-X` auto-generates pattern/product solvables from packages (e.g. from `pattern()`/`product()` provides). It falls back to a temporary solv path if the cache dir isn't writable. It also writes `solv.idx` (`sat::updateSolvFileIndex`: `name\tevr\tarch` lines used by zypper's bash completion), and on `/` it sends the `PACKAGESETCHANGED` plugin frame.
  3. `load()` does `systemRepo().addSolv(solv)`. If that fails it calls `clearCache` and `buildCache` and retries.
  4. It then sets `rootDir`, `initRequestedLocales(<root>/var/lib/zypp/RequestedLocales)` (only when non-empty), and `setAutoInstalled` from the AutoInstalled file. If that file is missing, it is derived from the history log, and an obsolete `SoftLocks` file is removed.
  5. It loads the needreboot spec: built-in provides `installhint(reboot-needed)` and `kernel`, plus the `needreboot` file and `needreboot.d`; `*.rpmnew|save|orig` files are skipped.
* **Repos**: `RepoManager::loadFromCache` (`zypp/zypp/ng/repomanager.cc`):
  1. `reposErase(alias)`, then `addRepoSolv(<solvcache>/<alias>/solv, info)`.
  2. If the solv file's `repositoryToolVersion` ≠ `LIBSOLV_TOOLVERSION`, it erases the repo and throws, which triggers clean, rebuild and reload.

  See [Repositories](05-repositories-and-services.md).

---

## 6. `sat::Solvable` and friends

`sat::Solvable` (`zypp/zypp/sat/Solvable.cc`) holds a `SolvableIdType`. `get()` returns `nullptr` if the id is out of range or the slot has no repo (`validSolvable`). Every accessor uses `NO_SOLVABLE_RETURN(default)`, so calls on stale or invalid solvables are safe and return defaults.

* Identity: `ident`, `kind`, `name`, `edition`, `arch`, `vendor`, `repository`, `isSystem`.
  * `arch()` returns `noarch` for src/nosrc.
  * `asString()` prints the raw libsolv `name-evr.arch`, so src packages show `.src`.
* `identical(rhs)` → `solvable_identical`. It is used to pair installed and available copies of the same package (e.g. `ui::Selectable::identicalInstalled`).
* Lookups: `lookupStrAttribute` / `Num` / `Bool` / `Id` / `CheckSum` / `lookupLocation`.
  * Localised strings walk the `Locale::fallback()` chain, then fall back to the untranslated value. With no locale given, `solvable_lookup_str_poollang` uses `pool->languages` (set by `setTextLocale`).
  * `lookupLocation`: medianr 0 becomes 1. For `YAST2` (susetags) repos, the path is prefixed with `susetags:datadir`, or `suse` if that is absent. For repos of type `NONE`, finding a datadir sets the probed type to `YAST2`.
* Dependencies: `dep_provides`, `dep_requires`, … `dep_prerequires` (see §4.4), plus `providesNamespace` / `valuesOfNamespace` (parse `ns(value)` provides) and `matchesSolvable`.
* Locales: `supportsLocale(s)` / `getSupportedLocales` walk the `supplements` for `namespace:language(xx)` entries, expanding and/or.
* Status flags: `onSystemByUser` / `onSystemByAuto` (autoinstalled list), `isNeedreboot`, `isRetracted`, `isPtf*`, `isBlacklisted`, `multiversionInstall`.
* `licenseToConfirm()`: the solvable's `eula`. For products without one, it uses the repo license (`RepoInfo::getLicense`, keyed `license-<name>` with fallback `license`), but only if the license must be accepted or the product is not yet installed (bnc#908976: no informational license on update). `needToAcceptLicense()` is always true for non-products.
* Special tokens: `pattern()`, `product()`, `retracted-patch-package()`, `ptf()`, `ptf-package()`.

Other sat helpers (only briefly reviewed for this chapter):
* `SolvableSet`: a hash set of `Solvable`s.
* `SolvableSpec` (`zypp/zypp/sat/SolvableSpec.h`): a set defined by idents and `provides:CAP` lines. It can be parsed from a file (one entry per line). It caches the matching solvables until `setDirty()`. `addIdenticalInstalledToo` also matches installed solvables identical to a match.
* `WhatProvides(cap)`: iterates `pool_whatprovides` results (it calls `prepare()` first). `WhatObsoletes(solvable)`: what that solvable obsoletes.
* `LookupAttr` / `LookupRepoAttr`: libsolv `Dataiterator` wrappers, with optional `StrMatcher` and solvable/repo/pool scope. They are used by `PoolQuery`, product queries, file lists and similar.
* `SolvAttr`: well-known attribute ids such as `SolvAttr::name`, `summary`, `filelist`, `updateStatus`, `productReferenceFile`, `repositoryToolVersion`, …
* `sat::Map` / `sat::Queue` / `StringQueue`: RAII wrappers around `Map` / `Queue`.
* `sat::Transaction`: wraps a libsolv transaction (commit ordering; see [Target & commit](08-target-commit-and-rpm.md)).

---

## 7. `ResPool`, `PoolItem`, `ResStatus`

### 7.1 `pool::PoolImpl` – the PoolItem store

`ResPool::instance()` is a static `ResPool` around a `pool::PoolImpl` (`zypp/zypp/pool/PoolImpl.h`). It keeps:

* `_store`: `std::vector<PoolItem>`, indexed by `SolvableIdType`, so `find(solvable)` is O(1).
* `_id2item`: `unordered_multimap<IdType, PoolItem>` keyed by ident id, **negated for srcpackages** because they share the ident with packages. `pool::ByIdent` uses the same trick, and it backs `ResPool::byIdent*`.
* A `ResPoolProxy` (lazy, reset on change), `EstablishedStates`, and `_hardLockQueries`.

`checkSerial()` runs on nearly every access. If `sat::Pool::serial()` changed, it calls `invalidate()`, and it always calls `satpool().prepare()`. `store()` then:

1. resizes to `pool.capacity()` and walks the ids downwards;
2. clears `PoolItem`s whose solvable has gone (repo unloaded);
3. creates new items (`PoolItem::makePoolItem`, "the only way to create a new one!") for new solvables, **or for all solvables if `serialIDs` changed**;
4. sets **product buddies**: each newly added product shares its `ResStatus` with `Product::referencePackage()` (the `-release` package);
5. calls `reapplyHardLocks()` if items were added;
6. computes `EstablishedStates` (the initial validate status of pseudo-installed items such as patches and patterns, via `solver::detail::establish`).

> ⚠️ A `PoolItem` keeps a `shared_ptr` to its `Impl` (status plus `ResObject`). The store is rebuilt lazily. Holding `PoolItem`s across `reposErase()` or reloads keeps stale objects alive. Their `ResStatus` changes no longer affect the pool.

### 7.2 PoolItem

`PoolItem` (`zypp/zypp/PoolItem.cc`) = `shared_ptr<Impl>` holding `ResStatus` + `ResObject::constPtr` + `_buddy`.

* `PoolItem(solvable)` looks up the existing item through `ResPool::instance().find()`.
* `status()` returns a **mutable** `ResStatus&` even on a `const PoolItem`. All state changes by applications and the solver go through it.
* `statusReset()` unlocks and resets the transaction as USER. `statusReinit()` restores the lock from the UserLockQuery bit (the `/etc/zypp/locks` state).
* Buddy encoding: `_buddy > 0` means "use the buddy's status"; `< 0` means "my status is used by `-_buddy`". A product therefore shares its `ResStatus` with its release package; locking one locks both (bsc#1225267 handling in `setHardLockQueries`). Second-level buddies are refused with an ERR log line.
* `saveState`, `restoreState`, `sameState`: used by `ResPool`/`Resolver` to detect user changes. `sameState` ignores solver-caused transact changes unless the item was LOCKED.
* `isNeeded`, `isUnwanted`, `isSatisfied`, `isBroken`: meaningful for patches and patterns. `isNeeded` = to be installed, or broken and not locked.

### 7.3 ResStatus bit layout

`zypp/zypp/ResStatus.h` packs everything into a `uint16_t` (15 of 16 bits used; the header warns that the compiler does not check field fit):

```
bit:  0  | 1-2      | 3-4      | 5-6        | 7-8          | 9        | 10-13          | 14
      State Validate Transact   TransactBy   TransactDetail LicConfirm Weak(bits)       UserLockQuery
      INST/ UNDET/   KEEP/      SOLVER<      EXPLICIT/SOFT  0/1        SUGGESTED|        (internal:
      UNINST BROKEN/ LOCKED/    APPL_LOW<    _INSTALL or    RECOMMENDED|      lock came from
             SATISF/ TRANSACT   APPL_HIGH<   EXPLICIT/SOFT/ ORPHANED|         /etc/zypp/locks)
             NONREL             USER         OBSOLETE/UPGR  UNNEEDED
```

Rules (memorise these; most bug reports about "can't select X" end up here):

* **State is immutable.** `setStatus()` fails if the installed bit differs.
* `transacts()` together with State gives the meaning: `isToBeInstalled = uninstalled && TRANSACT`, `isToBeUninstalled = installed && TRANSACT`, and `onSystem() = isInstalled != transacts`.
* Causer hierarchy: `SOLVER < APPL_LOW < APPL_HIGH < USER`.
  * `setTransact()`: when already in the target state, it only *raises* the causer.
  * Changing state out of a non-KEEP state needs a causer ≥ the current one.
  * Changing out of KEEP_STATE is allowed for any causer. Every call resets TransactDetail.
* `setLock(true, c)` is only allowed for `USER` or `APPL_HIGH`. It resets the transaction first. Unlocking requires a causer ≥ the lock causer and then resets the causer to `SOLVER`, so that "unlocked" can be told apart from "kept by user".
* `isSoftLocked()` = KEEP_STATE by APPL_LOW or USER. `setSoftTransact(val, causer, limit)` refuses to leave KEEP_STATE if the current causer is above `limit`. The solver uses this so that, for example, a user's "keep" is not overridden by soft installs.
* `setToBeInstalledSoft` / `UninstalledSoft` / `DueToObsolete` / `DueToUpgrade` are "only for the Resolver".
* `maySet*` variants do a trial run: back up the bitfield, try the change, restore.
* `resstatus::StatusBackup` is a raw bitfield snapshot and replay.
* The license-confirmed bit lives here, so it is per item and not persisted.

---

## 8. Resolvable object hierarchy

```
Resolvable (sat::SolvableType<Resolvable>, ref-counted)
  └─ ResObject   — generic accessors (summary, vendor, size, …)
       ├─ Package      — changelog, filelist, location, supersededBy, …
       ├─ Patch        — category, severity, reboot/restart flags, contents
       ├─ Pattern      — isVisible, category, contents (expanded via requires/recommends)
       ├─ Product      — referencePackage, replacedProducts, productLine, isTargetDistribution, register*
       ├─ SrcPackage
       └─ Application  — appdata
```

`makeResObject(solvable)` (`zypp/zypp/ResObject.cc`) chooses the subclass from `solvable.kind()`. Unknown kinds get a plain `ResObject`. All data is read lazily from the sat solvable; the objects hold no state of their own beyond the id. Use `asKind<T>()` / `isKind<T>()` for casting and checks.

### Products and `/etc/products.d`

* Installed products come from `rpmdb2solv -p /etc/products.d` (the `.prod` XML files).
* `Product::referencePackage()` searches `WhatProvides("product(<name>) = <edition>")` for a provider in the same repo with the same arch.
  * bsc#1259311: if the product provides `autoproduct() = <pkgname>`, only that package name is accepted.
  * Among several candidates, it prefers a buildtime equal to the product's, then the higher edition.
  * Fallback for installed products (bnc#784900): the package that owns `/etc/products.d/<referenceFilename>`.
  * If nothing is found, it logs `WAR ... no reference package found`.
* `isTargetDistribution()` = installed and `product:type == "base"`. The base product as such is read by `TargetImpl` through the `/etc/products.d/baseproduct` symlink (`baseproductdata()`, cached with a WatchFile; logs `ERR` if the symlink dangles). See [Target](08-target-commit-and-rpm.md).
* `replacedProducts()` collects products obsoleted by the product, or by its buddy's obsoletes. `droplist()` = the buddy's `weakremover(...)` provides.
* Orphaned product files are only removed if no package owns them (`TargetImpl.cc` around line 1977).

---

## 9. Namespaces: language, modalias, filesystem

> Modalias has its own chapter: [03b – Modalias](03b-modalias.md).

libsolv evaluates `namespace:X(value)` dependencies through `PoolImpl::nsCallback`. Return 1 means "system provides it", 0 means not supported:

| Namespace | Data source |
|---|---|
| `NAMESPACE_LANGUAGE` | `trackedLocaleIds()`: requested locales **expanded by fallbacks** (`de_DE` → `de`) |
| `NAMESPACE_MODALIAS` | `target::Modalias::instance().query(hexdecode(value))`. Values may be hex-encoded because rpm disallows `,` and spaces |
| `NAMESPACE_FILESYSTEM` | `USED_FS_LIST` from `/etc/sysconfig/storage` (re-read when the file changes, §5.2) |

Anything else is logged as `WAR "Unhandled ..."` and returns 0. Results are cached by libsolv in the whatprovides data, which is why locale changes call `depSetDirty()`.

### Requested locales

* `_requestedLocalesTracker` is a `base::SetTracker<LocaleSet>` holding the *unexpanded* user choice.
* `initRequestedLocales()` sets the baseline, which `TargetImpl::load` reads from the RequestedLocales file. Later calls to `set/add/eraseRequestedLocale` are tracked as `added()` / `removed()`. The resolver uses these sets to add or remove language packages; `trackedLocaleIds()` expands each set by its fallbacks.
* bsc#1155678: an empty RequestedLocales set is deliberately different from `{en}`. `en` is **no longer** forced in, so an empty set does not even pull in recommended `en` packages.
* `setTextLocale()` sets `pool->languages`, the fallback chain used for translated summaries. It defaults to `en` if the locale is empty.
* `getAvailableLocales()` = all `namespace:language` supplements in the pool (cached until `setDirty`).

---

## 10. Locks, PoolQuery, VendorAttr, multiversion

### Hard locks (`/etc/zypp/locks`)

* `Locks` (`zypp/zypp/Locks.cc`) reads the locks file (`ZConfig::locksFile()`, applied when `locks.apply_locks_file` / `apply_locks_file` is true) as a list of serialized `PoolQuery`s.
  * The format is blocks of `attribute: value` lines separated by an empty line.
  * Keys include `type` (kind), `match_type`, `case_sensitive`, `install_status`, `version`, `repo`, `query_string` / `global_string`, `comment`, `complex`, and solvable attributes such as `solvable_name`. `require_all` is a legacy key that was never implemented (`zypp/zypp/PoolQuery.cc` ~l.1112, l.1238).
* `ResPool::setHardLockQueries()` → `pool::PoolImpl::setHardLockQueries`: evaluates all queries and, for each item, calls `UserLockQueryManip::setLock(status, matches)`. That sets or clears the USER lock **and** the internal UserLockQuery bit.
  * bsc#1225267: an item is locked if it or its buddy matches. Otherwise whichever comes later in the pool would win.
* New items (repo refresh) only get locks *added* via `reapplyHardLocks()`.
* `getHardLockQueries()` diffs each item's current USER lock against its UserLockQuery bit, then adds or removes **trivial name queries** (`makeTrivialQuery`: exact, case-sensitive name plus kind). Added locks win over removed ones. This is how lock changes made in the UI get written back on commit via `Locks::merge()` / `save()`.
* Unlocking an item that was locked by a *complex* query cannot remove that query. Expect user reports about locks that "reappear".

### PoolQuery

`PoolQuery` (`zypp/zypp/PoolQuery.cc`, 1.8k lines):
* a query over `LookupAttr` with `StrMatcher` (string/substring/glob/regex/word);
* filters for kinds, repos, edition range, install status, `addDependency(SolvAttr, name, op, edition)`;
* `setMatchExact`, `setCaseSensitive`, `filesMatchFullPath`;
* serialisation (`serialize` / `recover`) for the locks file.

It is used by zypper search, by locks, and by `Capability`-style user input. §13 covers it in depth.

### VendorAttr

`VendorAttr` (`zypp/zypp/VendorAttr.cc`) is a singleton built from `ZConfig::vendorPath()` (`/etc/zypp/vendors.d/*`). Each file defines equivalence groups (`[main] vendors = a,b,c`).

* Matching is case-insensitive and by **longest prefix** against the group entries.
  * Default group: `suse*`.
  * bnc#812608: vendors starting with `opensuse` match only exactly, never by prefix.
  * bsc#1030686: the old built-in `suse` ≡ `opensuse` equivalence was removed. `relaxedEquivalent()` still treats suse/opensuse as close for specific uses.
* Vendor equivalence controls `Selectable::defaultCandidate()` / `updateCandidateObj()` (unless `Resolver::allowVendorChange()`), and the solver's vendor checks.

### Multiversion

* `PoolImpl::multiversionList()` builds the set lazily from `ZConfig::multiversionSpec()` (`zypp.conf multiversion=`, `multiversion.d`).
  * A plain entry matches solvables whose ident equals the entry, found via `WhatProvides`.
  * `provides:CAP` matches every provider.
  * Matches are logged as `Multiversion install <spec>: N matches`.
* The list is reset on `setDirty` and on `ZConfig` multiversion changes (bnc#906096 hack: `ZConfig` calls `myPool().multiversionSpecChanged()`).

---

## 11. Retracted packages, PTFs, blacklisting

* `PoolImpl::_retractedSpec` = solvables providing `retracted-patch-package()`, with `addIdenticalInstalledToo(true)`.
  * The retracted marker comes from the repo metadata and is not part of the rpm, so an installed copy of a retracted package is matched by identity.
  * For patches, `isRetracted()` is `updateStatus == "retracted"` instead.
* PTF: `_ptfMasterSpec` matches `ptf()` providers (the PTF master package). `_ptfPackageSpec` matches `ptf-package()` providers (the packages the PTF ships).
* `isBlacklisted()` = `isPtf() || isRetracted()`.
* Blacklisted items sort **last** in `Selectable` available sets (`AVOrder`), and they are skipped by `candidateObjFrom` and `updateCandidateObj`. `highestAvailableVersionObj` prefers non-blacklisted items. The solver treats them specially (see [Solver](04-solver.md)).
* `Selectable::hasBlacklisted*`, `hasRetracted*` and `hasPtf*` expose this to UIs.

---

## 12. `ui::Selectable` and `ResPoolProxy` (how YaST sees the pool)

`ResPoolProxy` (`zypp/zypp/ResPoolProxy.*`) groups the pool by `(kind, name)` into `ui::Selectable`s. It is obtained with `ResPool::instance().proxy()`, created lazily and dropped on every serial change. **UIs must re-fetch the proxy and Selectables after pool changes.**

`ui::Selectable` (`zypp/zypp/ui/Selectable*.{h,cc}`, `SelectableTraits.h`):

* `installed()` is ordered by `IOrder`: better arch, then higher edition, then newer installtime, then id.
* `available()` is ordered by `AVOrder`: non-blacklisted first, then higher repo priority (`satInternalPriority`), better arch (ignoring noarch), higher edition, newer buildtime, higher subpriority, …
* Candidates:
  * `candidateObj()` = the transacting candidate, else the user-set candidate (`setCandidate`), else `defaultCandidate()`.
  * `defaultCandidate()`: if something is installed, prefer an available item with the same arch (noarch changes allowed) **and an equivalent vendor** (unless vendor change is allowed). If no vendor match exists, fall back to the same arch. Otherwise use the first available item.
  * `updateCandidateObj()` additionally requires a vendor match, arch compatibility and a strictly greater edition. It returns nothing for blacklisted items.
* **picklist**: available plus installed items that have no identical available copy, used by version-selection dialogs. `picklistPos`.
* `ui::Status` (`zypp/zypp/ui/Status.h`):

  | Group | Values |
  |---|---|
  | Locks | `S_Protected` (installed, locked) / `S_Taboo` (not installed, locked) |
  | User requests | `S_Del` / `S_Update` / `S_Install` |
  | Solver or app | `S_AutoDel` / `S_AutoUpdate` / `S_AutoInstall` |
  | No change | `S_KeepInstalled` / `S_NoInst` |

* `Selectable::Impl::setStatus(Status, causer)` translates these into `ResStatus` operations on installed and candidate items (`zypp/zypp/ui/SelectableImpl.cc` ~l.355). Protected/Taboo map to locks and are allowed for USER only (l.242/259).
* `pickStatus`/`setPickStatus` handle per-version selection for multiversion packages; non-multiversion packages go through ordinary `setStatus`.
* `fate()`, `modifiedBy()`, `hasLicenceConfirmed()`/`setLicenceConfirmed()` act on the candidate's `ResStatus`.

YaST's package selector relies entirely on this layer, so ABI and behaviour here must stay stable.

---

## 13. `PoolQuery` in depth

Files: `zypp/zypp/PoolQuery.{h,cc}`, `zypp/zypp/PoolQueryUtil.tcc` (file I/O helpers), `zypp/zypp/PoolQueryResult.{h,cc}` (set algebra on results). Tests: `zypp/tests/zypp/PoolQuery_test.cc` and `PoolQueryCC_test.cc`.

### 13.1 Object model

`PoolQuery` is a `RWCOW_pointer<Impl>` value. It is cheap to copy, and copy-on-write when modified. The raw query options stored in `PoolQuery::Impl` are:

| Member | Set by | Meaning |
|---|---|---|
| `_strings` (`set<string>`) | `addString()` | **Global** search strings, applied to every attribute |
| `_attrs` (`map<SolvAttr, set<string>>`) | `addAttribute(attr, value="")` | Attributes to search, each with its own strings |
| `_uncompiledPredicated` (`set<AttrMatchData>`) | `addDependency(...)` | Attribute + `StrMatcher` + extra predicate (edition range, arch, cap match, explicit kind) |
| `_flags` (`Match`) | `setMatch*()`, `setCaseSensitive()`, `setFilesMatchFullPath()`, `setFlags()` | Default **`SUBSTRING \| NOCASE \| SKIP_KIND`** |
| `_match_word` | `setMatchWord()` | Word mode = SUBSTRING plus `\b…\b` anchors (see below) |
| `_status_flags` | `setInstalledOnly()` / `setUninstalledOnly()` / `setStatusFilterFlags()` | `ALL=0`, `INSTALLED_ONLY=1` (only `@System`), `UNINSTALLED_ONLY=2` (everything else) |
| `_edition`, `_op` | `setEdition(ed, op=EQ)` | Filter on the **solvable's** edition, using `Edition::Match` semantics (empty release is a wildcard) |
| `_repos` | `addRepo(alias)` | Repo aliases. An empty alias is ignored with a WAR |
| `_kinds` | `addKind()` | Kind filter (any of) |
| `_comment` | `setComment()` (const, mutable member) | Only for serialisation; **ignored by `operator==`** |

Semantics in one sentence: **a solvable matches if it passes the repo, status, edition and kind filters, and *any* attribute in the match list matches (OR across attributes, OR across strings).** There is no AND between attributes. `setRequireAll()` / `requireAll()` are deprecated no-ops (`PoolQuery.cc` ~l.1060), and the locks-file key `require_all` is parsed and ignored.

Match modes (`setMatch*`, `PoolQuery.cc` ~l.963):

| API | `Match` mode | Single-string behaviour | Locks file `match_type:` |
|---|---|---|---|
| `setMatchExact()` | `STRING` | Whole-string compare | `exact` |
| `setMatchSubstring()` (default) | `SUBSTRING` | Substring | `substring` |
| `setMatchGlob()` | `GLOB` | fnmatch, anchored | `glob` |
| `setMatchRegex()` | `REGEX` | regcomp, unanchored | `regex` |
| `setMatchWord()` | `SUBSTRING` + `_match_word` | Always turned into the regex `\b(…)\b` | `word` (**recover only**, see 13.5) |

* `setCaseSensitive(bool)` toggles `Match::NOCASE`. **Queries are case-insensitive by default.**
* `setFilesMatchFullPath(bool)` toggles `Match::FILES` (libsolv `SEARCH_FILES`). The header comment says full-path matching is the default, but the `Impl` ctor does **not** set `FILES`. So by default, filelist searches match only basenames (see `LookupAttr.cc` ~l.601: whether filelist entries are stringified depends on `SEARCH_FILES`). Check this with a test before you rely on either behaviour.
* `SKIP_KIND` makes libsolv ignore the `kind:` prefix when matching `SolvAttr::name`. So `addAttribute(name, "foo")` finds `patch:foo` too, and `addAttribute(name, "patch:foo")` finds **nothing**. To query for an explicit kind, use `addDependency(SolvAttr::name, "patch:foo", …)` (bsc#1035729, next section).
* An empty search string means "match always". To match an empty value, use `"^$"` with regex mode.

### 13.2 `addDependency` and predicates

`addDependency(attr, name, op, edition[, arch[, mode]])` (`PoolQuery.cc` ~l.900):

1. If `op == Rel::NONE`, nothing is added (it can never match).
2. If `attr == SolvAttr::name` and `ResKind::explicitBuiltin(name)` detects a `kind:` prefix, the prefix is stripped and stored in `AttrMatchData::kindPredicate`. That kind **overrides** the global `addKind()` filter for this attribute (bsc#1035729).
3. If there is no edition range, no arch, no explicit kind, and the mode is `Match::OTHER` (the default), it simply calls `addAttribute(attr, name)`. This legacy shortcut keeps `serialize→recover` round trips equal.
4. Otherwise it builds an `AttrMatchData` with `StrMatcher(name, mode)` plus a predicate:
   * `EditionRangePredicate` for dependency attributes (`provides`, `requires`, …; see `isDependencyAttribute`). It takes the cap at the iterator, rejects non-simple caps (rich deps never match), accepts NAMED caps, and otherwise checks `overlaps(cap range, query range)`, plus optional `inSolvable().arch()` equality.
   * `SolvableRangePredicate` for any other attribute. It compares the **solvable's** edition and arch.
5. `mode == Match::OTHER` marks the matcher as "to be compiled". At `compile()` time the name is merged with the global `_strings` and the global `_flags`. An explicit `Match::Mode` (the 6-arg overload, changelog "PoolQuery::addDependency add overload to define an individual…") is used as-is.

`addDependency(attr, Capability)` always uses an exact `STRING` match on `cap.name()`, plus `CapabilityMatchPredicate` (`Capability::matches == CapMatch::yes`) for dependency attributes, or `SolvableRangePredicate(cap.op(), cap.ed())` otherwise. Non-simple caps are dropped silently.

Every predicate has a `serialize()` string (`EditionRange <op> <ed> <arch>`, `SolvableRange …`, `CapabilityMatch <cap>`). This string is used for `operator==`/`<` (functions can't be compared) and for the locks file `complex:` line (`AttrMatchData::serialize`/`deserialize`). **If you add a new predicate type, you must extend `deserialize()` too.** Otherwise `recover()` logs `Unparsable value for complex` and drops the attribute.

### 13.3 `compile()`: from options to `AttrMatchList`

`Impl::compile()` (`PoolQuery.cc` ~l.564) builds `_attrMatchList` (a vector of `AttrMatchData`). It is called on every `begin()`, inside the `PoolQueryMatcher` ctor.

```
_attrs.empty()           → nothing yet (a 'query all' entry is added at the end)
_attrs.size()==1         → one entry: attr, joined(_strings ∪ attr strings)
_attrs.size()>1:
   all attr-strings empty, or "the same"  → one shared StrMatcher(joined) for every attr
   otherwise                              → per attr: joined(_strings ∪ that attr's strings)
for each _uncompiledPredicated:
   mode OTHER → copy, matcher = joined(_strings ∪ its searchstring) with global _flags
   else       → copy as is
still empty → AttrMatchData( SolvAttr::allAttr, joined(_strings) )
finally: every StrMatcher.compile()  → may throw MatchInvalidRegexException
```

`joinedStrMatcher()` (~l.742):
* With 0 strings, it returns an empty matcher (matches always).
* With exactly 1 string and no word mode, it returns `StrMatcher(str, flags)` unchanged, so libsolv does the matching natively.
* With more than one string (or word mode), it builds a **regex**. Each string goes through `rxEscape` (`rxEscapeGlob` for glob, `rxEscapeStr` for string/substring, unchanged for regex). The pieces are joined as `(a|b|c)`. The result is anchored `^…$` for STRING/GLOB, and `\b…\b` for word mode.

> ⚠️ Likely bug: the "same strings for all attributes" test (~l.620) only checks `set_difference(first, other)`. If a later attribute has a *superset* of the first attribute's strings, the query is treated as "same", and the extra strings of the later attributes are silently dropped. For example, with `name:{a}` and `summary:{a,b}`, `b` is never searched in summary. Unverified by a test.

### 13.4 Execution: `PoolQueryMatcher` on top of `LookupAttr`

`PoolQuery::begin()` creates a `detail::PoolQueryIterator` that owns a `shared_ptr<PoolQueryMatcher>`. The iterator adapts a `sat::LookupAttr::iterator`, which is a libsolv `Dataiterator`.

* **Repos**: aliases are resolved via `sat::Pool::reposFind` *when the query starts*. Unknown aliases are dropped. If *none* resolve, `_neverMatchRepo` makes the query empty, instead of falling back to "all repos" (bnc#661976). With exactly 1 repo, the restriction is pushed into `LookupAttr::setRepo`, so libsolv only walks that repo. With more than one, it is filtered in `isAMatch` with `nextSkipRepo()`.
* **Attribute**: with exactly one entry in `_attrMatchList` (including `allAttr`), the attribute and the `StrMatcher` are pushed into `dataiterator_init`, and libsolv does the string matching. With more than one entry, the base iterator runs over **`SolvAttr::allAttr` with no matcher**: it visits every attribute of every solvable. For each solvable, `isAMatch` then starts a fresh `LookupAttr(attr, solvable)` per match entry.
* **Order of filters in `isAMatch`**:
  1. status (`nextSkipRepo` on mismatch);
  2. repo set (`nextSkipRepo`);
  3. edition `_op`/`_edition` (`nextSkipSolvable`);
  4. kind: the global kind result is computed, but the decision is deferred, because a `kindPredicate` may override it;
  5. string match and predicate.
* On a hit, `advance()` calls `nextSkipSolvable()`, so **each solvable is returned at most once**. To see *which* attributes matched, call `PoolQueryIterator::matches()` / `matchesBegin()` (lazily computed by `matchDetail()`, which re-runs the lookups for that solvable). `dumpOn(str, iterator)` prints them.
* `empty()` and `size()` catch exceptions and report empty/0. `size()` iterates the whole query, so it is O(result) every time. `begin()` itself **throws** `MatchInvalidRegexException` / `MatchUnknownModeException`, so callers that iterate must catch.

Because `PoolQuery` is a `sat::SolvIterMixin`, you also get `poolItemBegin()`, `selectableBegin()` (unified by ident) and the `Iterable` helpers for free. `PoolItemBest` (§15) picks the best version per ident from such a range.

### 13.5 Serialisation (`/etc/zypp/locks`)

`serialize(ostream, delim='\n')` writes a record framed by a leading **and** trailing `delim` (the empty line that separates records). It only writes values that differ from a default-constructed `PoolQuery`:

```
repo: <alias>                          one line per repo
type: <kind>                           one line per kind
version: <op> <edition>                only if op != ANY and edition non-empty
match_type: exact|substring|glob|regex only if mode differs from SUBSTRING
case_sensitive: on|off                 only if it differs from default (off)
install_status: all|installed|not-installed
query_string: <s>                      one per global string
solvable_name: <s>                     one per attribute value; ':' in the attr name → '_'
complex: AttrMatchData <attr> <str> <mode> <predicate>    (str::appendEscaped words)
comment: <c>
```

Some state is **not serialised**:
* **Word mode**: `matchMode()` is `SUBSTRING`, which equals the default, so nothing is written, although `recover` understands `match_type: word`.
* `Match::FILES` (full-path file matching).
* Per-attribute `Match::Mode`s, except inside `complex:`.

A word-mode query therefore comes back from a round trip as a substring query. Also, `serialize` writes `solvable_name: foo` only for plain `_attrs`. A versioned name lock is written as a `complex:` line, and `recover` turns it back into the old style only under the bsc#1112911 conditions below.

`recover(istream, delim)`:
* `#` lines are comments. A blank line, or a line without `:`, ends the record, but only after something has been read.
* Aliases are accepted: `kind` = `type`, `global_string` = `query_string`, `string_type` = `match_type`.
* `case_sensitive:` logic is inverted in the code (~l.1243): a *true* value sets case-sensitive, a *false* value (`off`) falls into the `WAR unknown boolean value` branch, and *garbage* sets case-insensitive. Because the default is case-insensitive, the result is still right for `on`/`off`. The only visible symptom is a bogus WAR on every `case_sensitive: off` line.
* `version:` accepts a leading operator (`=<>!`), e.g. `version: >= 1.2`. Without one it means `==`.
* Any **unknown key** is treated as a solvable attribute (`_` → `:`). For `solvable:name` and dependency attributes, the value is parsed as a `Capability`. A versioned value (`solvable_name: kernel > 1`) becomes `addDependency(attr, name, op, ed)`; anything else becomes `addDependency(attr, value)`, which ends up in `addAttribute`.
* bsc#1112911: after parsing, a record that has exactly one `complex:` entry for `solvable:name` in mode `C` (compile), no plain name attribute, and **no arch** in its predicate is rewritten into the OLD STYLE (`kind`, `solvable_name`, `version`). zypper can only display that form. Complex locks with an arch are left alone on purpose ("better a strange looking `zypper locks` list than locking the wrong packages").

`PoolQueryUtil.tcc`: `readPoolQueriesFromFile(path, outIt)` loops `recover()` until it returns false. `writePoolQueriesToFile(path, b, e)` truncates and serializes each query. Both throw `Exception("Cannot open file …")`. They are used by `zypp/zypp/target/HardLocksFile.cc`, which writes to a temp file and then renames it.

`operator==` (~l.486) ignores `_comment`. It also treats exact and glob mode as equal for a query that consists of **only one `solvable:name` attribute and no global strings** (bnc#792901: libzypp writes name locks as exact, zypper writes glob; this lets `zypper removelock` match without loading repos). `operator<` compares the raw members in order. `Locks` relies on both for duplicate detection (bsc#969569).

### 13.6 `PoolQueryResult`

`PoolQueryResult` (`zypp/zypp/PoolQueryResult.h`) is an `unordered_set<sat::Solvable>` with `+=` / `-=` overloads for `Solvable`, `PoolItem`, `PoolQuery`, another `PoolQueryResult`, or an iterator range of any of them. It is also a `SolvIterMixin`. `+= PoolQuery` catches exceptions (an invalid regex counts as an empty result). Use it for "packages matching A but not B" without writing custom loops.

### 13.7 Performance notes

* Fast path: one attribute (or none), at most one repo, and one plain string. libsolv filters inside `dataiterator_step`.
* Several strings for one attribute turn into one regex. That is still native, but regex matching is slower than substring.
* **Several attributes** (e.g. zypper `search -d` = name + summary + description) make the base iterator walk *every attribute of every solvable* (`allAttr`). On top of that, each solvable gets a fresh `LookupAttr` per attribute. This is the expensive case.
* Kind filtering always happens in `isAMatch`, never in libsolv. With one attribute, a solvable of the wrong kind is not skipped (`return false` without `nextSkipSolvable`). The iterator steps through all remaining matches inside that solvable before moving on.
* `matches()` runs the lookups again. Don't call it in a loop over large results unless you need it.
* `PoolQuery::size()` and `empty()` re-run the query. Cache results in a `PoolQueryResult` or a vector.
* Every `begin()` recompiles, including regexes, and resolves repo aliases again. Keep a single iterator rather than calling `begin()` repeatedly.

---

## 14. `sat::LookupAttr`, `LookupAttrTools`, `SolvAttr`

### 14.1 `SolvAttr`

`SolvAttr` (`zypp-logic/zypp/sat/SolvAttr.{h,cc}`) is an `IdStringType` around a libsolv key name id (`SOLVABLE_*`, `UPDATE_*`, `PRODUCT_*`, `REPOSITORY_*`). Special values: `allAttr` (id 0, "any attribute") and `noAttr` (empty). Groups:

| Group | Examples |
|---|---|
| Common | `name`, `edition`, `arch`, `vendor`, `summary`/`description`/`insnotify`/`delnotify`/`eula` (translated), `cpeid`, `installtime`, `buildtime`, `installsize`, `downloadsize`, `diskusage` |
| Dependencies | `provides` … `enhances`, also as `dep_*` aliases (same ids) |
| Package | `checksum`, `medianr`, `mediafile`, `mediadir`, `changelog` (string key, normally empty in solv files), `buildhost`, `license`, `group`, `keywords`, `filelist`, `sourcename`/`sourceevr`/`sourcearch`, `headerend`, `url` |
| Patch (updateinfo) | `patchcategory`, `severity`, `updateStatus`, `rebootSuggested`/`restartSuggested`/`reloginSuggested`, `message`, sub-structures `updateCollection{Name,Evr,Arch,Filename,Flags}` and `updateReference{Type,Href,Id,Title}` |
| Pattern | `isvisible`, `icon`, `order`, `isdefault`, `category`, `script`, `includes`, `extends` |
| Product | `productReferenceFile`, `productProductLine`, `productShortlabel`, `productType`, `productFlags`, `productEndOfLife`, `productRegister{Target,Release,Flavor}`, `productUrl`/`productUrlType`, sub-structure `productUpdates`/`productUpdatesRepoid` |
| Repository (meta) | `repositoryTimestamp`, `repositoryExpire`, `repositoryKeywords`, `repositoryRevision`, `repositoryToolVersion`, `repositoryUpdates`/`repositoryDistros` (+ `repositoryProductLabel`/`Cpeid`), `repositoryRepoid`, `repositoryAddedFileProvides`, `repositoryDeltaInfo` |

`SolvAttr::parent()` knows the sub-structure parent of only the **update collection and update reference children**. `LookupAttr` uses it to automatically call `dataiterator_prepend_keyname(parent)`. For any other sub-structure (e.g. `productUpdatesRepoid`), you must pass the parent explicitly, as `Product::updateContentIdentifier()` does: `LookupAttr(productUpdatesRepoid, productUpdates, *this)`.

### 14.2 `LookupAttr`

`zypp/zypp/sat/LookupAttr.{h,cc}`. A query is defined by:
* an attribute (`allAttr` for all);
* an optional parent;
* a scope: pool (default), `setRepo(repo)`, or a single `Solvable`;
* a `Location`: `SOLV_ATTR` (default), or `REPO_ATTR`, which maps to `SOLVID_META` for repository-level data;
* an optional `StrMatcher`.

`LookupRepoAttr` is the same with `REPO_ATTR` pre-set.

`begin()` returns `end()` if the attribute is `noAttr` **or the pool has no repos**. Otherwise it creates a `detail::DIWrap`, which owns a heap `::Dataiterator` and a copy of the search string, because libsolv keeps the `const char*`. It calls `dataiterator_init(pool, repo, solvid, key, match, flags)`. The `Match` flags are passed straight through as libsolv `SEARCH_*` flags. That is why `StrMatcher` modes, `NOCASE`, `SKIP_KIND`, `FILES`, `COMPLETE_FILELIST` and `CHECKSUMS` all work in `LookupAttr`. Copying an iterator calls `dataiterator_init_clone`.

`LookupAttr::iterator` API highlights:
* Position: `inRepo()`, `inSolvable()`, `inSolvAttr()`, `atEnd()`.
* Skipping: `nextSkipSolvAttr()`, `nextSkipSolvable()`, `nextSkipRepo()` (each followed by `++`); `skipSolvAttr()`, `skipSolvable()`, `skipRepo()` combine both. `stayInThisSolvable()` / `stayInThisRepo()` pin the iterator (`SEARCH_THISSOLVID`). `PoolQuery::matchDetail` uses this.
* Type tests: `solvAttrType()`, `solvAttrNumeric()`, `solvAttrString()`, `solvAttrIdString()`, `solvAttrCheckSum()`, `solvAttrSubEntry()`.
* Values: `asInt()`, `asUnsigned()`, `asBool()`, `asUnsignedLL()`, `c_str()`, `asString()` (stringifies non-strings, e.g. deps via `pool_dep2str`), `idStr()`/`id()` (**only for poolized strings**; long texts like `description` have no id), `asCheckSum()`, `asType<T>()` (defaults to `T(id())`; specialised for `std::string`, `Capability`, …).
* **Sub-structures (flexarrays)**: when `solvAttrSubEntry()` is true, `subBegin()` / `subEnd()` iterate the children, `subFind(SolvAttr)` jumps to one child, and `subFind("name")` looks up `<currentattr>:name`. Internally this uses `dataiterator_clonepos` + `dataiterator_seek(DI_SEEK_CHILD|DI_SEEK_STAY)`. Limitation from the header: an iterator obtained from `subBegin`/`subFind` cannot be used to call `subBegin`/`subFind` again (it returns `end`), and it can't leave the sub-structure.

**Why patches and products use `LookupAttr` and not `lookupStrAttribute`:** updateinfo and product data are lists of structured records (`update:collection` = {name, evr, arch, filename, flags} per package; `update:reference` = {type, href, id, title}; `product:updates` = {repoid}). The `Solvable::lookup*Attribute` helpers only return the first scalar value, so walking these records needs `LookupAttr` with `subFind`. See `Patch::contents()`, `Patch::ReferenceIterator`, and `Product::updateContentIdentifier()`.

### 14.3 `LookupAttrTools.h`

* `LookupAttr::TransformIterator<TResult, TAttr>`: wraps `LookupAttr::iterator` and dereferences to `TResult(it.asType<TAttr>())`.
* `sat::ArrayAttr<TResult, TAttr>`: a small container around a `LookupAttr` with `begin`/`end`/`empty`/`size` (**O(n), it iterates**) and `find` (linear). Used for `Package::Keywords = ArrayAttr<PackageKeyword, IdString>`, `Package::FileList = ArrayAttr<std::string, std::string>`, and `Pattern::NameList` (`includes`/`extends`).

---

## 15. Pool helpers: `PoolItemBest`, `PoolStats`, filters, `Bitmap`, `CapMatch`

### `PoolItemBest` (`zypp/zypp/PoolItemBest.{h,cc}`)

This keeps an `unordered_map<IdString ident, PoolItem>` and, for each ident, keeps the "best" item according to **`ui::SelectableTraits::AVOrder`**, the same order used for `Selectable` available lists (§12): non-blacklisted first, then repo priority, arch, edition, and so on. Feed it a `PoolQuery` range, or any range of `PoolItem`/`Solvable`. `begin()`/`end()` iterate the items, `identBegin()` iterates the idents, and `find(ident)` looks up one ident.

* Ctor flag `preferNotLocked` (for bsc#1113296): an unlocked item always beats a locked one, and AVOrder decides only between two items of the same lock state.
* It keys on `ident`. A srcpackage and a package with the same name share an ident (§4.3), so they compete for one slot. **Filter by kind first** if both can appear.
* `AVOrder` is applied to installed items too. If you want "best available", filter installed items out yourself.

### `pool::PoolStats` (`zypp/zypp/pool/PoolStats.h`)

This is a functor that counts `ResObject`s per kind. `dumpPoolStats(str, begin, end)` prints `ResObjects: N` followed by one `kind: count` line per kind. It works on ranges of `ResObject::Ptr` or `PoolItem` (via conversion). It is handy in debug logs and tests.

### `ResFilters.h` / `ui/SelFilters.h`

These are functor predicates for `filter_iterator` / `make_filter_begin`.
* `resfilter::` (`zypp/zypp/ResFilters.h`) on `ResObject::constPtr`: `ByName`, `ByRepository` (compares `repoInfo().alias()`), `ByEdition<Compare>`, `ByArch<Compare>`, plus the `byKind<T>()` helper (based on `filter::ByKind`).
* `resfilter::` on `PoolItem`: `ByInstalled`, `ByUninstalled`, `ByTransact`, `ByLock`, `ByKeep`, `ByRecommended`, `BySuggested`.
* `ui::selfilter::` (`zypp/zypp/ui/SelFilters.h`) on `Selectable::constPtr`: `ByKind`, `ByName`, `ByHasInstalledObj`, `ByHasCandidateObj`, `ByStatus(ui::Status)`.

All of these are trivial. The doc comment in `ResFilters.h` still refers to the long-gone `ResStore::forEach`; ignore those examples. Combine predicates with `chain` and `not_c` from `zypp-core/base/LogicalFilters.h`.

### `ui::userWantedPackageNames()` (`zypp/zypp/ui/UserWantedPackages.{h,cc}`)

This returns the package *names* that the user explicitly wants to change: Package `Selectable`s with `toModify()` and `modifiedBy() == USER`, plus `Patch::contents()` names of every patch `Selectable` that `toModify()`. The header also promises patterns and required patterns. `addPkgSetPackages<>` is **declared but never defined or called**, so patterns are *not* included. Since `Patch::contents()` only lists packages whose name and arch are installed (§16.1), patch contents for new packages are not included either.

### `Bitmap` / `sat::Map`

`zypp/zypp/Bitmap.h` is just `using Bitmap = sat::Map;`. That type is a RAII libsolv `Map` (a bitset sized by solvable id, `zypp/zypp/sat/Map.h`).

### `CapMatch` (`zypp-logic/zypp/CapMatch.h`)

`CapMatch` is a tri-state result: `yes`, `no`, `irrelevant` (the default ctor gives `irrelevant`). `irrelevant` is neutral in `&&`, `||` and `!`: `irrelevant && x == x`, and `!irrelevant == irrelevant`. **There is no conversion to `bool`.** Compare explicitly with `== CapMatch::yes` (as `CapabilityMatchPredicate` does). `Capability::matches` returns `irrelevant` for rich expressions (§4.4), so "not yes" does not mean "no".

---

## 16. Resolvable kinds in detail

All accessors read lazily from the solvable via `lookup*Attribute` or `LookupAttr`. None of them cache anything.

### 16.1 Patch (`zypp/zypp/Patch.{h,cc}`)

| Accessor | Source / logic |
|---|---|
| `category()` | `SolvAttr::patchcategory`, **lower-cased** |
| `categoryEnum()` / static `categoryEnum(str)` | Case-insensitive: `yast`→`CAT_YAST`; `security`; `recommended` and RHN `bugfix`→`CAT_RECOMMENDED`; `optional`, `feature` and RHN `enhancement`→`CAT_OPTIONAL`; `document`; anything else → `CAT_OTHER`, logged at **INT** level (also for an empty category) |
| `isCategory(str)` | Same enum, or (for `CAT_OTHER`) a case-insensitive string compare. `isCategory(Categories)` takes OR'ed flags. There is also a template for containers of strings |
| `severity()` / `severityFlag()` | `SolvAttr::severity` lower-cased. `low`/`moderate`/`important`/`critical`; `unspecified` **or empty** → `SEV_NONE`; anything else → `SEV_OTHER` (INT log). `asString(SEV_OTHER)` = `"unknown"` |
| `rebootSuggested()` / `restartSuggested()` / `reloginSuggested()` | Bool attrs `UPDATE_REBOOT` / `UPDATE_RESTART` / `UPDATE_RELOGIN`. A restart means the patch touches the package manager itself (the zypper "update the stack first" logic) |
| `message(locale)` | `UPDATE_MESSAGE` (translated) |
| `interactiveFlags()` | `Reboot` if reboot is suggested, `Message` if the message is non-empty, `License` if the patch **or any package in `contents()`** has a `licenseToConfirm()` |
| `interactiveWhenIgnoring(flags)` / `interactive()` | `interactiveFlags() & ~flags`. zypper's `--skip-interactive` / `--with-interactive` and the reboot-ignore option (bnc#665853) are built on this |
| `contents()` | See below |
| `referencesBegin/End()` | `Patch::ReferenceIterator` over `update:reference` sub-structures: `id()`, `href()`, `title()`, `type()` (`bugzilla`, `cve`, …). The iterator has no meaningful `operator*` |
| `timestamp()` | = `buildtime()`, which libsolv sets to the **later** of `<issued>` and `<updated>` (`repo_updateinfoxml.c`). It is *not* strictly the issue date. See [03a B.1](03a-special-solvables.md). |

`Patch::contents()` walks `update:collection`. For each entry, `{name, evr, arch}`:
1. Entries with an empty name are skipped with a WAR.
2. **Relevance**: some `@System` solvable must have `ident == name` and `arch == arch`. Otherwise the entry is skipped. So `contents()` only lists packages that are currently installed (by name and arch). It is *not* the full package list of the updateinfo.
3. It looks for an exact provider of `name.arch = evr` (kind package). Failing that, it takes the first provider of `name.arch > evr`. If neither exists, it logs a WAR `Missing provider` and skips the entry.
4. It inserts the **first** provider (`*providers.begin()`). That choice is arbitrary if several repos carry the same package.

**Status of a patch** is not stored in the patch itself. It comes from the solver's establish pass (`EstablishedStates`, §7.1) as `ResStatus::validate()`:

| `PoolItem` query | Meaning |
|---|---|
| `isSatisfied()` | All referenced packages (or newer) are installed |
| `isBroken()` | Applies to the system but isn't satisfied |
| `isRelevant()` | `!isNonRelevant()`: the patch touches something installed |
| `isNeeded()` | To be installed, **or** broken and not locked |
| `isUnwanted()` | Broken **and** locked (a user-locked "needed" patch) |

`Solvable::isRetracted()` for patches is `updateStatus == "retracted"` (`zypp/zypp/sat/Solvable.cc` ~l.414; jsc#SLE-8770). Other `updateStatus` values (`stable`, `testing`, …) have no dedicated accessor; read `SolvAttr::updateStatus` directly.

### 16.2 Pattern (`zypp/zypp/Pattern.{h,cc}`)

| Accessor | Source / logic |
|---|---|
| `isDefault()` | Bool `isdefault` |
| `userVisible()` | `SolvAttr::isvisible`. If the value is a **string**, it is an *ident*, and the pattern is visible iff `ResPool::byIdent(ident)` is non-empty (conditional visibility). Otherwise it is read as a bool |
| `category(locale)`, `icon()`, `script()`, `order()` | Straight attributes. `order` is a **string**; UIs sort it as such |
| `isAutoPattern()` | Provides an `autopattern()` cap. Such patterns are generated by `rpmdb2solv -X` or repo tools from a `patterns-*` package |
| `autoPackage()` | For auto patterns: the package `name = edition` with the same arch, named in `autopattern() = <pkgname>`, **in the same repo**. This package carries the real dependencies |
| `includes()` / `extends()` | `Pattern::NameList` (`ArrayAttr`) of `includes` / `extends`: old susetags UI hints |
| `core()` | Packages providing the pattern's **requires**, plus the autoPackage's requires |
| `depends(includeSuggests=true)` | requires + recommends (+ suggests), from the pattern and its autoPackage. `dependsNoSuggests()` excludes suggests |
| `contents(includeSuggests=true)` | `depends()` of the pattern **and of every pattern reached through `includes`/`extends`** (`PatternExpander`). `contentsNoSuggests()` excludes suggests |

Notes:
* All `Contents` are `sat::SolvableSet`s built from `WhatProvides(caps)` filtered to `kind == package`. They contain *every* provider: all versions, all repos, installed and available. They are not a resolved selection.
* `PatternExpander::expandExtending` scans **all** patterns in the pool for every pattern it processes. That is O(P²) work, which is fine for typical pool sizes.
* Pattern-to-pattern dependencies in modern metadata are expressed through `pattern()` provides on `patterns-*` packages. The deprecated `contentsSet()` / `fullContentsSet()` (`#if LEGACY(1735)`) walked those packages. Keep them compiling, but don't use them.

### 16.3 Product (`zypp/zypp/Product.{h,cc}`)

The existing §8 covers `referencePackage`, `isTargetDistribution`, `replacedProducts` and `droplist`. Additionally:

| Accessor | Source / logic |
|---|---|
| `referenceFilename()` | `product:referencefile` (the `.prod` file name in `/etc/products.d`) |
| `productLine()`, `type()` (`base`, …), `flags()` | Attributes |
| `shortName()` | `productShortlabel`, falling back to `name()` |
| `flavor()` | Finds a provider of `product_flavor(<name>) = <edition>` **in the same repo** (arch ignored), then returns the content of its first `flavor(...)` provide. Empty if there is none |
| `endOfLife()` / `hasEndOfLife([Date&])` | `productEndOfLife`. *Missing* means no EOL planned. *Present but empty or invalid* means an EOL exists but is not yet known (FATE#320699). `hasEndOfLife` tells the two apart by using a `-1` default |
| `updateContentIdentifier()` / `hasUpdateContentIdentifier(cid)` | `product:updates` → `repoid` sub-structure. Compared against `Repository::ContentIdentifier` to decide which repos update this product |
| `registerTarget()`, `registerRelease()`, `registerFlavor()` | Product file `<register>` data. SUSEConnect / registration use it |
| `urls(key)` and `releaseNotesUrls()`, `registerUrls()`, `smoltUrls()`, `updateUrls()`, `extraUrls()`, `optionalUrls()` | Parallel walk of the `productUrl` and `productUrlType` arrays |
| `cpeId()` (inherited from `Solvable`) | `SolvAttr::cpeid` parsed into a `CpeId` (`zypp-logic/zypp/CpeId.{h,cc}`, CPE 2.3 with WFN matching) |

`Product::urls()` assumes that the `url` and `url:type` arrays have the same length and order. It pairs them by walking two independent `LookupAttr`s. A mismatch logs `ERR … The thing that should not happen, happened.` and truncates the list.

Base product / product file data (`baseproduct` symlink, `/etc/products.d/*.prod` parsed into `parser::ProductFileData`) is handled in the target layer, not in `Product`. See [Target](08-target-commit-and-rpm.md).

### 16.4 Package (`zypp/zypp/Package.{h,cc}`), SrcPackage, Application

| Accessor | Source / logic |
|---|---|
| `changelog()` | **Installed packages only.** It reads the rpm header via `target->rpmDb().getData(name(), header)` and returns `Changelog` = `std::list<ChangelogEntry{date, author, text}>` (`zypp/zypp/Changelog.h`). For uninstalled packages, or without a target, it returns an empty list with a WAR/ERR (bnc#530595: a header may disappear behind our back). The lookup is **by name only**, so for multiversion packages (kernels) you get the changelog of *some* installed version |
| `buildhost`, `distribution`, `license`, `packager`, `group`, `url`, `sourcesize` | Attributes |
| `authors()` | `authors` attribute split on `\n` |
| `keywords()` | `Package::Keywords` (`ArrayAttr<PackageKeyword, IdString>`). The support-level keywords live here |
| `filelist()` | `Package::FileList` (`ArrayAttr<string, string>`). Repo metadata usually only contains "primary" files (`/etc`, `*bin/`); installed packages have the full list |
| `checksum()` | `SolvAttr::checksum` |
| `location()` | `Solvable::lookupLocation()` → `OnMediaLocation` (medianr, path with susetags datadir prefix, checksum, download size; §6) |
| `cachedLocation()` | `zyppintern::cachedLocation(location(), repoInfo())`: the file in `RepoInfo::packagesPath()`, but **only if its checksum matches**. If the metadata has no checksum, it is accepted only for local `dir:`/`file:` repos, after an md5 compare with the original. Otherwise it returns an empty path |
| `sourcePkgName()` / `sourcePkgEdition()` | `sourcename` / `sourceevr` ids. **An id of 0 means "same as the binary"**: libsolv stores the source name/evr only when they differ, so these fall back to `name()` / `edition()` |
| `sourcePkgType()` | `sourcearch` (`src` or `nosrc`). `sourcePkgLongName()` = `name-edition.type` |
| `vendorSupport()`, `maybeUnsupported()`, `supersededBy()`, `supersededByItems()` | See 16.5 |

`SrcPackage` adds only `sourcePkgType()` (its arch: `src` or `nosrc`), `location()` and `cachedLocation()`. Its sat representation and ident are described in §4.3.

`Application` (kind `application`, generated from AppStream/appdata by libsolv's `repo_add_appdata` / `rpmdb2solv`) has **no accessors** beyond `ResObject`. Use the `Solvable` and `LookupAttr` APIs directly.

### 16.5 `VendorSupportOption` (`zypp/zypp/VendorSupportOptions.{h,cc}`)

These are enum flags: `VendorSupportUnknown=0`, `Unsupported`, `ACC` (additional customer contract), `Level1`, `Level2`, `Level3`, `Superseded` (jsc#OBS-301 / jsc#PED-8014, 2024). `asUserString()` and `asUserStringDescription()` give translated texts (SUSE semantics).

`Package::vendorSupport()` → `zyppintern::getVendorSupportOption()` (`Package.cc` top):
* It looks at the keywords of **every solvable identical to this one** (`WhatProvides(ident)`, same edition, `identical()`). Keywords exist only in repo metadata, so an installed package inherits them from its available twin. If no repo carries the twin, the result is `Unknown`.
* Keywords: `support_unsupported`, `support_acc`, `support_l1`, `support_l2`, `support_l3`, `support_superseded[(PKGNAME)]`.
* The intentional switch fall-through means a later keyword can only **raise** the level along `Unknown → Unsupported → ACC → L1 → L2 → L3 → Superseded`. `Superseded` wins over everything. Several `support_superseded(X)` keywords accumulate successor names. A missing or empty `(…)` logs a WAR.
* `maybeUnsupported()` is true for `Unknown`, `Unsupported`, `ACC` and `Superseded`.
* `supersededBy()` returns the raw successor names. `supersededByItems()` follows chains through `ui::Selectable::get(name)->supersededBy()` (with cycle protection). It returns `{resolved idents, unresolved names}`.

`isNeedreboot()` is **not** a vendor-support property. It is `Solvable::isNeedreboot()`, driven by the needreboot spec (§5.3).

---

## 17. `zypp/zypp/misc/`: loading helpers and testcases

| File | What it does |
|---|---|
| `DefaultLoadSystem.{h,cc}` | `misc::defaultLoadSystem(root="/", flags)`: creates the `ZYpp` instance and loads the full system. Used by tools and tests |
| `LoadTestcase.{h,cc}` | Parses a solver testcase directory: `solver-test.xml` (Helix, checked **first**) or `zypp-control.yaml` (YAML) |
| `TestcaseSetup.{h,cc}` (+ `TestcaseSetupImpl.h`) | The parsed `<setup>`: arch, system repo, channels, locales, vendors, modalias, multiversion, locks, resolver flags. `applySetup()` puts it into the live pool and config |
| `HelixHelpers.h` | libxml2 `XmlNode` wrapper, `parseSetup()` and `parseTrial()` for the Helix XML format (`YamlTestcaseHelpers.h` is the YAML counterpart) |
| `PoolInstallState.{h,cc}` | `misc::poolInstallState(PoolItem)` / `(Selectable)`: user-facing install state flags for tooling (MCP / REST) |

**`defaultLoadSystem`**:
1. With *no* flags and a non-root euid, it adds `LS_NOREFRESH` automatically.
2. It throws if `root` is not a directory, or if a `ZYpp` instance already exists ("Call this method earlier").
3. `LS_READONLY` → `zypp_readonly_hack::IWantIt()` (no zypp lock).
4. It runs `initializeTarget(root)` and `target()->load()`.
5. Unless `LS_NOREPOS`: for every enabled repo in `RepoManager(root).knownRepositories()`:
   * if it is cached and needs a refresh (plaindir repos always do) and refresh is allowed: `cleanCache` + `refreshMetadata`;
   * `buildCache` if it is not cached;
   * `loadFromCache`. Any failure is **rethrown**: a single broken repo aborts the whole load.

   Services are not refreshed (`\todo` in the header).

**Testcases**:
* `LoadTestcase::loadTestcaseAt(dir, &err)` → `setupInfo()` (`TestcaseSetup`) and `trialInfo()` (a vector of `TestcaseTrial`, each a tree of generic `Node{name, value, properties, children}`). The trial nodes (`install`, `lock`, `distupgrade`, …) are interpreted by the consumers: `zypp/tools/` deptestomatic-style runners, `zypp-solver-testcase-runner`, and `zypp-logic/tests/lib/TestSetup.h::loadTestcaseRepos`.
* Helix `<channel name file type priority>` always produces a `TestcaseRepoType::Helix` repo. The `type` attribute is read but **ignored**. `Testtags` repos only come from YAML. `<source url name>` produces a `Url` repo with priority 99.
* `TestcaseSetup::applySetup(RepoManager&)` (historic) applies arch, repos and hardware info:
  * The arch goes through `ZConfig::setSystemArchitecture` **and `setenv("ZYPP_TESTSUITE_FAKE_ARCH")`**, which disables commit in that process (§4.2).
  * `hardwareInfo` sets `ZYPP_MODALIAS_SYSFS`.
  * Helix/testtags repos are inserted straight into the sat pool (`reposInsert`, then `addHelix`/`addTesttags`; recall that those return 0 even on failure, §5.1).
  * `Url` repos go through `refreshMetadata` + `buildCache` + `loadFromCache`. A `testcase:<rel-path>` URL is resolved against the testcase dir, with gpgcheck off.
* `applySetup(manager, ApplySetupFlags)` additionally applies:
  * `AS_LOCALES`: `initRequestedLocales(current ∪ removed)` followed by `setRequestedLocales(current ∪ added)`, so the tracker's added/removed sets are reproduced;
  * `AS_AUTOINSTALLED`, `AS_VENDOR_LISTS` (`VendorAttr::noTargetInstance()`), `AS_MODALIAS`, `AS_MULTIVERSION`, `AS_LOCKS`, `AS_SOLVER_FLAGS`.
  * `AS_UNIVERSE` = everything except solver flags. `AS_ALL` = everything.

**`PoolInstallState`** flags: `NotInstalled=0`, `AutoInstalled`, `UserInstalled`, `OtherVersionInstalled` (always combined with Auto or User), `Satisfied`/`Broken` (pseudo-installed kinds only, from `validate()`), `Locked`, `Retracted`. Quirks:
* `Retracted` is set from `isBlacklisted()`, so **PTFs are reported as "Retracted" too**.
* The `PoolItem` overload uses the item's own `isBlacklisted()`. The `Selectable` overload uses `hasBlacklistedInstalled()` only. The two can disagree for the same package.
* In the `Selectable` overload, `OtherVersionInstalled` is set whenever no *identical* available exists, which includes "no repo at all" (zypper's foreign-installed case).

---

## 18. `sat::Transaction` and how commit uses it

Files: `zypp/zypp/sat/Transaction.{h,cc}`. It wraps a libsolv `::Transaction`.

### 18.1 Building it

`Resolver::getTransaction()` (`zypp/zypp/solver/detail/Resolver.cc` ~l.437) does `sat::Transaction(loadFromPool)`, then `ret.autoInstalled(_satResolver->autoInstalled())` ("FIXME: ugly way of pushing autoInstalled"). The `loadFromPool` ctor:

1. Builds a libsolv decision queue from **every `PoolItem` whose `ResStatus::transacts()`**. Installed items are pushed as `-id` (erase) and others as `+id` (install). So the transaction reflects the *current pool status* (possibly modified by the application after solving), not the solver's raw result (bnc#702576 "Always compute transaction from pool").
2. Builds a noobsoletes map from `multiversionList()` (`SOLVER_NOOBSOLETES`), so multiversion installs become `MULTIINSTALL` instead of updates.
3. Runs `transaction_create_decisionq(pool, decisionq, noobsmap)`.
4. For each step:
   * Non-package items with a buddy (products → release package) get a `_linkMap` entry, so their **step stage follows the buddy's** (they share a `ResStatus`).
   * `@System` steps of type ERASE are remembered in `_systemErase`.
   * Every `@System` step gets **post-mortem data** (ident, edition, arch) in `_pmMap`. After commit, `@System` is re-read and these solvables disappear. `Step::ident()`, `edition()` and `arch()` keep working, and `Step::satSolvable()` returns `noSolvable`.
5. A `SerialNumberWatcher` on `sat::Pool::serial()` is set up. `valid()` / `operator bool` become false as soon as the pool content changes (repo load or unload). After that, all step data is meaningless.

### 18.2 Step types and stages

| `StepType` | libsolv (`transaction_type(…, SOLVER_TRANSACTION_RPM_ONLY)`) | Meaning |
|---|---|---|
| `TRANSACTION_IGNORE` `[ ]` | everything else | No rpm action: implicit erases due to obsoletes/updates (the old version is replaced by rpm), patches, patterns, products |
| `TRANSACTION_ERASE` `[-]` | `SOLVER_TRANSACTION_ERASE` | Explicit rpm `-e` |
| `TRANSACTION_INSTALL` `[+]` | `SOLVER_TRANSACTION_INSTALL` | Install or update (`-U`) |
| `TRANSACTION_MULTIINSTALL` `[M]` | `SOLVER_TRANSACTION_MULTIINSTALL` | Install alongside (`-i`) |

The post-mortem `stepType()` of a deleted `@System` solvable is `ERASE` if it was in `_systemErase`, otherwise `IGNORE`.

`StepStage` flags are `STEP_TODO` (default), `STEP_DONE` and `STEP_ERROR`. They are kept in two `unordered_set`s, `_doneSet` and `_errSet`, keyed by the (buddy-resolved) solvable id.

Iteration:
* `begin()`/`end()` visit all steps, including IGNORE ones.
* `actionBegin(StepStages filter)` / `actionEnd()` / `action(filter)` / `actionEmpty(filter)` / `actionSize(filter)` skip IGNORE steps and optionally filter by stage. Example: `actionEmpty(~STEP_DONE)` asks "is anything left that isn't done?".
* `find(Solvable | PoolItem | ResObject)` is a **linear scan** of the step array. Calling it in a loop over steps is O(n²).
* `installedResult(Queue&)` → `transaction_installedresult`.

### 18.3 Ordering and commit

`order()` calls `transaction_order(trans, 0)` once (`_ordered` guard), so calling it again is cheap. It returns false if the transaction is no longer `valid()`. Ordering invalidates outstanding iterators. An `#if 0` block shows how libzypp could do its own ordering with `transaction_order_add_choices`; it is not used. libsolv ordering already groups by media number.

`TargetImpl::commit` (`zypp/zypp/target/TargetImpl.cc` ~l.1445):

```
ZYppCommitResult result( root() );
result.rTransaction() = pool_r.resolver().getTransaction();
result.rTransaction().order();
steps = result.rTransactionStepList()     // the todo list, a vector<Transaction::Step>
   restrictToMedia ? copy until first item with mediaNr() > 1 : copy all steps
… download (DownloadInHeaps/InAdvance/AsNeeded) …
for each step: TRANSACTION_IGNORE package = obsoleted by rpm → STEP_DONE, continue
               install/erase via rpm (classic) or zypp-rpm (single transaction),
               step.stepStage( STEP_DONE | STEP_ERROR )
autoinstalled: result.rTransaction().autoInstalled() → Pool::setAutoInstalled
logPatchStatusChanges(): if !actionEmpty(~STEP_DONE) (commit incomplete) it
               reloads the target and recomputes the patch status changes
               before writing them to the history log
```

* `ZYppCommitResult` exposes `transaction()` and `transactionStepList()` to applications. zypper's summary and its error reporting read the step stages from there. The JSON for commit plugins / history is built by `toJSON(Transaction::Step)` (`TargetImpl.cc` ~l.158; Fate#316203).
* `restrictToMedia` compares against the literal `1`, not against `policy_r.restrictToMedia()`. That is only correct for media 1 (unverified whether other values were ever supported).

---

## Pitfalls & special cases

* **There is exactly one libsolv pool.** It is owned by `zyppng::sat::StringPool`, and both the string layer and `sat::PoolImpl` share it. Ids from it are meaningless elsewhere and become invalid after `pool_freeallrepos(reuseIds=true)` (last repo removed → `serialIDs()` bump).
* The disttype must be `DISTTYPE_RPM` (libzypp#726); otherwise version comparison changes on non-rpm-default libsolv builds.
* libzypp constants are compile-time checked against libsolv's (`PoolImpl.cc` MPL asserts). A libsolv update can break the build there.
* After static destruction, `PoolMember::myPool()` throws. Code running at exit must check `poolValid()`.
* Non-system repos lose all solvables of incompatible arch at load time (`_postRepoAdd`). Changing `ZConfig::systemArchitecture()` afterwards does not bring them back; reload the repos.
* `ZYPP_TESTSUITE_FAKE_ARCH` overrides the arch and disables commit.
* The Arch table is close to the 64-bit limit (about 61 built-ins). `defCompatibleWith` takes at most 10 compat archs.
* srcpackages are `name REL_ARCH src` internally and share their ident with packages. `ByIdent` and `_id2item` use **negative ids** for them.
* Kind detection depends on the `kind:` prefix and on "no `:` in package names". `ResKind::explicitBuiltin` must keep guaranteeing the `:`.
* `Capability` parsing turns anything unparsable into a NAMED cap (by design, for search strings). `name.arch` is only split for built-in arches.
* `Capability::c_str()` on reldeps returns a pointer into a 5-slot round-robin static buffer: not thread-safe, short lifetime.
* `Capabilities` for requires *includes* prerequires; `dep_prerequires()` is empty if the marker is missing. Use `const_iterator::tagged()`.
* `Edition::match` is not transitive. Never use it for ordering.
* `ResStatus` State is immutable. Locks can only be set by USER or APPL_HIGH. Lower causers cannot undo higher causers' transactions, and unlocking resets the causer to SOLVER. "Cannot select" bugs usually come from these rules.
* Products share their `ResStatus` with their `-release` package (buddy). Locking or selecting either affects both (bsc#1225267). A missing reference package logs a WAR and leaves the product without a buddy.
* Locks written back from the UI are trivial name queries. Complex queries in `/etc/zypp/locks` survive unlocking. `reapplyHardLocks` only adds locks.
* bsc#1155678: empty RequestedLocales ≠ `{en}`. `en` is not forced.
* Modalias capabilities may be hex-encoded. Filesystem namespace results depend on `/etc/sysconfig/storage` `USED_FS_LIST`.
* Retracted status applies to installed packages through `addIdenticalInstalledToo` (the marker is only in repo metadata). Retracted patches are recognised by `updateStatus=="retracted"`.
* `Selectable` default candidates depend on vendor equivalence (`vendors.d`). Since bsc#1030686, `suse` and `opensuse` are **not** equivalent by default; `opensuse*` is never prefix-matched (bnc#812608).
* `ResPoolProxy` and `PoolItem` objects go stale after pool changes. Re-fetch them; held `PoolItem`s keep stale state alive.
* `_addHelix` and `_addTesttags` always return 0, so load errors there are not reported via the return code.
* Product license: repo licenses are suppressed on update unless they must be accepted (bnc#908976).
* `PoolQuery` ORs everything: strings, attributes and predicates. There is no AND (`requireAll` is a no-op). It is case-insensitive and substring by default, and `SKIP_KIND` means `addAttribute(name,"patch:foo")` matches nothing; use `addDependency(SolvAttr::name, "patch:foo")` (bsc#1035729).
* `PoolQuery::begin()` throws on an invalid regex, while `empty()`, `size()` and `PoolQueryResult` swallow the error and report an empty result. Every `begin()` recompiles and re-resolves repo aliases. A query with unknown repo aliases only matches nothing if *all* aliases are unknown.
* Multi-attribute `PoolQuery`s iterate *all* attributes of *all* solvables (`allAttr`). This is the main performance trap (zypper `search -d`).
* Likely bug: in `PoolQuery::Impl::compile`, per-attribute strings that form a superset of the first attribute's strings are dropped (one-way `set_difference`, §13.3).
* Word match mode and `Match::FILES` are not serialised, so they are lost on a locks-file round trip. `recover()` has inverted `case_sensitive` logic that only produces a spurious WAR (§13.5).
* `PoolQuery::setFilesMatchFullPath` is documented as the default, but `FILES` is not in the default flags, so file searches match basenames unless it is set.
* `Patch::contents()` only lists packages whose name and arch are currently installed, and picks the first provider. It is not the updateinfo package list. `ui::userWantedPackageNames()` inherits this, and it does not include patterns despite its documentation.
* `Patch::categoryEnum("")` logs at INT level and returns `CAT_OTHER`, while an empty severity is `SEV_NONE`. Category and severity strings are lower-cased.
* `Pattern::contents()`/`depends()`/`core()` return every provider (all versions, all repos, installed and available), not a resolved selection. Auto-pattern dependencies live on `autoPackage()`, which must be in the same repo.
* `Package::changelog()` only works for installed packages, and looks them up by **name**. Multiversion packages get an arbitrary version's changelog.
* `Package::cachedLocation()` returns empty if the metadata has no checksum and the repo is not local `dir:`/`file:`.
* `Package::vendorSupport()` reads keywords from identical *repo* solvables. An installed package whose repo is gone reports `VendorSupportUnknown`, and therefore `maybeUnsupported()`.
* `Product::urls()` pairs two independent attribute arrays by position. `Product::flavor()` requires the `product_flavor()` provider to be in the same repo.
* `CapMatch` is tri-state with no bool conversion. `irrelevant` (rich deps) is neither yes nor no.
* `PoolItemBest` keys by ident, so packages and srcpackages of the same name collide. It also considers installed items.
* `sat::Transaction` becomes invalid on any pool serial change. `find()` is a linear scan. Non-package steps share their stage with their buddy. Deleted `@System` steps keep working only through post-mortem `ident/edition/arch`.
* The transaction is built from `ResStatus::transacts()` of all PoolItems, not from the solver result. Status changes made by the application after `resolvePool()` do end up in the commit (bnc#702576).
* `TestcaseSetup::applySetup` sets `ZYPP_TESTSUITE_FAKE_ARCH` when the testcase has an arch, which disables commit for the rest of the process. Helix `<channel type=…>` is ignored (always Helix).
* `misc::defaultLoadSystem` rethrows on the first repo that fails to load and never refreshes services. It must be called before any `getZYpp()`.
* `misc::poolInstallState` reports PTFs as `Retracted`, and the PoolItem and Selectable overloads use different blacklist tests.

## Where to start when debugging

* **Logs**:
  * `zypp::satpool`: repo add/remove (`_createRepo`, `_addSolv` … with names), `pool_createwhatprovides...`, `pool_set_languages`, requested-locale changes, multiversion matches.
  * `libsolv`: libsolv's own output.
  * `solvidx`: solv.idx generation.
  * The default MIL log (`/var/log/zypper.log`) shows `Apply/Re-apply N HardLockQueries` and `HardLockQueries match N Solvables`, buddy assignment (`... has buddy ...`), and product reference warnings.
* **Env vars**:
  * `LIBSOLV_DEBUGMASK=<mask>`, `ZYPP_LIBSOLV_FULLLOG=1` (debug level 3), `ZYPP_FULLLOG=1` (level 2).
  * `ZYPP_TESTSUITE_FAKE_ARCH=<arch>` to simulate another architecture (read-only use).
* **Dumping**:
  * `operator<<` works on `sat::Pool` (serial, capacity, repos, solvables), `Solvable`, `PoolItem` (status + object), `ResStatus`, and `Capability`.
  * `dumpOn(str, solvable)` prints all dependencies. `dumpOn(str, cap)` uses libsolv notation. `dumpCap{cap}` prints a rich-dep tree.
  * `ResStatus::bitfield()` gives the raw bits.
* **Tools** (`zypp/tools/`): `zypp-NameReqPrv` queries names, requires and provides against a pool (it honours `ZYPP_TESTSUITE_FAKE_ARCH`). Solver testcases (`zypper -v --debug-solver`, or the `zypp-solver-testcase-runner` in the zypp-stack) load a pool from testtags/helix via `Pool::addRepoHelix` / `_addTesttags` (see [Testing](11-testing.md)).
* **Caches**:
  * Check `<root>/var/cache/zypp/solv/@System/{solv,cookie,solv.idx}` and the per-repo `solv` files. Deleting them forces a rebuild (`rpmdb2solv`).
  * A `repositoryToolVersion` mismatch triggers an automatic rebuild.
* **Tests**: the unit tests under `zypp/tests/` cover the sat layer, Capability, Edition, Arch, ResStatus, PoolQuery, Locks, VendorAttr and Selectable. Find the specific test sources with `ls zypp/tests/zypp zypp/tests/sat`. Many of them load a pool from `data/` testcases.
* **Typical questions**:
  * "Package not visible" → check the arch filter (`_postRepoAdd`), check the repo loaded (`sat::Pool` dump), check the kind prefix.
  * "Can't (un)select" → print `PoolItem` status and causer, then check locks and buddy.
  * "Wrong candidate" → `Selectable` `AVOrder` (priority, then arch, then edition) plus vendor equivalence.
  * "Language packages not installed or removed" → requested-locale tracker (`added/removed`), `nsCallback` LANGUAGE.
  * "Search/lock matches too much or too little":
    1. Print the query with `MIL << query` (`Impl::asString` shows kinds, repos, strings, attrs, predicated entries, and the "last attribute matcher compiled" list, including `+(predicate)`. The compiled list is filled only after a `begin()`; before that it reads "not yet compiled").
    2. Iterate with `dumpOn(str, it)` to see `matches()` per solvable.
    3. Remember the default `SUBSTRING|NOCASE|SKIP_KIND` and the OR semantics.
    4. For locks, `serialize()` the query and diff it against `/etc/zypp/locks` (log group `PoolQuery`).
  * "Lock reappears / can't be removed" → compare with `PoolQuery::operator==` semantics: exact≡glob only for a single name attribute (bnc#792901), comment ignored, and complex-lock rewriting in `recover` (bsc#1112911).
  * "Patch shows wrong packages / isn't needed" → `Patch::contents()` relevance rule (installed name and arch), then `PoolItem::isNeeded/isBroken/isSatisfied`. These come from `EstablishedStates`, not from the patch data. Dump the raw `update:collection` with `sat::LookupAttr q(SolvAttr::allAttr, patch)`.
  * "Pattern content wrong" → check `isAutoPattern()` / `autoPackage()` (same-repo rule) and then `depends()` on the auto package.
  * "Commit order or step results" → `dumpOn(str, sat::Transaction)` prints every step with `[+]/[-]/[M]/[ ]` and stage. `ZYppCommitResult` gives the step list after commit. Check `valid()` if steps look empty.
  * "Support level shown as unknown" → `Package::keywords()` of the identical *available* solvable; `support_*` keywords come from repo metadata only.
* **Attribute exploration**: `sat::LookupAttr q(sat::SolvAttr::allAttr, solvable); for (auto it = q.begin(); it != q.end(); ++it) MIL << it.inSolvAttr() << " = " << it.asString();`. For sub-structures, use `it.subBegin()/subEnd()`. Repo-level data: `sat::LookupRepoAttr(attr, repo)`.
* **Testcases**: `misc::testcase::LoadTestcase` + `TestcaseSetup::applySetup(manager, AS_ALL)` reproduce a `zypper --debug-solver` dump in-process. Watch out for the fake-arch commit lock.
