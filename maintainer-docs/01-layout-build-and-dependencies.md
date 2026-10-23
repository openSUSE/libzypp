# Repository Layout, Build System and Dependencies

This chapter covers how the libzypp source tree is organised, how the CMake build puts it together, and which libraries and binaries come out of it. It explains how shared `zypp-logic/` sources are compiled twice, once into the legacy library and once into the NG targets. It also lists every CMake option, the `ENABLE_DEVEL_BUILD` path rerouting, the install layout, the libsolv linking rules and the external dependencies. The last part describes the `zypp-stack` meta project that builds libsolv, libzypp and zypper together.

All paths are relative to `libzypp/` unless stated otherwise.

---

## 1. Big picture

libzypp is one CMake project (`project(zypp-libs)` in `CMakeLists.txt`). It contains two library *generations* that share code:

```
                      CMakeLists.txt  (project zypp-libs)
                      - global options, find_package()s
                      - INTERFACE targets zypp_initial_*_compiler_flags
                      - SET_LOGGROUP macro
                               |
      +------------------------+-------------------------+-----------------+
      |                        |                         |                 |
  external/               zypp/  (BUILD_LIBZYPP,      zyppng/ (BUILD_LIBZYPPNG,  po/
  header-only             default ON)                 default OFF)               translations
  polyfills               project(LIBZYPP)            project(zyppng)
                          C++17, libzypp.so           C++20 modules, libzypp-glib.so
                               \                         /
                                \  include(...cmake)    /  include(...cmake)
                                 v                     v
                              zypp-logic/  (NOT a CMake subdirectory!)
                              *.cmake files define functions such as
                              zypp_add_core_target(); each generation calls them
                              with its own TARGETNAME and FLAGS
```

The main design decision is stated at the top of the root `CMakeLists.txt`: one source tree builds both generations, so code that does not need a rewrite can be shared "without having to rely on building another shared library and also be able to recompile the shared code with different compiler flags... e.g. coroutine support on/off". `zypp-logic/` is **never** added with `add_subdirectory()`. It only provides CMake *functions*, and each generation calls those functions to create its own static or object library from the same `.cc` files.

The tier rules (legacy is frozen, NG is where new work goes, and the dependency directions) are described in `AGENTS.md`. See also [zyppng](10-zyppng-next-generation.md).

---

## 2. Directory map

```
libzypp/
├── CMakeLists.txt            Root: options, dependency discovery, global flag targets, SET_LOGGROUP
├── AGENTS.md / CLAUDE.md / GEMINI.md   Agent/developer guidance (tier rules, module rules)
├── COPYING                   GPL-2.0-or-later
├── cmake/modules/            Custom Find modules + ZyppCommon/ZyppConfig
│   ├── ZyppCommon.cmake      Install dirs, SPECFILE/GENERATE_PACKAGING macros, zypp_configure_apiconfig()
│   ├── ZyppConfig.cmake      Installed to <libdir>/cmake/Zypp; zypper/YaST use it to find libzypp
│   ├── FindRpm.cmake, FindGpgme.cmake, FindUdev.cmake, FindReadline.cmake, FindOpenSSL.cmake,
│   │   Findlibproxy.cmake, FindZsync.cmake       Dependency finders (some unused, see Pitfalls)
│   ├── GLibTools.cmake, GObjectIntrospection.cmake   glib-mkenums / .gir generation for zypp-glib
│   └── printrpmver.c.in      Helper FindRpm uses to detect RPM_LIB_VER
├── external/                 Vendored header-only polyfills (offline-safe)
│   ├── CMakeLists.txt        INTERFACE targets zypp_ranges_polyfill, zypp_span_polyfill; update-* targets
│   ├── nanorange/            NanoRange single header (C++17 ranges polyfill), BSL-1.0
│   └── tcb-span/             tcb::span (std::span polyfill), BSL-1.0
├── po/                       *.po translations, zypp.pot, PotfileDiff.sh, PotfileUpadte.sh
├── maintainer-docs/          This handbook
├── zypp/                     LEGACY project (project LIBZYPP) – frozen
│   ├── CMakeLists.txt        Version math, DEVEL_BUILD paths, legacy flags, conf-file install, subdirs
│   ├── VERSION.cmake         LIBZYPP_MAJOR/MINOR/PATCH/COMPATMINOR + bump rules
│   ├── libzypp.spec.cmake    RPM spec template (configured to build/package/libzypp.spec)
│   ├── libzypp.pc.cmake      pkg-config template
│   ├── package/libzypp.changes  Changelog (release notes, bug numbers)
│   ├── zypp.conf, zypp.conf.LEGACY, zypp.conf.README   Vendor config / legacy /etc copy / README
│   ├── needreboot, systemCheck, zypp-history.lr        Files installed below /etc
│   ├── zypp/                 The real legacy sources: ZYpp, RepoManager, ResPool, solver, target, media…
│   │   ├── base/ media/ misc/ parser/ pool/ repo/ sat/ solver/ target/ ui/ workflows/ zypp_detail/ private/
│   │   ├── ng/               Legacy-side "ng" layer (Context, repomanager workflows, adaptors)
│   │   └── libzypp.map       Linker version script (exports zypp::* only)
│   ├── zypp-core/            Thin CMake shim → zypp-logic/zypp-core (+ legacy-only ng/async headers)
│   ├── zypp-media/           Shim → zypp-logic/zypp-media
│   ├── zypp-curl/            Shim → zypp-logic/zypp-curl
│   ├── zypp-common/          Shim → zypp-logic/zypp-common (KeyManager, PublicKey, DrunkenBishop)
│   ├── shared/               Shim → zypp-logic/shared (commit-proto-obj)
│   ├── zypp-tui/             Terminal UI helpers for zypper (tables, colors, Out), libzypp-tui.a
│   ├── zypp-compat/          Installed-only compat headers <zypp/Url.h> → <zypp-core/Url.h> etc.
│   ├── tools/                Small programs; zypp-rpm/ (installed helper), refresh-ng/, repomirror/ (devtools)
│   ├── doc/                  Man pages (asciidoc) + autodoc/ (doxygen) + autoinclude/ (doxygen pages)
│   └── tests/                Boost.Test suites for legacy (media, zypp, parser, repo, sat, zyppng)
├── zypp-logic/               SHARED source pool (frozen except */ng/)
│   ├── logic-helper.cmake    zypp_logic_prefix_sources / zypp_add_sources / zypp_logic_setup_includes
│   ├── zypp-core/            Url, Pathname, ExternalProgram, logging, parsers, glib event loop (ng/), APIConfig.h.in
│   ├── zypp-media/           Media exceptions, mount, CDTools, credentials, ng/ provide specs
│   ├── zypp-curl/            Curl config, proxy info (libproxy via dlopen), metalink/zsync parsers, ng/network
│   ├── zypp-common/          KeyManager/KeyRing private impl, PublicKey (gpgme)
│   ├── zypp/                 Arch, Edition, IdString, Dep, Locale…; ng/sat/stringpool, ng/config/zyppconfig
│   ├── shared/commit/        CommitMessages: protocol between libzypp and zypp-rpm
│   └── tests/                Shared test helper lib (tests/lib) and test data
└── zyppng/                   NEXT-GEN project (project zyppng), C++20
    ├── CMakeLists.txt        NG flags, DEVEL_BUILD paths, -DZYPPNG=1 -DZYPP_ENABLE_ASYNC
    ├── lib/                  zyppng-core/-media/-curl static libs + zyppng-objlib (C++20 module 'zyppng')
    │   ├── zypp/ng/*.cppm    Module partitions (sat, log, base, context…)
    │   ├── zypp-media/ng/    Provide/worker framework (NG only)
    │   └── shared/tvm/       tvm-protocol-obj (test VM media protocol)
    ├── api/                  zypp-glib (GObject C API, .gir), VERSION.cmake (reads packaging/*.inc)
    ├── tools/                zypp-media-{http,disc,nfs,smb,dir,disk,iso,copy,tvm,chksum} workers
    ├── packaging/            libzypp-glib.spec, .changes, libzypp-glib-version.inc (version source of truth)
    └── tests/                NG tests (async, config, media, sat), using zyppng-objlib-test
```

