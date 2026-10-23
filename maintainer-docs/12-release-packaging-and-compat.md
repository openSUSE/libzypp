# Release Engineering, Packaging & Compatibility

This chapter covers how libzypp is versioned (`zypp/VERSION.cmake`, the soname math), how the changelog and release commits/tags are made (`zypp/mkChangelog`), how it is packaged (`zypp/libzypp.spec.cmake` with its per-codestream conditionals), and how binary compatibility is kept for zypper, YaST and PackageKit (symbol visibility, `libzypp.map`, `LEGACY()` guards, compat headers, deprecation).
It ends with a step-by-step release checklist and an ABI review checklist for PRs.

Related chapters: [Layout & build](01-layout-build-and-dependencies.md), [Configuration](09-configuration-and-environment.md), [zyppng](10-zyppng-next-generation.md), [Testing](11-testing.md), [Consumers & API surface](14-consumers-and-api-surface.md).

---

## 1. Overview: what gets released

| Artifact | Source of truth | Version file | Changes file |
|---|---|---|---|
| `libzypp` (+ `-devel`, `-devel-doc`) | `zypp/libzypp.spec.cmake` (configured to `build/.../package/libzypp.spec`) | `zypp/VERSION.cmake` | `zypp/package/libzypp.changes` |
| `libzypp-glib` (+ `-devel`, `typelib-1_0-Zypp-1_0`) | `zyppng/packaging/libzypp-glib.spec` (static spec, not configured) | `zyppng/packaging/libzypp-glib-version.inc` | `zyppng/packaging/libzypp-glib.changes` |

Consumers whose ABI we must not break: **zypper**, **YaST** (`yast2-pkg-bindings`, `libyui-qt-pkg`/`libyui-ncurses-pkg`), **PackageKit zypp backend**, and **deptestomatic** / testsuite tools (via `ZYPP_API_DEPTESTOMATIC`). The changelog shows that missing exports for each of these needed follow-up releases after the switch to hidden visibility (see §5).

---

## 2. Versioning: `zypp/VERSION.cmake`

### 2.1 The file (verbatim comments, explained)

```cmake
# MAJOR	Major number for this branch.
# MINOR	The most recent interface number this library implements.
# COMPATMINOR	The latest binary compatible minor number this library implements.
# PATCH	The implementation number of the current interface.
#
# - The package VERSION will be MAJOR.MINOR.PATCH.
# - Libtool's -version-info will be derived from MAJOR, MINOR, PATCH
#   and COMPATMINOR (see configure.ac).
# - Changing MAJOR always breaks binary compatibility.
# - Changing MINOR doesn't break binary compatibility by default.
#   Only if COMPATMINOR is changed as well.
#
# 1) After branching from TRUNK increment TRUNKs MAJOR and
#    start with version `MAJOR.0.0' and also set COMPATMINOR to 0.
# 2) Update the version information only immediately before a public release ...
# 3) If the library source code has changed at all since the last update,
#    then increment PATCH.
# 4) If any interfaces have been added, removed, or changed since the last
#    update, increment MINOR, and set PATCH to 0.
# 5) If any interfaces have been added since the last public release, then
#    leave COMPATMINOR unchanged. (binary compatible change)
# 6) If any interfaces have been removed since the last public release, then
#    set COMPATMINOR to MINOR. (binary incompatible change)
```

Notes:
- "see configure.ac" is **stale** — the build is CMake; the math lives in `zypp/CMakeLists.txt` (below).
- Rule 1 ("increment TRUNK's MAJOR after branching") is historic. In practice MAJOR has been `17` for all current codestreams (SLE15, SLE16, Tumbleweed all build from `master`); the only actively maintained older line is `16.22.x` (SLE‑12 LTSS, see §9).
- "Interface changed" in rule 4/6 must be read as **ABI** change: changing a class layout, a virtual table, an inline function's semantics, or removing an exported symbol all count.

The "MOST IMPORTANT" block below the rules describes the release commit convention:

```cmake
#   - Before you submitt to git:
#     - Remember the new version in 'LAST RELEASED:'
#     - State the new version in the changes file by adding a line
#       "- version MAJOR.MINOR.PATCH (COMPATMINOR)"
#     - Commit changes and version files together in a separate
#       commit using -m 'changes MAJOR.MINOR.PATCH (COMPATMINOR)'
#     - Tag the above commit with 'MAJOR.MINOR.PATCH' using
#       -m "tagging MAJOR.MINOR.PATCH".
# - Consider calling ./mkChangelog to assist you.
SET(LIBZYPP_MAJOR "17")
SET(LIBZYPP_COMPATMINOR "35")
SET(LIBZYPP_MINOR "38")
SET(LIBZYPP_PATCH "16")
#
# LAST RELEASED: 17.38.16 (35)
```

The `# LAST RELEASED:` line is **machine-parsed** by `mkChangelog` — do not reformat it.

### 2.2 Soname math (`zypp/CMakeLists.txt`)

```cmake
MATH( EXPR LIBZYPP_NUMVERSION "${LIBZYPP_MAJOR} * 10000 + ${LIBZYPP_MINOR} * 100 + ${LIBZYPP_PATCH}" )
MATH( EXPR LIBZYPP_CURRENT    "${LIBZYPP_MAJOR} * 100 + ${LIBZYPP_MINOR}" )
MATH( EXPR LIBZYPP_AGE        "${LIBZYPP_MINOR} - ${LIBZYPP_COMPATMINOR}" )
MATH( EXPR LIBZYPP_SO_FIRST   "${LIBZYPP_CURRENT}-${LIBZYPP_AGE}" )   # == MAJOR*100 + COMPATMINOR
SET( LIBZYPP_VERSION_INFO "${LIBZYPP_SO_FIRST}.${LIBZYPP_AGE}.${LIBZYPP_PATCH}" )
SET( LIBZYPP_SOVERSION_INFO "${LIBZYPP_SO_FIRST}" )
```

Worked example for 17.38.16 (35):

```
CURRENT  = 17*100+38 = 1738
AGE      = 38-35     = 3
SO_FIRST = 1738-3    = 1735          ==> SONAME libzypp.so.1735
file     = libzypp.so.1735.3.16
NUMVERSION = 173816
```

So **the soname only changes when MAJOR or COMPATMINOR changes**. A MINOR bump alone (17.37→17.38) keeps `libzypp.so.1735`. The same value `LIBZYPP_SO_FIRST` drives `LEGACY()` (§6).

These numbers are exported to C++ via `zypp-logic/zypp-core/APIConfig.h.in` (generated by `zypp_configure_apiconfig()` in `cmake/modules/ZyppCommon.cmake`):

```c
#define LIBZYPP_VERSION_STRING "@VERSION@"
#define LIBZYPP_VERSION_MAJOR  @LIBZYPP_MAJOR@
#define LIBZYPP_VERSION_MINOR / _PATCH ...
#define LIBZYPP_SOVERSION      @LIBZYPP_SO_FIRST@
#define LIBZYPP_VERSION        @LIBZYPP_NUMVERSION@
#define LIBZYPP_CODESTREAM     "@LIBZYPP_CODESTREAM@"
#define LIBZYPP_ZYPPCONFDIR    "@ZYPPCONFDIR@"
#cmakedefine01 LIBZYPP_CONFIG_USE_DELTARPM_BY_DEFAULT ... (see §4.3)
```

### 2.3 History of soname changes (from `libzypp.changes`)

The `(N)` in `- version X.Y.Z (N)` lines is COMPATMINOR. Recent transitions:

| Version | COMPATMINOR | Reason (changelog) |
|---|---|---|
| 17.9.0 | 9 | "Need to fix soname due to libsolv ABI changes (bsc#1115341)" |
| 17.22.0 | 22 | |
| 17.32.0 | 32 | |
| 17.33.0 | 33 | "Add ZYPP_API for exported functions and switch to visibility=hidden (jsc#PED-8153)" |
| 17.33.4 | **35** (mistake) | followed by 17.34.0 (34) "Fix versioning scheme" |
| 17.34.0 | 34 | |
| 17.35.0 | 35 | "Update soname due to RepoManager refactoring and cleanup." |
| 17.36 … 17.38.16 | 35 | current |

Lesson from 17.33.4: COMPATMINOR must never be set higher than MINOR; the mkChangelog `I` option does it right (COMPATMINOR = MINOR+1 = new MINOR).

