# 00a – Concepts Primer and Glossary

This chapter is the vocabulary primer for the rest of the handbook. It explains every important package-management and
libzypp concept from first principles: what it is, why it exists, how libzypp models it (classes and files), a small
example, the main gotcha, and where the details are. Part A is a narrative grouped by topic. Part B is an alphabetical
glossary (terms, abbreviations, bug-tracker prefixes) with code anchors and chapter links.

All paths are relative to `libzypp/`. Remember that many "legacy" classes physically live in `zypp-logic/` (see
[00](00-big-picture.md) and [01](01-layout-build-and-dependencies.md)); the include path `<zypp/Foo.h>` does not tell
you which directory holds the file.

How each concept is laid out:

> **What** – definition · **Why** – the problem it solves · **In libzypp** – classes/files ·
> **Example** · **Gotcha** · **See** – chapter with depth

---

# Part A – Conceptual primer

## A.1 Packages and identity

### rpm package

* **What**: a file (`foo-1.2-3.1.x86_64.rpm`) with a payload (files), scriptlets (`%pre`, `%post`, `%posttrans`, …),
  a header with metadata (name, version, dependencies, changelog, signatures).
* **Why**: rpm is the installer. libzypp never unpacks rpms itself. It decides *what* to install, downloads it,
  checks signatures, and hands it to rpm (directly via `rpm` calls or via the `zypp-rpm` helper).
* **In libzypp**: an rpm in a repo or in the rpmdb becomes a `sat::Solvable` of kind `package`
  (`zypp/zypp/sat/Solvable.h`), wrapped by `Package` (`zypp/zypp/Package.h`). rpm access is in
  `zypp/zypp/target/rpm/` (`RpmDb`, `librpmDb`, `RpmHeader`).
* **Gotcha**: most package attributes come from **repo metadata** (parsed into a `.solv` file), not from the rpm. Some
  things only exist on one side: support keywords and the "retracted" marker exist only in repo metadata; the full file
  list and the changelog exist only for installed packages (`Package::changelog()` reads the rpm header from the rpmdb).
* **See**: [03 §16.4](03-sat-pool-and-resolvables.md), [08](08-target-commit-and-rpm.md).

### NEVRA (Name, Epoch, Version, Release, Arch)

* **What**: the full identity of an rpm: `name-[epoch:]version-release.arch`, e.g. `bash-5.2.37-1.1.x86_64`.
* **Why**: name alone is ambiguous (several versions, several archs in a pool). NEVRA is how humans and tools name an
  exact build.
* **In libzypp**: the term NEVRA does not appear in libzypp headers. The parts are `Solvable::name()`,
  `Solvable::edition()` (E-V-R in one `Edition`), `Solvable::arch()`. `Solvable::asString()` prints libsolv's
  `name-evr.arch`. zypper uses NEVRA strings for requests (e.g. `addRequire` of an exact NEVRA for locked patches,
  [04 §19.4](04-solver.md)).
* **Gotcha**: two solvables with identical NEVRA can still differ (vendor, buildtime, repo). "Same package" checks use
  `Solvable::identical()` (`solvable_identical`), not a NEVRA compare.

### Edition (`[epoch:]version[-release]`)

* **What**: the "EVR" part. `epoch` is an integer override (rarely used), `version` is upstream's version, `release`
  is the distribution build counter.
* **Why**: deciding "newer/older" needs rpm's exact comparison rules, and dependency matching (`foo >= 1.2`) needs a
  wildcard-aware variant.
* **In libzypp**: `Edition` (`zypp-logic/zypp/Edition.{h,cc}`), stored as one `IdString`. `epoch()`, `version()`,
  `release()` are parsed on demand (release = text after the **last** `-`). Comparison is libsolv's
  `pool_evrcmp_str`:
  * `Edition::compare` / `operator<` → `EVRCMP_COMPARE` – a strict total order, use it for sorting.
  * `Edition::match` → `EVRCMP_MATCH` – an empty version or release acts as a wildcard (`1.2` matches `1.2-3`). Used
    for capability matching (`Edition::MatchRange`).
* **Example**: `Edition("1.0-1") < Edition("1:0.9-1")` (epoch wins). `1.a < 1.0` (alpha segment < numeric segment).
  `1.0 == 1_0` (separators ignored).
