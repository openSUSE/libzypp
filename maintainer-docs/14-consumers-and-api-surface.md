# Consumers and the de-facto API surface

This chapter covers **who calls libzypp and how**: zypper (source is local in `../zypper`), YaST (`yast2-pkg-bindings`), the libyui package selectors (`libyui-qt-pkg`, `libyui-ncurses-pkg`), the PackageKit zypp backend, Agama (`zypp-agama-sys`), and the plugin/CLI consumers (snapper, SUSEConnect). It lists the API areas, callbacks, env vars and on-disk behaviour those consumers depend on. It ends with risk tables, a "never change" list, a pre-change checklist, and notes on coordinating releases with zypper.

> **Where the facts come from.** Everything about zypper was checked in the local tree (`zypper/` next to `libzypp/`, version 1.14.101). YaST, libyui, PackageKit, Agama, snapper and connect-ng were checked against their GitHub default branches on 2026-10-22, by fetching and grepping the source. Line numbers for external projects are approximate and will drift. If a statement says "not verified", treat it as such.

Related chapters: [Core infrastructure (callbacks, logging)](02-core-infrastructure.md), [Pool & resolvables](03-sat-pool-and-resolvables.md), [Solver](04-solver.md), [Repositories](05-repositories-and-services.md), [Security/GPG](06-security-gpg-keyring.md), [Target/commit](08-target-commit-and-rpm.md), [Configuration](09-configuration-and-environment.md), [Release & compat](12-release-packaging-and-compat.md).

---

## 1. The big picture

```
                         +-------------------------------+
   CLI / scripts ------> |  zypper  (C++, links libzypp, |
   (SUSEConnect,         |   libzypp-tui)                |
    zypper-migration,    +---------------+---------------+
    salt, ansible ...)                   |
                                         |  C++ API + callbacks (ReceiveReport<>)
   YaST (Ruby) --YCP--> yast2-pkg-bindings ----+
   libyui-qt-pkg / libyui-ncurses-pkg ---------+--->  libzypp.so.17xx  (zypp/, zypp-logic/)
   PackageKit daemon -> pk-backend-zypp.cpp ---+         |
   Agama (Rust) -> zypp-agama-sys C layer -----+         |  fork/exec + stdin/stdout protocols
                                                         v
                                           plugins: commit (snapper-zypp-plugin), services,
                                           urlresolver, system, appdata, repoverification,
                                           sigcheck  (python3-zypp-plugin based or native)
```

How consumers are coupled to libzypp:

| Coupling | Examples | Breaks on |
|---|---|---|
| **C++ API/ABI** (headers + `ZYPP_API` exported symbols) | all C++ consumers | removed or changed symbols, changed class layout (soname is tied to COMPATMINOR, see [12](12-release-packaging-and-compat.md)) |
| **Callback contracts** (`callback::ReceiveReport<T>`, `UserData` content types and keys) | zypper, YaST, PK, Agama | renamed keys, a changed `boost::any` value type, reports no longer sent, changed meaning of return values |
| **Behavioural contracts** (solver defaults, lock semantics, exception types) | all | silent behaviour change, which is the most dangerous kind |
| **Process environment** (env vars, files in `/run`, `/var/log`, `/etc/zypp`) | zypper, PK, Agama | renamed env vars or paths |
| **Plugin protocols** (STOMP-like frames over stdin/stdout) | snapper, SUSE Manager, python plugins | changed frames/headers |
| **zypper CLI/exit codes** (indirect consumers) | SUSEConnect, zypper-migration, salt | zypper changes caused by libzypp behaviour changes |

---

## 2. zypper (the primary consumer)

zypper and libzypp have the same maintainers and are released together. zypper is the reference for *how libzypp is meant to be used*: Agama's README says outright "for seeing how it works see yast2-pkg-bindings and zypper".

### 2.1 Version requirements and build coupling

| Where | What |
|---|---|
| `zypper/VERSION.cmake` | zypper 1.14.101 |
| `zypper/zypper.spec.cmake` | `BuildRequires: libzypp-devel >= 17.38.0`, `BuildRequires: libzypp-tui-devel >= 1`, runtime `%requires_ge libzypp` (requires libzypp >= the version it was built against) |
| `zypper/CMakeLists.txt` | `FIND_PACKAGE( Zypp REQUIRED )`. In the zypp-stack build (`ZYPP_STACK_BUILD`) it links the in-tree `zypp` and `zypp-tui` targets directly |
| `libzypp/zypp/libzypp.spec.cmake` | `Conflicts: zypper < 1.14.91` ("API refactoring. Prevent zypper from using (now) private symbols") |
| `zypper/src/info.cc` | compile-time switch `#if LIBZYPP_VERSION <= 173718` (falls back to the `pattern()` capability check instead of `isPatternPackage()`). `LIBZYPP_VERSION` comes from `zypp-logic/zypp-core/APIConfig.h.in` via `zypp-core/Globals.h` |
| zypper `Provides:` | `zypper(auto-agree-with-product-licenses)`, `zypper(oldpackage)`, `zypper(updatestack-only)`, `zypper(purge-kernels)`, `zypper(include-all-archs)`. These are feature tests that *other* packages can require from zypper |

History of the coupling, from `zypp/package/libzypp.changes`:
- 17.33.0: `Add ZYPP_API for exported functions and switch to visibility=hidden (jsc#PED-8153)`. This was followed by a run of "add missing export for PackageKit / libyui-qt-pkg" releases (17.33.2/17.33.4), and in 17.35.4–17.35.6 by `Export RepoVariablesStringReplacer for yast2 (bsc#1228138)`, `Export asSolvable for YAST`, `Export CredentialManager for legacy YAST versions (bsc#1228420)`. **Lesson: the export list is a consumer contract, and hiding a symbol is a breaking change.**
- 17.35.10: `API refactoring. Prevent zypper from using now private libzypp symbols (bsc#1230267)`, `Conflicts: zypper <= 1.14.76`. This was later raised to `< 1.14.91`.
- 17.37.6: mirror handling during refresh "Requires updating zypper to 1.14.91."
- 17.31.1/17.32.0: "Don't expose header files which use types not available in c++11. In 15.3 and older, YAST and PK compile with -std=c++11", and "Don't expose std::optional where YAST/PK explicitly use c++11". **Public headers must stay C++11-clean** (this matches the tier rules in `AGENTS.md`).

zypper also uses **libzypp-tui** (`zypp/zypp-tui/`, shipped in `libzypp-devel`, which has `Provides: libzypp-tui-devel = 1`). Its headers are `zypp-tui/Application`, `zypp-tui/output/Out.h`, `zypp-tui/Table.h`, `zypp-tui/utils/*`. zypper is the only verified user. Changing it means changing zypper in lockstep and bumping the `libzypp-tui-devel` provide when it becomes incompatible.

### 2.2 Which headers zypper uses

There are 104 distinct `#include <zypp...>` lines across `zypper/src`. The most common (count = number of files):

```
21 zypp-core/base/Logger.h      10 zypp/PoolQuery.h         6 zypp/RepoManager.h
18 zypp-core/base/String.h       9 zypp/ZYppCallbacks.h      6 zypp/RepoInfo.h
16 zypp-core/base/Flags.h        9 zypp/Patch.h              6 zypp/Package.h
15 zypp-core/Pathname.h          8 zypp/ZYppFactory.h        5 zypp/PoolItem.h, Locks.h
15 zypp-core/base/LogTools.h     8 zypp/ui/Selectable.h      4 zypp/target/rpm/RpmHeader.h
12 zypp-core/TriBool.h           8 zypp/ResKind.h            4 zypp-media/MediaException
                                 6 zypp/ZYpp.h, ResPool.h
```

