# Target, Commit, RPM and Plugins

This chapter covers the *installed system* side of libzypp: the `Target` (rpm database, `@System` solv cache,
the state files in `/var/lib/zypp`), the commit pipeline (download, file-conflict check, the two rpm backends
*classic_rpmtrans* and *single_rpmtrans* with the out-of-process `zypp-rpm` helper, `%posttrans` handling,
update scripts/messages, needreboot), the history log, the global zypp lock, and the plugin framework.

All paths are relative to `libzypp/`. Nearly everything here is in the **legacy tier** (`zypp/zypp/...`, read-only per
`AGENTS.md`); the wire protocol to `zypp-rpm` is in `zypp-logic/shared/commit/`.

---

## 1. Big picture

```
 application (zypper / YaST / PK)
   │  getZYpp()  ──────────────► ZYppFactory  (/run/zypp.pid lock)
   │  zypp->initializeTarget("/")
   │                               ZYppImpl::initializeTarget → new Target(root) → TargetImpl
   │                                    RpmDb::initDatabase  (rpmdb path, compat symlink, key sync)
   │                               Target::buildCache()  → rpmdb2solv → /var/cache/zypp/solv/@System/solv
   │  zypp->target()->load()   → @System repo in the pool, AutoInstalled, locks, needreboot spec
   │  ... resolve ...
   │  zypp->commit(policy)
   ▼
 ZYppImpl::commit                          (zypp/zypp/zypp_detail/ZYppImpl.cc)
   │  env: ZYPP_IS_RUNNING, SYSTEMD_OFFLINE (chroot), GLIBC_TUNABLES
   ▼
 TargetImpl::commit(ResPool, policy)       (zypp/zypp/target/TargetImpl.cc)
   │  ShutdownLockCommit (systemd-inhibit)
   │  transaction = resolver.getTransaction().order()
   │  write upgrade testcase (dup), show patch messages
   │  CommitPackagePreloader (parallel HTTP) + CommitPackageCache (download/verify)
   │  commit plugins: COMMITBEGIN
   │  store RequestedLocales / AutoInstalled / locks
   │  ├─ singleTrans → commitInSingleTransaction → fork zypp-rpm (one rpm transaction)
   │  └─ classic     → commitFindFileConflicts + commit(): one `rpm -U/-e` per package
   │  commit plugins: COMMITEND
   │  buildCache()  (rebuild @System solv; system plugins PACKAGESETCHANGED)
   ▼
 ZYppImpl::commit: target->load() (syncPoolAfterCommit) or unload()
```

Key classes and files:

| Class / file | Role |
|---|---|
| `zypp/zypp/Target.{h,cc}` | Public pimpl facade; thin forwarding to `TargetImpl`. `chrooted()` = `!root().emptyOrRoot()`. |
| `zypp/zypp/target/TargetImpl.{h,cc}` | Everything: cache build/load, commit (both backends), update scripts/messages, product/distro queries. |
| `zypp/zypp/target/TargetImpl.commitFindFileConflicts.cc` | libsolv `pool_findfileconflicts` driver. |
| `zypp/zypp/target/rpm/RpmDb.{h,cc}` | Wrapper around the `rpm` CLI (install/remove/rebuilddb/runposttrans/key import) + `checkPackage`. |
| `zypp/zypp/target/rpm/librpmDb.{h,cc}` | Read-only librpm access (`db_const_iterator`), dbpath detection. |
| `zypp/zypp/target/rpm/RpmHeader.{h,cc}`, `BinHeader.*` | librpm `Header` wrappers (`readPackage`, `tag_*()`). |
| `zypp/zypp/target/rpm/RpmFlags.h` | `RpmInstFlag` bits (shared header-only with `zypp-rpm`). |
| `zypp/zypp/target/RpmPostTransCollector.*` | Delayed `%posttrans` / `%transfiletrigger` handling (classic mode). |
| `zypp/zypp/target/CommitPackageCache*.{h,cc}` | Package provision during commit (CD/DVD read-ahead). |
| `zypp/zypp/target/commitpackagepreloader.cc` | Parallel HTTP pre-download (NG network stack). |
| `zypp/zypp/target/{HardLocksFile,RequestedLocalesFile,SolvIdentFile}.*` | Persistent state files. |
| `zypp/zypp/target/modalias/Modalias.*` | `modalias()` namespace for hardware-supplements. |
| `zypp/tools/zypp-rpm/main.cc` | The single-transaction helper binary. |
| `zypp-logic/shared/commit/CommitMessages.{h,cc}` | Protocol messages libzypp ⇄ zypp-rpm. |
| `zypp/zypp/ZYppCommitPolicy.*`, `ZYppCommitResult.*` | Commit input/output. |
| `zypp/zypp/ZYppFactory.cc`, `zypp_detail/ZYppImpl.*` | Singleton + global lock. |
| `zypp/zypp/HistoryLog.*`, `HistoryLogData.*` | `/var/log/zypp/history` writer/reader. |
| `zypp/zypp/PluginScript.*`, `PluginExecutor.*`, `zypp-logic/zypp-core/rpc/PluginFrame.*` | Plugin framework. |
| `zypp/zypp/media/UrlResolverPlugin.*` | `plugin:` URL scheme. |
| `zypp/zypp/misc/CheckAccessDeleted.*` | `zypper ps` backend (lsof parsing). |
| `zypp-logic/zypp-core/ShutdownLock{.cc,_p.h}` | `systemd-inhibit` wrapper. |
| `zypp/zypp/DiskUsageCounter.{h,cc}` | Per-mount-point disk usage estimate (§12). |
| `zypp/zypp/target/TargetCallbackReceiver.*`, `target/rpm/RpmCallbacks.h` | Classic-mode rpm report adapters (§14). |
| `zypp/zypp/parser/ProductFileReader.*` | `/etc/products.d/*.prod` parser (§13). |

---

## 2. ZYpp singleton and the global lock

`ZYppFactory::instance().getZYpp()` (`zypp/zypp/ZYppFactory.cc`) returns a `ZYpp::Ptr`. The instance is held in a
`static weak_ptr<ZYpp>`; once all strong refs are gone the `ZYpp` is destroyed and the lock released
(`~ZYpp` resets `_theGlobalLock`). The `ZYpp::Impl` however is created **once per process** (`static _theImplInstance`)
and reused when re-acquiring — so pool/resolver state survives a release/re-acquire cycle. Each new `ZYpp`
calls `zyppintern::repoVariablesReset()`.

### Lock file

* Path: `ZYppFactory::lockfileDir() / "zypp.pid"` where `lockfileDir()` = `$ZYPP_LOCKFILE_ROOT/run`
  (default `/run`). **The lock is not placed inside the target root** – `--root` installs lock the host's `/run/zypp.pid`
  unless `ZYPP_LOCKFILE_ROOT` is set (documented as a "Hack to circumvent the currently poor --root support").
* Implementation `ZYppGlobalLock`: file opened `a+`, `boost::interprocess::file_lock` (flock) taken only while
  reading/writing the pid. The *real* lock semantics are "file contains a pid of a running process":
  `isProcessRunning()` checks `/proc/<pid>` and that `/proc/<pid>/cmdline` is non-empty (zombie → not running).
  A stale pid is ignored and overwritten.
* On release the file is **truncated, not deleted** (other processes may still flock it).
* **Non-root (`geteuid() != 0`) never locks** and never checks the lock.
* `ZYPP_READONLY_HACK` (env, read at static init) or `zypp_readonly_hack::IWantIt()` (`zypp/zypp/zypp_detail/ZYppReadOnlyHack.h`)
  skips locking for root as well.

### Lock timeout