Notes:
- The stack root contains a stray directory, `<zypp-stack>/zypp-logic/zypp/ng/sat` (outside `libzypp/`). Nothing in the build references it. It looks like leftover output from a refactoring script.
- `zypp-logic/zypp/mkdir/` is an empty directory and is also a leftover.
- `zypp/tools/migrate-sources/` has a `CMakeLists.txt`, but `zypp/tools/CMakeLists.txt` never adds it, so it is dead code.

---

## 3. Root `CMakeLists.txt` in detail

### 3.1 Options

| Option | Default | Effect (verified) |
|---|---|---|
| `BUILD_LIBZYPP` | ON | `add_subdirectory(zypp)`. Also requires Boost `program_options`. |
| `BUILD_LIBZYPPNG` | OFF | `add_subdirectory(zyppng)`. The spec turns it on only with `--with build_libzyppng` and forces it to 0 for `suse_version < 1600`. |
| `ENABLE_BUILD_DOCS` | OFF (ON with `-DSUSE`/`-DDEBIAN`) | Builds `zypp/doc/autodoc` (doxygen) as part of ALL. |
| `ENABLE_BUILD_TRANS` | OFF (ON with SUSE/DEBIAN) | `po/` is in ALL. Otherwise it is `EXCLUDE_FROM_ALL` and you build it with `make translations`. |
| `ENABLE_BUILD_TESTS` | OFF (ON with SUSE/DEBIAN) | `zypp/tests` is in ALL. Otherwise it is EXCLUDE_FROM_ALL. NG tests are always added when NG is built. |
| `ENABLE_ZSTD_COMPRESSION` | OFF | zypp-core links `${ZSTD_LIBRARY}`. |
| `ENABLE_ZCHUNK_COMPRESSION` | OFF | `pkg-config zck` is required. Defines `ENABLE_ZCHUNK_COMPRESSION=1` and adds `base/zckstream.cc` and `ng/network/zckhelper.cc`. |
| `ENABLE_VISIBILITY_HIDDEN` | ON | Legacy only: `-fvisibility=hidden` on `zypp_general_compiler_flags`. NG always uses hidden visibility. |
| `ENABLE_SIGC_BLOCK_WORKAROUND` | OFF | Defines `LIBZYPP_USE_SIGC_BLOCK_WORKAROUND=1` (gnome bug 784550, segfault when slots are cleared during emission). The spec enables it for `sle_version < 150200`. |
| `DISABLE_MEDIABACKEND_TESTS` | OFF | When OFF, the tests *require* nginx, vsftpd, squid, basic_ncsa_auth, libfcgi and libfcgi++ (FATAL_ERROR if any is missing). |
| `DISABLE_LIBPROXY` | OFF | When OFF, defines `WITH_LIBPROXY_SUPPORT`, compiles `proxyinfo/proxyinfolibproxy.cc` and links `${CMAKE_DL_LIBS}`. libproxy is **dlopen()ed** (`dlopen("libproxy.so.1")`) and never linked. |
| `DISABLE_AUTODOCS` | OFF | When OFF, a missing doxygen is a FATAL_ERROR, even if docs are disabled. |
| `ENABLE_DEVEL_BUILD` | OFF | See §6. |
| `ENABLE_CLANG_TIDY` / `ENABLE_CPPCHECK` | OFF | Sets `ZYPP_CXX_CLANG_TIDY` / `ZYPP_CXX_CPPCHECK`. Each sub-cmake copies these into `CMAKE_CXX_CLANG_TIDY`/`CMAKE_CXX_CPPCHECK` before `add_library`. Some files are marked `SKIP_LINTING` (`target/TargetImpl.cc`, `ng/providemessage.cc`). cppcheck runs with `--std=c++17`. |
| `ENABLE_UBSAN_CHECKS` | OFF | `-fsanitize=undefined` in DEBUG config only. |
| `NOSCHNICKSCHNACK` | (var) | Turns off docs, translations and tests. A legacy shortcut. |
| `SUSE` / `DEBIAN` | (var) | System type. Defines `-DSUSE`/`-DDEBIAN`, enables docs/trans/tests, and SUSE sets `LibSolv_USE_STATIC_LIBS ON`. Setting more than one is a FATAL_ERROR. The spec also passes `-DFEDORA=1`/`-DMAGEIA=1`, but the root CMake ignores these. |
| `ZYPPCONFDIR` | `/usr/etc` | Vendor config directory. Baked into `APIConfig.h` as `LIBZYPP_ZYPPCONFDIR`. See §8. |
| `LIBZYPP_CODESTREAM` | (var) | The spec passes `0%{suse_version}:0%{sle_version}:0%{is_opensuse}`. Ends up as `LIBZYPP_CODESTREAM` in `APIConfig.h`. |
| `LIB`, `INCLUDE`, `SYSCONFDIR`, `MANDIR`, `DOC_INSTALL_DIR` | (vars) | Install path overrides (see `ZyppCommon.cmake`). |

Options defined in `zypp/CMakeLists.txt` (legacy):

| Option | Default | Effect |
|---|---|---|
| `KEEP_LEGACY_ZYPPCONF` | OFF | ON: installs `zypp.conf.LEGACY` as `${SYSCONFDIR}/zypp/zypp.conf`. OFF: installs `zypp.conf.README` there instead. The spec enables it for `suse_version < 1610`. |
| `LIBZYPP_CONFIG_USE_DELTARPM_BY_DEFAULT` | OFF | `#cmakedefine01` in APIConfig.h. Used in `ZConfig.cc` as the default for `download.use_deltarpm`. The spec sets it for ≤ SLE15 SP6. |
| `LIBZYPP_CONFIG_USE_CLASSIC_RPMTRANS_BY_DEFAULT` | OFF | Default commit backend in `ZYppCommitPolicy.cc`. ON for `suse_version < 1600`. |
| `LIBZYPP_CONFIG_USE_LEGACY_CURL_BACKEND_BY_DEFAULT` | OFF | Default media backend in `media/MediaHandlerFactory.cc`. ON for `< 1600`. |
| `LIBZYPP_CONFIG_USE_SERIAL_PACKAGE_DOWNLOAD_BY_DEFAULT` | OFF | Used in `target/commitpackagepreloader.cc`. ON for `< 1600`. |

