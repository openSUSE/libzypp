# 15 – Bug Triage Guide

A practical handbook for incoming bug reports (bugzilla.suse.com / bugzilla.opensuse.org, GitHub issues).
It maps the **symptoms users report** to the subsystem, the exact log strings to grep for, the data to request,
the usual causes and the chapter to read. It also shows where historic bugs cluster (from
`zypp/package/libzypp.changes`), gives short reproduction recipes and explains when to reassign a bug to another team.
All quoted log/error strings were checked against the current sources (file given). Strings in *italics* are
paraphrases, not grep-able text.

---

## 1. Triage workflow

### 1.1 First questions (answer them before reading code)

| Question | How to answer it | Why |
|---|---|---|
| Which **front end**? zypper, YaST (installer or running system), PackageKit (GNOME Software / KDE Discover / offline update), Agama, transactional-update | Report text; log file present (§1.3) | Front ends set different solver flags and callbacks ([14](14-consumers-and-api-surface.md)) |
| Which **versions / codestream**? | `rpm -q libzypp zypper libsolv-tools rpm`; the log line `libzypp: 17.x.y (<suse_version>:<sle_version>:<is_opensuse>)` (`ZConfig.cc:349`) | Many fixes are codestream-specific. The HTTP backend default differs (MediaCurl on `< 1600`, MediaCurl2 on 16/TW, [07](07-media-and-downloads.md)) |
| Which **operation** exactly? | Full command line. zypper logs it as `===== 'arg' 'arg' … =====` right after `===== Hi, me zypper <ver>` (`zypper/src/main.cc:142`) | `zypper up` and `zypper dup` and `zypper patch` use different solver modes ([04](04-solver.md) §5) |
| **Phase** of the failure? refresh / load / solve / download / rpm commit / post-commit | Last milestone lines in the log (§1.3) | Picks the subsystem |
| **Regression**? "worked with the previous update" | `/var/log/zypp/history` shows when libzypp/zypper/libsolv/rpm were updated | Bisect against `libzypp.changes` (§3) |
| **Root** = `/`? `--root`, `--installroot`, chroot, container, transactional-update, installer (`/mnt`) | Command line, `TRANSACTIONAL_UPDATE` env, YaST stage | Many code paths change with root != `/` ([08](08-target-commit-and-rpm.md) §11, [09](09-configuration-and-environment.md) §13) |
| **Non-interactive**? cron, salt/ansible, `-n`, PackageKit | `running non-interactively, returning …` in the log (`zypper/src/utils/prompt.cc`) | Prompts silently take their defaults |
| Third-party repos/vendors, PTFs, retracted patches? | `zypper lr -d`, `rpm -qa --qf '%{NAME} %{VENDOR}\n'` | Vendor stickiness and blacklists explain most "solver is wrong" reports |

### 1.2 What to request from the reporter

Copy-paste checklist (extends [11](11-testing.md) §15.1):

| Data | Command / path | Notes |
|---|---|---|
| zypper log | `/var/log/zypper.log` (plus rotated `zypper.log-*.xz` if the run is older) | Path is the compiled-in `ZYPPER_LOG` (`zypper/src/main.h:48`). **Not** `/var/log/zypp/zypper.log` |
| More verbose log (re-run) | `ZYPP_FULLLOG=1 zypper -vvv <cmd>` | `ZYPP_FULLLOG` adds `<999>` (XXX) lines and sets libsolv debug level 2. `-v` only affects the console. Add `ZYPP_MEDIA_CURL_DEBUG=2` for download/auth problems, `ZYPP_LIBSOLV_FULLLOG=1` for solver internals (huge) |
| History | `/var/log/zypp/history` (`ZConfig.cc:1192`, configurable via `history.logfile`) | What was installed/removed when, by whom (`user@host`), scriptlet output in `# … rpm output:` blocks |
| Solver testcase | `zypper --debug-solver <same cmd>` → tar `/var/log/zypper.solverTestCase/` (`zypper/src/solve-commit.cc:544`). After a `dup` commit: `/var/log/updateTestcase-*` (`TargetImpl.cc:514`). YaST: `/var/log/YaST2/solverTestcase`; with `ZYPP_FULLLOG=1`: `/var/log/YaST2/autoTestcase` | Absolute path, not below `--root`; overwritten by each run |
| Repos and services | `zypper lr -d`, `zypper ls -d`, `/etc/zypp/repos.d/*.repo`, `/etc/zypp/services.d/*` | `lr -d` shows priority, autorefresh, gpgcheck flags (`*` = mandatory) and URLs (passwords stripped) |
| Configuration tree | `tar` of `/etc/zypp` **without** `credentials.d/` and `credentials.cat`; `/usr/etc/zypp/zypp.conf`, `/etc/zypp/zypp.conf.d/*`, `/run/zypp/` | zypp.conf is assembled from base file + drop-ins ([09](09-configuration-and-environment.md) §2). Also `vendors.d`, `locks`, `systemCheck*`, `needreboot*`, `vars.d` |
| Installed packages | `rpm -qa --qf '%{NAME}-%{VERSION}-%{RELEASE}.%{ARCH} %{VENDOR}\n' \| sort` | Vendor + arch matter for the solver |
| Products / arch | `ls -l /etc/products.d/` (is `baseproduct` a valid symlink?), `/etc/os-release`, `uname -m` | `$releasever`, service repos, arch detection |
| Env | `env \| grep -E 'ZYPP_\|_proxy\|TRANSACTIONAL'`, `/etc/sysconfig/proxy` | Env vars override config silently |
| Front-end logs | YaST: `/var/log/YaST2/y2log*` (or `save_y2logs`). PackageKit: `/var/log/pk_backend_zypp`. Agama: journal | See §1.3 and [14](14-consumers-and-api-surface.md) |
| Lock holders | `cat /run/zypp.pid /run/zypp-rpm.pid`; `tr '\0' ' ' </proc/<pid>/cmdline` | For lock reports |

Never ask for credential files, `/root/.curlrc` or full URLs with tokens (SCC service URLs contain them).
Note that `/var/log/zypper.log` contains repo URLs **without** passwords, but service/repo URLs may still
include query-string tokens.

### 1.3 Reading a zypper.log

**Line format** (`LogControl::LineFormater::format`, details in [02](02-core-infrastructure.md) §3.2):

```
2026-10-22 10:11:12 <1> host(4711) [zypp::repomanager] repomanagerwf.cc(operator()):376 RefreshCheckStatus returned: ...
│                   │   │    │     │                   │                  │            └ message
│                   │   │    │     │                   │                  └ line
│                   │   │    │     │                   └ file(function)
│                   │   │    │     └ log group
│                   │   │    └ PID (in forwarding mode the *parent* PID, e.g. lines from zypp-rpm / media workers)
│                   │   └ host
│                   └ level: 1=MIL (and DBG!), 2=WAR, 3=ERR, 4=SEC, 5=INT, 6=USR, 999=XXX (only with ZYPP_FULLLOG)
└ timestamp
```

* **Finding a run.** The log is shared by all zypper runs (append mode, never rotated by libzypp). A zypper run
  starts with `===== Hi, me zypper <version>` followed by the argv line and ends with
  `===== Exiting main(<exitcode>) =====` (`zypper/src/main.cc`). The libzypp singleton prints
  `ZYpp is on...` / `ZYpp is off...` (`ZYppFactory.cc:335/341`). The `zypper-log` script shipped with zypper
  (`zypper/tools/zypper-log`) lists all runs and extracts one by PID: `zypper-log` → pick PID → `zypper-log <PID>`.
  `-r N` includes rotated logs, `-d YYYY-MM-DD` filters by date. Interleaved PIDs mean two processes ran at the same time
  (e.g. a cron job, or `zypper` + `zypp-refresh`).
* **Exit code** of the run is in `Exiting main(N)`. Map it with `zypper/src/main.h`: 4 = libzypp error,
  7 = locked, 8 = commit error, 104 = capability not found, 106 = repos skipped, 107 = rpm scriptlet failed
  ([14](14-consumers-and-api-surface.md) §2.7).
* **Debug lines** (`DBG`) are written at level `<1>` in a group with `++` appended (`[zypp++]`, `[zypp-core++]`,
  `[Progress++]`). Grep `'++]'` to find or drop them.
* **INT lines are normal.** Every `ZYPP_THROW` / `ZYPP_CAUGHT` / `ZYPP_RETHROW` writes a `<5>` line with
  `THROW:`, `CAUGHT:` or `RETHROW:` and the exception history. Many exceptions are expected and handled
  (probing optional files, trying the next mirror, `doesFileExist`, solv toolversion mismatch → rebuild). Only the
  **last** unhandled exception before the user-visible error counts. Follow it backwards via the code location.