Less obvious but load-bearing: `zypp/zypp_detail/ZYppReadOnlyHack.h` (a "detail" header that is still API), `zypp/target/rpm/RpmDb.h`, `librpmDb.h`, `zypp/target/CommitPackageCache.h`, `zypp/PurgeKernels.h`, `zypp/misc/CheckAccessDeleted.h` (`zypper ps`), `zypp/parser/HistoryLogReader.h`, `zypp/parser/ProductFileReader.h`, `zypp/sat/FileConflicts.h`, `zypp/sat/Queue.h`, `zypp/ResolverFocus.h`, `zypp/repo/SrcPackageProvider.h`, `zypp/parser/xml/Reader.h`, `zypp-curl/auth/CurlAuthData`, `zypp/base/Backtrace.h`, `zypp/base/Measure.h`.

zypper already uses the **new include paths** (`zypp-core/...`, `zypp-media/...`). YaST, libyui and PK still use the **old paths** (`zypp/base/Logger.h`, `zypp/Pathname.h`, `zypp/base/Sysconfig.h`, ...). Those old paths are forwarding headers in `zypp/zypp-compat/` (78 files; e.g. `zypp/zypp-compat/zypp/base/Sysconfig.h` just does `#include <zypp-core/parser/Sysconfig>`). **Do not remove zypp-compat headers.** libyui-qt-pkg includes `zypp/base/Sysconfig.h`.

To regenerate these numbers:
```sh
grep -rhoE '#include <zypp[^>]*>' zypper/src | sort | uniq -c | sort -rn
```

### 2.3 Getting the ZYpp instance and the global lock

`zypper/src/Zypper.cc`, `Zypper::assertZYppPtrGod()`:

