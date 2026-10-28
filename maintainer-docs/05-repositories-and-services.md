# Repositories and Services

This chapter covers how libzypp keeps track of repositories and services. That includes the `.repo` / `.service` files, the
on-disk caches (raw metadata, solv files, packages), how a refresh is decided and run, how raw metadata becomes a
solv file, how services (RIS and plugin) add and maintain repos, how repo variables are expanded, and how packages are later
fetched from a repo. GPG details are covered in [Security / GPG / Keyring](06-security-gpg-keyring.md). Media access
is covered in [Media and Downloads](07-media-and-downloads.md).

---

## 1. Where the code really lives (migration state)

The most important fact first: **the public `zypp::RepoManager` no longer contains any logic.**

```
zypp/zypp/RepoManager.{h,cc}          public, ABI-stable API (zypper, YaST, PackageKit)
   │  RepoManager::Impl holds a zyppng::RepoManagerRef
   │  every method: return _pimpl->ngMgr().foo(...).unwrap();
   ▼
zypp/zypp/ng/repomanager.{h,cc}       class zyppng::RepoManager  (the real implementation)
   │  uses RefreshContext + "workflows"
   ▼
zypp/zypp/ng/repo/refresh.{h,cc}               zyppng::repo::RefreshContext (tmp dir, policy, raw cache path)
zypp/zypp/ng/repo/downloader.{h,cc}            zyppng::repo::DownloadContext
zypp/zypp/ng/repo/workflows/repomanagerwf.cc   probe, checkIfToRefresh, refreshMetadata, buildCache,
                                               addRepository(ies), refreshGeoIPData
zypp/zypp/ng/repo/workflows/repodownloaderwf.cc  downloadMasterIndex (+ signature hooks), repoStatus, download
zypp/zypp/ng/repo/workflows/{rpmmd,susetags,plaindir}.cc   per-format status + download
zypp/zypp/ng/repo/workflows/serviceswf.cc      probeServiceType, fetchRepoListfromService, refreshService
zypp/zypp/ng/workflows/{repoinfowf,keyringwf,signaturecheckwf,checksumwf,downloadwf}.cc
zypp/zypp/ng/media/provide.{h,cc}     zyppng::Provide: a *synchronous facade* over legacy MediaManager
zypp/zypp/ng/context.{h,cc}           zyppng::Context (defaultContext() bridges to the ZYpp singleton)
```

Points to keep in mind:

* The "NG" RepoManager is **physically inside the legacy tier** (`zypp/zypp/ng/`), not in `zypp-logic/zypp/ng/`
  and not in `zyppng/`. `zyppng/` (the C++20 module tree) has **no** repository manager yet. `grep RepoManager zyppng`
  finds only a media test. So the code that zypper actually runs is `zypp/zypp/ng/repomanager.cc` and the workflows.
* The workflows are written once in a sync/async-agnostic style (`MaybeAwaitable<T>`, `zypp_co_return`,
  `zypp_co_await`, `joinPipeline`). In the legacy build `zypp/zypp-core/ng/async/awaitable.h` sets
  `constexpr bool ZYPP_IS_ASYNC = false`, so everything runs synchronously. `ZYPP_ENABLE_ASYNC` is only defined by
  `zyppng/CMakeLists.txt`. Code under `#ifdef ZYPP_ENABLE_ASYNC` (e.g. the coroutine `repo2Solv`, the
  `Process`-based plugin runner in `serviceswf.cc`) is **not** compiled into libzypp.so. The `#else` branches with
  `ExternalProgram` are what runs.
* Errors move as `zyppng::expected<T>` holding an `std::exception_ptr`. The legacy facade calls `.unwrap()`, which
  rethrows the stored exception. So legacy callers still see the same `zypp::repo::Repo*Exception` types that
  are documented in `RepoManager.h`.
* `zypp::RepoManager::RepoManager(opt)` uses `zyppng::Context::defaultContext()`. `zyppng::RepoManager::create()`
  runs `initialize()` (read services, then repos). Any failure there throws from the legacy ctor.
* `zypp::RepoManager::Impl::clone()` (RWCOW) creates a **new** NG manager, which re-reads all files from disk.
  It does not copy in-memory state.

### Legacy facade quirks (`zypp/zypp/RepoManager.cc`)

| Legacy method | Notes |
|---|---|
| `refreshMetadata(info, policy, progress)` | **Ignores `progressrcv`** (passes `nullptr`). Wraps the call in `media::ScopedDisableMediaChangeReport` if `info.repoOrigins().hasFallbackUrls()`, so the user is not asked to insert media when other URLs could be tried. Does **not** build the solv cache. |
| `cleanMetadata/cleanPackages/cleanCache/loadFromCache/cleanCacheDirGarbage/addRepositories/modifyRepository` | Progress receiver ignored. |
| `buildCache`, `addRepository`, `removeRepository` | Progress adapted via `zyppng::ProgressObserverAdaptor` + `callback::SendReport<ProgressReport>`. |
| `addRepository(info)` / `modifyRepository(alias, info)` | `const_cast` the caller's `RepoInfo` and inject `filepath`, `metadataPath` and `packagesPath` back into it ("We should fix the API"). Callers depend on this. |
| `makeStupidAlias(url)` | Still implemented in the facade: `scheme-host-<8 hex of sha1(serial+url)>`. |
| `readRepoFile(url)` (free function) | `RepoManagerWorkflow::readRepoFile(defaultContext, url)`. |
| `refreshGeoIp(urls)` | Result is discarded. |

`zyppng::RepoManager` also has a vector overload, `refreshMetadata(std::vector<RepoInfo>, ...)`, which runs refresh **and** `buildCache(BuildIfNeeded)` for each repo. Only NG code uses it (e.g. the `refresh-ng` devtool).

---

## 2. Configuration objects

### 2.1 `RepoManagerOptions` (`zypp/zypp/RepoManagerOptions.{h,cc}`)

A plain struct. The default ctor takes values from `ZConfig::instance()`, each prefixed with `root_r` via
`Pathname::assertprefix`:

| Field | ZConfig source | Usual default |
|---|---|---|
| `repoCachePath` | `repoCachePath()` | `/var/cache/zypp` |
| `repoRawCachePath` | `repoMetadataPath()` | `/var/cache/zypp/raw` |
| `repoSolvCachePath` | `repoSolvfilesPath()` | `/var/cache/zypp/solv` |
| `repoPackagesCachePath` | `repoPackagesPath()` | `/var/cache/zypp/packages` |
| `knownReposPath` | `knownReposPath()` | `/etc/zypp/repos.d` |
| `knownServicesPath` | `knownServicesPath()` | `/etc/zypp/services.d` |
| `pluginsPath` | `pluginsPath()` | `/usr/lib/zypp/plugins` |
| `probe` | `repo_add_probe()` | (zypp.conf `repo.add.probe`) |
| `servicesTargetDistro` | — | empty → `Target::targetDistribution()` is used |
| `rootDir` | the `root_r` argument | |

The defaults come from ZConfig. See [Configuration](09-configuration-and-environment.md).
`RepoManagerOptions::makeTestSetup(root)` puts everything below one directory (`raw`, `solv`, `packages`,
`repos.d`, `services.d`, `plugins`). Tests use it heavily.

**Root prefix caveat:** only the RepoManager paths get the prefix. Repo variables are resolved through
`ZConfig::repoManagerRoot()` (see §6). Plugin services are run chrooted to `rootDir` (see §8).

### 2.2 Per-repo cache paths (`zypp/zypp/ng/repomanager.h`)

```
rawcache_path_for_repoinfo      = opt.repoRawCachePath      / escaped_alias   (tmp repo: info.metadataPath())
rawproductdata_path_for_repoinfo= rawcache / info.path()
packagescache_path_for_repoinfo = opt.repoPackagesCachePath / escaped_alias   (tmp repo: info.packagesPath())
solv_path_for_repoinfo          = opt.repoSolvCachePath     / escaped_alias   (tmp repo: metadataPath().dirname()/"%SLV%")
```

* `isTmpRepo(info)` means `filepath().empty() && usesAutoMetadataPaths()`. A metadataPath ending in `/%AUTO%` makes
  RepoInfo resolve `metadataPath()` to `<dir>/%RAW%` and `systemPackagesPath()` to `<dir>/%PKG%`
  (`RepoInfo::Impl` in `zypp/zypp/RepoInfo.cc`). This lets callers use repos that are not under RepoManager control.
* `escaped_alias()` replaces `/` with `_` (`zypp/zypp/repo/RepoInfoBase.cc`). `filenameFromAlias()` does the same for
  `.repo`/`.service` file names. `generateNonExistingName()` appends `_1`, `_2`, ... if the name is taken.