Note the libsolv angle: libsolv is linked **statically** on SUSE (`SET(LibSolv_USE_STATIC_LIBS ON)` in top-level `CMakeLists.txt` under `IF (SUSE)`), but libsolv types leak through public headers (`sat::detail::IdType` etc.), hence libsolv ABI changes can force a libzypp soname bump (bsc#1115341).

---

## 3. Changelog and release commits

### 3.1 Changes file format (`zypp/package/libzypp.changes`)

Standard OBS `.changes` format, newest entry first:

```
-------------------------------------------------------------------
Mon Sep 21 17:23:26 CEST 2026 - ma@suse.de

- BuildRequires:  %{libsolv_devel_package} >= 0.7.40 (bsc#1279541)
  This fix resolves issues in online migrations to SLES 16.1.
- version 17.38.16 (35)

-------------------------------------------------------------------
```

Conventions (verified in recent entries):
- One `- ` bullet per change, continuation lines indented by 2 spaces, wrapped at ~66 columns.
- Bug references in parentheses at the end of the first line: `(bsc#NNNN)`, `(bsc#NNNN, CVE-YYYY-NNNNN)`, `(jsc#PED-NNNN)`, `(fixes #NNN)` for GitHub issues, `(fixes openSUSE/zypper#NNN)` for cross-repo issues. OBS/IBS maintenance tooling checks for these references, so a fix destined for SLE maintenance **must** carry its bsc#/CVE reference.
- The last bullet is `- version MAJOR.MINOR.PATCH (COMPATMINOR)`.
- Bullet text is in practice the **git commit subject + body** (see mkChangelog), so write good commit messages: subject line = changelog line, incl. bug number.

### 3.2 `zypp/mkChangelog`

Must be run from `zypp/` (it uses relative paths `package/*.changes` and `VERSION.cmake`). Options: `-n` dry run, `-h` help.

Flow:

```
git fetch; branch must be fast-forward with origin/<branch>  (else exit 7)
files other than VERSION.cmake/changes staged?  -> forced dry run
parse VERSION.cmake (awk)  -> LAST_RELEASE/LAST_COMPAT from "# LAST RELEASED:" line,
                              THIS_RELEASE/THIS_COMPAT from SET() lines
tag $LAST_RELEASE must exist (else exit 8)
if version unchanged: prompt
   (a)bort (c)ontinue (P)atch+1 (M)inor+1,patch=0
   (I)ncompat: COMPATMINOR=MINOR+1, MINOR+1, PATCH=0   (e)dit VERSION.cmake in vi
build new entry:
   git log --no-merges --pretty='@@%B' LAST_RELEASE..HEAD
   drop "Translated using Weblate" commits
   join paragraphs, add trailing '.' if missing, fold -w 66, "- " / "  " prefixes
   + "- version X.Y.Z (C)"
open entry+old file in vi (hardcoded `vi`, not $EDITOR despite the help text),
strip trailing blanks; prompt (a)bort (c)ontinue (s)ubmit (e)dit
(s)ubmit: update "# LAST RELEASED:" line,
          git commit -m "changes X.Y.Z (C)" ; git tag -m "tagging X.Y.Z" X.Y.Z
          print: git push --tags origin HEAD
```

If the version is unchanged and you submit, it commits only the changes file with message `changes` (no tag).

The generated text is a draft: maintainers routinely edit it (drop internal/refactoring commits, merge related ones, add user-facing explanation, e.g. the 17.38.15 `repoGpgCheck` entry).

### 3.3 Observed release workflow (git log evidence)

- Release commits: `changes 17.38.16 (35)` touching only `zypp/VERSION.cmake` and `zypp/package/libzypp.changes`; annotated tag `17.38.16` with message `tagging 17.38.16`. ~295 of the last 300 release commits are by Michael Andres (ma@suse.de); occasional ones by Benjamin Zeller and Dirk Müller (the latter a changes-only entry without version line, for a packaging tweak).
- Cadence: roughly every 1–3 weeks, PATCH bumps; MINOR bumps for new features/API additions (17.36.0 Feb 2025 … 17.38.0).
- Security fixes are released as dedicated PATCH releases, often simultaneously on master (17.38.x) and the SLE‑12 branch (16.22.x), e.g. CVE-2026-44942 → 17.38.13 and 16.22.19.
- Weblate pushes `Translated using Weblate (<lang>)` commits directly to master. These are filtered from the changelog, but "Added translation using Weblate" commits are not (§15).
- `Translation: updated .pot file` commits are produced by the `potfile_update` target (§8).

### 3.4 OBS / submission

What the repo tells us:
- `make srcpackage` (target defined by `GENERATE_PACKAGING` in `cmake/modules/ZyppCommon.cmake`) first runs `svncheck` (fails if `git status --porcelain` is non-empty), then builds `libzypp-<VERSION>.tar.bz2` via CPack and copies it plus `libzypp.changes` into `<builddir>/package/`. `srcpackage_local` skips the clean-tree check. In a zypp-stack build these targets are prefixed (`LIBZYPP_srcpackage`).
- `SPECFILE()` configures `libzypp.spec.cmake` → `<builddir>/package/libzypp.spec` and `package/libzypp-rpmlint.cmake` → `<builddir>/package/libzypp-rpmlintrc` (Source1 of the spec).
- `.obs/workflows.yml`: for **pull requests**, OBS SCM-CI branches `zypp:ci:libzypp/libzypp` and `zypp:ci:libzypp-glib/libzypp-glib` into `home:zypp-team`. A push-to-master workflow is present but commented out.
- `.github/workflows/build-and-publish-doc.yml` builds the doxygen docs and deploys them to GitHub Pages.
- `.travis.yml` (Tumbleweed docker, trusty) is **obsolete** leftover.

**Not documented in the repo** but verified via the public OBS API (Oct 2026): the devel project is `zypp:Head` → `openSUSE:Factory`, and the SLE 15 maintenance projects are `_link`s to `zypp:Head`. See §13 for the full picture. The IBS (SUSE-internal) maintenance submission procedure is still not publicly documented. Ask the previous maintainer.

---

## 4. The spec file: `zypp/libzypp.spec.cmake`

`Version: @VERSION@` is substituted by CMake. `Release: 0`, `Source: %{name}-%{version}.tar.bz2`, `Source1: %{name}-rpmlintrc`.

### 4.1 Build conditionals by codestream

| Conditional | Condition | Effect |
|---|---|---|
| `build_libzyppng` | `%bcond_with` (off) | `-DBUILD_LIBZYPPNG=1`, BR gobject-introspection |
| `classic_rpmtrans_as_default` | on if `suse_version < 1600` | `-DLIBZYPP_CONFIG_USE_CLASSIC_RPMTRANS_BY_DEFAULT=1`; SLE16/TW default to single_rpmtrans |
| `libsolv_devel_package` | `suse_version >= 1600` → `libsolv-devel-static`, else `libsolv-devel` | Code16 moved static libs into -devel-static; needed because `-DSUSE` links libsolv statically |
| `zchunk` | `suse_version > 1500` or `sle_version >= 150400` or Leap ≥ 15.1 | `-DENABLE_ZCHUNK_COMPRESSION=1` |
| `zstd` | SLE/Leap 15+ | `-DENABLE_ZSTD_COMPRESSION=1` |
| `bz2`, `xz` | `sle_version >= 120300` / `suse_version >= 1330` / non-SUSE | BRs (libsolv external refs) |
| `mediabackend_tests` | on | BR nginx, vsftpd, squid, FastCGI; off → `-DDISABLE_MEDIABACKEND_TESTS=1` |
| `sigc_block_workaround` | `sle_version < 150200` | GNOME bug 784550 (segfault clearing connections during emission) |
| `keep_legacy_zyppconf` | `suse_version < 1610` | installs full `/etc/zypp/zypp.conf` (`zypp.conf.LEGACY`) instead of only `zypp.conf.README` |
| `zyppconfdir` | `%{_distconfdir}` if defined, Fedora → `/usr/share` (issue #693), else `/usr/etc` | vendor config location (UAPI.6 config spec) |

Additional `EXTRA_CMAKE_OPTIONS` on SUSE:

```
-DLIBZYPP_CODESTREAM=0%{?suse_version}:0%{?sle_version}:0%{?is_opensuse}
suse_version <= 1500 && sle_version <= 150600:  -DLIBZYPP_CONFIG_USE_DELTARPM_BY_DEFAULT=1
suse_version < 1600:  -DLIBZYPP_CONFIG_USE_LEGACY_CURL_BACKEND_BY_DEFAULT=1
                      -DLIBZYPP_CONFIG_USE_SERIAL_PACKAGE_DOWNLOAD_BY_DEFAULT=1
                      -DBUILD_LIBZYPPNG=0
```

So **SLE 15 SPx gets a conservative behaviour set** (classic rpmtrans, legacy curl backend, serial downloads, deltarpm on ≤15.6) from the *same source* as Tumbleweed. Behaviour differences between codestreams are therefore build-time defaults, not branches. When a bug is reported against SLE15, reproduce with those CMake options.

### 4.2 `LIBZYPP_CODESTREAM`

A free-form string `"<suse_version>:<sle_version>:<is_opensuse>"`, e.g. `01500:0150600:00` (note the `0` prefixes from the spec). Exposed as `LIBZYPP_CODESTREAM` in `APIConfig.h`, printed in the CMake configure message ("Building for SUSE (...)") and logged at startup by `ZConfig` (`zypp/zypp/ZConfig.cc`: `MIL << "libzypp: " LIBZYPP_VERSION_STRING << " (" << LIBZYPP_CODESTREAM << ")"`) and in `ZConfig::about()`. Use it in logs to tell which codestream build a user runs. No code branches on its value. Re-verified: the only uses are `zypp/zypp/ZConfig.cc` (lines ~349 and ~1224), the CMake status message in the top-level `CMakeLists.txt`, `zypp_configure_apiconfig()` in `cmake/modules/ZyppCommon.cmake` (`set(LIBZYPP_CODESTREAM "${CODESTREAM}")`, called from `zypp/zypp-core/CMakeLists.txt`) and `APIConfig.h.in`. Without the spec, e.g. in a developer build, it is an empty string and the log reads `libzypp: 17.38.16 ()`.

### 4.3 Builtin-default options (`zypp/CMakeLists.txt`)

`LIBZYPP_CONFIG_USE_DELTARPM_BY_DEFAULT`, `..._CLASSIC_RPMTRANS_BY_DEFAULT`, `..._LEGACY_CURL_BACKEND_BY_DEFAULT`, `..._SERIAL_PACKAGE_DOWNLOAD_BY_DEFAULT` (all default OFF) become `#cmakedefine01` macros in `APIConfig.h`. They change only the *builtin default*; `zypp.conf`/env vars can still override (see [Configuration](09-configuration-and-environment.md)).

### 4.4 Dependencies & feature Provides

- `BuildRequires: %{libsolv_devel_package} >= 0.7.40` (raised for bsc#1279541, SLES 16.1 migrations), `libsolv-tools-base >= 0.7.29` with `%requires_eq libsolv-tools-base` (older: `libsolv-tools`). `%requires_eq` pins the runtime repo2solv tools to the exact libsolv we were built (statically) against — keep that when touching it (see 17.34.x "Workaround broken libsolv-tools-base requirements").
- `cmake >= 3.17`, `gcc-c++ >= 7`, libcurl `>= 7.19.4` and `( libcurl4 without libcurl-mini4 )`, `rpm-devel > 4.4`, asciidoctor (≥1330) or asciidoc+xslt, doxygen/graphviz/texlive for docs, `libproxy-devel` (only for a test; libproxy is dlopened at runtime; `Requires: libproxy1` kept only for ≤ SLE 15.5).
- `Conflicts: zypper < 1.14.91` — "API refactoring. Prevent zypper from using (now) private symbols" (the visibility switch). `Conflicts: python[23]-zypp-plugin < 0.6.4` (bsc#1227793, stomp headers with `-`).
- `Provides: yast2-packagemanager`/`Obsoletes` (ancient rename).
- Feature provides — **must be kept in sync with `zypp/doc/autoinclude/FeatureTest.doc`** (comment in spec says so):

```
libzypp(plugin) = 0.1          libzypp(plugin:appdata) = 0   libzypp(plugin:commit) = 1
libzypp(plugin:services) = 1   libzypp(plugin:system) = 1    libzypp(plugin:urlresolver) = 0
libzypp(plugin:repoverification) = 0   libzypp(plugin:sigcheck) = 0
libzypp(repovarexpand) = 1.1   libzypp(econf) = 0
```
  Other packages (zypper, plugins, SUSEConnect…) `Requires:` these to detect capabilities; bump them when a feature's semantics change.

### 4.5 Subpackages and files

- `libzypp`: `%{_libdir}/libzypp*so.*`, `%{_libexecdir}/zypp/zypp-rpm`, `%{_prefix}/lib/zypp` (plugins dirs, workers), `%{_datadir}/zypp`, `%{_bindir}/*`, man1/man5, translations (`%find_lang zypp`), `/etc/zypp/*` directories, `needreboot`, `systemCheck`, logrotate `zypp-history.lr` (moved to `%{_distconfdir}` when defined).
- `libzypp-devel`: headers `zypp`, `zypp-core`, `zypp-media`, `zypp-curl`, `zypp-tui`, `zypp-common`; `libzypp.so`; **static `libzypp-tui.a`** (`Provides: libzypp-tui-devel = 1`); `cmake/Zypp` (`ZyppConfig.cmake` + `ZyppCommon.cmake`, which zypper/YaST use); `pkgconfig/libzypp.pc`. It `%requires_ge %{libsolv_devel_package}` because headers expose libsolv.
- `libzypp-devel-doc`: doxygen HTML in `%{_docdir}/libzypp`.
- `%pre`/`%posttrans` save/restore `.rpmsave` for `/etc/zypp/zypp.conf` and the logrotate file during the `/usr/etc` migration.
- `%check` runs `ctest --output-on-failure` — OBS builds run the full test suite (incl. media backend tests with nginx/squid/vsftpd unless disabled).

### 4.6 rpmlint and pkg-config

- `zypp/package/libzypp-rpmlint.cmake` contains only `addFilter("libzypp.* shlib-policy-name-error")`: by shlib policy the library package would have to be named `libzypp1735`; SUSE keeps the name `libzypp`, so the warning is filtered.
- `zypp/libzypp.pc.cmake`: `Libs: -L${libdir} -lzypp`, `Cflags: -I${includedir} -D_FILE_OFFSET_BITS=64 @ZYPP_CFLAGS@`, `features=@ZYPP_FEATURES@`. `ZYPP_CFLAGS`/`ZYPP_FEATURES` are not set anywhere in the current CMake files. Re-verified: a grep over all `CMakeLists.txt`/`*.cmake`/`*.in` finds them only in `libzypp.pc.cmake` itself. So they expand empty and the installed `.pc` has a trailing blank `Cflags` and an empty `features=` line. `-D_FILE_OFFSET_BITS=64` is part of the ABI contract (struct stat / off_t in headers) — consumers must compile with it.

### 4.7 `zypp/Makefile.cvs`

Generic helper from Stefan Hundhammer: `make -f Makefile.cvs` creates `build/`, runs `cmake -DCMAKE_INSTALL_PREFIX=/usr ..` and writes a toplevel `Makefile` that delegates to `build/`. Legacy convenience; not used by the spec or CI.

---

## 5. Symbol visibility and export control

Two layers decide what libzypp.so exports:

```
compile:  -fvisibility=hidden   (ENABLE_VISIBILITY_HIDDEN, default ON, top-level CMakeLists.txt)
          -> only declarations marked ZYPP_API (visibility default) are exported
link:     -Wl,--version-script=zypp/zypp/libzypp.map
          -> of those, only C++ symbols in namespace zypp:: (+ vtables/typeinfo/guard vars)
             are global, version node ZYPP_plain; everything else local
```

`zypp/zypp/libzypp.map`:

```
ZYPP_plain {
  global: extern "C++" { zypp::*; vtable?for?zypp::*; typeinfo?for?zypp::*;
                         typeinfo?name?for?zypp::*; guard?variable?for?zypp::*; };
  local: *;
};
```

There is a single version node; symbols are not individually versioned — ABI compatibility is managed only via the soname. The map was introduced in 11.6.0 ("Use ld version script to hide symbols outside zypp namespace (bnc#755324)"). Consequence: anything in `zyppng::` or global namespace is never exported from libzypp.so.

Macros, `zypp-logic/zypp-core/Globals.h`:

```cpp
#ifdef ZYPP_DLL          // set via ADD_DEFINITIONS(-DZYPP_DLL) in zypp/zypp/CMakeLists.txt etc.
  #define ZYPP_API   ZYPP_DECL_EXPORT   // __attribute__((visibility("default")))
  #define ZYPP_TESTS ZYPP_DECL_EXPORT
  #define ZYPP_LOCAL ZYPP_DECL_HIDDEN
#else
  #define ZYPP_API   ZYPP_DECL_IMPORT   // also visibility default
  #define ZYPP_TESTS ZYPP_DECL_IMPORT
  #define ZYPP_LOCAL                    // empty
#endif
#define ZYPP_API_DEPTESTOMATIC ZYPP_API  // internal symbols offered to deptestomatic
```

History (jsc#PED-8153, 17.33.0): "Add ZYPP_API for exported functions and switch to visibility=hidden". It was followed by a series of fixes for symbols that consumers still needed: "add one more missing export for PackageKit" (17.33.2), "... for libyui-qt-pkg" (17.33.3), "Keep UrlResolverPlugin API public (fixes #560)", "Export RepoVariablesStringReplacer for yast2 (bsc#1228138)", "Export asSolvable / CredentialManager for legacy YAST versions (bsc#1228420)", "Export TestcaseTrial for deptestomatic", "Remove Symbols exported with ZYPP_TESTS". The 17.38.15 entry notes the spec `%bcond visibility_hidden` was removed because it is always ON in CMake.

Tests do not link the stripped shared lib for internals: `zypp/zypp/CMakeLists.txt` builds a second library **`zypp-allsym` (STATIC)** from the same objects ("library with more symbols for tests etc"). So a test passing does **not** prove a symbol is exported — check with `nm -DC`.

### What to do when adding public API (legacy `zypp/`)

Per `AGENTS.md`, `zypp/` is frozen (no new features), but bug fixes sometimes need new API for YaST/zypper:
1. Declare in a public header under `namespace zypp`, mark the class or free function `ZYPP_API` (class-level `class ZYPP_API Foo` exports all members; use `ZYPP_LOCAL` for private members that must not leak).
2. Header must stay C++11/C++17-compatible for consumers (see `ZYPP_DEFINE_GLOBAL_CONSTANT` macros in `Globals.h` for namespace-scope constants).
3. Add the header to the CMake install list (otherwise `-devel` lacks it).
4. Only additions → bump MINOR, PATCH=0, COMPATMINOR unchanged (mkChangelog `M`).
5. Verify with `nm -DC --defined-only build/zypp/zypp/libzypp.so | grep YourSymbol`.

---

## 6. `LEGACY()` guards

`zypp-logic/zypp-core/Globals.h`:

```cpp
/**
 * Legacy code we still support.
 * Deprecated items we can't drop immediately because YAST/PK still
 * refer to them or they break binary compatibility, should be
 * enclosed in `#if LEGACY(#)` where # is either a major number(<=99),
 * a soversion [<=9999] or numversion [<=999999].
 */
#if defined(ZYPPNG) && ZYPPNG
#  define LEGACY(CL) 0            // NG never compiles legacy-compat code
#else
#  define LEGACY(CL) ( CL < 100 && LIBZYPP_VERSION_MAJOR <= CL ) \
                  || ( CL < 10000 && LIBZYPP_SOVERSION <= CL ) || LIBZYPP_VERSION <= CL
#endif
```

Meaning: code inside `#if LEGACY(1735)` is compiled as long as the soname is ≤ 1735, i.e. it **disappears automatically on the next soname bump**. `LEGACY(17)` lives until MAJOR 18. A numversion (e.g. `LEGACY(173816)`) would drop it at the next release after that version — none currently used.

Current inventory (grep, 18 hits):

| Guard | Location | What |
|---|---|---|
| `LEGACY(17)` | `zypp/zypp/ZYppCallbacks.h` | `ConvertDBReport` — "Still used by y2-bindings in Code15 (libzypp major 17)" |
| `LEGACY(1735)` | `zypp/zypp/ZConfig.h/.cc` | `update_dataPath/scriptsPath/messagesPath` are `const` members; non-legacy they become `static` (the `.cc` toggles `const`) |
| | `zypp/zypp/RepoInfo.h/.cc` | deprecated `setMirrorListUrl` (capital L), `setMirrorListUrls`, `setMetalinkUrls` |
| | `zypp/zypp/target/rpm/librpmDb.h/.cc` | `db_const_iterator(librpmDb::constPtr)` ctor kept mapped to `"/"` "until we need to raise our SOVERSION" |
| | `zypp/zypp/Pattern.h/.cc` | deprecated `Pattern::ContentsSet` helper |
| | `zypp/zypp/sat/SolvIterMixin.h/.cc` | old `UnifyByIdent` (flawed, kept for binary compat; recompiled code uses `UnifyByIdent2`) |
| `( LEGACY(1735) ) && !ZYPPNG` | `zypp-logic/zypp/IdString.h`, `IdStringType.h` | `IdString`/`IdStringType` still inherit `protected sat::detail::PoolMember` — a **class layout/ABI** detail |

Mechanics and cleanup on a soname bump:
1. Bump COMPATMINOR (mkChangelog `I`). All `LEGACY(<=old soname)` blocks compile out.
2. Build; fix compile errors (callers of removed API inside the tree).
3. **Delete** the dead `#if LEGACY(...)` blocks and the now-unneeded `#else` branches (keep the `#else` code). Don't leave dead guards around.
4. Check zypper, yast2-pkg-bindings, libyui-*-pkg, PackageKit still build against the new headers (`LEGACY(17)` notes YaST still uses `ConvertDBReport` on Code15!).
5. Mention the soname change in the changes file; downstream packages must be rebuilt in OBS (soname change → rebuild of all dependents).

Writing new guards: when you must change an exported signature without bumping the soname, keep the old symbol under `#if LEGACY(<current SO_FIRST>)` and mark it `ZYPP_DEPRECATED`, as done for `librpmDb::db_const_iterator`.

---

## 7. Deprecation and compat headers

### 7.1 `ZYPP_DEPRECATED`

Defined in `Globals.h` as `__attribute__((deprecated))` **only when `NDEBUG` is not defined**; in Release builds (the spec uses `-DCMAKE_BUILD_TYPE=Release`) it expands to nothing. Placement rules from the header comment: after the declaration for non-inline functions (`int f() const ZYPP_DEPRECATED;`), before the return type for inline functions, after `class`/`struct` keyword for types. Deprecation does not remove the symbol; pair it with a `LEGACY()` guard if it should go away at the next soname bump.

### 7.2 `zypp/zypp-compat/`

When core classes moved from `zypp/zypp/...` into `zypp-core`, `zypp-media`, etc. (now physically in `zypp-logic/`), consumers' `#include <zypp/Url.h>` style paths had to keep working. `zypp/zypp-compat/zypp/**` contains one-line forwarding headers, e.g. `zypp/Url.h` → `#include <zypp-core/Url.h>`, `zypp/base/Logger.h` → `<zypp-core/base/Logger.h>`, `zypp/APIConfig.h` → `<zypp-core/APIConfig.h>`. `zypp/zypp-compat/CMakeLists.txt` installs them into `${INCLUDE_INSTALL_DIR}/zypp/{,base,url,media,media/proxyinfo,parser,parser/xml}` (ZckStream.h only with zchunk; ProxyInfoLibproxy.h only if libproxy found). Changelog: "Install zypp/APIConfig.h legacy include (fixes #557)" in 17.35.1 — a missing compat header broke consumers right after a release. **Never delete a compat header** without checking zypper/YaST/PackageKit sources.

---

## 8. Translations and docs

### 8.1 Translations (`po/`)

- Text domain **`zypp`** (`-DTEXTDOMAIN="zypp"`, `LOCALEDIR=${prefix}/share/locale`); `zypp-logic/zypp-core/base/Gettext.cc` calls `bindtextdomain`/`dgettext(TEXTDOMAIN, ...)`. zypp-tui code is compiled into zypper but must use the `zypp` domain (17.34.1: "zypp-tui: Make sure translated texts use the correct textdomain (fixes #551)").
- `po/CMakeLists.txt`: xgettext over `zypp/zypp`, `zypp-common`, `zypp-tui`, `shared`, `zypp-logic`, `zyppng` with keywords `_ , _:1,2, __, N_, PL_:1,2`; msgmerge each `*.po` against the generated pot, msgfmt to `.gmo`, installed as `share/locale/<lang>/LC_MESSAGES/zypp.mo`.
- Targets: `translations` (ALL; prints a diff of the committed `po/zypp.pot` vs generated via `PotfileDiff.sh`) and `potfile_update` (runs `po/PotfileUpadte.sh` — sic — which requires branch `master`, a clean index, copies `zypp.pot` + `*.po` from the build dir and commits `Translation: updated .pot file`). It does this only if `PotfileDiff.sh -q` reports a difference. Details in §15.
- Translations are maintained in **Weblate** (changelog ~2016: "initial .po file set derived from tarballs. Translations are now maintained via Weblate"); Weblate commits `Translated using Weblate (<lang>)` land directly on master. Update the pot before a release that adds/changes strings so translators see them. Historic: "Use a uniform translation set for SLE and openSUSE (bnc#948924)"; "Protect code against broken translations (bsc#1082711)" — format strings from translations must be treated as untrusted.

### 8.2 Docs

- `zypp/doc/autodoc/`: `Doxyfile.cmake` configured by CMake; inputs are `zypp/zypp`, `zypp-common`, `zypp-tui`, `shared`, and `zypp-logic/{zypp,zypp-common,zypp-core,zypp-curl,zypp-media}` plus pages in `zypp/doc/autoinclude/*.doc` (FeatureTest, Plugins, EnvironmentVariables, RepoVariables, CodePitfalls, Testcases, …). Built when `ENABLE_BUILD_DOCS` (forced ON for `-DSUSE`), else EXCLUDE_FROM_ALL; `DISABLE_AUTODOCS` drops the doxygen requirement. Published to GitHub Pages by `.github/workflows/build-and-publish-doc.yml`.
- Man pages: `zypp/doc/*.txt` (asciidoc) → `ADD_A2X_MANPAGES` uses `asciidoctor -b manpage` (preferred if found) or `a2x -f manpage`; configure FATAL_ERRORs if neither exists. Current pages: `zypp.conf.5`, `locks.5`, `zypp-NameReqPrv.1`. Verified in `zypp/doc/CMakeLists.txt`: `zypp-CheckAccessDeleted.1` is **not** listed, so `zypp/doc/zypp-CheckAccessDeleted.1.txt` is a dead source. The tool itself (`zypp/tools/zypp-CheckAccessDeleted.cc`) is still *compiled* because `zypp/tools/CMakeLists.txt` globs `*.cc`, but only `zypp-NameReqPrv` is installed. The changelog entry was "Drop zypp-CheckAccessDeleted in favor of 'zypper ps'". See §16.

---

## 9. Branches, tags and codestreams

```
origin = github.com:openSUSE/libzypp   (canonical)
downstream = github.com:bzeller/libzypp (personal fork in this checkout)

master ───────────── 17.x  →  Tumbleweed, SLE/Leap 16.x, SLE/Leap 15 SPx (all from master,
                               behaviour differences via spec conditionals, §4.1)
SuSE-SLE-12-SP2-Branch ─ 16.22.x (COMPATMINOR 0)  →  SLE 12 (LTSS); still receives
                               security fixes (16.22.17–19 in 2026)
SuSE-SLE-11-SP3-Branch ─ 9.41.x (COMPATMINOR 8) → SLE 11 SP3 (LTSS); last release
                               9.41.5 on 2024-12-18, so treat it as "dormant", not dead (§18)
SuSE-Code-12_x / SuSE-Code-13_1 / other SuSE-SLE-1[01]-* / SuSE-Linux-* / SuSE-RES-* → dead
BASE-<branch> tags mark branch points.
release-17.33.3 → one-off branch from the 17.33/17.34 visibility turmoil.
ma-*, bsc-*, PTF-* branches → feature/bugfix/PTF work branches.
```

Tags are plain version numbers (`17.38.16`, `16.22.19`), annotated, message `tagging X.Y.Z`. On the 16.22 branch the layout is pre-split (files at `VERSION.cmake`, `package/libzypp.changes` at the repo root), so cherry-picks need path adjustment and `mkChangelog` there is that branch's own copy.

Backport policy evidenced by history: security/CVE fixes go to both master and SLE‑12‑SP2 branch, each with its own PATCH release. There are no SLE15 git branches. The OBS `zypp:SLE-15-SP*-Branch` projects are patch-less `_link`s to `zypp:Head` (§13), so SLE15 maintenance updates ship current 17.x master releases — so **every master release must remain ABI compatible and behaviour-safe for SLE 15** (C++ standard, compiler `gcc-c++ >= 7` BR, libsolv/curl minimums, `suse_version < 1600` defaults).

---

## 10. `libzypp-glib` (zyppng) versioning

- Separate package/library, spec `zyppng/packaging/libzypp-glib.spec`, built with clang19/lld19/ninja, CMake ≥ 3.28, targets TW and Leap/SLES 16+. Standalone: does not link legacy `libzypp.so`, bundles zyppng statically (changes: initial release 0.1.0, May 2026). PR CI via `.obs/workflows.yml` (`zypp:ci:libzypp-glib`).
- Single source of truth: `zyppng/packaging/libzypp-glib-version.inc` (`%define libzypp_glib_{major,minor,patch,compatminor}`, `# LAST RELEASED: 0.1.0 (0)`), `%include`d by the spec and regex-parsed by `zyppng/api/VERSION.cmake` ("we never write it"). Same rules and soname math as libzypp → `libzypp-glib.so.<SO_FIRST>.<AGE>.<PATCH>`; the spec recomputes `so_first = major*100 + compatminor` itself for `%files` — keep both formulas consistent.
- "Pre-1.0: API is unstable; any release may break ABI." No mkChangelog equivalent; update the `.inc` and `libzypp-glib.changes` by hand.
- Inside the legacy build, `BUILD_LIBZYPPNG` is forced off for `suse_version < 1600`; zyppng's `zyppng/lib` target reuses `LIBZYPP_VERSION_INFO` for its soname.

---

## 11. How to do a release (checklist)

Derived from `VERSION.cmake`, `mkChangelog` and the git history.

1. `git checkout master && git pull` (mkChangelog refuses non-fast-forward).
2. Make sure CI/OBS (`zypp:ci:*` for the last PRs) is green; locally `ctest` in a full build (`%check` will run it in OBS anyway).
3. If user-visible strings changed: build `potfile_update` target (on master, clean index) → commits `Translation: updated .pot file`.
4. Decide the bump:
   - bug fixes only → PATCH (`P`)
   - new API / new feature / new zypp.conf option → MINOR (`M`)
   - removed/changed exported API or class layout → incompatible (`I`, COMPATMINOR = MINOR) → soname bump; clean up `LEGACY()` blocks (§6); coordinate rebuilds of zypper/YaST/PackageKit.
   - If a feature `Provides: libzypp(...)` changed: update spec and `FeatureTest.doc`.
   - If minimum deps changed (libsolv etc.): update the spec BRs, add a changes bullet with bug ref.
5. `cd zypp && ./mkChangelog -n` (dry run) to preview; then `./mkChangelog`, choose bump, edit the entry in vi: one bullet per user-visible change with `(bsc#…)`/`(CVE-…)`/`(fixes #…)`, drop noise, ensure last line `- version X.Y.Z (C)`.
6. Choose `(s)ubmit` → commit `changes X.Y.Z (C)` + tag `X.Y.Z` ("tagging X.Y.Z"), `# LAST RELEASED:` updated.
7. `git push --tags origin HEAD` (push both commit and tag).
8. Create tarball: in the build dir `make srcpackage` (requires clean tree) → `package/libzypp-X.Y.Z.tar.bz2`, `libzypp.spec`, `libzypp-rpmlintrc`, `libzypp.changes`.
9. Check that the automatic upload to `zypp:Head/libzypp` happened (OBS revision comment `gitrev X.Y.Z (changes X.Y.Z (C), date)`) and that it builds. Then `osc sr zypp:Head libzypp openSUSE:Factory`. For SLE: IBS maintenance submission (not publicly documented, ask the team). See §13.
10. For security fixes: repeat on `SuSE-SLE-12-SP2-Branch` (16.22.x) if affected.
11. For libzypp-glib: bump `zyppng/packaging/libzypp-glib-version.inc` + `libzypp-glib.changes` manually.

## 12. Reviewing a PR for ABI safety (checklist)

- [ ] Does it touch an installed header (`zypp/zypp/**`, `zypp-logic/**` non-ng public headers, `zypp/zypp-compat/**`, `zypp-tui`)? If no, ABI is likely safe.
- [ ] No change to size/layout of exported classes: no added/removed/reordered data members (use the pimpl `_pimpl`/`RW_pointer` pattern for new state), no changed base classes (cf. `IdString : PoolMember` guarded by LEGACY).
- [ ] No added/removed/reordered **virtual** functions in exported classes (callbacks `ZYppCallbacks.h` reports are subclassed by zypper/YaST!); no changed default arguments semantics relied upon by old binaries.
- [ ] No changed signature of an exported function (constness, parameter types, return type, `static`) — otherwise keep old symbol under `#if LEGACY(<SO_FIRST>)` + `ZYPP_DEPRECATED`.
- [ ] Inline/template code in headers: behaviour change is baked into consumers at compile time; old binaries keep old inline code. Enum values: only append, never renumber.
- [ ] New API is `ZYPP_API`-marked, in `namespace zypp` (version script!), header installed via CMake; verify `nm -DC` on `libzypp.so` (not `zypp-allsym`).
- [ ] No removal of `ZYPP_API` from anything YaST/PackageKit/zypper/deptestomatic might use (grep their sources; history of bsc#1228138, bsc#1228420).
- [ ] No removal/rename of compat forwarding headers.
- [ ] Public headers stay C++11-consumable (YaST bindings, older compilers on SLE15); no C++20 in installed headers.
- [ ] libsolv types/struct usage in headers not changed (libsolv is static; layout leaks).
- [ ] Changelog bullet with bug ref present (or will be generated from commit message: check subject line).
- [ ] Version bump category correct (§11 step 4); never COMPATMINOR > MINOR.
- [ ] SLE15 build paths still work (`suse_version < 1600` defaults: classic rpmtrans, legacy curl, serial download).

---

## 13. OBS projects and the submission pipeline

Everything in this section was verified through the **public OBS API** (`https://api.opensuse.org/public/source/<project>/<package>[/_meta|/_history|/<file>]`, no login needed; the web UI returns 403 to scripts) in Oct 2026. The repository itself contains none of it except `.obs/workflows.yml`.

### 13.1 Project map

```
GitHub openSUSE/libzypp
  │  master push (incl. non-release commits)
  │  ── automatic upload by OBS user "zypp-team" (uploader lives outside this repo; unverified how)
  ▼
zypp:Head/libzypp           "ZYpp HEAD branch USE AT YOUR OWN RISK"
  files: libzypp-X.Y.Z.tar.bz2, libzypp.spec (configured), libzypp.changes,
         libzypp-rpmlintrc, _constraints (25G disk, 5000M RAM; exists only in OBS)
  repos: openSUSE_Tumbleweed, openSUSE_Factory, openSUSE_Factory_Staging_L,
         openSUSE_Leap_16.0, openSUSE_Leap_16.1, openSUSE_Leap_15.6 (on :Update), all x86_64
  │                                   ▲
  │  osc sr (by the maintainer)       │ _link (no patches)
  ▼                                   │
openSUSE:Factory/libzypp     zypp:SLE-15-Branch ◄─ SP1 ◄─ SP2 ◄─ … ◄─ SP6 ◄─ zypp:SLE-15-SP7-Branch
  _meta: <devel project="zypp:Head" package="libzypp"/>         (each _link → previous SP)

zypp:SLE-12-SP2-Branch/libzypp   plain package, 16.22.19 tarball   (fed from SuSE-SLE-12-SP2-Branch)
zypp:SLE-11-SP3-Branch/libzypp   9.41.5   ;  zypp:SLE-11-SP4-Branch 9.37.10 ; zypp:SLE-12-Branch 14.48.5 (dead)

zypp:ci:libzypp/libzypp            _service (tar_scm)  ── PR CI, see 13.3
zypp:ci:libzypp-glib/libzypp-glib  _service (obs_scm)
```

The `zypp:Head` project description says: *"Submissions to the git repositories are automatically built, tested, and if they pass the testsuite, uploaded here. Any changes applied here, but not in git, are volatile. That's why no submit requests for this project will be accepted."* It also says *"The zypp:SLE-15-SP{N}-Branch sibling projects of zypp:Head provide the builds for SLE-15-SP{N} / Leap15.{N} maintenance"*. The description still mentions gitorious, so it is outdated, but the mechanism is still running.

Other packages in `zypp:Head`: `libsolv`, `zypper`, `zypp-plugin`, `libzypp-testsuite-tools`, `mcp-server-zypp`. `libzypp-glib` is **not** in `zypp:Head` yet (Oct 2026). It only exists in `zypp:ci:libzypp-glib`.

Consequences:
- **Never `osc commit` into `zypp:Head/libzypp` by hand.** The next automatic upload overwrites it. Fix things in git.
- `zypp:Head` tracks **master HEAD, not just tags**. `_history` rev 3079/3080 have comments `gitrev 17.38.16-1-gb827956a9 (Remove outdated comment (fixes #751), 2026-10-13)` and `gitrev 17.38.16-3-g32d4383bf (...)`. The RPM version stays `17.38.16` (taken from `VERSION.cmake`) while the content moves on. So a `zypp:Head` binary with version X.Y.Z may contain post-X.Y.Z commits. Check the OBS revision comment (`git describe` output) before you assume what is in it.
- **SLE 15 SPx maintenance builds the identical sources** (`?expand=1` on `zypp:SLE-15-SP7-Branch/libzypp` shows `libzypp-17.38.16.tar.bz2` with the same md5 as `zypp:Head`). The `_link`s carry no patches. Codestream differences come only from the spec conditionals (§4.1). There is no SLE-16 branch project (`zypp:SLE-16-Branch` → `unknown_project`). SLE 16 / Leap 16 are built directly in `zypp:Head` repos.

### 13.2 Factory submission (verified example)

`openSUSE:Factory/libzypp/_history` shows request **1379503** for 17.38.16:

| Step | Who / when |
|---|---|
| tag `17.38.16` pushed | Michael Andres, 2026-09-21 17:26 CEST |
| `zypp:Head` auto-upload, comment `gitrev 17.38.16 (changes 17.38.16 (35), 2026-09-21)` | `zypp-team` |
| SR `zypp:Head/libzypp` → `openSUSE:Factory` | creator `mlandres`, 2026-09-21 15:53 UTC (≈30 min after tag) |
| reviews `factory-auto` (check script), `licensedigger` | automatic, minutes later |
| staging `openSUSE:Factory:Staging:E` accepted by `anag_factory`, comment "Softwarestack update" | 2026-09-23 |

Not every release goes to Factory: the Factory `_history` version list jumps from 17.38.11 to 17.38.13, so **17.38.12 was never submitted**. Factory has 538 revisions of libzypp in total.

Practical commands (standard `osc`, not repo-specific):

```sh
osc api /source/zypp:Head/libzypp/_history | tail     # did the auto-upload happen?
osc results zypp:Head libzypp                          # build status per repo
osc sr zypp:Head libzypp openSUSE:Factory -m "libzypp X.Y.Z"
osc rq list -P openSUSE:Factory -p libzypp              # follow staging
```

### 13.3 PR CI via OBS SCM/CI (`.obs/workflows.yml`)

```yaml
ci_workflow:
  steps:
    - branch_package: { source_project: zypp:ci:libzypp,      source_package: libzypp,      target_project: home:zypp-team }
    - branch_package: { source_project: zypp:ci:libzypp-glib, source_package: libzypp-glib, target_project: home:zypp-team }
  filters: { event: pull_request }
#master_workflow: ... event: push, branches: only: master     (commented out)
```

Step by step:
1. GitHub sends a `pull_request` event to OBS through the workflow token configured on `home:zypp-team` (the token is OBS-side config and not visible in the repo).
2. Each `branch_package` step branches the CI template package into a per-PR project under `home:zypp-team`. OBS SCM/CI rewrites the branched `_service` so that it fetches the PR's head commit. That is standard OBS behaviour; the exact project naming was not verified.
3. The template `_service`s run:
   - **`zypp:ci:libzypp/libzypp`**: `tar_scm` (git, `https://github.com/openSUSE/libzypp.git`, `versionprefix 18.0.0.git`, filename `libzypp`) → `extract_file */zypp/libzypp.spec.cmake` → `libzypp.spec` → `set_version` (replaces the unconfigured `Version: @VERSION@`) → `extract_file */zypp/package/libzypp.changes` → `extract_file */zypp/package/libzypp-rpmlint.cmake` → `libzypp-rpmlintrc` → buildtime `recompress` (bz2). CI RPMs are therefore versioned `18.0.0.git<date>.<hash>`. That is higher than any 17.x, so do not publish them where real systems can pick them up.
   - **`zypp:ci:libzypp-glib/libzypp-glib`**: `obs_scm` (`changesgenerate enable`, `changesauthor Benjamin Zeller`) → `extract_file` of `zyppng/packaging/libzypp-glib.spec`, `libzypp-glib-version.inc`, `libzypp-glib.changes` → buildtime `cpio_repack` (reads Name/Version through `rpmspec`, so the `%include` of the version `.inc` works; needs `BuildRequires: obs-service-cpio-repack` in the spec). Its comment says: "To trigger a new CI build: `osc service run && osc commit`".
4. OBS builds and runs `%check` (full `ctest`) and reports the status back to the PR.

Because the CI spec is the **raw** `libzypp.spec.cmake` (only `set_version` substitution), any new `@VAR@` you add to the spec template must also work, or be harmless, when it is not substituted. Otherwise PR CI breaks while the release tarball (CMake-configured spec) builds fine. The `master_workflow` was disabled on 2025-08-08 (`origin/doc-to-pages` tip: "fixup! Disable obs master workflow for now").

### 13.4 SLE maintenance (IBS)

What is public: SLE 15 SPx / Leap 15.x libzypp updates are built from the `zypp:SLE-15-SP{N}-Branch` link chain (project description). Leap 15.6 also builds directly in `zypp:Head` against `openSUSE:Leap:15.6:Update`. The changes file must carry `bsc#`/`CVE-`/`jsc#` references, because the maintenance tooling extracts them (§3.1). **Unverified / internal:** how the SUSE IBS maintenance request is created (presumably `osc -A <ibs> mr` from the corresponding `zypp:SLE-15-SPx-Branch` or an internal mirror), who approves it, and how SLE 16 maintenance is fed. Ask the team. Do not guess project names in IBS.

---

## 14. Anatomy of a real release (git evidence)

### 14.1 Commit sequence

From `git log --oneline origin/master` (Sep 2026):

```
a8aac2497 changes 17.38.16 (35)                       <- release commit (tag 17.38.16)
8ae9cdefc BuildRequires:  %{libsolv_devel_package} >= 0.7.40 (bsc#1279541)
3672511e1 Translated using Weblate (Ukrainian)        <- filtered by mkChangelog
23fd0361e Translated using Weblate (Ukrainian)
733a805b8 Translated using Weblate (Ukrainian)
fa8c09cdb changes 17.38.15 (35)                       <- previous release
3b1270785 Use private temp homedirs for volatile key contexts
...
1d3e742fd Translated using Weblate (Lao)
832191084 Added translation using Weblate (Lao)       <- NOT filtered (see §15), dropped by hand
bacd92ed1 repoGpgCheck: Strictly follow the repo_gpgcheck setting (bsc#1274625)
...
14f81f090 changes 17.38.14 (35)
```

`git show --stat 17.38.16`:

```
tag 17.38.16
Tagger: Michael Andres <ma@suse.de>
    tagging 17.38.16
commit a8aac2497...   changes 17.38.16 (35)
 zypp/VERSION.cmake           | 4 ++--
 zypp/package/libzypp.changes | 7 +++++++
```

A MINOR release looks the same. `17.38.0` changes `VERSION.cmake` by 6 lines (`MINOR 37→38`, `PATCH 18→0`, `LAST RELEASED`) and adds 19 lines to the changes file, covering the 36 non-merge commits since 17.37.18. On the SLE‑12 branch the release commit touches `VERSION.cmake` and `package/libzypp.changes` at the **repo root** (`16.22.19`).

The release commit **never** touches the spec. Spec changes (e.g. the libsolv BR bump above) are ordinary commits *before* the release commit, and their subject line becomes the changes bullet.

### 14.2 Cadence (tag dates, `git log -1 --format=%cs <tag>`)

| Period | Releases | Notes |
|---|---|---|
| 2023 | 20 `changes 17.*` commits | |
| 2024 | 36 | visibility/soname turmoil 17.33–17.35 |
| 2025 | 30 | 17.36.0 (2025-02-10), 17.37.0 (2025-05-22), 17.37.18 (2025-09-23) |
| 2026 (to Oct) | 17 | 17.38.0 (2026-01-11) … 17.38.16 (2026-09-21) |

- A typical gap is 1–3 weeks. A ~3.5 month gap (17.37.18 → 17.38.0) coincided with the master restructuring (split into `zypp/`, `zypp-logic/`, `zyppng/`).
- Bursts happen around security fixes. 17.38.10 + 16.22.17 (2026-05-27), 17.38.11 + 16.22.18 (05-29) and 17.38.13 + 16.22.19 (06-08, CVE-2026-44942) were released **the same day on both branches**.
- Authors since 2024: Michael Andres 80 release commits, Benjamin Zeller 3.

### 14.3 What to look at when reconstructing "what is in version X"

```sh
git log --oneline --no-merges 17.38.15..17.38.16          # raw content
git show 17.38.16:zypp/package/libzypp.changes | sed -n '1,/^-----/p' # curated text
osc api /source/zypp:Head/libzypp/_history | grep gitrev  # what OBS actually built
```

---

## 15. Translation workflow in detail

```
 developer adds _("...") in code
        │
        ▼  build target `translations` (ALL when ENABLE_BUILD_TRANS; forced ON for -DSUSE / -DDEBIAN,
        │                                OFF with NOSCHNICKSCHNACK)
 xgettext → <build>/po/zypp.pot ; for each po/*.po: msgmerge --no-fuzzy-matching → <build>/po/<lang>.po
        │   msgfmt → <lang>.gmo ; prints diff of committed po/zypp.pot vs generated (PotfileDiff.sh)
        ▼  build target `potfile_update` (maintainer, on master)
 PotfileDiff.sh -q  ── no diff ──► nothing happens
        │ diff
        ▼ po/PotfileUpadte.sh <builddir>: must be on master, cwd po/, index clean
 copies zypp.pot AND all msgmerged *.po from the build dir into po/, git add, commit
 "Translation: updated .pot file"           (author: the maintainer; ~every 3–4 months:
                                              2025-02-28, 05-27, 09-18, 2026-01-11, 05-12)
        │ push
        ▼
 openSUSE Weblate (committer address weblate-noreply@opensuse.org; project URL not verified) picks up zypp.pot; translators work there
        │
        ▼ Weblate pushes commits directly to master:
   author = translator, committer = "openSUSE Weblate <weblate-noreply@opensuse.org>"
   subjects: "Translated using Weblate (<Language>)", "Added translation using Weblate (<Language>)"
```

Facts and frequencies (`origin/master`):
- 65 `.po` files plus `zypp.pot` in `po/`.
- Weblate commits per month in 2025/2026 range from 1 to ~16. July 2025 had a spike of 60. The most active languages since 2025 are Danish, Ukrainian, Spanish, German, then Japanese, French, Dutch and Catalan.
- `zypp/mkChangelog` filters with `awk '/^@@/{p=1}/^@@Translated using Weblate/{p=0}(p){print}'`. Only **"Translated using Weblate"** commits are dropped. "Added translation using Weblate (Lao)" (832191084, before 17.38.15) and "Translation: updated .pot file" commits **do** appear in the draft. The former was removed by hand. The latter has historically been kept as a bullet ("- Translation: updated .pot file.", e.g. two entries in `libzypp.changes`).
- `po/CMakeLists.txt` collects `PO_FILES` with `FILE(GLOB ...)` at **configure** time. When Weblate adds a new language, an existing build dir does not know it, and `PotfileUpadte.sh` aborts with "Missing file $BINDIR/$F (touch CMakeLists.txt?)". The fix is to re-run cmake.
- `potfile_update` also commits the msgmerged `.po` files, so a pot update touches all 65 `.po` files. Weblate may then show merge conflicts if translators were active at the same time. Pull right before running it.
- Same for zypper: its own `po/` and Weblate component exist (`zypp:Head/zypper` auto-upload comment shows "Translated using Weblate (German)"). zypp-tui strings live in libzypp's `zypp` domain (§8.1).

---

## 16. Documentation publishing and man pages

### 16.1 Doxygen

- Config: `zypp/doc/autodoc/Doxyfile.cmake` (configured `@ONLY` to `<build>/zypp/doc/autodoc/Doxyfile`), `HTML_OUTPUT = html`. Warnings go to `<build>/zypp/doc/autodoc/Doxywarnings`.
- Targets in `zypp/doc/autodoc/CMakeLists.txt`:
  - `doc` (ALL when `ENABLE_BUILD_DOCS`): its output `html/index.html` **depends only on the Doxyfile**, so after the first build it does not rerun when sources change. Use `doc_forced` to regenerate.
  - Both run `doxygen ... || { echo '*** IGNORE FAILED DOXYGEN - DOC MIGHT BE INCOMPLETE!'; true; }`. A doxygen failure **never fails the build**, so check `Doxywarnings`.
- Install: `html/` → `${DOC_INSTALL_DIR}/libzypp` (the `libzypp-devel-doc` package) and `libzypp.doxytag` with `DESTINATION ${DOC_INSTALL_DIR}/libzypp/libzypp.doxytag`. Because `INSTALL(FILES ... DESTINATION)` takes a *directory*, this installs `.../libzypp/libzypp.doxytag/libzypp.doxytag` (a directory with the same name as the file; looks unintended, harmless).

### 16.2 GitHub Pages (`.github/workflows/build-and-publish-doc.yml`)

```
trigger: push of ANY tag ('*')  |  workflow_dispatch   (pull_request trigger commented out)
job build (ubuntu-latest, container registry.opensuse.org/opensuse/tumbleweed:latest):
   actions/checkout@v6
   zypper in -y <full BR list incl. libsolv-devel-static >= 0.7.34, doxygen, graphviz(-gd), texlive-*, …>
   mkdir build && cd build && cmake .. && make doc       (no -DSUSE; `doc` target built explicitly)
   actions/upload-pages-artifact@v5  path: ./build/zypp/doc/autodoc/html
job deploy: actions/deploy-pages@v5  (environment github-pages, concurrency group "pages")
```

Result: https://opensuse.github.io/libzypp/ (HTTP 200, Oct 2026) always shows the **last tagged** state. Tags on the SLE‑12 branch also trigger it. The workflow checks out the tagged commit, which on `16.22.x` is the old tree layout, so that run fails or publishes old docs (unverified which). History: tag trigger added 2025-09-22 (M. Andres), restructured for the new layout 2025-08-12 (B. Zeller), action versions bumped 2026-04-27. The dependency list is maintained **separately** from the spec (its libsolv minimum, 0.7.34, already lags the spec's 0.7.40). Update it when the docs build needs new packages.

### 16.3 Man pages

- Sources are asciidoc in `zypp/doc/*.txt`. `zypp/doc/CMakeLists.txt` prefers `asciidoctor -b manpage -d manpage` (found later, so it overrides `a2x -f manpage`). If neither is found, configure fails with **FATAL_ERROR**, even when docs are otherwise disabled.
- `ADD_A2X_MANPAGES(MAN_INST_PATH ${MANDIR} MAN_PAGES1 zypp-NameReqPrv.1 MAN_PAGES5 zypp.conf.5 locks.5)` creates one `ALL` target `<page>_Target` per page and installs to `${MANDIR}/man<N>`.
- To add a man page: add `zypp/doc/<name>.<N>.txt`, list it under `MAN_PAGES<N>`, and make sure the spec's `%files` glob (`%{_mandir}/man1/*`, `man5/*`) covers it.
- `zypp-CheckAccessDeleted.1.txt` is orphaned (§8.2). Delete it together with the still-compiled `zypp/tools/zypp-CheckAccessDeleted.cc` if nobody needs the dev tool.

---

## 17. Coordinating with zypper (release coupling)

Details of the API surface are in [Consumers & API surface §9](14-consumers-and-api-surface.md). Release-engineering facts:

- zypper has its own version (`zypper/VERSION.cmake`: 1.14.101), its own `mkChangelog` and `changes 1.14.N` commits. Its OBS package `zypp:Head/zypper` is auto-uploaded the same way (comment `gitrev 1.14.101-24-g…`). It is submitted to Factory separately.
- Build-time coupling: `zypper/zypper.spec.cmake` `BuildRequires: libzypp-devel >= 17.38.0` and `BuildRequires: libzypp-tui-devel >= 1` (provided by `libzypp-devel`, §4.5). Run-time coupling: `%requires_ge libzypp` (≥ the libzypp it was built with).
- The BR is raised when zypper starts using new API. History (`git log -G'libzypp-devel >= 17' zypper.spec.cmake`): 17.35.19 (2025-01-22), 17.36.0 (2025-02-10, same day as the libzypp 17.36.0 tag), 17.36.3, 17.36.4, 17.37.0 (2025-05-22, same day as the tag), 17.37.6, 17.37.12, 17.38.0 (2026-06-12, months after the tag). **Order: tag/submit libzypp first**, then zypper. Otherwise the zypper SR fails in Factory staging, or gets bundled into the same staging ("Softwarestack update").
- Reverse guard: `Conflicts: zypper < 1.14.91` in the libzypp spec (§4.4).
- zypper code can also adapt at compile time with `#if LIBZYPP_VERSION <= 173718` (`zypper/src/info.cc`; `LIBZYPP_VERSION` = NUMVERSION from §2.2) so it can be built against SLE maintenance libzypp too.
- Feature `Provides: libzypp(...)` / `FeatureTest.doc` (§4.4) are the runtime contract for scripts and plugins. zypper has its own `Provides: zypper(auto-agree-with-product-licenses)`, `zypper(oldpackage)`, `zypper(updatestack-only)`, `zypper(purge-kernels)`, `zypper(include-all-archs)`.
- In this zypp-stack checkout both are built together against the uninstalled libzypp (`ZYPP_STACK_BUILD` in `zypper/CMakeLists.txt`), so a stack build does **not** prove that zypper builds against the *released* `libzypp-devel` headers (installed header set, `ZyppConfig.cmake`). Check with an OBS build before you raise the BR.

---

## 18. Maintenance branches (verified ref listing)

`git for-each-ref --sort=-committerdate refs/remotes/origin --format='%(refname:short) %(committerdate:short)'` (Oct 2026), grouped:

| Branch | Last commit | Codestream / meaning | OBS |
|---|---|---|---|
| `master` | 2026-09-21 | 17.x: Tumbleweed, SLE/Leap 16.x, SLE 15 SP1–SP7 / Leap 15.x maintenance | `zypp:Head` + `zypp:SLE-15-SP*-Branch` links |
| `SuSE-SLE-12-SP2-Branch` | 2026-06-08 | 16.22.x (COMPATMINOR 0), SLE 12 SP2+ LTSS; active (16.22.17–19 in 2026) | `zypp:SLE-12-SP2-Branch` (16.22.19) |
| `SuSE-SLE-11-SP3-Branch` | 2024-12-18 | 9.41.x (COMPATMINOR 8), SLE 11 SP3 LTSS; last `changes 9.41.5 (8)` "Url: queryparams without value should not have a trailing '='" | `zypp:SLE-11-SP3-Branch` (9.41.5); `zypp:SLE-11-SP4-Branch` has 9.37.10 |
| `release-17.33.3` | 2024-05-08 | one-off `changes 17.33.3 (34)` | — |
| `SuSE-SLE-12-SP1-Branch` | 2020-11-27 | dead | |
| `SuSE-SLE-12-Branch` | 2020-07-16 | dead (14.48.x) | `zypp:SLE-12-Branch` 14.48.5 |
| `SuSE-RES-8/9-Branch` | 2022-09-29 | dead (RES = Expanded Support) | |
| `SuSE-SLE-11-SP2`, `SuSE-Code-1[123]_*`, `SuSE-SLE-10-*`, `SuSE-Linux-1[01]_*` | 2009–2016 | dead | |
| `zyppng-staging`, `split-zyppng`, `re-enable-zyppng`, `zypp2`, `switch-to-coro`, `iterators-ng`, `polyfill` | 2024–2026 | NG development topic branches | |
| `doc-to-pages`, `fix-json-install`, `proxy-tests`, `fix-DISABLE_MEDIABACKEND_TESTS`, `689-build-error-against-rpm-600`, `726-edition_test-fails-in-17386`, … | 2025–2026 | short-lived fix branches, numbered ones refer to GitHub issues | |
| `ma-*` (e.g. `ma-enhance_key_import_workflow`, `ma-1268321-duplicated_http_header`, `ma-PED-11922-…`) | | Michael Andres' work branches, bsc/PED number in the name | |
| `copilot/*` | 2026 | bot-generated PR branches | |

`downstream/*` refs belong to the personal fork `bzeller/libzypp` and are not codestreams.

Backport mechanics for the SLE‑12 branch: cherry-pick with path translation (`zypp/zypp/X` → `zypp/X`, `zypp/package/` → `package/`), run that branch's own `mkChangelog` from the repo root, tag `16.22.N`. Then update `zypp:SLE-12-SP2-Branch` and do the IBS maintenance submission. That branch is **not** auto-uploaded. `zypp:SLE-12-SP2-Branch/libzypp/_history` shows commits by user `mlandres` (not `zypp-team`) with comments like `6a8c7731f (changes 16.22.19 (0), 2026-06-08)`, one per release commit. So the maintainer uploads the `make srcpackage` output by hand. The tag push also triggers the GitHub Pages doc workflow (§16.2).

---

## Pitfalls & special cases

- `VERSION.cmake`'s "see configure.ac" is stale; soname math is in `zypp/CMakeLists.txt`.
- Soname = `MAJOR*100 + COMPATMINOR` (currently `libzypp.so.1735`); MINOR bumps don't change it.
- 17.33.4 was mistakenly released with COMPATMINOR 35 > MINOR, fixed by 17.34.0 (34) "Fix versioning scheme".
- `mkChangelog` must run in `zypp/`, uses hardcoded `vi`, needs the previous release tag present, and refuses non-fast-forward branches; it auto-switches to dry-run if other files are staged.
- `# LAST RELEASED:` line format is parsed by awk — don't edit its shape.
- Weblate commits are excluded from the generated changelog; Weblate writes directly to master, so pull before releasing.
- `ZYPP_DEPRECATED` is empty in Release (NDEBUG) builds — no warnings for consumers built by OBS.
- `LEGACY()` is an unparenthesized `||` expression: always wrap it, e.g. `#if ( LEGACY(1735) ) && !ZYPPNG`; `#if !LEGACY(x)` would be wrong.
- `LEGACY(CL)` is always 0 when `ZYPPNG` is defined: NG builds see the "new" API (e.g. static `ZConfig::update_dataPath`).
- `LEGACY(17)` `ConvertDBReport` is still used by yast2-pkg-bindings on Code15 — removing it needs YaST coordination.
- Two export filters: `-fvisibility=hidden`+`ZYPP_API` AND `libzypp.map` (`zypp::*` only). Symbols outside `namespace zypp` are never exported even with `ZYPP_API`.
- Tests link `zypp-allsym` (static, all symbols); passing tests don't prove export. After 17.33.0 several missing exports (PackageKit, libyui-qt-pkg, YaST bsc#1228138/bsc#1228420, UrlResolverPlugin #560) caused follow-up releases.
- Missing compat header broke consumers in 17.35.0 (fixed 17.35.1, #557).
- SLE15, SLE16 and Tumbleweed all build from master; codestream behaviour differs via spec-driven `LIBZYPP_CONFIG_*` defaults, `classic_rpmtrans_as_default`, `keep_legacy_zyppconf`. Reproduce SLE bugs with those CMake flags.
- On Code16 static libsolv is in `libsolv-devel-static`; `%requires_eq libsolv-tools-base` pins repo2solv tools to the build-time libsolv.
- libsolv ABI changes can force a libzypp soname bump (bsc#1115341) because libsolv is linked statically yet exposed in headers.
- `-D_FILE_OFFSET_BITS=64` in `libzypp.pc` Cflags is ABI-relevant; `ZYPP_CFLAGS`/`ZYPP_FEATURES` in the .pc are never set.
- rpmlint `shlib-policy-name-error` is filtered intentionally (package stays named `libzypp`).
- Feature `Provides: libzypp(...)` in the spec must match `doc/autoinclude/FeatureTest.doc`.
- SLE‑12 branch (16.22.x) has a different tree layout (no `zypp/` subdir); cherry-picks need path fixes.
- `.travis.yml` is dead; real CI is OBS SCM workflow (`.obs/workflows.yml`, PRs only) + GitHub doc workflow.
- OBS submission targets/procedure are not documented in the repo. The verified picture is in §13: `zypp:Head` is the Factory devel project, and `zypp:SLE-15-SP*-Branch` are patch-less `_link`s to it.
- libzypp-glib version lives in a spec `.inc` that CMake parses; spec and CMake compute the soname separately.
- `zypp:Head/libzypp` is overwritten automatically by user `zypp-team` from master. Manual `osc commit`s there are lost, and the project accepts no SRs.
- `zypp:Head` builds master HEAD *between* releases under the last released version number (`gitrev 17.38.16-3-g…`). The same RPM version can mean different content. Check the OBS revision comment.
- SLE 15 SP1–SP7 maintenance builds pick up whatever is in `zypp:Head` through the `_link` chain. A broken master upload propagates to all SLE 15 branch projects at once.
- Not every tag is submitted to Factory (17.38.12 was skipped). Don't assume Factory has every PATCH release.
- PR CI (`zypp:ci:libzypp`) builds the **unconfigured** `libzypp.spec.cmake` with only `set_version` applied, versioned `18.0.0.git…`. A new `@VAR@` in the spec template can break CI but not the release, or the other way round.
- The `master_workflow` in `.obs/workflows.yml` is commented out (since 2025-08-08). Pushes to master get no OBS SCM-CI build there (the `zypp:Head` auto-upload is a separate mechanism).
- `mkChangelog` filters only "Translated using Weblate". "Added translation using Weblate (<lang>)" and "Translation: updated .pot file" land in the draft and must be curated by hand.
- `po/*.po` are globbed at configure time. A language newly added by Weblate makes `potfile_update` abort with "Missing file … (touch CMakeLists.txt?)" until you re-run cmake.
- `potfile_update` commits only if the pot actually changed, but then it rewrites all 65 `.po` files (msgmerged). That can conflict with concurrent Weblate pushes.
- `doc` target depends only on the Doxyfile (stale docs after source edits; use `doc_forced`), and doxygen failures are swallowed (`|| true`).
- `libzypp.doxytag` is installed into a *directory* named `libzypp.doxytag` (`INSTALL(FILES … DESTINATION …/libzypp.doxytag)`).
- The GitHub Pages workflow fires on **any** tag, including `16.22.x` on the SLE‑12 branch (old layout, `make doc` path differs). Its package list duplicates the spec BRs and is already stale (libsolv ≥ 0.7.34 vs 0.7.40).
- Missing asciidoctor/a2x is a configure FATAL_ERROR in `zypp/doc/CMakeLists.txt`, even if you don't want docs.
- `zypp/doc/zypp-CheckAccessDeleted.1.txt` is orphaned and `zypp/tools/zypp-CheckAccessDeleted.cc` is still compiled (glob) though not installed.
- `LIBZYPP_CODESTREAM` is empty in non-spec builds. Logs then show `libzypp: X.Y.Z ()`.
- zypper must be released/submitted *after* the libzypp version it BuildRequires is in Factory. A zypp-stack build does not prove compatibility with the installed `libzypp-devel`.
- `SuSE-SLE-11-SP3-Branch` (9.41.x) got a release as late as Dec 2024. Don't treat SLE 11 as gone when assessing CVE impact.

## Where to start when debugging

- **Which build is the user running?** Look for the startup line `libzypp: 17.38.16 (01600:0160000:00)` in `/var/log/zypp/zypper.log` (from `ZConfig`), or `zypper -vv`/`ZConfig::about()`. The parenthesized part is `LIBZYPP_CODESTREAM` = `suse_version:sle_version:is_opensuse`.
- **Soname/version as configured:** CMake prints `soname: 1735.3.16` and `version: 17.38.16`; generated `build/.../zypp-core/APIConfig.h` has the macros.
- **Exported symbols:** `nm -DC --defined-only build/zypp/zypp/libzypp.so | grep 'zypp::Foo'`; compare two builds with `abidiff` (libabigail) or diff of sorted `nm -DC` output; check `readelf -d libzypp.so | grep SONAME`.
- **Undefined symbol in zypper/YaST after update:** `LD_DEBUG=symbols,bindings` or `ldd -r`, then find whether the symbol lost `ZYPP_API`, moved out of `namespace zypp`, or was compiled out by a `LEGACY()` guard.
- **Changelog preview:** `cd zypp && ./mkChangelog -n`.
- **Spec problems:** configure once, inspect `build/.../package/libzypp.spec`; test codestream conditionals with `osc build` against the SLE/Leap target or by passing the `EXTRA_CMAKE_OPTIONS` manually.
- **Translations:** build target `translations` shows a pot diff; `potfile_update` to commit; check `LANGUAGE=de zypper ...` for domain issues.
- **Docs:** `make -C <build>/zypp/doc/autodoc` (doxygen warnings in `Doxywarnings`); man pages built by asciidoctor targets `<page>_Target`.
- **What did OBS build?** `curl -s https://api.opensuse.org/public/source/zypp:Head/libzypp/_history | tail` (or `osc api …`). Revision comments are `gitrev <git describe> (<subject>, <date>)`. `…/openSUSE:Factory/libzypp/_meta` shows the devel project, and `…/_history` shows the accepted request ids. `https://api.opensuse.org/public/request/<id>` shows creator, reviews and staging.
- **Does SLE 15 SPx get the same sources?** `…/public/source/zypp:SLE-15-SP7-Branch/libzypp?expand=1` and compare the tarball md5 with `zypp:Head`.
- **PR CI failing but local build fine:** look at the expanded spec in `home:zypp-team` (raw `libzypp.spec.cmake` + `set_version`) and at the `_service` of `zypp:ci:libzypp/libzypp`.
- **Release content:** `git log --oneline --no-merges A..B`, `git show B:zypp/package/libzypp.changes | sed -n '1,/^-----/p'`, `git for-each-ref --sort=-committerdate refs/remotes/origin` for branch activity.
- **Translation not shown:** check `<build>/po/<lang>.gmo` exists (re-run cmake for new languages), that the string is in `po/zypp.pot` (run `translations` and look at the diff), and the text domain (`zypp`).
- **Stale online docs:** check the Actions run for the last tag. Locally `make doc_forced` and read `Doxywarnings`.
- History of compat decisions: `grep -n -i 'soname\|export\|visibility' zypp/package/libzypp.changes`, `git log --grep='^changes'`, `git tag --sort=-creatordate`.