* **Threads.** `{T:name}` appears for every thread except `Zypp-main`. Lines with a tag from a worker or the
  preloader are not "out of order", they are concurrent.
* **Useful groups:** `zypp::exec` (every spawned command with argv and exit status, e.g. `rpm`, `repo2solv`,
  `rpmdb2solv`, `gpg`), `zypp::repomanager`, `zypp::solver`, `libsolv`, `zypp::KeyRing`, `zypp::gpg`, `zypp::media`
  /`zypp-media`/`zypp-curl`, `zypp::fetcher`, `librpmDb`, `zypp-rpm`, `zypp::posttrans`, `zypp::plugin`, `zconfig`,
  `PoolQuery`, `PurgeKernels`. A group named `[DEFINE_LOGGROUP]` is a build-system omission, not a problem.
* **Startup block**: `libzypp: <version> (<codestream>)`, `Uname architecture is …` (`Arch.cc`), the zconfig
  file list, `Reading zypp.conf for root …` — check these first when "my config is ignored" or "wrong arch".
* **Phase markers** (in order of a typical `zypper in` run):

```
ZYpp is on...
Check if to refresh repo <alias> / RefreshCheckStatus returned: …        (refresh, zypp::repomanager)
<alias> building cache... / Starting repo2solv for repo …                (solv cache)
Read cookie: …                                                           (@System cache, TargetImpl)
Apply N HardLockQueries / HardLockQueries match N Solvables.             (locks)
Establish... / Establish DONE                                             (patch status)
-------------- Calling SAT Solver -------------------                    (Resolver.cc)
Encountered problems! Here are the solutions:                            (only with problems)
TargetImpl::commit( … ) / Todo: …                                        (commit start)
Downloading packages via N connections                                   (preloader)
zypp-rpm lines / [rpm …] / COLLECT posttrans / EXECUTE posttrans        (rpm)
Commit (<policy>) returned: <result>                                     (ZYppImpl.cc:255)
ZYpp is off...
```

* **Crash** lines: `Error: signal 11` (SIGSEGV) or `Error: signal 6` (SIGABRT) followed by a backtrace, written by
  `SigBacktraceHandler` (`ZYppFactory.cc:59`). After that the log thread is shut down, so nothing more follows.

### 1.4 Who owns the bug?

```
                           ┌─ wrong text/option/prompt/exit code, table output ─────────► zypper
                           ├─ only in YaST/PK/Agama, zypper with same request is fine ──► front end (check flags first!)
report ─► reproduce with ──┤─ testsolv testcase.t gives the same result ──────────────► libsolv (or the metadata)
          zypper + testcase├─ rpm error text, scriptlet output, rpmdb damage ─────────► rpm or the package
                           ├─ metadata wrong/missing/badly signed/404 on the server ──► repo owner / OBS / IBS / SCC / mirror admin
                           └─ everything else (refresh logic, caching, keyring, media,
                              commit orchestration, config parsing, pool state) ─────► libzypp
```

Details and hints: §5.

---

## 2. Symptom catalogue

Format per entry: **Subsystem** · **Grep for** · **Likely causes** · **Ask for** · **Read**.
"Grep for" lists exact substrings (from the source file named) that you can search for in `zypper.log` or the
console output. The catalogue is grouped A–L.

### A. Locking, startup, privileges