* `assert_alias()` rejects empty aliases and aliases starting with `.` (bnc#473834).

### 2.3 On-disk layout

```
/etc/zypp/repos.d/<alias>.repo[_N]     INI, one or more [alias] sections per file
/etc/zypp/services.d/<alias>.service   INI, one or more services per file
/etc/zypp/vars.d/<name>                repo variables (first line = value)
/usr/lib/zypp/plugins/services/*       plugin services (executables)
/usr/lib/zypp/plugins/repoverification/*   repo verification plugins
/usr/lib/zypp/plugins/appdata/*        appdata plugins (run from ~RepoManager)
/var/cache/zypp/raw/<alias>/           raw metadata (repodata/repomd.xml | content+media.1 | cookie)
        .repo_gpgcheck                 symlink -> "true"/"false"/"indeterminate" (validRepoSignature)
        mirrorlist, mirrorlist.cookie  cached mirror list (RepoMirrorList)
/var/cache/zypp/solv/<alias>/solv      libsolv file built by repo2solv
                    /solv.idx          content digest for zypper bash completion
                    /cookie            RepoStatus of raw metadata the solv was built from
/var/cache/zypp/solv/@System           system solv (never auto-removed)
/var/cache/zypp/packages/<alias>/      downloaded rpms (kept if keeppackages)
/var/cache/zypp/packages/.keep_packages   marker: effectiveKeepPackages() for all repos
/var/cache/zypp/packages/.no_auto_prune   marker: never prune unknown dirs (bsc#1204956)
<packages>/<alias>/.preload            predownload dir (commit preloader)
/var/cache/zypp/geoip.d/<hostname>     GeoIP redirect target (ZConfig::geoipCachePath(), see §11.4)
```

Directory listing: `repositories_in_dir()` only reads files whose extension matches `^\.repo(_[0-9]+)?$`. When not root,
unreadable dirs and files give a `JobReport` warning instead of an exception. `init_knownServices()` does **not**
filter by extension (the regex is commented out), so every file in `services.d` is parsed.

---

## 3. `RepoInfoBase`, `RepoInfo`, `ServiceInfo`

### 3.1 `RepoInfoBase` (`zypp/zypp/repo/RepoInfoBase.{h,cc}`)

Shared by repos and services: `alias`, `escaped_alias`, `name` (raw name with repo variables expanded through
`RepoVariablesStringReplacer`), `rawName`, `label()` (name or alias, depending on ZConfig's repo label setting),
`enabled` (default **true**), `autorefresh` (default **false**), `filepath`. `dumpAsIniOn` writes `[alias]`,
`name=` (raw), `enabled=`, `autorefresh=`.

### 3.2 `.repo` keys (`zypp/zypp/parser/RepoFileReader.cc`)

| Key | Parsing | Notes |
|---|---|---|
| `[section]` | alias | |
| `name` | string | Raw. Variables expanded on read via `name()`. |
| `enabled`, `autorefresh`, `keeppackages` | `str::strToTrue` | |
| `priority` | `strtonum<unsigned>` | `0` → default 99 (`setPriority`). Lower number = higher priority. Only written if != 99. |
| `path` | Pathname | If it contains `../` ("hostile path"), it is made absolute and a `JobReport::warning` is issued. `setPath` always stores `absolutename()`. |
| `type` | **ignored on read** | bsc#1177427: type in .repo is legacy; RepoManager probes. It *is* still written by `dumpAsIniOn` when known. |
| `gpgcheck` | `strToTriBool` | Unset = indeterminate = follow zypp.conf. |
| `repo_gpgcheck`, `pkg_gpgcheck` | **`strToTrue`** (not tri-bool!) | A present key is always definite. |
| `baseurl`, `gpgkey`, `mirrorlist`, `metalink` | multi-line | Continuation lines become URLs. #285: whitespace and/or comma separated lists allowed (dnf compat). For mirrorlist/metalink only the first URL counts. |
| `service` | string | Owning service alias. |
| `proxy` | `host[:port]` | Converted into `proxy=`/`proxyport=` query params on **baseurls only** (not mirrorlist). Not written back as `proxy=`. |
| anything else | `setExtraValue` | Kept as "extra value" and written back. Logged with WAR unless it is a builtin extra (`repo_sigcheck_plugin`). Keys containing `=` are internal and never written. |

`targetDistribution` is set only from repoindex.xml (services), not from `.repo` files.
`dumpAsIniOn` writes a "maintained by the 'X' service" comment for service repos (libzypp/#638). It uses
`hotfix1050625::asString(url)` so that passwords stored in URLs survive.

### 3.3 RepoInfo semantics worth knowing (`zypp/zypp/RepoInfo.cc`)

* **Type**: `type()` probes the cache lazily (`probeCache(metadataPath()/path)`) when `_type == NONE`.
  `setProbedType()` only changes a `NONE` type and is `const` (it mutates through `const_cast`).
* **GPG tri-state** (detail in chapter 06):
  * `gpgCheck()` = raw value or `ZConfig::gpgCheck()`.
  * `repoGpgCheck()` = `gpgCheck() || cfgRepoGpgCheck()`; `repoGpgCheckIsMandatory()` is false only when
    repo_gpgcheck is explicitly off ("unsigned ok").
  * `pkgGpgCheck()` is *enforced* when `gpgCheck()` is on and the repo signature is not valid.
  * `setGpgCheck(GpgCheck::{On,Strict,AllowUnsigned,AllowUnsignedRepo,AllowUnsignedPackage,Default,Off})` sets all three
    raw values together.
  * `validRepoSignature()` is stored as a **symlink** `metadataPath()/.repo_gpgcheck` with target `true|false|indeterminate`.
    It is forced to false when repoGpgCheck is off. A missing link means plaindir or not yet refreshed.
* **URLs**: `baseUrls()` = variables expanded; `rawBaseUrls()` = as configured. `url()` = first baseurl, or else the first
  mirror. `location()` = first baseurl, or else the mirrorlist URL. `addBaseUrl` drops duplicates (raw compare).
  Changing baseurls resets the mirror cache.
* **Mirrors** (`Impl::mirrorUrls()`): fetched through `repo::RepoMirrorList` and cached **in memory for 1 hour**
  (steady clock). `mirrorListUrl()` = `mirrorlist=` if valid, otherwise `metalink=`. **Auto mirrorlist**: if there is no
  mirrorlist/metalink and exactly one baseurl whose host is in `RepoMirrorList::urlSupportsMirrorLink` (currently
  `download.opensuse.org`, `cdn.opensuse.org`), libzypp asks for `<baseurl>/<path>/?mirrorlist`. If that fails, the
  failure is silent (bsc#1243901). An explicit mirrorlist that fails gives a `JobReport::warning` (`repoRefreshMirrorlist`)
  and is retried on the next call. Query params like `proxy` are copied onto the mirror URLs
  (`internal::propagateQueryParams`).
* **`repoOrigins()`** returns a `MirroredOriginSet`: each baseurl is an *authority* endpoint, and mirrors are added as
  endpoints. Refresh and package download walk the origins (see chapter 07 for `MirroredOrigin`).
  Note that `repoOriginsEmpty()` and `repoOrigins()` may trigger a **network fetch** of the mirrorlist.
* **`requireStatusWithMediaFile()`**: for repos whose origins are all downloading schemes, `media.1/media` is left out of the
  status, unless a cached `media.1/media` reports more than one medium (`SUSEMediaVerifier::totalMedia() > 1`).
* **Packages path**: if `packagesPath` is not writable for a non-root user, `packagesPath()` switches to
  `$XDG_CACHE_HOME/zypp/packages/<alias>` (or `~/.cache/...`). `systemPackagesPath()` still returns the configured path.
* `keepPackages()` defaults to false. `effectiveKeepPackages()` is also true if `<packages>/.keep_packages` exists.
* `hasContent(keyword)` reads `<content>` from repomd.xml or `REPOKEYWORDS` from a susetags `content` file directly in the raw cache
  (marked HACK in the code). Licenses are read with `tar` from `license*.tar.gz` (`needToAcceptLicense`: no
  `no-acceptance-needed` entry in the tarball means acceptance is required).
* `repoStatusString()` (mirrorlist URL, or else first baseurl) feeds `RepoStatus(RepoInfo)`. So **changing the URL changes
  the repo status** and forces a refresh/rebuild.

### 3.4 `ServiceInfo` and `.service` keys (`zypp/zypp/ServiceInfo.*`, `zypp/zypp/parser/ServiceFileReader.cc`)

Keys: `name`, `url`, `enabled`, `autorefresh`, `type` (`ris` / `plugin` / ...; see `repo/ServiceType.{h,cc}`),
`ttl_sec`, `lrf_dat` (last refresh date), `repostoenable`, `repostodisable`, and the `repo_<N>` / `repo_<N>_enabled|autorefresh|priority` group, which stores
**`RepoStates`**: what the service last asked for, per repo. Unknown keys are logged with ERR and ignored. They are **not**
kept, unlike `.repo` extras. A callback returning false throws `AbortRequestException`.

---

## 4. RepoManager lifecycle

### 4.1 Initialization (`zyppng::RepoManager::initialize`)

```
init_knownServices()
  ├─ every file in knownServicesPath → ServiceFileReader → _services
  └─ repo::PluginServices(pluginsPath/"services") → one ServiceInfo (type PLUGIN) per executable
init_knownRepositories()
  ├─ repositories_in_dir(knownReposPath) → set metadataPath/packagesPath → _reposX
  ├─ repos whose service() is not a known service → removeRepository() + JobReport warning
  │     ("Unknown service '%1%': Removing orphaned service repository '%2%'")
  └─ unless read-only mode (zypp_readonly_hack::IGotIt, bsc#1210740):
       cleanupNonRepoMetadtaFolders() for raw, solv and (unless .no_auto_prune) packages cache
         - ONLY if the configured path equals the builtin default (with rootDir prefix) – bnc#891515
         - deletes every entry whose name is not a known escaped alias, except "@System"
         - files/symlinks are removed too (bsc#1178966)
```

So **merely creating a RepoManager can delete cache directories and orphaned service repos** (and rewrite `.repo`
files). Read-only tools must set the read-only hack.

### 4.2 Destructor: appdata plugins

`~RepoManager` runs every executable in `pluginsPath/appdata` when the repo set changed (`_reposDirty`, which is set by any
`reposManip()` call, including successful refreshes of non-tmp repos) **or** `ZYPP_PLUGIN_APPDATA_FORCE_COLLECT` is true.
This only happens as root with rootDir `/` or empty. The arguments are `-R alias -t type -p <metadataPath/path>` for each enabled repo
(bsc#1197684: path points at the repodata dir inside the cache). Exceptions are swallowed.

### 4.3 Add / modify / remove

* `addRepository` (`AddRepoLogic`): rejects duplicates (`RepoAlreadyExistsException`). If `forcedProbe` (or, when that is
  indeterminate, `options.probe`) is set, it probes the first URL. NONE → `RepoUnknownTypeException`. Then `addProbedRepository`
  writes a **new** file `<alias>.repo`, sets the paths, collects credentials from the URLs (`UrlCredentialExtractor` →
  credentials.d, URLs are not changed in the file), and logs to `HistoryLog::addRepository`.
* `addRepositories(url)`: expands variables in the URL, downloads the .repo file through `Provide`, fails on any alias clash
  *before* writing, and saves all repos into one file named after the URL basename.
* `modifyRepository`: re-reads the repo's file and **rewrites it in place with `std::ofstream`** (no tmp+rename, so it is not
  atomic). Renaming onto an existing alias throws. Disabling a repo deletes `solv.idx`. Credentials are collected and history is logged.
  The raw/solv caches are **not** renamed when the alias changes.
* `removeRepository`: rewrites or unlinks the file (bsc#984494: a missing file is fine). Then it runs `cleanCache`, `cleanMetadata`
  (which **also wipes the whole GeoIP cache**), `cleanPackages(isAutoClean=true)` (respects `.no_auto_prune`). It
  matches by alias; an empty alias matches the first repo ("TODO match by url").
* `cleanCacheDirGarbage`: removes subdirs of raw/packages/solv that match no known alias **and are older than one day**.

---

## 5. Refresh: deciding, downloading, caching

### 5.1 Overview

```
refreshMetadata(info, policy)
  refreshGeoIPData(baseUrls + mirrorlist)      errors ignored
  RefreshContext::create                       tmp dir = TmpDir::makeSibling(rawCachePath)
                                               (non-root fallback: plain TmpDir)
  refreshMetadataLogic: firstOf(repoOrigins())  ── for each MirroredOrigin until success:
     Provide::prepareMedia(origin)
     RefreshMetadataLogic
        assert_alias, assert_urls
        checkIfToRefreshMetadata  ────────────►  REPO_UP_TO_DATE / REPO_CHECK_DELAYED → done
        REFRESH_NEEDED:
          non-root and raw cache parent not writable → RepoNoPermissionException (stop: no other URL tried)
          probeRepoType (ALWAYS re-probe, bsc#1048315); NONE → RepoUnknownTypeException
          RepoDownloaderWorkflow::download (rpmmd | susetags | plaindir) into tmp dir
          RefreshContext::saveToRawCache()  → filesystem::exchange(tmp, raw)
  all origins failed → RepoException("Failed to retrieve new repository metadata.")
                       first error remembered, others added to history
```

### 5.2 `checkIfToRefreshMetadata` (`CheckIfToRefreshMetadataLogic`, `repomanagerwf.cc`)

These are checked in order:

1. `oldstatus = RepoManager::metadataStatus(info)` (local raw cache). If empty → **REFRESH_NEEDED**.
2. Volatile scheme (CD/DVD) → **REPO_UP_TO_DATE** ("Never refresh CD/DVD").
3. `RefreshForced` → **REFRESH_NEEDED**.
4. Local scheme (`dir:`, `file:`, ...) → policy becomes `RefreshIfNeededIgnoreDelay`.
5. Unless the policy is IgnoreDelay: **only if `oldstatus == cacheStatus` (solv cookie)** (bsc#1174016: URL changed because of
   repovars, etc.), compare `now - oldstatus.timestamp()` (minutes) with `ZConfig::repo_refresh_delay()`. If it is below the delay → **REPO_CHECK_DELAYED**.
   A negative difference ("refreshed in the future") logs WAR and goes on to check.
6. Probe the type if unknown, then compute the **remote** status with `RepoDownloaderWorkflow::repoStatus`. The result is `RepoStatus(info) && <format status>`:
   * rpmmd: `repodata/repomd.xml` (+ `/media.1/media` if `requireStatusWithMediaFile()`)
   * susetags: `content` + `/media.1/media` (always tried)
   * plaindir: recursive max-mtime of the local directory (needs a local media path)
7. Equal → `touchIndexFile` (touch repomd.xml / content / cookie so the delay timer restarts) → **REPO_UP_TO_DATE**,
   otherwise **REFRESH_NEEDED**.

The legacy `checkIfToRefreshMetadata(info, url, policy)` wraps the single URL in a `MirroredOrigin`.

### 5.3 `RepoStatus` and cookie files (`zypp/zypp/RepoStatus.{h,cc}`)

* `RepoStatus(path)`: for a file, SHA256 of its content + mtime. For a dir, SHA256 of the recursive max mtime + that time.
  `RepoStatus(RepoInfo)`: SHA256 of `repoStatusString()` with timestamp 0.
* Values created by the ctors get a **magic suffix** added to the checksum. The source says: "*Changing magic will at once
  invalidate all solv file caches*". Use this on purpose if the solv format or the parse options change.
* `a && b` merges the checksum sets (the combined checksum is the SHA256 of the joined set) and keeps the newest timestamp.
* Cookie file line: `"<checksum> <time_t>"`. `fromCookieFile` reads both. `fromCookieFileUseMtime` uses the file's mtime
  instead of the stored time. That variant is used for the **plaindir** raw cookie, because the refresh check touches it.
* `metadataStatus()` returns an *empty* status if no metadata is cached (callers rely on this), otherwise
  `<format files> && RepoStatus(info)`.

### 5.4 Downloading the master index and where signature checks hook in

`DownloadMasterIndexLogic` (`repodownloaderwf.cc`) is shared by rpmmd (`repodata/repomd.xml`) and susetags
(`content`). The steps are:

1. If the repo has the extra value `repo_sigcheck_plugin`: create and launch `SigcheckPlugins` (`zypp/zypp/repo/PluginSigcheck.*`,
   bsc#1265223: configured args must not escape the plugin dir).
2. Download the master index (size limit 20 MB, **mirrors not allowed**: the index always comes from the authority URL).
3. `RepoInfoWorkflow::fetchGpgKeys` (keys from `gpgkey=` URLs).
4. Download `<index>.asc` as optional. If its key is unknown, download `<index>.key` as optional. Mirrors are allowed for the key
   only if the repo has no baseurl but a mirrorlist; in that case the user is warned to set `gpgkey=`.
5. Copy everything into the tmp dest dir.
6. **`pluginVerification()`**: repoverification plugins (`zypp/zypp/repo/PluginRepoverification.*`, from
   `pluginsPath/repoverification`). If there is a signature but no key file, the key is exported from the keyring first.
7. **`executeSigcheckPlugins()`**: download the plugin's sig/key extensions (mandatory) and call `plugin.sigcheck()`.
8. **`signatureCheck()`**: only if `repoGpgCheck()`. If the index is signed, or the check is mandatory, it builds `keyring::VerifyFileContext`,
   imports the `.key` file (untrusted), sets the key context to the RepoInfo (bnc#495977), and adds **buddy keys** from repomd
   `<keyhint>`s (`getExtraKeysInRepomd`: rejects unsafe short ids, uses the cache
   `repoManagerRoot/pubkeyCachePath/<file>`, or else downloads it). Then it runs
   `SignatureFileCheckWorkflow::verifySignature`. An unsigned repo with a non-mandatory check gives a WAR and is accepted.
9. `setMetadataPath(destdir)` + `setValidRepoSignature(result)` → writes the `.repo_gpgcheck` symlink in the **tmp**
   dir, which then becomes the raw cache through the exchange.

Then the format workflow downloads the rest:
* **rpmmd** (`rpmmd.cc`): `RepomdFileReader` + `RepomdFileCollector` (`zypp/zypp/repo/yum/`) choose the files
  (deltas from the old cache dir via `deltaDir()`). Every file must be on medium 1, otherwise this throws "*Repo can only require metadata files
  from primary medium.*". Files are fetched with `DownloadWorkflow::provideToCacheDir` (checksummed).
* **susetags** (`susetags.cc`): parses `content` (`ContentFileReader` → `RepoIndex`). It throws if there is no index or no
  meta checksums. It filters `packages.*` translations (always `packages`, `.DU`, `.en`; never `.FL`; others according to
  `repoRefreshLocales()` + `textLocale()` with fallbacks), skips `*.<arch>.pat` files for incompatible archs (bnc#298716),
  and also fetches `license.tar.gz` and the signing keys listed in `content`.
* **plaindir** (`plaindir.cc`): downloads nothing. It writes `RepoStatus(dir)` to `<tmp>/<path>/cookie`. It needs a
  medium with a local path. Plaindir is only probed for non-downloading, non-plugin URLs (bsc#1191286).

`saveToRawCache()` hardlink-copies `mirrorlist`/`mirrorlist.cookie` from the old raw dir if the new dir has none, then
`filesystem::exchange(tmp, raw)`. The old content goes into the TmpDir and is deleted with it.

### 5.5 Probing (`ProbeRepoLogic`)

`dir:` URLs whose path does not exist → NONE right away. Otherwise it tries `repodata/repomd.xml` → RPMMD, then `content` → YAST2.
A `MediaFileNotFoundException` is ignored, while other `MediaException`s are remembered (bnc#335906: broken proxies on ftp) and
thrown at the end if nothing matched. Then plaindir for local dirs, else NONE. The existence checks use
`setCheckExistsOnly` with mirrors disabled. Cache dirs are probed with `RepoManager::probeCache` instead, because cache
paths must not be rewritten by media handling (bnc#946129). Note: the YAST2 branch logs "Probed type RPMMD" (copy-paste in the
log message only).

`RepoType` (`zypp/zypp/repo/RepoType.*`): `RPMMD` ("rpm-md"), `YAST2` ("yast2"/susetags), `RPMPLAINDIR` ("plaindir"),
`NONE`. The type found during refresh is fed back through `RefreshContext::sigProbedTypeChanged`, but the handler in
`repomanager.cc` only builds a modified copy. Writing to `.repo` is commented out ("don't modify .repo in refresh"),
so **probed types are not saved by refresh**.

### 5.6 Building the solv cache (`BuildCacheLogic`)

```
metadataStatus(raw) empty?  → refreshMetadata() first (if raw cache writable, else RepoNoPermissionException)
isCached && cacheStatus == raw status?
   BuildIfNeeded → only create missing solv.idx, done
   BuildForced   → rebuild
cleanCache → assert solv dir (non-root needs write perms)
type NONE → probeCache(productdatapath)
plaindir → attachMedia(info.url()) to get a local path ("FIXME this does only work for dir: URLs")
repo2solv -o <solv> -X [-R <localdir>/<path>] | <productdatapath>
   binary: ZYPP_REPO2SOLV_PATH (build-time) else "repo2solv" if /usr/bin/repo2solv exists else "repo2solv.sh"
   -X = autogenerate patterns from pattern packages; -A dropped (bsc#1104415)
   output lines are logged as WAR and attached to RepoException("Failed to cache repo (%d).") history
   solv unlinked on failure (ManagedFile guard)
updateSolvFileIndex(solv) → solv.idx
setCacheStatus → solv/<alias>/cookie = raw metadata status
```

`loadFromCache` adds `solv` to the sat pool through `Context::satPool().addRepoSolv`. If the solv file's
`repositoryToolVersion` differs from `LIBSOLV_TOOLVERSION` (or loading fails in any other way), it runs **clean + rebuild + reload
automatically**. A missing solv file throws `RepoNotCachedException` first, and the same error handler catches it and tries to rebuild.

Known wart: on error, `BuildCacheLogic`'s `or_else` finishes the progress observer with `Success`.

### 5.7 GeoIP

`RefreshGeoIpLogic`: if `ZConfig::geoipEnabled()`, it collects per downloading origin the first endpoint host listed in
`geoipHostnames()` and tries them in order (`firstOf`) until one answers. It fetches `https://<host>/geoip`, reads `<host>`, and stores it in `geoipCachePath()/<hostname>` (an empty file if there is no answer). Entries
expire after 24 h. It runs before every probe, refresh and refresh check. Errors never fail the refresh. How the
redirect is used belongs to the media layer (chapter 07).

### 5.8 Mirror list cache (`zypp/zypp/repo/RepoMirrorList.cc`)

It caches the list in `<rawcache>/mirrorlist` with `mirrorlist.cookie` (a cookie made from the URL). The cache is reused unless the cookie
differs or the file is older than `repo.refresh.delay` minutes (and the dir is writable). Local mirrorlists are never cached.
The format is detected automatically from the first non-whitespace character: `<` → metalink (`media::MetaLinkParser`), `[` → the JSON
answer from opensuse.org `?mirrorlist`, anything else → a text list. Invalid URLs in a text list → `ParseException`. Details in §11.

---

## 6. Repo variables (`zypp/zypp/repo/RepoVariables.{h,cc}`)

* Syntax (`RepoVarExpand`): `$var`, `${var}`, `${var:-word}`, `${var:+word}`. Names are `[[:alnum:]_]` and case-insensitive.
  Unknown variables are **left as they are** (yum compatible). Backslash escaping only works inside braces.
* Built-ins: `$arch` (= `ZConfig::systemArchitecture()`), `$basearch` (= `arch.baseArch()`), `$releasever`,
  `$releasever_major`, `$releasever_minor`. User definitions: one file per variable in
  `repoManagerRoot()/varsPath()` (vars.d); the value is the first line, not trimmed. A user-defined `arch`/`basearch` wins.
* `$releasever` order: env `ZYPP_REPO_RELEASEVER` (also zypper `--releasever`) always wins. Then the vars.d file. Then
  `Target::distributionVersion(contextRoot)`, which is re-evaluated on **every** lookup (bnc#943563), so a `dup`
  that changes the release is picked up.
* `RepoVarsMap` is a **process-wide singleton**. It is cleared when `ZConfig::repoManagerRoot()` changes (bsc#1237044) or
  when the zypp lock is re-acquired (`zyppintern::repoVariablesReset`). A lookup calls `getZYpp()` as a guard.
  The comment says it should be per-context.
* `RepoVariablesUrlReplacer` expands without username and password, then puts the encoded credentials back. Known legacy
  issue: it also expands inside `proxypass=`.
* Raw vs. expanded values: config writers must use `raw*()` getters. `RepoVariablesReplacedUrl(List)` keeps both. The service
  refresh compares `rawBaseUrls()`.
* `ZYPP_DBG_VAREXPAND` is a compile-time debug switch for the expander.

---

## 7. Services

### 7.1 Types

| Type | Discovery | Repo list source |
|---|---|---|
| `RIS` (Repository Index Service) | `.service` file; `probeServiceType` = `/repo/repoindex.xml` exists | `<url>/repo/repoindex.xml` with `cookies=0` query param (bnc#573897), parsed by `RepoindexFileReader` (`zypp/zypp/parser/RepoindexFileReader.cc`, schema in `parser/schema/repoindex.rnc`). It also provides `ttl`. `type` in repoindex is ignored (bsc#1177427). |
| `PLUGIN` | executables in `pluginsPath/services` (`repo::PluginServices`) | stdout of `/bin/sh -c <script>` run **chrooted to rootDir**. The script path has rootDir removed (bsc#1080693). Output is parsed as a `.repo` file. Exit code != 0 → `ServicePluginInformalException(stderr)`. |
| `NONE` | unknown → probed lazily during refresh (`setProbedType`) | |

For RIS with `file:`/`dir:` URLs, the path gets rootDir prefixed (`adaptServiceUrlToChroot`). Parse errors in
repoindex.xml are converted into `ServicePluginInformalException` (bnc#1116840), because callers expect that type.

### 7.2 `refreshService` (`RefreshServiceLogic`, `serviceswf.cc`)

```
TTL check: ttl && !forceRefresh && !restoreStatus && lrf <= now < lrf+ttl → skip
probe type if NONE
fetch repo list (errors other than ServicePluginInformalException abort; informal → remembered, continue with empty list)
filter by target distro (options.servicesTargetDistro or Target::targetDistribution())
for each service repo:
    alias := "<service>:<alias>", service := <service>
    newRepoStates[alias] = repo
    path → appended to baseurls (path cleared); no baseurl → service rawUrl (+path)
old repos of this service not in the new list → removeRepository
    (if it was enabled although the service had it disabled before → addRepoToEnable(alias) to remember the user choice)
for each new repo: decide enable state (below), add or modify
    modify follows service for: rawName, autorefresh, priority, rawBaseUrls (credentials extracted first),
    gpgkey urls, mirrorlist, metalink; gpg flags only for PLUGIN services
clear reposToDisable (always), store repoStates
save the service file (not for PLUGIN) when modified; ttl → lrf=now
rethrow the remembered ServicePluginInformalException at the end
```

Enable decision:

| Situation | Result |
|---|---|
| `RefreshService_restoreStatus` | Remove from reposToEnable. Follow the service. |
| alias in `repostoenable` | enabled. Removed from the list (one-shot). |
| alias in `repostodisable` | disabled (list cleared after **every** refresh). |
| existing repo, service state unchanged since last refresh (`repoStates`) but user changed enable | **keep the user's choice** |
| service request changed since last refresh | follow the service |

`modifyService` throws `ServicePluginImmutableException` for plugin services. Changing a service's alias or enabled
state updates all its repos: disable → disable all; enable → restore each repo's state from `repoStates`. `removeService`
removes all repos of the service. `refreshServices` copies the service set and refreshes each one through
`transform | join | collect`. The `\todo` says callers should ignore `ServicePluginInformalException`. zypper handles that.

---

## 8. Package retrieval from repos (`zypp/zypp/repo/`)

* **`RepoMediaAccess` / `provideFile`** (`RepoProvideFile.{h,cc}`): keeps one `MediaSetAccess` per origin authority URL,
  sets the media label and `SUSEMediaVerifier` for the repo, and downloads through a **`Fetcher`**. Cache paths: the writable packages dir first,
  then the configured read-only dir, then `.preload` (`Fetcher::CleanFiles`). The comment about switching to a tmp dir when
  the destination is not writable (bnc#755239) is stale: the code only calls `assert_dir`. The non-root fallback really lives in
  `RepoInfo::packagesPath()` (XDG cache, §3.3). bsc#1253740: resource names with `../` (used by Tumbleweed) are mapped to paths below the cache
  dir. `SUSEMediaVerifier` (bsc#1180851) relaxes verification when only one non-volatile medium is in the set.
* **Metadata vs. packages**: refresh (sections 5.x) uses `zyppng::Provide` (`zypp/zypp/ng/media/provide.*`, a sync facade over
  `MediaManager`). Packages and `commitpackagepreloader.cc` still use `MediaSetAccess`/`Fetcher`. Both paths
  must work; they are covered in chapter 07.
* **`PackageProvider`** (`PackageProvider.{h,cc}`): `PackageProviderImpl<TPackage>` for Package and SrcPackage
  (`SrcPackageProvider`).
  1. Cache hit in the repo package cache (`cachedLocation`). If the hit is not kept (`!effectiveKeepPackages`), it is deleted after use.
  2. Toplevel cache hit (the default `/var/cache/zypp/packages` tree when the repo uses another one, e.g. the non-root XDG cache
     or a `--root` target), but only if a checksum exists. Full flow in §10.
  3. Otherwise download. `RpmPackageProvider` first tries **delta RPMs** when `download.use_deltarpm` is set and
     (the scheme is downloading or `download.use_deltarpm.always`), deltas exist (`DeltaCandidates`, `PackageDelta`), the base
     version is installed, and `applydeltarpm` is available and `quickcheck`s the sequence (`Applydeltarpm.cc`; bsc#1219442 creates the target
     dir). Deltas are optional resources (bsc#1245672).
  4. **Package signature check** when `info.pkgGpgCheck()` (`packageSigCheck` via RpmDb). The result goes to the
     `pkgGpgCheck` report. A NOKEY result tries to import keys from the repo's `gpgkey=` URLs. The user can choose abort, retry or ignore. Invalid files are removed from
     the cache (bsc#1045735).
* `RepoException` hierarchy (`RepoException.h`): `RepoException` → `RepoNotCached`, `RepoNoUrl`, `RepoNoAlias`,
  `RepoInvalidAlias`, `RepoNotFound`, `RepoAlreadyExists`, `RepoUnknownType`, `RepoMetadata`, `RepoNoPermission`;
  `ServiceException` → `ServiceNoAlias`, `ServiceInvalidAlias`, `ServiceAlreadyExists`, `ServiceNoUrl`,
  `ServicePlugin` → `ServicePluginInformal`, `ServicePluginImmutable`. These types are part of the API. zypper reacts to them by type.

### Other parsers in `zypp/zypp/parser/` (summary; details in §12)

`yum/RepomdFileReader` (also `keyhints()`), `susetags/ContentFileReader` + `RepoIndex`, `RepoindexFileReader`,
`ProductFileReader` (`/etc/products.d` product files), `HistoryLogReader` (`/var/log/zypp/history`), and the
libxml2 wrappers in `parser/xml/`. RNC/RNG schemas for repomd live in `parser/yum/schema/` (`validate-all` script).
Note that the solv file is produced by libsolv's `repo2solv`. libzypp's yum/susetags readers are only used for refresh
decisions (file lists, keyhints), **not** for loading packages into the pool.

---

## 9. Tools

* **`zypp-refresh`** is part of **zypper** (`../zypper/src/zypp-refresh.cc`, man page `zypp-refresh.8`): it refreshes enabled
  autorefresh repos, e.g. from cron.
* **`zypp/tools/refresh-ng`** (`ENABLE_DEVTOOL_REFRESHNG`, default OFF): a demo of the NG vector `refreshMetadata` with
  `UserRequest` handling. **It is stale**: it uses `zyppng::AsyncRepoManager` and `zypp-core/zyppng/...` include paths, which
  no longer exist. Expect it not to compile.
* **`zypp/tools/repomirror`** (`ENABLE_DEVTOOL_REPOMIRROR`, needs notcurses++): an NG `Provide` demo that downloads all packages of a
  repo with checksums. It uses the old `zypp-core/zyppng` include paths as well.
* Other related helpers in `zypp/tools/`: `ToolScanRepos.cc`, `ProvideSignedDirectory.cc`, `DownloadFiles.cc`.

---

## 10. `PackageProvider` in depth (`zypp/zypp/repo/PackageProvider.{h,cc}`)

### 10.1 Who calls it

```
TargetImpl::commit ── CommitPackageCache (ReadAhead unless $ZYPP_COMMIT_NO_PACKAGE_CACHE)
                         └─ RepoProvidePackage::operator()(pi, fromCache)      target/CommitPackageCache.cc
                              ├─ fromCache            → PackageProvider(access, pi, policy).providePackageFromCache()
                              ├─ Package              → DeltaCandidates(pool.knownRepositories, pi.name())
                              │                          PackageProvider(access, pi, deltas, policy).providePackage()
                              └─ SrcPackage (else)    → PackageProvider(access, pi, policy).providePackage()
ZYpp::provideSrcPackage / installSrcPackage (zypper source-install, ../zypper/src/misc.cc)
   └─ TargetImpl::provideSrcPackage → repo::SrcPackageProvider(access).provideSrcPackage()
                                       = access.provideFile(repoInfo, location)   (default policy!)
```

`RepoProvidePackage::Impl` owns one `repo::RepoMediaAccess` for the whole commit, so media handles are reused across
packages (see §10.6). `PackageProviderPolicy` carries one thing only: `queryInstalledCB`. `RepoProvidePackage` sets it to
`QueryInstalledEditionHelper`, which iterates the **rpm database of `/`** (`librpmDb::db_const_iterator it("/")`), not the
target root. That is on purpose: `applydeltarpm` rebuilds the new rpm from the files installed on the running system.
Without a callback `queryInstalled()` returns false, so no delta is ever tried.

Factory (`factory::make`):

| Input | Implementation |
|---|---|
| `Package` + `DeltaCandidates` | `RpmPackageProvider` (delta capable) |
| `Package` without deltas | plain `PackageProviderImpl<Package>` (no delta attempt) |
| `SrcPackage` | `PackageProviderImpl<SrcPackage>` (deltas ignored) |
| anything else | throws "Don't know how to cache non-package" |

### 10.2 `providePackage()` flow

```
newReport()                          one callback::SendReport<DownloadResolvableReport> per call (ScopedGuard)
providePackageFromCache()            Package::cachedLocation() (libsolv lookup of <packagesPath>/<path>/<location>)
   hit  → report.infoInCache(pkg, file)    ← NO start()/finish() for cache hits
          dispose=unlink unless effectiveKeepPackages()
toplevel cache (only if info.packagesPath().dirname() != RepoManagerOptions().repoPackagesCachePath
                and loc.checksum() non-empty)
   <defaultPkgCache>/<packagesPath basename>/<info.path()>/<loc.filename()> exists + checksum ok
        → report.start(pkg, file-url); hardlinkCopy into own packagesPath; report.finish(NO_ERROR)
info.repoOriginsEmpty() → Exception("No url in repository.")      (may fetch the mirrorlist!)
do {                                  retry loop driven by user answers
   report.start(pkg, info.location())
   ret = doProvidePackage()           RpmPackageProvider: deltas first, then full rpm
   catch UserRequestException         → rethrow (abort/skip already decided elsewhere)
   catch RpmSigCheckException         → sig problem was already reported by rpmSigFileChecker;
                                        action RETRY→loop, IGNORE→SkipRequestException, ABORT→AbortRequestException
   catch FileCheckException           → report.problem(INVALID, "seems to be corrupted…") → RETRY/IGNORE/ABORT
   catch Exception                    → report.problem(IO, "Failed to provide Package…"+history) → RETRY/IGNORE/ABORT
} while (_retry)
any escaping exception: a non-empty ret gets dispose=unlink (bsc#1045735: no invalid files stay in the cache)
report.finish(NO_ERROR)               ← only on success; failures end without finish()
```

`DownloadResolvableReport` (`zypp/zypp/ZYppCallbacks.h`) callbacks used here: `infoInCache`, `start`, `progress` (wired
through `ProvideFilePolicy::progressCB` → `DownloadFileReportHack`, see §10.6), `problem` (`NOT_FOUND`/`IO`/`INVALID`, default
answer **ABORT**), `pkgGpgCheck(UserData)`, `finish`, and the delta set `startDeltaDownload`, `progressDeltaDownload`,
`problemDeltaDownload`, `finishDeltaDownload`, `startDeltaApply`, `progressDeltaApply`, `problemDeltaApply`, `finishDeltaApply`.
The plain download progress also reaches any `media::DownloadProgressReport` receiver (zypper shows both).

### 10.3 Signature check hook (`rpmSigFileChecker`)

`doProvidePackage()` puts `rpmSigFileChecker` into `ProvideFilePolicy::fileChecker`. The `Fetcher` runs it in
`Fetcher::Impl::validate()` **after** the checksum check and **before** the file is hardlinked into the cache. It does nothing unless
`info.pkgGpgCheck()`. Steps:

1. `packageSigCheck()` → `getZYpp()->getTarget()->rpmDb().checkPackageSignature(file, detail)`. Without a target →
   `CHK_ERROR` "OOps. Target is not initialized!". `CHK_NOSIG` becomes `CHK_OK` when `!pkgGpgCheckIsMandatory()`
   ("Relax CHK_NOSIG").
2. `report()->pkgGpgCheck(userData)` is **always** sent (keys `ResObject`, `Package` (legacy), `Localpath`,
   `CheckPackageResult`, `CheckPackageDetail`).
3. `CHK_NOKEY`: read the key id from the rpm header (`RpmHeader::signatureKeyID`) and call
   `KeyRingWorkflow::provideAndImportKeyFromRepository(defaultContext, keyID, info)` (gpgkey URLs, see chapter 06). On success
   the check loops; otherwise it gives up.
4. Not OK: if the receiver set `userData["Action"]` it is used directly. If it only set the key (empty value), the default
   problem report without details is sent. Otherwise `report()->problem(INVALID, msg+details)`. `IGNORE` accepts the
   insecure file (just a WAR). `RETRY`/`ABORT` throw `RpmSigCheckException(action)`, which `providePackage` turns into
   retry / `AbortRequestException`.

Delta-built rpms are checked the same way (`tryDelta` calls `rpmSigFileChecker(builddest)` itself, and exceptions escape
on purpose: "It won't be different" with a full download).

### 10.4 Delta RPMs

```
RpmPackageProvider::doProvidePackage
  ZConfig::download_use_deltarpm()                      zypp.conf download.use_deltarpm
                                                        builtin default = LIBZYPP_CONFIG_USE_DELTARPM_BY_DEFAULT
                                                        (OFF upstream since 17.36.3, fixes #620; the spec may turn it on)
  && ( repoInfo().url().schemeIsDownloading()           "FIXME we only check the first url for now"
       || download_use_deltarpm_always() )              download.use_deltarpm.always (only honored if use_deltarpm)
  deltas = DeltaCandidates::deltaRpms(pkg)              repos' SolvAttr::repositoryDeltaInfo (from deltainfo/prestodelta)
                                                        filtered by pkgname, then exact name+edition+arch of target
  !deltas.empty() && queryInstalled() && applydeltarpm::haveApplydeltarpm()   (/usr/bin/applydeltarpm executable)
  for each delta: tryDelta()
     baseversion edition set and not installed  → skip
     applydeltarpm -C -s <sequenceinfo>         quickcheck → skip on failure
     report.startDeltaDownload(name, downloadSize)
     _access.provideFile(delta repo, delta location, progress→progressDeltaDownload)
        missing file (optional resource, bsc#1245672) → problemDeltaApply("download deltarpm: not found"), next
        exception                                     → problemDeltaDownload(history), next
     applydeltarpm -c -s <sequenceinfo>         full check → problemDeltaApply, next
     applydeltarpm -p -p <delta> <cachedest>.drpm   (percent progress parsed from "NN percent finished")
     rpmSigFileChecker(<cachedest>.drpm); hardlinkCopy to <packagesPath>/<path>/<filename>
     return ManagedFile(cachedest, unlink)
  none worked → Base::doProvidePackage() (full rpm)
```

* There is **no size heuristic** in libzypp (no "delta must be smaller than X% of the rpm"). Whether a delta pays off is
  left to the repo (which deltas it offers) and the `quickcheck`. The changelog entry for #620 says measurements showed
  deltas rarely help, which is why the default was turned off.
* `haveApplydeltarpm()` re-checks the binary on every call and logs changes only (static `TriBool`).
* `DeltaCandidates` scans **all** known repos for deltas, not just the package's repo. The delta file is fetched from the
  delta's own repo (`delta_r.repository().info()`).
* The delta-built file is always returned with `dispose=unlink`, regardless of `keeppackages` (see Pitfalls).
* Test: `zypp/tests/zypp/Deltarpm_test.cc`.

### 10.5 `ProvideFilePolicy` (`zypp/zypp/ProvideFilePolicy.{h,cc}`)

Two hooks only: `progressCB(function<bool(int)>)` (return false = abort) and `fileChecker(FileChecker)`. No retry or
timeout settings. The "retry" behaviour of package downloads lives entirely in `PackageProvider`'s report loop
and in the media layer (mirror fallback, `MediaChangeReport`).

`SrcPackageProvider` calls `RepoMediaAccess::provideFile(repo, loc)` directly instead of going through the
`PackageProvider` workflow. Source packages installed through a normal commit go through
`PackageProviderImpl<SrcPackage>`.

### 10.6 `RepoMediaAccess::provideFile` (`zypp/zypp/repo/RepoProvideFile.cc`)

```
locWithPath = loc.prependPath(repo.path())
DownloadFileReportHack: temporarily replaces the media::DownloadProgressReport receiver, forwards to the old one
                        and to policy.progress()  (restored in dtor)
repoOrigins = repo.repoOrigins()                       empty → RepoException("No url in repository.")
Fetcher fetcher; cache paths: packagesPath, packagesPath/.preload (CleanFiles), systemPackagesPath (if different)
assert_dir(packagesPath)
ScopedDisableMediaChangeReport(repoOrigins.hasFallbackUrls())
for origin in repoOrigins:
    access = mediaAccessForUrl(origin, repo)           cached per origin.authority().url()
    fetcher.enqueue(locWithPath, policy.fileChecker()); fetcher.start(packagesPath, *access)
    return ManagedFile(packagesPath / Fetcher::mapToCachePath(locWithPath), unlink unless repo.keepPackages())
    UserRequestException / FileCheckException → rethrow at once (no next origin)
    other Exception → remember, "Trying next url"
throw RepoException("Can't provide file '%s' from repository '%s'") with all remembered errors
```

* **How media sets are cached:** `RepoMediaAccess::Impl::_medias` maps the *first authority URL* of a `MirroredOrigin` to a
  `shared_ptr<MediaSetAccess>`. Because one URL (e.g. `dvd:///`) may serve several repos, every call re-applies the
  repo-specific data: `setLabel(repo.name())` and, if `metadataPath()/media.1/media` exists, one `SUSEMediaVerifier` per
  medium (`totalMedia()`). `_verifier` remembers which repo alias last set the verifiers, to skip redundant work. The dtor
  calls `release()` on every `MediaSetAccess`. The source marks this mix of media and repo data as fragile (`\todo`).
* **Origin grouping** (`MirroredOriginSet::addAuthorityEndpoint/addEndpoint`, `zypp-logic/zypp-core/MirroredOrigin.cc`): all
  *downloading* baseurls are collected into **one** `MirroredOrigin` (several authorities, bsc#1253193), and all mirrorlist
  URLs are added as mirrors to that same origin. Each non-downloading URL (dir, cd, nfs, ...) gets its own origin. So the
  `for origin` loop above (and `firstOf(repoOrigins())` in refresh) only iterates more than once for mixed-scheme repos.
  Switching between http baseurls and mirrors happens inside the media handler (chapter 07).
* The free function `repo::provideFile(repo, loc, policy)` creates a throw-away `RepoMediaAccess`, so it does not reuse
  attached media.
* `RepoMediaAccess::mapToCachePath(repo, res)` = `Fetcher::mapToCachePath(repo.path()/res.filename())`. The commit preloader
  uses it to place files in `.preload` with the same layout (`commitpackagepreloader.cc`).

### 10.7 Package cache layout and reuse

```
<packagesPath>                         /var/cache/zypp/packages/<escaped alias>  (non-root: $XDG_CACHE_HOME/zypp/packages/<alias>)
   /<repo path()>/<location>           e.g. x86_64/foo-1.0-1.x86_64.rpm ; leading ./../ encoded as ./%2E%2E/ (bsc#1253740)
   /.preload/…                         commit preloader downloads; consumed + unlinked by Fetcher (CleanFiles)
```

* A file is reused only when its **checksum matches** the metadata (`Fetcher::Impl::locateInCache`: final destination first,
  then the extra cache paths). No checksum = never reused by the Fetcher. `Package::cachedLocation()` (used for the first cache
  check in `providePackage`) is a libsolv lookup of the expected path and is not checksum-verified there. The rpm is still
  checksum- and signature-checked when it is installed. (The exact `cachedLocation` semantics are in `zypp/zypp/Package.cc`, not re-read for this chapter.)
* Keeping: `keepPackages()` (repo `keeppackages=1`) or `effectiveKeepPackages()` (also `<packages>/.keep_packages`, bsc#1232458).
  Only the **cache hit** paths in `PackageProvider` use `effectiveKeepPackages()`. Freshly downloaded files get their dispose
  from `RepoMediaAccess::provideFile`, which still checks `keepPackages()` only (commit f34c9b424 did not touch it).
* Cached packages are written with `0644 & ~umask` (bsc#1243887, 17.37.3).

---

## 11. `RepoMirrorList` in depth (`zypp/zypp/repo/RepoMirrorList.{h,cc}`)

### 11.1 Construction: `RepoMirrorList(url, metadatapath)`

```
url scheme "file"            → parse url.getPathName() directly, never cached   (dir:/ is NOT treated as local here)
metadatapath not a dir       → download to temp (RepoMirrorListTempProvider), parse, no cache
else (cache dir exists):
   needRefresh = !isFile(mirrorlist)
              || ( mtime < now - repo.refresh.delay*60  &&  userMayRWX(metadatapath) )   ← stale only matters if we can write
              || makeCookie(url) != readCookieFile(mirrorlist.cookie)
   !needRefresh → parse cache; parse error → fall through and re-download
   unlink mirrorlist + mirrorlist.cookie
   download (TempProvider), parse
   userMayRWX → hardlinkCopy into <metadatapath>/mirrorlist (or create an EMPTY file on failure: "remember no URLs" so
                RepoInfo does not hammer the server), write cookie
any zypp::Exception → ParseException("Failed to parse/receive mirror information for URL: …") with history
```

* Cookie = `sha256(url.asCompleteString())` (17.37.1, "Use a cookie file to validate mirrorlist cache"). Changing the
  mirrorlist URL (including credentials or query) invalidates the cache.
* `metadatapath` is `RepoInfo::metadataPath()`, i.e. the **raw cache** dir. During refresh a new raw dir is built in a sibling
  temp dir; `RefreshContext::saveToRawCache()` carries `mirrorlist`/`mirrorlist.cookie` over (17.37.0, "Preserve a mirrorlist
  file in the raw cache during refresh"). `cleanMetadata` deletes them with the raw cache.
* Two in-memory layers sit on top: `RepoInfo::Impl::mirrorUrls()` caches for 1 h (steady clock), and errors reset that timer
  except for auto mirrorlists (§3.3).

### 11.2 Download (`RepoMirrorListTempProvider`)

| URL | How it is fetched |
|---|---|
| downloading scheme, host in `urlSupportsMirrorLink` (`download.opensuse.org`, `cdn.opensuse.org`) **and** a `mirrorlist` query key | Direct `zyppng::NetworkRequest` through `internal::MediaNetworkRequestExecutor` into a `TmpFile`, with the curl auth workflow (`MediaNetworkCommonHandler::authenticate` + `CredentialManager` of `repoManagerRoot`). The file is chmodded to `0644 & ~umask`. |
| anything else | `MediaSetAccess(MirroredOrigin{url with path "/"})`, `provideFile(url path)`. This goes through the normal media handlers (URL resolver plugins work). |

In the first branch `prepareSettingsAndUrl(url, tSettings)` prepares the URL and transfer settings before the request is created.

### 11.3 Formats and post-processing

| First non-blank char | Format | Parser | Notes |
|---|---|---|---|
| (empty file / EOF) | Empty | — | No mirrors. Not an error. |
| `<` | Metalink XML | `media::MetaLinkParser::getUrls()` | Any parse error → `ParseException("Invalid repo metalink format.")`. |
| `[` | JSON (opensuse.org `?mirrorlist`) | `json::parseDocumentExpected` | Top level must be an array (`null` = no mirrors). Each object's `"url"` string is used. Non-object elements, non-string `url` and invalid URLs are skipped with MIL. |
| other | Plain text | line by line | `#` lines skipped. **Any invalid URL fails the whole list.** Non-downloading URLs are logged "Ignoring" but **still added** (missing `continue`). |

Then, for all formats: `rsync` URLs are dropped and a trailing `repodata/repomd.xml` is cut off the path (so metalinks
pointing at repomd.xml become repo base URLs). In `RepoInfo` the result is stored as *raw* URLs in a
`RepoVariablesReplacedUrlList`, so repo variables inside mirrorlist/metalink files are expanded (17.37.16, fixes #667), and
`internal::propagateQueryParams(mirror, mirrorListUrl)` copies query params like `proxy` from the **mirrorlist URL** onto each
mirror.

The `mirrorlist=`/`metalink=` distinction from `.repo` files is irrelevant for parsing: both end up in `mirrorListUrl()`
and the content decides the format. Metalink *data file* downloads (the media backend using metalinks per file) are a
different mechanism (chapter 07). The NG Provider dropped per-file metalink support in 17.38.0 (fixes #682).

### 11.4 GeoIP (config, cache, consumer)

| Item | Value / source |
|---|---|
| Switch | `zypp.conf` `download.use_geoip_mirror` → `ZConfig::geoipEnabled()`, builtin default **true** (`ZConfig.cc`) |
| Hosts | `ZConfig::geoipHostnames()` = `{"download.opensuse.org"}`, hardcoded (no zypp.conf key) |
| Cache dir | `ZConfig::geoipCachePath()` = `builtinRepoCachePath()/"geoip.d"` → `/var/cache/zypp/geoip.d`. **Not** the configured `cachedir` and **not** root-prefixed. |
| Writer | `RefreshGeoIpLogic` (`repomanagerwf.cc`): entries older than 24 h are deleted first. Hosts with an existing entry are skipped. Otherwise `https://<host>/geoip` is fetched through `Provide`, `<host>` is read with `xml::Reader::seekToNode(1,"host")` and checked as a valid https host. One line is written to `geoip.d/<hostname>`. An empty file means "asked, no answer" (negative cache for 24 h). Non-root without RWX on the dir → skipped. |
| Reader | `MediaNetworkCommonHandler::findGeoIPRedirect()` at handler construction, one redirect target per origin endpoint (`_redirTargets`). Disabled by a `COUNTRY` or `AVOID_COUNTRY` query param. For an empty cache file `Url::setHost("")` throws for http(s) (`require_host`), which is caught, so the result is "no rewrite". |
| Cleanup | `RepoManager::cleanMetadata()` → `recursive_rmdir(geoipCachePath())` (so `zypper clean -m` / removing any repo wipes it) |

Changelog: bsc#1228206 (17.35.4) fixed a typo in the geoip update pipeline. Which files are never redirected
(`repomd.xml(.asc|.key)`, `/geoip`) is documented in chapter 07.

---

## 12. Metadata and config parsers: who parses what

### 12.1 Division of labour

| Data | Parsed by | Used for |
|---|---|---|
| `primary`, `susedata(.LANG)`, `updateinfo`, `patterns`, `deltainfo`/`prestodelta`, `appdata`, `products`, (`filelists`, `other` only if fetched); susetags `packages*`, `*.pat`, … | **libsolv `repo2solv`** (§5.6) | Everything that ends up in the pool, including delta info (`SolvAttr::repositoryDeltaInfo`) |
| `repomd.xml` | `parser::yum::RepomdFileReader` | choosing files to download, `<content>` keywords, `keyhints()` |
| susetags `content` | `parser::susetags::ContentFileReader` → `RepoIndex` | file checksums (`META`, `HASH`, `KEY`), `DESCRDIR`, `DATADIR`. Also `Fetcher` content indexes. |
| `.repo` | `parser::RepoFileReader` (§3.2) | repo config, also plugin service output |
| `.service` | `parser::ServiceFileReader` (§3.4) | service config |
| `repoindex.xml` | `parser::RepoindexFileReader` | RIS repo lists (§7) |
| `/etc/products.d/*.prod` | `parser::ProductFileReader` | `Target::baseProduct`-related data (`TargetImpl.cc`), zypper `misc.cc` |
| `/var/log/zypp/history` | `parser::HistoryLogReader` | consumers only (zypper `misc.cc`); libzypp only writes the history |
| mirrorlist JSON, generic JSON | `zypp-logic/zypp-core/parser/json` | `RepoMirrorList` (parsing); `TargetImpl` uses the `json::Value/Object/Array` types to **write** JSON |
| CHECKSUMS/SHA1SUMS | `Fetcher::Impl::readChecksumsIndex` | signed-directory downloads (§13) |

### 12.2 `RepomdFileReader` (`zypp/zypp/parser/yum/RepomdFileReader.cc`)

Streams `repomd.xml` with `xml::Reader`. For every `<data type=…>` it collects `location@href` (medium 1),
`checksum`, `header-checksum`, `size`, `header-size`; `timestamp` and `xml:base` are ignored. On `</data>` it calls
`ProcessResource(OnMediaLocation, typestr)`. `<content>` texts (in `<tags>`) are collected as keywords.
* **Hostile location** (`Pathname::relativeDotDot()`, i.e. leaving the repo root): `JobReport::warning` + the whole data entry is
  discarded (bsc#1259802, CVE-2026-25707, 17.38.10).
* `keyhints()`: keywords starting with `gpg-pubkey` and matching `^(gpg-pubkey([^/?]*))(\?fpr=([[:xdigit:]]{8,}))?$`
  give `(keyfile, keyid-or-fpr)`. Without `?fpr=`, the id comes from `gpg-pubkey-<hex8+>`. A `/` in the file name is rejected
  (bsc#1267426, CVE-2026-44941, 17.38.12). Too-short ids are rejected later in `getExtraKeysInRepomd` (§5.4).
* The selection lives in `zypp/zypp/repo/yum/RepomdFileCollector.cc`: skip `*_db` (sqlite), skip `*_zck` unless libsolv
  supports zchunk (when it does, zck is preferred), skip `other` unless `$ZYPP_REPOMD_WITH_OTHER`, skip `filelists` unless
  `$ZYPP_REPOMD_WITH_FILELISTS`, keep `susedata.<LANG>` only for wanted locales. A delta file for zsync/zck reuse is looked up in
  `deltaDir()/repodata` by matching the file name with the checksum prefix removed.

### 12.3 `ContentFileReader` (`zypp/zypp/parser/susetags/ContentFileReader.cc`)

Line based: the first word is the key, the rest the value. `#` lines are comments. Defaults: `DESCRDIR=suse/setup/descr`,
`DATADIR=suse`. `META`/`HASH`/`KEY` need exactly `algorithm checksum filename`, otherwise `ParseException`. Paths are
sanitized: `../` escaping the root → warning + entry dropped. A leading `/` or `./` is removed. **Keys with a `.modifier` are
ignored entirely**: the `if (pos != npos) {strip modifier}` is followed directly by `else if (key == "DESCRDIR")`, so the
keyword chain is the `else` branch of the modifier test. Today this only affects localized product keys (`LABEL.de` etc.),
which are ignored anyway. Product data from `content` (`PRODUCT`, `VERSION`, `REPOKEYWORDS`…) is ignored here
(`DBG "ignored"`). `RepoInfo::hasContent()` reads `REPOKEYWORDS` with its own hack (§3.3).

### 12.4 `RepoindexFileReader` (`zypp/zypp/parser/RepoindexFileReader.cc`)

* `<repoindex>` attributes become **global variables** for `%{name}` substitution, and `ttl` also sets the service TTL. Per
  `<repo>`: `alias` and `url` become section variables (`%{alias}`, `%{url}`, 17.37.2). Unknown `%{var}` are left in place with WAR.
* Defaults for service repos: `enabled=false`, `autorefresh=true` (the service's enable logic in §7.2 decides).
* `url` or `path` is required. With only `path`, the RepoInfo path becomes `/repo/<path>` (the "SLES HACK") and the service URL is
  used as base in `serviceswf.cc`. With both, `<url>/repo/<path>` becomes the baseurl.
* Optional attrs (17.36.7): `name`, `distro_target`, `priority`, `enabled`, `autorefresh`, `gpgcheck` (tri-bool), `repo_gpgcheck`/
  `pkg_gpgcheck`/`keeppackages` (`strToTrue`), `gpgkey` (**single URL**, `setGpgKeyUrl`; unlike `.repo` no lists),
  `mirrorlist`, `metalink`. `type` is ignored (bsc#1177427).
* The final check "Neither url nor mirrorlist nor metalink" can never fire for repos without `url`/`path`, because the earlier check
  already threw. The code comment admits this.
* Parse errors become `ServicePluginInformalException` in `serviceswf.cc` (bsc#1227625 fixed that error report, 17.35.3).

### 12.5 `ProductFileReader` and `HistoryLogReader`

* `ProductFileReader` (`ParseDef` based): mandatory `product/{name,version,release,arch}`; optional `vendor`, `shortsummary`,
  `summary[lang]`, `productline`, `register/{target,release,flavor}`, `updaterepokey`, `upgrades/upgrade/{name,summary,repository,
  product,notify,status}`. `scanDir(consumer, dir)` / `scanFile(file)`; errors are logged and give an empty `ProductFileData`.
  `TargetImpl` uses `scanFile` on the `baseproduct` link.
* `HistoryLogReader`: splits each line with `str::splitEscaped(line, "|")` into `HistoryLogData` (factory by action id).
  `IGNORE_INVALID_ITEMS` turns a `ParseException` into a WAR. `readAll`, `readFrom(date)`, `readFromTo`, `addActionFilter`.
  If the consumer returns false, parsing stops.

### 12.6 JSON parser (`zypp-logic/zypp-core/parser/json*.{h,cc}`)

`zypp::json::parseDocument(InputStream)` (public, throws `parser::ParseException`) and `parseDocumentExpected()` (only with
`ZYPP_DLL`, returns `zyppng::expected<json::Value>`). The tokenizer is in `json/private/jsonparser_p.*`, the value types in `json/Json*.h`.
`zypp/zypp/base/Json.h` is a forwarding include. Users: `RepoMirrorList` (only parser user in the library),
`TargetImpl` (building JSON for commit plugins via `json::Object/Array`). Test: `zypp/tests/parser/JsonParser_test.cc`.

---

## 13. `Fetcher` in depth (`zypp/zypp/Fetcher.{h,cc}`, log group `zypp::fetcher`)

### 13.1 Model

```
Fetcher
  _resources : list<FetcherJob>   OnMediaLocation + checkers + flags {Directory, Recursive, AlwaysVerifyChecksum}
  _indexes   : set<FetcherIndex>  CHECKSUMS / SHA1SUMS / content files to read first
  _checksums : map<path,CheckSum> filled from indexes
  _caches    : set<CacheInfo>     extra cache dirs (+ CacheOptions Default | CleanFiles), ordered by path name
  _dircontent: dirInfo cache
  _options   : AutoAddContentFileIndexes(1) | AutoAddChecksumsIndexes(2) = AutoAddIndexes
```

API: `enqueue(loc, checker)`, `enqueueDigested(loc, checker[, deltafile])` (forces a checksum check, warns if none),
`enqueueDir(loc, recursive, checker)`, `enqueueDigestedDir`, `addIndex(loc)`, `addCachePath(dir[, CleanFiles])`,
`setMediaSetAccess`, `start(destdir, media, progress)`, `reset()`, `static mapToCachePath(path)`.
`enqueue*` calls `MediaSetAccess::precacheFiles()` if a media access was set beforehand (that lets parallel backends prefetch).

### 13.2 `start()`

1. `downloadAndReadIndexList`: for each unread index → `downloadIndex` (a nested `Fetcher`: `<index>.asc` optional → if
   present it builds a `SignatureFileChecker`; `<index>.key` optional → **imported into the keyring untrusted**
   (`importKey(key, false)`); then the index with the signature checker), then `readIndex` (CHECKSUMS/SHA1SUMS lines
   `<checksum> <file>`, or a susetags `content` via `ContentFileReader` using the `HASH` entries).
2. For each job: directory jobs expand through `dirInfo` (+ auto index discovery, `CHECKSUMS*`/`SHA1SUMS*` skipped, a missing dir is
   "hidden subtree" and skipped). For file jobs with `AutoAdd*` options, the file's dir **and** `/` are listed for indexes ("expensive").
3. Checksum checker: the job's own checksum, else one from an index, else (only `AlwaysVerifyChecksum`) an empty-checksum
   checker, which then fails.
4. `provideToDest`: `locateInCache` (needs a checksum: final dest, then `_caches`; `CleanFiles` hits get `dispose=unlink`) or
   `media.provideFile(loc, optional ? PROVIDE_NON_INTERACTIVE : PROVIDE_DEFAULT)` + `ReleaseFileGuard`. Then `validate()` (all
   checkers, FileCheckException passes through), then `hardlinkCopy` to `destdir/mapToCachePath(loc)`. A pre-existing file at the
   destination that fails validation is unlinked. Errors on **optional** resources are swallowed (WAR) and the file is just absent.
5. `progress.incr()` false → `AbortRequestException`.

`mapToCachePath()` only rewrites a leading `./(../)+` into `./(%2E%2E/)+` so that remote resources above the repo path stay
below the local cache dir (bsc#1253740). Everything else is returned unchanged.

### 13.3 Who uses it today

* `RepoMediaAccess::provideFile` (all package/delta downloads, §10.6): no options, no indexes. It relies on metadata checksums.
* `zypp/tools/ProvideSignedDirectory.cc`: `AutoAddIndexes` demo.
* Repo metadata refresh does **not** use the Fetcher any more. The old `repo::Downloader`, `yum::Downloader` and `susetags::Downloader`
  classes (which were Fetcher-based) were removed in the NG refactoring (commit fe353fe96, "Make use of sync pipelines in
  zypp::RepoManager"). Their replacement is `zyppng::repo::DownloadContext` (`zypp/zypp/ng/repo/downloader.{h,cc}`: RepoInfo, dest
  dir, `deltaDir`, collected `files()`, optional `PluginRepoverification`) plus the `RepoDownloaderWorkflow` per format (§5.4).
  The old test names stayed: `zypp/tests/repo/susetags/Downloader_test.cc` and `yum/YUMDownloader_test.cc` now test the workflows.
* `MediaSetAccess` itself (multi-medium handling, `rewriteUrl`, `ReleaseFileGuard`, media change) is described in chapter 07 §7.10.
  On the repo side it is created by `RepoMediaAccess` (packages), `RepoMirrorList` (non-opensuse mirrorlists) and
  `MediaSetAccess::provideFileFromUrl` helpers. `zyppng::Provide` (refresh) reuses only `MediaSetAccess::rewriteUrl`.

---

## 14. History of notable behavioural changes (2023–2026, from `zypp/package/libzypp.changes`)

| Version (date) | Change |
|---|---|
| 17.31.26 (Dec 2023) | Repo variables expanded in URLs of downloaded `.repo` files, `zypper ar 'https://…/$releasever/x.repo'` (bsc#1212160) |
| 17.32.3 / 17.33.0 (Apr–May 2024) | `RepoStatus::fromCookieFile` uses the file mtime; outdated-metadata check fixed for non-root (bsc#1222086) |
| 17.32.4 (Apr 2024) | Sibling cache dirs no longer created too restrictively (0700) (bsc#1222398) |
| 17.32.5 / 17.34.2 (2024) | Volatile media are not refreshed while raw metadata exist; media attached lazily during refresh (bsc#1223094) |
| 17.32.6 (Apr 2024) | Download from `gpgkey=` URL fixed (bsc#1223430) |
| 17.34.1–17.35.0 (Jun 2024) | **RepoManager refactored into NG workflows**, service workflows added, soname bump |
| 17.34.2 (Jun 2024) | `ssl_clientkey` stripped from repo URLs (bsc#1226030) |
| 17.35.3 (Jul 2024) | Error report for broken repoindex.xml fixed (bsc#1227625) |
| 17.35.4 (Jul 2024) | GeoIP update pipeline typo fixed (bsc#1228206) |
| 17.35.13 (Nov 2024) | `RepoNoPermissionException` when caches are not writable; buildCache no longer tries to fetch raw metadata then (bsc#1225451) |
| 17.35.19 (Jan 2025) | `.keep_packages` marker (bsc#1232458) |
| 17.36.0 (Feb 2025) | Master index fetched before key and signature (bsc#1236820); repoverification plugins actually run again (#614); unused RepoReports deprecated |
| 17.36.3 (Feb 2025) | `download.use_deltarpm` **off by default** (#620); repo vars evaluated in the right context (bsc#1237044) |
| 17.36.5 (Mar 2025) | RepoStatus recomputed correctly when repo URLs change |
| 17.36.7 (Apr 2025) | repoindex.xml may set `gpgcheck`, `repo_gpgcheck`, `pkg_gpgcheck`, `keeppackages`, `gpgkey`, `mirrorlist`, `metalink` |
| 17.37.0 (May 2025) | **Mirrors inside the media backends** (bsc#1240132); `gpgkey=` is the primary key source; mirrorlist preserved across refresh |
| 17.37.1 (May 2025) | Mirrorlist cache validated by cookie; services may update gpgkey/mirrorlist/metalink |
| 17.37.2 (May 2025) | `%{url}` in RIS repo sections; "maintained by service" note in `.repo` files (#638) |
| 17.37.3 (Jun 2025) | No warning for failing *auto* mirrorlist (bsc#1243901); cached packages `0644 & ~umask` (bsc#1243887) |
| 17.37.4 (Jun 2025) | Trailing slash on mirrorlist request path (#643) |
| 17.37.6 (Jun 2025) | Mirrors can be disabled for security relevant files (master index, keys); requires zypper ≥ 1.14.91 |
| 17.37.9 (Jul 2025) | DeltaRpm download errors ignored, they are optional (bsc#1245672) |
| 17.37.12 (Jul 2025) | Explicit probe request when adding a repo (bsc#1246466) |
| 17.37.15 (Jul 2025) | Preloader appends `RepoInfo::path()` to mirror URLs (bsc#1247054) |
| 17.37.16 (Aug 2025) | Repo variables replaced inside mirrorlist/metalink content (#667) |
| 17.38.0 (Jan 2026) | NG Provider: MediaCurl2-style download, per-file metalink dropped (#682) |
| 17.38.5 (Mar 2026) | Invalid mirrors deprioritized (zypper#636); preloader caches packages from arch subrepos, `../` mapping (bsc#1253740) |
| 17.38.6 (Apr 2026) | Several authorities per `MirroredOrigin` (bsc#1253193) |
| 17.38.10 (May 2026) | repomd/content entries pointing outside the repo discarded (bsc#1259802, CVE-2026-25707) |
| 17.38.11 (May 2026) | Crash on malformed metadata fixed (#740) |
| 17.38.12 (Jun 2026) | keyhint must be a file name (bsc#1267426, CVE-2026-44941) |
| 17.38.13 (Jun 2026) | `path=` must not leave the repo (bsc#1267874, CVE-2026-44942) |
| 17.38.15 (Sep 2026) | keyhints in repoindex.xml need at least a long id for auto-import (bsc#1271730); `repo_gpgcheck` strictly followed, no legacy exception for once-accepted unsigned repos (bsc#1274625) |

Version numbers are taken from the `- version` line that closes each changelog block.

---

## Pitfalls & special cases

* The legacy `RepoManager` is a facade. Fix bugs in `zypp/zypp/ng/repomanager.cc` and `zypp/zypp/ng/repo/workflows/*`, not in `RepoManager.cc`.
* That code runs **synchronously** (`ZYPP_IS_ASYNC=false`). `#ifdef ZYPP_ENABLE_ASYNC` branches are dead in libzypp.so, so test the `#else` branch.
* Legacy progress receivers are ignored for `refreshMetadata`, `clean*`, `loadFromCache`, `addRepositories` and `modifyRepository`.
* Legacy `refreshMetadata` does not build the solv cache. `buildCache` will call refresh itself if the raw cache is empty.
* Constructing a RepoManager can delete orphaned service repos and unknown cache dirs (default paths only, bnc#891515; not in read-only mode, bsc#1210740; `.no_auto_prune`, bsc#1204956).
* `type=` in `.repo` files is ignored on read (bsc#1177427) but written on save. Probed types found during refresh are never saved.
* `gpgcheck` is tri-state, but `repo_gpgcheck`/`pkg_gpgcheck` are parsed with `strToTrue`. Any value present is definite.
* `repo.refresh.delay` only applies when the raw metadata status equals the solv cookie (bsc#1174016). Local URLs ignore it. CD/DVD never refreshes.
* Changing the URL (or the vars that expand into it) changes `RepoStatus(RepoInfo)` and forces refresh and rebuild. Changing the RepoStatus magic invalidates every solv cache.
* The master index never comes from mirrors (`setMirrorsAllowed(false)`). Only data files and, in the mirror-only case, the `.key` may.
* `repoOrigins()`, `repoOriginsEmpty()` and `url()` may download the mirrorlist (in-memory cache 1 h). An auto mirrorlist for download/cdn.opensuse.org fails silently (bsc#1243901).
* `RepoNoPermissionException` stops the URL loop at once. Other errors try the next origin, and the first error becomes the main exception.
* A solv toolversion mismatch or failed load in `loadFromCache` silently rebuilds the cache, which may run repo2solv.
* `RepoVarsMap` is a singleton keyed on `ZConfig::repoManagerRoot()` (bsc#1237044). `$releasever` follows the target on every lookup unless vars.d or `ZYPP_REPO_RELEASEVER` set it.
* Plugin services run chrooted to rootDir with the rootDir removed from the path (bsc#1080693), and they are immutable. `ServicePluginInformalException` is a "warning" sent as an exception after the refresh finished.
* `repostodisable` is cleared on every service refresh, while `repostoenable` entries are one-shot per repo. A user's enable change survives only while the service request stays the same.
* `modifyRepository`/`removeRepository` rewrite `.repo` files in place (not atomic), and renaming does not move caches.
* `removeRepository` → `cleanMetadata` also wipes the whole GeoIP cache.
* `~RepoManager` runs appdata plugins as root when repos changed (`ZYPP_PLUGIN_APPDATA_FORCE_COLLECT` forces it).
* Hostile `path=../` is normalized with a warning. Resource names with `../` are remapped below the cache (bsc#1253740).
* Known small bugs: `BuildCacheLogic` reports progress `Success` on error, and the YAST2 probe logs "RPMMD". The `refresh-ng`/`repomirror` devtools are stale.
* **`.keep_packages` only half applied** (likely bug): `RepoMediaAccess::provideFile` sets the dispose with `repo.keepPackages()`, not
  `effectiveKeepPackages()`. With the marker but without `keeppackages=1`, packages downloaded on demand (DownloadAsNeeded) are still unlinked
  after install. Only cache hits honour the marker.
* Delta-built rpms are always returned with `dispose=unlink` (`tryDelta`), so they are not kept even with `keeppackages=1` (likely unintended).
* `QueryInstalledEditionHelper` queries the rpmdb of `/`, not the target root. With `--root`, delta eligibility is decided by the
  host system (by design for `applydeltarpm`, but surprising). The delta gate checks only `repoInfo().url()` (the first URL) for "downloading".
* The PackageProvider "toplevel cache" compares with `RepoManagerOptions()` (root `/`). For `--root` installs this means the host's
  `/var/cache/zypp/packages/<alias>` is used as a checksum-verified source and hardlinked/copied into the target cache.
* Package download reports: a cache hit sends `infoInCache` only (no `start`/`finish`), and failures leave the report without `finish()`.
  A missing delta file is reported via `problemDeltaApply`, not `problemDeltaDownload`.
* `RepoMediaAccess::provideFile` reuses **one** `Fetcher` across the origin loop without `reset()`. After a failed origin, the job is
  queued twice for the next one. Harmless (the second run hits the cache by checksum), but it wastes work.
* `Fetcher::Impl::enqueueDigested(loc, checker)` **ignores the checker argument** (unnamed parameter). Only the checksum check runs.
* `Fetcher` caches (`std::set<CacheInfo>`) are searched in path-name order, not insertion order. It only works because a hit requires a matching checksum.
* Mirrorlist text format: non-downloading URLs are logged as "Ignoring" but still added. One invalid line fails the whole list.
  `rsync` URLs and a trailing `repodata/repomd.xml` are stripped for all formats.
* Mirrorlist cache staleness (`repo.refresh.delay`) is only enforced when the raw dir is writable. Non-root users keep using an old cached list.
  Only `file:` mirrorlists bypass the cache (`dir:` does not).
* GeoIP cache is `/var/cache/zypp/geoip.d` (builtin, not root-prefixed, not following `cachedir`). It is enabled by default for
  `download.opensuse.org`, and any `cleanMetadata` wipes it.
* `ContentFileReader`: keys with a `.modifier` skip the whole keyword chain (dangling `else`). Adding a new susetags key with a modifier will silently do nothing.
* repoindex.xml `gpgkey` takes a single URL (no list), unlike `.repo` files. `enabled` defaults to **false** and `autorefresh` to true for service repos.

## Where to start when debugging

* **Logs**: the repo code logs under the log group `zypp::repomanager` (all of `repomanager.cc` and the workflows). Useful lines:
  `Check if to refresh repo`, `RefreshCheckStatus returned:`, `Old status: ... New Status:`,
  `has been refreshed less than repo.refresh.delay`, `Probed type`, `building cache...`, the repo2solv output
  (WAR lines), `Going to refresh service`, `Service repo X gets ...`. Log setup and env vars are in [Configuration](09-configuration-and-environment.md).
* **Compare status by hand**: `cat /var/cache/zypp/solv/<alias>/cookie`, `ls -l /var/cache/zypp/raw/<alias>/.repo_gpgcheck`,
  `sha256sum raw/<alias>/repodata/repomd.xml`. Remember the magic suffix in the cookie checksum.
* **Force paths**: `zypper ref -f` (RefreshForced), `zypper ref --force-build` (BuildForced; check the zypper man page for the short flags), `zypper clean -m/-M`,
  `zypper --releasever X lr -u` to check variable expansion, `zypper lr -d` (`RepoInfo::dumpOn` shows `gpgcheck` flags, `*` = mandatory).
* **Env**: `ZYPP_REPO_RELEASEVER`, `ZYPP_PLUGIN_APPDATA_FORCE_COLLECT`, `XDG_CACHE_HOME`/`HOME` (non-root package cache).
* **Isolated reproduction**: use `RepoManagerOptions::makeTestSetup(tmpdir)` or `RepoManagerOptions(root)` with a chroot copy of
  `/etc/zypp/repos.d`. For services, put a shell script into `<root>/<pluginsPath>/services`.
* **Tests**: `zypp/tests/zypp/RepoManager_test.cc`, `RepoInfo_test.cc`, `RepoStatus_test.cc`, `MirroredOrigin_test.cc`,
  `Deltarpm_test.cc`; `zypp/tests/repo/` (`MirrorList_test.cc`, `PluginServices_test.cc`, `RepoVariables_test.cc`,
  `RepoLicense_test.cc`, `RepoSigcheck_test.cc`). See [Testing](11-testing.md).
* **Package download problems**: log lines `provided Package from cache`, `provided Package from toplevel cache`, `provide Package`,
  `Failed to provide Package`, `tryDelta`, `got delta candidate`, `Applydeltarpm :`, `Relax CHK_NOSIG`, `Providing file of repo '…' from:`,
  `Trying next url`, `provideFile at`. Fetcher lines are in log group `zypp::fetcher` (`not found in cache, retrieving...`, `found in cache`,
  `Checking job [...]`, `Optional resource … could not be transferred`). `ZYPP_COMMIT_NO_PACKAGE_CACHE=1` disables the commit read-ahead cache.
* **Delta RPMs**: check `download.use_deltarpm` (and `.always` for non-downloading URLs), `/usr/bin/applydeltarpm` (`Found executable` /
  `No executable` in the log), and that the base version is installed in `/`. Run `applydeltarpm -C -s <seq>` by hand with the sequence from the log.
* **Mirrors**: `cat /var/cache/zypp/raw/<alias>/mirrorlist{,.cookie}`, compare the cookie with `echo -n '<url>' | sha256sum`. Look for
  `Detected Metalink|JSON|TXT`, `Mirror cachefile cookie valid`, `Getting MirrorList from URL`, `Detected opensuse.org baseUrl with no mirrors`.
  `ZYPP_METALINK_DEBUG=1` logs the mirrors parsed from a metalink. Delete the two files to force a refetch (the in-process 1 h cache still applies).
* **GeoIP**: `ls -l /var/cache/zypp/geoip.d/`. Log `Storing geoIP redirection`, `Found GeoIP rewrite`. Disable with
  `download.use_geoip_mirror = false` or a `COUNTRY=` query param.
* **Parsers**: `zypp/tests/parser/` (`JsonParser_test.cc`, `RepoFileReader_test.cc`, `RepoindexFileReader_test.cc`,
  `ProductFileReader_test.cc`, `HistoryLogReader_test.cc`, `yum/`), `zypp/tests/zypp/Fetcher_test.cc` (data in
  `zypp/tests/zypp/data/Fetcher`), schemas in `zypp/zypp/parser/yum/schema/` (`validate-all`) and `parser/schema/repoindex.rnc`.
* **Bisecting a behaviour change**: start with the table in §14, then search `zypp/package/libzypp.changes` for the bsc numbers listed in this chapter. Most
  refresh logic was moved from the old `RepoManager.cc` into `zypp/zypp/ng/` in the NG refactoring. Use `git log --follow`
  or `git log -S'<snippet>'` to find history from before the move.
