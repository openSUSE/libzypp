# Testing, CI and Bug Reproduction

This chapter covers the libzypp test suites (legacy `zypp/tests`, next-gen `zyppng/tests`), the shared test helpers in
`zypp-logic/tests/lib` (`TestSetup`, `WebServer` and friends), how to run single tests, what CI actually exists, the
manual-testing tools in `zypp/tools`, and how to reproduce user bug reports with solver testcases and logs. It also
explains the parent `zypp-stack` meta project, which builds libsolv + libzypp + zypper + the testcase runner together.

---

## 1. Overview: where tests live

```
libzypp/
├── CMakeLists.txt                  ENABLE_BUILD_TESTS, DISABLE_MEDIABACKEND_TESTS options; INCLUDE(CTest)
├── zypp/tests/                     legacy suite (Boost.Test, links static zypp-allsym)
│   ├── CMakeLists.txt              ADD_TESTS() macro, TESTS_* defines, pulls in zypp-logic/tests/lib
│   ├── README                      the "--catch_system_errors=no" note
│   ├── data/                       shared legacy test data (solver testcases TC*, openSUSE-11.1 repo, ...)
│   ├── media/  parser/  repo/  sat/  zypp/  zypp/base/
│   └── zyppng/                     *old* async-core tests (EventLoop, io/, rpc/, media/) built against legacy zypp
├── zypp-logic/tests/
│   ├── lib/                        TestSetup.h, TestTools, WebServer, FtpServer, ProxyServer (static helper lib)
│   └── data/                       server configs (nginxconf, vsftpconf, proxyconf, lighttpdconf), SSL certs, http/ files
└── zyppng/tests/                   next-gen suite (C++20 modules; links zyppng-objlib-test)
    ├── async/ config/ media/ sat/
    └── data/provider/tvm/          fake multi-CD media for the Provider test
```

Test data that only one area uses usually lives next to the test (`zypp/tests/zypp/data/<TestName>/`,
`zypp/tests/repo/yum/data`, `zypp/tests/parser/inifile/...`, `zyppng/tests/config/data/zypp.conf.ALL_KEYS`).

### Build switches

| Option (root `CMakeLists.txt`) | Default | Effect |
|---|---|---|
| `ENABLE_BUILD_TESTS` | OFF, but forced ON when `-DSUSE` or `-DDEBIAN` is set; OFF with `NOSCHNICKSCHNACK` | `zypp/CMakeLists.txt` adds `tests` normally; otherwise `ADD_SUBDIRECTORY( tests EXCLUDE_FROM_ALL )` (tests still configured, just not built by `make all`). |
| `DISABLE_MEDIABACKEND_TESTS` | OFF | Skips tests needing nginx/vsftpd/squid/libfcgi and does not compile `WebServer/FtpServer/ProxyServer` into the helper lib. |
| `DISABLE_LIBPROXY` | OFF | If OFF, `Proxy_test` is built and **libproxy is a hard requirement** (`FATAL_ERROR "libproxy not found"`). |
| `ENABLE_ZCHUNK_COMPRESSION` | OFF | Adds `ZChunk_test`; changes expectations in `MediaCurl_test` (`#ifdef ENABLE_ZCHUNK_COMPRESSION`). |
| `BUILD_LIBZYPPNG` | OFF | Without it `zyppng/` (and thus `zyppng/tests`) is not built at all. |

Note: Boost `unit_test_framework` is found unconditionally (`FIND_PACKAGE(Boost REQUIRED COMPONENTS unit_test_framework thread)`).

Spec file (`zypp/libzypp.spec.cmake`): `%bcond_without mediabackend_tests` → media backend tests are on in OBS builds and
pull in `BuildRequires: nginx vsftpd squid FastCGI-devel` (SUSE) / `fcgi-devel` (others). `%check` runs:

```
LD_LIBRARY_PATH="$(pwd)/../zypp:$LD_LIBRARY_PATH" ctest --output-on-failure .
```

So **every OBS package build runs the full test suite**, including the nginx/squid/vsftpd backed ones. A flaky media test
fails the package build.

---

## 2. The legacy suite: `zypp/tests`

### 2.1 The `ADD_TESTS` macro

`zypp/tests/CMakeLists.txt`:

```cmake
ADD_DEFINITIONS( -DTESTS_SRC_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
                 -DTESTS_SHARED_DIR="${zypp-libs_SOURCE_DIR}/zypp-logic/tests"
                 -DTESTS_BUILD_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
SET(TESTLIBNAME zypp_test_utils)  ; ADD_SUBDIRECTORY(<zypp-logic/tests/lib> ${CMAKE_CURRENT_BINARY_DIR}/lib)

MACRO(ADD_TESTS)            # for each NAME:
  NAME_test.cc compiled with -DBOOST_TEST_DYN_LINK -DBOOST_TEST_MAIN -DBOOST_AUTO_TEST_MAIN=""
  ADD_EXECUTABLE(NAME_test ...)
  link: zypp_exe_compiler_flags, Boost unit_test_framework, zypp_test_utils, zypp-allsym
  ADD_TEST(NAME_test <bindir>/NAME_test --catch_system_errors=no)
```

Consequences:

* One executable per `*_test.cc`, the ctest name equals the target name (`KeyRing_test`, `Resolver_test`, ...).
* `TESTS_SRC_DIR` is **the `zypp/tests` root** (set once at top level), so data paths look like
  `TESTS_SRC_DIR"/data/TCdup"` or `TESTS_SRC_DIR"/zypp/data/Fetcher/remote-site"`. Exception: subdirectories may redefine
  it (e.g. `zyppng/tests/config/CMakeLists.txt` re-adds `-DTESTS_SRC_DIR` for its own dir).
