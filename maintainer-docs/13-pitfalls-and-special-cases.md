# 13 – Pitfalls & Special Cases (cross-cutting)

This is a condensed checklist of the gotchas found while writing chapters
00–12, grouped by theme rather than by subsystem. Each item points to the
chapter that has the details and code references. Keep it at hand when you
review PRs or triage bugs.


---

## 1. Compatibility & ABI (read before every review)

* **Every master release must stay ABI-safe for SLE 15.** SLE 15, SLE/Leap 16
  and Tumbleweed all build from `master` and differ only in spec-file defaults.
  [12]
* **Exporting a symbol takes two things:** `ZYPP_API` (with
  `-fvisibility=hidden`) **and** a `zypp::*` match in `zypp/zypp/libzypp.map`.
  Symbols outside `namespace zypp` are never exported. [12]
* **Green tests prove nothing about exports.** Tests link the static
  `zypp-allsym` library. Check with `nm -DC libzypp.so`. Installed tools must
  link `libzypp.so` (bsc#1228787). [01][12]
* After the switch to hidden visibility (17.33.0, jsc#PED-8153) several
  releases had to re-export symbols for PackageKit, libyui-qt-pkg and YaST
  (bsc#1228138, bsc#1228420). Expect consumers to use "internal" headers. [12]
* **Never add a virtual method to a report/callback class** (`ZYppCallbacks.h`,
  `KeyRingReport`, ...). That breaks the ABI. Pass new data through `UserData` /
  `ContentType` generic reports instead. [02][06]
* `zypp::shared_ptr` is `boost::shared_ptr` and appears in the public ABI. [02]
* `solver/detail` headers are installed and exported (`ZYPP_API_DEPTESTOMATIC`)
  for the testcase runner. Changing them can break `zypp-solver-testcase-runner`.
  [04]
* libsolv is linked statically on SUSE, but its types appear in public headers.
  A libsolv ABI change can force a libzypp soname bump (bsc#1115341). The spec
  pins `libsolv-tools-base` with `%requires_eq`. [01][12]
* `LEGACY(1735)` changes the class layout of `IdString`/`IdStringType`.
  `LEGACY(17)` guards `ConvertDBReport`, which SLE 15 yast2-pkg-bindings still
  uses. The macro has no outer parentheses, so wrap it when combining. [12]
* **Never delete files in `zypp/zypp-compat/`.** 17.35.0 broke consumers (#557).
  [12]
* `ZYPP_DEPRECATED` expands to nothing in release builds, so consumers never
  see the deprecation warnings. [12]
* `-D_FILE_OFFSET_BITS=64` in the pkg-config flags affects the ABI. [12]
* `zypp-rpm` and libzypp share `RpmFlags.h`, `errorcodes.h` and
  `CommitMessages`. The helper and the library must always ship together. [08]
* Public headers are "C++11" on paper but already need C++17. [02]

## 2. Versioning & releasing

* SONAME = `MAJOR*100 + COMPATMINOR` (e.g. `libzypp.so.1735.x.y`). Bumping
  MINOR alone never changes it. **COMPATMINOR must never exceed MINOR**
  (17.33.4 shipped that way by mistake). [12]
* The comment in `VERSION.cmake` refers to the obsolete `configure.ac`. The real
  soname math is in `zypp/CMakeLists.txt`. [12]
* `mkChangelog`:
  * Run it from `zypp/`. It opens `vi`.
  * It needs the previous tag to exist and refuses to run on a branch that is
    not fast-forward.
  * It switches to a dry run if other files are staged.
  * It parses the `# LAST RELEASED:` line, so keep that line's format. [12]
* Changelog lines come from commit messages. SLE fixes need `(bsc#…)` or
  `(CVE-…)`. [12]
* The spec's `Provides: libzypp(...)` feature tags must match
  `zypp/doc/autoinclude/FeatureTest.doc`. [12]
* SLE 12 CVE fixes go to `SuSE-SLE-12-SP2-Branch` (16.22.x), which has a
  different tree layout. [12]
* CI is `.obs/workflows.yml` (PRs only) plus GitHub Pages. `.travis.yml` is
  dead. [12]

## 3. Build system

* `zypp-logic/` is not a CMake subdirectory. Its `*.cmake` files define
  functions that **each generation calls**, so shared sources are compiled
  twice: as C++17 for legacy and as C++20 with `-DZYPPNG -DZYPP_ENABLE_ASYNC`
  for NG. Shared code must compile both ways. [01][10]
* The same `#include <zypp-core/ng/async/...>` resolves to **different
  headers** per build: legacy gets `MaybeAwaitable<T> = T`, NG gets `Task<T>`.
  [10]
* `#ifdef ZYPP_ENABLE_ASYNC` blocks in `zypp/zypp/ng/**` are dead code, never
  compiled. [10]
* Static libraries must be linked in this order: common → curl → media → tui →
  core → libsolv. [01]
* `SET_LOGGROUP` overwrites the `COMPILE_FLAGS` source property. [01]
* `ENABLE_DEVEL_BUILD` only reroutes helper binaries (`zypp-rpm`, workers,
  repo2solv/rpmdb2solv in stack builds). It does **not** reroute config,
  plugins or caches. [01]
* `-DSUSE` silently turns on docs, translations and tests, which makes doxygen
  mandatory unless `DISABLE_AUTODOCS` is set. [01]
* `make install` gives an incomplete layout. The `/etc/zypp/*.d` directories,
  plugin dirs and `/var/...` are created only by the spec. [01]
* The translation scan only covers `*.h`/`*.cc`. Strings in `.cppm` never reach
  `zypp.pot`. [01]
* The zypp-stack parent force-sets cache options on every configure, so `-D`
  overrides don't stick. It also forces `SUSE` on. [01]
* `zypp_EARLY_SRCS` ordering is intentional (probably static initialisation
  order). Keep it. [01]
* NG needs CMake ≥ 3.28. Module units must be listed in bottom-up order. [01][10]

## 4. Configuration & environment

* **ZConfig always reads the host `zypp.conf`.** Only `solver.*` and
  `multiversion` are re-read from a `--root` target. Paths are host values, and
  callers must add the root prefix. [09]
* **Any `/etc/zypp/zypp.conf` fully replaces the vendor file** (UAPI.6). The
  admin then loses `multiversion=provides:multiversion(kernel)` unless they copy
  it. [09]
* Drop-ins without their own `[main]` header have all keys ignored, with only a
  warning (bsc#1272534). [09]
* `$ZYPP_CONF` is root-relative and disables drop-ins. If the file is missing,
  libzypp falls back to **builtin** defaults. An empty value means different
  things in legacy and NG. [09]
* The `[env]` section never overwrites existing environment variables. It also
  comes too late for anything read at static init (`ZYPP_LOGFILE`,
  `ZYPP_FULLLOG`, plugin timeouts, `ZYPP_READONLY_HACK`). [09]
* **Many env vars are cached in function statics.** A later `setenv()` (for
  example in tests) has no effect. [09]
* `ZConfig::systemRoot()` and `multiversionSpec()` call `getZYpp()`, so they
  may create ZYpp and **take the lock** as a side effect. [09]
* `ZYPP_TESTSUITE_FAKE_ARCH` makes every commit throw. `TestcaseSetup` sets it
  process-wide. [03][09]
* Arch detection is cached per process. x86_64 is upgraded to v2/v3/v4 via
  CPUID, and ARM hard-float detection reads the **host** `/etc/rpm/platform`.
  [09]
* The library sets `BOOST_TEST_CATCH_SYSTEM_ERRORS=no` in every process. During
  commit it sets `ZYPP_IS_RUNNING`, `SYSTEMD_OFFLINE` (chroot, bsc#1118758) and
  `GLIBC_TUNABLES` (bsc#1246912). [08][09]
* `techpreview.ZYPP_MEDIANETWORK` is still set, but nothing reads
  `ZYPP_MEDIANETWORK` any more. [09]
* The `update.*dir` keys are ignored, and the legacy key is misspelled
  `messagessdir`. `credentials.global.*` defaults are hardcoded to `/etc/zypp`.
  [09]
* `solver.upgradeTestcasesToKeep=-1` works only through unsigned wraparound.
  [09]

## 5. Logging & exceptions

* Nothing is logged unless `LogControl::logfile()` or `ZYPP_LOGFILE` is set.
  zypper uses `/var/log/zypper.log`. [02]
* `DBG` logs at `E_MIL` into group `"<group>++"`, so grep for `++]`. [02]
* **Legacy logging travels over an abstract unix socket** (`zypp-logsocket-<pid>`)
  using the NG event loop. Breaking `zypp-logic/zypp-core/ng` breaks all logging.
  Forked children don't log. [02]
* Every `ZYPP_THROW`/`CAUGHT`/`RETHROW` writes an INT line. An INT line is
  **not** necessarily a bug. [02]
* `ZYPP_THROW(e)` slices to the static type. Inside a `catch`, use
  `ZYPP_RETHROW`. [02]
* `Exception::remember(exception)` replaces the history, while
  `remember(string)` appends. [02]

## 6. Object model & threading

* `ReferenceCounted` uses a **non-atomic** counter. `RWCOW_pointer` clones on
  any non-const access. Legacy is not thread-safe. [02]
* There is one global receiver per report type, with no locking.
  Constructing/destroying a `SendReport` fires `reportbegin`/`reportend`. [02]
* `UserData::get<T>` needs the exact stored type. `set()` on a const UserData
  only fills empty keys, and receivers use that to send values back. [02]
* `Pathname("foo")` becomes `"./foo"`, and `"a" / "/b" == "a/b"`. The
  `filesystem::*` functions return errno instead of throwing. [02]
* `Url` hides passwords in `asString()` (CVE-2017-9271). Writing `.repo` files
  needs `hotfix1050625::asString()`. `Url ==` compares the full string,
  password included. [02][06]
* `ExternalProgram` with root `/` only does `chdir`, no chroot. A child killed
  by a signal returns 128+signo. [02]
* `Capability::c_str()` for compound dependencies returns a static 5-slot ring
  buffer. [03]
* `Edition::match` is not transitive, so never sort with it. [03]
* One libsolv pool per process, owned by `zyppng::sat::StringPool`
  (LEGACY-REQUIRED). Ids get reused when the last repo is removed, so caches
  must watch `serialIDs()` as well as `serial()`. [03]
* `PoolItem` and `ResPoolProxy` go stale after pool changes. [03]
* Foreign-arch solvables are dropped when a repo is added (except `@System`).
  [03]
* NG: an object must be `shared_ptr`-owned to `connect` signals. Each thread's
  `EventDispatcher` is held only weakly, so declare its ref member first. Task
  lifetime traps: destroying a Task in its notify callback, start() plus
  co_await, signals emitted before the await. [10]

## 7. ResStatus & solver semantics

* `ResStatus` causers: `USER > APPL_HIGH > APPL_LOW > SOLVER`. Installed state
  is immutable, only USER/APPL_HIGH may lock, and unlocking resets the causer to
  SOLVER. [03]
* **Only USER/APPL_HIGH transactions are solver requests.** SOLVER/APPL_LOW
  transactions are cleared on every run. `verifySystem()` also drops APPL_HIGH.
  [04]
* Solver modes (`doUpgrade`, `doUpdate`, `verifySystem`) **stay on** across
  `resolvePool()` calls. `reset()` only clears verify mode. [04]
* `doUpdate()` is a separate code path. It ignores user selections,
  `addRequire`/`addConflict` and upgrade repos, and reports no problems. [04]
* Read `problems()` before the next solve. [04]
* Flag defaults exist twice (`Resolver.cc` and the `SATResolver` constructor).
  `allowNameChange` and `ignoreAlreadyRecommended` can't be set in zypp.conf.
  [04]
* `onlyRequires` still installs language and modalias recommends. [04]
* Applying REMOVE/UNLOCK problem solutions works at USER level, so the result is
  **persisted to `/etc/zypp/locks`** on commit. [04]
* Soft locks work by name and are ignored if any version is installed. Locks
  written by UIs are simple name queries, and re-applying locks only adds. [03][04]
* Products share their status with the `-release` package (bsc#1225267). [03]
* DUP: a second solve for product drop lists (bsc#1155819); arch pinning only
  for root `/`; `dup-vendor-relax(suse)` (bsc#1182629). [04]
* `retracted-patch-package()` and `ptf()` providers are blacklisted,
  `ptf-package()` is not (bsc#1186503). [04]
* `suse` and `opensuse` are **not** vendor-equivalent by default (bsc#1030686).
  [03]
* An empty requested-locale set is not the same as `{en}` (bsc#1155678). [03]
* The "update stack first" patch logic lives in zypper, not libzypp. [04]
* `PurgeKernels::markObsoleteKernels()` sets `forceResolve` permanently. [04]
* Writing a testcase re-runs `resolvePool()` and wipes the output directory.
  [04]

* **Modalias** ([03b](03b-modalias.md)) is soft only. Only `Supplements` are
  rewritten into `namespace:modalias`; a `Requires: modalias(…)` makes a package
  uninstallable. An unencoded second `:` turns the first word into a package
  prefix, so CPU aliases need `%3A`. The host's `/sys` is used even with
  `--root`, and the list is read once per process. Rich-dep modalias needs
  libsolv ≥ 0.7.36 **when the cache is built**; old caches are not rebuilt
  automatically.

## 8. Repositories & services

* `zypp::RepoManager` forwards every call to the NG implementation in
  `zypp/zypp/ng/` (synchronously). [05][10]
* Just constructing a RepoManager can delete orphaned service repos and unknown
  cache dirs (bnc#891515; not in readonly mode, bsc#1210740;
  `.no_auto_prune` spares package caches, bsc#1204956). [05]
* `type=` in `.repo` files is ignored when reading (bsc#1177427). A probed type
  is never persisted. [05]
* `repo.refresh.delay` only applies when the raw status matches the solv cookie
  (bsc#1174016). Local URLs ignore it, and CD/DVD repos are never refreshed. [05]
* Changing a URL or a variable it expands to changes the repo status, which
  forces a refresh and rebuild. [05]
* The master index (`repomd.xml`/`content`) is **never** fetched from mirrors.
  `repoOrigins()`/`url()` may fetch a mirrorlist over the network. [05]
* `loadFromCache` silently rebuilds broken or outdated solv files. [05]
* Repo variables are a process-wide singleton keyed by root (bsc#1237044).
  `$releasever` follows the target (bnc#943563). [05]
* Plugin services run chrooted (bsc#1080693). For services,
  `repostodisable` is cleared each time and `repostoenable` applies once. [05]
* `.repo` files are rewritten non-atomically. Renaming a repo doesn't move its
  caches. [05]
* The RepoManager destructor may run appdata plugins as root. [05]

## 9. Security

* **There is no persistent zypp keyring.** The general and trusted rings are
  temp gpg homedirs. The rpmdb `gpg-pubkey-*` entries are the persistent trusted
  store. [06]
* `PublicKeyData::created()` is the newest self-signature time (bsc#1179222).
  Keys with subkeys are always re-imported (bsc#1008325). [06]
* Signatures from expired keys still validate. Unknown-key signatures are
  ignored if a good one exists and none is bad (bsc#1180721). [06]
* Accepted is not validated: only `fileValidated` is stored (as a
  `.repo_gpgcheck` symlink), and it decides whether packages need an rpm
  signature. [05][06]
* A repo-level `gpgcheck=` masks the zypp.conf `repo_gpgcheck`/`pkg_gpgcheck`
  defaults. `repo_gpgcheck` is strict (bsc#1274625). [06]
* Keyhint rules: long ids only (bsc#1271730); plain filenames (bsc#1267426,
  CVE-2026-44941); keys are only imported if repomd was validated by an
  already-trusted key. [06]
* A user's "accept wrong checksum" answer is cached for 12h in a static map.
  [06]
* An rpm counts as signed only with a header signature plus a payload digest
  (bsc#1184501). rpm output is parsed in the C locale. [06]

## 10. Target, commit & locking

* `dryRun` is the `RPMINST_TEST` bit inside `rpmInstFlags`. [08]
* `DownloadOnly` implies dry-run. `DownloadAsNeeded` **forces the classic
  backend**. [08]
* Single-rpm-transaction mode is opt-in on SUSE (`ZYPP_SINGLE_RPMTRANS`). It is
  refused for usrmerge transitions (bsc#1189788). [08]
* **A download failure means nothing is installed and nothing is thrown.** Check
  `noError()`/`attemptToModify()`. [08]
* Classic backend: an install failure aborts the commit, a remove failure
  continues. [08]
* zypp-rpm sends `PackageFinished` **before** `PackageError`. [08]
* Global lock gaps:
  * Non-root users never lock.
  * `ZYPP_READONLY_HACK` skips the lock.
  * `--root` still uses the host `/run/zypp.pid` unless `ZYPP_LOCKFILE_ROOT` is
    set.
  * A negative `ZYPP_LOCK_TIMEOUT` waits forever. [08]
* Commit plugins:
  * They only run with root `/` (or `TRANSACTIONAL_UPDATE`), never on dry-run.
  * They can't veto.
  * The code expects `ACK`, while the docs say `ACC`. [08]
* **The history log format is load-bearing.** The `user@host` field bootstraps
  `AutoInstalled`. [08]
* `/var/lib/rpm` → `/usr/lib/sysimage/rpm` compat symlink is re-created at init
  and after commit. [08]
* Chroot commits auto-mount `/proc` and `/dev` (bsc#1181328). Never keep two
  Targets alive at once (bsc#1203760). [08]
* `%posttrans` arg is 2 for updates, 1 for installs (bsc#1243279). No dump mode
  for chroot targets on rpm-4.18.0 (bsc#1216091). [08]

## 11. Media & downloads

* `MediaAccess`, `MediaMultiCurl`, `MediaNetwork` and the old
  `zyppng::Downloader`/`MirrorControl` are **gone**. Old docs and comments still
  mention them. [07]
* **The HTTP backend default depends on the codestream:** MediaCurl2 (the NG
  `NetworkRequestDispatcher`, in-process) on 16/TW, and legacy MediaCurl on
  `suse_version < 1600`. Overrides: `ZYPP_CURL2=0/1`, then
  `?mediahandler=curl|curl2`. [07]
* There are two unrelated `zyppng::Provide` classes: a sync facade in
  `zypp/zypp/ng/media/` and the NG worker scheduler in `zyppng/`. Legacy never
  starts `zypp-media-*` workers. [07][10]
* MediaCurl2 has no If-Modified-Since, no overall timeout, and no metalink or
  zsync block downloads. Metalink is only used for repo mirror lists. [07]
* Mirrors are tried before authorities. Security-relevant files
  (`mirrorsAllowed()==false`) and `doesFileExist` use authorities only. Mirrors
  with a different scheme are dropped silently. Changing the URL in the
  media-change dialog drops all mirrors. [07]
* **Media exceptions are dispatched by exact `typeid`.** Throw the exact class,
  not a subclass. [07]
* `download.max_silent_tries` defaults to 1, which means no retries. When
  retries happen, every error is retried, even 404. [07]
* `TransferSettings` starts with SSL verify **off**. Only
  `fillSettingsFromUrl()` turns it on, and only for https. [07]
* Any new URL query option must be added to `clearQueryString()`, or it leaks to
  servers. [07]
* Redirects are allowed to https only (bsc#1218831). Header values must not end
  in a space, or HTTP/2 fails with error 92 (bsc#1212187). [07]
* Credentials: stored credentials are offered only once (bsc#1210870), and
  `auth=basic` with a user in the URL preloads the password (bsc#1174011). [07]
* libproxy is loaded with `dlopen`. Under YaST the last proxy wins, elsewhere
  the first (bsc#1244710, bsc#1247690). `~/.curlrc` is used only for
  `proxy-user`. [07]
* ISO media depend on a parent handler: `releaseAll()` makes two passes and
  `close()` can throw `MediaIsSharedException`. [07]
* `plugin:` URLs need a UrlResolverPlugin; `MediaPlugin` is a stub
  (bsc#1228208). [07]
* `download.max_concurrent_connections` only affects the commit preloader.
  Ordinary downloads are serial. [07]

## 12. Testing

* Tests link `zypp-allsym`, so they say nothing about exported symbols. [11][12]
* When running a test binary by hand, pass `--catch_system_errors=no` (ctest
  does that for you). Otherwise tests that fork give false failures. [11]
* `TESTS_SRC_DIR` is the `zypp/tests` root, not the test's own folder. [11]
* Media backend tests need nginx, vsftpd, squid, `basic_ncsa_auth` and libfcgi
  **at configure time**. A missing tool fails the configure step instead of
  skipping tests. The stack force-enables them. `Proxy_test` needs libproxy
  unless `DISABLE_LIBPROXY=ON`. [11]
* Web server tests use fixed ports (10001/10002) with no ctest locks, so
  `ctest -j` can make them collide. [11]
* `TestSetup` changes process-wide singletons (ZConfig, `sat::Pool`, target),
  so test order inside one binary matters. Loading a testcase sets
  `ZYPP_TESTSUITE_FAKE_ARCH`, after which commits throw. [11]
* `TestSetup::loadRepo()` turns off gpgcheck unless `TSO_REPO_DEFAULT_GPG` is
  set. [11]
* Several test sources are never built and likely don't compile
  (`media1..4_test.cc`, `Signature_test.cc`, `Resolvable_test.cc`, ...). [11]
* `zypp/tests/zyppng` tests the async core inside legacy. `zyppng/tests` is the
  module-based NG suite, built only with `BUILD_LIBZYPPNG`. [11]
* `-DSUSE`/`-DDEBIAN` silently enable tests. Even with tests off, their
  dependencies are still needed at configure time. [11]
* Every OBS build runs the full suite in `%check`. That is the real CI. [11][12]
* Solver testcases: `zypper --debug-solver` writes
  `/var/log/zypper.solverTestCase`. Dist upgrades leave
  `/var/log/updateTestcase-*`. Results depend on the libsolv version. [04][11]
* Never package an `ENABLE_DEVEL_BUILD` build, because it embeds build-tree
  paths. [01][11]


## 13. Consumers (see [14](14-consumers-and-api-surface.md))

* **Conflict to resolve:** consumers on older codestreams (YaST, PackageKit)
  expect public headers to compile as C++11, but [02] found they already need
  C++17. Decide on a policy and document it.
* `connect()` replaces the previous receiver of a report. An internal libzypp
  receiver would silently steal a consumer's callbacks.
* The value types in callback `UserData` must stay exactly the same (for
  example `"line"` is a `std::cref` in classic mode but a `std::string` in
  single-transaction mode).
* The `pkgGpgCheck` `"Action"` key is tri-state (value / key without value /
  no key). zypper, YaST and Agama depend on it.
* Callback defaults are a security contract. PackageKit and Agama rely on the
  library defaults.
* zypper exit codes 4/8/102/103/107 derive from `ZYppCommitResult` and Patch
  flags. connect-ng checks 100–106.
* PackageKit holds `ZYpp::Ptr` only per job and calls `initializeTarget` once
  per process (bnc#899755).
* `ZYppFactoryException::lockerName()` is parsed by zypper to detect
  `packagekitd` (bnc#580513).