`zyppng/lib/CMakeLists.txt` declares the same four `LIBZYPP_CONFIG_*` options again. In `zypp/tools/CMakeLists.txt` you also get `ENABLE_DEVTOOL_REPOMIRROR` (needs Notcurses++) and `ENABLE_DEVTOOL_REFRESHNG`, both default OFF.

Code reads these build-time defaults through the macro `APIConfig(VAR)`. It is defined in `zypp-logic/zypp-core/APIConfig.h.in` as `#define APIConfig(VAR) VAR`. Use it as `#if APIConfig(LIBZYPP_CONFIG_...)`. Runtime configuration that can override these defaults is covered in [Configuration](09-configuration-and-environment.md).

### 3.2 Global flag targets

The root project defines INTERFACE "flag" libraries. Each generation layers its own flags on top:

```
zypp_initial_compiler_flags   (-pthread -fno-strict-aliasing -g -Wall -Wp,-D_GLIBCXX_ASSERTIONS
                               -fvisibility-inlines-hidden -Woverloaded-virtual -Wnon-virtual-dtor
                               -Werror=format-security(if supported) -Wl,-as-needed(GCC only)
                               -O2 + ZYPP_NDEBUG NDEBUG when not DEBUG
                               _FILE_OFFSET_BITS=64, LIBSOLV_SOLVABLE_PREPEND_DEP, HAVE_PIPE2,
                               HAVE_UDEV, WITH_LIBPROXY_SUPPORT, HAVE_LIBXML2_XMLCTXTSETERRORHANDLER,
                               include dirs of boost/curl/libxml2/zlib/yaml-cpp/libsolv/gpgme/sigc++/glib)
zypp_initial_lib_compiler_flags  (-fPIC -rdynamic -Wl,--no-allow-shlib-undefined -Wl,--no-undefined)
zypp_initial_exe_compiler_flags  (-fPIE -pie -Wl,--no-undefined)

legacy:  zypp_general_compiler_flags  = initial + -fvisibility=hidden + ZYPP_RPM_BINARY/ZYPP_WORKER_PATH
         zypp_lib_compiler_flags      = initial_lib + general
         zypp_exe_compiler_flags      = initial_exe + general
NG:      zyppng_general_compiler_flags = initial + hidden + -fconcepts-diagnostics-depth=4 (GCC dbg)
                                         + lld when Clang + ZYPP_RPM_BINARY/ZYPP_WORKER_PATH (NG values)
         zyppng_lib_compiler_flags / zyppng_exe_compiler_flags
```

- `LIBSOLV_SOLVABLE_PREPEND_DEP` makes libsolv rename struct members whose names are not valid in C++20 (comment: "tell libsolv to rename its variables to be C++20 compatible"). It is defined twice in the root file. That is harmless.
- `-g` is always on, so release builds carry debug info too. Packaging strips it.
- The legacy C++ standard comes from `ZYPPCOMMON_CXX_STANDARD 17` (`ZyppCommon.cmake`). NG sets `CMAKE_CXX_STANDARD 20`. `CMAKE_CXX_EXTENSIONS OFF` everywhere.
- Every sub-cmake does `ADD_DEFINITIONS(-DLOCALEDIR="<prefix>/share/locale" -DTEXTDOMAIN="zypp" -DZYPP_DLL)`. All generations use the single gettext domain `zypp`.

### 3.3 The `SET_LOGGROUP` macro

```cmake
MACRO( SET_LOGGROUP _group _files )
  SET_SOURCE_FILES_PROPERTIES( ${_files} COMPILE_FLAGS -DZYPP_BASE_LOGGER_LOGGROUP=\\"${_group}\\" )
  FOREACH (_currentFile ${ARGN}) ... same ... ENDFOREACH
ENDMACRO()
```

The macro sets the per-source define `ZYPP_BASE_LOGGER_LOGGROUP`, which the logging macros `XXX/DBG/MIL/WAR/ERR/SEC` in `zypp-logic/zypp-core/base/Logger.h` use. If no group is set, it falls back to `"DEFINE_LOGGROUP"`. To override the group for a single file, use `#undef`/`#define` in the file; many NG files do this (e.g. `zyppng/lib/zypp/ng/sat/pool.cc` → `"zyppng::satpool"`).

Default groups per target:

| Sources | Group |
|---|---|
| zypp-logic/zypp-core | `zypp-core` |
| zypp-logic/zypp-media | `zypp-media` |
| zypp-logic/zypp-curl | `zypp-curl` (but see the bug in Pitfalls) |
| zypp-logic/zypp-common, zypp-logic/zypp (+ all legacy `zypp/zypp` sources passed in) | `zypp` |
| zypp-glib | `zyppng` |
| zyppng module units | none (commented out). Files define their own group or get `DEFINE_LOGGROUP`. |

Caveat: the macro sets the `COMPILE_FLAGS` source property, so it **replaces** any `COMPILE_FLAGS` set earlier on the same file. `SET_LOGGROUP` and other `COMPILE_FLAGS` users, such as `ADD_TESTS`, must not touch the same file.

### 3.4 Dependency discovery and why each library is needed