* **Gotcha**: **`match` is not transitive** – never order a container by it. The rules only hold because the string
  pool uses `DISTTYPE_RPM` (`zypp-logic/zypp/ng/sat/stringpool.cc`, libzypp#726). `Edition::noedition` (`""`) is a
  valid edition.
* **See**: [03 §4.1](03-sat-pool-and-resolvables.md).

### Arch, compatible archs, noarch

* **What**: the CPU architecture a package is built for (`x86_64`, `aarch64`, `i586`, …). `noarch` packages (scripts,
  data) run anywhere. An arch is *compatible* with a system if the system can run it.
* **Why**: a pool may hold packages for many archs (multilib, foreign repos). The system can only install compatible
  ones and should prefer the "best" one.
* **In libzypp**: `Arch` (`zypp-logic/zypp/Arch.{h,cc}`) with a compat table built by `defCompatibleWith(...)`. For
  example `x86_64` is compatible with `noarch, i386, i486, i586, i686, athlon` (`Arch.cc` ~l.536). The system arch
  comes from `ZConfig::systemArchitecture()` (auto-detected from `uname` + cpuid for `x86_64_v2/3/4`; overridable via
  `arch=` in `zypp.conf` or `ZYPP_TESTSUITE_FAKE_ARCH`).
* **Gotcha**: when a repo is loaded, **solvables of incompatible arch are deleted from the pool**
  (`sat::detail::PoolImpl::_postRepoAdd`). Changing the arch later requires reloading the repos. The arch table is
  close to its 64-bit limit. `src`/`nosrc` are pseudo-archs that mark source packages.
* **See**: [03 §4.2](03-sat-pool-and-resolvables.md), [09 §6](09-configuration-and-environment.md).

### Vendor

* **What**: the string identifying who built a package (`SUSE LLC <https://www.suse.com/>`, `openSUSE`,
  `obs://build.opensuse.org/home:foo`).
* **Why**: replacing a SUSE build with a third-party build (or vice versa) during an update is risky. Vendor
  stickiness prevents silent vendor switches.
* **In libzypp**: `Solvable::vendor()` (an `IdString`); equivalence classes in `VendorAttr`
  (`zypp/zypp/VendorAttr.{h,cc}`) read from `/etc/zypp/vendors.d/*`. See [vendor stickiness](#vendor-stickiness-and-vendor-equivalence).
* **Gotcha**: matching is case-insensitive **longest prefix**; vendors starting with `opensuse` match only exactly
  (bnc#812608); the old built-in `suse` ≡ `opensuse` equivalence is gone (bsc#1030686).

### Ident and kind prefixes

* **What**: the *ident* is libsolv's "name" of a solvable, which carries the kind as a prefix for non-packages:
  `foo` (package), `patch:openSUSE-2024-123`, `pattern:base`, `product:SLES`, `application:org.gnome.Gedit`.
* **Why**: libsolv has one flat name space. Prefixing lets a package and a pattern both be called `base`.
* **In libzypp**: `ResKind` (`zypp-logic/zypp/ResKind.{h,cc}`): `package`, `srcpackage`, `patch`, `pattern`, `product`,
  `application`, `nokind`. `ResKind::satIdent(kind, name)` builds idents; `Solvable::ident()`/`kind()`/`name()` split
  them. `ResKind::explicitBuiltin()` recognises a `kind:` prefix.
* **Example**: `ResKind::satIdent(ResKind::pattern, "base") == "pattern:base"`; `Solvable::name()` of that solvable is
  `base`.
* **Gotcha**: source packages have **no prefix** – they are identified by arch `src`/`nosrc` and share the ident with the
  binary package. `pool::PoolImpl` and `pool::ByIdent` therefore store srcpackage ident ids **negated**. A package name
  containing `:` would be misdetected as a kind prefix.
* **See**: [03 §4.3](03-sat-pool-and-resolvables.md).

---

## A.2 The layered object model

### IdString and the string pool

* **What**: an `int` handle to a string interned in libsolv's string pool.
* **Why**: millions of names, versions and dependency strings repeat. Interning makes them cheap to copy and compare
  (equality = integer compare) and is how libsolv works internally.
* **In libzypp**: `IdString` (`zypp-logic/zypp/IdString.{h,cc}`), `IdStringType<Derived>` CRTP helper used by
  `Edition`, `ResKind`, `Arch`-like types, `Locale`. The single libsolv `Pool` is owned by `zyppng::sat::StringPool`
  (`zypp-logic/zypp/ng/sat/stringpool.{h,cc}`, `// LEGACY-REQUIRED`).
* **Gotcha**: every constructed string stays in the pool forever (until all repos are freed). Ids are only valid inside
  this process and pool – **never persist raw ids**. `IdString::Null` (id 0) and `IdString::Empty` (id 1) differ but
  are both `empty()`.

### Solvable → Resolvable/ResObject → PoolItem → Selectable

The most confusing part for newcomers is that one package is represented by up to five different objects. They are
layers, each adding something:

```
  libsolv C struct  ::Solvable  (in Pool->solvables[id])        owned by libsolv
        │  wraps an Id
        ▼
  sat::Solvable                  zypp/zypp/sat/Solvable.h       value handle (just the id). All data lookups,
        │                                                       dependencies, kind/name/edition/arch/vendor.
        │  makeResObject(solvable) picks the subclass by kind()
        ▼
  Resolvable  ◄─ ResObject  ◄─ Package / Patch / Pattern /      zypp/zypp/Resolvable.h, ResObject.h, Package.h …
                               Product / SrcPackage /           ref-counted (intrusive_ptr), stateless:
                               Application                      kind-specific accessors read lazily from the solvable
        │
        │  + ResStatus  (+ buddy)
        ▼
  PoolItem                       zypp/zypp/PoolItem.h           shared_ptr<Impl{ResStatus, ResObject::constPtr, buddy}>
        │                                                       = "a solvable plus its status". Stored in ResPool.
        │  grouped by (kind, name)
        ▼
  ui::Selectable                 zypp/zypp/ui/Selectable.h      all installed + available PoolItems of one
                                                                name, candidate selection, ui::Status (S_Install…)
```

| Layer | Owns state? | Who uses it |
|---|---|---|
| `sat::Solvable` | no (id only) | everything; cheapest; safe on stale ids (returns defaults) |
| `ResObject` & subclasses | no (id only) | code that needs kind-specific accessors (`Patch::category()`, `Product::flavor()`) |
| `PoolItem` | **yes**: `ResStatus` | solver input/output, commit, locks |
| `ui::Selectable` | candidate choice; status is that of its PoolItems | YaST package selector, zypper `info`/`search` |

* **Why so many**: `sat::Solvable` is the libsolv view (data); `ResObject` is a friendlier typed API; `PoolItem` adds the
  mutable *transaction state* the application and solver negotiate; `Selectable` is the "one row per package name" UI
  view.
* **Example**:
  ```cpp
  sat::Solvable s = …;                         // e.g. from WhatProvides
  PoolItem pi( s );                            // look up the PoolItem (O(1), index = solvable id)
  Package::constPtr p = pi->asKind<Package>(); // ResObject subclass
  ui::Selectable::Ptr sel = ui::Selectable::get( pi );   // the per-name group
  pi.status().setToBeInstalled( ResStatus::USER );
  ```
* **Gotcha**: `Resolvable` here means "the C++ base class". In everyday talk, "resolvable" means "anything the solver
  handles" (package, patch, …). `PoolItem::status()` returns a **mutable** `ResStatus&` even on a `const PoolItem`.
* **See**: [03 §6–8, §12](03-sat-pool-and-resolvables.md).

### Serial numbers and stale handles

* **What**: `sat::Pool::serial()` changes whenever the pool content changes (repo added/removed);
  `sat::Pool::serialIDs()` changes when solvable ids are *reused* (after the last repo was removed).
* **Why**: all derived layers (`ResPool` store, `ResPoolProxy`/Selectables, whatprovides, caches) are rebuilt
  lazily. They watch the serials to know when.
* **In libzypp**: `SerialNumber` / `SerialNumberWatcher` (`zypp-logic/zypp/base/SerialNumber.h`);
  `pool::PoolImpl::checkSerial()`; `sat::Transaction` becomes `!valid()` when the serial changes.
* **Gotcha**: holding `PoolItem`s, `Selectable`s or a `ResPoolProxy` across a repo reload keeps **stale objects** alive;
  status changes on them no longer affect the pool. UIs must re-fetch after pool changes.
* **See**: [03 §5.2, §7.1](03-sat-pool-and-resolvables.md).

---

## A.3 Dependencies

### Capability

* **What**: one dependency expression: `foo`, `foo >= 1.2`, `libc.so.6()(64bit)`, `/usr/bin/perl`,
  `(python3 or python311)`, `namespace:language(de)`.
* **Why**: packages do not depend on packages, they depend on *capabilities* that other packages *provide*. That gives
  alternatives, virtual provides, file deps, renames.
* **In libzypp**: `Capability` (`zypp/zypp/Capability.{h,cc}`) is just a libsolv dependency `Id`. `CapDetail` splits it
  into kind NAMED / VERSIONED / EXPRESSION with name, op (`Rel`), edition, lhs/rhs. `Capabilities` is a read-only
  view of a solvable's dependency array. `Rel` (`zypp-logic/zypp/Rel.h`) is the operator.
* **Example**: `Capability("foo >= 1.2")`, `Capability("foo", Rel::GE, Edition("1.2"))`,
  `Capability("(a and b)")` (rich dep parsed by `pool_parserpmrichdep`).
* **Gotcha**: string parsing is lenient – anything it can't parse becomes a NAMED capability (useful for search
  strings). `name.arch` is only split off for built-in arch names. `Capability::c_str()` for reldeps returns a pointer
  into a **5-slot round-robin static buffer** (not thread safe). `Capability::matches()` is libzypp's own simple
  matcher; the solver uses libsolv's whatprovides.
* **See**: [03 §4.4](03-sat-pool-and-resolvables.md).

### Dependency types

`Dep` (`zypp-logic/zypp/Dep.h`) enumerates them; `Solvable::dep_provides()`, `dep_requires()`, … return
`Capabilities`.

| Type | Meaning | Strength |
|---|---|---|
| `PROVIDES` | "I offer capability X" (own name = implicit provide, virtual names, files, `pattern()`, `product()`…) | – |
| `REQUIRES` | must be satisfied for me to be installed | hard |
| `PREREQUIRES` | requires that must be installed *before* me (rpm `Requires(pre)`); stored after `solvablePrereqMarker` inside the requires array | hard + ordering |
| `CONFLICTS` | X must not be installed together with me | hard |
| `OBSOLETES` | installing me removes X (renames/replacements) | hard |
| `RECOMMENDS` | install X too if possible | weak (forward) |
| `SUGGESTS` | X might be useful (UI hint; solver ignores) | weakest |
| `SUPPLEMENTS` | "install me if X is installed" (reverse recommends) | weak (reverse) |
| `ENHANCES` | reverse suggests | weakest |

* **Gotcha**: `dep_requires()` contains **both** requires and prerequires; `dep_prerequires()` yields an empty list if
  the marker is absent. Weak deps are followed unless "only requires" (`--no-recommends`) is active, and even then
  namespace recommends/supplements are followed ([onlyRequires](#recommends-supplements-and-onlyrequires)).

### Versioned and rich (boolean) dependencies

* **What**: versioned = `name op edition` (`<`, `<=`, `=`, `>=`, `>`, `!=` in libzypp; `!=` is rarely used in rpm).
  Rich deps (rpm ≥ 4.13) = boolean expressions `(a and b)`, `(a or b)`, `(a if b)`, `(a if b else c)`,
  `(a unless b)`, `(a with b)`, `(a without b)`.
* **In libzypp**: libsolv reldeps `REL_AND/REL_OR/REL_COND/REL_UNLESS/REL_WITH/REL_WITHOUT`; `CapDetail::EXPRESSION`
  with `CapDetail::CapRel`. libzypp does not evaluate them – libsolv does.
* **Gotcha**: problem texts print them with `pool_dep2str` ("nothing provides '(a or b)' needed by …").
* **See**: [04 §18.3](04-solver.md).

### Namespaces (language, modalias, filesystem)

> Modalias in depth (use cases, spec syntax, CPU vendor/features, pitfalls): [03b](03b-modalias.md).

* **What**: special capabilities `namespace:NAME(value)` whose truth is decided by a callback into libzypp at solve
  time instead of by a provider package.
* **Why**: "install the German translation of every installed package", "install the driver for this PCI device",
  "install btrfs tools if a btrfs filesystem is used" – facts about the *system*, not about packages.
* **In libzypp**: `ResolverNamespace` (`zypp-logic/zypp/ResolverNamespace.h`), answered by
  `sat::detail::PoolImpl::nsCallback` (`zypp/zypp/sat/detail/PoolImpl.cc`):

  | Namespace | True if |
  |---|---|
  | `namespace:language(xx)` | `xx` is in the requested locales (expanded by fallbacks, `de_DE` → `de`) |
  | `namespace:modalias(…)` | `target::Modalias` finds a matching device modalias (value may be hex-encoded) |
  | `namespace:filesystem(x)` | `x` is listed in `USED_FS_LIST` in `/etc/sysconfig/storage` |

* **Example**: `-lang` packages carry `Provides: locale(foo:de;fr)`, which libsolv's `repo_fix_supplements()` turns into
  `Supplements: (foo and (namespace:language(de) or namespace:language(fr)))`.
* **Gotcha**: libsolv caches namespace answers in its whatprovides data. Changing requested locales must call
  `depSetDirty()` (libzypp does). There is no "product" namespace in libzypp's callback; `product()`/`pattern()` are
  plain provides used as markers. Any other namespace logs `WAR "Unhandled ..."` and is false.
* **See**: [03 §9](03-sat-pool-and-resolvables.md), [04 §12, §18.2](04-solver.md).

### WhatProvides

* **What**: the list of solvables providing a capability.
* **In libzypp**: `sat::WhatProvides(cap)` (`zypp/zypp/sat/WhatProvides.h`) iterates `pool_whatprovides`;
  `sat::WhatObsoletes(solvable)`; `sat::Pool::whatMatchesDep`, `whatContainsDep`.
* **Gotcha**: it calls `sat::Pool::prepare()` first, which may (re)create libsolv's whatprovides index – expensive
  after pool changes.

### `packageand()` and other legacy SUSE dependency syntax

* **What**: pre-rich-dep SUSE syntax in `Supplements`: `packageand(a:b)` = "a and b", `modalias(...)`,
  `language(...)`, `filesystem(...)`, and `foo:/path` "split provides".
* **In libzypp**: none – libsolv rewrites them at parse time (`repo_fix_supplements()` in `libsolv/src/suse.c`) into
  `REL_AND` / `namespace:…` / `namespace:splitprovides(...)`. libzypp always sets `SOLVER_FLAG_SPLITPROVIDES`.
* **See**: [04 §18.3](04-solver.md).

---

## A.4 Resolvable kinds

> **Deep dive:** [03a – Special solvables](03a-special-solvables.md) covers how each of these kinds is *defined* in metadata, how libsolv converts it, and its quirks.

### package and srcpackage

* **package**: a binary rpm. `Package` adds `changelog()` (installed only), `filelist()`, `location()`,
  `cachedLocation()`, `sourcePkgName()`, `keywords()`, `vendorSupport()`, `supersededBy()`.
* **srcpackage**: a source rpm (`.src.rpm`/`.nosrc.rpm`). `SrcPackage` (`zypp/zypp/SrcPackage.h`). Internally
  `name REL_ARCH src`, printed as `srcpackage:name`. Only installable via `zypper source-install`; the solver ignores
  them unless `Resolver::setSolveSrcPackages(true)`.
* **Gotcha**: `Package::sourcePkgName()`: libsolv stores the source name only if it differs; id 0 means "same as
  binary". Support level keywords (`support_l3`, …) only exist in repo metadata; an installed package inherits them from
  an identical available copy, otherwise `VendorSupportUnknown`.
* **See**: [03 §16.4–16.5](03-sat-pool-and-resolvables.md).

### patch

* **What**: an update advisory from `updateinfo.xml` (`patch:openSUSE-SLE-15.6-2024-1234`): a set of fixed package
  versions plus category, severity, references (bugzilla/CVE), message, flags.
* **Why**: customers apply *fixes* ("all security patches"), not arbitrary newest packages. Patches also carry
  reboot/restart/interactive hints.
* **In libzypp**: `Patch` (`zypp/zypp/Patch.{h,cc}`). libsolv turns each `<package>` entry into a **conflict**
  `name.arch < evr`, so "installing" a patch means "no installed package may be older than the fixed version".
  * **Category**: `security`, `recommended` (also RHN `bugfix`), `optional`/`feature`/`enhancement`, `document`,
    `yast`, else `other` (`Patch::categoryEnum()`).
  * **Severity**: `low`, `moderate`, `important`, `critical`, `unspecified`/empty → `SEV_NONE`.
  * **Update stack / restart**: `restartSuggested()` (`restart_suggested`) = the patch affects the package manager.
    zypper installs these first ("update stack"), then the rest. The policy lives in **zypper**
    (`SolverRequester::updatePatches`), not libzypp.
  * **Interactive**: `interactiveFlags()` = `Reboot` (reboot suggested) | `Message` | `License` (patch or any content
    package has a license to confirm). `interactiveWhenIgnoring()` backs `--skip-interactive` / `--with-interactive`.
  * **Status** comes from the solver's "establish" pass (`ResStatus` validate field), not from the patch:

    | Status | Meaning |
    |---|---|
    | NONRELEVANT | none of its packages is installed |
    | SATISFIED | installed packages are at least the fixed versions |
    | BROKEN | relevant but not satisfied |
    | needed (`PoolItem::isNeeded()`) | to be installed, or broken and not locked |
    | unwanted (`isUnwanted()`) | broken and locked – "skip this patch forever" (bnc#577118) |

  * **Retracted**: `updateStatus == "retracted"` (jsc#SLE-8770). A retracted patch is never needed.
* **Gotcha**: patches are the only **pseudo-installed** kind (`traits::isPseudoInstalled`, `zypp-logic/zypp/ResTraits.h`):
  they are never in the rpmdb. `Patch::contents()` only lists packages that are currently installed (by name and arch),
  not the whole advisory. A patch for a package installed from a non-equivalent vendor is "irrelevant" (libsolv
  `solvable_is_irrelevant_patch`).
* **See**: [03 §16.1](03-sat-pool-and-resolvables.md), [04 §8, §19](04-solver.md).

### pattern

* **What**: a named group of packages for a use case (`pattern:kde`, `pattern:devel_C_C++`), shown in installers.
* **Why**: users pick "a KDE desktop", not 800 packages.
* **In libzypp**: `Pattern` (`zypp/zypp/Pattern.{h,cc}`). Since libzypp 14.7 patterns are backed by real
  **pattern packages** (`patterns-*` rpms providing `pattern() = name`). From these, `rpmdb2solv -X`/`repo2solv -X`
  auto-generate the `pattern:` solvable (an **autopattern**, `Pattern::isAutoPattern()`, `autoPackage()`). The pattern
  package's requires/recommends/suggests define the contents.
  * `userVisible()`: bool, or an ident string – then visible only if that ident is in the pool.
  * `contents()` / `depends()` / `core()`: sets of *all* providers (all versions/repos), not a resolved selection.
  * `order()` is a string; `category()`, `icon()`, `script()`, `includes()`/`extends()` (old susetags hints).
* **Gotcha**: patterns are no longer pseudo-installed (Fate#309385). `--no-recommends` also drops recommended pattern
  members. `contentsSet()`/`fullContentsSet()` are deprecated (`#if LEGACY(1735)`).
* **See**: [03 §16.2](03-sat-pool-and-resolvables.md).

### product

* **What**: a distribution or extension/module the system is "running" (`product:SLES`, `product:sle-module-basesystem`,
  `product:openSUSE`). Each product has a **release package** (e.g. `sles-release`) that installs an XML **product
  file** to `/etc/products.d/<name>.prod`.
* **Why**: registration, support status, upgrade paths, `$releasever`, which repos update what, end of life.
* **In libzypp**: `Product` (`zypp/zypp/Product.{h,cc}`); installed product solvables are generated by
  `rpmdb2solv -p /etc/products.d`. Key concepts:
  * **Release / reference package**: `Product::referencePackage()` – provider of `product(<name>) = <edition>` in the
    same repo and arch (bsc#1259311: honour `autoproduct() = <pkg>`; installed fallback: owner of the `.prod` file,
    bnc#784900). It becomes the product's [buddy](#a5-buddy--all-meanings).
  * **Base product**: the one `/etc/products.d/baseproduct` (symlink) points to. `Product::isTargetDistribution()` =
    installed and `product:type == "base"`. File view: `TargetImpl::baseproductdata()`.
  * **Flavor**: `Product::flavor()` via `product_flavor(<name>)`/`flavor(...)` provides in the same repo;
    `Target::distributionFlavor()` caches it in `/var/lib/zypp/LastDistributionFlavor`.
  * **CPE id**: `Solvable::cpeId()` → `CpeId` (`zypp-logic/zypp/CpeId.{h,cc}`, CPE 2.3 with WFN matching).
  * **Register target/release/flavor**: `<register>` in the product file (`registerTarget()` e.g. `sle-15-x86_64`).
    `Target::targetDistribution()` filters service repos by it and puts it in the HTTP User-Agent.
  * **Droplist / weakremover**: the release package's `weakremover(...)` provides list packages dropped from the
    distribution. `Product::droplist()`. DUP removes them in a second solver pass when
    `solver.upgradeRemoveDroppedPackages` (bsc#1155819, `SATResolver.cc` ~l.720).
  * **End of life**: `Product::endOfLife()` / `hasEndOfLife()`: *missing* = no EOL planned; *present but empty* = EOL
    exists but not yet known (FATE#320699).
  * `replacedProducts()`, `updateContentIdentifier()`, `urls()` (release notes, register, …), `productLine()`.
* **Gotcha**: product and release package **share one `ResStatus`**. Locking one locks both (bsc#1225267). Do not mix
  up `distributionFlavor()` (pool product) and `targetDistributionFlavor()` (product file `<register><flavor>`).
* **See**: [03 §8, §16.3](03-sat-pool-and-resolvables.md), [08 §13](08-target-commit-and-rpm.md).

### application

* **What**: an AppStream/appdata component (`application:org.gnome.Gedit`) for software centres.
* **In libzypp**: `Application` (`zypp/zypp/Application.h`), generated by libsolv's `repo_add_appdata`. **No accessors**
  beyond `ResObject`. Separately, **appdata plugins** (`/usr/lib/zypp/plugins/appdata/`) are run by `~RepoManager`
  after repo changes so software centres can refresh their caches.
* **See**: [03 §16.4](03-sat-pool-and-resolvables.md), [05 §4.2](05-repositories-and-services.md).

---

## A.5 "Buddy" – all meanings

"Buddy" is used for **three unrelated things** in the code. Always check which one a comment or bug report means.

### (a) PoolItem buddy: product ↔ release package (shared `ResStatus`)

* **What**: a `PoolItem` can use another item's `ResStatus` object. Used for every `Product` and its
  `referencePackage()` (the `-release` rpm).
* **Why**: a product is not an rpm; you install/remove it by installing/removing its release package. Sharing the
  status makes "select product" and "select release package" the same action.
* **In libzypp**: `PoolItem::buddy()` / private `setBuddy()` (`zypp/zypp/PoolItem.{h,cc}`). Set by
  `pool::PoolImpl::store()` for newly added products (`zypp/zypp/pool/PoolImpl.h`). Encoding: `_buddy > 0` = "use the
  buddy's status"; `< 0` = "my status is used by `-_buddy`". Second-level buddies are refused (ERR log).
* **Consequences in code**:
  * Locks: an item is locked if it *or its buddy* matches a lock query (bsc#1225267, `setHardLockQueries`).
  * `Product::replacedProducts()` also looks at what the buddy obsoletes; `Product::droplist()` reads the buddy's
    `weakremover` provides.
  * Commit (`TargetImpl.cc` ~l.1955, ~l.2899): non-package items *with* a buddy are skipped ("status is changed as the
    buddy package gets installed/deleted"); products without buddy are handled explicitly.
* **Gotcha**: products without a release package (e.g. broken metadata) have no buddy – they log
  `WAR ... no reference package found` and behave independently.

### (b) Buddy keys: GPG keys from repo keyhints

* **What**: extra public keys announced in `repomd.xml` as `<keyword>gpg-pubkey-…</keyword>` (**keyhints**). They are
  downloaded into the *general* keyring and remembered as "buddies" of the signing key.
* **Why**: a repo can announce the keys it will use for packages (or its next key). If the metadata is signed by a key
  the user already **trusts**, the buddies are imported as trusted automatically, without asking.
* **In libzypp**: `VerifyFileContext::buddyKeys()` / `addBuddyKey()` (`zypp/zypp/KeyRingContexts.{h,cc}`);
  collected in `getExtraKeysInRepomd()` (`zypp/zypp/ng/repo/workflows/repodownloaderwf.cc` ~l.380–404); imported in
  `VerifyFileSignatureLogic` (`zypp/zypp/ng/workflows/keyringwf.cc` ~l.240–290) only if `fileValidated` **and**
  `signatureIdTrusted`, after `KeyRingReport::reportAutoImportKey`.
* **Gotcha**: hardened in 17.38.12/17.38.15: keyhint file names may not contain `/` (bsc#1267426 / CVE-2026-44941), and
  short (< 16 hex) ids are skipped ("too short to safely identify a gpg key", bsc#1271730). A buddy that is already
  trusted, missing from the general ring, or equal to the signing key is skipped.
* **See**: [06 §13](06-security-gpg-keyring.md).

### (c) Buddy mapping in the solver and in `sat::Transaction`

* **SATResolver `mapBuddy` / `mapItem` / `mapSolvable`** (`zypp/zypp/solver/detail/SATResolver.cc` ~l.1074): when
  building problem texts, a **package** that has a buddy is replaced by the buddy (the product), so the user reads
  "product foo conflicts with product bar" instead of "package foo-release conflicts with …". Only packages map to
  buddies, never vice versa.
* **`sat::Transaction` `_linkMap`** (`zypp/zypp/sat/Transaction.cc`): non-package steps with a buddy follow the
  buddy's step stage (`STEP_DONE`/`STEP_ERROR`), because they share the status.

No other meanings of "buddy" were found (`grep -rni buddy zypp zypp-logic zyppng`).

---

## A.6 Repositories and services

### Repository (repo)

* **What**: a source of packages plus metadata describing them (an HTTP directory, a DVD, an ISO, a local dir).
* **Why**: the solver needs to know *all* available packages and their dependencies without downloading rpms.
* **In libzypp** – two different classes, don't mix them up:

  | Class | Meaning |
  |---|---|
  | `RepoInfo` (`zypp/zypp/RepoInfo.{h,cc}`, base `repo::RepoInfoBase`) | the **configuration**: one `[alias]` section of a `.repo` file (URLs, priority, flags) |
  | `Repository` (`zypp/zypp/Repository.{h,cc}`) | the **loaded** repo in the sat pool (a handle around libsolv's `Repo*`; `Repository::info()` gives the `RepoInfo`) |
  | `RepoManager` (`zypp/zypp/RepoManager.h`, thin wrapper around `zyppng::RepoManager` in `zypp/zypp/ng/`) | add/remove/refresh/build cache/load |

* **Gotcha**: `RepoManager` methods only change files and caches. To use a repo you must `loadFromCache()` it into the pool.
* **See**: [05](05-repositories-and-services.md).

### Alias vs name

* **alias**: the unique, stable id (`[repo-oss]`). Used as file name (`/etc/zypp/repos.d/<alias>.repo`), cache dir name
  (`escaped_alias()`: `/` → `_`), history log key, lock/testcase references. May not be empty or start with `.`
  (bnc#473834).
* **name**: free-text label (may contain repo variables). `RepoInfoBase::label()` returns name or alias depending on a
  `ZConfig` setting.
* **Gotcha**: service repos get the alias `<service>:<alias>`.

### baseurl, mirrorlist, metalink

* **baseurl**: one or more URLs where the repo lives (`baseurl=` in `.repo`; `RepoInfo::baseUrls()` expanded,
  `rawBaseUrls()` as configured).
* **mirrorlist** / **metalink**: a URL returning a list of mirrors (plain list, or the metalink XML format).
  `RepoInfo::mirrorListUrl()` = `mirrorlist=` if valid, else `metalink=`. Parsed by `repo::RepoMirrorList`
  (`zypp/zypp/repo/RepoMirrorList.cc`), cached on disk (`mirrorlist` + cookie in the raw cache) and in memory (1 h).
* **Auto mirrorlist**: for a single baseurl on `download.opensuse.org`/`cdn.opensuse.org`, libzypp asks for
  `<baseurl>/<path>/?mirrorlist`; failures are silent (bsc#1243901).
* **Gotcha**: baseurls are *authorities*, mirrors are just extra endpoints. Security relevant files (repomd.xml, keys,
  signatures) are only fetched from authorities (`OnMediaLocation::mirrorsAllowed()`, 17.37.6). Metalink is only used
  as a mirror source today; the parallel metalink downloader (`MediaMultiCurl`) was dropped in 17.37.0.
* **See**: [05 §3.3, §11](05-repositories-and-services.md), [07 §7.5](07-media-and-downloads.md).

### Repo type and master index

| `RepoType` (`zypp/zypp/repo/RepoType.{h,cc}`) | Accepted strings | Master index | Other files |
|---|---|---|---|
| `RPMMD` | `rpm-md`, `rpm`, `rpmmd`, `repomd`, `yum`, `up2date` | `repodata/repomd.xml` (+ `.asc`, `.key`) | `primary`, `filelists`, `updateinfo`, `deltainfo`, `appdata`, `susedata` … (xml.gz/zst/zck) |
| `YAST2` | `yast2`, `yast`, `susetags` | `content` (+ `.asc`, `.key`) | `media.1/media`, `suse/setup/descr/packages*`, `patterns` … |
| `RPMPLAINDIR` | `plaindir` | none – a directory of rpms; local URLs only | the rpms themselves (`repo2solv -R`) |
| `NONE` | `N/A`, `none` | unknown → probed | |

* **Master index**: the one small file that references (by checksum) all other metadata files. It is what gets GPG-signed.
  Its signature transitively secures everything else.
* **Gotcha**: `type=` in `.repo` files is **ignored on read** (bsc#1177427) – RepoManager probes. Plaindir is only probed
  for local, non-downloading URLs (bsc#1191286).
* **See**: [05 §5.4–5.5, §12](05-repositories-and-services.md).

### Raw cache vs solv cache

```
download ─► /var/cache/zypp/raw/<alias>/      raw metadata as on the server (+ cookie, .repo_gpgcheck symlink)
repo2solv ─► /var/cache/zypp/solv/<alias>/solv libsolv binary file (+ cookie = RepoStatus of the raw data, solv.idx)
loadFromCache ─► sat::Pool::addRepoSolv()
```

* **Why two**: parsing XML is slow; libsolv's `.solv` loads in milliseconds. The raw cache is kept so the solv can be
  rebuilt (e.g. after a libsolv update) without network.
* **Gotcha**: a solv file whose `repositoryToolVersion` differs from `LIBSOLV_TOOLVERSION` is rebuilt automatically.
  `RepoStatus` has a magic suffix: changing it "will at once invalidate all solv file caches".
* **See**: [05 §2.3, §5.6](05-repositories-and-services.md).

### The `@System` repo

* **What**: the pseudo-repository holding the **installed** packages, built from the rpm database (plus
  `/etc/products.d`) by `rpmdb2solv`. libsolv knows it as the "installed" repo (`pool_set_installed`).
* **In libzypp**: `sat::Pool::systemRepo()`, `Solvable::isSystem()`; built by `target::TargetImpl::buildCache()` into
  `<root>/var/cache/zypp/solv/@System/solv`, rebuilt when the rpmdb cookie changes.
* **Gotcha**: the system repo is exempt from the arch filter. Repo-only data (support keywords, retracted marker) is
  matched to installed packages by identity.
* **See**: [03 §5.3](03-sat-pool-and-resolvables.md), [08 §3](08-target-commit-and-rpm.md).

### Priority, autorefresh, keeppackages, enabled

| Setting | Default | Meaning |
|---|---|---|
| `priority` | 99 | 1 = highest. Mapped to libsolv `repo->priority = -priority`: the solver prefers higher-priority repos *before* comparing versions. `subpriority` from `MediaPriority(url)` breaks ties (prefer http over DVD). |
| `enabled` | true | disabled repos are not refreshed or loaded |
| `autorefresh` | false | refresh metadata automatically before use – but at most every `repo.refresh.delay` minutes (default 10) |
| `keeppackages` | false | keep downloaded rpms in `/var/cache/zypp/packages/<alias>` (`effectiveKeepPackages()` also honours a `.keep_packages` marker file) |

* **Gotcha**: priority beats version: a newer package in a priority-99 repo loses against an older one in priority 90
  for new installs/updates (that is the purpose).

### Repo variables

* **What**: `$arch`, `$basearch`, `$releasever`, `$releasever_major`, `$releasever_minor`, plus custom variables from
  `/etc/zypp/vars.d/<name>`, usable in URLs and names (`${var:-word}`, `${var:+word}` supported).
* **In libzypp**: `repo::RepoVarExpand`, `RepoVariablesStringReplacer`, `RepoVariablesUrlReplacer`
  (`zypp/zypp/repo/RepoVariables.{h,cc}`).
* **Gotcha**: `$releasever` order: env `ZYPP_REPO_RELEASEVER` (zypper `--releasever`) → `vars.d` → base product version
  (re-evaluated on every lookup, bnc#943563). Unknown variables are left as is. Config writers must use the `raw*()`
  getters, or the expanded value gets saved.
* **See**: [05 §6](05-repositories-and-services.md).

### RepoStatus and cookies

* **What**: a fingerprint (checksum + timestamp) of metadata, stored as one line `"<checksum> <time_t>"` in `cookie`
  files.
* **Why**: decides "do I need to refresh/rebuild?" by comparing the server's master index status with the raw cache, and
  the raw cache status with the solv cache.
* **In libzypp**: `RepoStatus` (`zypp/zypp/RepoStatus.{h,cc}`). `RepoStatus(RepoInfo)` hashes the repo URL string, so
  changing the URL forces a refresh.
* **See**: [05 §5.2–5.3](05-repositories-and-services.md).

### Services (RIS, plugin services)

* **What**: a *service* is a URL or script that manages a **list of repositories**. RepoManager refreshes the service
  and adds/removes/modifies the repos it lists.
* **Why**: SUSE registration (SCC/RMT/SMT) gives each system a personalized set of repos with credentials; the service
  keeps that set up to date when modules are added or products upgraded.
* **In libzypp**: `ServiceInfo` (`zypp/zypp/ServiceInfo.{h,cc}`), files in `/etc/zypp/services.d/*.service`.
  `repo::ServiceType`:
  * **RIS** (Repository Index Service): `<url>/repo/repoindex.xml` lists repos (`RepoindexFileReader`), with `ttl`.
  * **PLUGIN**: an executable in `/usr/lib/zypp/plugins/services/` prints a `.repo` file on stdout. Immutable via API.
* **Service-managed repos**: alias `<service>:<alias>`, `RepoInfo::service()` set; the service file remembers
  `repostoenable`/`repostodisable` and per-repo `RepoStates` so a user's enable/disable choice survives refreshes.
  Repos are filtered by `targetDistribution` (product register target).
* **Gotcha**: changing a service's enabled state cascades to all its repos. Parse errors become
  `ServicePluginInformalException` (callers expect that type).
* **See**: [05 §7](05-repositories-and-services.md).

---

## A.7 Pool and status

### sat::Pool vs ResPool

| | `sat::Pool` (`zypp/zypp/sat/Pool.h`) | `ResPool` (`zypp/zypp/ResPool.h`) |
|---|---|---|
| Wraps | the libsolv `Pool` (solvables, repos, whatprovides, locales, multiversion, autoinstalled) | the `PoolItem` store (`pool::PoolImpl`) on top |
| Elements | `sat::Solvable`, `Repository` | `PoolItem`, `ui::Selectable` (via `proxy()`) |
| State | data only | `ResStatus` per item, hard lock queries, established states |
| Typical use | lookups, loading repos | selecting/solving/committing |

Both are handles to process-wide singletons (`ResPool::instance()`). There is exactly **one** libsolv pool per process.

### ResStatus

* **What**: a 16-bit bitfield per `PoolItem` (`zypp/zypp/ResStatus.h`) with fields: State (installed/uninstalled –
  immutable), Validate (patch status), **Transact** (`KEEP_STATE` / `LOCKED` / `TRANSACT`), **TransactBy** (causer),
  TransactDetail, LicenceConfirmed, Weak bits (recommended/suggested/orphaned/unneeded), UserLockQuery.
* **Why**: the application, the UI and the solver all edit the same "what will happen" state. The bitfield records
  *what* should happen and *who* asked for it.
* **Meaning**: `isToBeInstalled() = uninstalled && TRANSACT`; `isToBeUninstalled() = installed && TRANSACT`;
  `onSystem() = installed != transacts`.

### Causer levels (USER, APPL_HIGH, APPL_LOW, SOLVER)

* **What**: `ResStatus::TransactByValue`, ordered `SOLVER < APPL_LOW < APPL_HIGH < USER`.
* **Why**: a lower causer may not override a higher one's decision. The solver must not undo what the user selected;
  an application can mark things "on behalf of the user" (APPL_HIGH) or as mere suggestions (APPL_LOW) that the next
  solver run may recompute.
* **Rules**: changing out of a non-KEEP state needs a causer ≥ the current one. Locks may only be set by `USER` or
  `APPL_HIGH`. On each solver run, every transaction by `SOLVER` or `APPL_LOW` is **reset**; only `APPL_HIGH`/`USER`
  transactions become solver jobs (`SATCollectTransact`). The history log's "requested-by" column depends on the causer
  (USER/APPL_LOW → `user@host`, APPL_HIGH → `pid:exe`, SOLVER → empty).
* **Gotcha**: the YaST selector depends heavily on these semantics (USER vs APPL_*). Unlocking resets the causer to
  `SOLVER` so "unlocked" can be told from "kept by user".
* **See**: [03 §7.3](03-sat-pool-and-resolvables.md), [04 §3.2](04-solver.md).

### Locks: hard locks, soft locks, taboo

| Kind | Stored | ResStatus | Solver job | Notes |
|---|---|---|---|---|
| **Hard lock** (`zypper al`) | `/etc/zypp/locks` as serialized `PoolQuery`s (`Locks`, `zypp/zypp/Locks.{h,cc}`) | `LOCKED` by USER + UserLockQuery bit | installed: `SOLVER_INSTALL\|SOLVABLE` (keep exactly this); not installed: `SOLVER_ERASE\|SOLVABLE` (never install) | applied by `ResPool::setHardLockQueries`; re-applied to new items; UI changes written back on commit as trivial name queries |
| **UI taboo / protected** | via hard locks | `ui::S_Taboo` (not installed) / `S_Protected` (installed) = USER lock | as above | USER only |
| **Soft lock** | not stored (bnc#863275) | `KEEP_STATE` by `APPL_HIGH`/`USER` | `SOLVER_ERASE\|SOLVABLE_NAME\|SOLVER_WEAK` once per name, only if no version is installed | "keep NOT installed name" (bnc#749418) |

* **Gotcha**: unlocking an item locked by a *complex* query cannot remove that query – the lock "reappears". A product
  and its release package lock together (buddy, bsc#1225267). Lock conflicts appear as problems with "remove lock …"
  solutions that really unlock.
* **See**: [03 §10, §13](03-sat-pool-and-resolvables.md), [04 §11](04-solver.md).

### License confirmed

* `ResStatus::isLicenceConfirmed()`/`setLicenceConfirmed()` – a per-item, **non-persistent** bit.
  `Solvable::licenseToConfirm()` returns the EULA (for products, possibly the repo license); `needToAcceptLicense()`.
  zypper `--auto-agree-with-licenses` sets it.
* **Gotcha**: bnc#908976 – no informational product license on update.

### Establish status (satisfied / broken / needed)

* The initial validate status of pseudo-installed items (patches) is computed by `solver::detail::establish()` whenever
  the pool content changes (`ResPool::EstablishedStates`), and recomputed after every solver run. Commit compares
  initial and final states to write `patch` lines to the history log (jsc#SLE-5116). See [patch](#patch).

### Selectable status (`ui::Status`)

`zypp/zypp/ui/Status.h`; computed and set by `ui::Selectable` (`zypp/zypp/ui/SelectableImpl.cc`):

| Status | Meaning |
|---|---|
| `S_Protected` | installed and locked (USER) – keep |
| `S_Taboo` | not installed and locked – never install |
| `S_Del` / `S_Update` / `S_Install` | user requested delete / update / install |
| `S_AutoDel` / `S_AutoUpdate` / `S_AutoInstall` | the same, decided by solver or application |
| `S_KeepInstalled` | installed, no change |
| `S_NoInst` | not installed, no change |

`Selectable::setStatus(status, causer)` translates these into `ResStatus` operations on the installed object and the
candidate. `fate()` and `modifiedBy()` summarise.

### Candidates

* **`candidateObj()`**: the available item that would be installed: the transacting one, else a user-set candidate
  (`setCandidate`), else `defaultCandidate()`.
* **`defaultCandidate()`**: if something is installed, prefer an available item with the same arch (noarch changes
  allowed) **and an equivalent vendor** (unless `allowVendorChange`); otherwise the first in `available()` order.
* **`updateCandidateObj()`**: additionally requires a strictly greater edition, vendor match, compatible arch; never a
  blacklisted item.
* **Available order** (`AVOrder`): non-blacklisted first → higher repo priority → better arch → higher edition → newer
  buildtime → subpriority.
* **picklist**: available items plus installed items without an identical available copy (version-selection dialogs).
* **Gotcha**: candidate choice depends on the solver's `allowVendorChange` flag (bnc#754286). `ui::Selectable` and
  `ResPoolProxy` are dropped on every pool change.
* **See**: [03 §12](03-sat-pool-and-resolvables.md).

---

## A.8 Solver concepts

### Jobs and requests

* **What**: libsolv solves a list of *jobs* (`SOLVER_INSTALL|SOLVER_SOLVABLE id`, `SOLVER_ERASE|SOLVER_SOLVABLE_NAME
  name`, `SOLVER_UPDATE|SOLVER_SOLVABLE_ALL`, …) under the dependency rules.
* **In libzypp**: the application expresses requests as `ResStatus` changes (`USER`/`APPL_HIGH`), extra
  `Resolver::addRequire(cap)` / `addConflict(cap)`, upgrade repos, mode flags, or `SolverQueueItem`s
  (`resolveQueue()`). `SATResolver::solverInit()` (`zypp/zypp/solver/detail/SATResolver.cc`) builds the job queue in a
  fixed order (vendor check, userinstalled, blacklist, locales, multiversion, PTF protection, SystemCheck, locks, mode
  jobs, pool jobs, extra requires, upgrade repos).
* **Gotcha**: problem solutions refer to job indexes, so the order matters.
* **See**: [04 §3](04-solver.md).

### Solve modes: solve, update, dist-upgrade, verify, patch

| Mode | API | What it does | zypper |
|---|---|---|---|
| solve | `Resolver::resolvePool()` | satisfy the requests in the pool | `in`, `rm`, `up <pkg>` |
| update | `Resolver::doUpdate()` | update all installed packages as far as possible; **ignores user requests, no problem reporting** | plain `up` |
| update mode | `setUpdateMode(true)` + `resolvePool()` | requests + `SOLVER_UPDATE\|SOLVABLE_ALL` | `patch --with-update` |
| dist-upgrade (DUP) | `Resolver::doUpgrade()` | `SOLVER_DISTUPGRADE\|SOLVABLE_ALL`: bring every package to the version in the repos (allows downgrade, name/arch change by default; vendor change off) + droplist pass | `dup` |
| upgrade repo | `addUpgradeRepo(repo)` + `resolvePool()` | DUP restricted to repos | `dup --from` |
| verify | `Resolver::verifySystem()` | `SOLVER_VERIFY\|SOLVABLE_ALL`: repair broken dependencies; resets all non-USER transactions | `verify` |
| patch | `resolvePool()` with patch items selected | install needed patches (conflicts force updates) | `patch` |

* **Gotcha**: `doUpdate`, upgrade mode and verify are **sticky** flags until reset. `doUpdate()` is a separate copy of
  the solve code. `Resolver::reset()` does not reset upgrade/update mode.
* **See**: [04 §5](04-solver.md).

### Focus

* `ResolverFocus` (`zypp-logic/zypp/ResolverFocus.h`), zypp.conf `solver.focus`, zypper `--focus`:
  `Job` (default – best version of requested packages), `Installed` (as few changes to installed packages as possible,
  libsolv `FOCUS_INSTALLED`), `Update` (also update the requested packages' dependencies, `FOCUS_BEST`). Since 17.15.0
  (bsc#1146415).

### Problems and solutions

* **What**: when the jobs can't be satisfied, libsolv returns *problems*, each with alternative *solutions* (sets of
  actions: "do not install X", "deinstallation of Y", "remove lock …", "break X by ignoring some of its dependencies").
* **In libzypp**: `ResolverProblem`, `ProblemSolution` (`zypp/zypp/ResolverProblem.h`, `ProblemSolution.h`),
  `SolutionAction`s; built by `SATResolver::problems()`, applied via `Resolver::applySolutions()`. Texts are translated
  copies of libsolv's rule texts; packages are mapped to their product buddy.
* **Gotcha**: "ignore dependencies" solutions add a `SOLVER_WEAKENDEPS` job on the next run. A "remove lock" solution
  really changes the USER lock and can be persisted.
* **See**: [04 §6, §16](04-solver.md).

### Vendor stickiness and vendor equivalence

* **What**: by default the solver will not replace an installed package with one from a different (non-equivalent)
  vendor. Vendors are grouped as equivalent in `/etc/zypp/vendors.d/*` (`[main] vendors = a,b,c`).
* **In libzypp**: `VendorAttr::equivalent()` used by libsolv through `pool_set_custom_vendorcheck`; switches
  `solver.allowVendorChange` (default false) and `solver.dupAllowVendorChange` (default **false**; was true in
  Code12). In DUP a non-installed solvable providing `dup-vendor-relax(suse)` enables a relaxed check (bsc#1182629).
* **Gotcha**: also affects candidate selection and patch relevance. Docs: `zypp/doc/autoinclude/SolverVendorChange.doc`.

### Multiversion (kernels)

* **What**: packages that may be installed in several versions at once (kernels, KMPs). Installing a new one does not
  remove the old one.
* **In libzypp**: zypp.conf `multiversion = provides:multiversion(kernel)` (+ `multiversion.d`), `ZConfig::multiversionSpec()`,
  `sat::Pool::multiversionList()`; libsolv `SOLVER_NOOBSOLETES` jobs; transaction step `TRANSACTION_MULTIINSTALL`
  (`rpm -i`). Old kernels are removed by `PurgeKernels` (`zypp/zypp/PurgeKernels.{h,cc}`, zypp.conf `multiversion.kernels`).
* **Gotcha**: `Package::changelog()` looks up by name only – for kernels you get *some* installed version's changelog.

### Autoinstalled, unneeded, orphaned, dropped

| Term | Meaning | Code |
|---|---|---|
| **autoinstalled** | installed as a dependency, not requested by a user | `/var/lib/zypp/AutoInstalled`, `sat::Pool::autoInstalled()`, `Solvable::onSystemByAuto()`; passed to libsolv as "userinstalled inverted" |
| **unneeded** | autoinstalled and no longer needed by anything user-installed | weak bit `ResStatus::isUnneeded()` from `solver_get_unneeded`; `Resolver::setRemoveUnneeded` adds weak erase jobs; zypper `packages --unneeded` lists them (`rm --clean-deps` is [cleandeps](#cleandeps), not this) |
| **orphaned** | installed, but no repo offers it any more | weak bit `isOrphaned()` from `solver_get_orphaned`; `setRemoveOrphaned` = `SOLVER_DROP_ORPHANED` (DUP only) |
| **dropped** | listed in a product's droplist (`weakremover`) | DUP second pass removes them if `solver.upgradeRemoveDroppedPackages` (default true) |

* **Gotcha**: 3rd-party rpms without a repo count as orphaned (put them in a plaindir repo to protect them). If the
  AutoInstalled file is missing it is bootstrapped from the history log ("requested-by" contains `@` = user).

### cleandeps

* `SOLVER_CLEANDEPS` on erase jobs also removes the packages that were only installed for the removed one.
  `Resolver::setCleandepsOnRemove()`, zypp.conf `solver.cleandepsOnRemove` (false). Removing a requested locale always
  uses cleandeps.

### Recommends, supplements and onlyRequires

* By default libsolv installs recommends of **newly installed** packages and packages whose supplements become true.
  `onlyRequires` (`--no-recommends`, `solver.onlyRequires`) sets `SOLVER_FLAG_ONLY_NAMESPACE_RECOMMENDED`: weak deps
  are ignored **except namespace ones** (language, modalias). `ignoreAlreadyRecommended=false` (`zypper inr`, `dup`)
  re-evaluates recommends of *all* installed packages.
* **Gotcha**: the public doc in `Resolver.h` claims language/modalias packages are not regarded with onlyRequires; the
  code does regard them. Recommended/suggested items show up as weak bits, not transactions.
* **See**: [04 §4.3, §18](04-solver.md).

### Requested locales and locale packages

* **What**: the languages the user wants (`/var/lib/zypp/RequestedLocales`). Translation packages supplement
  `namespace:language(xx)`.
* **In libzypp**: `sat::Pool::setRequestedLocales/addRequestedLocale`, `Locale` (`zypp-logic/zypp/Locale.h`, fallback
  `de_DE` → `de` → `en` → ""). Only *changes* create jobs (install providers for added, erase with cleandeps for removed).
* **Gotcha**: an empty set is deliberately different from `{en}` (bsc#1155678). `ZConfig::textLocale()` (translated
  summaries) is a different thing.

### PTF (Program Temporary Fix)

* **What**: a SUSE support hotfix: a **PTF master package** (provides `ptf()`) that requires specific
  **PTF packages** (provide `ptf-package()`), replacing official versions until the fix is released.
* **In libzypp**: `Solvable::isPtf()`, `isPtfMaster()`, `isPtfPackage()`; masters are blacklisted (not picked unless
  requested); installed masters are protected when only `@System` is loaded (bsc#1203248); problem texts carry a PTF
  hint (bsc#1194848, "use `zypper removeptf`").

### Retracted and blacklisted

* **Retracted**: a package/patch withdrawn by the vendor after release. Packages: provide `retracted-patch-package()`
  (from repo metadata, so installed copies match by identity). Patches: `updateStatus == "retracted"`.
* **Blacklisted** = `isPtf() || isRetracted()`: libsolv `SOLVER_BLACKLIST`, sorted last and skipped as candidates, but
  installable on explicit request (`SOLVER_SOLUTION_BLACK` → "install … although it has been retracted").

### SystemCheck

* Extra requirements that must always hold: `/etc/zypp/systemCheck` and `/etc/zypp/systemCheck.d/*.check`, lines
  `requires:<cap>` / `conflicts:<cap>` (shipped: `requires:glibc`). `solver::detail::SystemCheck`.
  Violations read "This request will break your system!". Loaded once per process (not reloaded on `--root` change).

### Solver testcase

* **What**: a directory with the complete solver input (repos as `.repo.gz` solv dumps, `testcase.t`,
  `zypp-control.yaml` with setup and trials, locks, flags) that reproduces a solver run elsewhere.
* **In libzypp**: writer `solver::detail::Testcase` (`zypp/zypp/solver/detail/Testcase.cc`); reader
  `misc::LoadTestcase` / `TestcaseSetup`; runner `zypp-solver-testcase-runner` (zypp-stack), libsolv `testsolv`.
  Created by `zypper --debug-solver` (`/var/log/zypper.solverTestCase`), `ZYPP_FULLLOG=1`
  (`/var/log/YaST2/autoTestcase`), and each DUP commit (`/var/log/updateTestcase-*`).
* **Gotcha**: `removeOrphaned`, `removeUnneeded`, `solveSrcPackages` and weak items are not recorded.
* **See**: [04 §9, §17](04-solver.md), [11](11-testing.md).

---

## A.9 Target and commit

### Target, root, installroot

* **Target**: the system being managed – its rpmdb, `@System` solv, `/etc/products.d`, `/var/lib/zypp` state.
  `Target` / `target::TargetImpl` (`zypp/zypp/Target.h`, `zypp/zypp/target/TargetImpl.{h,cc}`), created by
  `ZYpp::initializeTarget(root)`.
* **root**: the directory the target lives under (`/` normally). zypper `--root DIR`: rpm runs with `--root`, and
  repos.d/caches are taken from inside the root.
* **installroot**: zypper `--installroot DIR` = like `--root` but **shares the host's repositories**
  (`ZConfig::setRepoManagerRoot("/")`, `zypper/src/Config.cc`). So `ZConfig::systemRoot()` and
  `ZConfig::repoManagerRoot()` can differ.
* **Gotcha**: the global zypp lock is **not** placed inside the root (`/run/zypp.pid` of the host unless
  `ZYPP_LOCKFILE_ROOT`). Commit plugins only run for root `/` (or `$TRANSACTIONAL_UPDATE`).
* **See**: [08 §1–3, §11](08-target-commit-and-rpm.md).

### rpmdb

* rpm's database of installed packages (location via `RpmDb::initDatabase`, `librpmDb`). Source of truth for "what is
  installed" and for **trusted GPG keys** (`gpg-pubkey-*` pseudo packages). libzypp reads it via `rpmdb2solv` into
  `@System` and via `librpmDb` iterators for headers.

### Commit

* **What**: carrying out the solver result: download, verify, install/remove with rpm, run scripts, update state files.
* **In libzypp**: `ZYpp::commit(ZYppCommitPolicy)` → `TargetImpl::commit` → `ZYppCommitResult`. The step list comes
  from `sat::Transaction` built from the pool's `ResStatus` (not from the raw solver result; bnc#702576).
* **Transaction steps** (`sat::Transaction::StepType`): `TRANSACTION_INSTALL` `[+]` (install/update `-U`),
  `TRANSACTION_ERASE` `[-]`, `TRANSACTION_MULTIINSTALL` `[M]` (`-i`), `TRANSACTION_IGNORE` `[ ]` (implicit erase by
  update/obsoletes, patches/patterns/products). Stages `STEP_TODO`/`STEP_DONE`/`STEP_ERROR`.
* **Order**: `Transaction::order()` → libsolv `transaction_order` (prerequires first, groups by medium).
* **See**: [03 §18](03-sat-pool-and-resolvables.md), [08 §6](08-target-commit-and-rpm.md).

### Classic vs single transaction (zypp-rpm)

| Backend | How | Pros / cons |
|---|---|---|
| **classic_rpmtrans** | one `rpm -U/-e --nodeps --force` per package, libzypp orders; `%posttrans` collected and run by libzypp (`RpmPostTransCollector`) | robust, supports download-as-needed; slow; scripts/triggers timing emulated |
| **single_rpmtrans** | helper `zypp-rpm` (`zypp/tools/zypp-rpm`) builds **one** librpm transaction | fast, native ordering/scriptlets; needs all packages downloaded first |

* Selection (`ZYppCommitPolicy::singleTransModeEnabled()`): on SUSE builds opt-in via `ZYPP_SINGLE_RPMTRANS`
  (Code15: zypper only); default on non-SUSE builds. Forced off with `DownloadAsNeeded` or a pending usrmerge.
* **See**: [08 §5](08-target-commit-and-rpm.md).

### Download modes

`DownloadMode` (`zypp-logic/zypp/DownloadMode.h`, zypp.conf `commit.downloadMode`, zypper `--download-*`):
`DownloadOnly` (download, install nothing), `DownloadInAdvance` (all first), `DownloadInHeaps` (currently treated like
in-advance), `DownloadAsNeeded` (one by one, classic backend only), `DownloadDefault` (→ InHeaps for `/`, else
InAdvance/AsNeeded). The commit preloader (`zypp/zypp/target/commitpackagepreloader*`) downloads in parallel.

### Delta rpm

* **What**: a `.drpm` holding only the differences between an installed version and the new rpm; the full rpm is
  rebuilt locally with `applydeltarpm`.
* **In libzypp**: `RpmPackageProvider` in `zypp/zypp/repo/PackageProvider.cc`, `repo::DeltaCandidates`,
  `zypp/zypp/repo/Applydeltarpm.*`; zypp.conf `download.use_deltarpm` (off upstream since 17.36.3, #620; distro spec may
  enable), `download.use_deltarpm.always`.
* **Gotcha**: no size heuristic; failure falls back to the full rpm.
* **See**: [05 §10.4](05-repositories-and-services.md).

### Package cache

* Downloaded rpms land in `/var/cache/zypp/packages/<alias>/`; kept if `keeppackages`, else deleted after commit.
  `Package::cachedLocation()` reuses a cached file only if its checksum matches.

### `%posttrans`, needreboot, update messages/scripts

* **`%posttrans`**: rpm scriptlets run at the end of a transaction; in classic mode libzypp emulates this
  (`--noposttrans`, `_dump_posttrans`, `rpm --runposttrans`).
* **needreboot**: packages matching `/etc/zypp/needreboot(.d)` or providing `installhint(reboot-needed)` / `kernel` →
  after install, `/run/reboot-needed` is touched. `Solvable::isNeedreboot()`.
* **Update scripts/messages**: files `/var/adm/update-scripts/<n-v-r>*` and `/var/adm/update-messages/<n-v-r>*`
  shipped by packages; scripts run, messages collected into `ZYppCommitResult::updateMessages()` and the
  `update.messages.notify` command.
* **See**: [08 §6–7](08-target-commit-and-rpm.md).

### History log

* `/var/log/zypp/history` (`HistoryLog`, `zypp/zypp/HistoryLog.{h,cc}`): `|`-separated machine-readable lines:
  `command`, `install`, `remove `, `radd   `, `rremove`, `ralias `, `rurl   `, `patch  `, `#` comments.
* **Gotcha**: the format is parsed by others and by libzypp itself (AutoInstalled bootstrap). Never change field
  positions or padded action tokens.

### zypp lock

* **What**: the global "one package manager at a time" lock: `/run/zypp.pid` containing the holder's pid. Taken by
  `getZYpp()` for root only; `zypp-rpm.pid` protects a running rpm transaction.
* **In libzypp**: `ZYppFactory` / `ZYppGlobalLock` (`zypp/zypp/ZYppFactory.cc`), `ZYPP_LOCK_TIMEOUT` / zypp.conf
  `lock_timeout` (0 = fail, >0 wait seconds, <0 forever), `ZYPP_READONLY_HACK`.
* **Gotcha**: non-root processes never lock. The `ZYpp::Impl` survives release/re-acquire within a process.
* **See**: [08 §2](08-target-commit-and-rpm.md).

### Plugins

All under `ZConfig::pluginsPath()` = `/usr/lib/zypp/plugins/`, using a STOMP-like frame protocol (`PluginFrame`,
`PluginScript`, `PluginExecutor`):

| Type | Purpose |
|---|---|
| `commit/` | notified at `COMMITBEGIN`/`COMMITEND` with the step list (JSON); e.g. snapper snapshots. Cannot veto. |
| `system/` | `PACKAGESETCHANGED` after the `@System` solv was rebuilt |
| `services/` | plugin services (print a repo list) |
| `urlresolver/` | resolve `plugin:NAME?…` URLs to real URLs + HTTP headers |
| `repoverification/` | stateless, run for every repo's master index before the gpg check; can **veto** the metadata (exit code) |
| `sigcheck/` | stateful (17.38.8), only for repos with `repo_sigcheck_plugin=`; missing plugin = metadata rejected |
| `appdata/` | notified after repo changes |

* **See**: [08 §9](08-target-commit-and-rpm.md), [06 §11](06-security-gpg-keyring.md).

---

## A.10 Trust

### General vs trusted keyring, gpg-pubkey in the rpmdb

* **What**: libzypp keeps two temporary gpg keyrings: **general** (keys it has *seen*: downloaded `repomd.xml.key`,
  `gpgkey=` URLs, keyhints, the pubkey cache) and **trusted** (keys the admin trusts). Only a signature by a trusted
  key counts as *validated*.
* **Why**: downloading a key from the same server as the data proves nothing. Trust must come from the admin (or from
  an already trusted key, see buddy keys).
* **In libzypp**: `KeyRing` (`zypp/zypp/KeyRing.{h,cc}`, impl `keyring_p.h`), `PublicKey` / `PublicKeyData`
  (`zypp-logic/zypp-common/PublicKey.h`). There is **no persistent zypp keyring**: the trusted ring is synced from/to
  the **rpmdb**, where trusted keys are `gpg-pubkey-<id8>-<created-hex>` pseudo packages (`RpmDb::syncTrustedKeys`).
  The general ring is preloaded from `/var/cache/zypp/pubkeys` and `/usr/lib/rpm/gnupg/keys`.
* **Gotcha**: `PublicKeyData::created()` is the date of the latest self-signature (so extending a key's expiry makes a
  new "release", bsc#1179222). Signatures may name a subkey id – lookups use `providesKey()`.
* **See**: [06 §2, §6](06-security-gpg-keyring.md).

### Key context

* `KeyContext` (`zypp/zypp/KeyContext.h`) – just the `RepoInfo` a key/signature belongs to, passed to `KeyRingReport`
  callbacks so the UI can say *which* repo asks. Empty if unknown.

### gpgcheck, repo_gpgcheck, pkg_gpgcheck, validRepoSignature

* **What**: three tri-state switches (zypp.conf `[main]`, overridable per repo): `gpgcheck` (master switch, default on),
  `repo_gpgcheck` (check the master index signature), `pkg_gpgcheck` (check rpm signatures).
* **Logic** (`RepoInfo.cc`): a **validated repo signature** secures the packages through the checksums in the signed
  metadata, so rpm signatures are only *enforced* when `gpgcheck` is on and the repo signature is **not** valid.
  `validRepoSignature()` is persisted as a symlink `<raw cache>/.repo_gpgcheck` → `true|false|indeterminate`.
* **Gotcha**: a repo-level `gpgcheck=` hides zypp.conf's `repo_`/`pkg_gpgcheck`. Since 17.38.15 `repo_gpgcheck` is
  followed strictly (bsc#1274625). CVE-2017-7435/7436/9269 drove the matrix.
* **See**: [06 §7](06-security-gpg-keyring.md).

### Accepted vs validated

* `VerifyFileContext` (`zypp/zypp/KeyRingContexts.h`): **`fileValidated()`** = the signature was actually verified;
  **`fileAccepted()`** = the file may be used, which can also be true "due to user interaction or global defaults"
  (unsigned file accepted, key trusted temporarily). **`signatureIdTrusted()`** = the signing key is in the trusted
  ring (not just temporarily trusted).
* `KeyRingReport` answers (`zypp/zypp/KeyRing.h`): `KEY_DONT_TRUST`, `KEY_TRUST_TEMPORARILY`, `KEY_TRUST_AND_IMPORT`;
  defaults via `KeyRing::setDefaultAccept()` (default `ACCEPT_NOTHING`).
* **Gotcha**: only `fileValidated` makes a repo signature valid; an accepted-but-unvalidated repo still requires
  package signatures.

### Keyhints

* `gpg-pubkey-<id>-<rel>.asc[?fpr=…]` keywords in `repomd.xml` (`RepomdFileReader::keyhints()`), see
  [buddy keys](#b-buddy-keys-gpg-keys-from-repo-keyhints).

### Checksums

* `CheckSum` (`zypp-logic/zypp-core/CheckSum.h`: md5, sha1, sha224, sha256, sha384, sha512). Every metadata file and rpm
  is checked against the checksum from the (signed) master index/primary. Missing or wrong checksum → `DigestReport`
  question (`askUserToAcceptNoDigest`, `askUserToAcceptWrongDigest`).

### Credentials

* User/password for repos: `?credentials=<name>` URL parameter → `/etc/zypp/credentials.d/<name>`; catalogs
  `/etc/zypp/credentials.cat` and `~/.zypp/credentials.cat`. `media::CredentialManager`
  (`zypp-logic/zypp-media/auth/credentialmanager.{h,cc}`). Lookup by URL prefix. SUSE registration writes these.
* **Gotcha**: URLs with embedded passwords must survive `.repo` writing (`hotfix1050625::asString`) but must be hidden
  in logs.
* **See**: [06 §9.2](06-security-gpg-keyring.md), [07 §7.16](07-media-and-downloads.md).

---

## A.11 Media

### Url schemes

* `Url` (`zypp-logic/zypp-core/Url.{h,cc}`). Schemes handled by `MediaHandlerFactory`: `http`, `https`, `ftp`, `tftp`
  (network, `MediaCurl2` default / `MediaCurl` on Code15), `cd`, `dvd` (`MediaCD`), `nfs`, `nfs4` (`MediaNFS`), `iso`
  (`MediaISO`, loop-mounts an ISO from another URL), `file`, `dir` (`MediaDIR`), `hd` (`MediaDISK`), `cifs`, `smb`
  (`MediaCIFS`), `plugin` (resolved by an urlresolver plugin).
* "Downloading" schemes (`Url::schemeIsDownloading()`) vs local ones matter for refresh, delta rpms and status
  computation. URL query options (`proxy=`, `ssl_verify=`, `credentials=`, `mediahandler=`, …) are documented in the
  doxygen header of `zypp/zypp/media/MediaManager.h`.

### MediaManager, handler, attach point

* **MediaManager** (`zypp/zypp/media/MediaManager.{h,cc}`): process-wide registry; `open(origin)` → `MediaAccessId`,
  `attach`, `provideFile`, `release`.
* **MediaHandler** (`zypp/zypp/media/MediaHandler.{h,cc}`): one per opened medium, implements attach/getFile.
* **Attach point**: a local directory where the medium's content is visible – a real mount point for CD/NFS/ISO/CIFS,
  or a download cache directory for network media (`/var/adm/mount/AP_0x…` or a temp dir).
* **Gotcha**: never declare a static `MediaManager`. Network `releasePath` deletes downloaded copies – callers must move
  or copy files away.
* **See**: [07 §7.3–7.4](07-media-and-downloads.md).

### Media sets and media number

* **What**: multi-CD/DVD products. Each medium has a number; repo metadata says which medium a package is on
  (`OnMediaLocation::medianr()`), and `media.1/media` (vendor, id, number of media) identifies the set.
* **In libzypp**: `MediaSetAccess` (`zypp/zypp/MediaSetAccess.{h,cc}`) maps medium number → `MediaAccessId`,
  rewrites URLs (`…/CD1` → `…/CD2`, `rewriteUrl`), asks the user to change media via `MediaChangeReport`;
  `repo::SUSEMediaVerifier` checks the right disc is inserted.
* **Gotcha**: without a connected `MediaChangeReport` receiver, errors are simply rethrown. `ZYppCommitPolicy::restrictToMedia`
  only honours medium 1.

### Mirrors vs authorities (MirroredOrigin)

* `OriginEndpoint` = URL + config; `MirroredOrigin` = one or more **authorities** (the configured baseurls, several
  since 17.38.6, bsc#1253193) plus mirrors; `MirroredOriginSet` = several origins (`zypp-logic/zypp-core/MirroredOrigin.{h,cc}`).
  `RepoInfo::repoOrigins()` builds them.
* Mirrors are tried first for normal files; failing mirrors are deprioritised; security-relevant files come only from
  authorities.
* **See**: [07 §7.5](07-media-and-downloads.md).

### Proxies

* From `/etc/sysconfig/proxy` (or libproxy) via `ProxyInfo`, or per URL with `proxy=`/`proxyport=` query parameters
  (`.repo` `proxy=` key). See [07 §7.8, §7.17](07-media-and-downloads.md).

### Workers (NG)

* In the NG tree, media access runs in separate **worker processes** (`zypp-media-http`, `-chksum`, `-copy`, `-dir`, `-disc`,
  `-disk`, `-iso`, `-nfs`, `-smb`, `-tvm`; sources in `zyppng/tools/`, installed under `${libexecdir}/zyppng/workers`) driven by the async `zyppng::Provide`. **Legacy libzypp never
  spawns workers** – it uses `MediaManager` in-process. A legacy class also called `zyppng::Provide`
  (`zypp/zypp/ng/media/provide.{h,cc}`) is just a sync facade over `MediaManager`.
* **See**: [07 §7.12](07-media-and-downloads.md), [10 §8, §14–15](10-zyppng-next-generation.md).

---

## A.12 Application integration

### Reports, callbacks, receivers

* **What**: libzypp's way to report progress and ask questions ("trust this key?", "insert disc 2", "retry
  download?"). Each report type has **one global receiver**.
* **In libzypp**: `callback::SendReport<T>` (library side), `callback::ReceiveReport<T>` (application side,
  `connect()`), `DistributeReport<T>` (`zypp/zypp/Callback.h`); report types in `zypp/zypp/ZYppCallbacks.h`
  (`DownloadProgressReport`, `MediaChangeReport`, `InstallResolvableReport`, `JobReport`, …), `KeyRingReport`
  (`KeyRing.h`), `DigestReport` (`Digest.h`).
* **Example**: zypper's receivers live in `zypper/src/callbacks/*.h`; YaST connects through yast2-pkg-bindings.
* **Gotcha**: **without a receiver every question gets its default answer** (often "abort" or "don't trust"). Adding a
  virtual to a report breaks ABI – extend through `report(const UserData&)` instead.
* **See**: [02 §11](02-core-infrastructure.md), [14 §2.6](14-consumers-and-api-surface.md).

### UserData

* `callback::UserData` (`zypp-logic/zypp-core/UserData.h`) – a typed (`ContentType`) key/value bag passed with
  reports; receivers must ignore types they don't know. Also `ZConfig::userData()` (zypper `--userdata`) – a string
  written to the history log and sent to plugins. Different things with the same name.

### ZConfig

* `ZConfig::instance()` (`zypp/zypp/ZConfig.{h,cc}`) – singleton holding `zypp.conf` settings, system arch, text
  locale, paths (`repoCachePath`, `pluginsPath`, …), solver defaults. Read from `/etc/zypp/zypp.conf`,
  `/usr/etc/zypp/zypp.conf` + `.d` drop-ins or `$ZYPP_CONF`.
* **Gotcha**: zypp.conf of `/` is parsed even for `--root`; only `solver.*` target defaults are re-read from the target
  (bsc#1196368).
* **See**: [09](09-configuration-and-environment.md).

### ZYpp singleton and getZYpp

* `zypp::getZYpp()` = `ZYppFactory::instance().getZYpp()` (`zypp/zypp/ZYppFactory.h`) returns the `ZYpp::Ptr` that
  gives access to `pool()`, `resolver()`, `target()`, `keyRing()`, `commit()`. Acquiring it takes the zypp lock;
  releasing the last reference releases it.

### Logging

* Macros `DBG`, `MIL`, `WAR`, `ERR`, `SEC`, `INT`, `USR`, `XXX` (`zypp-logic/zypp-core/base/Logger.h`); log groups via
  `ZYPP_BASE_LOGGER_LOGGROUP`. Applications set the file (`zypp::base::LogControl::instance().logfile(...)`); zypper
  logs to `/var/log/zypper.log`, YaST to `y2log`. `ZYPP_FULLLOG=1` for everything, `ZYPP_LOGFILE`.
* **Gotcha**: `DBG` lines are logged as MIL in group `<group>++`.
* **See**: [02 §3](02-core-infrastructure.md).

### Exceptions

* `zypp::Exception` (`zypp-logic/zypp-core/base/Exception.h`) with a message and a *history*; always thrown with
  `ZYPP_THROW`, logged as `ZYPP_CAUGHT`, rethrown with `ZYPP_RETHROW` (all logged at INT level with source location).
  Subsystem families: `MediaException`, `RepoException`, `KeyRingException`, `ResolverException`, `ZYppFactoryException`,
  `UserRequestException` (ABORT/RETRY/SKIP/IGNORE carried out of callbacks), …
* **Gotcha**: NG code must use `expected<T,E>` for data-dependent errors, not new exception paths (AGENTS.md).
* **See**: [02 §4](02-core-infrastructure.md).

---

## A.13 Codebase concepts

### The three tiers

| Tier | Dir | Rule |
|---|---|---|
| 3 – Legacy | `zypp/` | `libzypp.so`; C++17 impl, C++11 public headers; **frozen**, ABI-stable, bug fixes only |
| 2 – Shared bridge | `zypp-logic/` | compiled into both legacy and NG; frozen **except `*/ng/` subtrees** |
| 1 – Next-gen | `zyppng/` | C++20 named module `zyppng`, only place for new logic; libzypp-glib, media workers |

Dependency direction: `zypp → zypp-logic ← zyppng`; never `zypp ↔ zyppng`. See [00](00-big-picture.md),
[10](10-zyppng-next-generation.md).

### LEGACY-REQUIRED

* A `// LEGACY-REQUIRED` comment at the top of a header in `zypp-logic/*/ng/` marks a class that legacy still needs;
  it must stay until that dependency is gone (e.g. `zypp-logic/zypp/ng/sat/stringpool.h`). Others are moved to
  `zyppng/` wholesale.

### `LEGACY(n)` macros

* `#if LEGACY(1735)` (`zypp-logic/zypp-core/Globals.h`): code compiled only while the soname/major/version is ≤ n –
  deprecated API that **disappears automatically at the next soname bump**. Always 0 in NG builds.
* **See**: [12 §6](12-release-packaging-and-compat.md).

### ZYPP_API and libzypp.map

* `-fvisibility=hidden` + `ZYPP_API` (`Globals.h`) marks exported declarations; the linker version script
  `zypp/zypp/libzypp.map` additionally exports only `zypp::*`. `ZYPP_LOCAL` hides. Tests link the static
  `zypp-allsym`, so a passing test does not prove a symbol is exported.
* **Gotcha**: since 17.33.0 (jsc#PED-8153) several releases had to re-export symbols consumers needed.
* **See**: [12 §5](12-release-packaging-and-compat.md).

### PIMPL, RW_pointer, RWCOW_pointer

* Most public classes keep data in a private `Impl` behind `RW_pointer` (shared, const-correct) or `RWCOW_pointer`
  (copy-on-write; `zypp-logic/zypp-core/base/PtrTypes.h`) so the ABI stays stable when members change.
  `zypp::shared_ptr` is **boost's**.
* **Gotcha**: non-const access to an `RWCOW_pointer` clones when shared, even for reads. `base::ReferenceCounted` is not
  thread-safe.
* **See**: [02 §5](02-core-infrastructure.md).

### Sync/async from the same source (MaybeAwaitable)

* Workflow code in `zypp/zypp/ng/` (repo refresh, keyring, services) is written once against `MaybeAwaitable<T>`,
  `zypp_co_await`, `zypp_co_return`, `operator|`. In legacy these expand to plain synchronous calls; in NG to C++20
  coroutines (`Task<T>`). `Context::isAsync` selects the tier; `ReportHelper` routes user questions to classic reports
  in sync mode.
* **Gotcha**: `LogicBase`/`AsyncOp`/`SyncContext` in `logichelpers.h` are dead remnants – don't model new code on them.
* **See**: [10 §4, §17](10-zyppng-next-generation.md).

---

# Part B – Alphabetical glossary

Bug and feature tracker prefixes are listed too, because they show up in code comments and `zypp/package/libzypp.changes`.
"Ch." links point to the chapter with the most detail.

| Term | Meaning | Code anchor | Ch. |
|---|---|---|---|
| `@System` | Pseudo-repo holding the installed packages, built from the rpmdb | `sat::Pool::systemRepo()`, `TargetImpl::buildCache()` | [03](03-sat-pool-and-resolvables.md), [08](08-target-commit-and-rpm.md) |
| AGENTS.md | Source-root file defining the tier rules (legacy frozen, new code in `zyppng/`) | `AGENTS.md` | [00](00-big-picture.md) |
| alias | Unique, stable id of a repo or service; used for file and cache names | `RepoInfoBase::alias()`, `escaped_alias()` | [05](05-repositories-and-services.md) |
| allowVendorChange | Solver flag allowing vendor switches on update | `Resolver::setAllowVendorChange`, `solver.allowVendorChange` | [04](04-solver.md) |
| APPL_HIGH / APPL_LOW | Application causer levels between SOLVER and USER | `ResStatus::TransactByValue` | [03](03-sat-pool-and-resolvables.md) |
| appdata | AppStream metadata. Becomes `application:` solvables; appdata plugins are notified after repo changes | `Application`, `plugins/appdata/` | [03](03-sat-pool-and-resolvables.md), [05](05-repositories-and-services.md) |
| application | Resolvable kind for AppStream components | `zypp/zypp/Application.h` | [03](03-sat-pool-and-resolvables.md) |
| applydeltarpm | External tool that rebuilds an rpm from a delta rpm | `zypp/zypp/repo/Applydeltarpm.{h,cc}` | [05](05-repositories-and-services.md) |
| Arch | Package architecture with a compatibility table | `zypp-logic/zypp/Arch.{h,cc}` | [03](03-sat-pool-and-resolvables.md) |
| attach point | Local dir where a medium's content appears (a mount point or a download dir) | `MediaHandler`, `createAttachPoint()` | [07](07-media-and-downloads.md) |
| authority | A configured baseurl endpoint of a `MirroredOrigin`, as opposed to a mirror | `zypp-logic/zypp-core/MirroredOrigin.h` | [07](07-media-and-downloads.md) |
| autoinstalled | Package installed as a dependency, not requested by the user | `/var/lib/zypp/AutoInstalled`, `sat::Pool::autoInstalled()` | [04](04-solver.md) |
| autopattern | Pattern solvable generated (`-X`) from a pattern package's `pattern()` provides | `Pattern::isAutoPattern()`, `autoPackage()` | [03](03-sat-pool-and-resolvables.md) |
| autorefresh | Refresh repo metadata automatically, at most every `repo.refresh.delay` minutes | `RepoInfoBase::autorefresh()` | [05](05-repositories-and-services.md) |
| base product | The product `/etc/products.d/baseproduct` points to | `Product::isTargetDistribution()`, `TargetImpl::baseProduct()` | [08](08-target-commit-and-rpm.md) |
| baseurl | URL(s) of a repo | `RepoInfo::baseUrls()` / `rawBaseUrls()` | [05](05-repositories-and-services.md) |
| blacklisted | Retracted or PTF: never picked automatically | `Solvable::isBlacklisted()`, `SOLVER_BLACKLIST` | [04](04-solver.md) |
| bnc# | Novell Bugzilla (bugzilla.novell.com), the historic SUSE tracker | comments, changelog | – |
| boo# | openSUSE Bugzilla (bugzilla.opensuse.org) | comments, changelog | – |
| broken | Patch status: relevant but not satisfied | `ResStatus::isBroken()` | [04](04-solver.md) |
| bsc# | SUSE Bugzilla (bugzilla.suse.com) | comments, changelog | – |
| buddy (PoolItem) | Product sharing its `ResStatus` with its release package | `PoolItem::buddy()` | [A.5](#a5-buddy--all-meanings) |
| buddy keys | Keyhint keys imported as trusted if the metadata validates with a trusted key | `VerifyFileContext::buddyKeys()` | [06](06-security-gpg-keyring.md) |
| candidate | Available item that would be installed for a Selectable | `ui::Selectable::candidateObj()` | [03](03-sat-pool-and-resolvables.md) |
| Capability | One dependency expression (libsolv dep Id) | `zypp/zypp/Capability.{h,cc}` | [03](03-sat-pool-and-resolvables.md) |
| CapDetail | Decomposes a Capability into name/op/edition/expression | `Capability.h` | [03](03-sat-pool-and-resolvables.md) |
| causer | Who requested a status change (USER > APPL_HIGH > APPL_LOW > SOLVER) | `ResStatus` | [03](03-sat-pool-and-resolvables.md) |
| CheckSum | md5/sha1/sha224/sha256/sha384/sha512 value | `zypp-logic/zypp-core/CheckSum.h` | [06](06-security-gpg-keyring.md) |
| classic_rpmtrans | Commit backend: one rpm call per package | `TargetImpl::commit(policy, cache, result)` | [08](08-target-commit-and-rpm.md) |
| cleandeps | Also remove dependencies that are no longer needed when erasing | `SOLVER_CLEANDEPS`, `solver.cleandepsOnRemove` | [04](04-solver.md) |
| Code12/15/16 | SLE generations (12, 15, 16) and their openSUSE siblings; select build defaults | spec `suse_version`, `LIBZYPP_CODESTREAM` | [12](12-release-packaging-and-compat.md) |
| commit | Running the transaction: download, verify, rpm, scripts | `ZYpp::commit`, `TargetImpl::commit` | [08](08-target-commit-and-rpm.md) |
| commit plugin | Script notified at COMMITBEGIN/COMMITEND (e.g. snapper) | `plugins/commit/`, `PluginExecutor` | [08](08-target-commit-and-rpm.md) |
| COMPATMINOR | `VERSION.cmake` component that controls the soname | `zypp/VERSION.cmake` | [12](12-release-packaging-and-compat.md) |
| conflicts | Dependency type: must not be installed together | `Dep::CONFLICTS` | [A.3](#dependency-types) |
| content (file) | Master index of susetags (YAST2) repos | `parser/susetags/ContentFileReader` | [05](05-repositories-and-services.md) |
| cookie | File storing a `RepoStatus` (`"<checksum> <time>"`) | `RepoStatus::fromCookieFile` | [05](05-repositories-and-services.md) |
| CPE | Common Platform Enumeration id of a product (`cpe:/o:suse:sles:15:sp6`) | `zypp-logic/zypp/CpeId.{h,cc}` | [03](03-sat-pool-and-resolvables.md) |
| credentials | Repo user/password files (`credentials.d`, `credentials.cat`) | `media::CredentialManager` | [06](06-security-gpg-keyring.md) |
| CVE | Common Vulnerabilities and Exposures id. Patches reference them; libzypp's own CVEs are in the changelog | `Patch::ReferenceIterator` | [06 §17](06-security-gpg-keyring.md) |
| DBG/MIL/WAR/ERR | Logging macros | `zypp-logic/zypp-core/base/Logger.h` | [02](02-core-infrastructure.md) |
| delta rpm (drpm) | Binary diff between an installed and a new rpm | `RpmPackageProvider`, `DeltaCandidates` | [05](05-repositories-and-services.md) |
| deltainfo / prestodelta | rpm-md metadata listing the available delta rpms | `SolvAttr::repositoryDeltaInfo` | [05](05-repositories-and-services.md) |
| Dep | Enum of dependency types | `zypp-logic/zypp/Dep.h` | [A.3](#dependency-types) |
| deptestomatic | Solver testcase runner (`zypp-solver-testcase-runner`) | `ZYPP_API_DEPTESTOMATIC` | [11](11-testing.md) |
| dir: / file: | Local directory URL schemes | `MediaDIR` | [07](07-media-and-downloads.md) |
| directory.yast | Directory listing file needed for dir listings over HTTP | `MediaHandler::getDirInfo` | [07](07-media-and-downloads.md) |
| DistributeReport | Per-report-type singleton that connects sender and receiver | `zypp/zypp/Callback.h` | [02](02-core-infrastructure.md) |
| DownloadMode | DownloadOnly / InAdvance / InHeaps / AsNeeded / Default | `zypp-logic/zypp/DownloadMode.h` | [08](08-target-commit-and-rpm.md) |
| droplist | Packages dropped from a product (`weakremover`), removed in DUP | `Product::droplist()`, `SATResolver.cc` | [04](04-solver.md) |
| DUP | Distribution upgrade (`zypper dup`) | `Resolver::doUpgrade()` | [04](04-solver.md) |
| dupAllow* | DUP-specific solver flags (downgrade/namechange/archchange/vendorchange) | `solver.dupAllow*` | [04](04-solver.md) |
| Edition | `[epoch:]version[-release]` with rpm comparison | `zypp-logic/zypp/Edition.{h,cc}` | [03](03-sat-pool-and-resolvables.md) |
| enhances | Reverse suggests | `Dep::ENHANCES` | [A.3](#dependency-types) |
| epoch | Integer override in an edition | `Edition::epoch()` | [A.1](#edition-epochversion-release) |
| establish | Computing patch status (satisfied/broken/nonrelevant) without a request | `solver::detail::establish()`, `ResPool::EstablishedStates` | [04](04-solver.md) |
| EULA / license | License text a user must confirm | `Solvable::licenseToConfirm()`, `ResStatus::setLicenceConfirmed` | [03](03-sat-pool-and-resolvables.md) |
| Exception | libzypp exception base, with history | `zypp-logic/zypp-core/base/Exception.h` | [02](02-core-infrastructure.md) |
| expected<T,E> | NG error type for data-dependent failures | `zypp-logic/zypp-core/ng/pipelines` | [10](10-zyppng-next-generation.md) |
| Factory | openSUSE development project in OBS that Tumbleweed is built from | – | [12](12-release-packaging-and-compat.md) |
| fate# / FATE | Old SUSE feature tracker (replaced by Jira, `jsc#`) | comments, changelog | – |
| Fetcher | Batch downloader with checksum/signature checks | `zypp/zypp/Fetcher.{h,cc}` | [05](05-repositories-and-services.md) |
| flavor | Product variant (e.g. `dvd`, `ftp`). Pool view vs `<register><flavor>` | `Product::flavor()`, `Target::distributionFlavor()` | [08](08-target-commit-and-rpm.md) |
| focus | Solver strategy Job / Installed / Update | `zypp-logic/zypp/ResolverFocus.h` | [04](04-solver.md) |
| forceResolve | Allow the solver to uninstall packages to resolve conflicts | `Resolver::setForceResolve`, `SOLVER_FLAG_ALLOW_UNINSTALL` | [04](04-solver.md) |
| general keyring | Keys that were seen but are not trusted | `KeyRingImpl` `Ring::General` | [06](06-security-gpg-keyring.md) |
| GeoIP | Redirect downloads to a nearby mirror host (cached in `geoip.d`) | `MediaNetworkCommonHandler::findGeoIPRedirect` | [07](07-media-and-downloads.md) |
| getZYpp() | Get the ZYpp singleton; takes the zypp lock | `zypp/zypp/ZYppFactory.h` | [08](08-target-commit-and-rpm.md) |
| gpg-pubkey | rpmdb pseudo package holding a trusted key (`gpg-pubkey-<id8>-<rel>`) | `RpmDb::pubkeys()`, `PublicKeyData::rpmName()` | [06](06-security-gpg-keyring.md) |
| gpgcheck | Master switch for signature checking | `RepoInfo::gpgCheck()`, zypp.conf | [06](06-security-gpg-keyring.md) |
| hard lock | Persistent lock from `/etc/zypp/locks` | `Locks`, `ResPool::setHardLockQueries` | [03](03-sat-pool-and-resolvables.md) |
| Helix | Old XML solver testcase/repo format | `misc/HelixHelpers.h`, `addRepoHelix` | [03](03-sat-pool-and-resolvables.md) |
| history log | `/var/log/zypp/history`, machine-readable transaction log | `zypp/zypp/HistoryLog.{h,cc}` | [08](08-target-commit-and-rpm.md) |
| IBS | Internal Build Service, SUSE's internal OBS instance (SLE maintenance) | – | [12](12-release-packaging-and-compat.md) |
| ident | Kind-prefixed solvable name (`pattern:base`) | `Solvable::ident()`, `ResKind::satIdent` | [03](03-sat-pool-and-resolvables.md) |
| IdString | Handle to an interned string in the libsolv pool | `zypp-logic/zypp/IdString.{h,cc}` | [03](03-sat-pool-and-resolvables.md) |
| ignoreAlreadyRecommended | When false, re-evaluate recommends of all installed packages (`zypper inr`) | `Resolver::setIgnoreAlreadyRecommended` | [04](04-solver.md) |
| installroot | zypper `--installroot`: like `--root` but uses the host's repos | `ZConfig::setRepoManagerRoot("/")` | [A.9](#target-root-installroot) |
| interactive (patch) | Patch needs a reboot, a message, or a license confirmation | `Patch::interactiveFlags()` | [03](03-sat-pool-and-resolvables.md) |
| iso: | URL scheme that loop-mounts an ISO from another URL | `MediaISO` | [07](07-media-and-downloads.md) |
| JobReport | Generic message callback (info/warning/error) | `ZYppCallbacks.h` | [02](02-core-infrastructure.md) |
| jobs | libsolv requests (`SOLVER_INSTALL` …) built from pool state | `SATResolver::solverInit` | [04](04-solver.md) |
| jsc# | SUSE Jira (jira.suse.com) feature/epic ids (e.g. `jsc#PED-8153`) | comments, changelog | – |
| keeppackages | Keep downloaded rpms in the package cache | `RepoInfo::keepPackages()` | [05](05-repositories-and-services.md) |
| KeyContext | The RepoInfo a key question belongs to | `zypp/zypp/KeyContext.h` | [06](06-security-gpg-keyring.md) |
| keyhint | `gpg-pubkey-…` keyword in repomd.xml announcing extra keys | `RepomdFileReader::keyhints()` | [06](06-security-gpg-keyring.md) |
| KeyRing | General + trusted gpg keyrings and verification | `zypp/zypp/KeyRing.{h,cc}` | [06](06-security-gpg-keyring.md) |
| kind | Resolvable type (package, patch, …) | `zypp-logic/zypp/ResKind.h` | [03](03-sat-pool-and-resolvables.md) |
| KMP | Kernel Module Package. Usually multiversion; pulled in via modalias supplements | – | [04](04-solver.md) |
| LEGACY(n) | Compile-time guard: code that disappears at the next soname bump | `zypp-logic/zypp-core/Globals.h` | [12](12-release-packaging-and-compat.md) |
| LEGACY-REQUIRED | Tag on `*/ng/` headers that legacy still needs | header comment | [10](10-zyppng-next-generation.md) |
| libsolv | SAT solver and package pool library underneath libzypp | `libsolv/`, `sat::` | [03](03-sat-pool-and-resolvables.md) |
| libzypp-glib | Experimental GObject API on top of NG | `zyppng/api` | [10](10-zyppng-next-generation.md) |
| libzypp.map | Linker version script exporting only `zypp::*` | `zypp/zypp/libzypp.map` | [12](12-release-packaging-and-compat.md) |
| Locale | `lang[_COUNTRY]` with a fallback chain | `zypp-logic/zypp/Locale.h` | [03](03-sat-pool-and-resolvables.md) |
| lock (zypp) | Global process lock `/run/zypp.pid` | `ZYppFactory.cc` `ZYppGlobalLock` | [08](08-target-commit-and-rpm.md) |
| LookupAttr | Iterator over solvable/repo attributes (libsolv Dataiterator) | `zypp/zypp/sat/LookupAttr.h` | [03](03-sat-pool-and-resolvables.md) |
| master index | The signed top-level metadata file (`repomd.xml` / `content`) | `DownloadMasterIndexLogic` | [05](05-repositories-and-services.md) |
| MaybeAwaitable | Type that is plain `T` in legacy and a coroutine Task in NG | `ng/async/awaitable.h` | [10](10-zyppng-next-generation.md) |
| media.1/media | File identifying a medium set (vendor, id, count) | `repo::SUSEMediaVerifier` | [07](07-media-and-downloads.md) |
| MediaCurl / MediaCurl2 | HTTP/FTP handlers. MediaCurl2 is the default on Code16/TW | `zypp/zypp/media/` | [07](07-media-and-downloads.md) |
| MediaManager | Process-wide registry of opened media | `zypp/zypp/media/MediaManager.{h,cc}` | [07](07-media-and-downloads.md) |
| MediaSetAccess | Access to a multi-medium set, including media change | `zypp/zypp/MediaSetAccess.{h,cc}` | [07](07-media-and-downloads.md) |
| metalink | XML mirror list format (`metalink=`). Only used as a mirror source today | `RepoMirrorList` | [05](05-repositories-and-services.md) |
| mirrorlist | URL returning a list of mirrors | `RepoInfo::mirrorListUrl()` | [05](05-repositories-and-services.md) |
| MirroredOrigin | Authorities plus mirrors for one location | `zypp-logic/zypp-core/MirroredOrigin.h` | [07](07-media-and-downloads.md) |
| modalias | Kernel hardware id string (PCI/USB/DT/DMI devices, CPU vendor and features). `Supplements: modalias(…)` recommends drivers, firmware, KMPs, CPU-specific packages | `target::Modalias` | [03b](03b-modalias.md) |
| multiversion | Packages installable in parallel versions (kernels) | `ZConfig::multiversionSpec()`, `SOLVER_NOOBSOLETES` | [04](04-solver.md) |
| name | Display label of a repo, or a solvable name without its kind prefix | `RepoInfoBase::name()`, `Solvable::name()` | [05](05-repositories-and-services.md) |
| namespace | Dependency answered by a libzypp callback (language/modalias/filesystem) | `PoolImpl::nsCallback` | [03](03-sat-pool-and-resolvables.md) |
| needed | Patch to be installed, or broken and not locked | `PoolItem::isNeeded()` | [04](04-solver.md) |
| needreboot | Packages that trigger the reboot hint (`/run/reboot-needed`) | `Solvable::isNeedreboot()`, `/etc/zypp/needreboot` | [08](08-target-commit-and-rpm.md) |
| NEVRA | Name-Epoch-Version-Release-Arch identity of an rpm | `Solvable::asString()` | [A.1](#nevra-name-epoch-version-release-arch) |
| NG | Next-generation C++20 rewrite (`zyppng/`, `*/ng/`) | `zyppng/` | [10](10-zyppng-next-generation.md) |
| noarch | Architecture-independent packages, compatible with every arch | `Arch_noarch` | [03](03-sat-pool-and-resolvables.md) |
| nonrelevant | Patch status: none of its packages is installed | `ResStatus::isNonRelevant()` | [04](04-solver.md) |
| obsoletes | Dependency type: installing me removes X | `Dep::OBSOLETES` | [A.3](#dependency-types) |
| OBS | Open Build Service (build.opensuse.org) | – | [12](12-release-packaging-and-compat.md) |
| OnMediaLocation | File location on a medium: path, medium number, checksum, size, mirrorsAllowed | `zypp-logic/zypp-core/onmedialocation.h` | [07](07-media-and-downloads.md) |
| onlyRequires | Ignore weak deps except namespace ones (`--no-recommends`) | `Resolver::setOnlyRequires` | [04](04-solver.md) |
| orphaned | Installed but no longer offered by any repo | `ResStatus::isOrphaned()`, `SOLVER_DROP_ORPHANED` | [04](04-solver.md) |
| packageand() | Old SUSE "a and b" supplements syntax, rewritten by libsolv | libsolv `repo_fix_supplements` | [04](04-solver.md) |
| PackageKit | D-Bus package management daemon with a zypp backend | – | [14](14-consumers-and-api-surface.md) |
| PackageProvider | Downloads and verifies one package (incl. deltas, cache) | `zypp/zypp/repo/PackageProvider.{h,cc}` | [05](05-repositories-and-services.md) |
| patch | Update advisory resolvable from updateinfo | `zypp/zypp/Patch.{h,cc}` | [03](03-sat-pool-and-resolvables.md) |
| pattern | Package group for a use case | `zypp/zypp/Pattern.{h,cc}` | [03](03-sat-pool-and-resolvables.md) |
| picklist | Available items plus non-identical installed items of a Selectable | `ui::Selectable::picklist*` | [03](03-sat-pool-and-resolvables.md) |
| PIMPL | Private implementation pattern for ABI stability | `RW_pointer`, `RWCOW_pointer` | [02](02-core-infrastructure.md) |
| pkg_gpgcheck | Switch for rpm signature checks | `RepoInfo::pkgGpgCheck()` | [06](06-security-gpg-keyring.md) |
| plaindir | Repo type: plain directory of rpms without metadata | `RepoType::RPMPLAINDIR` | [05](05-repositories-and-services.md) |
| plugin: | URL scheme resolved by an urlresolver plugin | `UrlResolverPlugin` | [08](08-target-commit-and-rpm.md) |
| PluginFrame | STOMP-like message frame for plugins | `zypp-logic/zypp-core/rpc/PluginFrame.cc` | [08](08-target-commit-and-rpm.md) |
| PoolItem | Solvable + ResStatus (+ buddy) | `zypp/zypp/PoolItem.{h,cc}` | [03](03-sat-pool-and-resolvables.md) |
| PoolQuery | Attribute/dependency query; also the format of the locks file | `zypp/zypp/PoolQuery.{h,cc}` | [03](03-sat-pool-and-resolvables.md) |
| %posttrans | rpm end-of-transaction scriptlet; emulated in classic mode | `RpmPostTransCollector` | [08](08-target-commit-and-rpm.md) |
| prerequires | `Requires(pre)`: must be installed before me | `Dep::PREREQUIRES`, `solvablePrereqMarker` | [03](03-sat-pool-and-resolvables.md) |
| priority | Repo priority, 1 (highest) … 99 (default) | `RepoInfo::priority()` | [05](05-repositories-and-services.md) |
| problem / solution | Unsatisfiable request and the alternative fixes for it | `ResolverProblem`, `ProblemSolution` | [04](04-solver.md) |
| product | Distribution/module resolvable with a release package | `zypp/zypp/Product.{h,cc}` | [03](03-sat-pool-and-resolvables.md) |
| products.d | `/etc/products.d/*.prod` product files; `baseproduct` symlink | `parser::ProductFileReader` | [08](08-target-commit-and-rpm.md) |
| ProgressData | Progress value plus receiver callback | `zypp-logic/zypp-core/ui/progressdata.h` | [02](02-core-infrastructure.md) |
| provides | Dependency type: capabilities I offer | `Dep::PROVIDES` | [A.3](#dependency-types) |
| proxy | HTTP proxy from sysconfig/libproxy or the URL `proxy=` param | `ProxyInfo` | [07](07-media-and-downloads.md) |
| pseudo-installed | Kind whose "installed" state is computed, not stored (patch only) | `traits::isPseudoInstalled` | [04](04-solver.md) |
| PTF | Program Temporary Fix: support hotfix package set (`ptf()`, `ptf-package()`) | `Solvable::isPtf*()` | [04](04-solver.md) |
| PurgeKernels | Removes old multiversion kernels per `multiversion.kernels` | `zypp/zypp/PurgeKernels.{h,cc}` | [04](04-solver.md) |
| raw cache | Downloaded metadata as on the server (`/var/cache/zypp/raw/<alias>`) | `rawcache_path_for_repoinfo` | [05](05-repositories-and-services.md) |
| ReceiveReport | Application-side callback receiver | `zypp/zypp/Callback.h` | [02](02-core-infrastructure.md) |
| recommends | Weak forward dependency | `Dep::RECOMMENDS` | [04](04-solver.md) |
| register target | Product file `<register><target>` (e.g. `sle-15-x86_64`), used for service repo filtering | `Target::targetDistribution()` | [08](08-target-commit-and-rpm.md) |
| release package | rpm installing a product file (`*-release`); the product's buddy | `Product::referencePackage()` | [03](03-sat-pool-and-resolvables.md) |
| $releasever | Repo variable: distribution version | `RepoVariables.cc` | [05](05-repositories-and-services.md) |
| repo2solv | libsolv tool converting raw metadata to `.solv` | `BuildCacheLogic` | [05](05-repositories-and-services.md) |
| repo_gpgcheck | Switch for master index signature checks | `RepoInfo::repoGpgCheck()` | [06](06-security-gpg-keyring.md) |
| RepoInfo | Repo configuration (one `.repo` section) | `zypp/zypp/RepoInfo.{h,cc}` | [05](05-repositories-and-services.md) |
| repoindex.xml | RIS service repo list | `RepoindexFileReader` | [05](05-repositories-and-services.md) |
| RepoManager | Add/refresh/build/load repos and services | `zypp/zypp/RepoManager.h`, `zypp/zypp/ng/repomanager.*` | [05](05-repositories-and-services.md) |
| repomd.xml | rpm-md master index | `RepomdFileReader` | [05](05-repositories-and-services.md) |
| RepoStatus | Checksum + timestamp fingerprint of metadata | `zypp/zypp/RepoStatus.{h,cc}` | [05](05-repositories-and-services.md) |
| Repository | Loaded repo in the sat pool (`Repo*` handle) | `zypp/zypp/Repository.{h,cc}` | [03](03-sat-pool-and-resolvables.md) |
| repoverification | Plugin type that can veto repo metadata | `PluginRepoverification` | [06](06-security-gpg-keyring.md) |
| requested locales | Languages the user wants; drive `-lang` packages | `sat::Pool::setRequestedLocales`, `/var/lib/zypp/RequestedLocales` | [03](03-sat-pool-and-resolvables.md) |
| requires | Hard dependency | `Dep::REQUIRES` | [A.3](#dependency-types) |
| ResKind | Resolvable kind | `zypp-logic/zypp/ResKind.{h,cc}` | [03](03-sat-pool-and-resolvables.md) |
| ResObject | Typed, stateless accessor object for a solvable | `zypp/zypp/ResObject.{h,cc}` | [03](03-sat-pool-and-resolvables.md) |
| Resolvable | C++ base of ResObject. Colloquially: anything the solver handles | `zypp/zypp/Resolvable.h` | [03](03-sat-pool-and-resolvables.md) |
| Resolver | Solver front-end | `zypp/zypp/Resolver.h`, `solver/detail/Resolver.*`, `SATResolver.*` | [04](04-solver.md) |
| ResPool | PoolItem store on top of sat::Pool | `zypp/zypp/ResPool.h`, `pool/PoolImpl.h` | [03](03-sat-pool-and-resolvables.md) |
| ResPoolProxy | Groups the pool into Selectables | `zypp/zypp/ResPoolProxy.h` | [03](03-sat-pool-and-resolvables.md) |
| ResStatus | Per-item 16-bit transaction state | `zypp/zypp/ResStatus.h` | [03](03-sat-pool-and-resolvables.md) |
| restartSuggested | Patch affects the package manager ("update stack") | `Patch::restartSuggested()` | [04](04-solver.md) |
| retracted | Package/patch withdrawn after release | `Solvable::isRetracted()` | [04](04-solver.md) |
| rich deps | rpm boolean dependencies `(a or b)` | `CapDetail::EXPRESSION` | [04](04-solver.md) |
| RIS | Repository Index Service (`repoindex.xml`) | `repo::ServiceType::RIS` | [05](05-repositories-and-services.md) |
| RMT / SMT / SCC | SUSE registration servers (Repository Mirroring Tool, Subscription Management Tool, SUSE Customer Center) that provide RIS services | – | [05](05-repositories-and-services.md) |
| root | Target root directory (`--root`) | `ZConfig::systemRoot()`, `Target::root()` | [08](08-target-commit-and-rpm.md) |
| rpm-md | Repo metadata format (`repodata/repomd.xml`), type `RPMMD` | `RepoType::RPMMD` | [05](05-repositories-and-services.md) |
| rpmdb | rpm's database of installed packages and trusted keys | `target::rpm::RpmDb`, `librpmDb` | [08](08-target-commit-and-rpm.md) |
| rpmdb2solv | libsolv tool building `@System` solv | `TargetImpl::buildCache()` | [08](08-target-commit-and-rpm.md) |
| RWCOW_pointer | Copy-on-write PIMPL pointer | `zypp-logic/zypp-core/base/PtrTypes.h` | [02](02-core-infrastructure.md) |
| S_* (ui::Status) | Selectable status (S_Install, S_Taboo, S_Protected, …) | `zypp/zypp/ui/Status.h` | [03](03-sat-pool-and-resolvables.md) |
| sat::Pool | Wrapper of the single libsolv pool | `zypp/zypp/sat/Pool.h` | [03](03-sat-pool-and-resolvables.md) |
| sat::Solvable | Id handle of a libsolv solvable | `zypp/zypp/sat/Solvable.h` | [03](03-sat-pool-and-resolvables.md) |
| sat::Transaction | Ordered commit steps derived from pool status | `zypp/zypp/sat/Transaction.{h,cc}` | [03](03-sat-pool-and-resolvables.md) |
| satisfied | Patch status: fixed versions are installed | `ResStatus::isSatisfied()` | [04](04-solver.md) |
| Selectable | All items of one (kind, name) for UIs | `zypp/zypp/ui/Selectable.h` | [03](03-sat-pool-and-resolvables.md) |
| SendReport | Library-side callback sender | `zypp/zypp/Callback.h` | [02](02-core-infrastructure.md) |
| serial / serialIDs | Pool change counters that invalidate derived caches | `sat::Pool::serial()`, `SerialNumber` | [03](03-sat-pool-and-resolvables.md) |
| service | Manages a set of repos (RIS or plugin) | `zypp/zypp/ServiceInfo.{h,cc}` | [05](05-repositories-and-services.md) |
| severity | Patch severity (low/moderate/important/critical) | `Patch::severity()` | [03](03-sat-pool-and-resolvables.md) |
| sigcheck plugin | Stateful repo metadata check plugin (`repo_sigcheck_plugin=`) | `zypp/zypp/repo/PluginSigcheck.{h,cc}` | [06](06-security-gpg-keyring.md) |
| single_rpmtrans | Commit backend: one librpm transaction via `zypp-rpm` | `TargetImpl::commitInSingleTransaction` | [08](08-target-commit-and-rpm.md) |
| SLE / SLES | SUSE Linux Enterprise (Server) | – | [12](12-release-packaging-and-compat.md) |
| soft lock | Non-persistent "keep not installed" by name | `ResStatus::isSoftLocked()` | [04](04-solver.md) |
| solv (file) | libsolv binary repo cache | `/var/cache/zypp/solv/<alias>/solv` | [05](05-repositories-and-services.md) |
| solv.idx | `name\tevr\tarch` index for zypper bash completion | `sat::updateSolvFileIndex` | [05](05-repositories-and-services.md) |
| SOLVER (causer) | Lowest causer; reset on each solve | `ResStatus::SOLVER` | [03](03-sat-pool-and-resolvables.md) |
| solver testcase | Directory reproducing a solver run | `solver::detail::Testcase`, `misc::LoadTestcase` | [04](04-solver.md) |
| splitprovides | `oldpkg:/file` provides used to replace split packages | `SOLVER_FLAG_SPLITPROVIDES` | [04](04-solver.md) |
| srcpackage | Source rpm resolvable (arch src/nosrc) | `zypp/zypp/SrcPackage.h` | [03](03-sat-pool-and-resolvables.md) |
| StringPool | Owner of the single libsolv `Pool` | `zypp-logic/zypp/ng/sat/stringpool.{h,cc}` | [03](03-sat-pool-and-resolvables.md) |
| subpriority | Tie-breaker between equal-priority repos, by URL scheme | `media::MediaPriority` | [03](03-sat-pool-and-resolvables.md) |
| suggests | Weakest dependency (UI hint) | `Dep::SUGGESTS` | [A.3](#dependency-types) |
| supplements | Reverse recommends ("install me if X is installed") | `Dep::SUPPLEMENTS` | [04](04-solver.md) |
| susetags | Repo format with a `content` master index (type `YAST2`) | `RepoType::YAST2`, `parser/susetags/` | [05](05-repositories-and-services.md) |
| SystemCheck | Always-enforced requires/conflicts from `/etc/zypp/systemCheck` | `solver/detail/SystemCheck.{h,cc}` | [04](04-solver.md) |
| system plugin | Notified with PACKAGESETCHANGED after an `@System` rebuild | `plugins/system/` | [08](08-target-commit-and-rpm.md) |
| taboo | UI status: not installed and locked (`S_Taboo`) | `ui::Status` | [03](03-sat-pool-and-resolvables.md) |
| Target | Installed system: rpmdb, `@System`, commit | `zypp/zypp/Target.h`, `target/TargetImpl.*` | [08](08-target-commit-and-rpm.md) |
| techpreview | zypp.conf keys that set experimental env switches | `ZConfig.cc` | [09](09-configuration-and-environment.md) |
| testtags | libsolv testcase repo format (YAML testcases) | `addTesttags` | [11](11-testing.md) |
| transaction step | One commit action `[+]`/`[-]`/`[M]`/`[ ]` with a stage | `sat::Transaction::Step` | [08](08-target-commit-and-rpm.md) |
| trusted keyring | Keys trusted by the admin, synced with the rpmdb | `KeyRingImpl` `Ring::Trusted` | [06](06-security-gpg-keyring.md) |
| TW | openSUSE Tumbleweed (rolling release) | – | [12](12-release-packaging-and-compat.md) |
| unneeded | Autoinstalled package no longer required | `ResStatus::isUnneeded()` | [04](04-solver.md) |
| unwanted | Broken but locked patch | `PoolItem::isUnwanted()` | [04](04-solver.md) |
| update (doUpdate) | "Update all, quietly"; ignores requests | `Resolver::doUpdate()` | [04](04-solver.md) |
| update messages / scripts | Files under `/var/adm/update-{messages,scripts}` handled after install | `RunUpdateMessages`, `RunUpdateScripts` | [08](08-target-commit-and-rpm.md) |
| update stack | Patches touching the package manager, installed first by zypper | `Patch::restartSuggested()` | [04](04-solver.md) |
| updateCandidate | Strictly newer, vendor-compatible available item | `ui::Selectable::updateCandidateObj()` | [03](03-sat-pool-and-resolvables.md) |
| updateinfo | rpm-md metadata containing patches | libsolv `repo_updateinfoxml` | [04](04-solver.md) |
| upgrade repo | Repo used for a DUP restricted to it (`dup --from`) | `Resolver::addUpgradeRepo` | [04](04-solver.md) |
| Url | URL class with scheme-specific rules | `zypp-logic/zypp-core/Url.{h,cc}` | [02](02-core-infrastructure.md) |
| urlresolver plugin | Resolves `plugin:` URLs | `plugins/urlresolver/` | [08](08-target-commit-and-rpm.md) |
| USER (causer) | Highest causer: explicit user request | `ResStatus::USER` | [03](03-sat-pool-and-resolvables.md) |
| UserData | Typed key/value bag for reports; also zypper `--userdata` string | `zypp-logic/zypp-core/UserData.h`, `ZConfig::userData()` | [02](02-core-infrastructure.md) |
| usrmerge | /usr merge migration; a pending one disables single_rpmtrans | `pendingUsrmerge()` | [08](08-target-commit-and-rpm.md) |
| validRepoSignature | Persisted result of the repo signature check | `RepoInfo::validRepoSignature()`, `.repo_gpgcheck` | [06](06-security-gpg-keyring.md) |
| vars.d | Custom repo variables, one file per variable | `/etc/zypp/vars.d` | [05](05-repositories-and-services.md) |
| vendor | Package builder string | `Solvable::vendor()` | [A.1](#vendor) |
| vendor equivalence | Vendor groups from `/etc/zypp/vendors.d` | `zypp/zypp/VendorAttr.{h,cc}` | [03](03-sat-pool-and-resolvables.md) |
| VendorSupportOption | Support level from `support_*` keywords | `zypp/zypp/VendorSupportOptions.h` | [03](03-sat-pool-and-resolvables.md) |
| verify | Repair broken dependencies of the installed system | `Resolver::verifySystem()` | [04](04-solver.md) |
| weak bits | ResStatus recommended/suggested/orphaned/unneeded flags | `ResStatus::WeakField` | [03](03-sat-pool-and-resolvables.md) |
| weakremover | Product release package provide naming dropped packages | `Product::droplist()` | [04](04-solver.md) |
| WhatProvides | Providers of a capability | `zypp/zypp/sat/WhatProvides.h` | [03](03-sat-pool-and-resolvables.md) |
| workers (NG) | `zypp-media-*` processes for async media access | `zyppng/tools/zypp-media-*` | [10](10-zyppng-next-generation.md) |
| yast2 (repo type) | Alias for susetags (`RepoType::YAST2`) | `RepoType.cc` | [05](05-repositories-and-services.md) |
| YaST | SUSE installer/admin tool; uses libzypp via yast2-pkg-bindings | – | [14](14-consumers-and-api-surface.md) |
| zck / zchunk | Chunked compression format; lets clients download only changed chunks of metadata | `ZckLoader`, `MediaCurl2::tryZchunk` | [07](07-media-and-downloads.md) |
| ZConfig | zypp.conf / global settings singleton | `zypp/zypp/ZConfig.{h,cc}` | [09](09-configuration-and-environment.md) |
| zypp-logic | Tier 2 shared-bridge sources | `zypp-logic/` | [01](01-layout-build-and-dependencies.md) |
| zypp-rpm | Helper process running the single rpm transaction | `zypp/tools/zypp-rpm/` | [08](08-target-commit-and-rpm.md) |
| zypp-tui | zypper TUI helper library (`ztui`) | `zypp/zypp-tui/` | [02](02-core-infrastructure.md) |
| ZYPP_API | Export macro for public symbols | `zypp-logic/zypp-core/Globals.h` | [12](12-release-packaging-and-compat.md) |
| ZYPP_CONF | Env: alternative zypp.conf | `ZConfig.cc` | [09](09-configuration-and-environment.md) |
| ZYPP_FULLLOG | Env: excessive logging, libsolv debug, auto testcases | `Logger`, `PoolImpl`, `detail/Resolver.cc` | [02](02-core-infrastructure.md) |
| ZYPP_LOCK_TIMEOUT | Env: how long to wait for the zypp lock | `ZConfig::lockTimeout()` | [08](08-target-commit-and-rpm.md) |
| ZYPP_LOCKFILE_ROOT | Env: prefix for the lock directory (`/run`) | `ZYppFactory::lockfileDir()` | [08](08-target-commit-and-rpm.md) |
| ZYPP_LOGFILE | Env: log file override (`-` = stderr) | `LogControl` | [02](02-core-infrastructure.md) |
| ZYPP_READONLY_HACK | Env: skip the zypp lock | `ZYppReadOnlyHack.h` | [08](08-target-commit-and-rpm.md) |
| ZYPP_REPO_RELEASEVER | Env: override `$releasever` | `RepoVariables.cc` | [05](05-repositories-and-services.md) |
| ZYPP_SINGLE_RPMTRANS | Env: enable the single-transaction backend | `ZYppCommitPolicy.cc` | [08](08-target-commit-and-rpm.md) |
| ZYPP_TESTSUITE_FAKE_ARCH | Env: fake system arch (disables commit) | `ZConfig.cc`, `ZYppImpl::commit` | [03](03-sat-pool-and-resolvables.md) |
| ZYPP_* (others) | Full env var list | – | [09 §8](09-configuration-and-environment.md) |
| zypper | The CLI front-end and primary libzypp consumer | `../zypper` | [14](14-consumers-and-api-surface.md) |
| ZYpp | Singleton giving access to pool, resolver, target, keyring, commit | `zypp/zypp/ZYpp.h`, `zypp_detail/ZYppImpl.*` | [08](08-target-commit-and-rpm.md) |
| ZYppCommitPolicy | Commit options (dry run, download mode, rpm flags) | `zypp/zypp/ZYppCommitPolicy.{h,cc}` | [08](08-target-commit-and-rpm.md) |
| ZYppCommitResult | Commit outcome: transaction steps, update messages, backend used | `ZYppCommitResult.h` | [08](08-target-commit-and-rpm.md) |
| zyppng | Tier 1 NG code, C++20 module | `zyppng/` | [10](10-zyppng-next-generation.md) |