* `TESTS_SHARED_DIR` points into the **source tree** `zypp-logic/tests` — server configs and SSL material are used in place.
* Tests link **`zypp-allsym`** (`zypp/zypp/CMakeLists.txt`: `add_zypp_lib( zypp-allsym STATIC )`, "library with more
  symbols for tests etc"), not the shared `libzypp.so` with its `libzypp.map` version script. A green test run therefore
  says nothing about whether a symbol is exported to zypper/YaST. Check `zypp/zypp/libzypp.map` separately (see
  [Release & compat](12-release-packaging-and-compat.md)).
* `--catch_system_errors=no` is passed by ctest. If you run binaries by hand, pass it yourself (or
  `BOOST_TEST_CATCH_SYSTEM_ERRORS=no`). Otherwise Boost reports "child has exited" for tests that fork
  (`ExternalProgram`, gpg, rpm) — see `zypp/tests/README`.
* The top-level CMake also defines a custom target `ctest` (`ctest -VV -a`), i.e. `make ctest` in the tests build dir.

### 2.2 Per-area layout

| Dir | Tests (registered in CMake) | Notes |
|---|---|---|
| `zypp/tests/zypp/` | Arch, Capabilities, CheckSum, ContentType, CpeId, Date, DrunkenBishop, Dup, Digest, Deltarpm, Edition, ExtendedPool, FileChecker, Flags, GZStream, InstanceId, KeyRing, Locale, Locks, MirroredOrigin, PathInfo, Pathname, PluginFrame, PoolQueryCC, PoolQuery, ProgressData, PtrTypes, PublicKey, PurgeKernels, RWPtr, RepoManager, RepoStatus, ResKind, Resolver, ResStatus, RpmPkgSigCheck, Selectable, SetRelationMixin, SetTracker, StrMatcher, StringV, Target, Url, UserData, Vendor; + Proxy (libproxy), ZChunk (zchunk); + Fetcher, MediaSetAccess, RepoInfo (media backend) | Core value types and high-level classes. Data in `zypp/tests/zypp/data/<Name>`. `KeyRingTestReceiver.h` is a reusable KeyRing callback receiver (also included by repo/ and sat/ tests via an extra include dir). |
| `zypp/tests/zypp/base/` | Glob, Sysconfig, String, ExternalProgram | |
| `zypp/tests/media/` | CredentialManager, CredentialFileReader, MediaProducts, MetaLinkParser; MediaCurl (media backend) | `MediaCurl_test` is the main HTTP(S)/proxy test, data-driven over `{ssl, no-ssl} × {"curl","curl2"}` backends (via `?mediahandler=`). |
| `zypp/tests/parser/` | ProductFileReader, RepoFileReader, RepoindexFileReader, HistoryLogReader, JsonParser; `yum/` RepomdFileReader; `inifile/` inidict, iniparser, EconfDict | |
| `zypp/tests/repo/` | DUdata, ExtendedMetadata, PluginServices, RepoLicense, RepoSigcheck, RepoVariables; MirrorList (media backend); `yum/` YUMDownloader; `susetags/` Downloader | |
| `zypp/tests/sat/` | Blacklisted, IdString, LookupAttr, Pool, Queue, Map, Solvable, SolvableSpec, SolvParsing, WhatObsoletes, WhatProvides | Pool/solvable level tests, mostly built on `TestSetup` + `zypp/tests/data`. |
| `zypp/tests/zyppng/` | EventLoop, IOBuffer, UnixSignalSource; `io/` UnixSocket, AsyncDataSource, Process; `rpc/` StompMessageStream; `media/` NetworkRequestDispatcher (media backend) | Tests of the `zypp-logic/*/ng` async core as compiled into *legacy* zypp — not the module-based `zyppng/tests`. `EvDownloader` is commented out. |

**Orphaned test sources** (present on disk, never built, verified by cross-checking against CMakeLists):
`zypp/tests/media/{media1,media2,media3,media4,file_exists,throw_if_not_exists}_test.cc`,
`zypp/tests/zypp/Signature_test.cc`, `zypp/tests/zypp/Resolvable_test.cc`. Don't assume they compile.

### 2.3 Shared test data `zypp/tests/data`

| Dir | Kind | Used for |
|---|---|---|
| `TCdup`, `TCNamespaceRecommends`, `TCSelectable`, `TCWhatObsoletes` | Solver testcases (`zypp-control.yaml` + `*.repo` testtags files, `@System.repo` = installed system) | `Dup_test`, `Resolver_test`, `Selectable_test`, `WhatObsoletes_test` via `TestSetup::loadTestcaseRepos()` |
| `openSUSE-11.1`, `11.0-update`, `OBS_zypp_svn-11.1`, `obs_virtualbox_11_1` | Real (old) repo metadata (susetags/rpm-md) incl. gpg keys | Loading/parsing; `TestSetup::loadRepo(dir)` builds a solv cache from it |
| `PoolReuseIds/SeqA,SeqB` | Pool id reuse sequences | sat tests |
| `Mirrorlist/` | Remote site for nginx | `RepoInfo_test`, `MirrorList_test` |

---

## 3. Shared helpers: `zypp-logic/tests/lib`

`zypp-logic/tests/lib/CMakeLists.txt` builds a static lib whose name comes from the includer
(`zypp_test_utils` for legacy, `zyppng_test_utils` for NG) and whose compile flags target is passed in via
`ZYPP_COMP_FLAGS`. It is compiled **twice**, once per suite. It contains `TestSetup.h` (header only), `TestTools`, and,
unless `DISABLE_MEDIABACKEND_TESTS`, `WebServer`, `FtpServer`, `ProxyServer`.

With the media backend enabled, configure **fails** (`FATAL_ERROR`) if any of `nginx`, `vsftpd`, `squid`,
`basic_ncsa_auth` (searched in bindir/sbindir/libexecdir, `/usr/libexec`, `/usr/lib`, suffix `squid`), `libfcgi`,
`libfcgi++` is missing. Binary paths are baked in as `WEBSRV_BINARY`, `FTPSRV_BINARY`, `PROXYSRV_BINARY`,
`PROXYAUTH_BINARY`. `fastcgi/fcgiapp.h` vs `fcgiapp.h` header location is detected (`FCGI_IN_SUBDIR`).

Include path is `<tests/lib/...>` (the `zypp-logic` dir is on the include path).

### 3.1 `TestSetup` — fake root with repos and solv files

`zypp-logic/tests/lib/TestSetup.h`. Lifetime of the object = lifetime of the fake root.

```
TestSetup( Arch = Arch_empty, Options = {} )          -> root = new filesystem::TmpDir (deleted in dtor)
TestSetup( Pathname root, Arch, Options )            -> root kept; TSO_CLEANROOT wipes it first
TestSetup( TestSetup::initLater )                    -> empty; assign later (pattern for global fixtures)

Impl ctor:
  getZYpp()->finishTarget();                        // drop any previous target
  sat::Pool::instance().reposEraseAll();            // global pool is a singleton!
  ZConfig::instance().setRepoManagerRoot( root );
  if (arch) ZConfig::instance().setSystemArchitecture( arch );
```

`repomanager()` returns `RepoManager( RepoManagerOptions::makeTestSetup(root) )` (`zypp/zypp/RepoManagerOptions.cc`),
which flattens the layout:

```
<root>/
  repos.d/      knownReposPath
  services.d/   knownServicesPath
  raw/          repoRawCachePath
  solv/         repoSolvCachePath   (solv/<alias>/solv ; solv/@System/solv = faked target)
  packages/     repoPackagesCachePath
  plugins/      pluginsPath
```

Loading functions:

| Call | What happens |
|---|---|
| `loadRepo(Pathname dir)` | → `loadRepo(dir.asUrl())`: creates `RepoInfo` (alias = `host:basename` or given), **gpgcheck off** unless `TSO_REPO_DEFAULT_GPG`, then `addRepository` + `buildCache` + `loadFromCache`. Alias collision → `RepoManager::makeStupidAlias`. |
| `loadRepo(Pathname file)` | A `.solv` file: `satpool().addRepoSolv(file, RepoInfo)` directly, no RepoManager. |
| `loadRepo(const char*/string)` | Leading `/` → path, else parsed as `Url`. |
| `loadHelix(path)` | `addRepoHelix` (old helix XML testcases). |
| `loadTarget()` | `initializeTarget(root)` + `target().load()` (real rpmdb below root — usually empty). |
| `loadTargetRepo(path/url)`, `loadTargetHelix(path)` | Fake `@System` by loading a repo under `sat::Pool::systemRepoAlias()`. |
| `loadTestcaseRepos(dir, trials*)` | `misc::testcase::LoadTestcase::loadTestcaseAt` → `setupInfo().applySetup(repomanager)` → sets requested locales → `poolProxy()`. Optionally returns the trials. Throws `Exception` on failure. |
| `loadRepos()` | Loads all enabled repos from `<root>/repos.d`; rebuilds cache if not cached or type `RPMPLAINDIR`. |
| `static LoadSystemAt(dir, arch=x86_64)` | Auto-detect: solver testcase (`isTestcase`) / TestSetup dir (`isTestSetup`: has `repos.d` and `raw`) / real system (target + cached repos, uncached/broken repos are skipped with a USR message). Used by tools via `--root`. |

Accessors: `root()`, `target()` (lazily initializes), `pool()`, `poolProxy()`, `satpool()`, `resolver()`.

Typical patterns (verified in `zypp/tests/zypp/Resolver_test.cc`, `Dup_test.cc`):

```cpp
static TestSetup test( TestSetup::initLater );
struct TestInit {
  TestInit()  { test = TestSetup( Arch_x86_64 ); test.loadTestcaseRepos( TESTS_SRC_DIR"/data/TCdup" ); }
  ~TestInit() { test.reset(); }
};
BOOST_GLOBAL_FIXTURE( TestInit );
```

Applying a testcase setup goes through `zypp/zypp/misc/TestcaseSetup.cc`, which calls
`setenv("ZYPP_TESTSUITE_FAKE_ARCH", ...)` — this is **process wide** and persists after the `TestSetup` is gone. With it
set, a commit throws (it is a guard against committing a faked system). Never mix "load a testcase" and "commit to a real
root" in the same test process.

Enabling logs inside a test (comment in `TestSetup.h`):
```cpp
base::LogControl::TmpLineWriter shutUp( new log::FileLineWriter( "/tmp/YLOG" ) );
```
or simply run the binary with `ZYPP_LOGFILE=-` (or a file) and `ZYPP_FULLLOG=1` for excessive logging
(`zypp-logic/zypp-core/base/LogControl.cc`). Also `TestSetup.h` provides `LABELED(V)` and a fallback `BOOST_CHECK_NE`.

### 3.2 `TestTools`

`TestTools::readFile`, `writeFile`, `checkLocalPort(port)` (TCP connect to 127.0.0.1). `ZYPP_REQUIRE_THROW(stmt, exc)`
wraps `BOOST_REQUIRE_THROW` and fails if a *different* exception escapes. With coroutine support it provides
`ZYPP_CORO_TEST_CASE(name)`: declares a fixture whose `zyppng::Task<void> coro_body()` is started; if not ready
immediately, an `EventLoop` runs until the task notifies; `res.get()` rethrows exceptions.

### 3.3 `WebServer` — nginx + FastCGI

`zypp-logic/tests/lib/WebServer.{h,cc}`. Default port **10001**, optional SSL.

```
 test thread                         worker thread                      nginx (child, ExternalTrackedProgram)
 ───────────                         ─────────────                      ───────────────────────────────────
 WebServer web(docroot, 10001, ssl)
 web.start() ───────────────► write config into TmpDir:
                               nginx.conf  -> symlink data/nginxconf/nginx.conf
                               srvroot.conf "root <docroot>;"
                               fcgisock.conf "fastcgi_pass unix:<tmp>/fcgi.sock;"
                               user.conf  ("user root;" only if uid==0)
                               port.conf  listen <port> [ssl]; + [::]
                               ssl.conf   -> ssl.conf | nossl.conf
                               cert.pem/cert.key -> data/webconf/ssl/server.{pem,key}
                              FCGX_OpenSocket(fcgi.sock)
                              exec nginx -p <tmp> -c <tmp>/nginx.conf ───► serves "/" from docroot
                              wait ≤10 s for port (checkLocalPort)         "/handler/*" → FastCGI socket
  ◄──── cv notify ───────────  poll(wakeup pipe, fcgi socket, nginx stdout)
                              FCGX_Accept_r → lookup handler by SCRIPT_NAME minus "/handler/"
                              → registered RequestHandler or default (404)
 web.stop() ─ write wakeup pipe → worker exits loop, SIGTERM nginx, join
```

API: `url()` (`http(s)://localhost:<port>`), `caPath()` = `zypp-logic/tests/data/webconf/ssl/certstore`,
`transferSettings()` (sets CA path), `addRequestHandler(path, fn)`, `setDefaultHandler`, `removeRequestHandler`,
helpers `makeResponse(...)`, `makeResponseString`, `makeBasicAuthHandler(cred, fn)`, `makeDefaultHandler()`.
A handler writes a raw CGI response to `req.rout` (e.g. `"Location: /primary.xml.zck\r\n\r\n"`); request env vars are in
`req.params` (`REQUEST_URI`, `HTTP_AUTHORIZATION`, ...). `start()` throws `"Invalid docroot"` if the docroot does not exist,
and returns false if nginx did not come up.

There is an `#ifdef HTTP_USE_LIGHTTPD` alternative path (configs in `zypp-logic/tests/data/lighttpdconf`, hard-coded
`/usr/sbin/lighttpd`); it is not enabled by CMake — legacy.

`FtpServer` (vsftpd, `data/vsftpconf`) and `ProxyServer` (squid + `basic_ncsa_auth`, `data/proxyconf`, default port
**10002**) follow the same shape (`start/stop/port/url`).

**Nearly all users of `WebServer` hard-code port 10001** (`MediaCurl_test`, `RepoInfo_test`, `Fetcher_test`,
`MirrorList_test`, `NetworkRequestDispatcher_test`, NG `Provider_test`); `MediaSetAccess_test` runs its WebServer/FtpServer
on **10002**, the same port as `ProxyServer` in `MediaCurl_test`. `FtpServer` also defaults to 10001. No ctest
`RESOURCE_LOCK`/`RUN_SERIAL` is set anywhere, so
**`ctest -j N` can make these tests collide**. Run them serially (or `ctest -j N -E 'MediaCurl|Fetcher|RepoInfo|MirrorList|MediaSetAccess|Provider|NetworkRequestDispatcher'` plus a serial pass).

---

## 4. The next-gen suite: `zyppng/tests`

Only built with `BUILD_LIBZYPPNG=ON` (on in the stack build; off by default in the spec: `%bcond_with build_libzyppng`,
and forced off for `suse_version < 1600`).

`zyppng/tests/CMakeLists.txt` mirrors the legacy macro, with differences:

* Defines also `ZYPPNG_WORKERS_DIR` (`${ZYPPNG_WORKER_BINARY_DIR}` = `<build>/zyppng/tools/workers`).
* Each test gets `CXX_SCAN_FOR_MODULES ON` and `CXX_STANDARD 20` so `import zyppng;` works (CMake ≥ 3.28).
* Links **`zyppng-objlib-test`**, the "test twin" of the module object library (`zyppng/lib/zypp/CMakeLists.txt`):
  same module sources, compiled with `ZYPPNG_TEST_BUILD=1`, which turns `ZYPPNG_TEST_EXPORT`
  (`zyppng/lib/zypp/ng/zypp_test_export.h`) into `export`. Module-internal symbols marked `ZYPPNG_TEST_EXPORT` become
  visible to tests without leaking into production (`zyppng-objlib`). Cost: the whole module is compiled twice.
* Comments in `zyppng/lib/zypp/CMakeLists.txt` refer to a helper `zyppng_add_module_test()` — it does not exist in the
  tree (stale comment). Tests are added with the plain `ADD_TESTS()` macro (section 12.5).

| Dir | Tests | Notes |
|---|---|---|
| `async/` | Task, Pipelines | coroutine `Task<>` and async pipeline combinators |
| `config/` | Config, ConfigCompat, ConfigParsers | uses `config/data/zypp.conf.ALL_KEYS`; ConfigCompat checks NG config vs legacy `ZConfig` semantics ([Configuration](09-configuration-and-environment.md)) |
| `media/` | Provider (media backend only) | links `tvm-protocol-obj`; uses `data/provider/tvm/cd1..cd4` (fake multi-medium "tvm" test-volume backend) |
| `sat/` | PoolNg, PoolComponentSet | NG pool |

---

## 5. Running tests

Build dir layouts:

| Build | Legacy test binaries | NG test binaries |
|---|---|---|
| standalone libzypp (`libzypp/build`) | `build/zypp/tests/<area>/<Name>_test` | `build/zyppng/tests/<area>/<Name>_test` |
| zypp-stack (`zypp-stack/build`) | `build/libzypp/zypp/tests/<area>/<Name>_test` | `build/libzypp/zyppng/tests/<area>/<Name>_test` |

```bash
# everything
ctest --output-on-failure                     # from the build dir (or a subdir to restrict)
# by regex on test name
ctest -R '^Resolver_test$' --output-on-failure -V
ctest -R 'RepoManager|RepoInfo'
ctest -E 'MediaCurl'                           # exclude
# directly (more control, Boost.Test options)
cd build/zypp/tests/zypp
./Resolver_test --catch_system_errors=no --log_level=all
./Resolver_test --catch_system_errors=no --run_test=install      # single BOOST_AUTO_TEST_CASE
./Resolver_test --list_content
ZYPP_LOGFILE=- ZYPP_FULLLOG=1 ./Dup_test --catch_system_errors=no 2>&1 | less
gdb --args ./KeyRing_test --catch_system_errors=no
```

Notes:
* Run ctest **from the build dir**. Running it from the source root leaves an untracked `Testing/` directory
  (`libzypp/Testing/Temporary/LastTest.log`, `CTestCostData.txt`) — this exists in the current checkout, is not tracked
  by git, and is not referenced anywhere; it is safe to delete. Don't commit it.
* The spec sets `LD_LIBRARY_PATH` to `../zypp` before ctest; locally with RPATH (no `CMAKE_SKIP_RPATH`) this is unneeded.
* Many env lookups in libzypp are cached in function-local statics; `setenv()` inside a test may not take effect if the
  value was already read. Set env vars **before** launching the binary.
* The `TestSetup` / `ZConfig` / `sat::Pool` / `getZYpp()` objects are process singletons. Tests within one binary share
  them; order of `BOOST_AUTO_TEST_CASE`s matters in several files.

---

## 6. Writing a new test

1. Put `Foo_test.cc` into the matching area directory and add `Foo` to that directory's `ADD_TESTS(...)`.
   Guard with `IF( NOT DISABLE_MEDIABACKEND_TESTS )` if it needs `WebServer`/`ProxyServer`/`FtpServer`.
2. Start with `#include <boost/test/unit_test.hpp>` (main is generated by the macro flags) and, if needed,
   `#include <tests/lib/TestSetup.h>`.
3. Data: `TESTS_SRC_DIR"/zypp/data/Foo"` (legacy) or the area's `data/`. Server docroots must exist (or `start()` throws).
4. For solver regressions, prefer a **solver testcase directory** under `zypp/tests/data/TC<Name>/` loaded with
   `loadTestcaseRepos()` — you can produce one with `zypper --debug-solver` and trim it (section 9).
5. NG: add to `zyppng/tests/<area>/CMakeLists.txt`; use `import zyppng;` and `ZYPP_CORO_TEST_CASE` for async code.
   If you need an internal symbol, mark its declaration `ZYPPNG_TEST_EXPORT` rather than exporting it.
6. Policy per `AGENTS.md`: legacy `zypp/` is frozen — new tests for legacy behaviour are fine, but fixes belong where the
   code lives (often `zypp-logic/`).

---

## 7. CI

What actually runs (verified files; there are no Jenkinsfiles, GitLab CI or `_service` files in libzypp):

| File | Trigger | What it does |
|---|---|---|
| `libzypp/.obs/workflows.yml` | GitHub **pull requests** (`ci_workflow`, `filters: event: pull_request`) | OBS SCM/CI integration: branches `zypp:ci:libzypp/libzypp` and `zypp:ci:libzypp-glib/libzypp-glib` into `home:zypp-team`, building the PR. Because `%check` runs ctest, **this is where tests run in CI**. A `master_workflow` (push to master) is commented out. |
| `libzypp/.github/workflows/build-and-publish-doc.yml` | git **tag** push, or manual | Tumbleweed container, installs build deps (incl. nginx/squid/vsftpd/FastCGI), `cmake .. && make doc`, deploys `build/zypp/doc/autodoc/html` to GitHub Pages. No tests. |
| `libzypp/.travis.yml` | — | Dead (Travis, `opensuse:tumbleweed` docker, paths like `build/tests` no longer exist). Ignore / candidate for removal. |

Parent stack: `zypper/.obs/workflows.yml` (branches `zypp:ci:zypper/zypper` into `home:zypp-team`, no filter),
`mcp-server-zypp/.obs` + `_service`, `obs-service-cpio-repack/_service`, `libsolv/.github`, `yast-pkg-bindings/.github`.

Practical consequence: there is **no pre-merge test run outside OBS**. Before merging, build with tests locally
(media backend enabled, since OBS enables it) and check the OBS `home:zypp-team` branch result on the PR.
Releases/submissions are covered in [Release & packaging](12-release-packaging-and-compat.md).

---

## 8. Manual testing tools: `zypp/tools`

`zypp/tools/CMakeLists.txt` globs every `*.cc` in the directory and builds **each as its own executable**, linked against
shared `zypp`, except `CalculateReusableBlocks` and `DownloadFiles`, which link `zypp-allsym` (need internal symbols).
Only `zypp-NameReqPrv` is installed (`/usr/bin`) plus `zypp-rpm` (subdir; the commit helper, see
[Target & commit](08-target-commit-and-rpm.md)). The rest are developer tools in the build dir.

| Tool | Purpose (from its usage text / source) |
|---|---|
| `zypp-NameReqPrv [--root DIR] [opts] NAME...` | **The** pool inspection tool. Loads enabled repos (no refresh) and searches NAME (regex; `-x` glob) in names/provides/requires/conflicts/obsoletes/recommends/supplements/enhances (`-n -p -r -c -o -m -s -e`, `-a`/`-A` shortcuts), `--installed`, `-B` buildtime, `-D pkg` dump deps. `--root` may point to a **solver testcase** dir, which is then loaded (via `TestSetup::LoadSystemAt` logic). Indispensable for analysing bug-report testcases. |
| `DumpSelectable [--root DIR] NAME...` | Loads repos and prints `ui::Selectable` objects (installed/available/candidate view as YaST/zypper see it). |
| `zypp-list [--root DIR] COMMAND` | List pool items according to a command (see its usage for commands). |
| `ToolScanRepos [-r ROOT] URL...` | Loads repos from URLs into a test system below `/tmp/<appname>` (or `/tmp/ROOT`) — quick "does this repo parse / build a solv" check. |
| `DownloadFiles [OPTIONS] <control-file>` | Exercises the download machinery with a list of files from a control file. |
| `CalculateReusableBlocks <metalink> <deltafile>` | Metalink/zchunk-style delta: how many blocks of a local file can be reused. |
| `ProvideSignedDirectory <url> <dir>` | Downloads a (signed) directory recursively via `MediaSetAccess` into a tmp dir. |
| `DiskUsageCounter` | Prints the default mountpoint set used for disk-usage computation. |
| `patch_find_bug [--root] bugnumber...` | Find patches referring to a bug number. |
| `percent-encode` | URL percent-encoding helper. |
| `zypp-cpeid [CPEID]...` | Parse/print CPE IDs. |
| `zypp-pubkey [key-files]` | Without args: info about all `gpg-pubkey`s in rpmdb; with files: key info and whether it is in the rpmdb. |
| `zypp-CheckAccessDeleted [--debugFile f] [--zypper]` | Runs `CheckAccessDeleted` (processes using deleted files, cf. `zypper ps`). |
| `zypp-runpurge [opts] path/to/testcase` | Computes which kernels `PurgeKernels` would remove for a spec on a testcase (purge-kernels debugging). |
| `zypp-install` | Demo: init, refresh, `dup`, resolve, commit in **DryRun/DownloadOnly**; comment warns "be careful if running it as root". |
| `migrate-sources/` | Old source → repo migration tool (subdir, not part of the glob). |
| `refresh-ng/` (`ENABLE_DEVTOOL_REFRESHNG`), `repomirror/` (`ENABLE_DEVTOOL_REPOMIRROR`, needs Notcurses++) | Devtools for the NG refresh/mirror code. Off by default (and forced off in the stack); bit-rotted, don't expect them to build. |
| `ScanTwHistory` | Python script scraping `download.opensuse.org/history/list` (Tumbleweed snapshot history). |
| `zypp/` (`main.py`, `tracker.py`, `test.py`) | Python/GObject-introspection (`gi.repository.Zypp`) demo/test client for libzypp-glib. |

---

## 9. Reproducing bugs from bug reports

### 9.1 What to ask for / where things are

| Artifact | Location / how produced | Use |
|---|---|---|
| zypper log | `/var/log/zypper.log` (`ZYPPER_LOG` in `zypper/src/main.h`; overridable with `ZYPP_LOGFILE`) | Full libzypp log of the zypper run (MIL/WAR/ERR, log groups). Rotated by logrotate (`zypper.lr`, see spec). |
| YaST log | `/var/log/YaST2/y2log*` (the "y2logs" tarball from `save_y2logs`) | libzypp log when run by YaST. Also contains `/var/log/YaST2/solverTestcase` if YaST dumped one (`Testcase` default `dumpPath` in `zypp/zypp/solver/detail/Testcase.cc`). Exact tarball contents are produced by YaST tooling, not libzypp — not verified here. |
| supportconfig | SUSE support tool; typically includes the above logs, repo config, rpm list. Contents not defined in this repo — check the actual tarball. | |
| History | `/var/log/zypp/history` (`ZConfig::historyLogFile()`), parsed by `HistoryLogReader` | Exact sequence of installs/removals, repo add/remove, commands. |
| Solver testcase from zypper | `zypper --debug-solver <cmd>` → writes `/var/log/zypper.solverTestCase` (`zypper/src/solve-commit.cc`) and exits without committing (works even if solving failed) | The primary reproduction artifact for solver bugs. |
| Upgrade testcases | `/var/log/updateTestcase-YYYY-MM-DD-HH-MM-SS` written on dist upgrade, number kept = `solver.upgradeTestcasesToKeep` (`writeUpgradeTestcase()` in `zypp/zypp/target/TargetImpl.cc`) | Often already present on the user's machine after a failed/odd `dup`. |

`Testcase::createTestcase()` writes: libsolv testcase `testcase.t` + `solver.result` (via `::testcase_write`, repo files
`*.repo` in testtags format, with mangled repo names), libzypp control file `zypp-control.yaml` (channels/repos,
arch, locales, autoinst, modalias, multiversion, resolver flags, trials/jobs), and — when `runSolver` — a `y2log` of the
solver run with excessive logging. `LoadTestcase` recognizes `zypp-control.yaml` and the legacy helix
`solver-test.xml` (`zypp/zypp/misc/LoadTestcase.cc`).

### 9.2 Replaying a solver testcase

```
 user: zypper --debug-solver dup   ──►  /var/log/zypper.solverTestCase/  ──► attach tarball to bug
                                               │
            ┌──────────────────────────────────┼─────────────────────────────────────┐
            ▼                                  ▼                                     ▼
 deptestomatic <dir>              testsolv testcase.t (libsolv tool)      zypp-NameReqPrv --root <dir> pkg
 (zypp-solver-testcase-runner;    pure libsolv level, bypasses             inspect deps of what's in the pool
  full libzypp Resolver replay)   libzypp policies
```

* **deptestomatic** (`zypp-stack/zypp-solver-testcase-runner/src/deptestomatic.cc`, package `libzypp-testsuite-tools`,
  installed to `<prefix>/lib/zypp/testsuite/bin/deptestomatic`): `deptestomatic [-h|-v|-vv] [testdir]` (default `.`).
  Loads the testcase via `LoadTestcase`, applies the setup, executes each trial through the libzypp resolver (uses
  `ZYPP_USE_RESOLVER_INTERNALS`, `SolverQueueItem*`) and prints results prefixed `>!> `. `-v` logs to stdout,
  `-vv` uses a "diffable" line formatter (`<level> [group] file(func):line msg`, no timestamps) so two runs (old vs.
  new libzypp) can be diffed. A `cycle:`/`CRITICAL` argument prints cycle info. It resets vendor settings
  (`VendorAttr::noTargetInstance() = VendorAttr()`), i.e. **local `/etc/zypp/vendors.d` of your machine is not used**,
  but setup-defined vendor info is.
* **`testsolv`** (libsolv) on `testcase.t`: tells you whether the problem is in libsolv or in libzypp's job/policy layer.
* Edit `zypp-control.yaml` trials / `testcase.t` jobs to bisect, then turn the trimmed directory into a regression test
  under `zypp/tests/data/TC<Name>` (pattern in section 2.3).
* Caution: replay depends on the libsolv version (stack builds a patched static libsolv).

More on solver internals: [Solver](04-solver.md).

### 9.3 Non-solver bugs

* Refresh/download: reproduce with `zypper -vvv ref` + `ZYPP_LOGFILE`, `ZYPP_MEDIA_CURL_DEBUG` etc.
  (see [Media & downloads](07-media-and-downloads.md) and [Configuration & environment](09-configuration-and-environment.md));
  a local nginx docroot via `WebServer` handlers is the best way to script HTTP edge cases (redirects, 404s, auth,
  zchunk, metalink) as a regression test.
* Repo parsing: `ToolScanRepos URL` or `TestSetup::loadRepo(dir)` in a test.
* GPG/keyring: `zypp-pubkey`, `KeyRing_test` data (`zypp/tests/zypp/data/KeyRing`); see [Security](06-security-gpg-keyring.md).
* Commit/rpm: needs a real (chroot) root; `zypper --root /tmp/x` against the build-dir `zypp-rpm` in a devel build.
  Remember `ZYPP_TESTSUITE_FAKE_ARCH` makes commit throw.

---

## 10. The zypp-stack meta project

`<zypp-stack>` (github `openSUSE/zypp-stack`) builds the whole stack with one CMake project.

```
zypp-stack/
├── CMakeLists.txt            orchestration (below)
├── libsolv/                  submodule; must `git apply ../libsolv.patch` before first build
├── libsolv.patch
├── libzypp/                  submodule (this repo)
├── zypper/                   submodule; has its own tests/ (Boost.Test: PackageArgs, SolverRequester, ZyppFlags, Locales, Search_104, test-cases/)
├── zypp-solver-testcase-runner/   submodule: deptestomatic (libzypp-testsuite-tools)
├── mcp-server-zypp/          submodule: MCP server (built with ZYPP_MCP_ENABLE_HTTP)
├── obs-service-cpio-repack/  submodule (OBS helper; not built)
├── zypp_graph/               Qt6/QML dependency graph viewer (links zypp-allsym + libsolv); NOT add_subdirectory'd by the root CMakeLists
├── yast-pkg-bindings/        checkout of YaST Pkg bindings (not built by the stack)
├── scripts/                  cxx_ns_pull.py, replace-compat-header.fish, bugzilla triage scripts
├── .devcontainer/            Dockerfile + devcontainer.json for a build environment
└── build/                    build dir
```

Root `CMakeLists.txt`:

1. Prefers version-matched `ld.lld` with Clang.
2. Sets `ZYPP_STACK_BUILD=TRUE` (subprojects switch to in-tree targets instead of `find_package`).
3. Forces SUSE libsolv options (`SUSE`, `ENABLE_STATIC`, `DISABLE_SHARED`, complex deps, zstd/zchunk, helixrepo, ...) and
   `add_subdirectory(libsolv)`; symlinks libsolv headers into `build/include/solv/` (+ generated `solvversion.h`).
4. Forces libzypp options: `ENABLE_DEVEL_BUILD`, `ENABLE_BUILD_TESTS`, `ENABLE_BUILD_TRANS`, `BUILD_LIBZYPPNG`,
   `INSTALL_NG_BINARIES`, `ENABLE_ZCHUNK_COMPRESSION`; disables clang-tidy, UBSAN, `ENABLE_DEVTOOL_REFRESHNG`,
   and `DISABLE_MEDIABACKEND_TESTS` (i.e. **media tests are on → nginx/vsftpd/squid/fcgi required to configure**).
   These use `CACHE ... FORCE`: passing `-D...` on the command line for them has no effect.
5. `add_subdirectory(libzypp)`, symlinks generated `APIConfig.h` into `build/include/zypp-core/`.
6. `add_subdirectory(zypper)`, `add_subdirectory(zypp-solver-testcase-runner)`, `add_subdirectory(mcp-server-zypp)`.

`ENABLE_DEVEL_BUILD` (`libzypp/zypp/CMakeLists.txt`, warns "do not do this in production") bakes build-dir paths into the
library: `ZYPP_RPM_BINARY=<build>/tools/zypp-rpm/zypp-rpm`, `ZYPP_WORKER_PATH=<build>/tools/workers`, and in a stack
build `ZYPP_REPO2SOLV_PATH` / `ZYPP_RPMDB2SOLV_PATH` pointing to the stack-built libsolv tools. So an uninstalled zypper
from `build/zypper/src/zypper` uses the uninstalled libzypp **and** its helpers — no interference from the system
libzypp. The flip side: these binaries are not suitable for `make install`.

zypper / testcase-runner in stack mode set `ZYPP_LIBRARY zypp`, `ZYPP_TUI_LIBRARY zypp-tui` (CMake targets) and add
`ZYPP_STACK_INCLUDE_DIRS`; target names get prefixes (`zypper_`, `zypp_solver_testcaserunner`) to avoid clashes.
Outside the stack, zypper can be pointed at an alternative install with `-DZYPP_PREFIX=...`.

Typical workflow:

```bash
cd zypp-stack/libsolv && git apply ../libsolv.patch && cd ..
mkdir -p build && cd build && cmake .. && make -j$(nproc)
ctest -R '^Resolver_test$' --output-on-failure          # libzypp tests are registered in the stack ctest
sudo ./zypper/src/zypper --root /tmp/fakeroot ar ...    # uninstalled zypper against uninstalled libzypp
./zypp-solver-testcase-runner/deptestomatic -vv /path/to/zypper.solverTestCase
```

The local checkout uses one build dir per IDE preset (`build/<preset>/`, e.g. `build/Desktop_GCC-Debug`); binaries end up
in `build/<preset>/zypper/src/zypper` and `build/<preset>/zypp-solver-testcase-runner/deptestomatic` (verified), so the
paths above are relative to such a preset dir (section 15.2).

---

## 11. Full test inventory

Generated by walking every `CMakeLists.txt` under `zypp/tests` and `zyppng/tests` and grepping each `*_test.cc` for
`BOOST_AUTO_TEST_CASE` / `BOOST_DATA_TEST_CASE` / `ZYPP_CORO_TEST_CASE` names and for `WebServer`/`FtpServer`/`ProxyServer`
usage. "Web" = needs the media backend (`IF( NOT DISABLE_MEDIABACKEND_TESTS )` in CMake; nginx and possibly
vsftpd/squid). "TS" = uses `TestSetup`. Data paths are relative to `zypp/tests/` unless noted otherwise.

### 11.1 `zypp/tests/zypp/` (core value types, high-level classes)

| Test | Covers (test case names / data) | Web | TS |
|---|---|---|---|
| Arch | `Arch` compat/ordering (`arch_test`) | | |
| Capabilities | `Capability` parsing, `Capability::guessPackageSpec` | | |
| CheckSum | `CheckSum` construction/type detection | | |
| ContentType | `ContentType` default/value/compare | | |
| CpeId | `CpeId` values (ANY/NA/wildcards), string conversion, matching | | |
| Date | `Date` formatting/parsing | | |
| Deltarpm | `DeltaRpm`/`DeltaCandidates` from `zypp/data/Delta` | | |
| Digest | `Digest` algorithms | | |
| DrunkenBishop | `base::DrunkenBishop` key fingerprint art | | |
| Dup | DUP orphan handling (`orphaned`), testcase `data/TCdup` | | ✓ |
| Edition | `Edition` compare/match | | |
| ExtendedPool | Pool id reuse across reloads, `data/PoolReuseIds/SeqA,SeqB` | | ✓ |
| FileChecker | `SignatureFileChecker`, `ChecksumFileChecker`, `zypp/data/FileChecker` | | |
| Flags | `base::Flags` operators | | |
| GZStream | gzip `ifgzstream/ofgzstream` read/write/seek | | |
| InstanceId | `InstanceId` ⇄ `PoolItem` conversion | | |
| KeyRing | import/delete/trust, `verifyFileSignatureWorkflow` on `zypp/data/KeyRing/repomd.xml{,.asc,.corrupted}` | | |
| Locale | `Locale`/`LanguageCode`/`CountryCode`, fallback chain | | |
| Locks | `Locks` load/save/redundancy, `zypp/data/Locks` | | ✓ |
| MirroredOrigin | `MirroredOrigin(Set)` construction, auth-change mirror cleanup, grouping, ordering | | |
| PathInfo | `PathInfo`, checksums, `expandlink`, `assert_dir/file`, `exchange` | | |
| Pathname | `Pathname` normalisation, `stripprefix`, `assertprefix` | | |
| PluginFrame | `PluginFrame`, `PluginScript` send/receive, `PluginExecutor` | | |
| PoolQuery | `PoolQuery` attribute/match-mode matrix (`pool_query_000..101`) | | ✓ |
| PoolQueryCC | `PoolQuery` on helix target `zypp/data/PoolQueryCC/rxnames.xml` | | ✓ |
| ProgressData | `ProgressData` + `ProgressReportAdaptor` | | |
| Proxy (needs libproxy) | `ProxyInfo` API (`proxy_api`) — no server | | |
| PtrTypes, RWPtr | `intrusive_ptr`/`RW_pointer` semantics | | |
| PublicKey | `PublicKey` parsing incl. multi-key files, `KeyManagerCtx` volatile context | | |
| PurgeKernels | `PurgeKernels` keep-spec evaluation, one testcase per sample under `zypp/data/PurgeKernels/*` (data-driven) | | ✓ |
| RepoInfo | `RepoInfo` mirrorlist/metalink handling against `data/Mirrorlist/remote-site` | Web | |
| RepoManager | add/refresh/build cache, plugin services, service file links, "don't overwrite external settings" (`zypp/data/RepoManager`, `repo/susetags/data/addon_in_subdir`) | | ✓ |
| RepoStatus | `RepoStatus` combining/compare | | |
| ResKind, ResStatus | `ResKind`; `ResStatus` transitions + save/restore state | | |
| Resolver | locale-namespace recommends with/without `onlyRequires`/`ignoreAlreadyRecommended`, `data/TCNamespaceRecommends` | | ✓ |
| RpmPkgSigCheck | `RpmDb::checkPackageSignature` for (un)signed/broken/broken-header rpms with and without key (`zypp/data/RpmPkgSigCheck`) | | |
| Selectable | `ui::Selectable` candidate (incl. noarch), `status` verify, `pickStatus` cycle, `data/TCSelectable` | | ✓ |
| SetRelationMixin, SetTracker | `SetCompare`/`SetRelation`; `base::SetTracker` | | |
| StrMatcher | `StrMatcher`/`Match` modes (STRING, STRINGSTART/END, GLOB, REGEX) | | |
| StringV | `strv::split` variants, trim/prefix | | |
| Target | `Target` product file reading (`zypp/data/Target/product.prod` copied into a TmpDir root) | | |
| Url | `Url` parsing incl. IPv6, plugin scheme, query `=` (bsc#1234304), FTP absolute path double-slash (bsc#1238315) | | |
| UserData | `callback::UserData` | | |
| Vendor | `VendorAttr` equivalence from `zypp/data/Vendor/vendors.d` | | |
| ZChunk (needs zchunk) | zchunk read/write | | |
| Fetcher | `Fetcher` enqueue dir/file, indexes (`SHA1SUMS`/content), digested, untrusted keys (`zypp/data/Fetcher/remote-site`) | Web | |
| MediaSetAccess | URL rewrite (`media.N`), provide file/dir/dirtree, verified sets, HTTP and FTP via template `msa_remote_tests_impl<WebServer/FtpServer>` | Web (port **10002**, also vsftpd) | |
| `base/` Glob, Sysconfig, String, ExternalProgram | glob, sysconfig read/write, `str::` helpers (bnc#909772 split escaped), `ExternalProgram` env/redirect/signal/timeout/close-fds | | |

### 11.2 `zypp/tests/media/`, `parser/`, `repo/`, `sat/`

| Dir | Test | Covers | Web | TS |
|---|---|---|---|---|
| media | CredentialFileReader / CredentialManager | credential file parsing; lookup by URL, save, service base URL | | |
| media | MediaProducts | compile-only (`compile`) | | |
| media | MetaLinkParser | metalink parsing incl. zsync hashes | | |
| media | MediaCurl | zck delta, file size mismatches (bsc#1245220 regression test), basic auth (+credfile), 404, mirrors via metalink, proxy (`provide_via_proxy_bsc1245220`, uses `ProxyServer` 10002), mirror fallback persistence; × SSL × `curl`/`curl2` | Web (+squid) | |
| parser | HistoryLogReader | `/var/log/zypp/history` parsing | | |
| parser | JsonParser | JSON incl. escaped UTF‑8/URLs, mirrorlist JSON | | |
| parser | ProductFileReader, RepoFileReader, RepoindexFileReader | `.prod`, `.repo`, repoindex.xml | | |
| parser/yum | RepomdFileReader | `repomd.xml` | | |
| parser/inifile | inidict, iniparser, EconfDict | INI parsing; `EconfDict` (libeconf-style stems, masked/sectioned drop-ins) | | |
| repo | DUdata | disk-usage data from susetags `repo/susetags/data/dudata` | | ✓ |
| repo | ExtendedMetadata | rpm-md extensions `repo/yum/data/extensions` | | ✓ |
| repo | PluginServices | plugin services | | |
| repo | RepoLicense | license tarball handling | | |
| repo | RepoSigcheck | unsigned / unknown-key / wrong-sig / signed rpm-md repos (`repo/RepoSigcheck/*`), uses `TSO_REPO_DEFAULT_GPG` | | ✓ |
| repo | RepoVariables | `$releasever`/`${var}` expansion, caching | | |
| repo | MirrorList | `RepoMirrorList` against `data/Mirrorlist/remote-site` | Web | |
| repo/yum | YUMDownloader | rpm-md download from local dir `repo/yum/data/10.2-updates-subset` (dir URL, no server) | | |
| repo/susetags | Downloader | susetags download, also gz | | |
| sat | Blacklisted | blacklisted/retracted/ptf solvables, testcase `sat/BlacklistedPool` | | ✓ |
| sat | IdString, Map, Queue | thin libsolv wrappers | | |
| sat | LookupAttr | `LookupAttr` iteration, sub-structures, repo attrs (bnc#435838) | | ✓ |
| sat | Pool | system repo, repo list | | ✓ |
| sat | Solvable | attributes, `asString`, `SplitIdent`, du data | | ✓ |
| sat | SolvableSpec | `SolvableSpec` parsing/eval | | ✓ |
| sat | SolvParsing | build solv via `RepoManagerOptions::makeTestSetup` in TmpDir, check attrs | | |
| sat | WhatObsoletes / WhatProvides | `sat::WhatObsoletes` (`data/TCWhatObsoletes`), `sat::WhatProvides` | | ✓ |

### 11.3 `zypp/tests/zyppng/` (async core in legacy build)

| Test | Covers | Web |
|---|---|---|
| EventLoop | create/run/quit, timers without loop, cleanup | |
| IOBuffer | `IOBuffer` reserve/discard/indexOf/readLine/move | |
| UnixSignalSource | signal catcher + cleanup | |
| io/UnixSocket, io/AsyncDataSource, io/Process | sockets (lots of data), pipes multi-channel, `zyppng::Process` (mirror of `ExternalProgram_test`) | |
| rpc/StompMessageStream | STOMP frame parse/serialize, invalid frames (worker protocol, see [Media](07-media-and-downloads.md)) | |
| media/NetworkRequestDispatcher | HTTP/FTP downloads, errors, delays, multipart range requests incl. overlap/missing data/no-range answer | Web (+vsftpd) |
| media/EvDownloader (**not built**, commented out) | old downloader incl. metalink `.meta` templates | — |

### 11.4 `zyppng/tests/` (module NG suite)

| Test | Covers | Web |
|---|---|---|
| async/Task | `Task<>` coroutines, destroy notification, exceptions, in pipelines | |
| async/Pipelines | sync/async/mixed pipelines, `transform` (`ZYPP_CORO_TEST_CASE`) | |
| config/Config | NG config defaults, typed/string set/get/reset, overrides vs file values, path keys (root-relative, escape rejection), parsing | |
| config/ConfigParsers | bool/tribool/long/unsigned token acceptance & clamping | |
| config/ConfigCompat | every key in `config/data/zypp.conf.ALL_KEYS` is recognised | |
| media/Provider | `zyppng::Provide` over HTTP (attach, 404, not-a-file, zck, auth incl. wrong password / no user response) and `tvm` multi-media; needs built workers (`ZYPPNG_WORKERS_DIR`) | Web |
| sat/PoolNg | NG `sat::Pool`: repo creation, prepare, whatprovides index rebuild on dirty, component ordering | |
| sat/PoolComponentSet | pool component registry: idempotency, priority/stage order, event fan-out | |

### 11.5 Coverage gaps (no dedicated test found)

Verified by absence of a matching `*_test.cc` (grep), so "untested" here means "no unit test", it may still be
exercised indirectly:

| Subsystem | Code | Note |
|---|---|---|
| Commit / rpm transaction | `zypp/zypp/target/TargetImpl.cc` commit, `zypp/tools/zypp-rpm`, `zypp-logic/shared/commit` | Only `RpmPkgSigCheck` touches `RpmDb`. No test installs a package. Commit regressions are found by OBS/openQA, not ctest. |
| `RpmDb` / `librpmDb` / `RpmHeader`, `RpmPostTransCollector`, `CommitPackageCache*` | `zypp/zypp/target/rpm/`, `target/` | none |
| `HardLocksFile`, `RequestedLocalesFile`, `SolvIdentFile` (autoinstalled) | `zypp/zypp/target/` | none |
| Media handlers other than curl | `MediaISO`, `MediaNFS`, `MediaCIFS`, `MediaCD`, `MediaDISK`, `MediaPlugin`, `UrlResolverPlugin` | `MediaDIR` only indirectly (local dir URLs in repo tests). NG workers `zypp-media-{iso,nfs,smb,disc,disk,copy,chksum}`: only http and tvm are tested via `Provider_test`. |
| Package download / delta application | `repo/PackageProvider`, `repo/DeltaCandidates` apply, `applydeltarpm` | `Deltarpm_test` only parses. |
| Solver problem/solution API | `solver/detail/SolverQueueItem*`, `ProblemSolution*`, `SATResolver` problem texts | `Resolver_test` covers only recommends; solver regressions rely on deptestomatic outside ctest. |
| Patch / Pattern / Product resolvables, `ServiceInfo` | `zypp/zypp/Patch.cc`, `Pattern.cc`, `Product.cc` | Partly via `Target_test` (product files). |
| `ZConfig` (legacy) | `zypp/zypp/ZConfig.cc` | Only `ConfigCompat` (NG) checks key recognition; no test of legacy semantics/defaults. |
| History log writing | `HistoryLog.cc` | Only the reader is tested. |
| `zypp-tui` | `zypp/zypp-tui` | no tests at all |
| libzypp-glib | `zyppng/api/zypp-glib` | no tests (only the Python demo `zypp/tools/zypp/test.py`) |
| NG workers' protocol end-to-end besides http/tvm, NG repo/refresh | `zyppng/tools/*`, `zypp-logic/zypp/ng/repo*` | no test; the `refresh-ng` devtool is bit-rotted |

---

## 12. Writing tests: step by step with skeletons

Signatures below are verified against `zypp-logic/tests/lib/{TestSetup.h,TestTools.h,WebServer.h,FtpServer.h,ProxyServer.h}`.
`TestSetup.h` itself includes `<boost/test/unit_test.hpp>` and does `using namespace zypp;` at file scope, so files that
include it see `zypp::` names unqualified.

### 12.1 (a) Plain unit test (legacy)

1. Create `zypp/tests/zypp/Foo_test.cc` (or the area dir that matches the code under test).
2. Add `Foo` to the `ADD_TESTS(...)` list in `zypp/tests/zypp/CMakeLists.txt`. The macro adds `BOOST_TEST_MAIN`, so
   **don't** define `BOOST_TEST_MODULE`/`main` yourself.
3. Reconfigure (the test list is not globbed), build the target `Foo_test`, run `ctest -R '^Foo_test$'`.

```cpp
#include <boost/test/unit_test.hpp>
#include <zypp-core/Pathname.h>
#include <zypp-core/base/String.h>

using zypp::Pathname;

BOOST_AUTO_TEST_CASE( foo_basic )
{
  BOOST_CHECK_EQUAL( Pathname("/a/b/../c").asString(), "/a/c" );
  BOOST_REQUIRE( zypp::str::startsWith( "libzypp", "lib" ) );
}

// data-driven, pattern used by MediaCurl_test / PurgeKernels_test
#include <boost/test/data/test_case.hpp>
namespace bdata = boost::unit_test::data;
BOOST_DATA_TEST_CASE( foo_matrix, bdata::make( { "a", "b" } ) * bdata::make( { 1, 2 } ), name, num )
{ BOOST_CHECK( !std::string(name).empty() && num > 0 ); }
```

Data files: put them under `zypp/tests/zypp/data/Foo/` and refer to them as `TESTS_SRC_DIR "/zypp/data/Foo/..."`. For
scratch space use `zypp::filesystem::TmpDir` (`zypp-core/fs/TmpPath.h`; created below `$ZYPPTMPDIR` or `/var/tmp`,
removed in the dtor).

### 12.2 (b) Test with `TestSetup` and a fake repo

```cpp
#include <tests/lib/TestSetup.h>           // pulls in boost unit_test + using namespace zypp
#include <zypp/ResPool.h>
#include <zypp/PoolQuery.h>

#define DATADIR ( Pathname(TESTS_SRC_DIR) / "zypp/data/Foo" )

BOOST_AUTO_TEST_CASE( foo_repo )
{
  TestSetup test( Arch_x86_64 );                       // TmpDir root, wiped in dtor
  test.loadRepo( DATADIR / "repo", "foo" );            // rpm-md or susetags *directory* -> addRepository+buildCache+loadFromCache
  // test.loadTargetRepo( DATADIR / "installed" );     // fake @System from a repo dir
  // test.loadTargetHelix( DATADIR / "system.xml" );   // fake @System from helix xml
  // test.loadRepo( DATADIR / "foo.solv" );            // a plain .solv file is added directly (no RepoManager)

  PoolQuery q; q.addAttribute( sat::SolvAttr::name, "bash" ); q.setMatchExact();
  BOOST_CHECK_EQUAL( q.size(), 1 );
  BOOST_CHECK( test.resolver().resolvePool() );
}
```

* Signature tests: `TestSetup( Arch_x86_64, TSO_REPO_DEFAULT_GPG )` keeps the repo's default gpgcheck; failures surface as
  `repo::RepoException` from `loadRepo` (see `repo/RepoSigcheck_test.cc`). Add a KeyRing receiver (`zypp/tests/zypp/KeyRingTestReceiver.h`,
  reachable from `repo/` and `sat/` via the extra `INCLUDE_DIRECTORIES( ${LIBZYPP_SOURCE_DIR}/tests/zypp )`) to answer trust prompts.
* Persistent root for inspection: `TestSetup( "/tmp/zypp-test/foo-root", Arch_x86_64, TSO_CLEANROOT )` — the root is
  **kept** after the test, so `zypp-NameReqPrv --root /tmp/zypp-test/foo-root ...` can inspect it afterwards (it is
  detected as a TestSetup dir: `repos.d` + `raw`).
* Several test cases sharing one pool: use the `TestSetup::initLater` + `BOOST_GLOBAL_FIXTURE` pattern (section 3.1);
  constructing a second `TestSetup` erases all repos of the first (`reposEraseAll()` in `Impl` ctor).
* Creating the repo data: `createrepo_c <dir>` for rpm-md (unverified whether a script exists; the checked-in repos were
  generated once and committed — there are **no** generator scripts under `zypp/tests`). Keep repos tiny; the real-world
  `data/openSUSE-11.1` is large and slows tests.

### 12.3 (c) Media test with `WebServer` (+ nginx config)

```cpp
#include <tests/lib/WebServer.h>
#include <tests/lib/TestTools.h>
#include <zypp/media/MediaManager.h>
#include <zypp-core/OnMediaLocation>
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE( foo_http )
{
  const zypp::Pathname docroot = zypp::Pathname(TESTS_SHARED_DIR) / "data/http";   // must exist, else start() throws
  WebServer web( docroot.c_str(), 10001, /*useSSL*/ true );
  BOOST_REQUIRE( web.start() );                                   // false if nginx did not come up within 10 s

  // dynamic responses: requests to /handler/<name> go to the FastCGI handler <name>
  web.addRequestHandler( "redir", WebServer::makeResponse( "301 Moved Permanently",
                         { "Location: /test.txt\r\n" }, "" ) );   // each header MUST end in \r\n (joined without trailing CRLF)
  web.addRequestHandler( "secret", WebServer::makeBasicAuthHandler( "Basic dGVzdDp0ZXN0",
                         WebServer::makeResponse( "200 OK", "hello" ) ) );
  web.addRequestHandler( "raw", []( WebServer::Request &req ) {
    // req.params: CGI env (REQUEST_URI, HTTP_AUTHORIZATION, QUERY_STRING, ...)
    req.rout << "Status: 404 Not Found\r\n\r\n";
  } );

  zypp::Url url = web.url();                       // https://localhost:10001
  url.setQueryParam( "ssl_capath", web.caPath().asString() );     // or web.transferSettings() for NG/ MediaCurl settings
  url.setQueryParam( "mediahandler", "curl2" );    // optional: force a backend

  zypp::media::MediaManager mm;
  zypp::media::MediaAccessId id = mm.open( url );
  mm.attach( id );
  zypp_defer { mm.releaseAll(); mm.close( id ); };
  BOOST_CHECK_NO_THROW( mm.provideFile( id, zypp::OnMediaLocation("/handler/redir") ) );
}
```

CMake: put the test name into the `IF( NOT DISABLE_MEDIABACKEND_TESTS )` block of the area's `CMakeLists.txt`.

Other servers: `FtpServer( root, port=10001, useSSL=false )` (vsftpd, same `start/stop/url/transferSettings`),
`ProxyServer( port=10002 )` with `setAuthEnabled(bool)` (squid + `basic_ncsa_auth` against `data/proxyconf/htaccess`).
Use distinct ports if you run two servers in one test (`MediaSetAccess_test` uses 10002 for Web/Ftp).

nginx config: you normally **don't** touch it. `WebServer` symlinks `zypp-logic/tests/data/nginxconf/nginx.conf`,
`mime.types` and `ssl.conf`/`nossl.conf` into its TmpDir and generates `port.conf`, `srvroot.conf`, `fcgisock.conf`,
`user.conf` (section 3.3). The static config sets `max_ranges 128` (multipart range tests depend on it), maps
`/handler` to FastCGI and passes `Status`/`Status-Code` headers through. If you need a new nginx directive for all tests,
edit `nginx.conf` there (shared by legacy and NG suites!).

Metalink-style tests: `data/http/*.meta` are metalink templates with a `%1%` placeholder for `<url>` entries that the
test fills with `zypp::str::Format` (only used by the unbuilt `EvDownloader_test`; `MediaCurl_test` builds metalinks
inline). Typical idiom: a handler returning `Location: /test.txt.meta` plus the file name.

### 12.4 (d) Solver testcase based test

1. Get a testcase (`zypper --debug-solver ...`, or write one by hand: `zypp-control.yaml` + one testtags `*.repo` per
   channel, `@System.repo` = installed).
2. Trim it: drop unrelated packages from the `.repo` files (format `=Pkg: name ver rel arch`, `+Prv:`/`-Prv:` blocks,
   `=Req:`, `=Con:`, `=Obs:`, `=Rec:`, `=Sup:`, `=Vnd:`); empty `trials: []` if the test drives the resolver itself. Verify
   with `deptestomatic -v` that the issue still reproduces.
3. Store as `zypp/tests/data/TC<Name>/` (shared) or `zypp/tests/<area>/data/<Name>/`.
4. Test:

```cpp
#include <tests/lib/TestSetup.h>
#include <zypp/ResPool.h>
#include <zypp/ResFilters.h>

static TestSetup test( TestSetup::initLater );
struct TestInit {
  TestInit()  { test = TestSetup( Arch_x86_64 ); test.loadTestcaseRepos( TESTS_SRC_DIR "/data/TCFoo" ); }
  ~TestInit() { test.reset(); }
};
BOOST_GLOBAL_FIXTURE( TestInit );

BOOST_AUTO_TEST_CASE( foo_upgrade )
{
  test.resolver().doUpgrade();     // or set ResStatus transact on PoolItems, then resolvePool()
  for ( const PoolItem & pi : test.pool().byName( "package" ) )
    if ( pi.status().transacts() ) BOOST_TEST_MESSAGE( pi );
  // compare against expected set, see Resolver_test.cc resolve()/getPi() helpers
}
```

`loadTestcaseRepos( dir, &trials )` optionally returns the parsed trials (`misc::testcase::LoadTestcase::TestcaseTrials *`)
if you want to replay the jobs encoded in the yaml instead of hard-coding them. Remember the process-wide
`ZYPP_TESTSUITE_FAKE_ARCH` side effect (section 3.1).

### 12.5 (e) NG test (`zyppng/tests`)

There is **no** `zyppng_add_test`/`zyppng_add_module_test` helper (the latter is only referenced in comments of
`zyppng/lib/zypp/CMakeLists.txt`); use the per-suite `ADD_TESTS()` macro of `zyppng/tests/CMakeLists.txt`, which adds
`CXX_SCAN_FOR_MODULES ON`, `CXX_STANDARD 20` and links `zyppng-objlib-test` + `zyppng_test_utils`.

```cmake
# zyppng/tests/sat/CMakeLists.txt
ADD_TESTS(
        PoolNg
        PoolComponentSet
        Foo            # -> Foo_test.cc
)
# extra objects (cf. media/: target_link_libraries( Provider_test PUBLIC tvm-protocol-obj ))
```

```cpp
// zyppng/tests/sat/Foo_test.cc
#include <boost/test/unit_test.hpp>   // keep #includes before `import zyppng;` (pattern of all existing NG tests)
#include <tests/lib/TestTools.h>       // ZYPP_CORO_TEST_CASE, ZYPP_REQUIRE_THROW

import zyppng;

using namespace zyppng;

struct PoolFixture {
  PoolFixture()  { pool.clear(); }
  ~PoolFixture() { pool.clear(); }
  sat::Pool pool;
};

BOOST_FIXTURE_TEST_SUITE( FooTest, PoolFixture )
BOOST_AUTO_TEST_CASE( insert )
{
  auto repo = pool.reposInsert( "test-repo" );
  BOOST_REQUIRE( repo );
  BOOST_CHECK_EQUAL( repo.alias(), "test-repo" );
}
BOOST_AUTO_TEST_SUITE_END()

// async code: body is a coroutine; an EventLoop runs until the Task completes, exceptions are rethrown
ZYPP_CORO_TEST_CASE( foo_async )
{
  auto timer = zyppng::Timer::create();
  timer->setSingleShot( true ); timer->start( 0 );
  co_await timer->sigExpired();
  BOOST_CHECK( true );
}
```

* `ZYPP_CORO_TEST_CASE` needs `zyppng::Task`, `zyppng::EventLoop` declared; the existing tests include
  `<zypp-core/ng/async/task.h>` / `<zypp-core/ng/base/eventloop.h>` (`Task_test.cc`) or get them via headers like
  `<zypp-core/ng/base/EventLoop>` (`Provider_test.cc`). `TestTools.h` only defines the macro under
  `#ifdef __cpp_impl_coroutine`, and does not include the Task/EventLoop headers itself.
* Module-internal symbol needed? Mark the declaration `ZYPPNG_TEST_EXPORT` (`zyppng/lib/zypp/ng/zypp_test_export.h`).
* Data: NG `TESTS_SRC_DIR` = `zyppng/tests` (root; `config/` redefines it to its own dir). Workers for Provider tests:
  `ZYPPNG_WORKERS_DIR` — build the worker targets (`zypp-media-http`, `zypp-media-tvm`, ...) first, a plain
  `make Provider_test` does not necessarily build them (unverified whether there is a dependency).

---

## 13. Test data reference

### 13.1 `zypp-logic/tests/data` (shared by both suites, via `TESTS_SHARED_DIR`)

| Path | Content | Used by |
|---|---|---|
| `nginxconf/` | `nginx.conf` (template, includes generated `*.conf`), `mime.types`, `ssl.conf`, `nossl.conf` | `WebServer` |
| `vsftpconf/vsftpd.conf` | vsftpd template (SUSE/Fedora default differences fixed, changelog "fixes #522") | `FtpServer` |
| `proxyconf/` | `squid.conf`, `htaccess` (ncsa auth users) | `ProxyServer` |
| `lighttpdconf/` | legacy lighttpd config + `conf.d/maybessl.sh` | only `#ifdef HTTP_USE_LIGHTTPD` (dead) |
| `webconf/ssl/` | `ca/ca.{crt,key}`, `server.{crt,csr,key,pem}`, `openssl.cnf`, `certstore/` (`ca.pem` + hash symlink `7dd4d2a5.0`) | SSL for nginx; `WebServer::caPath()` = `certstore` |
| `http/` | `test.txt` (2 MiB), `*.meta` metalink templates, `primary.xml.zck` + `primary-deltatemplate.xml.zck` (zchunk delta test), `credfile`, `media.1/` | `MediaCurl_test`, `Provider_test`, `EvDownloader_test` |
| `dummywebroot/` | only a `Readme` | Docroot for tests that serve everything from handlers (`NetworkRequestDispatcher_test`) |

Certificates: server cert `CN=localhost`, CA `CN=127.0.0.1 CA`, both valid until **2198** (checked with
`openssl x509 -enddate`), so expiry is not a concern. The certstore uses OpenSSL hash naming; if you regenerate the CA,
recreate the symlink (`openssl rehash certstore/` or `c_rehash`) — the hash file name changes with the subject.
Regeneration recipe is not documented in the tree; `openssl.cnf` is there for `openssl req/x509 -config openssl.cnf`.

### 13.2 `zypp/tests` data

```
zypp/tests/
├── data/                       shared: solver testcases TC*, real old repos, PoolReuseIds, Mirrorlist/remote-site
├── zypp/data/<Test>/           per test: Delta FileChecker Fetcher KeyRing Locks mediasetaccess PoolQuery PoolQueryCC
│                               PublicKey PurgeKernels(one testcase dir per sample) RepoManager RpmPkgSigCheck Target Vendor
├── zypp/base/data/ExternalProgram/echo.sh
├── media/data   parser/data   parser/inifile/data   parser/yum/data
├── repo/RepoSigcheck/{signed,unsigned,unknownkey,wrongsig}_repo/repodata
├── repo/susetags/data/{addon_in_subdir,dudata,shared_attributes,stable-x86-subset{,-gz}}
├── repo/yum/data/{10.2-updates-subset,extensions,ZCHUNK}
├── sat/BlacklistedPool/        solver testcase
└── zyppng/data/process/echo.sh (Process_test), zyppng/data/downloader (EvDownloader, unbuilt)
```

* **No `.solv` files are checked in.** Solv caches are always generated at test time by `RepoManager::buildCache`
  (→ `repo2solv` from the libsolv tools; in a stack devel build the path is baked in via `ZYPP_REPO2SOLV_PATH`), into
  the TestSetup root `solv/<alias>/solv`. Testcase repos (`*.repo` testtags) are read by libsolv's testcase reader
  directly.
* Adding a repo: drop rpm-md (`repodata/repomd.xml` + primary etc.) or susetags (`content` + `suse/setup/descr`)
  metadata into a directory and call `TestSetup::loadRepo(dir)`. For dependency-only scenarios prefer a testcase
  (`zypp-control.yaml` + testtags) — far smaller and readable in diffs.
* Testcase layout (example `data/TCdup`): `zypp-control.yaml` (`version`, `setup.channels[]` with `alias`, `priority`,
  `file`; `arch`, `locales`, `autoinst`, `modalias`, `multiversion`, `resolverFlags`; `trials`), `@System.repo`,
  `<alias>.repo`. Legacy helix testcases (`solver-test.xml`) still load (`LoadTestcase::testcaseTypeAt`).

### 13.3 GPG / signature material

| File | What | Notes |
|---|---|---|
| `zypp/data/KeyRing/{public,private}.asc` | test key pair; `readme.txt`: "passphrase for the key pair is zypp-devel" | `repomd.xml.asc` is signed with it; `repomd.xml.corrupted` = content changed after signing; `installkey.gpg` used for import tests |
| `zypp/data/PublicKey/{susekey,multikey,multikey2}.asc` | real SUSE key, files with several keys | `PublicKey_test` |
| `zypp/data/RpmPkgSigCheck/*.rpm`, `signed.key` | (un)signed, payload-broken and header-broken rpms + the signing key | `RpmPkgSigCheck_test` |
| `repo/RepoSigcheck/*_repo/repodata` | repos with no sig, sig by unknown key, wrong sig, valid sig | `RepoSigcheck_test` |
| `data/openSUSE-11.1` etc. | contain the (old) distribution keys | loading/parsing only |

No regeneration scripts or instructions exist besides the passphrase note. Generic re-signing (unverified, not taken
from the repo): `gpg --homedir $(mktemp -d) --import private.asc` (passphrase `zypp-devel`), then
`gpg --homedir ... --armor --detach-sign repomd.xml`. If you replace a key, all fingerprints/IDs hard-coded in
`KeyRing_test.cc`/`PublicKey_test.cc` (and the keyring trust expectations) must be updated. SHA1-based or expired keys may
start failing with newer gpgme/rpm policies — watch for that when distro crypto policies change.

---

## 14. Debugging a failing test

```bash
cd build/zypp/tests/zypp                    # stack: build/<preset>/libzypp/zypp/tests/zypp
# list / pick test cases
./Fetcher_test --list_content
./Fetcher_test --catch_system_errors=no --run_test=fetcher_simple
./Fetcher_test --catch_system_errors=no --run_test='fetcher_enqueue*'       # wildcard
./MediaCurl_test --catch_system_errors=no --run_test='base_provide_zck/_0'  # one data sample of a BOOST_DATA_TEST_CASE
# Boost.Test verbosity / reports
./X_test --catch_system_errors=no --log_level=all --report_level=detailed
./X_test --catch_system_errors=no --log_level=test_suite --color_output=no
BOOST_TEST_LOG_LEVEL=all BOOST_TEST_CATCH_SYSTEM_ERRORS=no ctest -R '^X_test$' -V   # env equivalents work through ctest
# libzypp log
ZYPP_LOGFILE=/tmp/zypp-test/x.log ZYPP_FULLLOG=1 ./X_test --catch_system_errors=no
# media debugging
ZYPP_MEDIA_CURL_DEBUG=1 ZYPP_LOGFILE=- ./MediaCurl_test --catch_system_errors=no --run_test=base_provide_auth
# keep temp files
ZYPPTMPDIR=/tmp/zypp-test/zt ./X_test ...     # all TmpDir/TmpFile (incl. WebServer prefix) go there
```

* **gdb**: `gdb --args ./X_test --catch_system_errors=no --run_test=case`. Useful breakpoints: `zypp::Exception::Exception`
  (`catch throw` is noisy because libzypp uses exceptions for control flow; `break zypp::Exception::log` is narrower),
  `__ubsan_on_report` with UBSAN. Boost.Test options `--catch_system_errors=no` and `--detect_fp_exceptions=no` keep gdb in
  charge of signals; `--break_exec_path` is not available in older Boost (unverified).
* **Temp dirs vanish**: `TmpDir` deletes in its dtor, `WebServer`'s nginx prefix (with `log/error.log`, `access.log`,
  generated `*.conf`) is a `TmpDir` member of `WebServer::Impl`. There is no "keep" env switch; set `ZYPPTMPDIR`, put a
  breakpoint before the object goes out of scope (or `sleep()` temporarily) and inspect `$ZYPPTMPDIR/TmpDir.*`. For
  `TestSetup`, pass an explicit root path instead (kept after the run).
* **`WebServer::log()` returns an empty string** (stub in `WebServer.cc`) — don't rely on it in assertions; nginx
  stdout/stderr goes to the test's stderr via the worker thread.
* Test case order and singletons: run the failing case alone (`--run_test`) *and* the full binary; if only the full run
  fails, suspect state left in `ZConfig`, `sat::Pool`, `getZYpp()` target, `KeyRing`, `CredentialManager` or env vars.
* Log groups/format and the `TmpLineWriter` trick: section 3.1 and [Core infrastructure](02-core-infrastructure.md).
* Timeouts in media tests: nginx takes up to 10 s to come up (`checkLocalPort` loop); "Webserver exited too early" on
  stderr = nginx could not start (port busy, bad config, missing permissions on the docroot path components).

---

## 15. Bug reproduction workflow

### 15.1 Checklist to request from reporters

| Ask for | Command / path | Why |
|---|---|---|
| Exact command + full output | `zypper -vvv <cmd>` (or the YaST action) | Distinguish libzypp from zypper behaviour |
| zypper log | `/var/log/zypper.log` (rotated: `zypper.log-*.xz`) | Main libzypp log |
| History | `/var/log/zypp/history` | What changed and when |
| Solver testcase | `zypper --debug-solver <same cmd>` → tarball of `/var/log/zypper.solverTestCase/`; after `dup` also `/var/log/updateTestcase-*` | Replay with deptestomatic |
| Repo & service list | `zypper lr -d` (URIs, priorities, gpgcheck, autorefresh), `zypper ls -d` | Priorities/URLs drive solver and refresh |
| Repo files | `/etc/zypp/repos.d/*.repo`, `/etc/zypp/services.d/*` | Exact settings incl. `gpgkey`, `keeppackages`, `mirrorlist`/`metalink` |
| Config | `/etc/zypp/zypp.conf` + `/etc/zypp/zypp.conf.d/*` (and `/usr/etc/zypp/zypp.conf`), `/etc/zypp/vendors.d/*`, `/etc/zypp/locks`, `/etc/zypp/systemCheck*`, `/etc/zypp/needreboot*` | Solver/vendor/lock behaviour (see [Configuration](09-configuration-and-environment.md)) |
| Installed packages | `rpm -qa --qf '%{NAME}-%{VERSION}-%{RELEASE}.%{ARCH} %{VENDOR}\n' \| sort` | Vendor and version state |
| Product/arch | `/etc/products.d/`, `uname -m`, `/etc/os-release` | Arch and product base |
| Versions | `rpm -q libzypp zypper libsolv-tools rpm` | Many fixes are version specific |
| Proxy/env | `/etc/sysconfig/proxy`, relevant `ZYPP_*` env vars | Media issues |

Do **not** ask for `/etc/zypp/credentials.d/*` or `/root/.curlrc` contents (secrets). The solver testcase contains
repo aliases and package lists but no credentials (aliases are mangled in the libsolv part; unverified whether URLs
appear in `zypp-control.yaml` — the `url: []` field exists in the format).

Note: `zypper --debug-solver` writes to the **absolute** path `/var/log/zypper.solverTestCase`
(`zypper/src/solve-commit.cc`), **not** below `--root`, and `Testcase::createTestcase` `clean_dir`s an existing directory
first (when dumping the pool, which zypper does) — a second run overwrites the previous testcase. It needs write permission on `/var/log` (i.e. root).

### 15.2 Replaying with the stack-built deptestomatic

Build outputs in the zypp-stack (verified in `zypp-stack/build/<preset>/` for the presets present:
`Desktop_GCC-Debug`, `Desktop_Clang-Debug`, `Desktop_GCC-RelWithDebInfo`, `OpenSUSE_Tumbleweed_Dev_Environment_Debug`,
`Podman_Tumbleweed_GCC_Debug`):

```
build/<preset>/zypp-solver-testcase-runner/deptestomatic
build/<preset>/zypper/src/zypper
build/<preset>/libzypp/zypp/tools/zypp-NameReqPrv   (and other zypp/tools)
```

```bash
B=zypp-stack/build/Desktop_GCC-Debug
tar xf solverTestCase.tar.xz -C /tmp/zypp-test/tc
$B/zypp-solver-testcase-runner/deptestomatic /tmp/zypp-test/tc            # results prefixed ">!> "
$B/zypp-solver-testcase-runner/deptestomatic -vv /tmp/zypp-test/tc > new.log 2>&1
/usr/lib/zypp/testsuite/bin/deptestomatic -vv /tmp/zypp-test/tc > old.log 2>&1   # distro libzypp-testsuite-tools
diff -u old.log new.log                                                   # -vv = timestamp-free "diffable" lines
$B/zypp-solver-testcase-runner/deptestomatic 'cycle: --> a-1-1.x86_64 ##45##> b-1-1.x86_64 --48--> '   # decode a PREREQ cycle line from zypper.log
testsolv /tmp/zypp-test/tc/testcase.t      # libsolv-only replay (libsolv-tools / stack-built libsolv tools)
```

* deptestomatic calls `getZYpp()`; running it **as root** takes the global lock (`/run/zypp.pid`) and fails/waits if a
  zypper/PackageKit holds it ("Can't aquire ZYpp lock"). Run it as a normal user (non-root skips the lock,
  `ZYppFactory.cc`) or set `ZYPP_LOCKFILE_ROOT=/tmp/zypp-test` / `ZYPP_READONLY_HACK=1`.
* `-h` prints the trial command help (`dumpHelpOn`) — the set of trial verbs it understands lives in `deptestomatic.cc`.

### 15.3 Quick `--root` test system

```bash
Z=$B/zypper/src/zypper; R=/tmp/zypp-test/root
sudo $Z --root $R ar -f https://download.opensuse.org/tumbleweed/repo/oss/ oss
sudo $Z --root $R --gpg-auto-import-keys ref
$Z --root $R se -s bash                         # read-only ops work as user if $R is readable
sudo $Z --root $R in --download-only bash       # exercise download/PackageProvider without rpm
sudo $Z --root $R --non-interactive in bash     # real commit into the chroot (rpm --root; needs root, runs scriptlets in chroot)
$B/libzypp/zypp/tools/zypp-NameReqPrv --root $R -D bash
```

* `--installroot DIR` = like `--root` but **shares the host's repos** (zypper man page), handy to test against the
  host's repo config without copying it.
* `--disable-system-resolvables` makes zypper ignore installed packages (testing only).
* `--root` does not relocate the lock file: use `ZYPP_LOCKFILE_ROOT` (comment in `ZYppFactory.cc`: "Hack to circumvent
  the currently poor --root support") when a host zypper is running.
* The devel build uses build-dir `zypp-rpm`, workers and libsolv tools (`ENABLE_DEVEL_BUILD`), so the chroot commit tests
  the uninstalled code.
* To turn a user's system state into a local root without their packages: recreate their repos from `zypper lr -d`,
  then load their testcase instead — a real chroot reproduction is only needed for commit/rpm/scriptlet bugs.

---

## 16. Sanitizers and static analysis

| Tool | How | Scope (verified) |
|---|---|---|
| UBSAN | `-DENABLE_UBSAN_CHECKS=ON` **and** `-DCMAKE_BUILD_TYPE=Debug` | `CMakeLists.txt` adds `$<$<CONFIG:DEBUG>:-fsanitize=undefined>` to `zypp_initial_compiler_flags` (compile+link), i.e. everything that links the flag targets (libs, tools, tests). No effect in Release/RelWithDebInfo. Force-disabled in the zypp-stack (`disableOption( ENABLE_UBSAN_CHECKS )`). |
| ASAN | not wired; commented-out `-fsanitize=address` lines in `zyppng/CMakeLists.txt` | Use `-DCMAKE_CXX_FLAGS=-fsanitize=address -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address -DCMAKE_SHARED_LINKER_FLAGS=-fsanitize=address` manually (unverified that the whole tree is clean). Forking tests (`ExternalProgram`, `Process`) and nginx children are unaffected. |
| clang-tidy | `-DENABLE_CLANG_TIDY=ON` (needs `clang-tidy` in PATH, else only a warning) | Sets `ZYPP_CXX_CLANG_TIDY`, which is applied as `CMAKE_CXX_CLANG_TIDY` **only in `zyppng/CMakeLists.txt` and the `zyppng/tools/zypp-media-*` CMakeLists** — legacy `zypp/` and `zypp-logic` are not checked. Config: `libzypp/.clang-tidy` (bugprone-use-after-move [as error], move-forwarding-reference, dynamic-static-initializers, unsafe-functions, copy-constructor-init; modernize-use-override; performance-move-const-arg/inefficient-vector-operation/for-range-copy; cppcoreguidelines-prefer-member-initializer/special-member-functions). Force-disabled in the stack. |
| cppcheck | `-DENABLE_CPPCHECK=ON` | `ZYPP_CXX_CPPCHECK = cppcheck --std=c++17 --suppress=rethrowNoCurrentException`, applied the same way (zyppng only). Note `--std=c++17` on C++20 module code — expect false positives / parse failures on `import`/`co_await`. Not disabled by the stack. |
| valgrind | no suppression files, no CMake/ctest integration in the tree | `ctest -T memcheck` works generically if `MEMORYCHECK_COMMAND` is set (CTest default module); unverified that anyone runs it. Expect noise from libsolv pools / gpgme / curl global state; Boost.Test `--catch_system_errors=no` needed under valgrind too. |

Practical UBSAN run:

```bash
cmake -S libzypp -B build-ubsan -DCMAKE_BUILD_TYPE=Debug -DENABLE_UBSAN_CHECKS=ON -DENABLE_BUILD_TESTS=ON
cmake --build build-ubsan -j$(nproc)
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 ctest --test-dir build-ubsan --output-on-failure
```

Without `halt_on_error=1` UBSAN only prints `runtime error:` lines and the test passes — grep the ctest log
(`Testing/Temporary/LastTest.log`) for `runtime error`.

---

## Pitfalls & special cases

* Tests link the static `zypp-allsym`, not `libzypp.so` + `libzypp.map`: passing tests do not prove a symbol is exported to zypper/YaST.
* Run test binaries manually with `--catch_system_errors=no` (or `BOOST_TEST_CATCH_SYSTEM_ERRORS=no`), otherwise forking tests fail spuriously.
* `TESTS_SRC_DIR` is the `zypp/tests` root, not the test's own directory (except where a subdir redefines it, e.g. `zyppng/tests/config`).
* Media backend tests need nginx, vsftpd, squid, `basic_ncsa_auth`, libfcgi(++) at **configure** time — missing ones are `FATAL_ERROR`, not skips. Use `-DDISABLE_MEDIABACKEND_TESTS=ON` standalone; in the stack the option is force-disabled.
* `Proxy_test` makes libproxy a hard configure requirement unless `-DDISABLE_LIBPROXY=ON`.
* All WebServer tests use port 10001 (ProxyServer 10002) and no ctest resource locks exist → parallel `ctest -j` can make them flaky; port collisions with local services too.
* nginx runs as `user root;` only if the test runs as uid 0; otherwise unprivileged. SSL tests use the in-source certstore (`zypp-logic/tests/data/webconf/ssl`); the server/CA certs are valid until 2198, so expiry is not an issue, but regenerating the CA changes the `certstore/<hash>.0` name.
* `TestSetup` manipulates process singletons (`ZConfig` repo root/arch, `sat::Pool`, target). Multiple `TestSetup`s in one binary don't isolate each other; test-case order matters.
* `loadTestcaseRepos()` → `TestcaseSetup` sets `ZYPP_TESTSUITE_FAKE_ARCH` process-wide; afterwards commits throw.
* Env vars are often cached in function statics; `setenv()` mid-test may be ignored — set them before process start.
* `TestSetup::loadRepo()` disables gpgcheck by default; use `TSO_REPO_DEFAULT_GPG` when testing signature behaviour.
* Orphaned test sources (`media1..4_test.cc`, `file_exists_test.cc`, `throw_if_not_exists_test.cc`, `Signature_test.cc`, `Resolvable_test.cc`) are not built and probably don't compile.
* `zypp/tests/zyppng/` tests the async core inside legacy zypp; `zyppng/tests/` is the separate module-based NG suite (only with `BUILD_LIBZYPPNG`). Don't confuse them.
* NG tests link `zyppng-objlib-test` (module compiled twice with `ZYPPNG_TEST_BUILD`); internal symbols need `ZYPPNG_TEST_EXPORT`. The referenced `zyppng_add_module_test()` doesn't exist.
* OBS `%check` runs the full suite in every package build (media backend on), and the PR CI is only OBS via `.obs/workflows.yml`; `.travis.yml` is dead; GitHub Actions only builds docs on tags.
* `ENABLE_BUILD_TESTS` defaults OFF, but `-DSUSE`/`-DDEBIAN` silently force it ON; when OFF, tests are `EXCLUDE_FROM_ALL` but still configured (still need deps).
* Running ctest in the source root creates an untracked `Testing/` dir — don't commit it.
* Stack options are set with `CACHE FORCE`; `-D` overrides for them on the command line are silently ignored. `ENABLE_DEVEL_BUILD` bakes build-dir paths into the library — never package such a build.
* `zypp/tools`: every top-level `*.cc` becomes an executable automatically — adding a stray `.cc` there adds a target. Only `zypp-NameReqPrv` (and `zypp-rpm`) are installed; `refresh-ng`/`repomirror` are bit-rotted.
* `deptestomatic` resets vendor settings; replay results also depend on the libsolv version/patches used.
* `MediaSetAccess_test` uses port 10002 for its Web/FtpServer — the same port as `ProxyServer` in `MediaCurl_test`; `FtpServer` defaults to 10001 like `WebServer`.
* `WebServer::log()` is a stub returning `""`; nginx's own logs live in the server's `TmpDir` and are deleted with it (use `ZYPPTMPDIR` + a breakpoint to inspect them).
* `WebServer.cc` filters nginx output with `line.find_first_of("nginx: [alert] could not open error log file:") == 0` — `find_first_of` matches *any* of those characters, so most nginx lines (anything starting with e.g. `n`, `g`, `i`, `x`, `:`, space, `[`, `a`, ...) are silently dropped. Likely meant `rfind(...,0)==0`/`starts_with`; nginx startup errors can therefore be invisible.
* `WebServer::makeResponseString(status, headers, content)` joins `headers` with `"\r\n"` but adds no CRLF after the last one; callers must terminate every header with `\r\n` themselves (as `EvDownloader_test` does), otherwise the header block isn't closed and the body is parsed as headers. `Content-Type`/`Content-Length` are added automatically if absent. `makeBasicAuthHandler` compares the raw `HTTP_AUTHORIZATION` value, i.e. pass `"Basic <base64>"`.
* `TestSetup.h` does `using namespace zypp;` at file scope and includes Boost.Test — any test including it gets `zypp::` names unqualified (watch for ambiguities with `std::`/NG names).
* No `.solv` files are committed; repo tests depend on `repo2solv` from libsolv tools at runtime (stack devel build: baked-in path to the stack-built tool).
* `zypper --debug-solver` writes to the host's `/var/log/zypper.solverTestCase` even with `--root`, and wipes it first.
* Tests/deptestomatic run **as root** take the global zypp lock `/run/zypp.pid` (non-root skips it): a running zypper/PackageKit blocks or fails them. Use `ZYPP_LOCKFILE_ROOT` or `ZYPP_READONLY_HACK`.
* clang-tidy and cppcheck are only applied to `zyppng/` (and its `zypp-media-*` workers), not to `zypp/` or `zypp-logic/`; cppcheck is invoked with `--std=c++17` on C++20 module code.
* `ENABLE_UBSAN_CHECKS` only acts in `CMAKE_BUILD_TYPE=Debug`, and without `UBSAN_OPTIONS=halt_on_error=1` findings don't fail tests.
* `EvDownloader_test` (and its `.meta` metalink templates / `zyppng/data/downloader`) is not built; its data is kept alive only for it.
* Commit/rpm, non-curl media handlers, legacy `ZConfig`, `zypp-tui` and libzypp-glib have no unit tests (section 11.5) — a green ctest says nothing about them.

## Where to start when debugging

* **Failing test:** `ctest -R '^Name_test$' --output-on-failure -V`, then run the binary directly with
  `--catch_system_errors=no --log_level=all --run_test=<case>` and `ZYPP_LOGFILE=- ZYPP_FULLLOG=1`. Use gdb on the binary.
* **Media test fails to start:** check that nginx/squid/vsftpd exist at the configured paths (CMake output "Using nginx binary at: ..."), that ports 10001/10002 are free, and look at nginx output printed to stderr by the WebServer worker; the nginx temp prefix contains `log/error.log`.
* **Solver bug report:** get `/var/log/zypper.solverTestCase` (`zypper --debug-solver ...`) or `/var/log/updateTestcase-*`; replay with `deptestomatic -vv`, compare with libsolv `testsolv testcase.t`, inspect with `zypp-NameReqPrv --root <testcase> -a <name>` / `-D <pkg>`; turn the trimmed case into `zypp/tests/data/TC<Name>` + a Boost test using `loadTestcaseRepos`.
* **Logs from users:** `/var/log/zypper.log` (zypper), `/var/log/YaST2/y2log*` (YaST), `/var/log/zypp/history` (what was done). Grep for `ERR`/`WAR` and the relevant log group; see [Core infrastructure](02-core-infrastructure.md) for log format and groups.
* **Pool/repo state questions:** `zypp-NameReqPrv`, `DumpSelectable`, `zypp-list` with `--root` pointing at a testcase, a TestSetup dir (`repos.d`+`raw`) or a chroot.
* **Download/refresh issues:** reproduce in a test with `WebServer` request handlers (redirects, status codes, auth via `makeBasicAuthHandler`), or with `DownloadFiles` / `ProvideSignedDirectory`.
* **Integration with zypper:** build in zypp-stack and run `build/zypper/...` against the uninstalled libzypp (devel build uses build-dir `zypp-rpm`, workers and libsolv tools); use `--root` to avoid touching the host.
* **Which test covers X?** Section 11 inventory; if the subsystem is in 11.5, add a test using the skeletons in section 12.
* **Test only fails in the full binary:** run with `--run_test=<case>` alone vs. whole binary; suspect singleton state (`ZConfig`, `sat::Pool`, target, KeyRing, env `ZYPP_TESTSUITE_FAKE_ARCH`).
* **Need the temp files of a failing test:** `ZYPPTMPDIR=/tmp/zypp-test/zt` + breakpoint before the `TmpDir`/`WebServer` dtor, or give `TestSetup` an explicit root.
* **Reporter data:** use the checklist in section 15.1; replay solver cases with the stack's `build/<preset>/zypp-solver-testcase-runner/deptestomatic -vv` and diff against the distro build.
* **Suspected UB / memory errors:** Debug build with `ENABLE_UBSAN_CHECKS` and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`; ASAN via manual `CMAKE_CXX_FLAGS` (section 16).
* **Before merging:** full local `ctest` with media backend tests enabled (serially for the WebServer tests), plus the OBS `home:zypp-team` branch build triggered by the PR.
