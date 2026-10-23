# Configuration and environment

This chapter covers how libzypp is configured at runtime. It explains the legacy `ZConfig` singleton and every
`zypp.conf` key it reads, and how the config files are found and merged (UAPI.6 / econf, `ZYPP_CONF`,
`ZYPPCONFDIR`). It also covers how settings follow `--root`, architecture and locale detection, a full table
of environment variables, and the new typed NG config engine (`zyppng::config::Config`) from the `ng-config`
branch. §11–§15 add a key-by-key audit of parser vs. docs, the env variables libzypp itself sets, every
other config/state file and how it handles roots, the runtime override API with its zypper/YaST users, and
the complete NG key registry with branch status.

---

## 1. Big picture

```
            build time                                   run time
  ┌─────────────────────────────┐        ┌────────────────────────────────────────────┐
  │ CMake ZYPPCONFDIR (/usr/etc)│──────► │ APIConfig(LIBZYPP_ZYPPCONFDIR)             │
  │ LIBZYPP_CONFIG_USE_* opts   │──────► │ builtin defaults (deltarpm, curl2, ...)    │
  └─────────────────────────────┘        └───────────────┬────────────────────────────┘
                                                         │
     $ZYPP_CONF set? ── yes ──► IniDict( <root>/$ZYPP_CONF )   (single file, or builtin defaults)
           │ no                                          │
           ▼                                             ▼
     EconfDict("/zypp/zypp.conf", root)          ┌──────────────────────┐
       /etc  ▸ /run ▸ <ZYPPCONFDIR>              │ legacy: ZConfig::Impl │──► MediaConfig singleton
       base file + zypp.conf.d/*.conf            │  (zypp/zypp/ZConfig.cc)│     (download.*, credentials.*)
                                                 └──────────────────────┘
                                                 ┌──────────────────────┐
                                                 │ NG: config::Config    │  owned by zyppng::Context
                                                 │  (zypp-logic/*/ng/config)│
                                                 └──────────────────────┘
     Environment variables: read all over the code base, see §8 (often cached in function statics).
```

Key files:

| File | Role |
|---|---|
| `zypp/zypp/ZConfig.h`, `zypp/zypp/ZConfig.cc` | Legacy singleton: parses zypp.conf, exposes the getters/setters. Log group `zconfig`. |
| `zypp-logic/zypp-media/mediaconfig.{h,cc}` | `MediaConfig` singleton: `download.*` transport keys and `credentials.*`. ZConfig forwards these keys. |
| `zypp-logic/zypp-core/parser/econfdict.{h,cc}` | `EconfDict`: UAPI.6 lookup and merge (a subclass of `IniDict`). |
| `zypp-logic/zypp-core/parser/inidict.cc`, `iniparser.cc` | The INI parser. A later key assignment overwrites an earlier one. |
| `zypp-logic/zypp/Arch.cc` | `Arch::detectSystemArchitecture()`: CPU and arch detection. |
| `zypp/zypp.conf` | The **vendor** config, installed to `${ZYPPCONFDIR}/zypp/zypp.conf`. It only sets `multiversion` and `multiversion.kernels`. |
| `zypp/zypp.conf.LEGACY` | The full commented old-style file. It is installed as `/etc/zypp/zypp.conf` only with `KEEP_LEGACY_ZYPPCONF`. |
| `zypp/zypp.conf.README` | Installed to `/etc/zypp/` instead of a zypp.conf on new distros. It explains the move. |
| `zypp/doc/zypp.conf.5.txt` | Man page ZYPP.CONF(5). This is the authoritative user documentation of the keys. |
| `zypp/doc/autoinclude/EnvironmentVariables.doc` | Doxygen page `zypp-envars`. It is **incomplete**; §8 is the full list. |
| `zypp-logic/zypp-core/ng/config/config.{h,cc}` | NG generic typed key/value engine. |
| `zypp-logic/zypp-core/ng/config/configparsers.{h,cc}` | NG parse/serialize helpers for core types. |
| `zypp-logic/zypp-media/ng/config/mediakeys.{h,cc}` | NG media-tier key tokens. |
| `zypp-logic/zypp/ng/config/zyppconfig.{h,cc}` | NG application key tokens and `registerZyppKeys()`. |
| `zyppng/lib/zypp/ng/contextconfig.{cppm,cc}` | Module partition `zyppng:contextconfig`, the `ContextConfig` alias and factories. |
| `zyppng/tests/config/*` | NG config tests (`Config`, `ConfigCompat`, `ConfigParsers`). |

---

## 2. Locating and merging zypp.conf

### 2.1 The UAPI.6 / econf scheme (default)

Since 17.37/17.38 (changelog entry "zypp.conf: follow the UAPI configuration file specification (PED-14658)")
zypp.conf is no longer a single file. `ZConfig.cc` → `ZyppConfIniMap::iniMapReadFrom()` calls
`parser::EconfDict( "/zypp/zypp.conf", root_r )`. `EconfDict` uses `econf::ConfigurationContext::getConfigFiles()`
(`zypp-logic/zypp-core/parser/econfdict.cc`):

1. The stem `zypp/zypp.conf` must be relative-safe: it must not contain `../` and must not be empty.
   Otherwise it throws `EconfException`.
2. Lookup dirs, **in order of preference**. Each one is only used if it exists as a directory below
   `root_r`:
   - `<root>/etc` (system configuration)
   - `<root>/run` (ephemeral overrides; **not** mentioned in the man page, but `ZConfig.h` documents it)
   - `<root>/<ZYPPCONFDIR>` (vendor configuration; `EconfDict::defaultDistconfDir()`, which defaults to
     `APIConfig(LIBZYPP_ZYPPCONFDIR)`)
3. **Base file**: the first `zypp/zypp.conf` found in the preferred order wins. Files found later are
   "masked" (`takeOrMask`). A candidate counts if `isExist() && !isDir()`. So `/etc/zypp/zypp.conf → /dev/null`
   or an empty file masks the vendor file, as the man page says.
4. **Drop-ins**: `zypp/zypp.conf.d/*`. Hidden files (`.`-prefixed) are skipped, and only names ending in
   the stem's suffix (`.conf`) are taken. They are collected into a `std::map<name, PathInfo>`. The same
   filename in a more preferred dir masks the others.
5. Result order: base file first, then the drop-ins in lexicographic order of filename, across all dirs.
   Each file is `IniDict::read()` into the same dict, so **later key assignments override earlier ones** for
   each section/key pair.

```
  /etc/zypp/zypp.conf          ┐ first existing wins (whole file)
  /run/zypp/zypp.conf          │
  /usr/etc/zypp/zypp.conf      ┘
  + zypp.conf.d/10-a.conf      ┐ union by filename, /etc ▸ /run ▸ /usr/etc masking,
  + zypp.conf.d/50-b.conf      │ applied in lexicographic order, later keys win
  + zypp.conf.d/90-c.conf      ┘
```

