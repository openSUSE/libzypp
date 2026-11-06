# Contributing to libzypp

libzypp sits under zypper, YaST, PackageKit and Agama on every openSUSE and
SUSE Linux Enterprise system. SLE 15, SLE/Leap 16 and Tumbleweed are all built
from `master`, so a mistake reaches all of them at once. These rules exist to
keep that safe. They apply to everyone, including the maintainers.

If something here is unclear for your change, ask in the pull request or
issue before you start.

## Current priorities

1. **Maintenance of the shipped codestreams:** security fixes, regressions,
   bug fixes, and features needed by openSUSE and SLE products.
2. **The next-generation code (`zyppng/`) is paused** and has the lowest
   priority. See [Next-generation code (NG)](#next-generation-code-ng).

## Where code lives

| Directory | What it is | Rules |
|---|---|---|
| `zypp/` | libzypp (`libzypp.so`): `zypp/zypp`, `zypp-tui`, `zypp-compat`, tools, tests | **Production code.** Fixes and requested features go here. Keep changes minimal. The public API/ABI is stable ([Compatibility rules](#compatibility-rules)). |
| `zypp-logic/` | Shared sources compiled into libzypp *and* the NG code: many core classes (`Url`, `Pathname`, `Arch`, `Edition`, `IdString`, …), curl/media helpers, the async/network framework in `*/ng/` | Production code. Headers included by `zypp/` must stay usable by libzypp's consumers ([Compatibility rules](#compatibility-rules)). Changes must compile in both builds. |
| `zypp/zypp/ng/` | Repository manager, refresh, download, keyring and service workflows | Production code, compiled only into `libzypp.so` and run synchronously. Despite the name, treat it like the rest of `zypp/`. |
| `zyppng/` | NG rewrite (C++20 modules), `libzypp-glib`, media workers | **Paused.** See [zyppng/CONTRIBUTING.md](zyppng/CONTRIBUTING.md). |

When looking for a class, search both `zypp/zypp/` and `zypp-logic/`. The
include path (`<zypp/Arch.h>`, `<zypp-core/Url.h>`) doesn't tell you the
directory.

## Before changing behaviour

Read the relevant parts of the [maintainer handbook](maintainer-docs/README.md)
first, at least:

* [Pitfalls & special cases](maintainer-docs/13-pitfalls-and-special-cases.md):
  known traps and historic quirks;
* [Consumers & API surface](maintainer-docs/14-consumers-and-api-surface.md):
  what zypper, YaST, PackageKit and Agama depend on, and what must never change;
* the chapter about the subsystem you touch.

The handbook was generated with AI assistance and may contain mistakes. The code
is the source of truth: if they disagree, trust the code and fix the handbook.

## Compatibility rules

* **Avoid ABI breaks.** An ABI break forces rebuilds of all dependent packages
  (zypper, YaST bindings, libyui, PackageKit, …) in every codestream. Prefer
  ABI-compatible solutions: new free functions or new classes instead of
  changing existing ones, and extending callback reports through `UserData`
  instead of new virtual methods.
* **An ABI break is acceptable only** when a needed feature can't reasonably be
  done otherwise, **and** it stays source compatible (existing consumer code
  still compiles unchanged), **and** the maintainers agree. Point out the need
  in the pull request. Version and soname bumps (`COMPATMINOR` in
  `zypp/VERSION.cmake`) are done by the maintainers at release time.
* Don't change public class layouts, virtual function lists or exported
  function signatures, except under the rule above.
* Don't change existing `UserData` keys or the C++ types stored in them.
  Consumers cast them exactly.
* New public symbols need `ZYPP_API` **and** must be inside `namespace zypp`
  (`zypp/zypp/libzypp.map` exports only `zypp::*`). Tests link the static
  `zypp-allsym` library, so passing tests don't prove that a symbol is exported.
  Check with `nm -DC libzypp.so`.
* Never delete or rename headers in `zypp/zypp-compat/`. Consumers still
  include the old paths.
* Public headers must keep compiling for all consumers. Don't add C++20
  features to them.
* Don't change these without an explicit request, because consumers rely on
  them: default answers of callbacks, results that decide zypper's exit codes
  (`ZYppCommitResult`), the history log format, lock file semantics and
  `zypp.conf` defaults.
* Code guarded by `LEGACY(n)` is removed only on a soname or major version
  bump.

## How to work

* **Keep diffs minimal.** Fix the problem; don't refactor surrounding code,
  rename things or reformat files in the same change.
* **Code in `zypp/zypp/ng/`:** prefer plain, synchronous C++17 for new or
  changed code. Don't add new `MaybeAwaitable`, `zypp_co_await` or `and_then`
  pipeline chains. Turning a function you are changing anyway into plain code is
  fine if the behaviour stays the same.
* **Add a regression test** with each fix where feasible (`zypp/tests/`; see
  [maintainer-docs/11-testing.md](maintainer-docs/11-testing.md)).
* **Logging:** use `MIL`/`WAR`/`ERR`/`DBG` like the surrounding code. Never log
  passwords or other credentials. `Url::asString()` hides passwords, but raw
  strings and URL query strings don't.
* **Errors:** follow the surrounding code (`ZYPP_THROW` / `ZYPP_CAUGHT` /
  `ZYPP_RETHROW`). Don't introduce new error mechanisms.
* **Style:** `.clang-format` exists, but match the surrounding code rather than
  reformatting whole files.
* **Translations:** user-visible strings use `_()`. Don't change existing
  message texts without a reason, because translations depend on them.

## Security

* **Don't report suspected security issues publicly**: no public issues,
  pull requests, commit messages, code comments or handbook entries. Report them
  privately to the SUSE security team (<security@suse.de>) or in a non-public
  Bugzilla bug. This includes issues in signature or checksum verification,
  credential handling, TLS, plugin execution, path handling and metadata
  parsing.
* Never weaken GPG, checksum or TLS checks, not even temporarily for testing.

## Building and testing

* Build instructions: [README.md](README.md#building). To build libsolv,
  libzypp and zypper together, use
  [zypp-stack](https://github.com/openSUSE/zypp-stack).
* Run `ctest` in the build directory (`ctest -R <name>` for single tests). At
  least run the tests of the subsystem you touched.

## Commits and changelog

* One logical change per commit. Use an imperative subject line; the body
  explains *why*.
* Reference bugs as `(bsc#1234567)`, `(CVE-…)` or `(fixes #123)`. SLE
  maintenance updates require the `bsc#`/`CVE-` references.
* The release changelog `zypp/package/libzypp.changes` is generated from the
  commit messages at release time (`zypp/mkChangelog`). Don't edit it in your
  pull request.

## Next-generation code (NG)

`zyppng/` (the C++20 module rewrite and `libzypp-glib`) is paused and has the
lowest priority. Changes there, and NG-only changes in `zypp-logic/*/ng`, follow
the additional rules in [zyppng/CONTRIBUTING.md](zyppng/CONTRIBUTING.md). The
rules in this file apply as well.