1. `God = getZYpp()` inside `try`.
2. It catches `ZYppFactoryException`. If `excpt_r.lockerName()` contains `"packagekitd"` (bnc#580513), zypper offers to ask PackageKit to quit (`packagekit_suggest_quit()`, polling `packagekit_running()`), then retries `getZYpp()`.
3. If it is still locked: it prints `excpt_r.asString()` and sets **exit code 7 `ZYPPER_EXIT_ZYPP_LOCKED`**.
4. Any other `zypp::Exception` sets **exit code 4 `ZYPPER_EXIT_ERR_ZYPP`**.

The lock is acquired *after* option parsing (bsc#1223766: `-y`/`--no-confirm` alias `--non-interactive`, which must be in effect before the PK prompt). Before calling `getZYpp()`, zypper:
- sets `ZYPP_LOCKFILE_ROOT=<root>` (setenv without overwrite) when `--root` is not `/` (bnc#575096 "Quick fix"). The same happens in `commandShell()`.
- calls `zypp_readonly_hack::IWantIt()` if `ZYPP_READONLY_HACK=1` is set, or for the commands `lr`, `ls`, `help`, `versioncmp`, `targetos` (#247001, #302152). Read-only mode skips the lock, and `RpmDb`/`RepoManager` then refuse to write (`zypp/zypp/target/rpm/RpmDb.cc`, `zypp/zypp/ng/repomanager.cc` check `IGotIt()`).
- skips the lock entirely for `ps` and subcommands (bnc#703598).

The libzypp side is `zypp/zypp/ZYppFactory.cc`:
- The lock file is `$ZYPP_LOCKFILE_ROOT/run/zypp.pid` (default root `/`), protected by a boost `file_lock`. It holds the locker's PID. A dead PID or a zombie (empty `/proc/PID/cmdline`) is ignored.
- `lockerName()` is the **first NUL-terminated argv element read from `/proc/PID/cmdline`**. zypper's `"packagekitd"` substring match depends on this exact source.
- **Non-root never takes the lock** (`geteuid() != 0` returns "not locked"). Many user-mode consumers rely on this.
- Lock wait: `ZConfig::lockTimeout()` (`ZYPP_LOCK_TIMEOUT`; negative means wait forever, bsc#1184399, bsc#1213231).
- After taking `zypp.pid`, it also checks `run/zypp-rpm.pid` and throws if a `zypp-rpm` from a previous commit is still running.
- `_theZYppInstance` is a `weak_ptr<ZYpp>`. When the **last `ZYpp::Ptr` dies, `~ZYpp()` resets `_theGlobalLock`**, which truncates the PID file. The `ZYpp::Impl` is a function-static `_theImplInstance` that is **never destroyed**, so the target, pool and resolver survive lock drop and re-acquire. `ZYpp::ZYpp()` calls `zyppintern::repoVariablesReset()` on each re-acquire. This is the "Allow PackageKit to drop the zypp lock (bnc#899755)" design (16.0.2), and PackageKit depends on it (§4).

### 2.4 Logging

`zypper/src/main.cc`: `base::LogControl::instance().logfile( getenv("ZYPP_LOGFILE") ?: "/var/log/zypper.log" )` (`ZYPPER_LOG` in `main.h`), followed by the `===== Hi, me zypper <VERSION>` banner and the argv dump. `zypp-refresh.cc` uses `ZYPP_REFRESH_LOG` the same way. The spec `touch`es `/var/log/zypper.log` and installs a logrotate config (`zypper.lr`, `zypp-refresh.lr`). The `zypper-log` tool parses this file, so **the libzypp log line format is a de-facto contract with `zypper-log`** (see [02](02-core-infrastructure.md) for the formatter). Agama and PackageKit install their own `LogControl` line writers or files (§4, §5).

Other on-disk artefacts zypper derives from libzypp:
- `--debug-solver` → `Resolver::createSolverTestcase("/var/log/zypper.solverTestCase")` (`solve-commit.cc`, `make_solver_test_case`).
- With `ZYPP_FULLLOG=1`, libzypp itself writes `/var/log/YaST2/autoTestcase` (changes 17.37.6, bsc#1244042).
- `/run/reboot-needed` handling (bsc#1183268, bsc#1217873) lives in zypper's `solve-commit.cc`, not in libzypp.

### 2.5 CLI option → libzypp mapping

| zypper option | libzypp call / effect | Where (zypper/src) |
|---|---|---|
| `--root DIR` / `-R` | `Config::root_dir`, `changedRoot=true`; `ZYPP_LOCKFILE_ROOT`; `initializeTarget(root)` in `init_target`; `ZConfig::announceSystemRoot(root)` even when no target is initialised (bsc#1237044) | `Config.cc handleRootOptionCB`, `commands/basecommand.cc` |
| `--installroot DIR` | as `--root`, plus `ZConfig::instance().setRepoManagerRoot("/")` so repos and caches stay on the host | `Config.cc` |
| `--reposd-dir`, `--cache-dir`, `--raw-cache-dir`, `--solv-cache-dir`, `--pkg-cache-dir` | `ZConfig::setRepoMetadataPath / setRepoSolvfilesPath / setRepoPackagesPath ...` | `Config.cc` |
| `--userdata STR` | `ZConfig::setUserData()` (validated; written to history/plugins) | `Config.cc` |
| `--non-interactive` / `-n` | Nothing in libzypp. **Callbacks return the prompt's default answer** (`utils/prompt.cc get_prompt_reply`) | `callbacks/*` |
| `--no-gpg-checks` | Nothing in libzypp. KeyRing/Digest/pkgGpgCheck callbacks accept (see §2.6) | `callbacks/keyring.h`, `callbacks/repo.h` |
| `--gpg-auto-import-keys` | `askUserToAcceptKey` → `KEY_TRUST_AND_IMPORT` | `callbacks/keyring.h` |
| `--download-only` / `-d`, `--download=MODE` | `ZYppCommitPolicy::downloadMode(DownloadOnly / InAdvance / InHeaps / AsNeeded)`; default `ZConfig::commit_downloadMode()`. Choosing `AsNeeded` together with single-trans mode prints a hint about `ZYPP_SINGLE_RPMTRANS=0` | `commands/optionsets.cc`, `solve-commit.cc SolveAndCommitPolicy::downloadMode` |
| `--dry-run` / `-D` | `ZYppCommitPolicy::dryRun(true)`; `syncPoolAfterCommit` only in the shell and not on dry-run | `solve-commit.cc` |
| `--replacefiles` | `ZYppCommitPolicy::replaceFiles()` | `solve-commit.cc` |
| `--recommends` / `--no-recommends` | `Resolver::setOnlyRequires()`. Default is `!ZConfig::solver_onlyRequires()`. **`remove` always uses onlyRequires** | `solve-commit.cc set_no_recommends` |
| (implicit) | `setIgnoreAlreadyRecommended(true)` except for `dup` (unless `--no-recommends`) and `inr` | `set_ignore_recommends_of_installed` |
| `--force-resolution` / `-R` (`--no-force-resolution`) | `Resolver::setForceResolve()`. Default per command from zypper.conf `solver.forceResolutionCommands` (`remove` by default) | `set_force_resolution` |
| `--clean-deps` / `-u`, `-U` | `Resolver::setCleandepsOnRemove()` | `set_clean_deps` |
| `--solver-focus` | `Resolver::setFocus(ResolverFocus)` | `set_solver_flags` |
| `--allow-vendor-change`, `--allow-downgrade`, `--allow-name-change`, `--allow-arch-change` (and the `--no-` forms) | **in `dup`:** `dupSetAllow*()`; **otherwise:** `setAllow*()`. Applied only if given (TriBool); otherwise libzypp/zypp.conf defaults apply | `set_solver_flags` |
| `dup --remove-orphaned` | `setRemoveOrphaned()` (dup only; libsolv limitation) | `set_solver_flags` |
| `dup --from REPO` | `Resolver::addUpgradeRepo(repo)` + `resolvePool()` (and `setIgnoreAlreadyRecommended(true)` again); otherwise `doUpgrade()` | `solve-commit.cc dist_upgrade` |
| `update` | `Resolver::doUpdate()` (`update.cc`); `setUpdateMode(solve_with_update)` | `update.cc`, `solve-commit.cc` |
| `patch --with-update`, `--updatestack-only`, `--category/--severity/--bugzilla/--cve` | patch selection in zypper via `ResPoolProxy`/`Patch` API; `setUpdateMode` | `commands/patch.cc`, `update.cc` |
| `verify` | `Resolver::verifySystem()` | `solve-commit.cc verify` |
| `purge-kernels` | `zypp::PurgeKernels` | `commands/...` |
| `ps` | `zypp::CheckAccessDeleted` (lock-free) | `commands/ps.cc` |
| `--oldpackage` | allow downgrades for explicitly requested packages | `commands/installremove.cc` |
| `--auto-agree-with-licenses` / `-l` | zypper-side license prompts (`Package::licenseToConfirm()`) | `commands/optionsets.cc` |

Commit result → exit code (`solve-commit.cc`, around `God->commit(policy)`): `!result.allDone() && !(dryRunEtc && result.noError())` gives **8 `ZYPPER_EXIT_ERR_COMMIT`** if `result.attemptToModify()` is true, otherwise **4**. Patches that need a reboot give **102**. A restart of the package manager itself (update-stack patch) gives **103**. `%post` script failures give **107** (`ZYPPER_EXIT_INF_RPM_SCRIPT_FAILED`). A signal during commit gives **8** (bsc#946750), otherwise **105**. Skipped repos give **106**. The full list is in `zypper/src/main.h`. Scripts consume these codes (connect-ng checks 100–106), so **anything in libzypp that changes `ZYppCommitResult::allDone()/noError()/attemptToModify()` semantics changes zypper exit codes**.

### 2.6 Callbacks zypper installs and its default/non-interactive answers

Receivers are static objects created in `main.cc` (`static RpmCallbacks rpm_callbacks;` ...). Each wrapper class `connect()`s in its constructor. Remember that **only one receiver per report type can be connected** (`zypp/zypp/Callback.h`: `connect()` disconnects any previous receiver), so a library that installs its own receiver silently steals the reports.

In non-interactive mode `get_prompt_reply()` returns `PromptOptions::defaultOpt()` and logs `running non-interactively, returning ...`. `read_action_ari_with_timeout()` sleeps *timeout* seconds and then returns the default. If there is no tty and it is not non-interactive, zypper exits with code 4 ("Cannot read input: bad stream or EOF").

| Report (libzypp) | zypper receiver (`src/callbacks/`) | Default / `-n` answer | Flag overrides |
|---|---|---|---|
| `KeyRingReport::askUserToAcceptUnsignedFile` | `keyring.h KeyRingReceive` | **no** (false) | `--no-gpg-checks` → true + warning |
| `askUserToAcceptUnknownKey` | ″ | **no** | `--no-gpg-checks` → true |
| `askUserToAcceptKey` | ″ | **`KEY_DONT_TRUST`** (option 0 = reject) | `--gpg-auto-import-keys` → `KEY_TRUST_AND_IMPORT`; `--no-gpg-checks` → `KEY_TRUST_TEMPORARILY`. A non-root user without `--root` is not offered "trust always" |
| `askUserToAcceptVerificationFailed` | ″ | **no** | `--no-gpg-checks` → true (QUIET warning) |
| `KeyRingReport::report(UserData)` with type `KeyRingReport/AcceptPackageKey` | ″ `askUserToAcceptPackageKey` | sets `"TrustKey"` = (answer == TRUST_AND_IMPORT). No temporary trust | `--gpg-auto-import-keys` |
| type `KeyRingReport/KeysNotImported` (key `"Keys"`: `std::set<Edition>`) | ″ | info only (old V3 rpm keys) | |
| type `KeyRingReport/reportAutoImportKey` (keys `KeyDataList`, `KeySigning`, `KeyContext`) | ″ | info only | |
| `KeyRingReport::infoVerify` | ″ | warns if expired or < 15 days left | |
| `DigestReport::askUserToAcceptNoDigest / UnknownDigest` | `DigestReceive` | default = `no_gpg_checks` | |
| `DigestReport::askUserToAcceptWrongDigest` | ″ | prompt "type first 4 chars of checksum to unblock"; default **discard** | |
| `repo::DownloadResolvableReport::problem` | `repo.h` | **ABORT** (a/r/i) | |
| `DownloadResolvableReport::pkgGpgCheck(UserData)` | ″ | reads `CheckPackageResult`, `CheckPackageDetail`, `Localpath`, `ResObject`; on error **`userData.reset("Action")`** = "use the default problem report" | `--no-gpg-checks` → `set("Action", IGNORE)` |
| `repo::RepoReport::problem` (**deprecated type, still used**) | ″ | ABORT | |
| `ProgressReport` | ″ | progress bars only | |
| `media::MediaChangeReport::requestMedia` | `media.cc` | CD/DVD: "insert medium" y/n, default **no** → ABORT. `IO_SOFT`: a/r/i with a 30 s timeout, default **RETRY** until the repeat limit for the same URL, then ABORT. https: special handler. Others: a/r/i/u, default ABORT | |
| `media::DownloadProgressReport` | `media.h` | `problem()` → ABORT; `NO_ERROR` → info + IGNORE; `progress()` returns false when an exit is requested | |
| `media::CommitPreloadReport` | `media.h` | reads UserData keys `bytesReceived`, `bytesRequired`, `dbps_current`, `dbps_avg`, `Url` | |
| `media::AuthenticationReport::prompt` | `media.cc` | **non-interactive: returns false (abort)**. Machine-readable mode reads user/password from stdin | |
| `target::rpm::InstallResolvableReport` / `RemoveResolvableReport` (classic rpm) | `rpm.h` | `problem()` → ABORT; uses only the **first line** of the description. `finish(error)` → exit 4. `report()` handles `contentRpmout` with key `"line"` as **`std::reference_wrapper<const std::string>`** and scans for `^(warning\|error): %.* scriptlet failed, ` → 107 | |
| `target::rpm::SingleTransReport` | `rpm.h` | `contentLogline`: `"line"` (`reference_wrapper`), `"level"` (`loglevel`) | |
| `InstallResolvableReportSA / RemoveResolvableReportSA / CommitScriptReportSA / TransactionReportSA / CleanupPackageReportSA` | `rpm.h` | `report()` reads `"line"` via `get("line", std::string&)` (**value type `std::string`**). `CommitScriptReportSA::finish(WARN)` → 107 | |
| `target::PatchMessageReport / PatchScriptReport` | `rpm.h` | script problem → ABORT | |
| `target::FindFileConflictstReport` | `rpm.h` | prompts to continue on file conflicts (default no) | |
| `JobReport::message` | `job.h` | `cmdout/%posttrans` (optional `"ripoff"` headline as `reference_wrapper`), any `cmdout/*` highlighted, `reporefresh/mirrorlist` with key `"error"` (`zypp::Exception`), `cmdout/monitor` handshake (`"?"` → `reset("!")`) | |
| `SavingLocksReport`, `CleanEmptyLocksReport` | `locks.h` | yes/no prompts | |

Note the swapped member names in `RpmCallbacks` (`_installReceiver` is the *Remove* receiver). This is harmless, but don't let it confuse you when debugging.

**Contract details zypper depends on, which are easy to break from the libzypp side:**
- `UserData` values are `boost::any`, and `get<T>()` must match the exact type. Classic rpm output and `contentLogline` send `std::cref(line)` (`zypp/zypp/target/rpm/RpmDb.cc` ~1756, `TargetImpl.cc` ~2056). The SA reports send the `std::string` by value (`TargetImpl.cc` ~2311). Changing either one breaks consumers at runtime: an exception, or silently missing output.
- `contentRpmout`/`contentLogline` constants are defined in `zypp/zypp/ZYppCallbacks.cc` (`"rpmout","installpkg"`, `"zypp-rpm","logline"`, `"zypp-rpm","installpkgsa"`, ...). `JobReport` has `"reporefresh","mirrorlist"`. `RpmPostTransCollector.cc` emits `"cmdout","%posttrans"`.
- The `pkgGpgCheck` tri-state on `"Action"` (has a value = user decision / key with no value = request default problem report / no key = OK) is handled in `zypp/zypp/repo/PackageProvider.cc` ~220–270.
- `problem()` descriptions: zypper prints **only the first line**, because the rest has already been streamed via `report()`.
- zypper parses rpm stdout text for `scriptlet failed` in classic mode.

### 2.7 Exit codes related to libzypp

```
 4 ZYPPER_EXIT_ERR_ZYPP     getZYpp() failure (non-lock), commit errors without modification,
                            rpm finish(error), refresh failure with FailIfReposFail, no tty
 7 ZYPPER_EXIT_ZYPP_LOCKED  ZYppFactoryException from getZYpp()
 8 ZYPPER_EXIT_ERR_COMMIT   commit attempted to modify the system and failed; or signal during commit
100/101                     updates / security updates needed (list-updates/patch-check)
102/103                     reboot / package-manager restart needed (Patch::rebootSuggested/restartSuggested)
106                         repos skipped (refresh errors)
107                         rpm %script failed (classic: stdout parse; single-trans: CommitScriptReportSA WARN)
```

---

## 3. YaST: yast2-pkg-bindings and the libyui package selectors

### 3.1 yast2-pkg-bindings (github.com/yast/yast-pkg-bindings, `src/`)

This is a C++ YCP component that exposes `Pkg.*` to Ruby YaST. What was verified by grepping `Callbacks.cc`, `PkgFunctions.{h,cc}`, `Package.cc`, `Target*.cc`, `Resolvable_*.cc`, `Source_*.cc`, `ServiceManager.cc`, `Keyring.cc`:

**Instance and lock** (`PkgFunctions::zypp_ptr()`): it loops `zypp::getZYpp()` with sleep and retry, and rethrows on the last attempt. `ZYppFactoryException` is caught separately for the error message. Right after acquiring it calls `resolver()->setIgnoreAlreadyRecommended(true)` ("be compatible with zypper"). **The pointer is kept for the module's lifetime**, so YaST holds the lock for the whole session.

**Target**: `zypp_ptr()->initializeTarget(root, rebuild_rpmdb)`, `target()->load()`, `finishTarget()` (`Target_Load.cc`). Also direct `target()->rpmDb().installPackage() / removePackage() / rebuildDatabase()` (`Target.cc`). Disk usage uses `DiskUsageCounter` and `ZYpp::setPartitions()/diskUsage()` (`Target_DU.cc`). `ZYppCommitPolicy` + `zypp_ptr()->commit(*policy)` wrapped in `OldStyleCommitResult`. It catches `target::TargetAbortedException`.

**Repos/services**: `RepoManager` built from `RepoManagerOptions(_target_root)` and **recreated when the target root changes** (`RepoManagerUpdateTarget`). It uses `refreshMetadata`, `buildCache`, `loadFromCache`, `ServiceInfo`, `refreshService`, `RepoVariables*` (`RepoVariablesStringReplacer` export, bsc#1228138), `KeyRing`/`importKey` (`Keyring.cc`), `Fetcher`, `repo::PackageProvider`.

**Pool/selection**: `ResPoolProxy`, `ui::Selectable`, `ResStatus` with causers **`USER`, `APPL_HIGH`, `APPL_LOW`, `SOLVER`** (`Package.cc`, `Resolvable_Install.cc`, `Resolvable_Properties.cc`), `setTransact`, `setToBeInstalled`, `setSoftLock`, `Product::`/`ProductFileReader`, `PoolQuery`, `sat::WhatProvides`, `sat::LocaleSupport`, `VendorAttr`, `ResFilters`.

**Solver**: `resolvePool`, `verifySystem`, `doUpgrade` (preceded by `setIgnoreAlreadyRecommended(false)`), `createSolverTestcase`, and a generic `set_solver_flag()` table over `Resolver` getters/setters, including `resetOnlyRequires`. The DUP flags (`dupAllowArchChange`, `dupAllowDowngrade`, `dupAllowNameChange`, `dupAllowVendorChange`) are behind `#ifdef HAVE_ZYPP_DUP_FLAGS`, a configure-time feature probe on libzypp headers.

**Callbacks** (`ReceiveReport<>` in `Callbacks.cc`; all forwarded to YCP/Ruby handlers): `KeyRingReport`, **`KeyRingSignals`**, `DigestReport`, `ProgressReport`, `repo::DownloadResolvableReport` (including `pkgGpgCheck`: maps Ruby "A"/"I"/"R" to `userData.set("Action", ABORT/IGNORE/RETRY)`), `media::DownloadProgressReport`, `media::MediaChangeReport`, `media::AuthenticationReport`, `target::rpm::InstallResolvableReport`, `InstallResolvableReportSA`, `RemoveResolvableReport`, `RebuildDBReport`, `target::PatchMessageReport`, `PatchScriptReport`, `target::FindFileConflictstReport`. It also reads `ZConfig::instance().textLocale()`.

Implication: YaST still uses the **classic** `InstallResolvableReport` and `RebuildDBReport`, and does its own media-change UI for **multi-medium installs** (`MediaChangeReport` with `devices`/`index`). Installer flows (`/mnt` root, `RepoManagerOptions(root)`, `ZConfig` announcements) must keep working with a target root that differs from `/` and that changes during the session.

### 3.2 libyui-qt-pkg / libyui-ncurses-pkg (github.com/libyui/libyui)

Includes (count over both `src/` trees): `zypp/ui/Selectable.h` 9, `zypp/ZYppFactory.h` 8, `ResObject.h` 5, `Resolver.h` 5, `Patch.h`, `Product.h`, `PoolQuery.h` 4, `sat/LocaleSupport.h`, `RepoManager.h` 3, **`zypp/base/Sysconfig.h`** (compat header), **`zypp/SysContent.h`**, **`zypp/ui/UserWantedPackages.h`**, `zypp/ui/Status.h`, `VendorSupportOptions.h`, `ServiceInfo.h`, `Changelog.h`, `ResolverProblem.h`, `ProblemSolution.h`, `DiskUsageCounter.h`, `parser/HistoryLogReader.h` (qt).

APIs used:
- `getZYpp()`, `ResPoolProxy` with `saveState() / restoreState() / diffState()` (cancel/accept in the selector).
- `ui::Selectable::status()/setStatus()` with the full `ui::Status` enum: `S_Install, S_AutoInstall, S_Update, S_AutoUpdate, S_Del, S_AutoDel, S_NoInst, S_KeepInstalled, S_Taboo, S_Protected`.
- `ResStatus::USER/APPL_LOW/APPL_HIGH/SOLVER`, `ResStatus::TransactByValue`.
- Solver: `resolvePool`, `verifySystem`, `createSolverTestcase`, `ResolverProblemList`/`ProblemSolutionList` UI, `setOnlyRequires/onlyRequires`, `setCleandepsOnRemove`, `setIgnoreAlreadyRecommended`, `setSystemVerification`, `setAllowVendorChange`, `dupSetAllowVendorChange`.
- Locales: `sat::LocaleSupport`, `addRequestedLocale`, `ResPool::instance().getAvailableLocales()`. Also `ResPool::byKindBegin/End`, `DiskUsageCounter`/`diskUsage()`, `HistoryLogReader`.

They get no callbacks of their own; YaST/pkg-bindings own those. They are very sensitive to **`ui::Status` / `Selectable` semantics** (what counts as "auto", taboo/protected mapping to locks) and to `ResPoolProxy` state save/restore.

---

## 4. PackageKit zypp backend

Source: `backends/zypp/pk-backend-zypp.cpp` (~4300 lines, the only file besides `meson.build`).

**Long-running daemon, lock dropping.** Every job builds a `ZyppJob` RAII object (lines ~631–653). It locks a process-wide `pthread_mutex_t zypp_mutex` (libzypp is not thread-safe, and PK runs jobs on threads), calls `pk_backend_job_set_locked`, and points the `EventDirector` (the callback receivers) at the job. `ZyppJob::get_zypp()` calls `ZYppFactory::instance().getZYpp()` and keeps the `ZYpp::Ptr` **only for the job's duration**. When the job ends the pointer dies, `~ZYpp` releases `/run/zypp.pid`, and zypper can run between PK jobs. `initializeTarget("/")` is called **once per process** (`static gboolean initialized`) because `ZYpp::Impl` is a never-destroyed static in libzypp (§2.3). Errors: `ZYppFactoryException` → `PK_ERROR_ENUM_FAILED_INITIALIZATION` (with `asUserString()`), other `Exception` → `PK_ERROR_ENUM_INTERNAL_ERROR`.

**This is the most important hidden contract with PK:** dropping and re-acquiring the `ZYpp` instance must keep the pool, target and repos intact, and must not double-initialize. PK also re-reads repo variables per job: it sets `ZYPP_REPO_RELEASEVER` with `g_setenv` around upgrade-system jobs and relies on `repoVariablesReset()` in the `ZYpp` constructor.

**Pool management** (`zypp_build_pool`): it loads the target (`target->load()`) only if the `@System` repo is empty, and erases `@System` from the pool when local packages are not wanted. Repos are loaded once per process (`static repos_loaded`) via `RepoManager().loadFromCache()`, skipping repos that are not cached. Then `ZConfig::apply_locks_file()` → `Locks::instance().readAndApply()`. Refresh (`zypp_refresh_cache`) calls `sat::Pool::reposEraseAll()` when pool density < 33 %, `initializeTarget` if there is no target, otherwise **`target->rpmDb().exportTrustedKeysInZyppKeyRing()`**, then `refreshMetadata`/`buildCache`/`loadFromCache`. `PoolStatusSaver` wraps operations in `ResPoolProxy::saveState()/restoreState()` (bnc#804054: must be created after the pool is built, or the locks are lost).

**Solver/commit**: `setForceResolve`, `setIgnoreAlreadyRecommended(TRUE)`, `resolvePool`, `doUpdate`, `doUpgrade` with `dupSetAllowVendorChange(ZConfig::solver_dupAllowVendorChange())`, `setUpgradeMode/setUpdateMode(FALSE)` resets, `setCleandepsOnRemove(autoremove)`. Patches use `isNeeded()`, `candidateObj().isUnwanted()`, `isCategory(Patch::CAT_SECURITY)`, `restartSuggested()`, `rebootSuggested()`; patches that touch the update stack are handled first. Commit uses `ZYppCommitPolicy` with `restrictToMedia(0)`, `downloadMode(DownloadOnly | DownloadInHeaps)`, `syncPoolAfterCommit(true)`, and **`rpmNoSignature(true)` unless `ONLY_TRUSTED`**. Success is `result.allDone()`. Status is changed with `setToBeInstalled/setToBeUninstalled/resetTransact(ResStatus::USER)`. It searches with `PoolQuery`, and has a temporary repo `"PK_TMP_DIR"` for local files.

**Callbacks** (non-interactive by nature): `InstallResolvableReport`, `RemoveResolvableReport` (**classic types only, no SA receivers**), `ProgressReport`, **`repo::RepoReport` (deprecated)**, `DownloadResolvableReport`, `MediaChangeReport`, `KeyRingReport`, `DigestReport`, `CommitPreloadReport`. The KeyRing answers are delegated to `zypp_signature_required()` (PK's own signature workflow), and accepting a key means `KEY_TRUST_AND_IMPORT`. A source comment says the KeyRing callbacks are "called only from zypp_refresh_meta_and_cache, otherwise uncaught Abort exceptions".

**Environment**: it sets `PATH` and `http_proxy`/`https_proxy`/`ftp_proxy`/`socks_proxy`/`no_proxy`/`pac` via `g_setenv` at runtime (proxy settings from PK), so libzypp's curl/proxy code **must read the proxy env per request, not cache it at startup**. Not verified on the libzypp side: check `zypp-curl` proxy handling and libproxy (dynamically resolved since 17.33.0) before changing it. Logging: `LogControl::instance().logfile("/var/log/pk_backend_zypp")`, with PK rotating it itself at 10 MB, and log group `"packagekit"` via `ZYPP_BASE_LOGGER_LOGGROUP`.

zypper's side of the relationship: `ZYppFactoryException::lockerName()` contains `packagekitd`, and zypper offers to ask PK to quit (§2.3).

---

## 5. Other verified consumers

| Consumer | How it uses libzypp | What it relies on |
|---|---|---|
| **Agama** (`agama-project/agama`, `rust/zypp-agama/zypp-agama-sys/c-layer/lib.cxx`, `callbacks.cxx`) | A thin C layer over the C++ API, wrapped by safe Rust (`zypp-agama`) and used by `agama-software` (`zypp_server.rs`). The README says "libzypp is not thread safe". | `setenv("ZYPP_LOCKFILE_ROOT", root)` before `getZYpp()` (lock inside the target, e.g. `/mnt`); `initializeTarget(root, false)`; `RepoManagerOptions(root)` and moving the package cache out of tmpfs (cites pkg-bindings); `refreshMetadata/buildCache/loadFromCache/refreshService`; `ui::Selectable`, `ResStatus::USER/APPL_HIGH/SOLVER`, `TransactByValue`; `sat::Pool::setRequestedLocales`; `setOnlyRequires`; `resolvePool`; `ZYpp::setPartitions` + `DiskUsageCounter`; `ZYppCommitPolicy::downloadMode(DownloadInAdvance)`; `ZConfig::set_download_media_prefer_download`; custom `LogControl::setLineFormater/setLineWriter` (journal). Callbacks: `ProgressReport`, `media::DownloadProgressReport`, `repo::DownloadResolvableReport`, `media::CommitPreloadReport`, `KeyRingReport` (`KEY_TRUST_TEMPORARILY`/`KEY_TRUST_AND_IMPORT`), `DigestReport`, `target::PatchScriptReport`, `target::rpm::InstallResolvableReport`. Includes **`zypp-core/ui/progressdata.h`** (a newer internal path), `zypp/RepoManagerFlags.h`, `zypp/ZYppCommit.h`, `zypp-core/Pathname.h`, `zypp-core/Url.h`. |
| **snapper-zypp-plugin** (`openSUSE/snapper`, `zypp-plugin/`) | Native C++ commit plugin speaking the plugin protocol over stdin/stdout; it does **not** link libzypp. | Frames `PLUGINBEGIN`, `PLUGINEND`, `COMMITBEGIN`, `COMMITEND` and their JSON body (`TransactionStepList`, `type`, `stage`, `solvable`, `userdata`). `libzypp(plugin:commit) = 1`. Env `DISABLE_SNAPPER_ZYPP_PLUGIN`, `SNAPPER_ZYPP_PLUGIN_*`. The `userdata` comes from `ZConfig::setUserData` (`zypper --userdata`). See `zypp/doc/autoinclude/Plugin-Commit.doc`. |
| **python3-zypp-plugin** | Python helper library for writing plugins (verified only through libzypp's spec). | `Conflicts: python2/3-zypp-plugin < 0.6.4` because of bsc#1227793: old versions reject STOMP headers containing `-`. **Adding a new header name to plugin frames can break old plugin libraries.** |
| **SUSEConnect / connect-ng** (`SUSE/connect-ng`, `internal/zypper/zypper.go`) | Calls **the zypper CLI**, not libzypp. | zypper options `--root`, `--non-interactive`, `--xmlout`, `--no-refresh`, `--gpg-auto-import-keys`, `--releasever`, `addservice/removeservice/ref/products`, `--auto-agree-with(-product)-licenses`, `--updatestack-only`, and exit codes 100–106. Indirect: any change to libzypp service/repo semantics (`ServiceInfo`, repoindex, credentials) or to the commit result that changes zypper's exit codes. Ships `zypper-migration` and `zypper-search-packages` as zypper subcommands. |

Not verified, and so not documented: ruby/python SWIG bindings (`libzypp-bindings`, believed unmaintained), salt/ansible modules (they use the zypper CLI with `--xmlout`), SUSE Manager/Uyuni plugins.

---

## 6. API area → consumers → risk

Risk legend: **H** = several consumers, or a silent behavioural dependency, or ABI-critical. **M** = one or two consumers, or a compile-time break that shows up in OBS. **L** = zypper-only, so it can be changed in lockstep.

| API area | zypper | YaST bindings | libyui | PK | Agama | Risk |
|---|---|---|---|---|---|---|
| `getZYpp()` / `ZYppFactoryException` (`lockerName`, `lockerPid`) / lock semantics | ✔ | ✔ (retry loop) | ✔ | ✔ (drop/reacquire) | ✔ | **H** |
| `ZYPP_LOCKFILE_ROOT`, `ZYPP_LOCK_TIMEOUT`, `ZYPP_READONLY_HACK` + `zypp_readonly_hack::IWantIt()` | ✔ | ? | | | ✔ (LOCKFILE_ROOT) | **H** |
| `ZYpp::initializeTarget/finishTarget/target()/getTarget()`, `Target::load()` | ✔ | ✔ | | ✔ (once) | ✔ | **H** |
| `RepoManager`/`RepoManagerOptions(root)`/`RepoInfo`/`ServiceInfo` refresh+cache | ✔ | ✔ | ✔ (read) | ✔ | ✔ | **H** |
| `ResPool`/`ResPoolProxy` (`saveState/restoreState/diffState`) | ✔ | ✔ | ✔ | ✔ | ✔ (proxy) | **H** |
| `ui::Selectable`, `ui::Status` | ✔ | ✔ | ✔✔ | ✔ | ✔ | **H** |
| `ResStatus` causers (`USER/APPL_HIGH/APPL_LOW/SOLVER`), `TransactByValue`, `setTransact/setToBe*/setSoftLock/resetTransact` | ✔ | ✔ | ✔ | ✔ | ✔ | **H** |
| `Resolver` flags (`setOnlyRequires`, `setIgnoreAlreadyRecommended`, `setForceResolve`, `setCleandepsOnRemove`, `(dup)setAllow*`, `setUpdateMode/setUpgradeMode`, `setFocus`, `setRemoveOrphaned`, `setSystemVerification`) | ✔ | ✔ | ✔ | ✔ | ✔ | **H** |
| `resolvePool/verifySystem/doUpdate/doUpgrade/addUpgradeRepo`, `ResolverProblem/ProblemSolution`, `createSolverTestcase` | ✔ | ✔ | ✔ | ✔ | ✔ | **H** |
| `ZYppCommitPolicy` (`downloadMode`, `dryRun`, `restrictToMedia`, `rpmNoSignature`, `syncPoolAfterCommit`, `replaceFiles`) / `ZYppCommitResult` (`allDone/noError/attemptToModify/singleTransactionMode`) | ✔ | ✔ | | ✔ | ✔ | **H** |
| `KeyRingReport` (incl. `report(UserData)` content types), `DigestReport` | ✔ | ✔ | | ✔ | ✔ | **H** (security) |
| `DownloadResolvableReport` incl. `pkgGpgCheck` `"Action"` protocol | ✔ | ✔ | | ✔ | ✔ | **H** |
| `MediaChangeReport`, `DownloadProgressReport`, `AuthenticationReport`, `CommitPreloadReport` (UserData keys) | ✔ | ✔ | | ✔ (no Auth) | ✔ | **H** |
| Classic `Install/RemoveResolvableReport` (+`contentRpmout`) | ✔ | ✔ | | ✔ | ✔ | **H**: PK/Agama have no SA receivers, so in single-trans mode they may see nothing. See [08](08-target-commit-and-rpm.md) for which reports single-trans sends |
| `*ReportSA`, `SingleTransReport` | ✔ | ✔ (InstallSA) | | | | M |
| `RepoReport` (`ZYPP_DEPRECATED`), `ProgressReport` | ✔ | ✔ (Progress) | | ✔ | ✔ | M (the "deprecated" type cannot be removed) |
| `KeyRingSignals`, `RebuildDBReport` | | ✔ | | | | M |
| `JobReport` content types (`cmdout/%posttrans`, `cmdout/monitor`, `reporefresh/mirrorlist`) | ✔ | | | | | L |
| `Patch` API (`isNeeded`, categories, `rebootSuggested/restartSuggested`, update-stack detection) | ✔ | ✔ | ✔ | ✔ | | **H** (drives exit codes 100–103 and PK self-update) |
| `PoolQuery`, `sat::WhatProvides`, `Capability` parsing | ✔ | ✔ | ✔ | ✔ | | M |
| `sat::LocaleSupport`, requested locales | ✔ | ✔ | ✔ | | ✔ | M |
| `DiskUsageCounter`, `ZYpp::setPartitions/diskUsage` | | ✔ | ✔ | | ✔ | M |
| `Locks` (`/etc/zypp/locks`), `ZConfig::apply_locks_file` | ✔ | ? | | ✔ | | M |
| `target::rpm::RpmDb` direct (`installPackage`, `removePackage`, `rebuildDatabase`, `exportTrustedKeysInZyppKeyRing`), `RpmHeader`, `librpmDb` | ✔ | ✔ | | ✔ | | M |
| `ZConfig` getters/setters (paths, `setRepoManagerRoot`, `announceSystemRoot`, `setUserData`, `solver_*`, `commit_downloadMode`, `textLocale`, `download_media_prefer_download`) | ✔ | ✔ | ✔ | ✔ | ✔ | **H** |
| `LogControl` (`logfile`, `setLineWriter/Formater`) and log line format | ✔ (+zypper-log) | (YaST own) | | ✔ | ✔ | M |
| `RepoVariablesStringReplacer`, `ZYPP_REPO_RELEASEVER` | ✔ (`--releasever`) | ✔ | | ✔ | | M |
| Exotic: `SysContent`, `ui::UserWantedPackages`, `VendorSupportOptions`, `HistoryLogReader`, `Changelog`, `ProductFileReader`, `PurgeKernels`, `CheckAccessDeleted` | ✔ (some) | ✔ (some) | ✔ | ✔ (ProductFileReader) | | M (rarely tested, easy to break unnoticed) |
| `zypp-tui` | ✔ | | | | | L |
| Plugin protocol (commit/services/urlresolver/system/...) | | | | | | **H** (snapper, SUSE Manager, python plugins) |
| Compat include paths (`zypp/zypp-compat/`) | | ✔ | ✔ | ✔ | | **H** (compile-time) |

---

## 7. "Never change" behaviours (without a coordinated plan)

1. **The global lock**: `/run/zypp.pid` under `$ZYPP_LOCKFILE_ROOT`, PID-content format, root-only locking, `zypp-rpm.pid` check, `ZYPP_LOCK_TIMEOUT` semantics (0 = fail fast, < 0 = forever).
2. **`ZYppFactoryException::lockerName()` = argv[0] from `/proc/PID/cmdline`.** zypper matches `"packagekitd"` in it.
3. **Lock is released when the last `ZYpp::Ptr` dies, but `ZYpp::Impl` (target, pool) survives**, and `repoVariablesReset()` runs on re-acquire. PK depends on this.
4. **`zypp_readonly_hack::IWantIt()` / `ZYPP_READONLY_HACK`** skip the lock and make RpmDb/RepoManager read-only.
5. **Exactly one receiver per report type**: `connect()` replaces. Adding an internal receiver in libzypp steals the consumer's reports.
6. **Callback default return values** in `ReportBase`-derived structs (`ZYppCallbacks.h`, `KeyRing.h`). Consumers that do not override a method get the library default, which is what PK and Agama do for many methods. Changing a default (e.g. `askUserToAcceptKey` → trust) is a **security change for every consumer**.
7. **UserData content types and key names/value types** listed in §2.6 (`"line"` as `std::cref` vs `std::string`, `"Action"` tri-state, `"TrustKey"`, `"KeyDataList"/"KeySigning"/"KeyContext"`, `"Keys"`, `"bytesReceived"...`, `"CheckPackageResult"/"CheckPackageDetail"/"Localpath"/"ResObject"`).
8. **`problem()` returns are obeyed literally** (ABORT/RETRY/IGNORE). RETRY loops must stay bounded on the library side only if the consumer bounds them (zypper's media repeat counter).
9. **Solver defaults that consumers set expecting the opposite default**: everyone sets `setIgnoreAlreadyRecommended(true)`. YaST and zypper use `dupSet*` vs `set*` differently. `onlyRequires` defaults come from `zypp.conf` (zypper removed its own `installRecommends` in 1.14.99, so zypp.conf is now the single source).
10. **`ResStatus` causer ordering** (`USER > APPL_HIGH > APPL_LOW > SOLVER`) and transact-by-value rules. YaST and libyui encode UI semantics on them.
11. **`ui::Status` enum values and their mapping** (taboo/protected ↔ locks, Auto* ↔ solver causer).
12. **`ZYppCommitResult::allDone()/noError()/attemptToModify()` semantics.** They drive zypper exit codes 4 vs 8 and PK success.
13. **`Patch::rebootSuggested()/restartSuggested()/isNeeded()`** and update-stack ("restart package manager") semantics. They drive exit codes 102/103 and PK self-update ordering.
14. **Public headers stay C++11-compilable** (YaST/PK on older code streams) and **`ZYPP_API` exports are never removed** (removal is ABI-breaking: bump COMPATMINOR and soname, see [12](12-release-packaging-and-compat.md)).
15. **`zypp-compat` include paths** (`zypp/base/*.h`, `zypp/Pathname.h`, ...) must keep working.
16. **Plugin frame names and header syntax** (`PLUGINBEGIN`, `COMMITBEGIN`, ...), including not introducing header names that old python3-zypp-plugin cannot parse.
17. **`libzypp(...)` feature Provides** are never lowered or removed once published.
18. **Proxy and env are read at runtime** (PK sets `http_proxy` etc. per job; PK/zypper set `ZYPP_REPO_RELEASEVER`). Not verified in code; see §4.

---

## 8. Checklist: before changing X, check Y

| If you change… | …check |
|---|---|
| `ZYppFactory`/lock code | zypper `Zypper.cc assertZYppPtrGod` (PK detection, exit 7); PK `ZyppJob` (drop/reacquire, `static initialized`); YaST `zypp_ptr()` retry loop; Agama `ZYPP_LOCKFILE_ROOT`; `zypp-refresh.cc` |
| Any `*Report` struct in `ZYppCallbacks.h` / `KeyRing.h` (signature, default, enum) | zypper `src/callbacks/*.h`; pkg-bindings `Callbacks.cc`; PK receivers; Agama `callbacks.cxx`. A changed virtual signature **silently stops the override from being called** (none of them uses `override` everywhere) |
| What sends a report (e.g. classic vs single-trans, a new `report(UserData)` type) | PK and Agama only handle **classic** `Install/RemoveResolvableReport`; YaST handles `InstallResolvableReportSA` but no other SA. zypper prints unknown `KeyRingReport::report` types as a `WAR` only |
| `UserData` keys or value types | `grep -rn 'get<\|haskey\|hasvalue' zypper/src/callbacks` plus the external callback files listed above |
| Keyring/signature defaults or flow ([06](06-security-gpg-keyring.md)) | zypper `--no-gpg-checks` / `--gpg-auto-import-keys` matrix; PK `zypp_signature_required` + `rpmNoSignature`; `pkgGpgCheck` `"Action"` handling in `PackageProvider.cc` |
| Solver flag defaults or meaning ([04](04-solver.md)) | zypper `solve-commit.cc set_solver_flags`; pkg-bindings `set_solver_flag` table and `HAVE_ZYPP_DUP_FLAGS`; libyui menu toggles (`setSystemVerification`, `setAllowVendorChange`, `dupSetAllowVendorChange`); PK's `dupSetAllowVendorChange(ZConfig::solver_dupAllowVendorChange())` |
| `ResStatus`/`ui::Status`/`ResPoolProxy` save/restore ([03](03-sat-pool-and-resolvables.md)) | libyui Qt + ncurses status strategies; pkg-bindings `Resolvable_*.cc`; PK `PoolStatusSaver` (bnc#804054) |
| `ZYppCommitPolicy` / `ZYppCommitResult` / download modes ([08](08-target-commit-and-rpm.md)) | zypper exit-code mapping (`solve-commit.cc` ~970) and `DownloadAsNeeded` vs single-trans hint; PK `DownloadInHeaps`/`DownloadOnly`, `restrictToMedia(0)`; Agama `DownloadInAdvance`; YaST `OldStyleCommitResult` |
| `initializeTarget` / root handling / `ZConfig` roots ([09](09-configuration-and-environment.md)) | zypper `--root` vs `--installroot` (`setRepoManagerRoot("/")`, `announceSystemRoot`); YaST target switching (`RepoManagerUpdateTarget`); Agama `/mnt` + cache relocation; PK calling `initializeTarget` once |
| `RepoManager` refresh/cache/services ([05](05-repositories-and-services.md)) | PK's "repos loaded once" + pool density heuristic; connect-ng via `zypper addservice/ref`; YaST `Source_*.cc`; zypper exit 106 / `FailIfReposFail` |
| `Patch` attributes / categories | zypper `patch`, `list-patches`, `patch-check` exit codes 100/101; PK `get-updates`, security and self-update detection |
| Log format / `LogControl` | `zypper-log` tool; PK file rotation; Agama custom formatter/writer |
| Plugin protocol ([doc/autoinclude/Plugin-*.doc]) | snapper-zypp-plugin tests (`zypp-plugin/testsuite`); python3-zypp-plugin header parsing (bsc#1227793); bump the `libzypp(plugin:...)` Provide |
| Removing/hiding a symbol or header | grep zypper, pkg-bindings, libyui, PK, Agama (list in §2.2, §3, §4, §5); `zypp-compat` forwarding; OBS rebuild of all reverse deps (`osc whatdependson`) |
| `zypp-tui` | zypper only; bump `libzypp-tui-devel` Provide if incompatible |

---

## 9. Coordinating releases with zypper and other consumers

The version mechanics (`zypp/VERSION.cmake`: MAJOR 17 / MINOR 38 / PATCH 16 / COMPATMINOR 35, soname, changelog conventions) are covered in [12](12-release-packaging-and-compat.md). The consumer-facing parts:

1. **New API needed by zypper.** Release libzypp first. In zypper, raise `BuildRequires: libzypp-devel >= X.Y.Z` in `zypper/zypper.spec.cmake` and mention it in `zypper/package/zypper.changes` (example: 1.14.99 "BuildRequires: libzypp-devel >= 17.38.0. Code uses some of the new libzypp log and string tools"). Runtime is covered automatically by `%requires_ge libzypp`. If zypper has to build against both old and new libzypp (maintenance code streams), use `#if LIBZYPP_VERSION > NNNNNN` as in `zypper/src/info.cc`.
2. **libzypp change that requires a newer zypper** (zypper used something that became private, or behaviour changed incompatibly): add `Conflicts: zypper < N` to `zypp/libzypp.spec.cmake` and the changelog (history: `<= 1.14.76` in 17.35.10, then `< 1.14.91`). Do the same for other consumers if needed (the `python3-zypp-plugin < 0.6.4` precedent).
3. **Feature tests for packages and scripts.** Add or raise `Provides: libzypp(FEATURE) = VERSION` in `zypp/libzypp.spec.cmake` (comment: "Features we provide (update doc/autoinclude/FeatureTest.doc)") **and** document it in `zypp/doc/autoinclude/FeatureTest.doc`. Current provides: `plugin 0.1`, `plugin:appdata 0`, `plugin:commit 1`, `plugin:services 1`, `plugin:system 1`, `plugin:urlresolver 0`, `plugin:repoverification 0`, `plugin:sigcheck 0`, `repovarexpand 1.1`, `econf 0`. Note that **`plugin:appdata` is provided but has no entry in FeatureTest.doc** (it is documented in `ApplicationMetada.doc`). Fix this the next time you touch the file. Consumers `Requires: libzypp(plugin:commit) >= 1` etc. zypper publishes its own `zypper(...)` feature provides the same way.
4. **Compile-time feature probes by consumers**: pkg-bindings' `HAVE_ZYPP_DUP_FLAGS`, and zypper's `LIBZYPP_VERSION`. Keep header and macro names stable so these probes keep working.
5. **ABI exports**: after any change touching `ZYPP_API`, rebuild the reverse dependencies (yast2-pkg-bindings, libyui-qt-pkg, libyui-ncurses-pkg, PackageKit, zypper, Agama) in an OBS staging/devel project. The 17.33.x "missing export for PackageKit/libyui-qt-pkg" and 17.35.x "export … for YAST" releases are what happens when you don't.
6. **Behaviour changes**: write them in `libzypp.changes` in user terms. zypper's changelog usually needs a matching entry when output or exit codes change. Point the YaST and PK maintainers at the bug.

---

## Pitfalls & special cases

- `getZYpp()` never locks as non-root: user-mode tests do not exercise the lock path at all.
- `lockerName()` comes from `/proc/PID/cmdline`, and zypper's PackageKit prompt depends on the substring `packagekitd` (bnc#580513).
- Dropping the last `ZYpp::Ptr` releases the lock but keeps `ZYpp::Impl`. PK calls `initializeTarget` only once per process because of this (bnc#899755).
- `ZYPP_LOCKFILE_ROOT` is a "quick fix" (bnc#575096) used by zypper (`--root`) *and* Agama. Don't fold it into `ZConfig::systemRoot` without checking both.
- Only one `ReceiveReport` per report type: connecting a second receiver silently disconnects the first.
- `UserData` value-type mismatch: classic rpm/`contentLogline` send `std::cref(line)`, SA reports send `std::string`. A `get<T>` with the wrong type throws or returns the default.
- PK and Agama have no `*ReportSA` receivers. With single-trans rpm they may get less progress or error info than zypper.
- `repo::RepoReport` is `ZYPP_DEPRECATED` but still used by zypper and PK. It cannot be dropped.
- Changing a callback's default return value changes behaviour for every consumer that doesn't override it. For KeyRing/Digest this is security-relevant.
- zypper `--non-interactive` = "take the prompt default". zypper's defaults reject unknown keys, unsigned files and auth prompts, and abort on problems. `IO_SOFT` media errors retry with a 30 s timeout.
- zypper prints only the first line of `problem()` descriptions; extra rpm output is expected through `report()`.
- zypper's classic-mode `%script` failure detection parses rpm's stdout text (`scriptlet failed`), so changing how rpm output is forwarded affects exit code 107.
- `--installroot` vs `--root`: only `--installroot` calls `setRepoManagerRoot("/")`. `announceSystemRoot` is called even without target init (bsc#1237044).
- The `DownloadAsNeeded` + single-trans combination is invalid. zypper prints a `ZYPP_SINGLE_RPMTRANS=0` hint, and libzypp switches to classic (17.37.7 changes).
- Hiding symbols (17.33.0, visibility=hidden) took several releases of "export X for YaST/PK/libyui" fixes. Treat the export list as API.
- Public headers must compile as C++11 for YaST/PK on older code streams (17.31.1/17.32.0 changes).
- libyui-qt-pkg still includes `zypp/base/Sysconfig.h`. It exists only as a `zypp-compat` forwarder.
- `libzypp(plugin:appdata)` is provided in the spec but missing from `FeatureTest.doc`.
- python3-zypp-plugin < 0.6.4 rejects STOMP headers containing `-` (bsc#1227793). Watch new plugin header names.
- PK changes `http_proxy` and similar env vars at runtime, and sets `ZYPP_REPO_RELEASEVER` per job. Caching env at library init would break it (not verified in libzypp code).

## Where to start when debugging

- **Which consumer, which log?** zypper: `/var/log/zypper.log` (or `$ZYPP_LOGFILE`), with the `zypper-log` tool to slice by run. PackageKit: `/var/log/pk_backend_zypp` (log group `packagekit`). YaST: `/var/log/YaST2/y2log` (pkg-bindings logs via y2log; the libzypp log routing there was not verified). Agama: the systemd journal (custom `LogControl` writer). `ZYPP_FULLLOG=1` adds a solver autotestcase to `/var/log/YaST2/autoTestcase`.
- **Lock problems**: `cat /run/zypp.pid /run/zypp-rpm.pid`, `tr '\0' ' ' </proc/$(cat /run/zypp.pid)/cmdline`. Grep the log for `Lockfile`, `ZYpp is on/off`, `$ZYPP_LOCK_TIMEOUT`. zypper exit 7 means `ZYppFactoryException`.
- **"Callback not called"**: check that the override signature matches `ZYppCallbacks.h` exactly (add `override` locally to find out), and that no other receiver for the same report type is connected later (`connect()` replaces).
- **Non-interactive surprises**: grep zypper.log for `running non-interactively, returning`. It names the PromptId and the default that was chosen.
- **Solver differences between consumers**: reproduce with `zypper --debug-solver ...` (writes `/var/log/zypper.solverTestCase`), then compare the flags each consumer sets (§2.5, §3, §4) and replay with the testcase tools ([11](11-testing.md), [04](04-solver.md)).
- **Exit-code questions**: `zypper/src/main.h` (codes) and `zypper/src/solve-commit.cc` (commit mapping); `ZYppCommitResult` in `zypp/zypp/ZYppCommitResult.h`.
- **API usage survey before a change**: in this tree, `grep -rn '<symbol>' zypper/src`. For externals, GitHub search in `yast/yast-pkg-bindings`, `libyui/libyui` (`libyui-qt-pkg`, `libyui-ncurses-pkg`), `PackageKit/PackageKit` (`backends/zypp`), `agama-project/agama` (`rust/zypp-agama/zypp-agama-sys/c-layer`), `openSUSE/snapper` (`zypp-plugin`). In OBS: `osc whatdependson openSUSE:Factory libzypp standard x86_64`.
- **Feature/version gating**: `zypp/libzypp.spec.cmake` (Provides/Conflicts), `zypp/doc/autoinclude/FeatureTest.doc`, `zypp/VERSION.cmake`, `zypper/zypper.spec.cmake`, `zypper/package/zypper.changes`.