**Section pitfall (bsc#1272534):** since 17.38.x every file starts in the *unnamed* section
(`iniparser`). A drop-in that has `foo = bar` without a `[main]` header puts the key into section `""`.
ZConfig then logs `zypp.conf: Unknown section: [] ...` and ignores the key. Before that fix the
section "leaked" over from the previous file. The man page warns that assignments outside any section
"may not be recognized".

### 2.2 `$ZYPP_CONF`

`env::ZYPP_CONF()` in `ZConfig.cc`: if the variable is set (even to an empty value), econf is **disabled**.
Then `root_r / $ZYPP_CONF` is read with plain `IniDict` if it is a file. If it is not a file, ZConfig logs
"denotes no file below root. Using builtin defaults." and uses **builtin defaults only**. The path is always
interpreted below the root. That also applies when the target root changes (§4). No drop-ins are read.

NG (`Config::parse()`) treats an *empty* `ZYPP_CONF` as unset and falls back to econf, which differs from
legacy. NG also logs `WAR` if the file is missing.

### 2.3 `ZYPPCONFDIR`, packaging and the legacy file

- Root `CMakeLists.txt`: `ZYPPCONFDIR` defaults to `/usr/etc`. It is baked into `APIConfig.h` as
  `LIBZYPP_ZYPPCONFDIR` (`zypp-logic/zypp-core/APIConfig.h.in`).
- `zypp/libzypp.spec.cmake` sets `zyppconfdir`: it uses `%{_distconfdir}` if defined, else
  `%{_prefix}/share` on Fedora (github issue #693), else `%{_prefix}/etc`. It passes `-DZYPPCONFDIR=%{zyppconfdir}`.
  `zyppng/packaging/libzypp-glib.spec` hardcodes `%{_prefix}/etc`.
- `zypp/CMakeLists.txt` installs `zypp/zypp.conf` → `${ZYPPCONFDIR}/zypp/`. With `KEEP_LEGACY_ZYPPCONF`
  (spec: `%bcond keep_legacy_zyppconf` for `suse_version < 1610`) it also installs `zypp.conf.LEGACY` as
  `/etc/zypp/zypp.conf` (`%config(noreplace)`). Otherwise it installs `zypp.conf.README` into `/etc/zypp/`.
- Migration trick in the spec: `%pre` moves an old `zypp.conf.rpmsave` aside, and `%posttrans` renames a new
  `/etc/zypp/zypp.conf.rpmsave` back to `zypp.conf`. Effect: an *unmodified* old `/etc/zypp/zypp.conf`
  disappears (vendor defaults apply), while a *modified* one stays in place.
- Both `/etc/zypp/zypp.conf.d` and `${zyppconfdir}/zypp/zypp.conf.d` are owned directories.
  The man page says that versions supporting drop-ins `provide: libzypp(econf)`.
- Tests can redirect the vendor dir with `EconfDict::defaultDistconfDir( Pathname )`.

Because the vendor `zypp.conf` sets `multiversion = provides:multiversion(kernel)` and
`multiversion.kernels = latest,latest-1,running`, an admin who keeps their own full `/etc/zypp/zypp.conf`
**must** carry those two keys, or kernel multiversioning stops working. The man page states this explicitly.

### 2.4 The `[env]` section (17.38.10)

After all `[main]` keys are processed, each `key = value` in `[env]` is applied with `setenv(key, value, 0)`.
This only happens if the variable is **not already set** (it logs `skip ... is already set`). Duplicates
inside the merged dict are shadowed with a `WAR`. This runs inside the `ZConfig` constructor, so it only
affects code that reads the variable **later**. Variables read in static initialisers or before the first
`ZConfig::instance()` call are not affected (examples: `ZYPP_LOGFILE`, `ZYPP_FULLLOG` in `LogControlImpl`,
`ZYPP_PLUGIN_*` in `PluginScript.cc`, `ZYPP_READONLY_HACK`). The man page warns about `ZYPP_LOGFILE`.
`[env]` is evaluated only for the initial `/` parse, not again when the target root changes.

### 2.5 Techpreview keys

In `[main]`:
- `ZYPP_SINGLE_RPMTRANS` or `techpreview.ZYPP_SINGLE_RPMTRANS` → `setenv("ZYPP_SINGLE_RPMTRANS", v, 0)`
  (does not overwrite).
- `techpreview.ZYPP_MEDIANETWORK` → `setenv("ZYPP_MEDIANETWORK", v, 1)` (**overwrites**). Nothing in the
  current tree reads `ZYPP_MEDIANETWORK` (only `zypp/tools/DownloadFiles.cc` sets it). It is dead.

Changelog 17.28.4 introduced these ("May set techpreview variables for testing in /etc/zypp/zypp.conf").

---

## 3. `ZConfig` (legacy) architecture

`ZConfig` is a Meyers singleton (`ZConfig::instance()`). Its `Impl` is built on first use. The constructor
logs the version and calls `about(MIL)`, which prints the libzypp/libsolv versions, the text locale and the
arch.

Construction sequence (`ZConfig::Impl::Impl()`):

1. Member defaults (see the table in §5). `cfg_arch` comes from `defaultSystemArchitecture()` and
   `cfg_textLocale` from `defaultTextLocale()`.
2. `ZyppConfIniMap iniMap;` parses the zypp.conf of **`/`**. It always uses `/`, even if the application
   later runs with `--root`.
3. For each section/entry pair:
   1. `_initialTargetDefaults.consume()` handles the root-sensitive `solver.*` keys (§4).
   2. `_mediaConf.setConfigValue()` forwards `download.max_*`, `download.min_download_speed`,
      `download.connect_timeout`, `download.transfer_timeout` and `credentials.global.*` to `MediaConfig`.
   3. The big `if/else` chain on `[main]`. Unknown keys get `pWAR("zypp.conf: Unknown entry in [main]:")`.
   4. `[env]` entries are collected (see §2.4), and any other section produces an "Unknown section" warning.
4. The `[env]` values are applied.
5. `$ZYPP_TESTSUITE_FAKE_ARCH` overrides the arch (logged as "legacy").

### 3.1 Option helpers

- `Option<T>`: a plain mutable value.
- `DefaultOption<T>`: remembers a *default* in addition to the current value. A value read from zypp.conf is
  stored via `restoreToDefault(value)`, so it becomes the default. Application `set...()` calls only change
  the current value, and `reset...()` / `set_default_...()` go back to the **config file** value. Used for
  `cachedir`, `metadatadir`, `solvfilesdir`, `packagesdir`, `gpgcheck`, `repo_gpgcheck`, `pkg_gpgcheck`,
  `download.media_preference`, `download.media_mountdir`, `update.messages.notify` and
  `solver.upgradeRemoveDroppedPackages`.
- The `builtinRepo*Path()` getters (private, friends `RepoManager` and `zyppng::RepoManager`) return the
  `DefaultOption` default. RepoManager uses them to decide whether automatic cleanup of orphaned cache dirs is
  safe ("Cleanup orphaned cache dirs only at zypp.conf default locations"). `geoipCachePath()` is also
  built from `builtinRepoCachePath()`, **not** from a `setRepoCachePath()` override.

### 3.2 Parsing leniency

Legacy parsing never fails:
- Booleans use `str::strToBool(value, current)`. When the current value is `true` it only looks for a false
  word, and vice versa. So garbage means "keep the default".
- Integers use `str::strtonum` (strtol/strtoul, base 0). Garbage gives 0 or the numeric prefix, and `-1`
  given to an unsigned wraps to `UINT_MAX`.
- `repo_gpgcheck` and `pkg_gpgcheck` use `strToTriBool`: an unknown value becomes `indeterminate`.
- `commit.downloadMode` uses `deserializeDownloadMode` (unknown → `DownloadDefault`).
- `solver.focus` uses `fromString` (unknown → unchanged).

Valid booleans per the man page: `1/0, yes/no, on/off, true/false`, case-insensitive. `strToTrue` and
`strToFalse` also accept `always/never` (see `zypp-logic/zypp-core/base/String.h` and `Env.h`).

---

## 4. ZConfig and root changes

libzypp has three different "roots":

| Getter | Meaning |
|---|---|
| `systemRoot()` | `getZYpp()->getTarget()->root()` if a Target is up. Otherwise the root set by `announceSystemRoot()`, or empty. |
| `repoManagerRoot()` | `setRepoManagerRoot()` value if set, else `systemRoot()`. |
| `announceSystemRoot(root)` | bsc#1237044 (17.36.x): lets `--root` commands run **without** launching a Target, so that sub components such as `RepoVariables` and the keyring resolve vars and caches in the right root. Cleared by any Target change. |

**Caution:** `systemRoot()` calls `getZYpp()`, which creates the ZYpp instance if needed and may therefore
**acquire the zypp lock** (see `ZYppFactory::getZYpp()`). So `multiversionSpec()` can create ZYpp too
(it goes through `_autodetectSystemRoot()`).

### 4.1 `notifyTargetChanged()`

`ZYppImpl::changeTargetTo()` (`zypp/zypp/zypp_detail/ZYppImpl.cc`) calls
`ZConfig::instance().notifyTargetChanged()` and then `resolver()->setDefaultSolverFlags(false)`.
`notifyTargetChanged()` does the following:
- It clears `_announced_root_path`.
- If the new root is empty or `/`, it uses `_initialTargetDefaults` (parsed from `/`).
- Otherwise it creates fresh `TargetDefaults()` (builtins) and consumes the **target root's** zypp.conf
  (econf below that root, or `<root>/$ZYPP_CONF`).

**Only `TargetDefaults` follow the root:** `solver.focus`, `onlyRequires`, `allowVendorChange`,
`dupAllow{Downgrade,NameChange,ArchChange,VendorChange}`, `cleandepsOnRemove`, `noUpdateProvide`,
`upgradeTestcasesToKeep` and `upgradeRemoveDroppedPackages`. Host settings for these keys are **ignored** when
running on another root. A runtime `setSolverUpgradeRemoveDroppedPackages()` is lost on a target change.

**Everything else stays the host's value.** That includes cache, repos, services, vars and vendor paths,
gpgcheck, download.* and history. Callers prefix those paths with the root themselves. For example,
`RepoManagerOptions(root)` uses `Pathname::assertprefix(root_r, ZConfig::instance().repoCachePath())`, and
`TargetImpl` does the same for `vendorPath()`. So `zypper --root /mnt` with a host `cachedir=/srv/cache`
uses `/mnt/srv/cache`.

### 4.2 Multiversion (bnc#906096, bsc#1193488)

`MultiversionMap` caches one spec per root:
- `_specMap[""]` is the plain value of `multiversion` from the `/` zypp.conf (filled during construction
  through `getDefaultSpec()`).
- `_specMap["/"]` is `[""]` plus the files in `multiversion.d` at `/`.
- `_specMap[root]` is the `multiversion` value from `root`'s zypp.conf. **If the root has no such key or no
  zypp.conf, the host value `[""]` is used** (bsc#1193488: a bare-metal install must behave like a default
  install, because the builtin default (empty) differs from the vendor file (kernel)). It then adds
  `root/<multiversiondir>`.
- The `multiversion.d` dir is taken from the **host's** `multiversiondir`/`configdir` (code comment:
  "Actually we'd need to scan and use the root_r! zypp.conf values").
- `scanConfAt()` takes the first `multiversion` entry of **any** section.
- The setters (`multiversionSpec(set)`, `add/remove/clearMultiversionSpec`) mutate the cached entry for the
  *current* root and call `PoolImpl::multiversionSpecChanged()` if a pool exists.
- Each line of a `multiversion.d` file is one spec entry (`iostr::simpleParseFile`).

---

## 5. All `zypp.conf` keys read by ZConfig / MediaConfig

All keys belong in `[main]`. "TD" means the key is root-sensitive (`TargetDefaults`). "DO" means it is a
`DefaultOption` (resettable to the file value).

| Key | Builtin default | Getter | Notes |
|---|---|---|---|
| `lock_timeout` | `0` (signed long) | `lockTimeout()` | `$ZYPP_LOCK_TIMEOUT` overrides it (read on every call). Negative waits forever (bsc#1184399, bsc#1213231, bsc#1239809). |
| `arch` | autodetected | `systemArchitecture()` | Logged as WAR "Overriding system architecture". Expert only. |
| `cachedir` (DO) | `/var/cache/zypp` | `repoCachePath()` | `pubkeyCachePath()` = cachedir/`pubkeys`. |
| `metadatadir` (DO) | cachedir/`raw` | `repoMetadataPath()` | An empty value follows cachedir. |
| `solvfilesdir` (DO) | cachedir/`solv` | `repoSolvfilesPath()` | |
| `packagesdir` (DO) | cachedir/`packages` | `repoPackagesPath()` | Non-root users fall back to `$XDG_CACHE_HOME/zypp/packages/...` (RepoInfo.cc, bsc#1247948). |
| `configdir` | `/etc/zypp` | `configPath()` | Base for the next rows. |
| `reposdir` | configdir/`repos.d` | `knownReposPath()` | |
| `servicesdir` | configdir/`services.d` | `knownServicesPath()` | bnc#765528 fixed it being ignored. |
| `varsdir` | configdir/`vars.d` | `varsPath()` | Repo variables (see [Repositories](05-repositories-and-services.md)). |
| `vendordir` | configdir/`vendors.d` | `vendorPath()` | Read by `VendorAttr` (`zypp/zypp/VendorAttr.cc`) and `TargetImpl` (prefixed with root). Format: `[main] vendors = A,B`. |
| `multiversiondir` | configdir/`multiversion.d` | (internal) | See §4.2. |
| — | configdir/`needreboot`, `needreboot.d` | `needrebootFile()/Path()` | Not configurable. |
| `repo.add.probe` | `false` | `repo_add_probe()` | |
| `repo.refresh.delay` | `10` (min) | `repo_refresh_delay()` | |
| `repo.refresh.locales` | empty | `repoRefreshLocales()` | If empty, it returns `Target::requestedLocales("")` **at call time**. |
| `download.use_deltarpm` | `LIBZYPP_CONFIG_USE_DELTARPM_BY_DEFAULT` | `download_use_deltarpm()` | OFF by default since 17.36.x (#620). The spec turns it ON for SLE ≤ 15.6. |
| `download.use_deltarpm.always` | `false` | `download_use_deltarpm_always()` | Masked by `use_deltarpm` (`&&`). |
| `download.media_preference` (DO) | `download` | `download_media_prefer_download()` | Any value other than `volatile` (case-insensitive) means "download". |
| `download.media_mountdir` (DO) | `/var/adm/mount` | `download_mediaMountdir()` | |
| `download.use_geoip_mirror` | `true` | `geoipEnabled()` | Hosts are hardcoded to `{download.opensuse.org}` (`geoipHostnames()`). The cache is `builtinRepoCachePath()/geoip.d`. |
| `download.max_concurrent_connections` | `5` | via MediaConfig | |
| `download.min_download_speed` | `0` | via MediaConfig | |
| `download.max_download_speed` | `0` | via MediaConfig | |
| `download.max_silent_tries` | `1` | via MediaConfig | `0` means retry forever. |
| `download.connect_timeout` | `60` | `MediaConfig::download_connect_timeout()` only | Clamped to ≥ 0. ZConfig has **no** getter for it. |
| `download.transfer_timeout` | `180` | via MediaConfig | Clamped to [0, 3600]. |
| `credentials.global.dir` | `/etc/zypp/credentials.d` | `credentialsGlobalDir()` | The default is **hardcoded** `/etc/zypp`, *not* `configdir` (the man page claims `{configdir}`). |
| `credentials.global.file` | `/etc/zypp/credentials.cat` | `credentialsGlobalFile()` | Same caveat. |
| `commit.downloadMode` | `DownloadDefault` | `commit_downloadMode()` | `DownloadOnly`/`InAdvance`/`InHeaps`/`AsNeeded`. |
| `gpgcheck` (DO) | `true` | `gpgCheck()` | See [Security](06-security-gpg-keyring.md). |
| `repo_gpgcheck` (DO) | indeterminate | `repoGpgCheck()` | bsc#1274625: repo_gpgcheck is followed strictly. |
| `pkg_gpgcheck` (DO) | indeterminate | `pkgGpgCheck()` | |
| `solver.focus` (TD) | `Default` | `solver_focus()` | `Job` / `Installed` / `Update`. |
| `solver.onlyRequires` (TD) | `false` | | |
| `solver.allowVendorChange` (TD) | `false` | | |
| `solver.dupAllowDowngrade` (TD) | `true` | | |
| `solver.dupAllowNameChange` (TD) | `true` | | |
| `solver.dupAllowArchChange` (TD) | `true` | | |
| `solver.dupAllowVendorChange` (TD) | `false` | | The builtin was `true` in Code12. Code15 changed it only via the shipped file, and this was fixed in code later (changelog 17.36.6). |
| `solver.cleandepsOnRemove` (TD) | `false` | | |
| `solver.noUpdateProvide` (TD) | `false` | `solver_noUpdateProvide()` | 17.38.14, "may help in bsc#1261038". |
| `solver.upgradeTestcasesToKeep` (TD) | `2` | | Parsed as **unsigned**. `-1` ("keep all" per the man page) works only because strtoul wraps it. `0` means no testcase. Consumed by `TargetImpl.cc: writeUpgradeTestcase()`. |
| `solver.upgradeRemoveDroppedPackages` (TD, DO) | `true` | | |
| `solver.checkSystemFile` | configdir/`systemCheck` | | |
| `solver.checkSystemFileDir` | configdir/`systemCheck.d` | | |
| `multiversion` | empty (vendor: `provides:multiversion(kernel)`) | `multiversionSpec()` | `splitEscaped` on `", \t"`. |
| `multiversion.kernels` | empty (vendor: `latest,latest-1,running`) | `multiversionKernels()` | Used by purge-kernels. |
| `locksfile.path` | configdir/`locks` | `locksFile()` | Format: `locks(5)`. |
| `locksfile.apply` | `true` | `apply_locks_file()` | |
| `update.datadir`, `update.scriptsdir`, `update.messagessdir` | — | — | **Parsed and deliberately ignored.** The paths are constants: `/var/adm`, `/var/adm/update-scripts`, `/var/adm/update-messages`. Note the legacy typo `messagessdir`. |
| `update.messages.notify` (DO) | empty | `updateMessagesNotify()` | `format \| command` (single/none/digest/bulk, `%p`, `%P`). |
| `rpm.install.excludedocs` | `false` | `rpmInstallFlags()` | Sets `RPMINST_EXCLUDEDOCS`. |
| `history.logfile` | `/var/log/zypp/history` | `historyLogFile()` | |
| `ZYPP_SINGLE_RPMTRANS`, `techpreview.*` | — | — | See §2.5. |

Not file-configurable, but ZConfig still holds them: `pluginsPath()` (`/usr/lib/zypp/plugins`),
`userData()` (`setUserData()` rejects control characters other than TAB; the data ends up in history/rpm,
see `UserData.doc`), `repoLabelIsAlias()`, `distroverpkg()` (`system-release`) and the text locale.

---

## 6. Architecture detection

`ZConfig::defaultSystemArchitecture()` → `Arch::detectSystemArchitecture()` (`zypp-logic/zypp/Arch.cc`,
cached in a function static) → `_autoDetectSystemArchitecture()`:

| uname machine | Refinement |
|---|---|
| `x86_64` | `get_x86_64_level()` via `cpuid` (code taken from rpm's `lib/rpmrc.c`): level 2 → `x86_64_v2` (SSE3, SSSE3, CX16, SSE4.1/4.2, POPCNT, LAHF), level 3 → `x86_64_v3` (+ FMA, MOVBE, OSXSAVE, AVX, F16C, BMI1/2, AVX2, LZCNT), level 4 → `x86_64_v4` (+ AVX512 F/DQ/CD/BW/VL). Logged as WAR "architecture upgraded". |
| `i686` | Read `/proc/cpuinfo` `flags`. Missing `cx8` or `cmov` → `i586` (bug #18885). |
| `sparc`, `sparc64` | `/proc/cpuinfo` `type`: `sun4v` → `sparc64v`/`sparcv9v`, `sun4u` → `sparc64`/`sparcv9`, `sun4m` → `sparcv8` (bug #566291). |
| `armv8l`, `armv7l`, `armv6l` | Read **host** `/etc/rpm/platform`. A prefix `armv8hl-`/`armv7hl-`/`armv6hl-` → the hard-float arch. Not root-aware. |
| `ppc64` | glibc ≥ 2.16: `getauxval(AT_PLATFORM)` = `powerN` with N > 6 → `ppc64p7`. |

The override order at startup is: autodetect → zypp.conf `arch=` → `$ZYPP_TESTSUITE_FAKE_ARCH`. Later,
`setSystemArchitecture()` can change it, but the header warns that zypp does not expect the arch to change at
runtime, so set it before touching any other component. While `ZYPP_TESTSUITE_FAKE_ARCH` is set,
`ZYppImpl::commit()` **throws** ("Commit not allowed and disabled"). `TestcaseSetup.cc` sets it when loading
a solver testcase, together with `ZYPP_MODALIAS_SYSFS`.

During commit, `ZYppImpl::commit()` also appends `glibc.cpu.hwcaps=-SSE4_1,-arch_3_00,-z13` to
`GLIBC_TUNABLES` (bsc#1246912), so that `ld.so` ignores the subarch library packages while scripts run.

## 7. Text locale

`ZConfig::defaultTextLocale()` is a function static computed by `_autodetectTextLocale()`. It takes the
first of `LC_ALL`, `LC_MESSAGES`, `LANG` that is set, not `C`/`POSIX`, and a valid `Locale`. The fallback is
`en`. `LANGUAGE` is **not** honoured (there is a `\todo`). The function also does
`setenv("BOOST_TEST_CATCH_SYSTEM_ERRORS", "no", 1)` with a `#warning HACK`. That is a library-wide side effect
that exists only for the boost test runner.
`setTextLocale()` propagates to `sat::Pool::instance().setTextLocale()`. `Locale::bestMatch()`
(`zypp-logic/zypp/Locale.cc`, `#ifndef ZYPPNG`) uses `ZConfig::textLocale()` as the default request.

---

## 8. Environment variables (complete list)

Sources: grep for `getenv`, `secure_getenv`, `getenvBool`, `L_ENV_CONSTR_DEFINE_FUNC` and `"ZYPP…"` string
literals in `zypp/`, `zypp-logic/` and `zyppng/` (tests excluded). There is no `secure_getenv` anywhere.
"Bool (exists)" means the code only tests `getenv() != NULL`. "strToBool" means the value is parsed.
`env::getenvBool` (`zypp-logic/zypp-core/base/Env.h`) returns a TriBool: true for 1/yes/true/always/on/+,
false for 0/no/false/never/off/-, otherwise indeterminate. Many values are cached in a `static` on first use.
Changing them later in the process has no effect.

### 8.1 Configuration and locking

| Variable | Read in | Effect |
|---|---|---|
| `ZYPP_CONF` | `zypp/zypp/ZConfig.cc` (`env::ZYPP_CONF`), `zypp-logic/zypp-core/ng/config/config.cc` | Parse only `<root>/$ZYPP_CONF` (plain INI), with no econf merge. If missing, builtin defaults are used. |
| `ZYPP_LOCK_TIMEOUT` | `ZConfig::lockTimeout()` | Overrides `lock_timeout`. Negative waits forever. Read on every call. |
| `ZYPP_LOCKFILE_ROOT` | `zypp/zypp/ZYppFactory.cc` | The zypp lock lives in `$ZYPP_LOCKFILE_ROOT/run` (default `/run`). Called "Hack to circumvent the currently poor --root support". |
| `ZYPP_READONLY_HACK` | `ZYppFactory.cc` (static init) | Bool (exists): skip the zypp lock. Same effect as `zypp_readonly_hack::IWantIt()`. |
| `ZYPP_TESTSUITE_FAKE_ARCH` | `ZConfig.cc`, `zypp_detail/ZYppImpl.cc`, `zypp/tools/zypp-NameReqPrv.cc` | Override the arch and **forbid commit**. Set by `misc/TestcaseSetup.cc`. |
| `ZYPP_ARCH` | `zypp/tools/zypp-NameReqPrv.cc` | Tool-only arch fallback. |
| `LC_ALL`, `LC_MESSAGES`, `LANG` | `ZConfig.cc`, `zypp-logic/zypp/ng/config/zyppconfig.cc` | Text locale (§7). `LANG` is also used by `zypp-tui/output/Utf8.h`. |
| `HOME` | `zypp-logic/zypp-media/auth/credentialmanager.cc`, `zypp-curl/curlconfig.cc`, `RepoInfo.cc` | `~/.zypp/credentials.cat` (prefixed with the root!), `~/.curlrc`, and the `XDG_CACHE_HOME` fallback. |
| `XDG_CACHE_HOME` | `zypp/zypp/RepoInfo.cc` | Alternate packages dir for non-root users (`…/zypp/packages/<repo>`). |
| `ZYPPTMPDIR` | `zypp-logic/zypp-core/fs/TmpPath.cc` | Base for temp files (default `/var/tmp`). As root, `zypp.tmp` is appended (bsc#1249435, SELinux). Cached. |

### 8.2 Repositories and metadata

| Variable | Read in | Effect |
|---|---|---|
| `ZYPP_REPO_RELEASEVER` | `zypp/zypp/repo/RepoVariables.cc` | Fakes `$releasever` (e.g. for `zypper dup` to the next distro). |
| `ZYPP_REPOMD_WITH_OTHER` | `repo/yum/RepomdFileCollector.cc` | strToBool (default true when set): also download `other` metadata. |
| `ZYPP_REPOMD_WITH_FILELISTS` | same | Also download `filelists`. |
| `ZYPP_PLUGIN_APPDATA_FORCE_COLLECT` | `zypp/zypp/ng/repomanager.cc` | Trigger the appdata plugin unconditionally. |

### 8.3 Commit, rpm and target

| Variable | Read/set in | Effect |
|---|---|---|
| `ZYPP_SINGLE_RPMTRANS` | `ZYppCommitPolicy.cc` (read), `TargetImpl.cc` (set), `ZConfig.cc` (set from zypp.conf) | On SUSE builds, single-transaction (zypp-rpm) commit is enabled only if this is set and not a false word. With `LIBZYPP_CONFIG_USE_CLASSIC_RPMTRANS_BY_DEFAULT` it additionally applies only when the executable is `zypper`. Non-SUSE builds always use single-trans. During commit `TargetImpl` sets it to `1` (single) or unsets it (classic), so scripts can tell which backend runs. |
| `ZYPP_CLASSIC_RPMTRANS` | `TargetImpl.cc` (set only) | `1` during a classic commit, unset during a single-trans commit. |
| `ZYPP_IS_RUNNING` | `ZYppImpl::commit()` (set only) | The PID, so rpm scripts can detect that libzypp called them. |
| `SYSTEMD_OFFLINE` | `ZYppImpl::commit()` (set only) | `1` if the target is chrooted (bsc#1118758). |
| `GLIBC_TUNABLES` | `ZYppImpl::commit()` (read+set) | Appends `glibc.cpu.hwcaps=-SSE4_1,-arch_3_00,-z13` (bsc#1246912). |
| `RPM_IgnoreFailedSymlinks` | `target/rpm/RpmDb.cc` ctor (set only) | Always `1` (for patched rpm). |
| `TRANSACTIONAL_UPDATE` | `target/TargetImpl.cc` | strToBool: load commit plugins even if root ≠ `/`. |
| `ZYPP_NO_USRMERGE_PROTECT` | `ZYppCommitPolicy.cc` | QA only: disable the forced classic commit for a pending UsrMerge (bsc#1189788). |
| `ZYPP_COMMIT_NO_PACKAGE_CACHE` | `target/CommitPackageCache.cc` | Bool (exists): no package cache during commit. |
| `ZYPP_PCK_PRELOAD` | `target/commitpackagepreloader.cc` | getenvBool: parallel package preload. If unset, the default is `!LIBZYPP_CONFIG_USE_SERIAL_PACKAGE_DOWNLOAD_BY_DEFAULT`. |
| `ZYPP_RPM_DEBUG` | `target/rpm/RpmDb.cc`, `zypp/tools/zypp-rpm/main.cc` | strToBool: verbose rpm output logging. |
| `ZYPP_MODALIAS_SYSFS` | `target/modalias/Modalias.cc` | Use this dir or file instead of `/sys` (testcases set it). |
| `ZYPP_PLUGIN_DEBUG` | `PluginScript.cc` (static) | Dump the plugin communication. |
| `ZYPP_PLUGIN_TIMEOUT`, `ZYPP_PLUGIN_SEND_TIMEOUT`, `ZYPP_PLUGIN_RECEIVE_TIMEOUT` | `PluginScript.cc` (static) | Plugin I/O timeouts (default 30 s). The specific variable beats the generic one. |

### 8.4 Media, network and processes

| Variable | Read in | Effect |
|---|---|---|
| `ZYPP_CURL2` | `zypp/zypp/media/MediaHandlerFactory.cc` | getenvBool: choose MediaCurl2 (true) or MediaCurl (false). If unset, the default is `!LIBZYPP_CONFIG_USE_LEGACY_CURL_BACKEND_BY_DEFAULT` (the spec sets legacy for `suse_version < 1600`). |
| `ZYPP_MEDIA_CURL_DEBUG` | `zypp-logic/zypp-curl/curlhelper.cc`, `ng/network/networkrequestdispatcher.cc` (L_ENV_CONSTR) | ≥ 1 logs HTTP headers, 2 also logs responses, > 3 adds extra MediaCurl2 traces. Also enables the `L_*_MEDIA` log macros. |
| `ZYPP_MEDIA_CURL_IPRESOLVE` | `curlhelper.cc` | `4` or `6`: restrict name resolution. |
| `ZYPP_METALINK_DEBUG` | `zypp-curl/parser/metalinkparser.cc`, `zsyncparser.cc` | Log the parsed mirrors. |
| `ZYPP_MEDIA_PROVIDER_DEBUG` | `zyppng/lib/zypp-media/ng/provide.cc` (L_ENV_CONSTR) | Enables NG Provide debug logging (group `ZYPP_MEDIA_PROVIDE`). |
| `YAST_IS_RUNNING` | `zypp-curl/proxyinfo/proxyinfolibproxy.cc` | Changes the libproxy behaviour under YaST. |
| `PX_DEBUG` | same | getenvBool: libproxy debugging. |
| `ZYPP_FORK_BACKEND` | `zypp-logic/zypp-core/ng/io/abstractspawnengine.cc` | `auto`/`pfork` (default) or `gspawn`. |
| `ZYPP_MEDIANETWORK` | *(set only: ZConfig techpreview, tools/DownloadFiles.cc)* | **Dead**, nothing reads it. |

Proxy settings come from `/etc/sysconfig/proxy` (via libproxy or `ProxyInfoSysconfig`). curl honours the
usual `*_proxy` variables itself. See [Media](07-media-and-downloads.md).

### 8.5 Logging and debugging

| Variable | Read in | Effect |
|---|---|---|
| `ZYPP_LOGFILE` | `zypp-logic/zypp-core/base/LogControl.cc` | A log file path, or `-` for stderr. Read once (call_once) at LogControl construction. |
| `ZYPP_PROFILING` | `LogControl.cc` | Use the profiling line formatter. |
| `ZYPP_FULLLOG` | `LogControl.cc`, `sat/detail/PoolImpl.cc`, `solver/detail/Resolver.cc` | Excessive logging. libsolv debug level 2. If strToTrue, `ScopedAutoTestCaseWriter` writes a solver testcase to `/var/log/YaST2/autoTestcase` on each resolver run. It dumps the pool only the first time. |
| `ZYPP_LIBSOLV_FULLLOG` (`ZYPP_LIBSAT_FULLLOG` deprecated) | `PoolImpl.cc` | libsolv debug level 3. |
| `LIBSOLV_DEBUGMASK` | `PoolImpl.cc`, `zyppng/lib/zypp/ng/sat/pool.cc` | Raw `pool_setdebugmask` value. Takes precedence over the `*FULLLOG` variables. |

### 8.6 zypper TUI helpers (`zypp/zypp-tui`)

| Variable | Read in | Effect |
|---|---|---|
| `ZYPPER_PBD` | `output/OutNormal.cc` | Print every progress bar redraw on its own line. |
| `NO_COLOR`, `TERM` | `utils/colors.cc` | Colour handling. |
| `COLUMNS` | `utils/console.cc` | Terminal width. |

Spawned children: `ExternalProgram`/`forkspawnengine.cc` set the caller-supplied environment. They also set
`LC_ALL=C` **when** `_useDefaultLocale` is true, which means "run the child in the C locale" (see §12.3).

---

## 9. The NG config engine (`ng-config` branch)

### 9.1 History (branch vs `origin/master`)

`git log origin/master..HEAD` contains exactly eight commits by Benjamin Zeller:

| Commit | Summary |
|---|---|
| `15e974f8d` | Introduce `zyppng::config::Config` + `ContextConfig`. Moves `DownloadMode`/`ResolverFocus` from `zypp/zypp` to `zypp-logic/zypp` (dependency layering), adds the `zyppng:contextconfig` partition, wires it into `Context`, and adds tests and the `zypp.conf.ALL_KEYS` fixture. |
| `378d44411` | Move the core-type parsers to `zypp-core/ng/config/configparsers`, so the media tier needs no Arch/Locale. |
| `0d2c128a8` | Media-tier key set: `zypp-media/ng/config/mediakeys.h` and `registerMediaKeys()`. Each token documents its HOST/CONTEXT scope. |
| `78044d841` | `serializeEntries()` / `parseEntries()`: a flat `section/key → value` map, because the worker transport (STOMP headers) cannot carry multi-line INI. |
| `815f140ef` | Type-safe write API. The string-keyed typed templates are private, `set(Key<T>)` throws on an unregistered key, `set(string_view, string)` parses registered keys, and `zyppng::type_identity` is polyfilled. |
| `b80f8b2be` | Fix `parseTriBool`. `strToBoolNodefault` had made `repo_gpgcheck = no` indeterminate; it now uses `strToTriBool` like legacy. String sets are escaped so they round-trip. Adds `ConfigParsers_test`. |
| `d69788aa8` | `parseLong`/`parseUnsigned` accept the legacy forms (base 0: hex, octal, sign, whitespace). Garbage, overflow and negative unsigned values stay errors. |
| `dd0d61120` | `parseArch` accepts any value, like legacy `Arch(value)` (unknown names and `""` become custom arches). |

### 9.2 Tier placement

```
zypp-logic/zypp-core/ng/config/config.h         Config, Key<T>, Entry, ParseError   (C++17, no domain types)
zypp-logic/zypp-core/ng/config/configparsers.h  parse/serialize bool,long,unsigned,string,Pathname,TriBool,set<string>
zypp-logic/zypp-media/ng/config/mediakeys.h     media/* tokens + registerMediaKeys()    (usable by media workers)
zypp-logic/zypp/ng/config/zyppconfig.h          app tokens (Arch, Locale, ResolverFocus, DownloadMode) + registerZyppKeys()
zyppng/lib/zypp/ng/contextconfig.cppm/.cc       export module zyppng:contextconfig; ContextConfig = config::Config;
                                                makeSystemConfig(), makeRootConfig(root), makeDefaultConfig()
zyppng/lib/zypp/ng/context*.cppm/.cc            Context owns ContextConfig (_config), ctor: registerZyppKeys + parse,
                                                parse errors logged as WAR "... (using compiled-in default)"
```

These headers live under `zypp-logic/*/ng/` (the tier that may change freely). Legacy code does **not** use
them: `ZConfig` is untouched and has no link to `config::Config`. Both read the same files through the same
`EconfDict`, which is the only shared piece. Nothing in `zyppng/` uses `ZConfig` (only doc comments mention it).

### 9.3 Design

**Typed key tokens.** `Key<T>` is an aggregate of `name` (canonical `"section/keyname"`), `legacyAlias`
(the old flat zypp.conf name or empty), `defaultFn(const Config&)`, `parseFn(string_view) → expected<T>`
and `serializeFn(const T&)`. These are plain function pointers, so tokens are `inline constexpr` globals,
for example:

```cpp
inline constexpr Key<long> TransferTimeout {
  "media/transfer_timeout", "download.transfer_timeout",
  [](const Config&) { return 180L; },  /* parse with clamp [0,3600] */ ..., serializeLong };
```

**Registry.** `registerKey(Key<T>)` stores type-erased `std::function`s (default/parse/serialize via
`std::any`) and maps `legacyAlias → canonical`. Registration is **explicit** (`registerZyppKeys(cfg)`, which
calls `registerMediaKeys()` first). A key parsed before it is registered stays a raw string; this is a
documented gap.

**Storage and lookup order.** `Entry { any fileValue; any overrideValue; optional<signal> }`:

```
get(Key<T>):   overrideValue (set())  →  fileValue (parse())  →  defaultFn(*this)
getBaseline:                             fileValue            →  defaultFn          (≈ ZConfig::builtin*Path)
```

`defaultFn` receives the `Config`, so cross-key defaults are evaluated lazily. Examples: `metadatadir` =
`get(RepoCachePath)/raw`, `reposdir` = `get(ConfigPath)/repos.d`, `GeoipCachePath` = cachedir/`geoip.d`.
`effectiveUseDeltarpmAlways(cfg)` reproduces the legacy `&&` masking, and consumers **must** call it rather
than `get(UseDeltarpmAlways)`.

**Parsing (`parse()`).** It uses `$ZYPP_CONF` (non-empty) → `IniDict(root/$ZYPP_CONF)`, otherwise
`EconfDict("zypp/zypp.conf", root)`. For each entry, `resolveCanonical(section, key)` checks the alias map
for `"section/key"`, then for the bare `key`, and otherwise uses `"section/key"` unchanged. A registered key
goes through `parseFn`. On failure a `ParseError{key, rawValue, exception_ptr}` is collected and the key falls
back to its default; nothing is silently corrupted. Unknown keys are stored as raw `std::string`
(`getRaw()`). `parse()` does not emit signals.

**Value parsers (`configparsers.cc`, `zyppconfig.cc`).**
- `parseBool` is strict: an unrecognised value is an error, while legacy keeps the default.
- `parseLong`/`parseUnsigned` use base 0 like `strtonum`. Trailing garbage, empty input or overflow are
  errors, and `parseUnsigned("-1")` is an error.
- `parseTriBool` has exact legacy semantics, so unknown input means indeterminate and is not an error.
- `parseStringSet` uses `splitEscaped` on `", \t"`. `serializeStringSet` backslash-escapes the separators and
  quotes and writes `''` for an empty element.
- `parsePath` rejects values that are still `relativeDotDot()` after normalisation, because they would
  escape `root()`. Absolute values are fine.
- `parseLocale` rejects `noCode`.
- `parseResolverFocus` and `parseDownloadMode` error on unknown values. An empty `DownloadMode` means
  `DownloadDefault`, which serialises to `""`.
- `MediaPreferDownload` parses "≠ volatile" and serialises to `download`/`volatile`.
- `MediaMountDir` uses a non-validating path parser (it is host-relative).

The deliberate differences from legacy are marked `LEGACY-DIFF` in `zyppng/tests/config/ConfigParsers_test.cc`.
That file is the checklist.

**Paths are never root-prefixed** by the engine. Callers compose `cfg.root() / cfg.get(KnownReposPath)`.
Some tokens are documented as host-relative: `PluginsPath` ("plugins run on the host") and `MediaMountDir`.
Legacy `RepoManagerOptions` *does* prefix `pluginsPath` with the root, so keep this in mind when porting.

**Write API (815f140ef).**
- `set(Key<T>, type_identity_t<T>)` deduces `T` from the key only (so `set(StringKey, "lit")` works). It
  throws `std::logic_error` if the key is unregistered, which is a programmer error.
- `set(string_view sectionKey, string)` takes a canonical name or a bare legacy alias and returns
  `expected<void>`. A registered key is parsed (with clamping). On a parse error the old value is kept. An
  unregistered key must contain `/` and is stored raw.
- `reset(key)` / `resetAll()` drop the overrides. `resetAll()` snapshots the keys first, because slots may
  insert entries.
- Invariant: registered keys hold only their registered type, and unregistered keys hold only `std::string`.
  So `serialize()` cannot hit a `bad_any_cast`.

**Change signals.** `sigChanged("section/key")` creates a `sigc::signal<void(const std::any&)>` lazily.
`set()`/`reset()` emit the new effective value. The value is copied before emitting to survive an
`unordered_map` rehash caused by slots.

**IPC.** `serialize()` writes INI with sections and keys sorted, in **canonical** form (`[media]
transfer_timeout = 180`, `[solver] focus = ...`). It contains the effective value of every registered key plus
the raw keys. `parseFromString()` reads it back. `serializeEntries()`/`parseEntries()` do the same with a flat
map (one STOMP header per key). Media workers register only `registerMediaKeys()`, and the test
`media_registry_accepts_full_host_entries` checks that they accept the full host map. The serialized form is
**not** a legacy-readable zypp.conf. Legacy ZConfig would warn about unknown sections and ignore everything.

**Threading.** `Config` contains no mutex. It is owned by `Context` and must be used from the owning
context's thread (my reading of the code; there is no explicit doc).

### 9.4 Key map (canonical ← legacy alias)

| Canonical | Legacy alias | Canonical | Legacy alias |
|---|---|---|---|
| `main/arch` | `arch` | `solver/focus` | `solver.focus` |
| `main/textLocale` | — | `solver/onlyRequires` | `solver.onlyRequires` |
| `main/repo.refresh.locales` | `repo.refresh.locales` | `solver/allowVendorChange` | `solver.allowVendorChange` |
| `main/configdir`, `main/cachedir` | same name | `solver/cleandepsOnRemove` | `solver.cleandepsOnRemove` |
| `main/{metadata,solvfiles,packages}dir` | same name | `solver/dupAllow{Downgrade,NameChange,ArchChange,VendorChange}` | `solver.dupAllow…` |
| `main/{repos,services,vars,vendor}dir` | same name | `solver/upgradeTestcasesToKeep` | `solver.upgradeTestcasesToKeep` |
| `main/locksfile` | `locksfile.path` | `solver/upgradeRemoveDroppedPackages` | `solver.upgradeRemoveDroppedPackages` |
| `main/history.logfile` | `history.logfile` | `main/solver.checkSystemFile[Dir]` | same |
| `repo/add.probe` | `repo.add.probe` | `main/multiversion`, `main/multiversion.kernels`, `main/multiversiondir` | same |
| `repo/refresh.delay` | `repo.refresh.delay` | `main/rpm.install.excludedocs` | same |
| `media/use_deltarpm` | `download.use_deltarpm` | `main/lock_timeout` | `lock_timeout` |
| `media/use_deltarpm_always` | `download.use_deltarpm.always` | `main/locksfile.apply` | same |
| `media/prefer_download` | `download.media_preference` | `main/commit.downloadMode` | same |
| `media/media_mountdir` | `download.media_mountdir` | `main/update.messages.notify` | same |
| `media/{max_concurrent_connections,min_download_speed,max_download_speed,max_silent_tries,transfer_timeout,connect_timeout}` | `download.…` | `main/update.{datadir,scriptsdir,messagesdir}` | same (legacy ignores them; legacy's own spelling is `messagessdir`) |
| `media/credentials.dir`, `media/credentials.file` | `credentials.global.dir/file` | `main/gpgcheck`, `main/repo_gpgcheck`, `main/pkg_gpgcheck` | same |
| `media/geoip.enabled` | `download.use_geoip_mirror` | `main/geoip.hosts`, `media/geoip.cachedir` | — |
| `main/userData`, `main/pluginsPath`, `main/needreboot.{file,dir}`, `main/distroverpkg`, `main/repo.label.isAlias` | — (NG-only, settable programmatically **and via zypp.conf** under their canonical name) | | |

`ConfigCompat_test` installs `zyppng/tests/config/data/zypp.conf.ALL_KEYS` and asserts that every key is
registered (none stored raw) and parses without error. **When you add a legacy zypp.conf key, add it to that
fixture, to `zyppconfig.h`/`mediakeys.h` and to `registerZyppKeys()`/`registerMediaKeys()`.**

### 9.5 Known gaps NG vs legacy (verified in code at HEAD `dd0d61120`)

- **`solver.noUpdateProvide` is not registered** in NG and is missing from `zypp.conf.ALL_KEYS`. In NG it
  would be stored raw as `main/solver.noUpdateProvide`.
- There is no `[env]` handling and no `techpreview.*` / `ZYPP_SINGLE_RPMTRANS` → setenv. Worse,
  `resolveCanonical()` falls back to the **bare key alias regardless of section**. So `[env] arch = …` or
  `[env] gpgcheck = …` would be interpreted as `main/arch` / `main/gpgcheck`.
- `$ZYPP_LOCK_TIMEOUT` and `$ZYPP_TESTSUITE_FAKE_ARCH` overrides are not applied by `Config`.
- There is no `multiversion.d` scanning and no bsc#1193488 host fallback. `makeRootConfig(root)` reads only
  `root`'s files, so a root without zypp.conf gets an empty multiversion spec.
- There is no "TargetDefaults" split. A `Context` for a root parses everything (paths too) from that root's
  files, whereas legacy uses host paths + root prefix and root solver settings.
- `RepoRefreshLocales` defaults to an empty set. Legacy falls back to `Target::requestedLocales()` at query
  time, so NG callers must replicate this.
- Text locale default: legacy caches it once per process, while NG re-reads the env on every `defaultFn`
  call (on purpose, for test isolation).
- `solver.upgradeTestcasesToKeep = -1`: legacy wraps to `UINT_MAX` ("keep all", as documented in the man
  page). NG reports a parse error and uses `2`. This is a `LEGACY-DIFF` that must be resolved before
  consumers switch.
- Empty `$ZYPP_CONF`: legacy uses builtin defaults, while NG ignores the variable and uses econf.

---

## 10. Other docs in `zypp/doc`

- `zypp.conf.5.txt` → `zypp.conf(5)`. `locks.5.txt` → `locks(5)` (the `/etc/zypp/locks` format: attribute
  `repo`, `type`, `match_type`, `case_sensitive`, `solvable_name`, …, separated by blank lines).
- `zypp-NameReqPrv.1.txt` → `zypp-NameReqPrv(1)` (a testcase investigation tool).
  `zypp-CheckAccessDeleted.1.txt` exists but is **not** in `libzypp_MAN1` in `zypp/doc/CMakeLists.txt`,
  so it is not installed.
- The man pages are built with a2x (`ADD_A2X_MANPAGES`) when `ENABLE_BUILD_DOCS` is on.
- `zypp/doc/autoinclude/*.doc` are doxygen pages: `EnvironmentVariables.doc` (`zypp-envars`, outdated, see
  §8), `RepoVariables.doc`, `Plugins*.doc`, `UserData.doc`, `Modalias.doc`, `SolverVendorChange.doc`
  (vendors.d semantics) and `CodePitfalls.doc`. Update `EnvironmentVariables.doc` when you add a variable.

---

## 11. zypp.conf key audit: parser vs. documentation

Method: list every `entry == "…"` comparison in the three parser functions and compare the result with
`zypp/zypp.conf`, `zypp/zypp.conf.LEGACY` and `zypp/doc/zypp.conf.5.txt`:

- `ZConfig::Impl::TargetDefaults::consume()` (`ZConfig.cc` ~l.261)
- `ZConfig::Impl::Impl()` (~l.366–533)
- `MediaConfig::setConfigValue()` (`zypp-logic/zypp-media/mediaconfig.cc` l.56–90)

`MultiversionMap::scanConfAt()` (~l.745) reads `multiversion` a second time for each root.

**Result: the §5 table lists every parsed key.** No parsed key is missing from it. The table below adds the
parser type and the real consumer of each key. It also flags where the documentation disagrees with the code.

Legend for the documentation columns: **M** = documented in `zypp.conf.5.txt`, **L** = present (usually
commented out) in `zypp.conf.LEGACY`, **V** = set in the vendor `zypp/zypp.conf`.

| Key | Parsed in / parser | Main consumer(s) (outside ZConfig) | Docs |
|---|---|---|---|
| `lock_timeout` | Impl, `strtonum` → `long` | `ZYppFactory.cc` (`ZYppGlobalLock`) | M L |
| `arch` | Impl, `Arch(value)` (any string accepted) | `sat::PoolImpl::prepare()` (`pool_setarch`), `RepoVariables` (`$arch`/`$basearch`), `TargetImpl`, testcases | M L |
| `cachedir` / `metadatadir` / `solvfilesdir` / `packagesdir` | Impl, `Pathname` | `RepoManagerOptions(root)` (root-prefixed), `TargetImpl` (solv), zypper `-C`/`--raw-cache-dir`/… | M L |
| `configdir` | Impl, `Pathname` | base for the derived defaults (see §5) | M L |
| `reposdir` / `servicesdir` | Impl, `Pathname` | `RepoManagerOptions` | M L |
| `varsdir` | Impl, `Pathname` | `RepoVariables.cc` (`repoManagerRoot() / varsPath()`) | M L |
| `vendordir` | Impl, `Pathname` | `TargetImpl` (`_vendorAttr`, root-prefixed), `VendorAttr::noTargetInstance()` (host, function static) | M L |
| `multiversiondir` | Impl, `Pathname` | `MultiversionMap` | M L |
| `multiversion` | Impl + `scanConfAt`, `splitEscaped(", \t")` | `PoolImpl::multiversionList()`, testcases | M L **V** |
| `multiversion.kernels` | Impl, raw string | `PurgeKernels.cc` | M L **V** |
| `repo.add.probe` | Impl, `strToBool` | `RepoManagerOptions::probe` | M L |
| `repo.refresh.delay` | Impl, `strtonum` → `unsigned` | `RepoMirrorList.cc` (mirror cache age = delay×6 min), `ng/repo/workflows/repomanagerwf.cc` (refresh check) | M L |
| `repo.refresh.locales` | Impl, `str::split(", \t")` (**no** escaping) → `Locale` | `repo/yum/RepomdFileCollector.cc` | M L. The man page shows default `en`. The code default is empty, and the getter then falls back to `Target::requestedLocales()` |
| `download.use_deltarpm` / `.always` | Impl, `strToBool` | `repo/PackageProvider.cc` | M L |
| `download.media_preference` | Impl, `!= "volatile"` | `media/MediaPriority.cc` | M L |
| `download.media_mountdir` | Impl, `Pathname` | `media/MediaHandler.cc` | M L |
| `download.use_geoip_mirror` | Impl, `strToBool` | `media/MediaNetworkCommonHandler.cc`, `ng/repo/workflows/repomanagerwf.cc` | M L |
| `download.max_concurrent_connections` | MediaConfig, `strtonum` → `int` | `zypp-curl/transfersettings.cc`, `target/commitpackagepreloader.cc` | M L |
| `download.min_download_speed` / `max_download_speed` / `max_silent_tries` | MediaConfig, `int` | `TransferSettings` (`maxSilentTries` → `media/detail/MediaNetworkRequestExecutor.cc`) | M L |
| `download.connect_timeout` | MediaConfig, `int`, clamped ≥ 0 | `TransferSettings` ctor | M L |
| `download.transfer_timeout` | MediaConfig, `int`, clamped [0, 3600] | `TransferSettings` ctor | M L |
| `credentials.global.dir` / `.file` | MediaConfig, `Pathname` | `CredManagerOptions(rootdir)` (prefixed with `repoManagerRoot()`) | M L. The man page says `{configdir}/…`, but the code hardcodes `/etc/zypp/…` |
| `commit.downloadMode` | Impl, `deserializeDownloadMode` | `ZYppCommitPolicy.cc` (default for `downloadMode()`), zypper `optionsets.cc` | M L |
| `gpgcheck` / `repo_gpgcheck` / `pkg_gpgcheck` | Impl, `strToBool` / `strToTriBool` | `RepoInfo.cc` (`RepoInfo::{repo,pkg}GpgCheck()` defaults) | M L |
| `solver.focus` … `solver.upgradeRemoveDroppedPackages` (TD) | TargetDefaults | `solver/detail/Resolver.cc` (ctor defaults), `SATResolver.cc`; `upgradeTestcasesToKeep` → `TargetImpl.cc` | M L (except `noUpdateProvide`: M only) |
| `solver.checkSystemFile` / `…Dir` | Impl, `Pathname` | `solver/detail/SystemCheck.cc` | M L. The man page shows the default of `checkSystemFile` as `/etc/zypp/systemCheck`; the code uses `{configdir}/systemCheck` (LEGACY says `{configdir}`) |
| `locksfile.path` / `locksfile.apply` | Impl | `TargetImpl` (`_hardLocksFile`, root-prefixed), `Locks.h`, zypper `locks/*.cc`, yast-pkg-bindings `Target_Load.cc` | M L |
| `update.messages.notify` | Impl, raw string | `TargetImpl.cc` (`sendNotification`) | M L |
| `update.datadir`, `update.scriptsdir`, `update.messagessdir` | Impl, **ignored** | (constants are used) | not in M. L only mentions `{update.messagesdir}` in a comment. That spelling is **not** recognised and produces "Unknown entry". NG registers `update.messagesdir` (see §15) |
| `rpm.install.excludedocs` | Impl, `strToBool(value, false)` | `ZYppCommitPolicy.cc` (`rpmInstFlags`) | M L |
| `history.logfile` | Impl, `Pathname` | `HistoryLog.cc`, `parser/HistoryLogReader.h`, `TargetImpl.cc`, zypper `utils/misc.cc` | M L |
| `ZYPP_SINGLE_RPMTRANS`, `techpreview.ZYPP_SINGLE_RPMTRANS`, `techpreview.ZYPP_MEDIANETWORK` | Impl, `setenv` | env readers (§8.3) | **undocumented** |
| `[env] *` | Impl, collected and then `setenv(…, 0)` | everything that reads env later | M |

Documented-but-not-parsed and parsed-but-not-documented, summarised:

- **Documented, not parsed:** `update.messagesdir` (LEGACY comment only). Nothing in the man page.
- **Parsed, not in the man page:** `update.{datadir,scriptsdir,messagessdir}` (ignored anyway),
  `ZYPP_SINGLE_RPMTRANS`, `techpreview.ZYPP_SINGLE_RPMTRANS`, `techpreview.ZYPP_MEDIANETWORK`.
- **Parsed, in the man page, not in `zypp.conf.LEGACY`:** `solver.noUpdateProvide`. Only matters for old
  distros that ship the LEGACY file.
- **Getters with no zypp.conf key:** `pluginsPath()`, `needrebootFile()/Path()`, `distroverpkg()`,
  `repoLabelIsAlias()`, `userData()`, `textLocale()`, `pubkeyCachePath()`, `geoipHostnames()`,
  `geoipCachePath()`, `update_{data,scripts,messages}Path()`.
- **Default mismatches man page ↔ code:** `repo.refresh.locales` (`en` vs. empty), `solver.checkSystemFile`
  (`/etc/zypp` vs `{configdir}`), `credentials.global.*` (`{configdir}` vs `/etc/zypp`).

Legacy parser oddity: the `[env]` duplicate-key branch ("duplicate key … shadowing") cannot trigger. `iniMap`
has already merged duplicate keys, so each section/key pair appears only once.

---

## 12. Environment variables libzypp *sets* (and other env side effects)

§8 lists what libzypp reads. This table lists every place where libzypp itself changes the process
environment (from a grep for `setenv`, `unsetenv` and `env::ScopedSet` in `zypp/`, `zypp-logic/` and
`zyppng/`, tests excluded).

### 12.1 Persistent changes (whole process)

| Variable | Where | When | Purpose |
|---|---|---|---|
| `BOOST_TEST_CATCH_SYSTEM_ERRORS=no` | `ZConfig.cc` `_autodetectTextLocale()` | first `defaultTextLocale()` | `#warning HACK` for the boost test runner. It leaks into every libzypp process and its children. |
| `ZYPP_SINGLE_RPMTRANS` | `ZConfig::Impl()` | zypp.conf key present | no-overwrite (`setenv(…,0)`) |
| `ZYPP_MEDIANETWORK` | `ZConfig::Impl()`, `zypp/tools/DownloadFiles.cc` | techpreview key | overwrites. **Dead**, nothing reads it. |
| any `[env]` key | `ZConfig::Impl()` | after parsing | no-overwrite |
| `RPM_IgnoreFailedSymlinks=1` | `target/rpm/RpmDb.cc` (`RpmDb` ctor) | every RpmDb | for a SUSE-patched rpm |
| `ZYPP_TESTSUITE_FAKE_ARCH` | `misc/TestcaseSetup.cc` | loading a solver testcase with an arch | **disables commit** for the rest of the process |
| `ZYPP_MODALIAS_SYSFS` | `misc/TestcaseSetup.cc` | testcase with `hardwareInfo` | fakes modalias matching |

### 12.2 Scoped changes (`env::ScopedSet`, restored on scope exit)

`env::ScopedSet` (`zypp-logic/zypp-core/base/Env.h`) remembers the old value and restores or unsets it in
its destructor. A `nullptr` value means "unset for this scope".

| Variable(s) | Where | Scope |
|---|---|---|
| `ZYPP_IS_RUNNING=<pid>`, `SYSTEMD_OFFLINE=1` (only if root ≠ `/`, bsc#1118758), `GLIBC_TUNABLES+=glibc.cpu.hwcaps=-SSE4_1,-arch_3_00,-z13` (bsc#1246912) | `zypp_detail/ZYppImpl.cc` `commit()` | the whole commit |
| `ZYPP_SINGLE_RPMTRANS` unset + `ZYPP_CLASSIC_RPMTRANS=1` | `target/TargetImpl.cc` `commit()` (classic, ~l.1746) | classic rpm transaction |
| `ZYPP_SINGLE_RPMTRANS=1` + `ZYPP_CLASSIC_RPMTRANS` unset | `TargetImpl.cc` `commitInSingleTransaction()` (~l.2091) | zypp-rpm transaction |
| `KDE_FULL_SESSION`, `GNOME_DESKTOP_SESSION_ID`, `DESKTOP_SESSION` **unset** | `zypp-curl/proxyinfo/proxyinfolibproxy.cc` `getProxyFactory()` (`TmpUnsetEnv`) | while creating the libproxy factory. Forces libproxy to use `/etc/sysconfig/proxy` instead of desktop settings. |

These are plain `setenv`/`unsetenv` calls on the **process** environment. They are not thread-safe, so any
thread that calls `getenv` during a commit (for example a download thread of the package preloader) races
with them. No problem has been reported; this is a code-reading observation.

### 12.3 Environment given to children only

| Variable | Where | Note |
|---|---|---|
| caller map (`ExternalProgram::Environment`) | `zypp-core/ng/io/forkspawnengine.cc` (after `fork`, before `exec`) / gspawn engine (copies `environ` + map) | `setenv()` in the forked child of a multi-threaded process is not async-signal-safe (theoretical deadlock on the libc env lock). |
| `LC_ALL=C` | same | when the spawn uses `use_default_locale = true`. Most libzypp helpers (rpm, mount, …) run this way. |
| `USER`, `PASSWD` | `media/MediaCIFS.cc`, `zyppng/tools/zypp-media-smb/smbprovider.cc` | credentials for `mount.cifs`. They are visible in `/proc/<pid>/environ` of the mount helper. |

### 12.4 Variables set by zypper (not libzypp) that libzypp reads

| Variable | zypper code | Why |
|---|---|---|
| `ZYPP_LOCKFILE_ROOT=<root>` | `zypper/src/Zypper.cc` (shell and normal command dispatch, `setenv(…,0)`) | bnc#575096 "Quick fix": with `--root`/`--installroot` (`changedRoot && root != "/"`) the zypp lock lives inside the target. Not set for the few commands that need no lock (bnc#703598). |
| `ZYPP_REPO_RELEASEVER` | `zypper/src/Config.cc` (`--releasever`) | overwrites. Read by `RepoVariables.cc`. |

zypper also reads `ZYPP_READONLY_HACK` itself (`Zypper.cc`). YaST (yast-pkg-bindings) sets no env variables.

---

## 13. Other configuration and state files read by libzypp

"Root" column: **T** = prefixed with the target or RepoManager root, **H** = always the host path,
**ctx** = prefixed with `ZConfig::repoManagerRoot()`.

| File / dir | Format | Reader (class / file) | Root | Notes |
|---|---|---|---|---|
| `{configdir}/repos.d/*.repo` | INI, one section per repo | `parser::RepoFileReader`, `RepoManager` (`zypp/zypp/ng/repomanager.cc`) | T (`RepoManagerOptions`) | Written by RepoManager. See ch. 05. |
| `{configdir}/services.d/*.service` | INI | `parser::ServiceFileReader` | T | |
| `{pluginsPath}/services/*` | executable, prints repo INI | `repo::PluginServices` | T | |
| `{configdir}/vars.d/*` | file name = variable name, first line (untrimmed) = value, dotfiles skipped | `repo/RepoVariables.cc` `RepoVarsMap::parse` | ctx | Reset when `repoManagerRoot()` changes (bsc#1237044). The singleton TODO is in the code. |
| `{configdir}/vendors.d/*` | INI `[main] vendors = a,b,…` | `VendorAttr` (`zypp/zypp/VendorAttr.cc`) | T via `TargetImpl::_vendorAttr`; **H** for `VendorAttr::noTargetInstance()` | `noTargetInstance()` is a function static that is built once from the host path. |
| `{configdir}/locks` | `locks(5)`: blank-line separated attribute blocks | `Locks` (`zypp/zypp/Locks.cc`) | T (`TargetImpl::_hardLocksFile`) | Written by zypper `al`/`rl` and YaST. Applied when `locksfile.apply`. |
| `{configdir}/multiversion.d/*` | one spec per line | `ZConfig` `MultiversionMap::scanDirAt` | T for the root, but the **dir name comes from host config** | §4.2 |
| `{configdir}/needreboot`, `needreboot.d/*` | one capability per line (`SolvableSpec::parseFrom`) | `TargetImpl.cc` (~l.1365) | T | Feeds `PoolImpl::needrebootSpec`. The builtin `installhint(reboot-needed)` is always added. |
| `{configdir}/systemCheck`, `systemCheck.d/*` | `requires:<cap>` / `conflicts:<cap>` per line, `#` comments | `solver/detail/SystemCheck.cc` | T **at first use only** | See pitfall: the singleton and its file-scope statics are never re-read. |
| `/etc/zypp/credentials.d/*`, `/etc/zypp/credentials.cat` | `[url]` sections with `username`/`password` | `media::CredentialManager` / `CredentialFileReader` | ctx (`CredManagerOptions(repoManagerRoot())`) | Paths come from `MediaConfig` (`credentials.global.*`). |
| `$HOME/.zypp/credentials.cat` | same | `CredentialManager` | ctx: `rootdir / $HOME / …` | The *caller's* `$HOME` is used inside the target. |
| `$HOME/.curlrc` | curl config subset (`proxy-user` only) | `zypp-curl/curlconfig.cc` `CurlConfig::parseConfig` | H | Parsed only if the dir and file are readable by the user (logs "Not allowed to parse"). Used by `MediaCurl`, NG `request.cc`. |
| `/etc/sysconfig/proxy` | shell-style sysconfig | `ProxyInfoSysconfig`, or libproxy (`proxyinfolibproxy.cc` `getProxyFactory()`) | H | Watched with `WatchFile`. When it changes, the libproxy factory is rebuilt with the desktop session variables temporarily unset (§12.2). |
| `/etc/sysconfig/storage` (`USED_FS_LIST`) | sysconfig | `sat/detail/PoolImpl.cc` `requiredFilesystems()` | **H** | Watched with `WatchFile` in `prepare()`; a change triggers `depSetDirty`. Used for `filesystem(…)` namespace deps. Not root-aware. |
| `/etc/products.d/*.prod`, `/etc/products.d/baseproduct` | product XML | `parser::ProductFileReader`, `TargetImpl` (base product, distribution flavor/version) | T | `baseproduct` is watched per root (`_baseproductWatcher`). |
| `/etc/rpm/platform` | first line, `arch-vendor-os` | `zypp-logic/zypp/Arch.cc` (ARM hard-float detection) | **H** | §6 |
| `/proc/cpuinfo`, `cpuid`, `getauxval` | — | `Arch.cc` | H | §6 |
| `/sys` (or `$ZYPP_MODALIAS_SYSFS`) | modalias files | `target/modalias/Modalias.cc` | H | |
| `/var/lib/zypp/AutoInstalled` | one name per line | `TargetImpl` (`_autoInstalledFile`) | T (`TargetImpl::home()` = `root/var/lib/zypp`) | |
| `/var/lib/zypp/RequestedLocales` | one locale per line | `TargetImpl` (`RequestedLocalesFile`) | T | Also the fallback for `repoRefreshLocales()`. |
| `/var/lib/zypp/AnonymousUniqueId` | single UUID line (created from `/proc/sys/kernel/random/uuid`) | `TargetImpl::anonymousUniqueId()` | T | Sent as HTTP header `X-ZYpp-AnonymousId` (`MediaNetworkCommonHandler.cc`, function static, root guessed). |
| `/var/lib/zypp/LastDistributionFlavor` | single line | `TargetImpl::distributionFlavor()` | T | Written by YaST/SUSEConnect, not libzypp. |
| `/var/lib/zypp/SoftLocks` | — | `TargetImpl.cc` (~l.1353) | T | **Deleted** if found (legacy file). |
| `{cachedir}/solv/@System/` | solv + cookie | `TargetImpl::buildCache()` | T | |
| `{cachedir}/pubkeys`, `/usr/lib/rpm/gnupg/keys` | ASCII armored keys | `zypp-common/private/keyring_p.cc` | H, plus `systemRoot()` and `repoManagerRoot()` copies | The key cache is searched in all three roots. |
| `{pluginsPath}/{commit,system,urlresolver,appdata,repoverification}` | executables (stomp protocol for commit/system) | `PluginExecutor` (`TargetImpl`), `UrlResolverPlugin`, `ng/repomanager.cc` | commit/system/urlresolver: **H** (`ZConfig::pluginsPath()` unprefixed); appdata/repoverification/services: T (`RepoManagerOptions::pluginsPath`) | commit plugins only if root == `/` or `$TRANSACTIONAL_UPDATE`. system plugins only if root == `/`. |
| `/var/lib/YaST2/cookies` | curl cookie jar | `MediaCurl` (`_cookieFile`), NG `request_p.h` | H | Mode forced to 0600 if root or owner. |
| `/etc/ssl/certs` | CA dir | `TransferSettings` default `_ca_path` | H | Overridable per URL (`ssl_capath`). |
| `/etc/mtab`, `/proc/mounts` | mount table | `zypp-media/mount.cc`, `DiskUsageCounter.cc` | H | |
| `/var/lib/rpm` / `/usr/lib/sysimage/rpm` | rpmdb | `target/rpm/librpmDb.cc` | T | |
| `/run/reboot-needed` | flag file (written) | `TargetImpl.cc` (classic and single-trans commit) | T | Created or touched when a committed item `isNeedreboot()`. |
| `/var/lib/update-messages/yast2-packagemanager.rpmdb.configfiles` | text (**written**) | `RpmDb.cc` (~l.1637) | **H** literal path | Lists changed config files (`.rpmnew`/`.rpmsave` diffs). Not root-prefixed. |
| `{lockfileDir}/zypp.pid` | PID | `ZYppFactory.cc` (`ZYppGlobalLock`) | `$ZYPP_LOCKFILE_ROOT/run` | |
| `/etc/zypp/zypper.conf`, `~/.zypper.conf` | augeas INI | **zypper**, not libzypp | — | Listed only to avoid confusion. zypper's `main.showAlias` calls `ZConfig::repoLabelIsAlias()`. |

There is no `tvm.conf` in libzypp. The name does not appear in the tree (grep).

---

## 14. Runtime override API (`ZConfig` setters) and their users

Applications override config values after start. All of these setters work on the singleton and are
process-wide. None of them writes back to a file.

| Setter / reset | Semantics | Users |
|---|---|---|
| `setRepoManagerRoot(root)` | Changes `repoManagerRoot()` only (vars, credentials, keyring, media). Empty = follow `systemRoot()`. | zypper `--installroot` → `setRepoManagerRoot("/")` (`zypper/src/Config.cc`); tests (`zypp-logic/tests/lib/TestSetup.h`, `zyppng/tests/media/Provider_test.cc`) |
| `announceSystemRoot(root)` | `systemRoot()` without a Target, cleared on target change (bsc#1237044) | zypper `ZypperBaseCommand::defaultSystemSetup()` when the command does not init the target |
| `setSystemArchitecture(arch)` / `resetSystemArchitecture()` | Plain member store + WAR. **Not propagated**: the pool calls `pool_setarch` only in `PoolImpl::prepare()` when `_serial` changed (repo added/removed). A new arch set after loading repos is ignored until the next repo change. `RepoVariables` caches `$arch` until the context root changes. | `TestcaseSetup.cc`, `TestSetup.h`. zypper/YaST only read it. |
| `setTextLocale(loc)` / `resetTextLocale()` | Propagates to `sat::Pool::setTextLocale()` immediately | yast-pkg-bindings `Locale.cc` (`PkgFunctions::SetTextLocale`) |
| `setUserData(str)` | Rejects control chars except TAB; returns false | zypper `--userdata` |
| `setRepoCachePath` / `setRepoMetadataPath` / `setRepoSolvfilesPath` / `setRepoPackagesPath` | `DefaultOption::set`. Does **not** change `builtinRepo*Path()`, so geoip and orphan cleanup keep using the file value. Affects only `RepoManagerOptions` constructed **afterwards**. | zypper `-C`, `--raw-cache-dir`, `--solv-cache-dir`, `--pkg-cache-dir` (they also copy into `rm_options` directly) |
| `setGeoipEnabled(bool)` | plain member | no user found in zypper/YaST |
| `repoLabelIsAlias(bool)` | plain member, read by `RepoInfoBase::label()` | zypper `main.showAlias` from zypper.conf |
| `set_download_media_prefer_download(bool)` / `set_default_…()` | `DefaultOption` | yast-pkg-bindings `Pkg::SetZConfig` key `download_media_prefer_download` |
| `set_download_mediaMountdir(p)` / `set_default_…()` | `DefaultOption` | none found |
| `setGpgCheck` / `setRepoGpgCheck` / `setPkgGpgCheck` + `reset…()` | `DefaultOption` | none found in zypper/YaST (zypper's `--no-gpg-checks` works through `KeyRing` callbacks and `RepoInfo::setGpgCheck`) |
| `setSolverUpgradeRemoveDroppedPackages(bool)` / `reset…()` | writes `targetDefaults()`, **lost** on `notifyTargetChanged()` | yast-pkg-bindings `Pkg::SetZConfig` key `solver_upgrade_remove_dropped_packages` |
| `multiversionSpec(set)` / `add…` / `remove…` / `clearMultiversionSpec()` | per-root cache, notifies `PoolImpl::multiversionSpecChanged()` | testcases |
| `setUpdateMessagesNotify(str)` / `reset…()` | `DefaultOption` | yast-pkg-bindings `Pkg::SetZConfig` key `update_messages_notify` |
| `notifyTargetChanged()` | internal, `ZYppImpl::changeTargetTo()` only | — |

There are **no** setters for: the `solver.*` flags (except `upgradeRemoveDroppedPackages`), `lock_timeout`,
`download.*` transport values (MediaConfig has no setters at all), `commit.downloadMode`, the locks file,
`rpm.install.excludedocs`, `history.logfile` or `pluginsPath`. Applications change solver behaviour per
`Resolver` (`Resolver::setOnlyRequires()`, `setFocus()`, … – zypper's `--no-recommends` etc.). Commit
behaviour goes per `ZYppCommitPolicy` (`downloadMode()`, `rpmInstFlags()`), and download behaviour per URL
query parameters (`TransferSettings`). The read API used by yast-pkg-bindings is `Pkg::ZConfig()`
(`PkgFunctions.cc` ~l.287), which exports a map of selected getters.

### 14.1 `MediaConfig` is only filled by `ZConfig`

`MediaConfig::instance()` (`zypp-logic/zypp-media/mediaconfig.cc`) starts with builtin defaults. The only
callers of `setConfigValue()` are:
- the `ZConfig::Impl` constructor, and
- `zyppng/lib/zypp-media/ng/worker/provideworker.cc`, which takes entries `zconfig://main/<key>` from the
  worker configuration.

Consequences:
- Code that touches `MediaConfig` (for example `TransferSettings()` default ctor, `CredManagerOptions`)
  before `ZConfig::instance()` exists sees builtin defaults. In legacy libzypp this is almost impossible
  because `getZYpp()` builds ZConfig early. Standalone tools linking only zypp-logic are affected.
- **NG workers:** a grep finds no producer of `zconfig://main/...` keys. `provide-configvars.h` only defines
  `zconfig://media/{UserAgent,DistributionFlavor,AnonymousId,AttachPoint,ProviderRoot}`. So the media
  workers always run with the builtin timeouts, `max_silent_tries` and credentials paths, whatever zypp.conf
  says. The NG `serializeEntries()` (§9.3) is meant to replace this, but it is not wired in yet (§15.3).

---

## 15. NG config: complete key registry and branch status

### 15.1 All registered keys (`registerMediaKeys()` + `registerZyppKeys()`)

The 11 media keys are in `zypp-logic/zypp-media/ng/config/mediakeys.h` and the 52 app keys in
`zypp-logic/zypp/ng/config/zyppconfig.h` (count of `inline constexpr Key`). The default is the `defaultFn`
result. "→" means the default is derived lazily from another key.

| Canonical key | Type | Default | Legacy key / getter |
|---|---|---|---|
| `media/credentials.dir` | Pathname | `/etc/zypp/credentials.d` | `credentials.global.dir` |
| `media/credentials.file` | Pathname | `/etc/zypp/credentials.cat` | `credentials.global.file` |
| `media/geoip.enabled` | bool | true | `download.use_geoip_mirror` |
| `main/geoip.hosts` | set\<string\> | `{download.opensuse.org}` | `geoipHostnames()` (no file key) |
| `media/max_concurrent_connections` | long | 5 | `download.max_concurrent_connections` |
| `media/min_download_speed` | long | 0 | `download.min_download_speed` |
| `media/max_download_speed` | long | 0 | `download.max_download_speed` |
| `media/max_silent_tries` | long | 1 | `download.max_silent_tries` |
| `media/transfer_timeout` | long, clamp [0,3600] | 180 | `download.transfer_timeout` |
| `media/connect_timeout` | long, clamp ≥ 0 | 60 | `download.connect_timeout` |
| `media/media_mountdir` | Pathname (non-validating) | `/var/adm/mount` | `download.media_mountdir` |
| `main/arch` | Arch | `Arch::detectSystemArchitecture()` | `arch` |
| `main/textLocale` | Locale | env `LC_ALL`/`LC_MESSAGES`/`LANG` → `en` (re-read on each call) | `textLocale()` |
| `main/repo.refresh.locales` | LocaleSet | `{}` | `repo.refresh.locales` |
| `main/configdir` | Pathname | `/etc/zypp` | `configdir` |
| `main/cachedir` | Pathname | `/var/cache/zypp` | `cachedir` |
| `main/metadatadir`, `solvfilesdir`, `packagesdir` | Pathname | → cachedir/`raw`, `solv`, `packages` | same names |
| `main/reposdir`, `servicesdir`, `varsdir`, `vendordir` | Pathname | → configdir/`repos.d`, `services.d`, `vars.d`, `vendors.d` | same names |
| `main/locksfile` | Pathname | → configdir/`locks` | `locksfile.path` |
| `main/history.logfile` | Pathname | `/var/log/zypp/history` | `history.logfile` |
| `media/geoip.cachedir` | Pathname | → cachedir/`geoip.d` | `geoipCachePath()` |
| `repo/add.probe` | bool | false | `repo.add.probe` |
| `repo/refresh.delay` | unsigned | 10 | `repo.refresh.delay` |
| `media/use_deltarpm` | bool | `LIBZYPP_CONFIG_USE_DELTARPM_BY_DEFAULT` | `download.use_deltarpm` |
| `media/use_deltarpm_always` | bool | false | `download.use_deltarpm.always` (read via `effectiveUseDeltarpmAlways()`) |
| `media/prefer_download` | bool (≠ `volatile`) | true | `download.media_preference` |
| `main/gpgcheck` | bool | true | `gpgcheck` |
| `main/repo_gpgcheck`, `main/pkg_gpgcheck` | TriBool | indeterminate | same |
| `solver/focus` | ResolverFocus | `Default` | `solver.focus` |
| `solver/onlyRequires`, `allowVendorChange`, `cleandepsOnRemove` | bool | false | `solver.*` |
| `solver/dupAllowDowngrade`, `dupAllowNameChange`, `dupAllowArchChange` | bool | true | `solver.*` |
| `solver/dupAllowVendorChange` | bool | false | `solver.dupAllowVendorChange` |
| `solver/upgradeTestcasesToKeep` | unsigned | 2 | `solver.upgradeTestcasesToKeep` (`-1` is a parse error) |
| `solver/upgradeRemoveDroppedPackages` | bool | true | `solver.upgradeRemoveDroppedPackages` |
| `main/multiversion` | set\<string\> | `{}` | `multiversion` |
| `main/multiversion.kernels` | string | `""` | `multiversion.kernels` |
| `main/rpm.install.excludedocs` | bool | false | `rpm.install.excludedocs` |
| `main/lock_timeout` | long | 0 | `lock_timeout` |
| `main/locksfile.apply` | bool | true | `locksfile.apply` |
| `main/userData` | string | `""` | `userData()` (no file key in legacy) |
| `main/pluginsPath` | Pathname | `/usr/lib/zypp/plugins` | `pluginsPath()` |
| `main/update.datadir` | Pathname | `/var/adm` | `update.datadir` (**ignored** by legacy) |
| `main/update.scriptsdir` | Pathname | `/var/adm/update-scripts` | `update.scriptsdir` (ignored by legacy) |
| `main/update.messagesdir` | Pathname | `/var/adm/update-messages` | legacy spells it `update.messagessdir` and ignores it |
| `main/update.messages.notify` | string | `""` | `update.messages.notify` |
| `main/solver.checkSystemFile` | Pathname | → configdir/`systemCheck` | same |
| `main/solver.checkSystemFileDir` | Pathname | → configdir/`systemCheck.d` | same |
| `main/needreboot.file`, `main/needreboot.dir` | Pathname | → configdir/`needreboot`, `needreboot.d` | `needrebootFile()/Path()` |
| `main/multiversiondir` | Pathname | → configdir/`multiversion.d` | `multiversiondir` |
| `main/distroverpkg` | string | `system-release` | `distroverpkg()` |
| `main/repo.label.isAlias` | bool | false | `repoLabelIsAlias()` |
| `main/commit.downloadMode` | DownloadMode | `DownloadDefault` | `commit.downloadMode` |

Not registered: `solver.noUpdateProvide`, `ZYPP_SINGLE_RPMTRANS`, `techpreview.*`, and anything under `[env]`
(all of them end up raw, or are misrouted through the bare-alias fallback, see §9.5).

**Behaviour differences you only see in this table:**
- NG makes `update.*dir` **effective** where legacy ignores them on purpose ("should not be user
  configurable"). An old LEGACY-style zypp.conf with these keys would change paths under NG.
- NG accepts `main/userData`, `main/pluginsPath`, `main/distroverpkg`, … from **files**. Legacy has no file
  key for them. Decide whether these should be settable programmatically only before NG consumers rely on them.

### 15.2 Write API recap

The API is described in §9.3. Quick reference (`zypp-logic/zypp-core/ng/config/config.h`):

```cpp
cfg.set( SolverOnlyRequires, true );                 // typed, throws std::logic_error if unregistered
auto r = cfg.set( "download.transfer_timeout", "9000" ); // string, parsed+clamped → 3600, expected<void>
cfg.reset( SolverOnlyRequires ); cfg.resetAll();     // drop overrides → file value → default
cfg.getBaseline( RepoCachePath );                    // file/default, ignoring set() (≈ builtinRepoCachePath)
cfg.sigChanged( "solver/onlyRequires" ).connect(…);  // emitted by set()/reset(), not by parse()
```

Nothing in NG writes a config **file**. `serialize()` exists only for IPC.

### 15.3 Branch status (`git log origin/master..HEAD`, HEAD = `dd0d61120`)

The eight commits are listed in §9.1. There are no `TODO`/`FIXME`/`\todo` markers in
`zypp-logic/*/ng/config/`, `zyppng/lib/zypp/ng/contextconfig*` or `zyppng/tests/config/` (grep). The open
work below comes from reading the code.

| Done | Not done / TODO |
|---|---|
| Typed `Key<T>` engine, registry, alias mapping, lazy cross-key defaults | **No consumer** reads it yet. A grep for `config().get`/`cfg.get(` in `zyppng/lib` and `zypp-logic` (outside docs) finds nothing, so all behaviour still comes from code that does not use `Config`. |
| econf/`$ZYPP_CONF` parsing identical in file selection to legacy | `serializeEntries()` not yet sent to media workers. Workers still get only `zconfig://media/*` (see §14.1). |
| Media/app key split by tier, `registerMediaKeys()` usable in workers | `solver.noUpdateProvide` key + `ALL_KEYS` fixture entry |
| Write API with overrides, reset, signals | `[env]`, `techpreview.*`, `ZYPP_SINGLE_RPMTRANS` handling; section-aware alias lookup |
| Parsers aligned with legacy (TriBool, numeric forms, Arch) | `$ZYPP_LOCK_TIMEOUT`, `$ZYPP_TESTSUITE_FAKE_ARCH` overrides |
| IPC serialization (INI + flat map) | multiversion.d + bsc#1193488 host fallback; TargetDefaults-like host/target split |
| Tests: `Config_test`, `ConfigCompat_test` (all legacy keys parse), `ConfigParsers_test` (`LEGACY-DIFF` list) | Decide `upgradeTestcasesToKeep=-1`, strict `parseBool`, and the empty-`$ZYPP_CONF` semantics; decide whether NG-only keys (`pluginsPath` …) may come from files; `update.*dir` effective vs ignored |

---

## Pitfalls & special cases

- `ZConfig` always parses the **host** (`/`) zypp.conf at construction. Only the `solver.*` TargetDefaults
  and `multiversion` are re-read from the target root. Paths stay host values and are prefixed with the root
  by callers (`RepoManagerOptions`, `TargetImpl`).
- A target change resets runtime `setSolverUpgradeRemoveDroppedPackages()` and recomputes TargetDefaults.
  The resolver's default flags are re-applied right after (`ZYppImpl::changeTargetTo`).
- `ZConfig::systemRoot()` and `multiversionSpec()` call `getZYpp()`, which may create ZYpp and grab the lock.
- `/etc/zypp/zypp.conf` (any content, even `/dev/null`) **replaces** the vendor file entirely. It then loses
  `multiversion=provides:multiversion(kernel)` unless the admin copies it.
- `/run/zypp/zypp.conf[.d]` is part of the lookup (between /etc and vendor) but is undocumented in the man page.
- Drop-ins need their own `[main]` header (bsc#1272534). Otherwise the keys go into the unnamed section and
  are ignored with a warning.
- Only `*.conf` drop-ins count. Hidden files are skipped. The same filename in /etc masks the vendor one.
- `$ZYPP_CONF` is relative to the root, disables drop-ins, and falls back to *builtin* defaults (not vendor)
  if the file is missing.
- `[env]` never overwrites existing variables and is too late for anything read at static init or before
  ZConfig exists (`ZYPP_LOGFILE`, `ZYPP_FULLLOG`, plugin timeouts, `ZYPP_READONLY_HACK`).
- `techpreview.ZYPP_MEDIANETWORK` overwrites the env, but nothing reads `ZYPP_MEDIANETWORK` any more.
- The `credentials.global.*` defaults are hardcoded to `/etc/zypp/...` and do not follow `configdir`, despite
  the man page.
- `download.connect_timeout` has no ZConfig getter (it is MediaConfig only). `transfer_timeout` is clamped to
  [0, 3600].
- `geoipCachePath()` uses the *builtin* cachedir, not a runtime `setRepoCachePath()`.
- `update.*dir` keys are silently ignored (constants under `/var/adm`). The legacy key name has a typo
  (`messagessdir`).
- `solver.upgradeTestcasesToKeep=-1` only means "keep all" because strtoul wraps it. NG rejects it.
- `ZYPP_TESTSUITE_FAKE_ARCH` makes every commit throw. `TestcaseSetup` sets it (and `ZYPP_MODALIAS_SYSFS`)
  process-wide.
- Arch detection is cached per process. ARM hard-float detection reads the **host** `/etc/rpm/platform`.
  x86_64 is silently upgraded to `_v2/_v3/_v4` from CPUID.
- `_autodetectTextLocale()` sets `BOOST_TEST_CATCH_SYSTEM_ERRORS=no` in every process using libzypp.
  `LANGUAGE` is ignored.
- `ZYPP_SINGLE_RPMTRANS` semantics depend on build flags (`SUSE`, `LIBZYPP_CONFIG_USE_CLASSIC_RPMTRANS_BY_DEFAULT`,
  zypper-only check via `/proc/self/exe`). During commit the library itself sets or unsets it, together with
  `ZYPP_CLASSIC_RPMTRANS`.
- Many env variables are cached in function statics. Tests that `setenv()` mid-process may see stale values.
- NG: register keys **before** `parse()`. Keys parsed earlier stay raw strings forever.
- NG: the bare-alias fallback in `resolveCanonical()` ignores the section (`[env] arch=` → `main/arch`).
- NG: `solver.noUpdateProvide` is missing, and there is no `[env]`/techpreview, multiversion.d, lock-timeout
  env or fake-arch env support yet.
- NG: `serialize()` output uses canonical sections. It is for workers, not a zypp.conf for legacy.
- NG: always use `effectiveUseDeltarpmAlways()`. Paths are never root-prefixed by `Config`.
- `update.messagesdir` (the spelling used in the LEGACY comment and by NG) is *not* a legacy key. Legacy only
  knows the typo `update.messagessdir`, and ignores it anyway. NG makes all `update.*dir` keys effective.
- Man page and code disagree on the defaults of `repo.refresh.locales` (`en` vs. requested locales),
  `solver.checkSystemFile` (`/etc/zypp` vs `{configdir}`) and `credentials.global.*` (`{configdir}` vs
  `/etc/zypp`) (§11).
- `SystemCheck` is a singleton whose file and caps live in file-scope statics. `loadFile()` prefixes the
  path with the target root **in place**, once, at first use. If it is first used before a Target exists or
  under another root, the stale file and prefix are kept across target changes. Only
  `TestcaseSetup` (`setFile()`) re-reads it. Likely bug for long-running multi-root users (YaST).
- `setSystemArchitecture()` does not reach the solver pool until the pool serial changes (repo
  add/remove). `RepoVariables` keeps `$arch`/`$basearch` until the context root changes. Set the arch before
  loading repos.
- `/etc/sysconfig/storage`, `/etc/rpm/platform`, `~/.curlrc`, `/etc/sysconfig/proxy`, the cookie jar, and
  the commit/system/urlresolver plugin dirs are always **host** paths, even with `--root` (§13).
- `VendorAttr::noTargetInstance()` is a function static built from the host `vendordir`. Changes to
  zypp.conf or `vendors.d` after the first use without a Target are not seen.
- `RpmDb` writes the changed-config-files report to the literal host path
  `/var/lib/update-messages/yast2-packagemanager.rpmdb.configfiles`, also for `--root` installs.
- `MediaConfig` gets its values only from the `ZConfig` constructor. The NG media workers never get the
  `download.*`/`credentials.*` values (no producer of `zconfig://main/*`), so they always use builtin
  timeouts and credential paths (§14.1).
- libzypp changes the process environment during commit (`ZYPP_IS_RUNNING`, `SYSTEMD_OFFLINE`,
  `GLIBC_TUNABLES`, `ZYPP_{SINGLE,CLASSIC}_RPMTRANS`) and during libproxy setup (it unsets the desktop
  session variables). Concurrent `getenv` from other threads races with this.
- `BOOST_TEST_CATCH_SYSTEM_ERRORS=no` and `RPM_IgnoreFailedSymlinks=1` are set permanently and inherited by
  every child (rpm scriptlets, plugins).
- zypper, not libzypp, sets `ZYPP_LOCKFILE_ROOT` (`--root`) and `ZYPP_REPO_RELEASEVER` (`--releasever`).
  Other libzypp applications must do the same themselves.
- `setRepo*Path()` overrides only affect `RepoManagerOptions` built afterwards. Builtin cleanup and geoip
  keep using the zypp.conf value. `setSolverUpgradeRemoveDroppedPackages()` is lost on every target change.
- NG accepts `pluginsPath`, `userData`, `distroverpkg` etc. from zypp.conf under their canonical names. In
  legacy these were API-only (§15.1).

## Where to start when debugging

- **Which files were read?** Run with `ZYPP_LOGFILE=/tmp/z.log` (or `-`) and grep the log group `zconfig`
  and the econf messages: `getConfigFiles for stem`, `take <file>`, `masked <file> by <file>`,
  `No config files found`, `Reading zypp.conf for root`, `$ZYPP_CONF is set to`. `ZyppConfIniMap` logs the
  merged dict at MIL and dumps it at DBG.
- **Unknown or ignored keys:** grep for `zypp.conf: Unknown entry in [main]` and `Unknown section`
  (the classic symptom of a drop-in without `[main]`), and for `zypp.conf [env]: set/skip`.
- **Arch and locale:** the startup lines `Uname architecture is`, `architecture upgraded/downgraded`,
  `Overriding system architecture`, `Default text locale is`, and the `about()` block (`SystemArchitecture: 'x' (default)`).
- **Root issues:** `notifyTargetChanged (<root>)`, `MultiversionSpec '<root>' = ...`. Check whether
  `announceSystemRoot()` or `setRepoManagerRoot()` was called (`RepoVariables` and the keyring use
  `repoManagerRoot()`).
- **Reproduce with an isolated config:** `ZYPP_CONF=/path/test.conf` (relative to root!), or build a fake
  root with `etc/zypp/zypp.conf.d/` and pass `--root`. In unit tests, `EconfDict::defaultDistconfDir(path)`
  redirects the vendor dir.
- **Tests:** `zypp/tests/parser/inifile/EconfDict_test.cc` (+ `data/EconfDict`) for the merge rules;
  `zyppng/tests/config/{Config,ConfigCompat,ConfigParsers}_test.cc` for NG (run via `ctest -R Config`).
  `ConfigParsers_test` lists all `LEGACY-DIFF` rows.
- **Env variables:** check §8. Remember that `ZYPP_FULLLOG=1` also writes solver testcases on each solve,
  and that `ZYPP_MEDIA_CURL_DEBUG=2` / `ZYPP_LIBSOLV_FULLLOG=1` are very verbose.
- **Is a key parsed at all?** `grep -n 'entry == "' zypp/zypp/ZConfig.cc zypp-logic/zypp-media/mediaconfig.cc`
  is the complete list (§11). Compare it with `grep -c 'inline constexpr Key'` in the NG headers (§15.1) and
  with `zyppng/tests/config/data/zypp.conf.ALL_KEYS`.
- **Who overrides a value at runtime?** Check §14. In the stack: `grep -rn 'ZConfig::instance()\.set\|announceSystemRoot' zypper/src`
  and `yast-pkg-bindings/src/PkgFunctions.cc` (`Pkg::SetZConfig`: only `download_media_prefer_download`,
  `update_messages_notify`, `solver_upgrade_remove_dropped_packages`).
- **Unexpected env in scriptlets:** §12 lists everything libzypp sets. Dump `/proc/<scriptlet-pid>/environ`
  during a commit to see `ZYPP_IS_RUNNING`, `ZYPP_SINGLE_RPMTRANS`/`ZYPP_CLASSIC_RPMTRANS`, `SYSTEMD_OFFLINE`
  and `GLIBC_TUNABLES`.
- **Wrong file used under `--root`:** check the Root column in §13. Log lines: `Read <file>` / `Can't read`
  (SystemCheck), `Going to parse` / `Not allowed to parse` (`.curlrc`), `RepoVars context changed`,
  `Build Libproxy Factory from /etc/sysconfig/proxy`.
- **Changelog:** `zypp/package/libzypp.changes`. Search for `zypp.conf`, `econf`, `UAPI`, `[env]`, bsc#1237044,
  bsc#1193488, bsc#1272534 and bnc#906096 for the history behind each special case.
