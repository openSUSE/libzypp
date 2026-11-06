# Contributing to the next-generation code (`zyppng/`)

> **Status: NG development is paused and has the lowest priority.** Maintenance of
> the shipped libzypp ([../CONTRIBUTING.md](../CONTRIBUTING.md#current-priorities))
> comes first. Please discuss NG work with the maintainers before starting on it,
> especially migration, module or `libzypp-glib` work.
>
> All rules in [../CONTRIBUTING.md](../CONTRIBUTING.md) (compatibility, security,
> commits) apply here as well. This file adds the rules specific to NG.
>
> Background, history and lessons learned:
> [../maintainer-docs/10-zyppng-next-generation.md](../maintainer-docs/10-zyppng-next-generation.md).

## Design: three tiers

The NG application logic is a separate **rewrite** in C++20 inside `zyppng/`. It
shares only plain data types and infrastructure with libzypp (`zypp/`), never
application logic.

### `zyppng/` (next generation)

* The only place new **NG** application logic is written. Fixes and features for
  the shipped codestreams go into `zypp/` ([../CONTRIBUTING.md](../CONTRIBUTING.md#where-code-lives)).
* C++20, written with C++23 in mind. Strict RAII and modern idioms: ranges,
  concepts, `std::expected`, `std::span`, `std::string_view`.
* If a C++23 feature is not available in the compiler, use a C++20-compatible
  backport or polyfill and document it, so it can be replaced later.
* No new Boost dependencies where the standard library or a header-only C++20
  alternative exists.
* All components must be thread-aware. Shared mutable state needs explicit
  synchronisation (`std::atomic`, `std::mutex`).
* **All `zyppng/` code is delivered as a C++20 named module** (see
  [Module architecture](#module-architecture)).

### `zypp-logic/` (shared code)

* No NG logic outside the `ng/` subtrees. Fixes for libzypp in other parts of
  `zypp-logic/` are fine.
* Everything under `zypp-logic/*/ng/` (e.g. `zypp-core/ng/`, `zypp-media/ng/`,
  `zypp/ng/`) may be reworked when that improves correctness or safety, as long
  as the tests stay green and all callers are updated. Much of it is used by the
  shipped libzypp too, so these changes must still compile and work there.
* C++17 implementation files. Headers included by `zypp/` must stay usable by
  libzypp's consumers ([../CONTRIBUTING.md](../CONTRIBUTING.md#compatibility-rules)).
* `zypp-logic/zypp/ng/` is the **migration source**: code there is moved to
  `zyppng/` as a whole. Classes that `zypp/` still needs stay in place and are
  tagged with a `// LEGACY-REQUIRED` comment at the top of their header.
* Only code that must be shared across the tiers (e.g. media workers that can't
  link the application code) is added under `zypp-logic/*/ng/`.
* Prefer header-only polyfills unless there is no alternative.

### `zypp/` (libzypp)

* From the NG point of view: never move NG logic into `zypp/`, and never make NG
  depend on it.
* Logic needed by both sides is moved into `zypp-logic/`, not duplicated.

### Dependency direction

No cycles are allowed:

```
zyppng  →  zypp-logic   (may use the shared code)
zypp    →  zypp-logic   (may use the shared code)
zyppng  ↛  zypp         (NG must not depend on libzypp)
zypp    ↛  zyppng       (libzypp must not depend on NG)
```

### Migrating code from `zypp-logic/zypp/ng/` to `zyppng/`

1. Pick the class to migrate.
2. Check whether any file under `zypp/` includes it, directly or transitively.
   * **Yes:** tag it `// LEGACY-REQUIRED` and leave it in place until that
     dependency is gone. Write the new version in `zyppng/` and switch the
     `zyppng/` users to the module import.
   * **No:** move the implementation to `zyppng/`, switch all users to the module
     (`import :<partition>;` inside the `zyppng` module, `import zyppng;` outside
     it) and delete the old file.
3. Leave no forwarding headers behind in `zypp-logic/zypp/ng/`. Either delete the
   file or replace it with a `static_assert` that tells users where the code
   went.

### Further rules

* **Prefer a clean rewrite** in `zyppng/` over shims or wrappers around libzypp
  types.
* **Drop Boost** when moving or rewriting code, whenever `std::` or a
  header-only C++20 alternative exists.
* **No raw ownership:** no raw `new`/`delete` or `malloc`/`free`. Use
  `std::unique_ptr` or `std::shared_ptr`.
* **Errors:** anything that depends on data (user input, configuration values,
  files, network, parsed content) is a recoverable error and uses
  `zyppng::expected<T, E>` or `std::optional`. Don't add new exception-based
  error paths for it. Programmer errors (violated preconditions with no sensible
  recovery, e.g. using an unregistered key token) may throw
  `std::logic_error`-family exceptions.

## Module architecture

All code in `zyppng/` belongs to a single named module, `zyppng`, structured with
**module partitions**.

### Why partitions and not several top-level modules

The dot in a module name such as `zyppng.sat` is only part of the name. The
compiler treats `zyppng.sat` and `zyppng.solver` as unrelated modules, with no
hierarchy and no shared internal visibility. Partitions are a language feature
that:

* allow internal partitions that are invisible outside the `zyppng` module;
* enforce the internal/external boundary in the language rather than by
  convention.

Partitions can only be imported by units of the same module. Users outside the
module always write `import zyppng;` and get the whole exported API; importing a
single domain from outside (e.g. `import zyppng:sat;`) is not possible in C++20.
That would need separate top-level modules, which this design deliberately
avoids.

### File extensions

| File role | Extension | Example |
|---|---|---|
| Module interface unit (exported partition or primary interface) | `.cppm` | `sat.cppm` |
| Module implementation unit | `.cc` | `pool.cc` |

The extension alone tells what a file is. Don't use `.ixx` or `.cpp`.

### Layout

**1. Primary module interface (`zyppng.cppm`)** re-exports all public
partitions; users who want the full API write `import zyppng;`.

```cpp
export module zyppng;

export import :sat;      // partition imports are relative: ":name", not "zyppng:name"
export import :solver;
// … one line per interface partition
```

Every **interface** partition (`export module zyppng:…;`) must be re-exported by
the primary interface, directly or through another interface partition.
Otherwise the module is ill-formed.

**2. Exported partition interface (`<domain>.cppm`)**

```cpp
// Global module fragment: the ONLY place where #include is allowed in a .cppm file.
// Use it only for zypp-logic/ headers or system headers that are not modules yet.
module;
#include <zypp-logic/some/legacy/header.h>
#include <string>   // only if std library modules are not available

// Named module declaration: no #include below this line.
export module zyppng:sat;

// Internal (implementation) partition, invisible outside the module
import :sat_detail;

// Exported public API
export namespace zyppng::sat {
  // …
}
```

**3. Internal partition (`<domain>_detail`)**, importable by other units of the
module but not by its users. Make it an **implementation partition** (no `export`
in the module declaration). An interface partition (`export module
zyppng:sat_detail;`) would have to be re-exported by the primary interface and
would then no longer be internal.

```cpp
module;
#include <…>          // shared headers if needed

module zyppng:sat_detail;   // implementation partition: no `export`

// implementation details, helpers, internal types
namespace zyppng::sat::detail {
  // …
}
```

The current tree has no implementation partitions yet. Since such a partition can
be imported, CMake must scan it; check where the CMake version in use expects it
(the `FILE_SET CXX_MODULES` or the normal sources) when adding the first one.

**4. Module implementation unit (`<domain>.cc`)**, for large partitions whose
implementation is better kept separate from the interface:

```cpp
module;
#include <…>          // shared headers if needed

module zyppng;        // implementation unit of the module (no partition name)

import :sat;          // the interface partition whose declarations are implemented here

// implement what sat.cppm declares
```

Don't write `module zyppng:sat;` here: that declares a second partition named
`sat`, which can't coexist with `export module zyppng:sat;`. The existing code
follows this pattern, e.g. `zyppng/lib/zypp/ng/sat/pool.cc` (`module zyppng;` +
`import :sat_pool;`).

### Rules for module interface units (`.cppm`)

* `#include` is **only allowed in the global module fragment** (before
  `export module …;`), never below the module declaration.
* Don't export implementation details, internal helpers or anything tagged
  `// INTERNAL`.
* Don't `export using namespace …;`. Export individual declarations.
* Every exported name lives in `namespace zyppng` or one of its sub-namespaces.
* No `#pragma once` or include guards in `.cppm` files; they have no meaning for
  module interface units.

### CMake

`zyppng` needs **CMake 3.28** or later (for C++ module dependency scanning). The
rest of the project keeps its minimum of 3.17.

```cmake
# in zyppng/CMakeLists.txt
cmake_minimum_required(VERSION 3.28)   # required for FILE_SET CXX_MODULES

target_sources(zyppng
  PUBLIC
    FILE_SET CXX_MODULES FILES
      zyppng.cppm        # primary interface
      sat.cppm
      solver.cppm
      # … one entry per .cppm file
)
```

Module implementation units (`.cc`) go into a normal `target_sources()` call, not
into the `FILE_SET`.

### Naming

| Concept | Convention | Example |
|---|---|---|
| Module name | `zyppng` | – |
| Public partition | `zyppng:<domain>` | `zyppng:sat`, `zyppng:solver` |
| Internal partition | `zyppng:<domain>_detail` | `zyppng:sat_detail` |
| Exported namespace | `zyppng::<domain>` | `zyppng::sat`, `zyppng::solver` |
| Interface file | `<domain>.cppm` | `sat.cppm` |
| Implementation file | `<domain>.cc` | `sat.cc` |