`ZConfig::lockTimeout()`: `$ZYPP_LOCK_TIMEOUT` overrides `zypp.conf [main] lock_timeout` (bsc#1239809), default `0`.
`0` = fail immediately with `ZYppFactoryException` (carries `lockerPid()`/`lockerName()`), `>0` = wait that many
seconds, `<0` = wait forever (bsc#1184399, bsc#1213231). The retry loop sleeps 1,2,…,60 s (capped) and logs a
warning once per day.

### zypp-rpm lock

`zypp-rpm` writes its own `zypp-rpm.pid` in the same dir (path passed in the `Commit` message as `lockFilePath`).
After obtaining the global lock, `getZYpp()` checks `zypp-rpm.pid` and refuses if a zypp-rpm is still running
(e.g. parent zypper was killed but the rpm transaction continues). **Observation:** in the current code this check is
nested inside the `else if ( globalLock().zyppLocked() )` branch, i.e. it is only executed when the global lock was
initially contended. Keep this in mind if you get reports of concurrent access after a killed zypper.

---

## 3. Target initialisation, rpmdb and the `@System` cache

### `ZYppImpl::initializeTarget(root, doRebuild)`

* Repeated call with same root is a no-op.
* Different root: old target is `unload()`ed and **destroyed before** the new one is created (bsc#1203760 — two
  live `TargetImpl`s break the rpmdb/HistoryLog globals; `changeTargetTo` logs `INT` if it happens).
* `changeTargetTo` calls `ZConfig::notifyTargetChanged()` (target-dependent config like `/etc/zypp` inside root)
  and resets solver default flags.
* Then `Target::buildCache()`. Note `initializeTarget` does **not** `load()` – the application must call
  `target()->load()`.

### `TargetImpl` ctor

```cpp
_root, _requestedLocalesFile(home()/"RequestedLocales"), _autoInstalledFile(home()/"AutoInstalled"),
_hardLocksFile(root + ZConfig::locksFile())  // /etc/zypp/locks
_vendorAttr(root + ZConfig::vendorPath()), _baseproductWatcher(root/etc/products.d/baseproduct)
_rpm.initDatabase(root, doRebuild); HistoryLog::setRoot(root); createAnonymousId();
sigMultiversionSpecChanged();   // HACK bnc#906096: multiversion spec depends on target root
```

`home()` is **`root/var/lib/zypp`** (static `TargetImpl::home(root)`), independent of `ZYppImpl::homePath()`.

Files in `/var/lib/zypp`:

| File | Writer | Notes |
|---|---|---|
| `AutoInstalled` | `SolvIdentFile` | idents of auto-installed packages. Written at commit (from `Transaction::autoInstalled()`) and by `updateAutoInstalled()`. If missing, initialised from history (see §8). |
| `RequestedLocales` | `RequestedLocalesFile` | written at commit; on load only applied if **non-empty** (bare-metal install case: don't clobber application settings). |
| `AnonymousUniqueId` | `createAnonymousId()` | UUID from `/proc/sys/kernel/random/uuid`; **not created if root != "/"** (bsc#1024741); `anonymousUniqueId(root)` falls back to the host's id. Sent as HTTP header to d.o.o. |
| `LastDistributionFlavor` | `createLastDistributionFlavorCache()` | from base product flavor on each `load()`. |
| `SoftLocks` | – | obsolete, unlinked on the fly when `AutoInstalled` is first created. |

All three state-file classes write via `TmpFile::makeSibling` + `rename` (atomic), mode 0644, and only rewrite when data
`differs()`.

Hard locks (`/etc/zypp/locks`, `HardLocksFile`, PoolQuery serialisation) are applied on load and written on commit only
if `ZConfig::apply_locks_file()` (zypp.conf `locksfile.apply`, default true; file path via `locksfile.path`).

### rpmdb location (`librpmDb.cc`, `RpmDb::initDatabase`)

* `internal::suggestedDbPath(root)`: if `root/var/lib/rpm` **or** `root/usr/lib/sysimage/rpm` exists as a *real
  directory* (LSTAT, symlinks don't count) use it (checked in that order); else rpm's `%{_dbpath}`; if that is
  empty assume `/usr/lib/sysimage/rpm`.
* `dbExists` looks for `Packages`, `Packages.db` or `rpmdb.sqlite`.
* **Compat symlink:** if dbpath ≠ `/var/lib/rpm` and `root/var/lib/rpm` doesn't exist, inject
  `/var/lib/rpm -> ../../usr/lib/sysimage/rpm`. Comment: "Required at least until rpmdb2solv takes a dbpath
  argument" (rpmdb2solv is now called with `-D`, but the symlink stays). `TargetImpl::commit` re-creates it at the end
  of every commit because removing/updating `rpm` in a transaction can delete it.
* `initDatabase` on an already initialised `RpmDb` with the same root is a no-op even if the suggested dbpath has
  changed meanwhile (rpm `%post` migrating `/var/lib/rpm` → `/usr/lib/sysimage/rpm`); a different root throws
  `RpmDbAlreadyOpenException`.
* `doRebuild_r` → `rebuildDatabase()` runs `rpm --rebuilddb -vv` with root `/` and the **symlink-expanded** dbpath
  (otherwise rpm would replace the symlink with a directory). Progress is computed by counting headers then parsing
  `D:  read h#` lines.
* `initDatabase` ends with `syncTrustedKeys()` (rpm keyring ⇄ zypp keyring, see [Security](06-security-gpg-keyring.md)).
  `RpmDb` also connects a `KeyRingSignalReceiver` so keys trusted/removed in the zypp keyring are imported/removed in rpm.
* `RpmDb` ctor sets `RPM_IgnoreFailedSymlinks=1` in the environment (for patched rpm versions).
* `librpmDb` is read-only (`O_RDONLY`), opened per `db_const_iterator`; there are no persistent handles (the old
  `dbRelease` code is `#if 0`).

All writes go through the `rpm` CLI: `RpmDb::run_rpm` builds `rpm --root R --dbpath D [-vv if ZYPP_RPM_DEBUG] ...`,
prefixed by `#/` (ExternalProgram "chdir to /" magic, bnc#819354/bsc#827609 – rpm needs a readable cwd; relative package
paths are made absolute by `workaroundRpmPwdBug`). `systemReadLine` polls with a 5 s timeout and checks the process
is still alive — protection against daemons started from `%post` that keep the pipe open (bnc#174548).

### `@System` solv cache (`TargetImpl::buildCache`)

* Location: `root + ZConfig::repoSolvfilesPath() / "@System"` → typically `/var/cache/zypp/solv/@System/{solv,cookie,solv.idx}`.
* Freshness cookie: `RepoStatus( rpmDbStateHash(root) ) && RepoStatus(root/etc/products.d)`. `rpmDbStateHash` uses
  libsolv's `rpm_hash_database_state` (SHA1 of the rpmdb state). Cookie match → no rebuild.
* Rebuild: `rpmdb2solv [-r root] -D <dbpath> -X -p root/etc/products.d [oldsolv] -o tmpfile` (binary path
  `ZYPP_RPMDB2SOLV_PATH` in zypp-stack devel builds). `-X` autogenerates pattern/product pseudo packages. `-A`
  (application pseudo packages) was dropped (bsc#1104415). The old solv file is passed to speed up conversion.
* If the cache dir is not writable (non-root user, stale cache) it falls back to `getZYpp()->tmpPath()/@System`
  (`_tmpSolvfilesPath`, `solvfilesPathIsTemp()`), cleaned at application exit.
* After a rebuild, `sat::updateSolvFileIndex` writes `solv.idx` (zypper bash completion) and, **only if root == "/"**,
  system plugins in `/usr/lib/zypp/plugins/system` get `PACKAGESETCHANGED`.

### `TargetImpl::load(force)`

1. `buildCache()`; if `@System` already loaded and cache unchanged and `!force` → return early.
2. `system.addSolv()`; on failure `clearCache()` + rebuild + retry.
3. `satpool.rootDir(root)`.
4. Requested locales (if non-empty), AutoInstalled (bootstrap from history if missing).
5. needreboot spec (see §7).
6. Hard locks → `ResPool::setHardLockQueries`.
7. `createLastDistributionFlavorCache()`.

`reload()` = `load(false)`; `unload()` erases `@System` from the pool.

### Product / distribution queries

`targetDistribution*()`, `distributionLabel()`, `distributionVersion()` read `root/etc/products.d/baseproduct`
(`parser::ProductFileReader`). A missing/dangling baseproduct symlink is logged as `ERR`. `distributionVersion` falls
back to the first package providing `ZConfig::distroverpkg()` (RHEL/Fedora `system-release` style). Static overloads take
a root; `staticGuessRoot("")` uses `ZConfig::systemRoot()` or `/`. `distributionVersion()` uses a `WatchFile` cache.

---

## 4. ZYppCommitPolicy

`zypp/zypp/ZYppCommitPolicy.{h,cc}` (RWCOW pimpl):

| Setting | Default | Notes |
|---|---|---|
| `restrictToMedia(n)` | 0 | Only `1` is honoured (`n==1 ? 1 : 0`). "Fake outstanding YCP fix": YaST installs media 1 first. In `TargetImpl::commit` collection stops at the first item with `mediaNr() > 1` (to not violate install order). |
| `dryRun(bool)` | false | Stored as `RPMINST_TEST` bit in `rpmInstFlags`! |
| `downloadMode(m)` | `ZConfig::commit_downloadMode()` (zypp.conf `commit.downloadMode`) | see below |
| `rpmInstFlags(f)` | `ZConfig::rpmInstallFlags()` (only `rpm.install.excludedocs` is configurable) | |
| `rpmNoSignature`, `rpmExcludeDocs`, `allowDowngrade`, `replaceFiles` | false | flag setters on `rpmInstFlags` |
| `syncPoolAfterCommit` | true | `ZYppImpl::commit` reloads `@System` afterwards; false → unloads it. |
| `singleTransModeEnabled()` | build/env dependent | see §5 |

`DownloadMode` (`zypp-logic/zypp/DownloadMode.h`): `DownloadDefault`, `DownloadOnly`, `DownloadInAdvance`,
`DownloadInHeaps`, `DownloadAsNeeded`. Resolution in `TargetImpl::commit`:

* `DownloadDefault` → `DownloadInHeaps` if root == "/", else `DownloadInAdvance` (singletrans) or `DownloadAsNeeded` (classic).
* `DownloadOnly` → forces `dryRun(true)`; packages are downloaded but nothing installed.
* `DownloadInHeaps` is currently treated like `DownloadInAdvance` ("Until now this means pre-loading all packages").
* `DownloadAsNeeded` disables the preload phase **and forces the classic backend** (see below).

`RpmInstFlag` (`zypp/zypp/target/rpm/RpmFlags.h`): `EXCLUDEDOCS 0x1, NOSCRIPTS 0x2, FORCE 0x4, NODEPS 0x8, IGNORESIZE 0x10,
JUSTDB 0x20, NODIGEST 0x40, NOSIGNATURE 0x80, NOUPGRADE 0x100, TEST 0x200, NOPOSTTRANS 0x400, ALLOWDOWNGRADE 0x800,
REPLACEFILES 0x1000, ALLOWUNTRUSTED 0x2000`. This header is compiled into both libzypp and `zypp-rpm`; changing values
breaks the wire protocol (flags are sent as integer).

---

## 5. Backend selection: classic_rpmtrans vs single_rpmtrans

### Why two backends

* **classic_rpmtrans** (`TargetImpl::commit(policy, cache, result)`): libzypp orders the transaction itself
  (`sat::Transaction::order()`) and calls `rpm -U`/`rpm -e` **once per package** with `--nodeps --force`. rpm never sees
  the whole transaction, so `%pretrans`/`%posttrans` and file triggers would run at the wrong time; libzypp therefore
  collects and runs `%posttrans` itself (§6). Packages can be downloaded on demand (`DownloadAsNeeded`). Robust, slow.
* **single_rpmtrans** (`TargetImpl::commitInSingleTransaction`): all steps are passed to the `zypp-rpm` helper which
  builds **one librpm transaction** (`rpmtsAddInstallElement/rpmtsAddEraseElement`, `rpmtsOrder`, `rpmtsRun`). rpm does
  ordering, scriptlets, triggers and `%posttrans` natively. Much faster. Requires all package headers up front →
  incompatible with `DownloadAsNeeded`.

Why out-of-process: librpm must not run inside the application process (it can print to stdout, it may be updated in the
transaction, signal handling, crash isolation). The helper ignores SIGINT/SIGPIPE and libzypp intercepts
SIGINT/SIGTERM/SIGHUP/SIGQUIT during the transaction and only reports "not possible to cancel a running rpm transaction".

### Selection logic (`ZYppCommitPolicy.cc`)

```
#ifdef SUSE
  #if LIBZYPP_CONFIG_USE_CLASSIC_RPMTRANS_BY_DEFAULT       (spec: SUSE suse_version < 1600)
     enabled = (/proc/self/exe basename == "zypper") && ZYPP_SINGLE_RPMTRANS set && !false-ish
  #else                                                    (Code16 / suse_version >= 1600)
     enabled = ZYPP_SINGLE_RPMTRANS set && !false-ish      (any application, still opt-in!)
  #endif
#else
  enabled = true                                           (non-SUSE builds: singletrans default)
#endif
singleTransModeEnabled():
  if enabled && downloadMode == DownloadAsNeeded  → false  ("negotiated with YAST/Agama", don't change)
  if enabled && pendingUsrmerge()                 → false  (bsc#1189788, bsc#1236481; one JobReport::info)
```

* "false-ish" = `str::strToFalse` → `0,no,false,never,off,-` disable; anything else enables.
* zypp.conf `[main] techpreview.ZYPP_SINGLE_RPMTRANS=…` (or `ZYPP_SINGLE_RPMTRANS=…`) is translated by
  `ZConfig` into `setenv(…, overwrite=0)`, so a real environment variable wins.
* `pendingUsrmerge()`: target root has a real `/lib` directory **and** a to-be-installed package provides
  `may-perform-usrmerge`. `ZYPP_NO_USRMERGE_PROTECT=1` disables the protection (QA only).
* The decision is evaluated at commit time (the spec comment says "level of enablement is handled in the code").
* While committing, each backend scopes `ZYPP_SINGLE_RPMTRANS` / `ZYPP_CLASSIC_RPMTRANS` in the environment so
  scriptlets can tell which backend runs them.
* `ZYppCommitResult::singleTransactionMode()` tells the application which backend ran (zypper prints extended error
  info for singletrans failures).

---

## 6. The commit pipeline in detail (`TargetImpl::commit(ResPool, policy)`)

1. Copy policy; `ShutdownLockCommit lck("zypp")` → runs
   `systemd-inhibit --what=sleep:shutdown:idle --who=zypp --mode=block --why="Zypp commit running." /usr/bin/cat`
   in a separate process group; killed (SIGTERM, then SIGKILL after 10 s) in the destructor. Failures are ignored.
2. Resolve `DownloadMode` (§4).
3. `result.rTransaction() = resolver.getTransaction(); .order()` — libsolv transaction ordering
   (see [Solver](04-solver.md), `sat::Transaction`). `TransactionStepList` = all steps (or the media-1 prefix).
4. Dist-upgrade (`upgradeMode()` or `upgradingRepos()`) and not dry-run → `writeUpgradeTestcase()` into
   `root/var/log/updateTestcase-YYYY-MM-DD-HH-MM-SS`, keeping `solver.upgradeTestcasesToKeep` (default 2).
5. Not dry-run → for every to-be-installed patch with a `message()`, `PatchMessageReport::show()`; `false` aborts
   (`TargetAbortedException`).
6. If `!dryRun || DownloadOnly`:
   * `CommitPackageCache packageCache; setCommitList(steps)`.
   * Unless `DownloadAsNeeded`: `CommitPackagePreloader::preloadTransaction(steps)` then a loop calling
     `packageCache.get(pi)` for every install step (this is where checksum/signature verification happens through
     `repo::PackageProvider`, see [Media & downloads](07-media-and-downloads.md) and
     [Security](06-security-gpg-keyring.md)). Abort → throw; skip/error → `STEP_ERROR`, `miss = true`.
     Then `packageCache.preloaded(true)` (avoids duplicate `infoInCache` callbacks later).
   * If anything was missed → `ERR "Some packages could not be provided. Aborting commit."` and **nothing is installed**
     (no exception; the result has `STEP_ERROR`/`STEP_TODO` steps and `attemptToModify()==false`).
   * Else `CommitActiveReport::start()` (may veto), load commit plugins **only if (root == "/" or
     `$TRANSACTIONAL_UPDATE` is true) and not dry-run**, send `COMMITBEGIN`.
   * Not dry-run: store `RequestedLocales`, `AutoInstalled`, hard locks **before** touching rpm.
   * Not dry-run: singletrans → `commitInSingleTransaction`; classic → `commitFindFileConflicts` + `commit(...)`.
     Then `preloader->cleanupCaches()` (wipes each repo's `predownloadPath()`).
   * Explicit dry-run (not DownloadOnly): singletrans runs `zypp-rpm` with `RPMINST_TEST` (rpm test transaction);
     classic only runs the file-conflict check.
   * `COMMITEND` to plugins, `buildCache()` ("while rpm database is still in cache"), `CommitActiveReport::end()`.
7. Else (pure dry-run): explicit dry-run → file-conflict check only.
8. Re-inject `/var/lib/rpm` compat symlink if rpm removed it.

`ZYppImpl::commit` wraps this with:
* `ZYPP_TESTSUITE_FAKE_ARCH` set → commit refused.
* `ZYPP_IS_RUNNING=<pid>` (for scriptlets), `SYSTEMD_OFFLINE=1` when chrooted (bsc#1118758),
  `GLIBC_TUNABLES+=glibc.cpu.hwcaps=-SSE4_1,-arch_3_00,-z13` (bsc#1246912 – keep ld.so from picking up subarch
  (hwcaps) libraries while they are half-installed).
* After commit: `syncPoolAfterCommit` → `target->load()` else `unload()` (not on dry-run).

### Package provisioning

* `CommitPackageCache` (`CommitPackageCache.cc`): `$ZYPP_COMMIT_NO_PACKAGE_CACHE` → plain `Impl` (no read-ahead), else
  `CommitPackageCacheReadAhead`. The read-ahead only matters for `cd:`/`dvd:` repos: before switching media it copies all
  remaining packages of the current medium into the cache to avoid swapping back and forth.
* `RepoProvidePackage` uses `repo::PackageProvider` with `DeltaCandidates` (delta rpms) and a
  `QueryInstalledEditionHelper` that queries the **host `/` rpmdb** (applydeltarpm rebuilds from installed files).
* `CommitPackagePreloader` (`commitpackagepreloader.cc`): parallel HTTP(S) downloads via
  `zyppng::NetworkRequestDispatcher` with `download.max_concurrent_connections`, mirror tainting on 404, GeoIP redirect,
  URL resolver plugins, d.o.o/cdn.o.o specific headers (flavor + anonymous id). Enabled unless
  `$ZYPP_PCK_PRELOAD` is false, or (when unset) disabled by the build option
  `LIBZYPP_CONFIG_USE_SERIAL_PACKAGE_DOWNLOAD_BY_DEFAULT` (spec: SUSE < 1600). **It also does nothing unless someone
  listens on `media::CommitPreloadReport`.** Only packages with a checksum and not already cached, from repos with
  curl-handled URLs, are preloaded into `RepoInfo::predownloadPath()`; the regular `packageCache.get()` then still
  verifies them. User cancel in the progress callback → `missed()`.

### File conflicts (`commitFindFileConflicts`)

Runs libsolv `pool_findfileconflicts` on `transaction().installedResult()` with
`USE_SOLVABLEFILELIST | CHECK_DIRALIASING | USE_ROOTDIR`. Headers come from the rpmdb (`rpm_byrpmdbid`) for installed
packages and from the **cached rpm file** for new ones; packages without a cached file land in `noFilelist()`.
`FindFileConflictstReport::result()` decides whether to continue (zypper asks the user). Abort →
`TargetAbortedException`. Only done for classic mode and explicit dry-run (singletrans relies on rpm).

### Classic backend (`TargetImpl::commit(policy, cache, result)`)

Per step:
* `TRANSACTION_IGNORE` package step = obsoleted by rpm implicitly → `STEP_DONE`, ident remembered in
  `obsoletedPackages` (bsc#1243279).
* Install: `packageCache.get()`, then `rpm().installPackage(file, flags, &postTransCollector)` with
  `flags = (policy & JUSTDB) | NODEPS | FORCE [| NOUPGRADE if multiversion] [| TEST] [| EXCLUDEDOCS] [| NOSIGNATURE]`.
  "Why force and nodeps? … We use rpm just to unpack and register the package." `HistoryLog().install()`.
  If `isNeedreboot()` → touch `root/run/reboot-needed`. Install **failure stops the commit** (`break`).
* Remove: `rpm().removePackage(p, NODEPS [|JUSTDB] [|TEST])`; failure only marks `STEP_ERROR` and continues
  (unless user aborted). `rpm -e --allmatches name-version-release.arch` (no epoch: "rpm -e does not like epochs").
* Non-packages: orphan `Product` removal deletes `/etc/products.d/<referenceFilename>` if not owned by a package;
  `SrcPackage` installs via `installSrcPackage`. Products can't be installed without their release package.
* `RpmInstallReport::problem()` allows ABORT / RETRY / IGNORE per rpm call (`RpmDb::installPackage` loop).
* After the loop: `%posttrans` execution (or discard on abort), update scripts, update messages,
  `logPatchStatusChanges` (jsc#SLE-5116), then throw `TargetAbortedException` if aborted.

`RpmDb::doInstallPackage` details:
* `rpm -U|-i --percent --noglob [--ignorearch if ZConfig arch incompatible with default] … -- <quoted file>`.
* Lines `%%NN` → progress; every other line → `InstallResolvableReport::contentRpmout` report (with `lineno`) and
  collected for the history log, capped at `MAXRPMMESSAGELINES` (10000) except lines containing `" scriptlet failed, "`.
* `warning: … saved as / created as …` lines → `processConfigFiles` writes a diff to `root/var/log/YaST2/config_diff_<date>.log`
  and touches `/var/lib/update-messages/yast2-packagemanager.rpmdb.configfiles` (**host path, not root-prefixed** –
  legacy YaST behaviour).
* `rpm_status != 0` → history comment + `RpmSubprocessException` with the rpm output in the exception history.
* Optional backup (`_packagebackups`, `/var/adm/backup`) is legacy and normally off.

### `%posttrans` collection (`RpmPostTransCollector`, log group `zypp::posttrans`)

Classic mode must emulate transaction-end scripts:
1. `RpmDb::installPackage` checks `hasPosttransScript(file)` (header `POSTTRANSPROG` non-empty and not `<lua>` — Lua
   is left to rpm) and adds `RPMINST_NOPOSTTRANS` (`--noposttrans`).
2. If root == "/" or the rpm-4.18.0 bug workaround is not active, rpm is called with `--define "_dump_posttrans 1"`
   (bsc#1041742). Newer rpm then prints `dump_posttrans:` lines (for `%posttrans` *and* `%transfiletrigger(in|postun)`),
   which are collected into a dumpfile; old rpm ignores the define.
3. If no dump lines arrived, the script body is extracted from the header into a tmp script
   `root/var/adm/update-scripts/posttransXXXX/<ident>XXXXXX`.
4. At the end `executeScripts()`: first own scripts via `/bin/sh <script> <N>` chrooted (`N=2` if the package was an
   update i.e. in `obsoletedPackages`, else 1 — bsc#1243279; earlier versions passed 0, see changelog #168/#190), then
   `rpm -vv --runposttrans <dumpfile>` with `--define "_tmppath <dir>"` (Bug 1218459: avoid leftover `rpm-tmp.*`,
   root prefix stripped bsc#1250343). Script identity is parsed from `-vv` debug lines (`RIPOFF:`), output goes to
   history + `JobReport` with content type `cmdout/%posttrans`, plus a `ProgressReport`.
5. Mixed case: if rpm was *downgraded* during the commit and stops emitting dump lines, already collected
   `dump_posttrans: install …` entries are converted back to header scripts (`recallFromDumpfile`).
6. On abort `discardScripts()` only logs what was skipped.

`workaroundDUMPPOSTTRANS_BUG_1216091`: rpm-4.18.0 without `rpm_fixed_runposttrans` provide does not chroot in
`rpm --root X --runposttrans`; for chrooted targets dump mode is then not used. The check is evaluated in
`initDatabase` against the **host** rpmdb (`/`) (fixed runtime check bsc#1257068).

### Single-transaction backend (`commitInSingleTransaction` + `zypp-rpm`)

Flags: `policy.rpmInstFlags() | NODEPS | NODIGEST | NOSIGNATURE | ALLOWUNTRUSTED` — digests/signatures were already
checked by libzypp at download time (bsc#1190059 "Do not double check signatures"). Steps are filled into
`proto::target::Commit` (`InstallStep{stepId, pathname, multiversion}`, `RemoveStep{stepId, n, v, r, a}`); `stepId` is
the index in `TransactionStepList`. `ignoreArch` mirrors the classic `--ignorearch` rule. `SrcPackage`s go in as
install steps.

Process setup (careful, order matters):

```
libzypp (parent)                                  zypp-rpm (child, ZYPP_RPM_BINARY in libexecdir/zypp/)
  stdin  ── Commit STOMP frame ──────────────────►  PluginFrame(std::cin) → Commit::fromStompMessage
  fd 3   ◄── STOMP messages (PackageBegin, …) ────  pushMessage() on ExpectedFds::MessageFd (=3)
  fd 4   ◄── raw scriptlet output + 0xDEADBEEF\n ─  rpmtsSetScriptFd(fdDup(4)), endOfScriptTag after each *_STOP
  stdout/stderr ◄── diagnostics (logged L_ERR "zypp-rpm")
```

* `zypp-rpm` refuses to run on a TTY and checks fds 3/4 are pipes. The comment in `TargetImpl.cc` about a size header
  ("1) Size of the commit message…") is outdated – the commit is a normal STOMP frame; the step list is a binary,
  `\0`-separated body (`CommitMessages.cc`, with `content-length`).
* zypp-rpm: `rpmReadConfigFiles`, `_dbpath` macro, `rpmtsSetRootDir`, VS flags from NODIGEST/NOSIGNATURE, trans flags
  from EXCLUDEDOCS/NOSCRIPTS/JUSTDB/TEST/NOPOSTTRANS, opens db `O_RDWR`, reads each header
  (`RPMRC_NOTTRUSTED/NOKEY/NOTFOUND` tolerated only with ALLOWUNTRUSTED; `RPMRC_FAIL` always fatal), problem filters:
  always `REPLACEPKG|OLDPACKAGE`; `IGNOREARCH`; `REPLACEFILES` → replace new/old files; `FORCE`; `IGNORESIZE` →
  disk space/nodes. `rpmtsCheck` only without NODEPS. Then `rpmtsOrder`, `rpmtsClean`, `rpmtsRun`.
* Source rpms (bsc#1228647): `rpmtsAddInstallElement` silently accepts them but `rpmtsRun` can't handle them, so they are
  remembered and installed afterwards with `rpmInstallSourcePackage`.
* Remove steps whose NEVRA isn't found are only logged (`"Unable to remove … not found"`).
* `ZYPP_RPM_DEBUG` raises the rpm log mask to DEBUG; every rpm log record is sent as `RpmLog`.
* Exit codes (`zypp/tools/zypp-rpm/errorcodes.h`): `NoError, FailedToOpenDb, WrongHeaderSize, WrongMessageFormat,
  RpmInitFailed, FailedToReadPackage, FailedToAddStepToTransaction, RpmFinishedWithTransactionError,
  RpmFinishedWithError, RpmOrderFailed, FailedToCreateLock, OtherError=255`. libzypp maps them to exceptions
  (`RpmTransactionFailedException` with `TransactionError::problems`, `RpmDbOpenException`, `RpmInitException`,
  `RpmSubprocessException`). `RpmFinishedWithError` is treated like success: the per-step stages tell what failed.

Message → report mapping in libzypp (`processMessages`):

| Message | Effect |
|---|---|
| `RpmLog` | `zypp-rpm` log group + `SingleTransReport` `contentLogline` |
| `PackageBegin` | finalize previous report, start `InstallResolvableReportSA` / `RemoveResolvableReportSA` |
| `PackageProgress` | progress on current report |
| `PackageFinished` | mark `STEP_DONE` only — report is finished at the *next* begin because librpm sends `PackageError` *after* finished |
| `PackageError` | `STEP_ERROR`, finalize |
| `ScriptBegin/ScriptFinished/ScriptError` | `CommitScriptReportSA`; `ScriptError.fatal` (librpm abuses `total != RPMRC_OK`) → `STEP_ERROR` of that step; ScriptError comes after ScriptFinished |
| `CleanupBegin/Progress/Finished` | `CleanupPackageReportSA` — erasure of old versions (no own transaction step) |
| `TransBegin/Progress/Finished` | `TransactionReportSA` ("Preparing" / "Verifying") |
| `TransactionError` | remembered, used for the exception text |

Script output arrives asynchronously on fd 4; `waitForScriptEnd()` blocks (100 ms polls) until the `0xDEADBEEF\n` tag
is seen before closing a report, so output is attributed to the right package (bsc#1197134 hang fix and "flush rpm script
output buffer before sending" are in the changelog). History gets `"<ident> installed ok"` / `"install failed"` comments
plus the buffered rpm output.

If the application doesn't listen to any `*SA`/`SingleTransReport`, `SingleTransReportLegacyWrapper` forwards install/remove
start/progress/finish to the classic `InstallResolvableReport`/`RemoveResolvableReport` (log `WAR "Activating
SingleTransReportLegacyWrapper!"`).

After the run: orphan products are handled, then update scripts/messages and patch status logging as in classic mode.
No `RpmPostTransCollector` is used (rpm runs `%posttrans` itself). Downloaded files of successful installs are released
from `locCache`; failed ones are kept in the package cache.

---

## 7. needreboot, update scripts and update messages

### needreboot

`TargetImpl::load()` builds a `sat::SolvableSpec` that always contains `provides:installhint(reboot-needed)` and
`provides:kernel`, plus entries parsed from `root/etc/zypp/needreboot` (`ZConfig::needrebootFile()`) and every file in
`root/etc/zypp/needreboot.d/` except `*.rpmnew|rpmsave|rpmorig`. Format: one `PACKAGENAME` or `provides:CAPABILITY` per
line (`zypp/needreboot`, installed empty to `/etc/zypp`, jsc#PM-2645). History: udev removed (bsc#1179083),
kernel-firmware no longer auto-flagged (bsc#1200993). `sat::Solvable::isNeedreboot()` → `PoolImpl::isNeedreboot`.

When such a package is successfully installed (not removed, not in test mode), `root/run/reboot-needed` is created/touched.
zypper checks this file; libzypp never removes it.

### Update scripts and messages (`RunUpdateScripts`, `RunUpdateMessages`)

Directories (`ZConfig::update_dataPath()` = `/var/adm`): `root/var/adm/update-scripts`, `root/var/adm/update-messages`.
For every **successfully installed** package, files named `name-version-release` or `name-version-release-*` are picked
up (the old "Code11 Scripts and Messages" mechanism):
* Scripts: made executable (`addmod 0500`), deduplicated by md5 (or symlink target for dangling symlinks — instsys case),
  executed chrooted, output to history and `PatchScriptReport` (ABORT/RETRY/IGNORE). On abort remaining scripts are only
  logged.
* Messages: non-empty files are added to `ZYppCommitResult::updateMessages()` (paths relative to root) and history
  (`"New update message"`), then `sendNotification()` runs `ZConfig::updateMessagesNotify()`
  (zypp.conf `update.messages.notify`, format `"<single|digest|bulk|none> | command"`, `%p` = solvable, `%P` = file;
  executed chrooted). Messages run after scripts so scripts may fill `%ghost` message files.

---

## 8. HistoryLog (`/var/log/zypp/history`)

`zypp/zypp/HistoryLog.{h,cc}`, parser `HistoryLogData.{h,cc}`, logrotate `zypp/zypp-history.lr`
(installed to `/etc/logrotate.d`: `size 10M, rotate 99, maxage 1827, compress, dateext`).

* File: `ZConfig::historyLogFile()` (zypp.conf `history.logfile`, default `/var/log/zypp/history`), prefixed with the
  target root by `HistoryLog::setRoot` (called by the `TargetImpl` ctor), except `/dev/null`.
* Global, refcounted `std::ofstream` (`HistoryLog` objects are cheap; the file stays open while any instance exists).
* Separator `|`, date format `HISTORY_LOG_DATE_FORMAT "%Y-%m-%d %H:%M:%S"`. Action IDs are padded to 7 chars
  (`"remove "`, `"radd   "`).

| Action | Fields |
|---|---|
| `command` | date, `command`, user@host, quoted cmdline (from `/proc/self/cmdline`, `|` escaped), userdata |
| `install` | date, `install`, name, evr, arch, requested-by, repo alias, checksum, userdata |
| `remove ` | date, `remove `, name, evr, arch, requested-by, userdata |
| `radd   ` / `rremove` / `ralias ` / `rurl   ` | repo add/remove/rename/url change (written by RepoManager) |
| `patch  ` | name, evr, arch, repo, severity, category, old state, new state, userdata (jsc#SLE-5116) |
| `# …` | comments: rpm output, script output, errors (multi-line comments get `# ` on every line) |

"requested-by": `user@host` if status is by USER or by APPL_LOW (solver acting on behalf of the user); `pid:exe` if
APPL_HIGH; empty if solver-selected. **This field is semantic**: `getUserInstalledFromHistory()` in `TargetImpl.cc`
bootstraps `AutoInstalled` by treating every installed package whose last install entry contains an `@` in that column as
user-installed. Do not change field positions or the padded `"install|"`/`"remove |"` tokens (the parser does
`strncmp(sep1, "remove |", 8)`).

`userdata` comes from `ZConfig::userData()` (set by `zypper --userdata`), also sent to plugins.

---

## 9. Plugins

### Protocol (`PluginFrame`, `zypp-logic/zypp-core/rpc/PluginFrame.cc`)

STOMP-like: `COMMAND\nkey:value\n…\n\nbody\0`. libzypp sends `content-length`; when reading, `content-length` (if present)
is honoured, else the body is read up to `\0`. Special commands: `ACK`, `ERROR`, `_ENOMETHOD`, `_DISCONNECT`.

### `PluginScript` (log group `zypp::plugin`)

* Script must be a regular executable file; launched via `ExternalProgramWithStderr` (optionally chrooted). stderr lines
  are logged as `L_WAR("PLUGIN") "! "`.
* Non-blocking pipes with `g_poll` timeouts: send/receive default 30 s, overridable by `ZYPP_PLUGIN_TIMEOUT`,
  `ZYPP_PLUGIN_SEND_TIMEOUT`, `ZYPP_PLUGIN_RECEIVE_TIMEOUT` (read at static init) or
  `PluginScript::defaultSendTimeout()/…`. Exceptions: `PluginScriptSendTimeout`, `PluginScriptReceiveTimeout`,
  `PluginScriptDiedUnexpectedly`, `PluginScriptNotConnected`.
* `close()` sends `_DISCONNECT`; if answered with `ACK` the script is not killed and an `exit:` header sets the return
  value; otherwise it's killed.
* `ZYPP_PLUGIN_DEBUG=1` dumps all frames to the `PLUGIN` log group.

### `PluginExecutor`

`load(dir)` launches every executable file (`userMayRX`), sends `PLUGINBEGIN` (+ `userdata` header). Every `send()` expects
`ACK` or `_ENOMETHOD`; any other answer (including `ERROR` or a timeout) **closes and drops that plugin** — plugins cannot
veto anything. `/bin/cat` is accepted as reflector for tests. Destructor sends `PLUGINEND`.

Note: the doxygen pages (`zypp/doc/autoinclude/Plugin-Commit.doc`, `Plugin-System.doc`) say "send an `ACC` message"; the
code checks for `ACK`. Trust the code.

### Plugin types (default base `ZConfig::pluginsPath()` = `/usr/lib/zypp/plugins`)

| Dir | When | Messages |
|---|---|---|
| `commit/` | `TargetImpl::commit`, only root == "/" or `$TRANSACTIONAL_UPDATE`, not dry-run, and only if the commit actually starts (after successful downloads) | `PLUGINBEGIN`, `COMMITBEGIN` / `COMMITEND` with JSON `{"TransactionStepList":[{"type":"+|-|M","stage":"ok|err","solvable":{"n","e"?,"v","r","a"}}]}` (IGNORE steps omitted), `PLUGINEND` |
| `system/` | after an `@System` solv rebuild, root == "/" only | `PACKAGESETCHANGED` |
| `urlresolver/` | `UrlResolverPlugin::resolveUrl` for `plugin:NAME?k=v` URLs | `RESOLVEURL` (query params as headers) → `RESOLVEDURL` (body = real URL, headers become HTTP headers; empty values replaced by a curl workaround header) or `ERROR` → `MediaException`. Missing plugin file → URL returned unchanged. |
| `services/` | service plugins (stateless, see [Repositories](05-repositories-and-services.md)) | |
| `appdata/` | stateless, after repo changes (RepoManager) | |

The snapper pre/post snapshot integration is **not in this repo**: it is a commit plugin shipped by snapper
(`snapper-zypp-plugin`) using exactly the `commit/` interface above.

---

## 10. ZYppCommitResult

`zypp/zypp/ZYppCommitResult.{h,cc}`:
* `transaction()` – full ordered `sat::Transaction` (incl. IGNORE steps); `transactionStepList()` – what commit tried.
  Step stages `STEP_TODO/DONE/ERROR`; deleted solvables keep post-mortem `ident/edition/arch`.
* `allDone()` (no step `!= DONE`), `noError()` (no `STEP_ERROR`; skipped TODO is ok).
* `attemptToModify()` – set (via `NotifyAttemptToModify`) right before the first rpm call. False ⇒ failure happened during
  download/preparation and the system is untouched. zypper uses this for exit codes.
* `singleTransactionMode()`.
* `updateMessages()` – `UpdateNotificationFile{solvable, file}` relative to `root()`.
* A commit that fails in download returns normally with errors in the steps; user aborts throw
  `TargetAbortedException`; singletrans protocol/rpm failures throw `RpmException` subclasses.

---

## 11. Misc

### CheckAccessDeleted (`zypper ps`)

`zypp/zypp/misc/CheckAccessDeleted.cc` runs `lsof -n -FpcuLRftkn0 -K i` (without `-K i` for lsof < 4.90 unless it
provides `backported-option-Ki`, bsc#1099847/bsc#1036304) and keeps processes with deleted files: link count 0 (or none),
type `REG`/`DEL`, fd `mem`/`txt`/`DEL`/`ltx`; stat errors ignored (permission false positives); unless verbose only paths
containing `/lib` or `bin/`; memory maps under `/SYSV /var/ /dev/ /tmp/ /proc/ /memfd: /snap/` ignored (snaps
bsc#1226014; containers bsc#1218291 via `/proc/<pid>/map_files` checks). `findService(pid)` derives the systemd unit from
`/proc/<pid>/cgroup`. `check(lsofOutputFile)` allows replaying a captured lsof output for debugging.

### Modalias

`zypp/zypp/target/modalias/Modalias.cc` collects `modalias` files under `/sys` (skipping `/sys/devices/system/memory`,
bnc#824110) and matches `modalias(...)` capabilities with `fnmatch`. `ZYPP_MODALIAS_SYSFS` may point to a directory or a
file with concatenated modaliases (handy for testcases). See `zypp/doc/autoinclude/Modalias.doc` and the full chapter [03b – Modalias](03b-modalias.md).

### Application

`zypp/zypp/Application.{h,cc}` is a `ResObject` kind for appdata (`application:` solvables); it has no attributes.
rpmdb2solv no longer generates application pseudo-packages for `@System` (bsc#1104415).

### root != "/"

Summary of chroot-specific behaviour:
* rpm called with `--root`; scripts (posttrans, update scripts, notifications, plugins via `PluginScript` root arg) chrooted.
* `AssertProcMounted` / `AssertDevMounted` mount `proc` and `--rbind --make-rslave /dev` into the root if missing
  (bsc#1181328, issue #444) for the duration of the commit, then `umount -R -l`.
* `SYSTEMD_OFFLINE=1`; no commit/system plugins (unless `TRANSACTIONAL_UPDATE`); no anonymous id creation;
  `DownloadAsNeeded` default in classic mode; lock file still on the host unless `ZYPP_LOCKFILE_ROOT`.
* `rpm --runposttrans` dump mode disabled only with broken rpm-4.18.0 on the host.

---

## 12. Disk usage estimation (`DiskUsageCounter`)

Files: `zypp/zypp/DiskUsageCounter.{h,cc}`, used by `ZYpp::diskUsage()/setPartitions()/getPartitions()`
(`zypp/zypp/zypp_detail/ZYppImpl.cc`). Debug tool: `zypp/tools/DiskUsageCounter.cc` (`[--root DIR]`, prints the result of
`detectMountPoints()`). Tests: `zypp/tests/repo/DUdata_test.cc` (growonly vs normal mount points),
`zypp/tests/sat/Solvable_test.cc`.

### Data model

`DiskUsageCounter::MountPoint` (sorted by `dir`; the set is a `std::set<MountPoint>`):

| Field | Unit | Meaning |
|---|---|---|
| `dir` | – | mount point **relative to the target root** (`/`, `/usr`, …) |
| `fstype` | – | from `/proc/mounts`; `btrfs` changes the waste computation |
| `block_size` | B | `statvfs.f_bsize` |
| `total_size` | KiB | `f_blocks * f_bsize / 1024` |
| `used_size` | KiB | `(f_blocks - f_bfree) * f_bsize / 1024` (**`f_bfree`, not `f_bavail`**: root-reserved blocks count as free) |
| `pkg_size` | KiB | *mutable*, the computed "used after commit" value |
| `readonly`, `growonly` | bit | hints |

Convenience: `freeSize()`, `usedAfterCommit()`, `freeAfterCommit()`, `commitDiff()` (= `pkg_size - used_size`).

### `detectMountPoints(rootdir = "/")`

Parses `/proc/mounts` and drops:
* devices without a `/` in field 0 (proc, tmpfs, sysfs …) and `/proc` (bnc#418783);
* mount points not at or below `rootdir`; the prefix is stripped so `dir` is root-relative;
* `iso9660`, `vfat`, `fat`, `ntfs`, `ntfs-3g`;
* `/mnt /media /mounts /floppy /cdrom /suse /tmp /var/tmp /var/adm/mount /var/adm/YaST` and anything below them;
* **read-only mounts** (`ro` option, #297405). This means `Hint_readonly` is never present in a detected set. Only an
  application that builds its own set can set it, and `calcDiskUsage` ignores it anyway;
* zero-sized file systems (`f_blocks == 0 || f_bsize == 0`, bnc#769819).

btrfs handling:
* **growonly**: as root, `btrfs subvolume list -s <mp>` runs once per btrfs mount. If it prints at least one line
  (i.e. at least one snapshot exists) the mount gets `Hint_growonly`. A non-root user can't check this, so growonly is
  assumed. If `btrfs` isn't installed, the program prints nothing and the mount is treated as normal.
* **Subvolume filter** (bnc#949945, by device not fsid): only one entry per btrfs *device* (field 0) is kept, using
  the lexicographically smallest mount dir. Nested subvolumes would otherwise break the per-package computation
  ("FIX: Computation must learn to handle multiple mount points contributing to the same file system").
* If `statvfs` fails the mount point is still inserted, with all sizes 0.

`justRootPartition()` = `{ "/" }` with all sizes 0 (handy for "total installed size" tests).

### Computation (`calcDiskUsage` → libsolv `pool_calc_duchanges`)

```
disk_usage(ResPool):  bitmap = { pi | pi.status().isInstalled() != pi.status().transacts() }   // the state after commit
disk_usage(Solvable|Bitmap|range):  same, but pool->installed is temporarily set to NULL (DtorReset)
   → pool_calc_duchanges(pool, bitmap, DUChanges[]{path, flags = growonly ? DUCHANGES_ONLYADD : 0})
pkg_size = used_size + du.kbytes + du.files * (fstype=="btrfs" ? 4096 : block_size) / 2048
```

* Source data: the per-directory `SOLVABLE_DISKUSAGE` attribute (dir → kbytes, inodes):
  * `@System`: `rpmdb2solv`/`repo_rpmdb.c` computes it from the header file list (regular files only for kbytes, all
    entries for the inode count).
  * susetags `=Dir:` lines (`repo_susetags.c`), rpm-md `susedata.xml` `<diskusage><dirs><dir …/>` (`repo_rpmmd.c`),
    and plaindir repos (from the rpm headers).
* libsolv maps each directory to the deepest configured mount point (`create_mptree`). New packages are added and
  installed packages that are not in the bitmap are subtracted. On a `DUCHANGES_ONLYADD` (growonly) mount nothing is
  ever subtracted, because the deleted blocks stay referenced by the snapshot.
* **New packages without DU data** contribute nothing, and libsolv then also **ignores** the installed packages they
  obsolete (it doesn't subtract them). On growonly mounts the old package is added and subtracted again (net zero).
  `Solvable::installSize()` (`SOLVABLE_INSTALLSIZE`) is **not** used as a fallback.
* Waste estimate: half a block per file. It is capped at 4 KiB blocks for btrfs because btrfs reports up to 64 KiB
  `f_bsize` (bsc#974275, bsc#965322).
* `disk_usage(Solvable)` (bnc#852943) unsets `@System`, so the result is "used_size + this package" and `commitDiff()`
  is the package's own footprint.

### Who uses it and what is checked

* `ZYppImpl::diskUsage()` lazily calls `setPartitions(detectMountPoints())`. It uses the **default rootdir "/"**,
  not the target root. A `--root` install computes against the host's mount table, unless the application calls
  `setPartitions(detectMountPoints(root))` itself. `getPartitions()` re-detects every time if nothing was set.
* **zypper doesn't use `DiskUsageCounter`.** Its summary line ("will use / free …") is
  `Σ installSize(new) - Σ installSize(removed)` (`zypper/src/Summary.cc`, also the `space-usage-diff` XML attribute).
  YaST's package selector shows the per-partition table via `ZYpp::diskUsage()` (YaST side not verified here).
* **libzypp never checks free space before commit.** The only real check is done by rpm. In classic mode each
  `rpm -U` checks disk space unless `RPMINST_IGNORESIZE` (`--ignoresize`) is set. In single-transaction mode
  `rpmtsRun` checks the whole transaction unless `IGNORESIZE` adds `RPMPROB_FILTER_DISKSPACE|DISKNODES`. Nothing in
  zypp.conf sets `IGNORESIZE`; only the API (`rpmInstFlags`) can.
* Accuracy caveats: `/proc/mounts` octal escapes (`\040`) are not decoded, so such mounts end up with 0 sizes; bind
  mounts and overlayfs are treated like normal file systems; compression and dedup (btrfs/zfs) are ignored; the
  `f_bfree` vs `f_bavail` difference overstates free space by the reserved blocks.

---

## 13. Products on the target: `/etc/products.d`, baseproduct, `$releasever`

### Two views of "the base product"

| API | Source | Notes |
|---|---|---|
| `TargetImpl::baseProduct()` | **pool**: first `Product` with `isTargetDistribution()` = `isSystem() && productType == "base"` | `productType` "base" is set by libsolv `repo_products.c` when the product file's **inode** equals the one `stat(products.d/baseproduct)` resolves to (symlink followed). Needs a loaded `@System`. |
| `baseproductdata(root)` (anon. ns. in `TargetImpl.cc`) | **file**: `parser::ProductFileReader::scanFile(root/etc/products.d/baseproduct)` | Works without a pool. Parse errors are only `ZYPP_CAUGHT`. A missing/dangling symlink logs `ERR` only if `products.d` exists. |
| `cachedBaseproductdata(root)` | same, cached per root in a function-static `map<Pathname, {WatchFile, data}>` | Used by `distributionVersion(root)` only. |

`ProductFileReader` (`zypp/zypp/parser/ProductFileReader.cc`) parses `<product>` with mandatory `name, version,
release, arch` and optional `vendor, shortsummary, summary[lang], productline, register{target,release,flavor},
updaterepokey, upgrades{upgrade{name,summary,repository,product,notify,status}}`.

### Target queries (all have a static `(root)` overload; empty root → `staticGuessRoot` = `ZConfig::systemRoot()` or `/`)

| Method | Value | Consumers |
|---|---|---|
| `targetDistribution()` | `<register><target>` e.g. `sle-15-x86_64` | **service repo filtering**: `RepoCollector` in `zypp/zypp/ng/repo/workflows/serviceswf.cc` drops service repos whose `RepoInfo::targetDistribution()` doesn't match (unless `RepoManagerOptions::servicesTargetDistro` is set); also the HTTP **User-Agent** (`MediaNetworkCommonHandler::agentString()`: `"ZYpp <ver> (curl <ver>) <targetDistribution>"`). |
| `targetDistributionRelease()` | `<register><release>` | |
| `targetDistributionFlavor()` | `<register><flavor>` | |
| `distributionLabel()` | `{shortsummary, summary}` | |
| `distributionVersion()` | baseproduct `<version>` (`edition().version()`), **fallback** `tag_version()` of the first rpmdb package providing `ZConfig::distroverpkg()` | `$releasever` (below). The member version caches in `_distributionVersion` and refreshes when `_baseproductWatcher` reports a change. |
| `distributionFlavor()` | first non-empty line of `root/var/lib/zypp/LastDistributionFlavor` | `X-ZYpp-DistributionFlavor` header to d.o.o/cdn.o.o (media backends and the commit preloader). |

`createLastDistributionFlavorCache()` (each `load()`) writes `baseProduct()->flavor()` from the **pool** product into
`LastDistributionFlavor`. With no base product it only logs `WAR "No base product, I won't create flavor cache"` and
leaves the old file in place. Don't mix up `distributionFlavor()` (pool product's flavor, cached on disk) and
`targetDistributionFlavor()` (`<register><flavor>` from the file).

`ZConfig::distroverpkg()` is **hard-coded to `"system-release"`** (`zypp/zypp/ZConfig.cc`). The NG `zyppconfig.h` key
`main/distroverpkg` has an empty conf name, i.e. it can't be configured either. The fallback is meant for RHEL/Fedora-like
systems without `/etc/products.d`.

### `$releasever` (`zypp/zypp/repo/RepoVariables.cc`, `checkOverride`)

For `releasever`, `releasever_major` and `releasever_minor` (bnc#943563 "Don't cache repo releasever"):
1. `$ZYPP_REPO_RELEASEVER` set → always wins (it overwrites any user value, and major/minor are derived by splitting at
   the first `.`).
2. Otherwise, if the user defined no such variable (`/etc/zypp/vars.d/releasever`) →
   `Target::distributionVersion(contextRoot)` is evaluated **on every lookup** and stored as `$_releasever`
   (`_major`/`_minor` derived).
3. Otherwise → the user value.

So `$releasever` follows the baseproduct symlink live. After a dup that changes the baseproduct, the next refresh
expands repo URLs with the new version. `zypper --releasever X` works by
`setenv("ZYPP_REPO_RELEASEVER", X, 1)` (`zypper/src/Config.cc`).

### Products in the commit

* rpmdb2solv is run with `-p root/etc/products.d`, so product solvables in `@System` come from the files, not from the
  rpmdb. The `@System` cookie includes `products.d` (§3).
* A product's buddy is its release package (`Product::referenceFilename()` / `referencePackage()`). For installed
  products whose release package can't be found by provides, the package owning `/etc/products.d/<refFile>` is used
  (bnc#784900).
* Orphan products (no buddy) are removed by deleting the `.prod` file if no package owns it (§6). Installing a product
  without its release package is not possible.

---

## 14. Classic-mode rpm callbacks (`TargetCallbackReceiver`, `RpmCallbacks.h`)

`zypp/zypp/target/rpm/RpmCallbacks.h` defines two **internal** reports that `RpmDb` sends:

| Report | Methods | Default answers |
|---|---|---|
| `rpm::RpmInstallReport` | `start(Pathname)`, `progress(unsigned)`, `finishInfo(str)`, `finish()`, `problem(Exception&) → ABORT/RETRY/IGNORE`, `finish(Exception&)` | `progress` → `true`, `problem` → `ABORT` |
| `rpm::RpmRemoveReport` | same with `start(name)` | `progress` → `false`, `problem` → `ABORT` |

Note the doc comments disagree ("Return false on abort" for install, "Return true on abort" for remove). `RpmDb`
**ignores the return value of `progress()` anyway**, because rpm can't be stopped in the middle of a package.

`zypp/zypp/target/TargetCallbackReceiver.{h,cc}` adapts them to the public per-resolvable reports:

```
TargetImpl::commit (classic)                     RpmDb                                application
  RpmInstallPackageReceiver r(res); r.connect()
  r.tryLevel(RPM_NODEPS_FORCE)
  rpm().installPackage(file,…) ──► SendReport<RpmInstallReport>
                                     start(file)        ──► r.start ──► InstallResolvableReport::start(res)
                                     "%%NN" lines       ──► r.progress(NN) ──► …::progress(NN,res); r._abort = !ret
                                     other lines        ──► report(UserData contentRpmout{line,lineno}) (+"solvable")
                                     RpmException       ──► r.problem ──► …::problem(res, INVALID, asUserHistory(), level)
                                     finishInfo(rpm output) / finish()  ──► …::finish(res, NO_ERROR, info, level)
  if r.aborted() → STEP_ERROR, abort = true (checked only after rpm returned)
```

* Progress comes from `rpm --percent` (`%% 12.3456` → `sscanf %d`). Removal has no real progress: `doRemovePackage`
  sends fixed values (5, 50, …) and `RpmRemovePackageReceiver::finish()` sends `progress(100)` before `finish`.
* `RpmLevel` (`RPM`, `RPM_NODEPS`, `RPM_NODEPS_FORCE`) is a legacy of YaST's old "retry with --nodeps/--force"
  escalation. Classic commit always uses `RPM_NODEPS_FORCE`, because `--nodeps --force` is always passed.
* `problem()` loop in `RpmDb::installPackage/removePackage`: `RETRY` reruns rpm; `ABORT` sends `finish(excpt)` and
  rethrows; **`IGNORE` breaks out and returns normally**. `TargetImpl::commit` then writes a history `install` record
  and marks the step `STEP_DONE`, even though rpm failed (see Pitfalls).
* `RpmInstallPackageReceiver` is also used by `installSrcPackage()`.
* Single-transaction mode uses none of this. It uses the `*ReportSA` reports or the `SingleTransReportLegacyWrapper` (§6).

---

## 15. Package provisioning during commit in depth

### Flow and responsibilities

```
TargetImpl::commit
  CommitPackageCache packageCache;  setCommitList(steps)            // vector<sat::Solvable>
  ├─ DownloadAsNeeded: nothing now; classic commit() calls packageCache.get(pi) right before each rpm -U
  └─ otherwise:
       CommitPackagePreloader::preloadTransaction(steps)          // optional parallel HTTP into <packagesPath>/.preload
       if (!preloader.missed())
          for each INSTALL/MULTIINSTALL Package|SrcPackage step:
              packageCache.get(pi).resetDispose()                  // → RepoProvidePackage → PackageProvider
          packageCache.preloaded(true)
       else  → "Some packages could not be provided. Aborting commit."   (no sequential fallback!)
```

* `CommitPackageCache::Impl` (`CommitPackageCacheImpl.h`) = no caching. `sourceProvidePackage()` throws
  `Exception("Package provider failed.")` for an empty result. `sourceProvideCachedPackage()` asks the provider with
  `fromCache = true` (`PackageProvider::providePackageFromCache()`, which doesn't download).
* `CommitPackageCacheReadAhead` (default unless `$ZYPP_COMMIT_NO_PACKAGE_CACHE`):
  * `get()` returns the cached file directly once `preloaded()` is set, which avoids a second `infoInCache` callback.
  * `onInteractiveMedia()`: `mediaNr() != 0` and the first baseUrl scheme is `cd`/`dvd` (mirrorlist-only repos are
    never interactive). When the needed medium (`IMediaKey{repo, mediaNr}`) changes, `doCacheLastInteractive()` first
    copies **all remaining to-be-installed packages of the previous medium** that come later in the commit list into
    the cache (`resetDispose()`). Errors there are only logged (`"Failed to cache …"`).
* `RepoProvidePackage` builds `repo::DeltaCandidates` from all known repos for every `Package` (deltarpm) and uses
  `QueryInstalledEditionHelper` against the host `/` rpmdb (`applydeltarpm` must rebuild from installed files).
* In the "download in advance" loop the `ManagedFile` is `resetDispose()`d, so files stay in the cache. In classic
  `commit()` the second `get()` returns a `ManagedFile` whose disposal (delete unless `keeppackages`) is decided by
  `PackageProvider` (see [Media & downloads](07-media-and-downloads.md)). On rpm failure or abort `localfile.resetDispose()`
  keeps the file. In singletrans, successful installs are released from `locCache` after the run and failed ones kept.
* Abort/skip exceptions: in the advance loop `AbortRequestException` → `TargetAbortedException` (thrown, plugins not
  started yet). `SkipRequestException` and other `Exception`s → `STEP_ERROR` + `miss`.

### `CommitPackagePreloader` (`zypp/zypp/target/commitpackagepreloader.cc`, `private/commitpackagepreloader_p.h`)

Introduced as a tech preview (openSUSE/zypper#104; changelog "Add a transaction package preloader"). Progress throttling:
bsc#1239543. Since then: appending `RepoInfo::path()` to mirror URLs, and the fix for arch-specific subrepos.

Preconditions (otherwise it returns silently and the sequential path does everything):
`preloadEnabled()` (`$ZYPP_PCK_PRELOAD` tri-bool, default from `LIBZYPP_CONFIG_USE_SERIAL_PACKAGE_DOWNLOAD_BY_DEFAULT`)
**and** `_report->connected()` (`media::CommitPreloadReport`; zypper has `CommitPreloadReportReceiver` in
`zypper/src/callbacks/media.h`).

Job collection, per INSTALL/MULTIINSTALL step of a `Package`/`SrcPackage`:
* skipped if `lookupLocation().checksum()` is empty, or `cachedLocation()` is non-empty (already in the cache);
* per repo (once): `assert_dir(predownloadPath())` (= `packagesPath()/.preload`; failure → **return**, nothing more
  is preloaded); mirror set = every `repoOrigins()` endpoint → `UrlResolverPlugin::resolveUrl` (plugin: URLs + headers)
  → kept only if `MediaHandlerFactory::handlerType == MediaCURLType` → GeoIP rewrite (`findGeoIPRedirect`) →
  `+ RepoInfo::path()`;
* repos without a curl URL are skipped (their packages use the sequential path);
* jobs are sorted by repository; `_requiredBytes += downloadSize()`.

Execution:
* A private `zyppng::EventLoop` + `NetworkRequestDispatcher` with `download.max_concurrent_connections` (MediaConfig,
  default **5**), the libzypp agent string, and the d.o.o/cdn.o.o flavor + anonymous-id headers.
* **One `PreloadWorker` per connection.** Each worker takes the next job and keeps its mirror while the repo stays the
  same. Mirror choice: lowest `refs + miss` (load + history of misses).
* Target: `predownloadPath()/mapToCachePath(repoInfo, loc)`. An existing file with a matching checksum → "already in
  cache". Anything else at that path is removed. Downloads go to a `TmpFile` sibling, then chmod (umask applied, 0644)
  and rename.
* Errors: most `NetworkRequestError`s (incl. NotFound, Timeout, ServerReturnedError, Forbidden, ConnectionFailed, HTTP/2
  errors) **taint** the mirror for this file (`miss++`), report a non-fatal `fileDone(ERROR, "trying next mirror")` and
  retry on the next untainted mirror. When none is left → `fileDone(NOT_FOUND)`, fatal. `Unauthorized/AuthFailed` →
  `MediaNetworkCommonHandler::authenticate` with the CredentialManager (re-enqueue on new creds, else `ACCESS_DENIED`,
  fatal). `Cancelled` → fatal.
* Any fatal file error sets `_missedDownloads`. User cancel (`progress()` returns false, called at most every
  **500 ms**; the comment says "one time per second") clears the queue, `cancelAll()`s and sets `_missedDownloads`.
* `report->finish(MISS|SUCCESS)`; mirror miss statistics are logged at `MIL`.

How the files are consumed: `RepoMediaAccess::provideFile` (`zypp/zypp/repo/RepoProvideFile.cc`) adds `packagesPath()`
and `packagesPath()/.preload` (with `Fetcher::CleanFiles`) as Fetcher cache paths. `Fetcher::Impl::locateInCache` accepts
a file only if its **checksum matches** and gives the `.preload` copy an `unlink` disposer. So the normal
`PackageProvider` path (signature check, `RpmPkgSigCheck`, deltas) still runs on preloaded files.

Cleanup: `preloader->cleanupCaches()` (`clean_dir` of each used repo's `.preload`) is only called after a real
(non-dry-run) commit. After a preload miss, an abort, an exception, or `DownloadOnly`, leftover files stay in `.preload`.
They are harmless (checksum-validated and unlinked when consumed by a later run), but they use disk space.

**No fallback after a fatal preload miss:** `miss = preloader->missed()` skips the sequential loop, so the commit
aborts even if a sequential download with media-backend failover would have worked. The steps are **not** marked
`STEP_ERROR`; they all stay `STEP_TODO`. See §16 for how zypper evaluates this.

---

## 16. Commit result evaluation and rpm problem reporting

### StepType / StepStage (`zypp/zypp/sat/Transaction.h`)

| StepType | Symbol | Meaning |
|---|---|---|
| `TRANSACTION_IGNORE 0x00` | `[ ]` | nothing to do: implicit erase via obsoletes, non-package items. Not visited by `actionBegin()` |
| `TRANSACTION_ERASE 0x10` | `[-]` | |
| `TRANSACTION_INSTALL 0x20` | `[+]` | install/update |
| `TRANSACTION_MULTIINSTALL 0x30` | `[M]` | multiversion install |

`StepStage`: `STEP_TODO 1`, `STEP_DONE 2`, `STEP_ERROR 4` (a bitmask for `actionBegin(stageMask)`). The stage is stored in the
`sat::Transaction` itself, so `transactionStepList()` (copies of `Step`) and `transaction()` see the same stages.

Who sets what:

| Situation | Stages |
|---|---|
| download skip/error (advance loop or classic as-needed) | that step `STEP_ERROR` |
| fatal preload miss / preload cancel | **all `STEP_TODO`**, `attemptToModify() == false` |
| classic install failure | `STEP_ERROR` and **break** (rest TODO) |
| classic remove failure | `STEP_ERROR`, continue |
| classic `IGNORE` answer to rpm problem | `STEP_DONE` (!) |
| classic package IGNORE step (obsoleted) | `STEP_DONE` |
| singletrans | `PackageFinished` → DONE; `PackageError` / fatal `ScriptError` → ERROR; steps rpm never reported stay TODO |
| `restrictToMedia(1)` | steps not in `transactionStepList()` stay TODO in `transaction()` |

`ZYppCommitResult::allDone()` = `transaction().actionEmpty(~STEP_DONE)`, `noError()` = `actionEmpty(STEP_ERROR)`. Both look
at the **whole transaction**, not only the step list.

### zypper (`zypper/src/solve-commit.cc`)

```
dryRunEtc = policy.dryRun() || downloadMode == DownloadOnly
if ( !allDone() && !(dryRunEtc && noError()) )
     exit = attemptToModify() ? ZYPPER_EXIT_ERR_COMMIT (8) : ZYPPER_EXIT_ERR_ZYPP (4)
show_update_messages(updateMessages())
exception: MediaException / RepoException (hint "zypper refresh" if metadata outdated) / FileCheckException / Exception → ZYPPER_EXIT_ERR_COMMIT
on error: singleTransactionMode() ? CommitSummary (per-step table from transactionStepList, XML capable) : showBasicErrorMessage
```

Consequence: with `--download-only`, a fatal preload miss leaves all steps TODO and `noError()` true, so the exit code
stays 0 unless the `CommitPreloadReportReceiver` sets one itself (zypper side not checked).

### rpm problems (file conflicts, disk space, dependencies)

* **Classic:** libzypp runs its own file-conflict check (`commitFindFileConflicts`, §6) before the first `rpm -U`.
  Every later rpm problem (disk space, unexpected conflicts despite `--force`, scriptlet failures with non-zero exit) comes
  back as `RpmSubprocessException` with the rpm output, goes through the `problem()` loop, and is shown as
  `InstallResolvableReport::problem`.
* **Singletrans:** there is no libzypp file-conflict check (except for explicit dry-run). `zypp-rpm` lets rpm detect
  conflicts and disk space (`RPMPROB_FILTER_*` only for `REPLACEFILES`, `FORCE`, `IGNORESIZE`, `IGNOREARCH`, always
  `REPLACEPKG|OLDPACKAGE`). `rpmtsRun() > 0` → `pushTransactionErrorMessage(rpmtsProblems)` (one `rpmProblemString`
  per problem) → exit `RpmFinishedWithTransactionError` → libzypp throws `RpmTransactionFailedException("Executing the
  transaction failed because of the following problems: …")`. `rpmtsRun() < 0` → `RpmFinishedWithError`, treated like
  success, so the per-step stages tell the story. Dependency problems (`rpmtsCheck`) only occur without `NODEPS`, which
  libzypp always sets.
* `FindFileConflictstReport` (`zypp/zypp/ZYppCallbacks.h`): `start(progress)`, `progress(progress, noFilelist)`,
  `result(progress, noFilelist, conflicts)`; returning `false` from any of them → `AbortRequestException` →
  `TargetAbortedException`. `noFilelist` = packages without a cached rpm. With `DownloadAsNeeded` (classic default for
  chroot targets) **no** new package is cached at that time, so the check covers nothing and only reports them all as
  unchecked.
* The file-conflict check runs **after** `COMMITBEGIN` was sent and after `AutoInstalled`/locks/locales were stored.
  If the user aborts there, `COMMITEND` is never sent and `buildCache()` doesn't run (see Pitfalls).

### zypp.conf keys that affect commit

Only `commit.downloadMode` (`DownloadOnly|DownloadInAdvance|DownloadInHeaps|DownloadAsNeeded`, `ZConfig.cc`
`deserializeDownloadMode`) and `rpm.install.excludedocs` (sets `RPMINST_EXCLUDEDOCS` in `ZConfig::rpmInstallFlags()`)
exist. There is **no** `commit.conflictcheck`, `commit.imageMode` or similar key in this tree. File-conflict checking
can't be disabled from config. zypper offers `--replacefiles`, which maps to `ZYppCommitPolicy::replaceFiles`.

---

## 17. Persistent state files at commit in depth

| File | Class (`zypp/zypp/target/`) | Read | Written | Format |
|---|---|---|---|---|
| `root/etc/zypp/locks` (`ZConfig::locksFile()`) | `HardLocksFile` (`Data = PoolTraits::HardLockQueries`, a list of `PoolQuery`) | lazily on first `data()` (in `load()`) via `readPoolQueriesFromFile` | `TargetImpl::commit` (non-dry-run, `apply_locks_file()`), **before** rpm runs | PoolQuery serialisation (blank-line separated records) |
| `root/var/lib/zypp/AutoInstalled` | `SolvIdentFile` (`unordered_set<IdString>`) | `load()` | commit (`Transaction::autoInstalled()`), `updateAutoInstalled()` | `# AutoInstalled generated <date>` + sorted idents, `#` comments allowed |
| `root/var/lib/zypp/RequestedLocales` | `RequestedLocalesFile` (`LocaleSet`) | `load()` (applied only if non-empty) | commit | `# zypp::RequestedLocales generated <date>` + one locale per line |

Common: `store()` writes a `TmpFile::makeSibling` (so the directory must exist; commit does `assert_dir(home())`, but not
for `/etc/zypp`), `chmod 0644`, then `rename`. `setData/setLocales` only writes if the new data `differs()` from the cached
data. **None of them reports write errors upward.** `HardLocksFile::store` doesn't even check the stream (`if ( true )`).

### Hard locks round trip

```
load():  HardLocksFile::data() → ResPool::setHardLockQueries(q)
           PoolImpl: evaluate all queries; setLock(USER) + UserLockQueryMatch bit on every match
                     (bsc#1225267: an item is locked if it *or its buddy* matches)
app:     pi.status().setLock(true/false, USER)   (YaST, zypper's solver, …)
commit:  ResPool::getHardLockQueries(newdata)
           diffLock(): UserLocked != UserLockQueryMatch → added (+1) / removed (-1) idents
           removed: drop the *trivial* query (name exact, kind, case-sensitive) for that ident
           added:   append a trivial query unless an equal one exists     ("Added locks win")
         HardLocksFile::setData(newdata) → rewrite file if it differs
```

* `zypper addlock/removelock/cleanlocks` don't go through this path. They edit the file directly with `zypp::Locks`
  (`Locks::instance().read/save()`, `zypper/src/commands/locks/*.cc`).
* bsc#1096803: `setData()` with empty data while the file was never loaded is a no-op. This protects against a commit
  without a loaded target wiping the locks file.
* Unlocking an item that was locked by a **non-trivial** query (wildcard, `provides`, repo-restricted) only tries to
  remove a trivial query. The original query stays in the file, so the lock comes back on the next `load()`.
* The rewrite loses comments and formatting of hand-edited lock files, but only when the lock set changed.

### AutoInstalled

* At commit the file is replaced by `Transaction::autoInstalled()`, the idents that are auto-installed **after** the
  transaction (computed by the resolver, see [Solver](04-solver.md)). This happens before rpm runs, so on a failed commit
  the file may describe a state that was never reached. The next `load()` + `setAutoInstalled` just carries unknown
  idents along.
* Bootstrap when missing: history-based (§8). It also unlinks the obsolete `SoftLocks`.

---

## 18. Shutdown inhibition, transactional-update and snapshots

### `ShutdownLock` (`zypp-logic/zypp-core/ShutdownLock{.cc,_p.h}`)

* `ShutdownLockCommit("zypp")` is created at the top of `TargetImpl::commit` (also for dry-run and download-only).
  `zypp-rpm` creates its own `ShutdownLockCommit("zypp-rpm")` (`zypp/tools/zypp-rpm/main.cc`), so the inhibitor stays
  even if the parent dies during the rpm transaction.
* Implementation: `ExternalProgramWithSeperatePgid` running
  `/usr/bin/systemd-inhibit --what=sleep:shutdown:idle --who=<who> --mode=block --why="Zypp commit running." /usr/bin/cat`
  with stderr discarded. The separate process group keeps a terminal SIGINT from killing the inhibitor. `cat` blocks on
  the pipe.
* All exceptions are swallowed. If `systemd-inhibit` is missing (containers, chroots without systemd) the child just
  fails and nothing is logged beyond `"Try to acquire an inhibitor lock..."`.
* Dtor: `SIGTERM`, wait up to 10 s, then `SIGKILL`.
* `--mode=block` can be overridden by root (`systemctl poweroff -i`). Tools can detect the lock with
  `systemd-inhibit --list` (who = zypp / zypp-rpm).

### Transactional update / read-only root

* `zypp::env::TRANSACTIONAL_UPDATE()` (top of `TargetImpl.cc`) reads `$TRANSACTIONAL_UPDATE` **once** (function-static)
  with `strToBool(…, true)`, so any value except a false-ish one enables it. Its **only** effect is to load commit plugins
  even if root != "/" (changelog: "Call zypp commit plugins during transactional update", fixes #506).
* libzypp has **no** read-only-root detection of its own. A read-only `/` just shows up as rpm or state-file write
  errors. The only `ro` handling is in `DiskUsageCounter::detectMountPoints` (ro mounts are dropped). transactional-update
  / tukit run zypper against a writable snapshot (how exactly is not verified here).
* Snapshots: snapper's pre/post snapshots come from the external `snapper-zypp-plugin` via the commit plugin interface
  (§9). The btrfs-related logic inside libzypp is limited to the `growonly` disk-usage hint (§12). Other `zypp-plugin-*`
  packages (e.g. `zypp-plugin-python`, a helper library for writing plugins) are also external.

### needreboot / purge-kernels interplay

* `/etc/zypp/needreboot` (shipped from `zypp/needreboot`, `%config(noreplace)`) and `/etc/zypp/needreboot.d/*` use the
  `sat::SolvableSpec` syntax: one `NAME` or `provides:CAP` per line, `#` comments and empty lines ignored
  (`iostr::simpleParseFile`). Source packages never match (`SolvableSpec::contains` excludes `srcpackage`). The spec is
  re-evaluated whenever the pool changes (`setDirty`).
* `zypper needs-rebooting` only tests `root/run/reboot-needed` (exit `ZYPPER_EXIT_INF_REBOOT_NEEDED`). zypper's summary
  hint (`Summary::showNeedRebootHInt`) uses `isNeedreboot()` of the to-be-installed items, and patch reboot flags
  override included packages (bsc#1183268, zypper side).
* Kernel purging: `zypper purge-kernels` → `PurgeKernels::markObsoleteKernels()` + a normal commit, see
  [Solver §13](04-solver.md). The boot-time `purge-kernels.service` (calls `zypper purge-kernels`) ships outside this repo
  (not verified). The commit itself treats a kernel removal like any other erase. Note that `provides:kernel` is always
  in the needreboot spec, but only **installs** touch `/run/reboot-needed`, removals don't.

---

## Pitfalls & special cases

* `dryRun` is stored as the `RPMINST_TEST` bit — setting `rpmInstFlags()` later can silently clear/enable dry-run.
* `DownloadOnly` implies dry-run; `DownloadAsNeeded` silently forces classic_rpmtrans (agreed with YaST/Agama — don't change).
* On SUSE builds singletrans is still **opt-in** via `ZYPP_SINGLE_RPMTRANS` (old codestreams: zypper only); non-SUSE builds always use it. Env wins over `zypp.conf techpreview.ZYPP_SINGLE_RPMTRANS`.
* singletrans is refused on non-UsrMerged roots installing a `may-perform-usrmerge` provider (bsc#1189788/bsc#1236481); `ZYPP_NO_USRMERGE_PROTECT=1` bypasses.
* If any package can't be downloaded, nothing is installed and **no exception** is thrown — check `noError()`/`attemptToModify()`.
* Classic mode: an install failure stops the commit, a remove failure continues.
* singletrans: `PackageFinished` arrives before `PackageError`, `ScriptFinished` before `ScriptError` — reports are closed lazily on the next begin; script output sync relies on the `0xDEADBEEF\n` tag on fd 4.
* The size-header comment in `commitInSingleTransaction` is stale; the commit is sent as a STOMP frame on zypp-rpm's stdin.
* `RpmFlags.h`, `errorcodes.h` and `CommitMessages` are shared with `zypp-rpm` — they form an ABI between the library and the libexec helper (installed in lockstep).
* Global lock is skipped for non-root and with `ZYPP_READONLY_HACK`; with `--root` it lives on the host `/run` unless `ZYPP_LOCKFILE_ROOT`.
* `/var/lib/rpm` compat symlink is (re)injected at init and after every commit; `--rebuilddb` must use the expanded real path.
* `rpmdb2solv`/@System cookie depends on rpmdb state hash **and** `/etc/products.d`; unwritable cache falls back to a tmp solv.
* Commit plugins run only for root "/" (or `TRANSACTIONAL_UPDATE`), not on dry-run, and can't veto: any non-ACK answer just drops the plugin. Docs say `ACC`, code wants `ACK`.
* History `install`/`remove` field 6 (`user@host`) is used to bootstrap `AutoInstalled`; keep the format stable.
* `%posttrans` arg is `2` for updates, `1` for fresh installs (bsc#1243279); Lua posttrans is left to rpm; rpm-4.18.0 `--runposttrans` chroot bug (bsc#1216091/bsc#1257068).
* `processConfigFiles` touches the host `/var/lib/update-messages/...` file even for chroot installs; real update messages live in `root/var/adm/update-messages`.
* `GLIBC_TUNABLES` hwcaps masking during commit (bsc#1246912) and `SYSTEMD_OFFLINE` in chroot (bsc#1118758) are injected for all scriptlets.
* Two simultaneous `Target`s must never exist (bsc#1203760): `initializeTarget` destroys the old one first.
* `RequestedLocales` empty on disk ⇒ pool locales untouched (bare-metal installs).
* The preloader does nothing if no `CommitPreloadReport` receiver is connected.
* **A fatal preload miss aborts the commit without falling back to the sequential download**, and leaves every step `STEP_TODO`. With `--download-only` that means `noError()` is true and zypper's commit-result check doesn't flag an error.
* Preloaded files in `<packagesPath>/.preload` are only cleaned after a real commit. After an abort or download-only they stay until a later run consumes them (checksum-validated, unlinked on use).
* The preloader's progress throttle is 500 ms although the comment says one second.
* Classic mode: answering **IGNORE** to an rpm install problem makes `RpmDb::installPackage` return normally. The step becomes `STEP_DONE` and a history `install` line is written although rpm failed.
* Classic mode: the return value of `RpmInstallReport/RpmRemoveReport::progress()` is ignored. A user abort is only noticed after the current rpm call has finished. The doc comments of the two reports contradict each other.
* A `TargetAbortedException` thrown after `COMMITBEGIN` (file-conflict abort, user abort in classic mode, `RpmTransactionFailedException` in singletrans) skips `COMMITEND`, `buildCache()`, `CommitActiveReport::end()` and the `ZYppImpl::commit` pool reload. Snapper then gets a pre snapshot without a post snapshot, and the in-memory `@System` is stale until the next `load()`.
* `AutoInstalled`, `RequestedLocales` and `/etc/zypp/locks` are written **before** rpm runs, so a failed commit can leave them describing the intended state.
* Unlocking an item that was locked by a wildcard/provides query doesn't remove that query. The lock comes back on the next load. `zypper al/rl` bypass the pool round trip and edit the file with `zypp::Locks`.
* State-file write errors (`HardLocksFile`, `SolvIdentFile`, `RequestedLocalesFile`) are only logged, never reported. `HardLocksFile::store` doesn't check the stream at all.
* The classic file-conflict check is useless with `DownloadAsNeeded` (the chroot default in classic mode): nothing is cached yet, so every package ends up in `noFilelist`.
* `ZYpp::diskUsage()` auto-detects mount points of `/`, not of the target root. Read-only mounts are dropped, `Hint_readonly` is never used in the computation, and packages without DU data count as 0 bytes (and hide the size of what they replace). libzypp itself never checks free space; only rpm does, unless `IGNORESIZE`.
* `DiskUsageCounter::detectMountPoints` runs `btrfs subvolume list -s` once per btrfs mount (as root). If any snapshot exists the whole volume becomes growonly, and only one mount per btrfs device is kept.
* `ZConfig::distroverpkg()` is hard-coded to `system-release`, and `$releasever` is re-evaluated from the baseproduct on every lookup unless the user defined it. `ZYPP_REPO_RELEASEVER` beats everything.
* `distributionFlavor()` (pool product flavor cached in `LastDistributionFlavor`) ≠ `targetDistributionFlavor()` (`<register><flavor>` in the file). `baseProduct()` needs a loaded pool, while `targetDistribution*()` read the file.
* The User-Agent (`targetDistribution`) and `X-ZYpp-DistributionFlavor` header values are function-statics, computed once per process with a guessed root.
* `TRANSACTIONAL_UPDATE` is read once per process. Its only effect is enabling commit plugins for root != "/".
* `ShutdownLock` failures are silent. Without `systemd-inhibit` there is no inhibitor and no warning.

## Where to start when debugging

* **Logs**: `/var/log/zypp/zypper.log` (or `ZYPP_LOGFILE`). Grep for `TargetImpl::commit(`, `Todo:`, `CommitPolicy(`,
  `DownloadAsNeeded enforces`, `bsc#1189788`, `zypp-rpm`, `[rpm`, `rpm-script-fd`, `posttrans` (`zypp::posttrans`
  group: `COLLECT`, `EXECUTE`), `librpmDb` group (`Suggest rpm dbpath`, `Inject missing /var/lib/rpm`),
  `zypp::plugin` / `PLUGIN`, `zypp::misc` (CheckAccessDeleted), `Read cookie:` (@System cache).
* **History**: `root/var/log/zypp/history` — `# … rpm output:` blocks contain scriptlet output and failure reasons.
* **Env vars**: `ZYPP_RPM_DEBUG=1` (rpm `-vv` / librpm DEBUG in zypp-rpm), `ZYPP_SINGLE_RPMTRANS=0|1`,
  `ZYPP_PCK_PRELOAD=0|1`, `ZYPP_COMMIT_NO_PACKAGE_CACHE=1`, `ZYPP_PLUGIN_DEBUG=1`, `ZYPP_PLUGIN_*TIMEOUT`,
  `ZYPP_LOCK_TIMEOUT`, `ZYPP_LOCKFILE_ROOT`, `ZYPP_READONLY_HACK`, `ZYPP_MODALIAS_SYSFS`, `ZYPP_NO_USRMERGE_PROTECT`,
  `TRANSACTIONAL_UPDATE`. Full list: `zypp/doc/autoinclude/EnvironmentVariables.doc`, chapter
  [Configuration & environment](09-configuration-and-environment.md).
* **Reproduce safely**: `zypper --root /tmp/x` (chroot target), `zypper in --dry-run` (singletrans: rpm test
  transaction; classic: file conflicts), `--download-only`. For singletrans vs classic comparisons toggle
  `ZYPP_SINGLE_RPMTRANS`.
* **Stale @System**: delete `/var/cache/zypp/solv/@System` (or `TargetImpl::clearCache`) and reload; compare with
  `rpmdb2solv -r ROOT -D DBPATH -X -p ROOT/etc/products.d -o /tmp/s`.
* **zypp-rpm**: cannot be run from a TTY. For gdb there is commented-out `gdbserver localhost:10001` in the `argv`
  array of `commitInSingleTransaction`. In devel builds `ZYPP_RPM_BINARY` points to `build/tools/zypp-rpm/zypp-rpm`
  (`zypp/CMakeLists.txt`), so the uninstalled helper is used.
* **Plugins**: test with a `/bin/cat` reflector or `zypp-plugin-python`; `PluginFrame_test.cc` in `zypp/tests/zypp/`.
* **Tests**: `zypp/tests/zypp/Target_test.cc` (initializeTarget in a tmp root), `PluginFrame_test.cc`,
  `RpmPkgSigCheck_test.cc`, `Locks_test.cc`. Upgrade testcases written to `/var/log/updateTestcase-*` replay
  the solver part (see [Testing](11-testing.md)).
* **zypper ps** issues: capture `lsof -n -FpcuLRftkn0 -K i > out` and feed `CheckAccessDeleted::check(out, verbose)`.
* **Lock problems**: inspect `/run/zypp.pid` and `/run/zypp-rpm.pid` (pid inside; empty file = free).
* **Disk usage questions**: run `zypp/tools/DiskUsageCounter [--root DIR]` to see which mount points libzypp detects
  (look for `g` = growonly). Check whether the repo provides DU data (`dumpsolv` on the repo solv file, look for
  `solvable:diskusage`). `DUdata_test.cc` shows the expected growonly arithmetic.
* **Preload failures**: grep for `CommitPackagePreloader disabled`, `No receiver for the CommitPreloadReport`,
  `Downloading packages via N connections`, `trying to taint mirror`, `No mirror found, giving up`,
  `Preloading done, mirror stats` and `Some packages could not be provided`. Set `ZYPP_PCK_PRELOAD=0` to compare with
  the serial path. Leftovers are in `/var/cache/zypp/packages/<alias>/.preload`.
* **Wrong `$releasever` / service repos missing**: check `readlink /etc/products.d/baseproduct`, grep for
  `baseproduct symlink is dangling`, `Remember distributionVersion`, `ServicesTargetDistro:` and
  `Skipping repository meant for`, and check `ZYPP_REPO_RELEASEVER` and `/etc/zypp/vars.d/releasever`.
* **Locks/AutoInstalled surprises**: grep for `Apply N HardLockQueries`, `HardLockQueries match`, `Wrote … locks`,
  `updateAutoInstalled`. Compare `/etc/zypp/locks` before and after the commit (it is rewritten only if the lock set
  changed).
* **Exit code puzzles in zypper**: `ZYppCommitResult` `operator<<` (logged at `Commit (…) returned:`) lists the step
  stages. Combine `allDone/noError/attemptToModify` as in `solve-commit.cc` to predict 0/4/8.
* **Inhibitor**: `systemd-inhibit --list` during a commit should show `zypp` (and `zypp-rpm` in singletrans mode).