| Dependency | How found | Why |
|---|---|---|
| pthreads | `find_package(Threads)`, FATAL without pthreads | Threads (CleanerThread, NG event loops, zypp-rpm). |
| Boost: `unit_test_framework`, `thread` (+ `program_options` if legacy) | `find_package(Boost REQUIRED …)` (policy CMP0167 NEW = BoostConfig) | Tests, `Boost_THREAD_LIBRARY` linked into libzypp, CLI parsing in `zypp/tools`. Header-only boost is used throughout legacy code. AGENTS.md says not to bring Boost into NG. |
| Gettext | `find_package(Gettext REQUIRED)` | Translations (`_()`), and msgmerge/msgfmt for `po/`. |
| libcurl | `find_package(CURL REQUIRED)` | HTTP/FTP downloads (zypp-curl, legacy MediaCurl, NG http worker). The spec requires ≥ 7.19.4. |
| libxml2 | `find_package(LibXml2 REQUIRED)` | repomd/primary/metalink XML parsing. A symbol check for `xmlCtxtSetErrorHandler` defines `HAVE_LIBXML2_XMLCTXTSETERRORHANDLER` (newer libxml2 API). |
| zlib | `find_package(ZLIB REQUIRED)` | gzstream (`base/gzstream.cc`), compressed metadata. |
| yaml-cpp | `find_package(YAML-CPP REQUIRED)` | YAML parsing. If config mode is used, it falls back to the target name `yaml-cpp`. |
| libsolv + libsolvext | stack override or `find_package(LibSolv REQUIRED ext)` | Dependency solver and pool, `.solv` files. See §7. |
| gpgme ≥ 1.8.0 | `pkg_search_module`, fallback `FindGpgme.cmake` (`GPGME_PTHREAD_LIBRARIES`) | Keyring and signature checks (zypp-common KeyManager). |
| OpenSSL | `find_package(OpenSSL REQUIRED)` (custom `FindOpenSSL.cmake`) | Digest/CheckSum, crypto. zypp-core links `${OPENSSL_LIBRARIES}`. |
| udev | `find_package(Udev)` (optional) | CD/DVD device detection. Without it you get a warning "CD device detection will be poor" and `HAVE_UDEV` is not defined. |
| sigc++-2.0 | `pkg_check_modules REQUIRED` | Signals in the NG event loop / zypp-core ng base. |
| glib-2.0 | `pkg_check_modules REQUIRED` | GLib main loop underlying `zypp-core/ng/base/eventdispatcher_glib.cc`. This applies to legacy too, because zypp-core is shared. |
| zck | `pkg_check_modules` when zchunk is enabled | zchunk deltas. |
| zstd | `ZSTD_LIBRARY` | zstd streams (only `find_library`'d in the non-stack libsolv feature probe, see Pitfalls). |
| rpm / rpmio | `FindRpm.cmake` (called inside `zypp_add_zypp_target` and in `tools/zypp-rpm`) | rpmdb access, install transactions. `RPM_LIB_VER` controls `HAVE_RPM_VERIFY_TRANSACTION_STEP` (rpm ≥ 4.15 <5, or ≥ 6) and `_RPM_5`. The changelog notes "cmake: correctly detect rpm6 (fixes #689)". |
| readline | `FindReadline.cmake` (in zypp-tui) | Interactive prompts in zypp-tui (zypper). |
| libutil | linked as `util` | `forkpty` in ExternalProgram / zypp-rpm. |
| doxygen, a2x/asciidoctor | `find_program` | Autodocs (FATAL unless `DISABLE_AUTODOCS`). Man pages: `zypp/doc/CMakeLists.txt` FATALs if neither a2x nor asciidoctor is found. |
| gobject-2.0, gio-2.0, g-ir-scanner | NG `api/` only | zypp-glib and introspection. |
| nginx, vsftpd, squid, fcgi | tests only | Media backend tests (`zypp/tests/media`). |

The spec file (`zypp/libzypp.spec.cmake`) lists the matching BuildRequires. Check it when you bump minimum versions, e.g. `libsolv-devel(-static) >= 0.7.40 (bsc#1279541)`.

---

## 4. How `zypp-logic/*.cmake` turns shared sources into per-generation targets

### 4.1 Helper functions (`zypp-logic/logic-helper.cmake`)

- `zypp_logic_prefix_sources(PREFIXDIR d OUT var FILES …)` prepends a directory to each file name.
- `zypp_add_sources(var files…)` is a macro that calls the above with `PREFIXDIR "${CMAKE_CURRENT_FUNCTION_LIST_DIR}"`. Because it uses `CMAKE_CURRENT_FUNCTION_LIST_DIR`, the paths resolve **relative to the `.cmake` file that defines the calling function** (i.e. inside `zypp-logic/<lib>/`), not relative to the calling `CMakeLists.txt`. So `SOURCES` passed in from `zypp/zypp/CMakeLists.txt` stay relative to that directory, while the files listed inside `zypp.cmake` are absolute paths into `zypp-logic/zypp/`.
- `zypp_logic_setup_includes()` is a macro that needs `arg_TARGETNAME`. It adds the *parent* of the calling source dir, the parent of the `.cmake` dir (= `zypp-logic/`) and the parent bin dir as PRIVATE + INTERFACE (BUILD_INTERFACE) include paths. That is why code includes `<zypp-core/Url.h>`, `<zypp-media/…>` and `<zypp/…>`, and why the generated `APIConfig.h` (in `<bindir>/zypp-core/`) is found as `<zypp-core/APIConfig.h>`.

### 4.2 Target factory functions

Every factory takes `TARGETNAME`, `FLAGS` (the flag interface lib, linked PRIVATE), optional extra `HEADERS`/`SOURCES`, and an `INSTALL_HEADERS` option.

| `.cmake` | Function | Kind | Legacy call (zypp/…) | NG call (zyppng/lib/…) |
|---|---|---|---|---|
| `zypp-core/zypp-core.cmake` | `zypp_add_core_target` | STATIC | `zypp-core` (+ `ng/async/*.h` headers, INSTALL_HEADERS) | `zyppng-core` (+ `ng/async/pipelines/await.h`, `wait.hpp`) |
| `zypp-media/zypp-media.cmake` | `zypp_add_media_target` | STATIC | `zypp-media` | `zyppng-media` + NG-only Provide/worker sources from `zyppng/lib/zypp-media/ng/` |
| `zypp-curl/zypp-curl.cmake` | `zypp_add_curl_target` | STATIC | `zypp-curl` | `zyppng-curl` |
| `zypp-common/zypp-common.cmake` | `zypp_add_common_target` | STATIC | `zypp-common` | *not built*: `add_subdirectory(zypp-common)` is commented out in `zyppng/lib/CMakeLists.txt` |
| `zypp/zypp.cmake` | `zypp_add_zypp_target` | OBJECT | `zypp-objlib` with **all legacy `zypp/zypp` sources passed as SOURCES** | `zyppng-objlib` and `zyppng-objlib-test` (+ module FILE_SETs) |
| `shared/shared.cmake` | `zypp_add_shared_targets` | OBJECT | `commit-proto-obj` | `zyppng-commit-proto-obj` (TARGETPREFIX "zyppng") |

Things you only see by reading the code:

- The shared code is compiled **twice** (C++17 + legacy flags, then C++20 + NG flags). Code under `zypp-logic/` must compile in both modes. Use `#ifdef ZYPPNG` / `ZYPP_ENABLE_ASYNC` (defined only in `zyppng/CMakeLists.txt`) to tell them apart.
- `zypp_add_zypp_target` contains a tiny file list of its own (Arch, Edition, IdString, Dep, CapMatch, Locale, …, `ng/sat/stringpool.cc` (LEGACY-REQUIRED) and `ng/config/zyppconfig.cc`). Many variables in it are now empty placeholders ("kept so the aggregation below does not need to be restructured").
- `zypp_EARLY_SRCS` exists both in `zypp/zypp/CMakeLists.txt` (`ZConfig.cc`) and `zypp.cmake` (`Arch.cc CountryCode.cc LanguageCode.cc Locale.cc`). These are appended near the *end* of the source list. The name suggests it controls static-initialisation order (these TUs hold globals other TUs use during startup), but no comment documents this. **Assume the order matters and do not reorder these lists.**
- `ADD_DEFINITIONS(...)` inside these functions changes *directory* properties of the calling directory, not just the target.
- `zypp-core.cmake` prints `Comp defs for <target>: …` at configure time (debug output).
- `zypp_add_core_target` links `zypp_ranges_polyfill`/`zypp_span_polyfill` PUBLIC, plus glib, openssl, pthread, sigc++, zlib and util as INTERFACE.

### 4.3 Generated `APIConfig.h`

`zypp_configure_apiconfig(PREFIX CODESTREAM IN OUT)` (in `ZyppCommon.cmake`) configures `zypp-logic/zypp-core/APIConfig.h.in` with the version from the given prefix:
- legacy: `zypp_configure_apiconfig(LIBZYPP "${LIBZYPP_CODESTREAM}" …)` → `<build>/zypp/zypp-core/APIConfig.h`, installed to `include/zypp-core/APIConfig.h`.
- NG: `zypp_configure_apiconfig(LIBZYPPGLIB "ng" …)` → NG copy carrying the libzypp-glib version numbers and codestream `"ng"`.

The header contains `LIBZYPP_VERSION_*`, `LIBZYPP_SOVERSION`, `LIBZYPP_CODESTREAM`, `LIBZYPP_ZYPPCONFDIR` and the four `LIBZYPP_CONFIG_*` `#cmakedefine01`s.

---

## 5. Targets produced

### 5.1 Legacy (`BUILD_LIBZYPP`)

```
               zypp-objlib (OBJECT: zypp/zypp/*.cc + zypp-logic/zypp/*.cc)
               commit-proto-obj (OBJECT: zypp-logic/shared/commit)
                         |  objects linked directly
                         v
  libzypp.so.<SO_FIRST>  "zypp" SHARED, -Wl,--version-script=zypp/zypp/libzypp.map
     PRIVATE link (whole static libs pulled in as needed), IN THIS ORDER:
       zypp-common.a → zypp-curl.a → zypp-media.a → zypp-tui.a → zypp-core.a
       → libsolvext/libsolv → rpm/rpmio → gettext → boost_thread → yaml-cpp → udev
  zypp-allsym.a          same objects as STATIC, deps INTERFACE; no version script → all symbols
  zypp-tui.a             STATIC, installed (libzypp-devel ships libzypp-tui.a for zypper)
  zypp-core.a, zypp-media.a, zypp-curl.a, zypp-common.a   STATIC, NOT installed
```

- `add_zypp_lib()` exists because "object files are not derived transiently" from OBJECT libraries. It links `zypp-objlib` directly into each of `zypp` and `zypp-allsym`.
- The link order comment in `zypp/zypp/CMakeLists.txt` says: "CAUTION, link those libs always first and always in that order, otherwise you get symbol lookup errors". zypp-tui must come before zypp-core ("link zypp-tui.a at last, to prevent undefined references in case tui functions are used in libzypp").
- `libzypp.map` exports only `zypp::*` (plus vtables, typeinfo and guard variables). Everything else is local, including `zyppng::*` symbols from zypp-core/ng and libsolv symbols. Combined with `-fvisibility=hidden`, this means that tests and tools needing internals must link `zypp-allsym`.
- SONAME math (`zypp/CMakeLists.txt`): `CURRENT = MAJOR*100+MINOR`, `AGE = MINOR-COMPATMINOR`, `SO_FIRST = CURRENT-AGE`. For 17.38.16 with COMPATMINOR 35 this gives `libzypp.so.1735.3.16` (SOVERSION 1735). Bumping `COMPATMINOR` therefore changes the SONAME. See [Release](12-release-packaging-and-compat.md).
- A POST_BUILD hook runs `./buildmon` if it exists in the build dir. This is an optional developer hook.
- `zypp/zypp/CMakeLists.txt` installs the yum `.rng` schemas into `share/zypp/schema/yum`.

### 5.2 NG (`BUILD_LIBZYPPNG`)

```
 zyppng-objlib        OBJECT  zypp_add_zypp_target + FILE_SET CXX_MODULES (ng/*.cppm) + impl units
 zyppng-objlib-test   OBJECT  same sources, -DZYPPNG_TEST_BUILD=1 (ZYPPNG_TEST_EXPORT → export)
                              INTERFACE deps: zyppng-curl → zyppng-media → zyppng-core → libsolv …
 zyppng               STATIC  dummy.cc + zyppng-objlib + zyppng-commit-proto-obj (no public C++ ABI)
 libzypp-glib.so      SHARED  zypp-glib (GObject API), Zypp-1.0.gir, version from packaging/*.inc
 tvm-protocol-obj     OBJECT  zyppng/lib/shared/tvm
 zypp-media-* workers EXE     one per backend, output into <build>/zyppng/tools/workers
```

- `zyppng/lib/zypp/CMakeLists.txt` requires CMake 3.28 (FILE_SET CXX_MODULES), but the root project only requires 3.17. Building NG with an older CMake fails at that point.
- Module interface units must be listed in bottom-up dependency order in `ZYPPNG_MODULE_INTERFACE_UNITS`.
- libzypp-glib versioning: `zyppng/api/VERSION.cmake` parses `%define libzypp_glib_{major,minor,patch,compatminor}` from `zyppng/packaging/libzypp-glib-version.inc`, using the same SONAME math as legacy.
- zyppng and libzypp-glib are a separate RPM (`zyppng/packaging/libzypp-glib.spec`).

### 5.3 Executables

| Binary | Source | Installed? | Destination |
|---|---|---|---|
| `zypp-rpm` | `zypp/tools/zypp-rpm/` | yes | `${CMAKE_INSTALL_FULL_LIBEXECDIR}/zypp/zypp-rpm` (`ZYPP_LIBEXEC_INSTALL_DIR`) |
| `zypp-NameReqPrv` | `zypp/tools/zypp-NameReqPrv.cc` | yes (the only tool) | `<prefix>/bin` |
| `CalculateReusableBlocks`, `DownloadFiles` | `zypp/tools` | no | link `zypp-allsym` (they need internal symbols) |
| other `zypp/tools/*.cc` (DiskUsageCounter, DumpSelectable, ToolScanRepos, zypp-CheckAccessDeleted, zypp-cpeid, zypp-install, zypp-list, zypp-pubkey, zypp-runpurge, percent-encode, patch_find_bug, ProvideSignedDirectory) | globbed `*.cc` | no | dev/debug tools linked against `libzypp.so` |
| `refresh-ng`, `repomirror` | `zypp/tools/*/` | opt-in devtools | – |
| `zypp/tools/zypp/*.py`, `ScanTwHistory` | scripts | no | developer helpers |
| `zypp-media-{http,disc,nfs,smb,dir,disk,iso,copy,tvm,chksum}` | `zyppng/tools/` | yes, unconditionally when NG is built | `${CMAKE_INSTALL_LIBEXECDIR}/zyppng/workers` |

`zypp/tools/CMakeLists.txt` builds **every** `*.cc` in the directory as its own executable (`FILE(GLOB ALLCC …)`). Dropping a new `.cc` file in there creates a new program. Linking the installed tools against the shared lib instead of allsym is deliberate: "Make sure not to statically linked installed tools (bsc#1228787)".

`zypp-rpm` is the out-of-process rpm transaction helper. It links `commit-proto-obj` + `zypp-core` + rpm, not libzypp. The wire protocol order is documented in `zypp/zypp/target/TargetImpl.cc` near `ZYPP_RPM_BINARY` (see [Target/commit](08-target-commit-and-rpm.md)).

---

## 6. `ENABLE_DEVEL_BUILD` – what it reroutes

When set, CMake prints `Zypp devel build enabled, do not do this in production` and changes these compile-time defines (interface defines on `zypp_general_compiler_flags` / `zyppng_general_compiler_flags`):

| Define | Normal value | DEVEL_BUILD value | Consumer |
|---|---|---|---|
| `ZYPP_RPM_BINARY` (legacy) | `<libexecdir>/zypp/zypp-rpm` | `${LIBZYPP_BINARY_DIR}/tools/zypp-rpm/zypp-rpm` | `TargetImpl.cc` (SINGLE_RPMTRANS commit) |
| `ZYPP_WORKER_PATH` (legacy) | `<libexecdir>/zypp/workers` | `${LIBZYPP_BINARY_DIR}/tools/workers` | **unused in legacy code** (grep finds no consumer; legacy media handlers are in-process) |
| `ZYPP_RPM_BINARY` (NG) | `<libexecdir>/zyppng/zyppng-rpm` | `${zyppng_BINARY_DIR}/tools/zyppng-rpm/zyppng-rpm` | no `zyppng-rpm` target exists yet |
| `ZYPP_WORKER_PATH` (NG) | `<libexecdir>/zyppng/workers` | `${zyppng_BINARY_DIR}/tools/workers` | `zyppng/lib/zypp-media/ng/private/provide_p.h` `DEFAULT_PROVIDE_WORKER_PATH` |
| `ZYPP_REPO2SOLV_PATH` | *undefined* → `repo2solv` (or `repo2solv.sh` if `/usr/bin/repo2solv` is missing) | `${zypp-stack_BINARY_DIR}/libsolv/tools/repo2solv` **only if `ZYPP_STACK_BUILD`** | `zypp/zypp/ng/repo/workflows/repomanagerwf.cc` |
| `ZYPP_RPMDB2SOLV_PATH` | *undefined* → `rpmdb2solv` from `$PATH` | `${zypp-stack_BINARY_DIR}/libsolv/tools/rpmdb2solv` (stack only) | `zypp/zypp/target/TargetImpl.cc` |

Consequences:
- A DEVEL_BUILD binary embeds absolute build-tree paths. Never package or install it. The zypp-stack README warns that such builds are "not out of the box suitable to be used with make install".
- DEVEL_BUILD does **not** reroute config files (`/etc/zypp`, `ZYPPCONFDIR`), plugins (`/usr/lib/zypp/plugins`, hard-coded in `ZConfig.cc`) or caches. For those, use `ZYPP_CONF`, a target root, etc. (see [Configuration](09-configuration-and-environment.md)).
- NG tests do not rely on the define. They get `ZYPPNG_WORKERS_DIR="${ZYPPNG_WORKER_BINARY_DIR}"` from `zyppng/tests/CMakeLists.txt`.

---

## 7. libsolv linking

Normal (distribution) build:
- `-DSUSE` sets `LibSolv_USE_STATIC_LIBS ON`, and `find_package(LibSolv REQUIRED ext)` then picks the static `libsolv.a`/`libsolvext.a`. libzypp links libsolv **statically** on SUSE. Because of this, the spec needs `libsolv-devel-static` on `suse_version >= 1600` ("Code16 moved static libs to libsolv-devel-static", changelog), and libzypp must be rebuilt whenever libsolv changes ABI or behaviour.
- Static libsolvext needs its compression libraries linked explicitly. The root CMake probes `solv/solvversion.h` for `LIBSOLVEXT_FEATURE_{ZLIB,LZMA,BZIP2,ZSTD}_COMPRESSION` and appends zlib, lzma, bz2 and zstd to `LibSolv_LIBRARIES` (the comment says "static case: Assert libsolv.a ist the last one linked"). zchunk is assumed to be libsolv-internal (the probe is commented out).
- libsolv symbols do not leak out of libzypp.so because of `libzypp.map`.
- `repo2solv` and `rpmdb2solv` are run as **external programs**, so libsolv tools are a runtime requirement too. The spec uses `libsolv-tools-base`, jsc#PED-8153.

Stack build (`ZYPP_STACK_BUILD` set by the parent):
- `LibSolv_FOUND TRUE`, `LibSolv_INCLUDE_DIRS = ${ZYPP_STACK_INCLUDE_DIRS}` (the symlinked `build/include/solv`), and `LibSolv_LIBRARIES = libsolvext libsolv`. These are the in-tree CMake targets, built static because the parent forces `ENABLE_STATIC`/`DISABLE_SHARED`.
- The feature probe is skipped. Compression libs come from the patched libsolv targets (`libsolvext INTERFACE libsolv ${SYSTEM_LIBRARIES}`).
- `GENERATE_PACKAGING` prefixes the `svncheck`/`srcpackage*` targets with `${PROJECT_NAME}_` to avoid target-name clashes between subprojects.

---

## 8. Install layout

`ZyppCommon.cmake` computes `LIB_INSTALL_DIR` (`<prefix>/${LIB}` or GNUInstallDirs libdir), `INCLUDE_INSTALL_DIR` (`<prefix>/include`), `SYSCONFDIR` (`/etc` when the prefix is `/usr`, else `<prefix>/etc`), `ZYPP_LIBEXEC_INSTALL_DIR = ${CMAKE_INSTALL_FULL_LIBEXECDIR}/zypp`, `ZYPPNG_LIBEXEC_INSTALL_DIR = …/zyppng`, `MANDIR`, and `DOC_INSTALL_DIR` (`<prefix>/share/doc/packages/libzypp`).

| Path | What | Origin |
|---|---|---|
| `<libdir>/libzypp.so.*` | the library | CMake |
| `<libdir>/libzypp-tui.a`, `libzypp.so`, `pkgconfig/libzypp.pc`, `cmake/Zypp/{ZyppConfig,ZyppCommon}.cmake` | devel | CMake |
| `include/zypp`, `zypp-core`, `zypp-media`, `zypp-curl`, `zypp-common`, `zypp-tui` | public headers (+ `zypp-compat` forwarding headers into `include/zypp/…`) | CMake |
| `<libexecdir>/zypp/zypp-rpm` | rpm helper | CMake |
| `<prefix>/bin/zypp-NameReqPrv` | tool | CMake |
| `${ZYPPCONFDIR}/zypp/zypp.conf` (default `/usr/etc/zypp/zypp.conf`) | vendor config | CMake |
| `/etc/zypp/zypp.conf.README` **or** `/etc/zypp/zypp.conf` (LEGACY copy) | depends on `KEEP_LEGACY_ZYPPCONF` | CMake |
| `/etc/zypp/needreboot`, `/etc/zypp/systemCheck` | config(noreplace) | CMake |
| `/etc/logrotate.d/zypp-history.lr` | the spec moves it to `%{_distconfdir}/logrotate.d` if defined | CMake + spec |
| `share/zypp/schema/yum/*.rng` | schemas | CMake |
| `share/locale/<lang>/LC_MESSAGES/zypp.mo` | translations | `po/` |
| `share/man/man1,man5` | man pages | `zypp/doc` |
| `/etc/zypp/{repos.d,services.d,systemCheck.d,vars.d,vendors.d,multiversion.d,needreboot.d,credentials.d,zypp.conf.d}` | empty dirs (on Fedora/RHEL `repos.d` is a symlink to `/etc/yum.repos.d`) | **spec only** |
| `${zyppconfdir}/zypp/zypp.conf.d` | vendor drop-ins | spec only |
| `/usr/lib/zypp/plugins/{appdata,commit,services,sigcheck,system,urlresolver}` | plugin dirs (hard-coded `pluginsPath "/usr/lib/zypp/plugins"` in `ZConfig.cc`; note `/usr/lib`, not `lib64`) | spec only |
| `/var/lib/zypp`, `/var/log/zypp` (mode 750), `/var/cache/zypp` | state | spec only |
| `<libexecdir>/zyppng/workers/zypp-media-*` | NG workers | CMake (NG) |

`ZYPPCONFDIR` vs `/etc`: libzypp is moving to UAPI.6 config handling. The vendor `zypp.conf` lives in `ZYPPCONFDIR` (`/usr/etc` by default). In the spec it is `%{_distconfdir}` if defined, else `/usr/share` on Fedora ("fixes #693"), else `/usr/etc`. Admin overrides live in `/etc/zypp/zypp.conf[.d/*.conf]`. The value is baked into `LIBZYPP_ZYPPCONFDIR` and used by `zypp-logic/zypp-core/parser/econfdict.cc` (`ConfigurationContext::_defaultDistconfDir`). The spec's `%pre`/`%posttrans` `rpmsaveSave`/`rpmsaveRestore` macros keep an admin-modified `/etc/zypp/zypp.conf` (which becomes `.rpmsave` when the package stops owning it) during the migration. The spec also `Provides: libzypp(econf) = 0` and plugin capability versions.

---

## 9. Translations (`po/`)

- POT domain `zypp`. `xgettext` options: `-L C++ --boost --keyword=_ --keyword=_:1,2 --keyword=__ --keyword=N_ --keyword=PL_:1,2`.
- Sources scanned (`GLOB_RECURSE`, `*.h` and `*.cc` only): `zypp/zypp`, `zypp/zypp-common`, `zypp/zypp-tui`, `zypp/shared`, `zypp-logic`, `zyppng`.
- The `translations` target (in ALL only with `ENABLE_BUILD_TRANS`) regenerates the pot in the build dir, runs `msgmerge --no-fuzzy-matching` + `msgfmt` per language, and prints a diff against the committed `po/zypp.pot` through `PotfileDiff.sh`.
- `make potfile_update` commits the new pot into the source tree via `PotfileUpadte.sh` (sic) if there is a difference. Run it before a release when translatable strings have changed. The `.po` files themselves are maintained upstream through openSUSE Weblate. The repo only receives them.
- `zyppng/api/CMakeLists.txt` exports `POT_FILE_DEPENDS_ZYPP_GLIB` to the parent scope, but `po/CMakeLists.txt` does not use it.

---

## 10. `external/` polyfills

- `zypp_ranges_polyfill` provides `external/nanorange/include` (NanoRange, pinned commit `bf32251d…`). `zypp_span_polyfill` provides `external/tcb-span/include` (tcb/span, pinned `836dc6a0…`). Both are INTERFACE targets with BUILD_INTERFACE includes only, so they are never installed. Public installed headers must therefore not include them (unverified whether any do; check before installing new headers that use `ng/base/ranges.h`/`span.h`).
- They are used by zypp-core (PUBLIC) and zypp-objlib (PRIVATE), through `zypp-logic/zypp-core/ng/base/ranges.h` and `span.h`, so that C++17 legacy code and C++20 NG code share one API.
- `make update-ranges` / `make update-span` re-download the headers with curl. These targets need network access, and you run them manually before committing.

---

## 11. The parent meta project `zypp-stack`

`<zypp-stack>` is a superproject with git submodules: `libzypp`, `libsolv`, `zypper`, `zypp-solver-testcase-runner`, `obs-service-cpio-repack`, `mcp-server-zypp`. It also contains non-submodule dirs: `zypp_graph` (Qt6 visualiser), `yast-pkg-bindings` and `scripts/`.

Its `CMakeLists.txt`:
1. With Clang, selects `ld.lld-<major>` or `ld.lld` via `CMAKE_*_LINKER_FLAGS_INIT` for the whole stack. zyppng does the same on its own.
2. Declares `ENABLE_DEVEL_BUILD` ON and sets `ZYPP_STACK_BUILD TRUE` (a normal variable, visible to subprojects).
3. Uses `enableOption()`/`disableOption()` to force cache values (`CACHE … FORCE`), so command-line `-D` overrides for these options are **overwritten** on each configure. libsolv gets SUSE defaults: `SUSE`, `WITH_LIBXML2`, `MULTI_SEMANTICS`, `ENABLE_STATIC`, `DISABLE_SHARED`, `ENABLE_APPDATA`, `ENABLE_COMPS`, the bzip2/lzma/zstd/zchunk compressions, `ENABLE_COMPLEX_DEPS`, `ENABLE_SUSEREPO`, `ENABLE_HELIXREPO`, `USE_VENDORDIRS`, and also `CMAKE_VERBOSE_MAKEFILE`.
4. Sets `__CMAKE_PARSE_ARGUMENTS_INCLUDED` because libsolv overloads `cmake_parse_arguments`, then runs `add_subdirectory(libsolv)`.
5. Symlinks `libsolv/src/*.h`, `libsolv/ext/*.h` and the generated `solvversion.h` into `build/include/solv/`, then sets `ZYPP_STACK_INCLUDE_DIRS`.
6. Forces libzypp options: DEVEL_BUILD, BUILD_TESTS, BUILD_TRANS, VISIBILITY_HIDDEN, ZCHUNK, `BUILD_LIBZYPPNG` and `BUILD_LIBZYPP` ON. CLANG_TIDY, UBSAN, `DEVTOOL_REFRESHNG` and `DISABLE_MEDIABACKEND_TESTS` OFF, so the media tests need nginx/vsftpd/squid/fcgi. It also forces `INSTALL_NG_BINARIES`, which libzypp never reads (no-op).
7. `add_subdirectory(libzypp)`, then symlinks `build/include/zypp-core/APIConfig.h` for zypper, then adds `zypper`, `zypp-solver-testcase-runner`, and `mcp-server-zypp` (with `ZYPP_MCP_ENABLE_HTTP`).
8. Includes a commented-out FetchContent alternative.

Note that `SUSE` is forced for libsolv as a cache var, so libzypp's root `IF (SUSE)` also triggers in a stack build ("Building for SUSE"). That sets `-DSUSE` and turns on docs (doxygen required).

`libsolv.patch` (must be applied by hand: `cd libsolv && git apply ../libsolv.patch`) makes libsolv usable as a subproject:
- replaces `CMAKE_SOURCE_DIR`/`CMAKE_BINARY_DIR` with `libsolv_SOURCE_DIR`/`libsolv_BINARY_DIR` throughout (top-level, bindings, ext, src, test), because those variables point to the stack root when libsolv is nested;
- adds `TARGET_LINK_LIBRARIES(libsolvext INTERFACE libsolv ${SYSTEM_LIBRARIES})` and the `_static` equivalent in `ext/CMakeLists.txt`, so linking `libsolvext` pulls in libsolv and the compression libs;
- includes some whitespace-only hunks (tabs → spaces in `check_linker_flag`).

If the libsolv submodule moves forward, the patch may stop applying. Refresh it then.

---

## Pitfalls & special cases

- `zypp-logic/` is not a CMake subdirectory. A new shared `.cc` file must be added to the right `zypp-logic/<lib>/<lib>.cmake` list. It then compiles into **both** generations (C++17 and C++20), so check that both builds pass.
- `zypp_add_sources` resolves paths relative to the `.cmake` file that defines the function, while `SOURCES`/`HEADERS` passed in by callers are relative to the caller's directory.
- `zypp-logic/zypp-curl/zypp-curl.cmake` calls `SET_LOGGROUP( "zypp-curl" $${zypp_curl_lib_SRCS} )`. The `$$` typo turns the first file into a bogus `$/…` path, so that one file (`curlconfig.cc` unless `arg_SOURCES` is set) gets no group and logs as `DEFINE_LOGGROUP`.
- `SET_LOGGROUP` overwrites the `COMPILE_FLAGS` source property. Don't combine it with other `COMPILE_FLAGS` users on the same file.
- Static-lib link order in `add_zypp_lib()` and `zyppng_propagate_link_deps()` is significant: common → curl → media → tui → core → libsolv. Reordering causes undefined symbols.
- `libzypp.map` exports only `zypp::*`. Tests and internal tools must link `zypp-allsym`, but installed tools must link `libzypp.so` (bsc#1228787).
- `zypp/tools/CMakeLists.txt` globs every `*.cc` into an executable. Only `zypp-NameReqPrv` (and `zypp-rpm`) are installed. `migrate-sources/` is never built.
- Bumping `LIBZYPP_COMPATMINOR` in `zypp/VERSION.cmake` changes the SONAME (`SO_FIRST = MAJOR*100 + COMPATMINOR`).
- `ENABLE_DEVEL_BUILD` bakes build-tree paths (zypp-rpm, workers, repo2solv/rpmdb2solv in stack builds) into the binary. It does not reroute config, plugins or caches.
- Legacy `ZYPP_WORKER_PATH` is defined but nothing in legacy code reads it. NG `ZYPP_RPM_BINARY` points to a `zyppng-rpm` target that does not exist.
- Without `ZYPP_REPO2SOLV_PATH`, libzypp runs `repo2solv`, or `repo2solv.sh` if `/usr/bin/repo2solv` is not a file. Both libsolv tools are runtime dependencies, run as external programs.
- SUSE builds link libsolv statically. A libsolv update can require a libzypp rebuild, and on 16.0+ you need `libsolv-devel-static`.
- `HAL_FOUND` and `LIBPROXY_FOUND` are never set by any `find_package` call, so the HAL branches are dead code. The `ProxyInfoLibproxy.h` compat header is never installed (looks unintended; unverified). libproxy is dlopen'ed (`libproxy.so.1`).
- `ZSTD_LIBRARY` is only searched for in the non-stack libsolv feature probe, yet `ENABLE_ZSTD_COMPRESSION` links `${ZSTD_LIBRARY}` into zypp-core. It may come out empty in other configurations (unverified).
- Doxygen is required at configure time unless `DISABLE_AUTODOCS` is set, and a2x/asciidoctor is required for man pages. `-DSUSE` silently enables docs, translations and tests.
- NG needs CMake ≥ 3.28 (C++ module FILE_SETs), even though the root only declares 3.17. Module interface units must be listed bottom-up.
- `zypp-common` is not built for NG (commented out). NG code must not depend on KeyManager from zypp-common.
- The zypp-stack parent force-sets cache options on every configure, so `-D` overrides for those options do not stick. Edit the parent `CMakeLists.txt` instead.
- `po/CMakeLists.txt` scans only `*.h`/`*.cc` (`GLOB_RECURSE`), not `.cppm`/`.cpp`/`.hpp`. Translatable strings in NG module interface units (`.cppm`) or in `.cpp` files are **not** extracted into `zypp.pot`. Check `zypp.pot` after moving translatable code.
- `EARLY_SRCS` lists (ZConfig, Arch, Locale, CountryCode, LanguageCode) are placed last on purpose. The intent is undocumented (presumably static-init order), so keep the order.
- `/usr/lib/zypp/plugins` is hard-coded (not libdir/lib64), and most `/etc/zypp/*.d` dirs are created by the spec, not by CMake. A plain `make install` gives an incomplete layout.

## Where to start when debugging

- **Configure problems:** run `cmake` with `--log-level=STATUS` and read the messages printed by the root file: `Building for SUSE`, `boost found…`, `Found gpgme…`, `Using zypp-rpm from path: …`, `soname: …`, `Comp defs for zypp-core: …`. These show which branches were taken.
- **Wrong helper binary at runtime:** check `ZYPP_RPM_BINARY`/`ZYPP_WORKER_PATH` in `build/compile_commands.json` (configure with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`) or with `strings libzypp.so | grep zypp-rpm`.
- **Undefined symbols at link time:** check the static link order in `zypp/zypp/CMakeLists.txt` (`add_zypp_lib`) and whether the consumer should link `zypp-allsym`. Use `nm -DC libzypp.so | grep ' T '` to confirm the version script exports the symbol.
- **Log lines with group `DEFINE_LOGGROUP`:** the file has no `SET_LOGGROUP` coverage. Add a local `#undef/#define ZYPP_BASE_LOGGER_LOGGROUP`. Logs go to `/var/log/zypp/zypper.log` via zypper, or set `ZYPP_LOGFILE=-` / `ZYPP_FULLLOG=1` (see [Core infrastructure](02-core-infrastructure.md)).
- **libsolv header/version mismatch in the stack:** look at `build/include/solv/` symlinks and confirm `libsolv.patch` is applied (`git -C libsolv diff --stat`).
- **Media tests fail to configure:** install nginx, vsftpd, squid and fcgi, or pass `-DDISABLE_MEDIABACKEND_TESTS=ON` (not possible inside zypp-stack without editing its CMakeLists).
- **Tests:** `ctest --output-on-failure` in the build dir. Legacy tests are in `zypp/tests/*` (Boost.Test via `ADD_TESTS`, linked against `zypp-allsym` + `zypp_test_utils` from `zypp-logic/tests/lib`). NG tests are in `zyppng/tests/*` against `zyppng-objlib-test`. See [Testing](11-testing.md).
- **Translations:** `make translations`, then `make potfile_update` to refresh `po/zypp.pot`.
- **Packaging:** `make srcpackage` (requires a clean git tree, `svncheck`) builds `build/package/libzypp-<ver>.tar.bz2`, the configured `libzypp.spec` and a copy of `libzypp.changes`. See [Release](12-release-packaging-and-compat.md).