**A1. "System management is locked by the application with pid N (name). Close this application before trying again."**
- Subsystem: global lock, `zypp/zypp/ZYppFactory.cc` (`getZYpp()`), [08](08-target-commit-and-rpm.md) §2.
- Grep: `System management is locked`, `$ZYPP_LOCK_TIMEOUT=`, `Gave up waiting for the zypp lock`, `Finally got the zypp lock`. zypper exit 7.
- Causes: another zypper/YaST running; **packagekitd** holding the lock between jobs (zypper offers to ask PK to quit); stale pid file whose pid was reused by an unrelated process (the name in the message tells); `zypp-rpm` still running (`/run/zypp-rpm.pid`, checked only on the contended path); a hung plugin or `%posttrans` keeping the holder alive.
- Ask for: `cat /run/zypp.pid /run/zypp-rpm.pid`, `ps -fp <pid>`, the holder's log.
- Notes: `ZYPP_LOCK_TIMEOUT` / zypp.conf `lock_timeout` (bsc#1239809); negative = wait forever (bsc#1184399, bsc#1213231). Non-root users and `ZYPP_READONLY_HACK` never lock. `--root` still uses the **host** `/run/zypp.pid` unless `ZYPP_LOCKFILE_ROOT` is set — "locked" while installing into a chroot is expected behaviour.

**A2. Lock appears to be ignored (two package managers ran in parallel)**
- Subsystem: global lock. Grep: `Running as user. Skip creating`, `ZYPP_READONLY active.` (`ZYppFactory.cc`).
- Causes: one side ran as non-root (read-only access, no lock by design); `ZYPP_READONLY_HACK`; different `ZYPP_LOCKFILE_ROOT`; a tool using librpm directly (rpm itself has its own lock only).
- Read: [08](08-target-commit-and-rpm.md) §2.

**A3. "No permission to write repository cache." / refresh does nothing as user**
- Subsystem: RepoManager. Grep: `No permision to write cache` (sic, `repomanagerwf.cc:385`), `No permission to write raw cache`, `RepoNoPermissionException`.
- Causes: non-root user; since bsc#1225451 this is a clean exception instead of a confusing failure. Non-root sees `Repository '…' is out-of-date. You can run 'zypper refresh' as root to update it.` (zypper). Outdated check as user: bsc#1222086.
- Read: [05](05-repositories-and-services.md).

### B. Repository refresh and metadata

**B1. "Signature verification failed for file 'repomd.xml' from repository '…'"**
- Subsystem: KeyRing / signature workflow (`zypp/zypp/ng/workflows/signaturecheckwf.cc`, `ng/reporthelper.cc`, zypper `callbacks/keyring.h`), [06](06-security-gpg-keyring.md).
- Grep: `Signature verification failed`, `Going to verify signature`, `Failed signature check` (`zypp::gpg`), `validated:` in the `VerifyFileContext` dump.
- Causes, in order of frequency: (1) **mirror out of sync** — `repomd.xml` and `repomd.xml.asc` from different generations (zypper even prints *"This might be a transient issue if the server is in the midst of receiving new data"*); (2) **captive portal / proxy error page** delivered with HTTP 200 instead of the file (inferred, there is no dedicated detection in libzypp: look for HTML in the downloaded file under `/var/cache/zypp/raw/<alias>/` or the temp dir); (3) repo re-signed with a new key; (4) truly tampered data.
- Ask for: `zypper -vvv ref -f <alias>` log with `ZYPP_MEDIA_CURL_DEBUG=2`, `curl -sI <baseurl>/repodata/repomd.xml{,.asc}`, time of failure, mirror used (`Trying to fetch file … via URL` lines).
- Note: since bsc#1236820 the master index is fetched **before** key and signature; since bsc#1274625 `repo_gpgcheck` is strictly enforced (legacy exception for previously accepted unsigned repos removed) — a source of new "unsigned" reports after updates.

**B2. "File 'repomd.xml' … is signed with an unknown key 'XXXX'" / new key prompt in non-interactive mode**
- Subsystem: KeyRing. Grep: `signed with an unknown key`, `New repository or package signing key received`, `Import Tkey`/`Import Gkey`, `Check keyhints`, `Keyhint is too short`, `askUserToAccept`.
- Causes: repo key not imported yet (`zypper -n` answers **reject** by default, `--gpg-auto-import-keys` accepts); key rotation; keyhint in `repomd.xml` using a short id (rejected since bsc#1271730) or containing a path (bsc#1267426, CVE-2026-44941); `gpgkey=` URL unreachable (bsc#1223430).
- Ask for: `zypper lr -d <alias>` (gpgkey), `rpm -q gpg-pubkey --qf '%{NAME}-%{VERSION}-%{RELEASE} %{SUMMARY}\n'`, `ls /var/cache/zypp/pubkeys/`.

**B3. "The gpg key signing file '…' has expired" / expired-key warnings**
- Subsystem: KeyRing / zypper. Grep: `has expired` (zypper `keyring.h:123`), `Accept good signature from expired key` (`zypp-common/KeyManager.cc`).
- Facts: signatures from **expired keys still validate** (`GPG_ERR_KEY_EXPIRED` is treated as good); zypper warns when a key expires within 15 days. There is no expiry callback; renewal needs the repo's `gpgkey` (or keyhints) to deliver the extended key. Trusted key updates from the general keyring: bsc#1259706.
- Usually a **repo owner** issue (extend the key) unless libzypp fails to pick up the renewed key.

**B4. "Digest verification failed for file '…'" / "File '…' is unsigned, continue?"**
- Subsystem: checksum / FileChecker (`ng/reporthelper.cc`, `checksumwf.cc`). Grep: `Digest verification failed`, `No digest for file`, `Unknown digest`, `is unsigned`.
- Causes: mirror delivered stale or truncated data files; repo without signature (`repo_gpgcheck`); broken transparent proxy. Wrong-checksum acceptances are cached for 12 h per process.

**B5. "File '…/repomd.xml' not found on medium '…'" / HTTP 404 / "Repository type can't be determined."**
- Subsystem: media + repo probing (`zypp-logic/zypp-media/mediaexception.cc`, `zypp/zypp/repo/RepoException.cc`, probe in `repomanagerwf.cc`).
- Grep: `not found on medium`, `Probed type NONE`, `Probed type RPMMD at`, `Repository type can't be determined.`, zypper `Repository '%s' is invalid.` and `Please check if the URIs defined for this repository are pointing to a valid repository.`
- Causes: wrong URL / `$releasever` / `$basearch` expansion (B11); repo removed upstream (EOL distro, moved to `/history`); mirror missing the path; wrong `path=` (now restricted, bsc#1267874). Older versions printed *"Valid metadata not found at specified URL"* — that string **no longer exists** in libzypp/zypper sources; reporters may still quote it from old systems or forums.
- Ask for: `zypper lr -d`, the expanded URL (`zypper --releasever X lr -u`), `curl -I` of `<url>/repodata/repomd.xml`.

**B6. "Timeout exceeded when accessing '…'" / "Download (curl) error for '…'" / "Connection failed"**
- Subsystem: media (MediaCurl or MediaCurl2), [07](07-media-and-downloads.md). Grep: `Timeout exceeded when accessing`, `Download (curl) error for`, `Location '%s' is temporarily unaccessible.`, `Trying next url`, `Server returned a HTTP/2 error.`
- Causes: network/IPv6 black hole (test `ZYPP_MEDIA_CURL_IPRESOLVE=4`); proxy; `download.connect_timeout` (60 s, bsc#1208329) / `download.transfer_timeout`; HTTP/2 header with trailing space (bsc#1212187); hang without network (bsc#1230912); MediaCurl vs MediaCurl2 difference (compare `ZYPP_CURL2=0` vs `=1`). `download.max_silent_tries` defaults to 1 (no retries).
- Ask for: log with `ZYPP_MEDIA_CURL_DEBUG=2`, `/etc/sysconfig/proxy`, result with `ZYPP_CURL2=0` and `=1`.

**B7. Proxy / SSL errors: " SSL certificate problem, verify that the CA cert is OK for '…'." / "The peer certificate could not be verified"**
- Subsystem: media (curl settings, libproxy). Grep: `SSL certificate problem`, `The peer certificate could not be verified`, `Build Libproxy Factory from /etc/sysconfig/proxy`.
- Causes: corporate TLS-intercepting proxy without CA in the system store; clock skew; `ssl_capath`/`ssl_clientcert` URL params; libproxy result evaluation (bsc#1244710, bsc#1247690 — under YaST the last proxy wins); `?proxy=_none_`. PackageKit sets `*_proxy` env vars at runtime ([14](14-consumers-and-api-surface.md) §4). Redirects only to https are allowed (bsc#1218831).

**B8. "Authentication required" / "Login failed." / 401 loops / HTTP 403 on SCC repos**
- Subsystem: media auth + CredentialManager, [07](07-media-and-downloads.md) and [06](06-security-gpg-keyring.md). Grep: `Authentication required`, `Login failed.`, `got stored credentials`, `Found credentials for`, `No credentials for`, `callback answer: retry|cancel`, and for 403: `Visit the SUSE Customer Center to check whether your registration is valid and has not expired.` (`networkrequesterror.cc:124`).
- Causes: expired registration / subscription (→ SCC/registration, not libzypp); wrong or unreadable `/etc/zypp/credentials.d/<name>` (mode, stored URL not matching); non-interactive mode cannot prompt (`AuthenticationReport::prompt` returns false); historic endless loops with wrong stored credentials (bsc#1210870), username without password (bsc#1273242), HEAD requests (bsc#1244105), 401/407 body misinterpreted as "filesize exceeded" (bsc#1245220, bsc#1213673).
- Ask for: `ls -l /etc/zypp/credentials.d/` (not contents!), `SUSEConnect -s`, log with `ZYPP_MEDIA_CURL_DEBUG=2` (credentials masked).

**B9. Mirror problems: random failures, slow downloads, wrong country, "No mirror found, giving up"**
- Subsystem: mirrorlist/metalink/GeoIP (`repo/RepoMirrorList.cc`, `repomanagerwf.cc`), preloader. Grep: `Detected Metalink`, `Getting MirrorList from URL`, `Mirror cachefile cookie valid`, `Storing geoIP redirection`, `Found GeoIP rewrite`, `trying to taint mirror`, `No mirror found, giving up`.
- Causes: stale mirrorlist cache (`/var/cache/zypp/raw/<alias>/mirrorlist{,.cookie}`); GeoIP pinning a bad mirror (`/var/cache/zypp/geoip.d/`, disable via `download.use_geoip_mirror=false`); mirrors with different scheme silently dropped; mirror path handling (bsc#1247054, bsc#1253740, bsc#1253193). Security-relevant files (repomd, keys) are **never** taken from mirrors.
- Read: [05](05-repositories-and-services.md) "Mirrors", [07](07-media-and-downloads.md).

**B10. Every run rebuilds the solv cache / "building cache…" loops / refresh every time**
- Subsystem: RepoStatus / cache cookies, `zypp/zypp/ng/repomanager.cc`, `ng/repo/workflows/repomanagerwf.cc`. Grep: `building cache...`, `cache is up to date with metadata`, `Metadata and solv cache don't match. Check data on server...`, `Old status: … New Status:`, `Solv-file was created by '…'-parser (want …)`, `Try to handle exception by rebuilding the solv-file`, `has been refreshed less than repo.refresh.delay`.
- Causes: repo URL containing a variable that changes (URL is part of RepoStatus); clock/mtime problems (bsc#1222086 cookie mtime); a libsolv update changing `LIBSOLV_TOOLVERSION` (one rebuild per repo, expected); unwritable cache (falls back each time); two front ends with different cache dirs; `@System` cache rebuilt because `/etc/products.d` or rpmdb changed (`Read cookie:` lines). `repo.refresh.delay` only applies when raw status equals the solv cookie (bsc#1174016).
- Ask for: `ls -la --time-style=full-iso /var/cache/zypp/{raw,solv}/<alias>/`, `cat …/solv/<alias>/cookie`, two consecutive logs.

**B11. Wrong `$releasever` / `$basearch`, 404 on versioned paths, service repos for wrong product**
- Subsystem: RepoVariables (`zypp/zypp/repo/RepoVariables.cc`) + target products. Grep: `baseproduct symlink is dangling or missing:` (`TargetImpl.cc:2995`), `Remember distributionVersion =`, `ServicesTargetDistro:`, `Skipping repository meant for` (`ng/repomanager.cc`), `RepoVars context changed`.
- Causes: missing/dangling `/etc/products.d/baseproduct` (e.g. after a broken migration); `/etc/zypp/vars.d/releasever` left over; `ZYPP_REPO_RELEASEVER` set (PackageKit does that for upgrade jobs); `--root` evaluated against the wrong context (bsc#1237044); `--releasever` on the command line. `$releasever` follows the baseproduct on every lookup unless overridden.
- Ask for: `readlink -f /etc/products.d/baseproduct`, `ls /etc/zypp/vars.d`, `zypper lr -u`.

**B12. Refresh is slow**
- Subsystem: refresh + media. Grep: timestamps between `Check if to refresh repo` and `RefreshCheckStatus returned`; `Getting MirrorList from URL`; `Skipping GeoIP request`; `Starting repo2solv for repo` (CPU time of repo2solv); `Failed to setup zchunk because of:`.
- Causes: many repos with `autorefresh=1` and small `repo.refresh.delay`; big `filelists`/`other` (only fetched if needed); DNS/IPv6 timeouts (B6); zchunk fallback; GeoIP/mirrorlist round trips per repo; slow repo2solv on huge repos. Compare `zypper ref` with `zypper --no-refresh`.

**B13. "Error parsing metadata for '…'" / repoindex.xml / susetags errors**
- Subsystem: parsers / repo2solv. Grep: zypper `Error parsing metadata for`, `This may be caused by invalid metadata in the repository`, `parser::yum`, repo2solv WAR lines in `zypp::exec`.
- Causes: corrupt or HTML-instead-of-XML downloads (proxy/portal), invalid upstream metadata (→ repo owner/OBS), repoindex parse errors (bsc#1227625), libxml2 behaviour changes (fixes #635). Hostile metadata with `../` locations is discarded since CVE-2026-25707 (bsc#1259802) — a "missing package" after update may be such an entry.

**B14. Service (SCC/RIS/plugin) repos disappear, re-enable themselves, or vanish after migration**
- Subsystem: services (`ng/repo/workflows/serviceswf.cc`), [05](05-repositories-and-services.md). Grep: `Going to refresh service`, `Service repo … gets …`, `ServicePluginInformalException`, `ServicesTargetDistro:`.
- Causes: service refresh rewrites its repos (`repostoenable` one-shot, `repostodisable` cleared every refresh) — user changes to service repos are overwritten (by design, zypper adds a note to such `.repo` files, fixes #638); target distro mismatch (B11); orphaned service repos auto-removed when a RepoManager is constructed (bnc#891515; `.no_auto_prune`, bsc#1204956).

### C. Solver results (install / remove / update)

General first step for all C/D/E entries: **get the solver testcase** and replay it (§4). Then grep `zypp::solver`:
`-------------- Calling SAT Solver`, job lines (`Install …`, `Delete …`, `Locked N installed items and M NOT installed items.`),
the flag dump before `Starting solving....`, `Solverrun finished with an ERROR`, `Encountered problems! Here are the solutions:`.
Since bsc#1277790 all problem rules are logged.

**C1. "nothing provides 'X' needed by the to be installed Y"**
- Subsystem: solver problem text (`SATResolver.cc:1201-1203`), usually **metadata**. Grep: `nothing provides`, `Have you enabled all the required repositories?`.
- Causes: required repo disabled/not refreshed; package built against something not (yet) published (→ OBS/maintenance, often transient during repo syncs); arch filtering (package for another arch); retracted or blacklisted provider; locked provider (look for `remove lock to allow installation of`).

**C2. "the to be installed X conflicts with 'Y' provided by the installed Z" / "cannot install both X and Y"**
- Subsystem: solver, usually metadata/packaging. Grep: `conflicts with`, `obsoletes`, `cannot install both`.
- Causes: packaging conflicts (→ package maintainer), mixing third-party repos (Packman, devel projects), multiversion misconfigured (`cannot install both` for kernels → C6). PTF hints: `is not yet fully integrated into`, `blocks the desired action` (bsc#1194848).

**C3. Unexpected removals ("zypper wants to remove half my system")**
- Subsystem: solver flags + metadata. Grep: `deinstallation of`, `Delete `, `allowuninstall=`, `forceResolve`/`ALLOW_UNINSTALL`, `cleandeps`.
- Causes: `--force-resolution` (zypper sets `forceResolve` explicitly); `zypper rm --clean-deps` / `solver.cleandepsOnRemove`; DUP orphan handling (`removeOrphaned`, `solver.upgradeRemoveDroppedPackages` — 3rd-party rpms without repo count as orphans); `PurgeKernels` turned `forceResolve` on in a long-running app and never off; a conflicting new package winning over installed ones. Check whether removed packages are `autoinstalled` (unneeded cleanup, `removeUnneeded`, bsc#1175678).

**C4. Vendor change refused / unexpected vendor change ("install X (with vendor change)")**
- Subsystem: solver vendor rules, `/etc/zypp/vendors.d`, `VendorAttr`. Grep: `with vendor change`, `from vendor`, `Relaxed vendor check requested by`, `allowvendorchange`.
- Causes: sticky vendor (default for `up`); `solver.allowVendorChange` / `solver.dupAllowVendorChange` (default false since Code12 — old reports assume true); `dup-vendor-relax(suse)` provided by the release package switches DUP to relaxed checks (bsc#1182629); vendors.d equivalence classes; front ends set different flags (PK passes `ZConfig::solver_dupAllowVendorChange()`).

**C5. `zypper up` does not update package X (but `zypper in X` / `dup` would)**
- Subsystem: zypper + solver mode. No problem is ever reported because plain `zypper up` uses `Resolver::doUpdate()`, which silently keeps blocked packages ([04](04-solver.md) §5.2).
- Causes: vendor stickiness, arch, a lock, a conflict. Explain with `zypper up X` (uses `resolvePool()` and shows the problem) or `zypper -v up` / `zypper lu -a`.

**C6. Kernel multiversion: old kernels not removed / removed too early / "cannot install both kernel-…"**
- Subsystem: multiversion config + `PurgeKernels` (`zypp/zypp/PurgeKernels.cc`, log group `PurgeKernels`). Grep: `MultiversionSpec`, `PurgeKernels` lines, `is locked by the user, not removing.`
- Causes: `multiversion` / `multiversion.kernels` in zypp.conf (vendor file only sets these; a user `/etc/zypp/zypp.conf` masks the whole vendor file!); `uname -r` not matching an installed kernel (whole flavour kept); `-rc` kernels (bsc#1239718); kernel-firmware no longer flagged reboot-needed (bsc#1200993). Reproduce with `zypp/tools/zypp-runpurge`.

**C7. PTF / retracted packages: "install X although it has been retracted", PTF blocks updates or removal**
- Subsystem: solver blacklist (`ptf()`, `retracted-patch-package()` tokens, `sat/Solvable.cc`). Grep: `although it has been retracted`, `allow installing the PTF`, `blacklisted`, `is not yet fully integrated into`, `The PTF should be removed by calling`.
- Causes: by design blacklisted providers are not picked automatically; removing a PTF without enabled repos must fail (bsc#1203248); `ptf-package()` is not blacklisted (bsc#1186503). Untranslated libsolv texts for `SOLVER_RULE_BLACK` are expected.

**C8. Patterns: pattern shows as not installed / removing a pattern removes nothing or too much**
- Subsystem: patterns are real packages (`pattern()` / autopattern packages), not pseudo-installed since Fate#309385. Grep: `isAutoPattern`, the `patterns-*` package in the transaction.
- Causes: pattern package not installed although its content is; recommends not installed with `--no-recommends` / `solver.onlyRequires`; `zypper rm -t pattern` only removes the pattern package unless `--clean-deps`.

**C9. Recommended packages installed although `--no-recommends` / language packages appear**
- Subsystem: solver flags. `onlyRequires` still follows **namespace recommends** (language/modalias) — the `Resolver.h` doc is misleading ([04](04-solver.md) §4.3). `zypper inr` = `ignoreAlreadyRecommended=false`. Bug in `zypper update` recommends init: bsc#1201972.

**C10. "the installed X has inferior architecture" / arch flips (i586 ↔ x86_64, x86_64_v3)**
- Subsystem: arch detection (`zypp-logic/zypp/Arch.cc`) + solver. Grep: `Uname architecture is`, `architecture upgraded`, `Overriding system architecture`, `inferior architecture`, `architecture change of`.
- Causes: `arch=` in zypp.conf; x86_64_v[234] detection (fixes #439); hwcaps subarch packages (bsc#1246912); DUP pins the installed `rpm` arch only for root `/`.

**C11. Solver is very slow / hangs in "Resolving package dependencies…"**
- Grep (group `libsolv`): `pkg rule creation took`, `solver_solve took`, `create_solutions for problem #N took`, `N of M installable solvables considered`.
- Causes: many repos with many versions; many multiversion jobs; huge problem sets. There is no solver timeout knob. Usually **libsolv** territory; attach the testcase.

### D. Distribution upgrade (`zypper dup`, YaST/Agama migration)

**D1. dup drops packages / "the installed X does not belong to a distupgrade repository and must be replaced"**
- Subsystem: DUP mode + product droplists ([04](04-solver.md) §5.3). Grep: `does not belong to a distupgrade repository`, `Checking droplists`, `keep obsolete`, `install X from excluded repository`.
- Causes: third-party/orphaned packages; product `weakremover`/droplist second solve (bsc#1155819); `--from` used together with `doUpgrade()` semantics; repos missing for the new release (`$releasever` not switched — B11). Migration-specific libsolv requirements: e.g. libsolv ≥ 0.7.40 for SLES 16.1 migrations (bsc#1279541), product obsoletes with ':' (bsc#1243486).
- Ask for: `/var/log/updateTestcase-*` from the failed run, `zypper lr -d` **before and after** switching repos.

**D2. dup downgrades / changes arch / renames packages unexpectedly**
- Subsystem: DUP flag defaults (`dupAllowDowngrade`, `dupAllowArchChange`, `dupAllowNameChange` default **true**; vendor change default false). Grep: the `<resolver>` flag dump, `downgrade of`, `architecture change of`, `replacement of`.
- Often correct behaviour (the repo has the older version). Check the priorities in `zypper lr -d`.

**D3. Orphans: dup with `--remove-orphaned` removes packages from local rpms / unreachable repos**
- `removeOrphaned` (bsc#1221525) treats every package without a repo as orphan, including those from disabled or failing repos. Ask whether a refresh failed earlier in the same run (`Skipping repository '…' because of the above error.`, exit 106).

### E. Patches, locks, search

**E1. Patch shows "needed" although packages are updated (or "applied" although not)**
- Subsystem: patch establish ([04](04-solver.md) §8, §19). Grep: `Establish...`, `Establish DONE`, `Establish not needed.`; dump `zypper info -t patch <id>` (conflicts).
- Causes: patch is "needed" if an installed package is older than a fixed one **and** a vendor-compatible fix exists; only packages whose name+arch are installed are relevant; vendor change (installed from Packman/PTF) makes it "needed" forever; locked → shown as `unwanted` (`isUnwanted`, bnc#577118); retracted patches are never needed. Patch status changes are written to history (jsc#SLE-5116).
- Usually metadata or libsolv `solver_trivial_installable`; rarely libzypp.

**E2. `zypper patch` installs only the update stack first / needs to be run twice**
- By design: zypper's `updatePatches()` first selects `restartSuggested()` patches and stops (exit 103). Not libzypp.

**E3. Locks not working / lock reappears after removal / "remove lock to allow …" solution persisted a lock**
- Subsystem: `/etc/zypp/locks`, PoolQuery, HardLocksFile. Grep: `Apply N HardLockQueries`, `Re-apply N HardLockQueries`, `HardLockQueries match N Solvables.` (`pool/PoolImpl.h`), `Wrote … locks`, group `PoolQuery`.
- Causes: lock by **substring/glob** matching too much or too little (default `SUBSTRING|NOCASE`); lock on name but kind differs (patch/pattern/product prefixes); unlocking an item locked by a wildcard query does not remove the query; buddy handling for products (bsc#1225267); soft locks ignored if any version is installed; problem solutions at USER level written back to `/etc/zypp/locks` on commit; YaST/PK reading locks differently (PK uses `Locks::instance().readAndApply()`).
- Ask for: `/etc/zypp/locks`, `zypper ll`, before/after commit.

**E4. "Package 'X' not found." / zypper exit 104 / `zypper se` misses packages**
- Subsystem: pool/repo load. Causes: repo not refreshed or skipped (`Skipping repository … because of the above error`); repo disabled; arch-filtered (`ZYPP_TESTSUITE_FAKE_ARCH` to test); capability vs name (`zypper in 'perl(Foo)'`); package dropped as hostile metadata (B13); source packages need `-t srcpackage`.
- Tool: `zypp-NameReqPrv` against a testcase or root ([03](03-sat-pool-and-resolvables.md)).

**E5. Product / `zypper products` wrong, "isbase" wrong, release package mismatch**
- Subsystem: products ([08](08-target-commit-and-rpm.md) §13, [03](03-sat-pool-and-resolvables.md)). Grep: `has buddy`, `baseproduct symlink is dangling`, `referencePackage`. Historic: `Product::referencePackage` lookup (bsc#1259311).

### F. Commit (download and rpm)

**F1. "Failed to provide Package X. Do you want to retry retrieval?" / download failures during commit**
- Subsystem: PackageProvider / RepoProvideFile / preloader ([05](05-repositories-and-services.md), [08](08-target-commit-and-rpm.md) §15). Grep: `Failed to provide Package`, `provide Package`, `Trying next url`, `Some packages could not be provided. Aborting commit.` (`TargetImpl.cc:1602`), `Package %s seems to be corrupted during transfer`.
- Causes: metadata refreshed long ago while mirrors already deleted old rpms (→ `zypper ref` and retry); mirror lag; proxy; disk full in `/var/cache/zypp/packages`. **A download failure installs nothing and throws nothing** — check `Commit (…) returned:` (`noError`/`attemptToModify`).

**F2. Preloader problems: parallel download stuck, "No mirror found, giving up", packages from arch subrepos missing**
- Subsystem: `zypp/zypp/target/commitpackagepreloader.cc`. Grep: `CommitPackagePreloader disabled`, `No receiver for the CommitPreloadReport`, `Downloading packages via N connections`, `trying to taint mirror`, `No mirror found, giving up`, `Preloading done, mirror stats`.
- Facts: a fatal preload miss aborts the commit **without** falling back to serial download (all steps stay TODO); leftovers in `/var/cache/zypp/packages/<alias>/.preload`. Compare with `ZYPP_PCK_PRELOAD=0`. Historic: bsc#1253740 (arch subrepos), bsc#1247054 (`path()` missing), bsc#1239543 (progress throttling), bsc#1247948 (non-root precache).

**F3. Package signature errors: "Signature verification failed [4-Signatures public key is not available]" / "Package header is not signed!"**
- Subsystem: `PackageProvider.cc` + `RpmDb::checkPackageSignature`. Grep: `Signature verification failed`, `Signature does not verify`, `Signatures public key is not available`, `File is unsigned`, `Package header is not signed!`, `Package payload is not signed!`, `Relax CHK_NOSIG`, `User requested to accept insecure file`.
- Causes: package key not imported into rpm (NOKEY); package signed with a new key; unsigned local/3rd-party rpm with `pkg_gpgcheck`; header+payload must both be signed (bsc#1184501); rpm text output parsed in C locale (bsc#1076415); SHA1 unavailable in FIPS mode (bsc#1240529). Reproduce with `rpmkeys -Kv pkg.rpm`.

**F4. File conflicts: "Detected N file conflicts:"**
- Subsystem: `commitFindFileConflicts` (libzypp) + zypper prompt (`callbacks/rpm.h`). Grep: `Checking for file conflicts:`, `Detected`, `[--replacefiles]`.
- Causes: **packaging** bugs (→ package maintainers) or mixing repos. The classic check is useless with `DownloadAsNeeded` (nothing cached). In single-transaction mode rpm itself reports conflicts in its test transaction.

**F5. rpm failures: "Installation of X failed:" / "RPM failed: …" / "Executing the transaction failed because of the following problems:"**
- Subsystem: RpmDb (classic) or zypp-rpm (single trans). Grep: `RPM failed: `, `Additional rpm output`, `Executing the transaction failed because of the following problems:`, `RPM failed with a unexpected error`, `RpmTransactionFailedException`, group `zypp-rpm`, `[rpm`.
- Causes: scriptlet failures (F6), rpmdb damage (`rpm --rebuilddb`, check `Suggest rpm dbpath`, `Inject missing /var/lib/rpm`), dependencies rpm sees differently (`--nodeps` not used by zypper → packaging inconsistency), disk space (F7). The rpm text itself usually says who is to blame.
- Ask for: `/var/log/zypp/history` around the time (rpm output blocks), log with `ZYPP_RPM_DEBUG=1`.

**F6. Scriptlet / %posttrans failures: "warning: %post(X) scriptlet failed, exit status N" / zypper exit 107**
- Subsystem: rpm + package; libzypp only collects and runs `%posttrans` in some modes. Grep: `scriptlet failed`, `Executing %s script for`, `COLLECT posttrans:`, `EXECUTE posttrans:`, `UNEXECUTED posttrans:`, `LOST dump_posttrans support`, `rpm-script-fd`.
- Causes: package bug (most frequent → package maintainer); chroot environment (`/proc`, `/dev` auto-mounted since bsc#1181328; `SYSTEMD_OFFLINE`); `%posttrans` argument (2 for updates since bsc#1243279); broken `rpm --runposttrans` in rpm-4.18.0 (bsc#1216091, bsc#1246149, bsc#1257068); temp paths (bsc#1250343, bsc#1218459); `%transfiletrigger` delay (bsc#1041742); scripts can check `ZYPP_SINGLE_RPMTRANS`/`ZYPP_CLASSIC_RPMTRANS` env (bsc#1246038).

**F7. Disk space: "installing package X needs NMB more space on the / filesystem" / wrong size estimate**
- The first is **rpm**'s message. libzypp only *estimates* (`DiskUsageCounter`) and never refuses itself. Wrong estimates: btrfs snapshots make the volume growonly, read-only mounts dropped, packages without DU data count as 0, mount points of `/` used even with `--root`. Tool: `zypp/tools/DiskUsageCounter`.

**F8. Single-transaction (zypp-rpm) specific: hangs, missing output, "Received signal … not possible to cancel a running rpm transaction"**
- Subsystem: `commitInSingleTransaction` + `zypp/tools/zypp-rpm`. Grep: group `zypp-rpm`, `Ignore $ZYPP_SINGLE_RPMTRANS=1: Bug 1189788`, `DownloadAsNeeded enforces the classic_rpmtrans backend!`, `Activating SingleTransReportLegacyWrapper`, `Failed to create zypp-rpm pidfile.`, `Received signal`.
- Causes: opt-in on SUSE via `ZYPP_SINGLE_RPMTRANS` (or `techpreview.ZYPP_SINGLE_RPMTRANS`); refused for usrmerge transitions; protocol mismatch between libzypp and the installed `zypp-rpm` (bsc#1196925 — check both come from the same build); hangs (bsc#1197134); `.src.rpm` (bsc#1228647). Unsigned-package acceptance differs from classic mode (zypp-rpm installs with `--nosignature --nodigest`). First step: retry with `ZYPP_SINGLE_RPMTRANS=0` to classify.

**F9. Classic mode: answered "ignore" and the package is recorded as installed although rpm failed**
- Known behaviour: `IGNORE` makes `RpmDb::installPackage` return normally, the step becomes DONE and history gets an `install` line ([08](08-target-commit-and-rpm.md) pitfalls). Not a history bug.

**F10. Snapper pre snapshot without post snapshot / stale package list after an aborted commit**
- A `TargetAbortedException` after `COMMITBEGIN` skips `COMMITEND`, `buildCache()` and the pool reload ([08](08-target-commit-and-rpm.md) pitfalls). Commit plugins only run for root `/` (or `TRANSACTIONAL_UPDATE`), never on dry-run, and cannot veto (`ACK` expected). Snapper's plugin side: [14](14-consumers-and-api-surface.md) §5.

### G. System state after commit

**G1. `zypper ps` / needs-restarting lists wrong processes or nothing (containers)**
- Subsystem: `CheckAccessDeleted` (`zypp/zypp/misc/CheckAccessDeleted.cc`, group `zypp::misc`). Historic: incus/lxc (bsc#1229106), container detection (bsc#1218782, bsc#1218291), `/snap` executables (bsc#1226014), service detection (bsc#1205636).
- Ask for: `lsof -n -FpcuLRftkn0 -K i > out` (feed into `CheckAccessDeleted::check(out, verbose)`), `systemd-detect-virt`.

**G2. "Reboot required" wrong / missing (`zypper needs-rebooting`, `/run/reboot-needed`)**
- Subsystem: needreboot spec (`TargetImpl.cc:1362ff`). Packages providing `installhint(reboot-needed)` or `kernel`, plus specs from `/etc/zypp/needreboot` and `/etc/zypp/needreboot.d/*`, touch `<root>/run/reboot-needed` after a successful install. Wrong flagging is usually packaging (missing/extra `installhint`) or a local needreboot file. Patch `rebootSuggested()` (exit 102) is separate (metadata).

**G3. History / autoinstalled wrong: packages marked manual or auto, `zypper packages --unneeded` wrong**
- Subsystem: `/var/lib/zypp/AutoInstalled` + history. Grep: `updateAutoInstalled`. `AutoInstalled` is bootstrapped from history field `user@host`; it is written **before** rpm runs (a failed commit leaves the intended state) and not on dry-run. Installs via `rpm -i` are not tracked.

**G4. `/var/lib/rpm` vs `/usr/lib/sysimage/rpm` confusion, "rpmdb open failed"**
- Grep (group `librpmDb`): `Suggest rpm dbpath`, `Inject missing /var/lib/rpm`. The compat symlink is re-created at init and after each commit. `--rebuilddb` must use the real path. Usually rpm/migration issue.

### H. Crashes and hangs

**H1. zypper/YaST/PackageKit crashed (SIGSEGV/SIGABRT)**
- Grep: `Error: signal 11` / `Error: signal 6` plus the backtrace that follows (`ZYppFactory.cc:59`). For NG preconditions: `PRECONDITION VIOLATED`.
- Ask for: coredump (`coredumpctl info` / `coredumpctl gdb` with debuginfo), the log tail, versions of **all** stack packages (mixed libzypp/zypper/libsolv builds cause ABI crashes, e.g. bsc#1230267 private symbols). Historic: MediaDISK SEGV (bsc#1245452), malformed metadata (fixes #740), exceptions from `curl_easy_cleanup` (bsc#1201092), crashes at exit from singletons/static destructors ([02](02-core-infrastructure.md) §19).

**H2. Hang: waiting for the lock**
- Grep: `$ZYPP_LOCK_TIMEOUT=… sec. Waiting for the zypp lock`, `Another day has passed waiting for the zypp lock...`. See A1.

**H3. Hang in non-interactive runs: media change / auth / key prompt**
- Grep: `running non-interactively, returning`; for CD/DVD the default is **no → abort**, for network `IO_SOFT` errors the default is **retry** with a 30 s timeout until the repeat limit. Without a tty and without `-n` zypper exits 4 (*"Cannot read input"*). Machine-readable mode reads credentials from stdin. If nothing is logged at all, attach `gdb -p` / `py-spy`-like stack dumps (`gdb -batch -ex 'thread apply all bt' -p <pid>`).

**H4. Hang at the end of an operation / during commit: plugins**
- Grep (group `zypp::plugin`): `Not ready to write within timeout.`, `Not ready to read within timeout.` (`PluginScript.cc`), `Metadata rejected by`. Defaults 30 s; `ZYPP_PLUGIN_TIMEOUT`, `ZYPP_PLUGIN_SEND_TIMEOUT`, `ZYPP_PLUGIN_RECEIVE_TIMEOUT`; `ZYPP_PLUGIN_DEBUG=1` dumps frames. Old python `zypp-plugin` < 0.6.4 rejects headers with `-` (bsc#1227793); unescaped colons (bsc#1231043). Delays from appdata plugins with redirected output: bsc#1195326. `~RepoManager` runs appdata plugins when repos changed.

**H5. Hang in downloads (no progress)**
- Grep: last `Trying to fetch file` line; with `ZYPP_MEDIA_CURL_DEBUG=2` the curl trace. Historic: no-network hang (bsc#1230912), transfer timeout started too late (bsc#1199042), range requests (bsc#1204548). Compare `ZYPP_CURL2=0/1`.

### I. Front-end specific (YaST, PackageKit, Agama)

**I1. "Works with zypper, fails in YaST" (or vice versa)**
- YaST keeps the ZYpp instance (and the lock) for the whole session, recreates the RepoManager when the target root changes, uses classic rpm reports and its own media-change UI ([14](14-consumers-and-api-surface.md) §3). Solver flags differ: compare `<resolver>` flag dumps. Ask for `save_y2logs` and `/var/log/YaST2/solverTestcase`. Installer-specific: target `/mnt`, raw cache dir modes (bsc#1222398), volatile media refresh (bsc#1223094), exported symbols needed by old YaST (bsc#1228420, bsc#1228138).

**I2. PackageKit / GNOME Software: "zypper says system management is locked by packagekitd", updates shown but not installable, refresh errors only in the GUI**
- PK drops and re-acquires the ZYpp instance per job, keeps repos loaded once per process, sets `ZYPP_REPO_RELEASEVER` and proxy env vars at runtime, auto-accepts keys via its own signature workflow, and logs to `/var/log/pk_backend_zypp` ([14](14-consumers-and-api-surface.md) §4). A GUI-only failure is most often in the PK backend (→ PackageKit maintainers) unless the same calls fail in the libzypp log there.

**I3. Agama / transactional-update**
- Agama logs into the journal through a custom `LogControl` writer. transactional-update: `TRANSACTIONAL_UPDATE` only enables commit plugins for root != `/` (read once per process). `DownloadAsNeeded` silently forces the classic backend (agreed with YaST/Agama).

### J. Local and removable media

**J1. CD/DVD/ISO/USB: "Media source '…' does not contain the desired medium", "Medium '…' is in use by another instance", "Failed to mount … on …", "Cannot eject media"**
- Subsystem: media handlers (`MediaCD`, `MediaISO`, `MediaDISK`, `MediaNFS`), `zypp-logic/zypp-media/mediaexception.cc`. Grep: those strings, `Using a shared media`, `Using a system mounted media`, `MountEntries:`, `checkDesired`, leftover `AP_0x*` in `/var/adm/mount`.
- Causes: wrong medium (media.1/media checks), loop device still busy (retry since bsc#1196061), automounted media, ISO depending on a parent handler (`MediaIsSharedException` on close), non-interactive media change → abort. CD/DVD repos never auto-refresh.

**J2. NFS/SMB/`dir:` repos: "Permission to access '…' denied.", stale mounts, slow refresh**
- Causes: mount options, root squash (permission), server down (hang in mount). `Mount` logs every `mount` output line at DBG and the final `mount <src> <dir>: <exit>: <err>`.

### K. Configuration

**K1. zypp.conf setting not honoured**
- Subsystem: ZConfig + econf ([09](09-configuration-and-environment.md)). Grep (group `zconfig`): `getConfigFiles for stem`, `take`, `masked`, `No config files found`, `Reading zypp.conf for root`, `$ZYPP_CONF is set to`, `zypp.conf: Unknown entry in [main]:`, `zypp.conf: Unknown section:` (`ZConfig.cc:536/550`), `zypp.conf [env]: set/skip`.
- Causes: drop-in without `[main]` header → `Unknown section: []` (bsc#1272534, since 17.38.x); `/etc/zypp/zypp.conf` exists and masks the vendor file `/usr/etc/zypp/zypp.conf` entirely (incl. `multiversion`); key misspelled or not parsed at all (check `grep -n 'entry == "' zypp/zypp/ZConfig.cc zypp-logic/zypp-media/mediaconfig.cc`); an env var overrides it (`ZYPP_SINGLE_RPMTRANS`, `ZYPP_CURL2`, `ZYPP_LOCK_TIMEOUT`, …); front end overrides it at runtime (zypper CLI flags, YaST `Pkg::SetZConfig`); `--root` reads the target's zypp.conf; `ZYPP_CONF` set (even empty) disables econf.

**K2. zypper behaves differently for root and user / in a container**
- Non-root: no lock, no cache writes, packages cached under `XDG_CACHE_HOME`/`HOME` (bsc#1247948), keys cannot be trusted permanently. Containers: `zypper ps` filters (G1), `/proc`/`/dev` assumptions, read-only `/usr`.

### L. Quick lookup: console message → entry

| User quotes | Entry |
|---|---|
| `System management is locked` | A1 |
| `Signature verification failed for file 'repomd.xml'` | B1 |
| `signed with an unknown key` / `New repository or package signing key received` | B2 |
| `has expired` | B3 |
| `Digest verification failed` | B4 |
| `Repository type can't be determined.` / `Repository '…' is invalid.` / `not found on medium` / *Valid metadata not found* | B5 |
| `Timeout exceeded when accessing` / `Download (curl) error` | B6 |
| `SSL certificate problem` | B7 |
| `Authentication required` / `Login failed.` / `Visit the SUSE Customer Center` | B8 |
| `Skipping repository '…' because of the above error.` | B5/B6/B8, D3 |
| `nothing provides` | C1 |
| `conflicts with` / `cannot install both` | C2 |
| `deinstallation of` | C3 |
| `with vendor change` | C4 |
| `although it has been retracted` / `PTF` | C7 |
| `inferior architecture` | C10 |
| `does not belong to a distupgrade repository` | D1 |
| `Package 'X' not found.` | E4 |
| `Failed to provide Package` | F1 |
| `Signatures public key is not available` / `Package header is not signed!` | F3 |
| `Detected N file conflicts` | F4 |
| `RPM failed:` / `Executing the transaction failed` | F5 |
| `scriptlet failed` | F6 |
| `needs …MB … on the / filesystem` | F7 |
| `Error: signal` (log) | H1 |
| `Unknown section` / `Unknown entry in [main]` (log) | K1 |

---

## 3. Historic bug classes (libzypp.changes, Jan 2022 – Sep 2026)

Derived from the ~190 changelog bullets with a `bsc#`/`boo#`/`jsc#`/`fixes #` reference between line 1 and the
2022-01-10 entry of `zypp/package/libzypp.changes`. Counts are approximate (one bullet = one count; some bugs
appear in several bullets). GitHub `fixes #NNN` items are mostly build/test issues and are counted under "build".

| Area | ~Count | Representative references | What it tells you |
|---|---|---|---|
| **Media / download / auth / proxy / mirrors** | 30 | bsc#1210870 (credential loop), bsc#1245220 + bsc#1213673 (false "filesize exceeded" on 401/407), bsc#1212187 (HTTP/2 header trailing space), bsc#1244710/bsc#1247690 (libproxy), bsc#1230912 (hang without network), bsc#1208329 (`connect_timeout`), bsc#1240132/bsc#1253193 (mirrors in media backends), bsc#1273242, bsc#1244105, bsc#1238315 (FTP double slash), bsc#1218831 (redirect protocols), bsc#1196061 (ISO release), bsc#1245452 (MediaDISK SEGV) | Largest cluster. The MediaCurl2/mirror rework moved a lot here. Always compare `ZYPP_CURL2=0/1` |
| **Commit / rpm / zypp-rpm / %posttrans / preloader / lock** | 27 | bsc#1216091 + bsc#1246149 + bsc#1257068 (rpm-4.18 `--runposttrans`), bsc#1243279 (`$1` for %posttrans), bsc#1189788/bsc#1236481 (usrmerge vs singletrans), bsc#1246912 (hwcaps subarch), bsc#1253740/bsc#1247054/bsc#1239543 (preloader), bsc#1197134 (singletrans hang), bsc#1196925 (zypp-rpm protocol), bsc#1203760 (two Targets), bsc#1213231/bsc#1239809 (lock timeout), bsc#1228647 (src.rpm), bsc#1222044 (triggers) | Interaction with rpm versions is the main risk. Check the rpm version first |
| **Repos / refresh / services / cache / variables** | 16 | bsc#1237044 (repo var context), bsc#1236820 (fetch order), bsc#1225451 (no-permission), bsc#1223094 (volatile media), bsc#1222086 (outdated check as user), bsc#1222398 (cache dir mode), bsc#1212160 (vars in .repo URL), bsc#1227625 (repoindex parse error), bsc#1204956/bsc#1210740 (auto prune), bsc#1246466 (probe), bsc#1249435 (SELinux tmp dir) | Many are installer/YaST flows or non-root use |
| **Security hardening (CVE) & GPG** | 11 | CVE-2026-44942/bsc#1267874 (`path=`), CVE-2026-44941/bsc#1267426 (keyhint path), CVE-2026-25707/bsc#1259802 (locations outside repo), CVE-2026-44933/bsc#1265223 (sigcheck script dir), bsc#1274625 (strict repo_gpgcheck), bsc#1271730 (long key ids), bsc#1259706 (trusted key updates), bsc#1184501 (header+payload signature), bsc#1240529 (SHA1/FIPS), bsc#1226030 (ssl_clientkey in URLs) | Hardening changes produce **new rejections** of previously accepted repos — expect follow-up reports after each such release |
| **Solver / pool / locks / kernels / products** | 16 | bsc#1279541 + bsc#1243486 (libsolv minimums for migrations), bsc#1277790 (log all problem rules), bsc#1239718 (purge -rc kernels), bsc#1225267 (lock buddies), bsc#1221525 (`removeOrphaned`), bsc#1175678 (`removeUnneeded`), bsc#1203248 + bsc#1194848 (PTF), bsc#1201972 (`up` recommends), bsc#1196368 (flags on target change), bsc#1259311 (referencePackage), bsc#1244042 (autotestcase) | Few real solver-logic bugs in libzypp; most solver reports end as metadata/libsolv or "works as designed" |
| **zypper ps / CheckAccessDeleted / needreboot** | 6 | bsc#1229106, bsc#1226014, bsc#1218782, bsc#1218291, bsc#1205636, bsc#1200993 | Container environments keep breaking detection |
| **API/ABI/build/compat & consumers** | ~25 | bsc#1230267 (private symbols used by zypper), bsc#1228420/bsc#1228138 (exports for YaST), bsc#1236983 (deprecation warnings vs libyui `-Werror`), bsc#1194597 (C++11 headers on old distros), bsc#1227793/bsc#1231043 (plugin STOMP compat), jsc#PED-8153 (visibility=hidden, dynamic libproxy) | Visibility switch caused a burst of "undefined symbol" reports — see [12](12-release-packaging-and-compat.md) |
| **Config** | 3 | bsc#1272534 (drop-in section), boo#1215979, bsc#1239809 | Expect more since the UAPI/econf switch (PED-14658) |

Older recurring themes visible in code comments and earlier changelog sections: vendor change defaults (bnc#754286),
lock query equality (bnc#792901, bsc#1112911), refresh delay (bsc#1174016), chroot `/proc` (bsc#1181328).
Use `grep -n 'bsc#NNNN' zypp/package/libzypp.changes` and `git log -S'bsc#NNNN'` to locate fixes.

---

## 4. Reproduction recipes (details: [11](11-testing.md) §15)

| Subsystem | Recipe |
|---|---|
| Solver | Unpack the testcase; `testsolv testcase.t` (pure libsolv) and stack `deptestomatic -vv <dir>` (libzypp semantics); diff with distro `/usr/lib/zypp/testsuite/bin/deptestomatic -vv`. Edit `zypp-control.yaml` / `solver-test.xml` to try flags. Inspect packages with `zypp-NameReqPrv --root <dir> -D <pkg>`. Turn into `zypp/tests/data/TC*` + test via `loadTestcaseRepos`. Run as non-root (root takes the global lock) |
| Refresh / signatures | `zypper --root /tmp/zypp-test/r ar <url> x; zypper --root /tmp/zypp-test/r -vvv ref` with `ZYPP_LOGFILE=- ZYPP_MEDIA_CURL_DEBUG=2`. Toggle `ZYPP_CURL2`. For key issues inspect `/tmp/zypp-test/r/var/cache/zypp/raw/x/repodata/`. In tests: `WebServer` handlers (redirect, 401, 404) |
| Media / auth | `zypp/tools/DownloadFiles` against the URL; `WebServer` + `makeBasicAuthHandler` in a unit test ([07](07-media-and-downloads.md)) |
| Commit / rpm | Chroot target: `zypper --root R in --download-only`, then a real install into `R`; compare `ZYPP_SINGLE_RPMTRANS=0/1` and `ZYPP_PCK_PRELOAD=0/1`; `ZYPP_RPM_DEBUG=1`. Set `ZYPP_LOCKFILE_ROOT` if the host zypper runs |
| Config | `ZYPP_CONF=/path` (relative to root) or a fake root with `etc/zypp/zypp.conf.d/`; check the `zconfig` lines. Unit: `EconfDict_test`, `zyppng/tests/config/*` |
| Locks / queries | Copy `/etc/zypp/locks` into a testcase root; print `PoolQuery` with `MIL << query`; `Locks_test` |
| zypper ps | Capture `lsof -n -FpcuLRftkn0 -K i` output and feed `CheckAccessDeleted::check()` |
| Kernel purge | `zypp/tools/zypp-runpurge --uname <r> [--keepSpec …] <testcase-dir>`; data set under `zypp/tests/zypp/data/PurgeKernels/` |
| Crash | Debug build in the stack with `ENABLE_UBSAN_CHECKS` or ASAN flags ([11](11-testing.md) §16); run the stack's zypper against `--root` |

---

## 5. Escalation map

| Reassign to | When | How to tell |
|---|---|---|
| **libsolv** (component libsolv, upstream openSUSE/libsolv) | Solver result/problem is wrong in pure `testsolv testcase.t`; slow solving; `solver_trivial_installable` patch status; repo2solv/rpmdb2solv parse issues | Same transaction/problem with `testsolv` as with zypper. libzypp job/flag translation matches (`testcase.t` `job`/`solverflags` vs `zypp-control.yaml`) |
| **rpm** | Error text from rpm (`error: …`, `%posttrans` runner bugs, rpmdb corruption, `rpmkeys -K` disagrees with expectations, `--runposttrans` bugs) | Reproducible with plain `rpm -Uvh --test` / `rpmkeys -Kv`. Check rpm version (4.18.0 chroot bug) |
| **Package maintainer** (the package's component) | Scriptlet failures, file conflicts, wrong requires/conflicts/obsoletes, wrong `installhint(reboot-needed)`, wrong `Provides: ptf()` | rpm output in history; `rpm -qp --scripts`; conflict exists in metadata (`zypper info --requires`) |
| **zypper** (openSUSE/zypper) | Wrong prompts, defaults, option handling, exit codes, output formatting, `zypper patch` update-stack policy, `zypper up` using `doUpdate()`, non-interactive defaults, `zypper ps` presentation | Same libzypp API call from another consumer behaves correctly; code is in `zypper/src/` (callbacks in `src/callbacks/`) |
| **YaST** (yast2-pkg-bindings, yast2-packager, libyui-*-pkg) | Only in YaST UI; selection/status semantics in the package selector; installer media flows; registration UI | Not reproducible with zypper using equivalent flags; y2log shows the Ruby/pkg-bindings side failing |
| **PackageKit** (backends/zypp) | Only in GNOME Software/Discover/pkcon; lock held between jobs; key acceptance; proxy env; offline update flow | Reproduce with `pkcon`; `/var/log/pk_backend_zypp`; zypper is fine |
| **Agama** | Only in the Agama installer | Journal logs from the zypp-agama C layer |
| **Repo owner / OBS / IBS / maintenance / mirror admins** | 404s, unsigned or wrongly signed metadata, expired keys, out-of-sync mirrors, missing packages during publishing, invalid XML | `curl` the files directly; signature mismatch fixes itself after mirror sync; same for all clients |
| **SCC / registration (SUSEConnect)** | 401/403 on SCC repos, wrong services, products not activated | 403 hint text (B8), `SUSEConnect -s`, service URL tokens |
| **snapper / zypp-plugin authors** | Snapshot behaviour, plugin protocol errors | `zypp::plugin` log lines, plugin package version |
| **Keep in libzypp** | Refresh/cache decisions, keyring workflow, media/auth handling, commit orchestration, config parsing, locks/pool state, solver job construction, testcase writer, crashes in libzypp frames | — |

Before reassigning a solver bug, verify the flags: a different `allowvendorchange`, `onlyrequires`, `allowuninstall`
or a sticky mode (`doUpgrade`/`doUpdate`/`verifySystem`) in a front end explains many "libsolv bugs".

---

## Pitfalls (triage mistakes)

- Treating `<5>` INT `THROW:`/`CAUGHT:` lines as the bug. Most are handled; find the last unhandled one before the user-visible error.
- Reading the wrong run: the log interleaves processes. Always cut by PID (`zypper-log <PID>`).
- Looking for `/var/log/zypp/zypper.log` — zypper logs to `/var/log/zypper.log`; history is `/var/log/zypp/history`.
- Expecting `zypper -v` to make the log more verbose. Only `ZYPP_FULLLOG` / the `*_DEBUG` env vars do.
- Assuming a solver testcase from `--root` lives below the root. It is always `/var/log/zypper.solverTestCase` and is overwritten by the next `--debug-solver` run.
- Replaying a testcase with your host's `/etc/zypp/systemCheck` or as root (global lock); SystemCheck caps may be applied twice.
- Blaming libzypp for "nothing provides" during repo publishing windows. Re-check after the next mirror sync.
- Forgetting front-end flags: YaST, PK and zypper set different solver flags, and zypper `up` never shows problems.
- Debugging signature failures without looking at the downloaded file: an HTML proxy/portal page or a half-synced mirror explains most of them.
- Missing that `/etc/zypp/zypp.conf` masks the whole vendor zypp.conf (including `multiversion`), and that drop-ins need a `[main]` header.
- Trusting the exit code alone: a download failure installs nothing and may still look "clean" (`noError`) with `--download-only`; preload misses abort without fallback.
- Not checking whether libzypp, zypper, libsolv and zypp-rpm come from matching builds (ABI and zypp-rpm protocol).
- Assuming the lock protects `--root` operations separately; it uses the host `/run/zypp.pid` unless `ZYPP_LOCKFILE_ROOT` is set.
- Quoting old message texts (*Valid metadata not found at specified URL*) to grep the source: they no longer exist; use the strings in §2.
- Closing hardening fallout as "invalid": strict `repo_gpgcheck` (bsc#1274625), long key ids (bsc#1271730) and `path=` restrictions (bsc#1267874) intentionally reject formerly accepted repos. Point the reporter to the repo owner and document it.
- Asking for `/etc/zypp` as a whole tarball without excluding `credentials.d` / `credentials.cat`.

## Where to start

1. Get versions (`libzypp: …` line) and the exact command; cut the run out of `/var/log/zypper.log` with `zypper-log <PID>`.
2. Find the phase with the markers in §1.3, then jump to the symptom entry (§2, quick table §2.L).
3. Ask for the right data (§1.2): testcase for solver issues, `ZYPP_MEDIA_CURL_DEBUG=2` log for downloads, history + `ZYPP_RPM_DEBUG=1` for rpm, zconfig lines for configuration.
4. Classify ownership (§1.4, §5) before reading code: `testsolv` for libsolv, `rpmkeys`/`rpm --test` for rpm, `curl` for repo servers, another front end for zypper/YaST/PK.
5. Reproduce in the zypp-stack build with `--root` or a testcase (§4, [11](11-testing.md)); switch backends (`ZYPP_CURL2`, `ZYPP_SINGLE_RPMTRANS`, `ZYPP_PCK_PRELOAD`) to isolate.
6. Search history: `grep -n -i '<keyword>' zypp/package/libzypp.changes`, `git log -S'<log string>'` — the log string from §2 leads directly to the code (`grep -rn '<string>' zypp zypp-logic zyppng ../zypper/src`).
7. Deep dives: [04 Solver](04-solver.md), [05 Repositories](05-repositories-and-services.md), [06 GPG](06-security-gpg-keyring.md), [07 Media](07-media-and-downloads.md), [08 Commit](08-target-commit-and-rpm.md), [09 Configuration](09-configuration-and-environment.md), [14 Consumers](14-consumers-and-api-surface.md), cross-cutting [13 Pitfalls](13-pitfalls-and-special-cases.md).
