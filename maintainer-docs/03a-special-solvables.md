# 03a – Special Solvables: Products, Patterns, Patches, PTFs & Co.

To the solver, everything is a solvable: a name, an EVR, an arch, a vendor and dependency arrays. Users also see
*products*, *patterns*, *patches*, *PTFs* and *source packages*. This chapter explains where each of these comes from in
the real world (spec file tags, XML files, repo metadata), how libsolv turns it into a solvable, how libzypp models it,
and the lifecycle quirks you will meet in bug reports.

It goes deeper than the per-class reference in [03 §16](03-sat-pool-and-resolvables.md#16-resolvable-kinds-in-detail)
and the target-side product handling in [08 §13](08-target-commit-and-rpm.md). Read those for accessor tables. This
chapter is about **definition and mechanics**.

Unless stated otherwise, paths are relative to `libzypp/`. libsolv paths are prefixed `libsolv/` and zypper paths are
prefixed `zypper/`.

## Table of contents

**Part A – Products, patterns, applications, source packages**

- [A1. Overview of the non-package kinds](#a1-overview-of-the-non-package-kinds)
- [A2. Products](#a2-products)
- [A3. Patterns](#a3-patterns)
- [A4. Applications (dead kind)](#a4-applications-dead-kind)
- [A5. Source packages](#a5-source-packages)
- [Part A pitfalls](#part-a-pitfalls)

**Part B – Patches, retracted, PTFs, blacklisting, special cases** (appended below)

- B.1 Patches: metadata, libsolv conversion, libzypp model, status, update stack
- B.2 Retracted packages and patches
- B.3 PTFs (Program Temporary Fixes)
- B.4 Blacklisting in general
- B.5 Other special packages (livepatch, multiversion, needreboot, update stack)
- Part B pitfalls, Where to start when debugging special solvables

---

# Part A

## A1. Overview of the non-package kinds

libsolv has a single flat namespace of solvable names. Kinds are encoded in two ways (see
[00a glossary "kind prefix"](00a-concepts-and-glossary.md) and [03 §4.3](03-sat-pool-and-resolvables.md#43-reskind-and-kind-prefixes)):

* a **name prefix**: `product:`, `pattern:`, `patch:`, `application:`;
* for source packages, the **arch** `src` / `nosrc`. Their name is the plain package name.

`sat::Solvable::kind()` (`zypp/zypp/sat/Solvable.cc`) checks the arch first and then the prefix
(`ResKind::explicitBuiltin`). A name with an unknown `xxx:` prefix becomes a `ResKind("xxx")`. A name without `:` is a
`package`.

| Kind | sat ident | Real-world definition | libsolv converter | Owns an rpm? | libzypp class |
|---|---|---|---|---|---|
| product | `product:SLES` | Release package `Provides: product(...)` + `/etc/products.d/*.prod` | `repo_add_autopattern` (repos), `repo_add_products` (installed), `repo_add_content`, rpm-md `products.xml` | **no**: the `-release` package is its buddy | `Product` |
| pattern | `pattern:base` | `patterns-*` package with `Provides: pattern() = base` (Code 12+); legacy `patterns.xml` / susetags `.pat` | `repo_add_autopattern`; legacy `repo_add_rpmmd` / `repo_add_susetags` | no: linked to the pattern package | `Pattern` |
| patch | `patch:SUSE-2024-123` | `updateinfo.xml` | `repo_add_updateinfoxml` | no (pseudo-installed) | `Patch` → Part B |
| application | `application:Foo` | AppStream `appdata.xml` / metainfo | `repo_add_appdata` | no | `Application` (**dead since 17.7.0**) |
| srcpackage | `foo` + arch `src`/`nosrc` | `*.src.rpm` listed in primary.xml / susetags | normal package parsers | yes, but never "installed" | `SrcPackage` |

libsolv treats all of `patch:`, `pattern:`, `product:` and `application:` as **pseudo packages**:
`is_pseudo_package()` in `libsolv/src/transaction.c` excludes them from the rpm transaction. libzypp maps them to
`TRANSACTION_IGNORE` steps ([03 §18](03-sat-pool-and-resolvables.md#18-sattransaction-and-how-commit-uses-it)).

### A1.1 "Linked packages": the common mechanism behind products, patterns and applications

libsolv (`libsolv/src/linkedpkg.c`, compiled with `ENABLE_LINKED_PKGS`, ON by default in
`libsolv/CMakeLists.txt`) knows that `product:`, `pattern:` and `application:` solvables are generated from a real
package. `find_package_link()` dispatches to `find_product_link` / `find_pattern_link` / `find_application_link`, and
the rule generator (`addlinks()` in `libsolv/src/rules.c`) adds **two** package rules:

```
pseudo  -> real      (SOLVER_RULE_PKG_REQUIRES, dep = "product(X) = evr" / pattern pkg name / appdata(...))
real    -> pseudo    (SOLVER_RULE_PKG_REQUIRES, dep = pseudo's self-provide)
```

So the pair can only be installed or removed **together**. Other consequences:

* `solver_addpkgrulesforlinked()` (`libsolv/src/solver.c`) pulls the link rules into the rule set.
* For installed pseudos, `solv->instbuddy[]` records the buddy. A `SOLVER_ERASE` job on an installed product or
  pattern also adds an erase job for its buddy (`solver.c`, around the `instbuddy` erase loop).
* `solver_get_userinstalled`/cleandeps (`libsolv/src/cleandeps.c`) marks **all installed products and their buddies
  as userinstalled**. `--clean-deps` therefore never removes a release package as an "unneeded" dependency.
* `policy.c` uses `pool_link_evrcmp()` to break EVR ties between linked packages.
* `solvable_lookup_str_base()` (`libsolv/src/solvable.c`) uses `find_autopackage_name()` to find **translations** of an
  autopattern's or autoproduct's summary/description in the generating package.
* `solvable_identical()` (`libsolv/src/solvable.c`) ignores vendor and buildtime differences for `product:` and
  `application:` solvables ("workaround for bugs 881493 and 885830").

libzypp adds its own, coarser coupling **only for products**. A `Product` and its `referencePackage()` share one
`ResStatus` (the PoolItem *buddy*, see [A2.5](#a25-libzypp-model)). Patterns are coupled to their package **only by the
libsolv link rules**, not by a shared status.

## A2. Products

### A2.1 What a product is

A product (SLES, SLED, openSUSE Leap, an SLE module or extension, …) is a named, versioned unit used for
registration, lifecycle and migration. It has no files of its own. On disk it is represented by:

1. a **release package** (`SLES-release`, `openSUSE-release`, `sle-module-basesystem-release`, …). Its `Provides:`
   carry all the product metadata;
2. a **product file** `/etc/products.d/<name>.prod`. The release package installs it. It describes the
   *installed* product;
3. the symlink **`/etc/products.d/baseproduct`**, which points to the `.prod` file of the base product. Who creates
   it (release package `%post`, installer) is outside libzypp. libzypp only reads it.

Repositories usually carry no separate product metadata today. The product solvable is synthesized from the release
package's provides (`repo2solv -X`). Older sources still exist: susetags `content` (`PRODUCT`/`NAME` tags) and rpm-md
`products.xml` (repomd type `products` or `product`, `libsolv/tools/repo2solv.c`).

### A2.2 Definition in the release package (`Provides:`)

`repo_add_autopattern()` (`libsolv/ext/repo_autopattern.c`) recognises the following provides. Only `REL_EQ`
(`=`) relations are honoured, except for `product-obsoletes(...)`. Values are **URL-hex-unescaped** (`%20` → space,
`%2D` → `-`), because rpm provides can't contain arbitrary characters.

| Provide (in release pkg) | Effect on generated `product:NAME` solvable | libsolv key → libzypp `SolvAttr` / accessor |
|---|---|---|
| `product() = NAME` | **Triggers** generation. Two `product()` provides → the lexically smallest value wins | – |
| `product(NAME) = EVR` | **Required.** Without it no product is created. Its EVR becomes the product EVR (not the package EVR!) | `s->evr` |
| `product-label() = X` | short label | `PRODUCT_SHORTLABEL` → `Product::shortName()` |
| `product-register-target() = X` | registration target | `PRODUCT_REGISTER_TARGET` → `registerTarget()` |
| `product-register-flavor() = X` | registration flavor | `PRODUCT_REGISTER_FLAVOR` → `registerFlavor()` |
| `product-type() = X` | e.g. `base` (libzypp only ever tests for `"base"`) | `PRODUCT_TYPE` → `type()` |
| `product-cpeid() = X` | CPE 2.3 name (hex-encoded) | `SOLVABLE_CPEID` → `cpeId()` |
| `product-flags() = X` | appended to flag array | `PRODUCT_FLAGS` → `flags()` |
| `product-updates-repoid() = X` | adds a `product:updates` sub-structure with `repoid` | `PRODUCT_UPDATES`/`_REPOID` → `updateContentIdentifier()` |
| `product-endoflife()` / `product-endoflife() = DATE` | `PRODUCT_ENDOFLIFE`; no value → `0` ("EOL exists, date unknown", FATE#320699). Date formats: epoch digits, `%F%T` or `%F` | `endOfLife()`, `hasEndOfLife()` |
| `product-url(TYPE) = URL` | parallel arrays `url` / `url:type` (TYPE < 32 chars) | `urls(TYPE)`, `releaseNotesUrls()` (`releasenotes`), … |
| `product-obsoletes(NAME)` [op EVR] | `Obsoletes: product:NAME [op EVR]` **on the product solvable** | `replacedProducts()` |

The generator also:

* copies `arch`, `vendor`, `buildtime`, `summary`, `description` and `distribution` from the release package;
* adds `Requires: product(NAME) = EVR` (the link dependency), `Provides: autoproduct() = <release-pkg-name>` and the
  self-provide `product:NAME = EVR`;
* skips packages with an `installtime` ("eek, not for installed packages, please!") and the whole
  `pool->installed` repo (`ADD_NO_AUTOPRODUCTS`). **Installed products never come from provides, only from `.prod`
  files** ([A2.4](#a24-installed-products-etcproductsd)).
* skips creation if a `product:NAME` with the same arch and EVR already exists in the repo, e.g. from `content` or
  `products.xml`.

Not mapped from provides, although the `.prod` parser knows them: `productline`, `register/release`,
`referencefile`. Consequence: for **available** (repo) products, `Product::productLine()`, `registerRelease()` and
`referenceFilename()` are empty. They are only filled for installed products.

Other provides on release packages that libzypp reads directly (not libsolv):

| Provide | Reader | Meaning |
|---|---|---|
| `weakremover(PKG)` | `Product::droplist()` = `buddy().valuesOfNamespace("weakremover")` (`zypp/zypp/Product.cc`); DUP second solver pass in `SATResolver` ([04 §3](04-solver.md)) | Packages to drop when migrating *to* this product (bsc#1155819) |
| `product_flavor(NAME) = EVR` + `flavor(X)` | `Product::flavor()`: any package in the **same repo** (arch ignored) providing `product_flavor(NAME) = EVR`; returns the content of its first `flavor(...)` provide | Historic media flavors (`dvd`, `ftp`, …) |
| `product()` | `sat::Solvable::isProductPackage()` / `productProvides()` (`productToken`) | "this is a release package". Used by the DUP droplist pass |

**Illustrative** release-package spec fragment. The tag names are verified against `repo_autopattern.c`. The values
are made up. Real SUSE release specs are generated by OBS product tooling, not hand-written:

```spec
Name:     SLES-release
Version:  15.7
Release:  0
Provides: product() = SLES
Provides: product(SLES) = 15.7-0
Provides: product-label() = SLES15-SP7
Provides: product-cpeid() = cpe%3A%2Fo%3Asuse%3Asles%3A15%3Asp7
Provides: product-type() = base
Provides: product-register-target() = sle-15-x86_64
Provides: product-endoflife() = 2031-07-31
Provides: product-url(releasenotes) = https%3A%2F%2Fwww.suse.com%2Frelnotes%2F...
Provides: product-updates-repoid() = obsproduct%3A%2F%2Fbuild.suse.de%2FSUSE%3ASLE-15-SP7%3AUpdate%2FSLES%2F15.7%2Fx86_64
Provides: product-obsoletes(SUSE_SLES)
Provides: weakremover(some-dropped-package)
Obsoletes: SUSE_SLES-release < 15
```

### A2.3 The `.prod` file (`/etc/products.d/*.prod`)

Two parsers read the same file:

* **libsolv** `repo_add_code11_products()` (`libsolv/ext/repo_products.c`) builds the installed `product:` solvables
  during `rpmdb2solv -p <root>/etc/products.d`. rpmdb2solv strips a leading `-r` root from the `-p` path before it
  prepends the root again (`libsolv/tools/rpmdb2solv.c`).
* **libzypp** `parser::ProductFileReader` (`zypp/zypp/parser/ProductFileReader.cc`) is used for `baseproduct` and
  distribution queries without a pool ([08 §13](08-target-commit-and-rpm.md)). It knows `<upgrades>` and
  `<updaterepokey>`. libsolv ignores both.

Elements recognised by libsolv (`stateswitches[]` in `repo_products.c`) and where they end up:

```
<product schemeversion="...">            new solvable; schemeversion parsed but unused
  <vendor/>                              s->vendor
  <name/>                                s->name = "product:" + name
  <version/> <release/>                  s->evr = version[-release]   (release without version: stderr warning)
  <arch/>                                s->arch            (missing -> noarch)
  <productline/>                         PRODUCT_PRODUCTLINE
  <summary lang=".."/>                   SOLVABLE_SUMMARY (per language)
  <shortsummary/>                        PRODUCT_SHORTLABEL
  <description lang=".."/>               SOLVABLE_DESCRIPTION (per language)
  <cpeid/>                               SOLVABLE_CPEID (only if non-empty)
  <endoflife/>                           PRODUCT_ENDOFLIFE  (empty -> 0 = "unknown", FATE#320699)
  <updaterepokey/>                       ignored ("obsolete")
  <urls><url name="TYPE">..</url></urls> PRODUCT_URL / PRODUCT_URL_TYPE (url without name= is dropped)
  <register>
     <target/> <release/> <flavor/>      PRODUCT_REGISTER_TARGET / _RELEASE / _FLAVOR
     <updates><repository repoid=".."/></updates>   PRODUCT_UPDATES{repoid}
  </register>
  <linguas/> <runtimeconfig/>            accepted, ignored
</product>
```

Synthesized by libsolv for each `.prod` file:

* `PRODUCT_REFERENCEFILE` = the **basename** of the file (`Product::referenceFilename()`);
* `SOLVABLE_INSTALLTIME` = the file's **ctime**, not the rpm install time;
* `PRODUCT_TYPE = "base"` **iff the file's inode equals the inode `stat("baseproduct")` resolves to**. No other type
  is ever set from a `.prod` file. An installed add-on therefore has an **empty** `type()`, while the same product in
  the repo may have `product-type() = extension`;
* only the self-provide `product:NAME = EVR`. There are **no** obsoletes and **no** `product(NAME)` requires. The link
  to the release package is reconstructed by `find_product_link()` (below).

A file that fails to parse is dropped (`solvable_free`), with a `SOLV_ERROR` debug line. Only files ending in `.prod`
are read.

**Illustrative** `.prod` file. The element names are verified. Compare the real test file
`zypp/tests/zypp/data/Target/product.prod`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<product schemeversion="0">
  <vendor>SUSE</vendor>
  <name>SLES</name>
  <version>15.7</version>
  <release>0</release>
  <arch>x86_64</arch>
  <productline>sles</productline>
  <cpeid>cpe:/o:suse:sles:15:sp7</cpeid>
  <endoflife>2031-07-31</endoflife>
  <register>
    <target>sle-15-x86_64</target>
    <updates>
      <repository repoid="obsrepository://build.suse.de/SUSE:Updates:SLE-Product-SLES:15-SP7/x86_64" />
    </updates>
  </register>
  <summary>SUSE Linux Enterprise Server 15 SP7</summary>
  <shortsummary>SLES15-SP7</shortsummary>
  <urls>
    <url name="releasenotes">https://www.suse.com/releasenotes/x86_64/SUSE-SLES/15-SP7/release-notes-sles.rpm</url>
  </urls>
</product>
```

Fallbacks in `repo_add_products()` when the products dir can't be opened ("three parsers for the price of one"):

1. `/etc/products.d` (Code 11+ `.prod`);
2. `/var/lib/zypp/db/products` (Code 10 zypp db, `repo_zyppdb.c`);
3. `/etc/*-release` (except `lsb-release`; `repo_releasefile_products.c`): line 1 `NAME [VERSION] (ARCH)` → `product:NAME`,
   with `VERSION = x` lines for the EVR. On non-SUSE systems without `/etc/products.d` this can produce odd product
   names, for example from `/etc/os-release`. That last part is derived from the code and not tested on a real system.

### A2.4 Installed products (`@System`)

```
rpmdb ──rpmdb2solv -X──▶ packages (+ autopatterns, NO autoproducts)
/etc/products.d/*.prod ──repo_add_products──▶ product:NAME solvables (type base via baseproduct inode)
                       └─ @System cookie includes RepoStatus(/etc/products.d)  (TargetImpl.cc)
```

The `@System` cookie covers the products dir, so editing or deleting a `.prod` file by hand triggers a rebuild of the
`@System` solv file ([08 §3](08-target-commit-and-rpm.md)).

How libsolv links an installed product to its release package (`find_product_link`, `libsolv/src/linkedpkg.c`):

1. Look for a requires `product(NAME) = EVR` on the product. Installed products have none, so it is constructed from
   the name.
2. Providers of it in the **same repo with the same arch**.
3. If there are several providers, filter by `autoproduct()` name, then by equal buildtime.
4. If nothing is found and the product is installed, use the owner of `/etc/products.d/<referencefile>` from the
   filelist (bnc#784900-style fallback).

libzypp's `Product::referencePackage()` (`zypp/zypp/Product.cc`) re-implements the same search. It does not call
libsolv's:

* `WhatProvides("product(NAME) = EDITION")`, same repo, same arch;
* bsc#1259311: if the product provides `autoproduct() = PKG`, only that package name is accepted;
* several candidates (update repos with several release package versions, bnc#497696): prefer equal buildtime, then the
  higher edition;
* installed and still not found: the package owning `/etc/products.d/<referenceFilename>` (bnc#784900; bsc#1086602
  made sure the path is under `/etc/products.d`);
* nothing found: `WAR … no reference package found`.

The **arch must match**. If a `.prod` file says `<arch>noarch</arch>` (or omits it) while the release rpm is `x86_64`,
only the file-owner fallback can find the buddy, and that fallback only works for installed products.

### A2.5 libzypp model

* **Class**: `Product` (`zypp/zypp/Product.{h,cc}`). See [03 §16.3](03-sat-pool-and-resolvables.md#16-resolvable-kinds-in-detail)
  for the accessor table.
* **Buddy**: `pool::PoolImpl::store()` (`zypp/zypp/pool/PoolImpl.h`) collects every newly added product and, once the
  store is complete, calls `pi.setBuddy( asKind<Product>(pi)->referencePackage() )`. From then on product and release
  package **share one `ResStatus`**. Selecting, locking or transacting either one does the same to the other
  (bsc#1225267: `setHardLockQueries` locks the pair if either one matches a lock). See
  [03 §7.2](03-sat-pool-and-resolvables.md#72-poolitem) for the `_buddy` sign encoding. This is the libzypp-side
  successor of "Tell satsolver about product buddies" (bnc#466565). The libsolv side is the link rule (A1.1).
* **Status derivation**: a product is *installed* iff a `.prod` file for it exists in `@System`. Installing a product
  means installing its release package. libsolv requires it through the link rule, and the shared status makes the UI
  consistent.
* **Selectable**: one `ui::Selectable` per `product:NAME`. `installedObj()` is the `.prod`-based solvable, and the
  candidates are the repo autoproducts. Because of the attribute asymmetry in A2.2/A2.3, **`zypper info -t product`
  shows installed data when installed** (zypper `info.cc`: "commit#b40f49ae: if installed, get info from
  installedObj").
* **Problem texts**: `SATResolver`'s `mapSolvable()` maps a release package to its buddy, so solver problems say
  "product X" instead of "X-release" ([04 §6](04-solver.md)).
* **Base product**: `Product::isTargetDistribution()` = installed and `type() == "base"`, which comes from the
  `baseproduct` inode match. `TargetImpl::baseProduct()` / `distributionVersion()` / `$releasever`
  ([08 §13](08-target-commit-and-rpm.md), [05](05-repositories-and-services.md)).
* **Update repos**: `Product::updateContentIdentifier()` (the repoids from A2.2/A2.3) is compared with
  `Repository::hasContentIdentifier()`, i.e. `SolvAttr::repositoryRepoid`. That comes from susetags
  `content` `REPOID` (verified in `repo_content.c`) or, presumably, repomd.xml `<tags><repo>` (not verified in
  `repo_repomdxml.c`). `Repository::isUpdateRepo()` also checks whether a known product
  references the repo (bnc#892579).
* **License**: `ResObject::licenseToConfirm()` falls back to the repo's `license-<name>` / `license` tarball for
  products only (bnc#908976, FATE#322276; [03 §6](03-sat-pool-and-resolvables.md#6-satsolvable-and-friends)).

### A2.6 Lifecycle

| Action | What actually happens |
|---|---|
| `zypper in -t product SLES` | Job `install product:SLES`. Link rule → release package; shared status. The release package's own deps pull in the rest (usually little: the product's *pattern* drives the content) |
| `zypper up -t product` | **Rejected** by zypper: "To update installed products use 'zypper dup [--from <repo>]'" (`zypper/src/commands/update.cc`) |
| Plain `zypper up` | The release package updates like any package. After commit, the new `.prod` file is read on the next `@System` load |
| `zypper dup` with product change | The new release package `Obsoletes:` the old release package and/or `product-obsoletes()`. `Product::replacedProducts()` reports what it replaces (own obsoletes + buddy obsoletes). The new product's `weakremover()` list triggers a **second solver pass** with `SOLVER_DROP_ORPHANED` per name (bsc#1155819; older history: bnc#552180 "only if product actually changes", bnc#551697) |
| `zypper rm -t product X` | Erase product → libsolv `instbuddy` erases the release package too. This often fails because other packages require the release package |
| Release package removed without a product job | The link rule `real → pseudo` removes the product solvable. At commit time a buddy-less product is handled as an **orphan** (below) |
| Commit of a product without a buddy (`TargetImpl::commit`, classic and zypp-rpm paths) | To-be-installed: `ERR "Can't install orphan product without release-package!"`. To-be-removed: delete `/etc/products.d/<referenceFilename>`, **unless an rpm owns it** (`rpm().hasFile`), in which case `WAR "Won't remove orphan product"` (bnc#432932) |
| `--clean-deps` | Never removes installed products or their buddies (userinstalled in `cleandeps.c`) |

### A2.7 Display

* `zypper products` (`zypper/src/commands/query/products.cc`), and `zypper info -t product`
  (`printProductInfo` in `zypper/src/info.cc`) shows End of Support (unknown EOL on a `suse*` vendor adds a lifecycle
  URL hint), Flavor (`flavor()` + `registerFlavor()`), Is Base, Status (up-to-date or out-of-date by `updateCand`),
  Update Repositories (content IDs, plus whether an enabled repo provides each one), CPE Name (with a malformed-CPE
  hint from `CpeId::NoThrowType::lastMalformed`) and Short Name.
* `--xmlout products --xmlfwd TAG`: zypper copies the given tags **literally from the installed `.prod` file**
  (`zypper/src/utils/misc.cc`, `XmlFilter::fwd`). This is how tools get `.prod` content that neither libsolv nor
  libzypp model. It only works for installed products, because only they have a `referenceFilename`.

**Illustrative** `zypper info -t product` output. The fields are the ones `printProductInfo` emits:

```
Information for product SLES:
-----------------------------
Repository     : @System
Name           : SLES
Version        : 15.7-0
Arch           : x86_64
Vendor         : SUSE
End of Support : 2031-07-31
Flavor         :
Is Base        : Yes
Installed      : Yes
Status         : up-to-date
Update Repositories:
  Content Id   : obsrepository://build.suse.de/SUSE:Updates:SLE-Product-SLES:15-SP7/x86_64
                 Provided by enabled repository: SLES15-SP7-Updates
CPE Name       : cpe:2.3:o:suse:sles:15:sp7:*:*:*:*:*:*
Short Name     : SLES15-SP7
Summary        : SUSE Linux Enterprise Server 15 SP7
```

## A3. Patterns

### A3.1 Definitions: three generations

| Generation | Source | Converter | Notes |
|---|---|---|---|
| Code 10/11 susetags | `descr/patterns` index file listing `*.pat[.gz]` files, tags `=Pat:`, `=Sum:`, `=Vis:`, `=Cat:`, `=Ord:`, `=Ico:`, `=Inc:`, `=Ext:`, `=Prq:`/`=Prc:`/`=Psg:` (package requires/recommends/suggests), … | `repo2solv` "add legacy patterns" → `repo_add_susetags` | Real pattern solvables with their own deps |
| Code 11 rpm-md | `patterns.xml` (repomd type `patterns`) with `<pattern>` elements: `<uservisible/>`, `<category>`, `<icon>`, `<order>`, `<script>`, `<default>`, `<includes><item pattern=".."/>`, `<extends><item pattern=".."/>`, rpm deps | `repo_add_rpmmd` (`repo_rpmmd.c`, kind detection by element name) | `isdefault`/`script`/`install-time` exist only here |
| **Code 12+ (current)** | **pattern package** `patterns-<product>-FOO` with `Provides: pattern() = FOO` | `repo_add_autopattern` (`repo2solv -X`, `rpmdb2solv -X`) | The documented convention is in `zypp/doc/autoinclude/PatternPackages.doc` |

Since libzypp 14.7.0, patterns are **not pseudo-installed** any more (FATE#309385; `traits::isPseudoInstalled()` is
true only for patches). With pattern packages, a pattern is installed iff its package is installed. The installed
autopattern is generated from the rpmdb by `rpmdb2solv -X`.

### A3.2 Pattern-package provides (`repo_add_autopattern`)

| Provide | Effect | libzypp |
|---|---|---|
| `pattern() = FOO` | Creates `pattern:FOO`. **A value starting with `.` is ignored** ("hack to allow provides that do not create a pattern"). Two `pattern()` provides → the lexically smallest wins | – |
| `pattern-visible()` (no value) | `SOLVABLE_ISVISIBLE` = void → true | `Pattern::userVisible()` |
| `pattern-visible() = IDENT` | stored as string: visible **iff `IDENT` is in the pool** (bsc#900769) | `userVisible()` checks `ResPool::byIdent(ident)` |
| *(no `pattern-visible`)* | **invisible** (`lookupBoolAttribute` → false) | hidden by zypper/YaST |
| `pattern-category() = X` / `pattern-category(LANG) = X` | `SOLVABLE_CATEGORY[:lang]`. `en` or empty → base key. Latin-1 is converted to UTF-8 if the value isn't valid UTF-8. LANG ≤ 8 chars | `category(Locale)` |
| `pattern-icon() = X` | `SOLVABLE_ICON` | `icon()` |
| `pattern-order() = X` | `SOLVABLE_ORDER` (a string such as `"1099"`, sorted as a string by UIs) | `order()` |
| `pattern-includes() = FOO` | `SOLVABLE_INCLUDES` += `pattern:FOO` | `includes()` → `PatternExpander` |
| `pattern-extends() = FOO` | `SOLVABLE_EXTENDS` += `pattern:FOO` | `extends()` |

Generated `pattern:FOO` solvable:

* `name = pattern:` + unescaped value. `arch`, `evr` and `vendor` = the package's (unlike products, where the EVR comes
  from `product(NAME)`).
* `Requires: <pkgname> = <evr>`, `Provides: autopattern() = <pkgname>`, self-provide.
* `installtime`, `buildtime`, `summary` and `description` are copied. Category translations already on the package
  (`solvable:category:*` keys) are copied as well.
* It is skipped if a `pattern:FOO` with the same arch and EVR already exists in the repo, e.g. from legacy
  `patterns.xml`.
* Unlike products, autopatterns are generated for `@System` too, so installed patterns exist.

The pattern's **content** is simply the package's `Requires`/`Recommends`/`Suggests`, including other `patterns-*`
packages and rich deps. The pattern solvable itself has no content deps.

**Example** (real provides list from `zypp/doc/autoinclude/PatternPackages.doc`, shortened):

```
patterns-sles-Minimal-12-37.1.x86_64.rpm
 PROVIDES { pattern() == Minimal; pattern-icon() == pattern-basis; pattern-order() == 1099;
            pattern-visible(); pattern-category() == Base%20Technologies;
            pattern-category(de) == Basistechnologien; ... }
 REQUIRES { systemd coreutils insserv aaa_base ... }
 RECOMMENDS { grub2 rpcbind nfs-client rsyslog ... }
```

→ `pattern:Minimal-12-37.1.x86_64`, visible, category "Base Technologies", `Requires: patterns-sles-Minimal = 12-37.1`.

### A3.3 libzypp model

* **Class**: `Pattern` (`zypp/zypp/Pattern.{h,cc}`); table in [03 §16.2](03-sat-pool-and-resolvables.md#16-resolvable-kinds-in-detail).
* `isAutoPattern()`: has an `autopattern()` provide. `autoPackage()`: provider of `arch:PKG = edition` **in the
  same repo** (the arch-qualified capability is built with `Capability(arch, name, EQ, edition)`).
* `core()`, `depends()`, `contents()` merge the pattern's own deps with the autoPackage's deps (bnc#864087 "Fix pattern
  content for auto-patterns"). Suggests can be excluded (bnc#857671). `contents()` follows `includes`/`extends`
  through `PatternExpander`, which is O(P²) because of `expandExtending`.
* **No buddy.** Unlike products, a pattern and its package do **not** share a `ResStatus`. Selecting `pattern:FOO`
  leaves the package unselected in the libzypp status layer. The solver installs it through the link rule, and the
  status shows up after the solve (as `BY_SOLVER`).
* `sat::Solvable::isPatternPackage()` / `patternProvides()` (`patternToken = "pattern()"`) identify pattern packages.
  zypper's `info -t pattern` rewrites `patterns-*` package entries in the content table back to their pattern
  (`rewriteIfPatternsPackge` in `zypper/src/info.cc`, via `autopattern() = <pkg>`). It also shows the
  "Buddy Package" (`autoPackage()`).
* Visibility: zypper `patterns`, `search` and `info` hide or skip invisible patterns (bnc#538152, `zypper/src/search.cc`).
  YaST presumably sorts by `order()` and groups by `category()`. That is not verified in YaST code. libzypp itself
  ignores icon, order and category.

### A3.4 Lifecycle & semantics

| Action | Result |
|---|---|
| `zypper in -t pattern FOO` | Install `pattern:FOO` → link → `patterns-*-FOO` → its requires (hard) and recommends (unless `--no-recommends`, which also drops pattern members, [04](04-solver.md)) |
| `zypper in patterns-sles-FOO` | Same result: the link rule `real → pseudo` installs `pattern:FOO` too |
| `zypper rm -t pattern FOO` | Removes `pattern:FOO` **and the pattern package** (`instbuddy` erase / link rule). **The content stays installed.** Only `--clean-deps` removes members that nothing else needs. zypper's `SolverRequester` has `// TODO handle … patterns (remove content(?))` |
| `zypper up -t pattern` | **NOOP**: `SolverRequester::updatePatterns() { /*NOOP*/ }`. Patterns update with their packages during `up`/`dup` |
| A member package removed later | The pattern stays installed as long as its pattern package is installed. Hard `Requires` of the pattern package block the member's removal (or the solver proposes removing the pattern package too). Removing a *recommended* member doesn't affect the pattern at all |
| Pattern status in UI | Comes from the `@System` autopattern, i.e. "pattern package installed". Not from "all members installed" |

## A4. Applications (dead kind)

* **Definition**: AppStream `appdata.xml(.gz)` in the repo (repomd type `appdata`, or susetags `appdata.xml`), or
  installed `/usr/share/metainfo` and `/usr/share/appdata` files.
* **Converter**: `repo_add_appdata` (`libsolv/ext/repo_appdata.c`). Top level is `<applications>`/`<components>`.
  Per `<application>`/`<component>`: `id`, `pkgname`, `name`, `summary`, `description` (p/ul/ol flattened into
  text), `url`, `project_group`, `keywords`, `extends`, `product_license`. Name = `application:` + `<name>` (falls back
  to the desktop id minus `.desktop`). Link: `Requires: <pkgname>` + `Provides: application-appdata(<pkgname>)`, or
  `appdata(<file>)` for installed files.
* **Only `type="desktop"` (or no type) is accepted.** Components with any other type are skipped, including the
  modern `desktop-application`. This explains the "poor metadata support".
* **libzypp**: `ResKind::application` and `Application` (`zypp/zypp/Application.{h,cc}`) still exist, with no
  accessors. **Since 17.7.0 nothing creates them**: `repo2solv`/`rpmdb2solv` are called without `-A`
  (`// bsc#1104415: no more application support` in `zypp/zypp/target/TargetImpl.cc` and
  `zypp/zypp/ng/repo/workflows/repomanagerwf.cc`; changelog bsc#1100095, bsc#1104415;
  `zypp/doc/autoinclude/ApplicationMetada.doc`).
* **What still lives on**: the **appdata plugin hook**. `RepoManager`'s destructor (`zypp/zypp/ng/repomanager.cc`) runs
  every executable in `<pluginsPath>/appdata/` with `-R alias -t type -p <metadata repodata path>` for all enabled repos
  (bsc#1197684 fixed the path). This only happens if repos changed (`reposManip()`) or `ZYPP_PLUGIN_APPDATA_FORCE_COLLECT`
  is set (bsc#1009745), and only as root on `/` (bnc#866257). This is how desktop software centres collect AppStream
  data. It is independent of the solvable kind.
* The libsolv side (`find_application_link`, `solvable_identical` workaround) is still compiled in. It would apply if
  someone fed appdata into the pool, e.g. a testcase.

## A5. Source packages

### A5.1 Definition & representation

* Real-world source: `*.src.rpm` / `*.nosrc.rpm` listed in rpm-md `primary.xml` (usually separate `-source` repos)
  or susetags `packages` with arch `src`/`nosrc`, or plaindir repos. `nosrc` = a source rpm without some sources
  (non-redistributable).
* libsolv parses them with the normal parsers, with these differences:
  * **No self-provide.** The rpmmd, susetags and rpmdb parsers skip it for `ARCH_SRC`/`ARCH_NOSRC`, so source
    packages provide nothing and are not in the whatprovides index;
  * filelist dirs without a leading `/` are put under `/usr/src` (`repo_rpmmd.c`);
  * rules: a source package gets **only "requires" rules** (its `BuildRequires`). No conflicts or obsoletes
    (`libsolv/src/rules.c`: "that's all we check for src packages"). It counts as installable unless its repo is
    disabled, although `pool_installable()` says no;
  * `name.src` lookups: `pool_addrelproviders` `REL_ARCH` with `src`/`nosrc` does a **linear scan of all solvables**
    (`poolwhatprovides.c`: "small hack"). `NAME.src` matches `src` and `nosrc` packages.
* Binary → source mapping: binary packages carry `SOLVABLE_SOURCENAME` / `SOURCEEVR` / `SOURCEARCH`. Id 0 / empty
  means "same as the binary" ([03 §16.4](03-sat-pool-and-resolvables.md#16-resolvable-kinds-in-detail)).
  zypper's `findBinPkgsBuiltFromSrc()` (`zypper/src/info.cc`) walks `SolvAttr::sourcename` for the reverse mapping
  (marked "TODO for libzypp").

### A5.2 libzypp model & identity quirk

* `ResKind::srcpackage` is detected **by arch**, not by prefix (`Solvable::kind()`/`isKind()`). `ResKind::satIdent()`
  adds no prefix for `package` and `srcpackage`, so **`foo` the binary and `foo` the source package have the same
  ident.**
* To keep them apart, `pool::PoolImpl::id2item()` (`zypp/zypp/pool/PoolImpl.h`) and `pool::ByIdent`
  (`zypp/zypp/pool/ByIdent.h`) store source packages under the **negative** ident id. `ResPool::byIdent(ResKind::srcpackage,
  "foo")` and `ui::Selectable::get(ResKind::srcpackage, "foo")` work. Raw `sat::WhatProvides("foo")` never returns
  source packages, because they have no provides.
* `Capability( "foo", ResKind::srcpackage )` (and the embedded form `srcpackage:foo`) is mapped to `foo REL_ARCH src`
  (`zypp/zypp/Capability.cc`). An explicit arch is ignored for source packages.
* `SrcPackage` (`zypp/zypp/SrcPackage.{h,cc}`) offers `sourcePkgType()` (arch), `location()` and `cachedLocation()`.
* `operator<<` prints source packages with a `srcpackage:` prefix (`Solvable.cc`). Watch out when you grep logs.
* `SolvableSpec::contains` excludes srcpackages, e.g. for the needreboot spec ([08](08-target-commit-and-rpm.md)).
  Historic: the pool package index included source packages by mistake (bnc#380283).

### A5.3 Lifecycle

* **Install only.** They are never "installed" from libzypp's point of view: `rpm -i` of a src.rpm unpacks into
  `%_topdir` and creates no rpmdb entry. Removal is refused by zypper ("Uninstallation of a source package not defined
  and implemented", `commands/installremove.cc`). `zypper up -t srcpackage` is refused ("Zypper does not keep track of
  installed source packages … use 'zypper si'").
* `zypper source-install` / `si` (`zypper/src/commands/sourceinstall.cc`, `misc.cc`):
  * `source_find()`: first a srcpackage by name (glob, `PoolItemBest`). Otherwise the binary package's
    `sourcePkgName()`/`sourcePkgEdition()` are used to find the srcpackage ("Package '%s' has source package '%s'").
  * Default: `resolver()->addRequire( Capability(name, EQ, edition, ResKind::srcpackage) )`. The solver "installs"
    the srcpackage and its `BuildRequires`. `-d/--build-deps-only` adds the srcpackage's requires one by one instead.
    `-D/--no-build-deps` skips the solver part.
  * In addition, `mark_src_pkgs` → `install_src_pkgs()` installs the marked srcpackages after the commit via
    `ZYpp::installSrcPackage` (or `provideSrcPackage` for download-only).
* libzypp commit: the download phase fetches `SrcPackage`s through `CommitPackageCache`/`PackageProvider`
  (`PackageProviderImpl<SrcPackage>`, bsc#1037210; download-only no longer skips them, bnc#635596). The install step is
  `TargetImpl::installSrcPackage()` → `provideSrcPackage()` → `repo::SrcPackageProvider::provideSrcPackage()` =
  `RepoMediaAccess::provideFile(repoInfo, location)` → `rpm().installPackage()`. In the zypp-rpm path, the srcpackage
  goes into the transaction as an `InstallStep` (`TargetImpl.cc` around the "SrcPackage is install-only" comments).
  `SrcPackageProvider` calls `provideFile` directly, not the `PackageProvider` workflow (see [05](05-repositories-and-services.md)).

## Part A pitfalls

- **Installed products come only from `/etc/products.d/*.prod`.** Autoproducts are disabled for `@System`. Delete a
  `.prod` file and the product is gone, even though the release rpm is still installed.
- **Available products and installed products carry different attribute sets.** `productLine()`, `registerRelease()`
  and `referenceFilename()` are empty for repo products. `type()` is empty for installed non-base products. Always ask
  which side a bug report is looking at; zypper `info` prefers the installed object.
- **`type()=="base"` is an inode comparison** with `stat(/etc/products.d/baseproduct)`. A copied file instead of a
  symlink, or a dangling symlink, silently yields "no base product" and breaks `$releasever` and the flavor cache.
- **Product EVR comes from `product(NAME) = EVR`**, not from the release package EVR. A mismatch between that provide
  and the `.prod` `<version>-<release>` breaks the buddy link.
- **The product/release-package arch must match** for `referencePackage()`. Otherwise only the file-owner fallback
  helps, and only for installed products. A buddy-less product can't be installed ("orphan product"), and removing
  it deletes the `.prod` file if no rpm owns it.
- **Product and release package share one `ResStatus`.** Locking or selecting one affects the other (bsc#1225267).
  Patterns do **not** share status with their package. They are tied together only by libsolv link rules.
- **libsolv link rules make pseudo and real package inseparable.** Removing a pattern removes its `patterns-*`
  package but **not the content**. Removing a product removes its release package, which is often blocked by
  dependencies.
- **Patterns without `pattern-visible()` are invisible.** A `pattern-visible() = IDENT` makes visibility depend on
  `IDENT` being in the pool (bsc#900769).
- **`pattern() = .foo`** (leading dot) deliberately creates no pattern. With several `pattern()` provides, only the
  lexically smallest is used. The same applies to `product()`.
- **Provide values are URL-hex-encoded** (`%20`, `%2D`, …) and libsolv unescapes them. Raw provides in `rpm -q
  --provides` look different from what libzypp shows.
- **`zypper up -t pattern` is a NOOP and `zypper up -t product` is rejected.** Products move with `dup`, where
  `weakremover()` drop lists cause a second solve (bsc#1155819).
- **`--clean-deps` never removes products or their release packages** (libsolv marks them userinstalled).
- **The `application` kind is dead since 17.7.0** (bsc#1100095/bsc#1104415). The class remains for ABI. Only the
  `appdata` plugin hook in `RepoManager`'s destructor is still alive. libsolv would accept only `type="desktop"`
  components anyway.
- **Source packages share their ident with binaries.** libzypp separates them with negative ids in `id2item`/`ByIdent`.
  They have no provides, so `WhatProvides` never finds them, and `NAME.src` lookups are a linear pool scan.
- **Source packages are install-only and never tracked.** In the solver they only get BuildRequires rules, and after
  commit they leave no rpmdb trace.

---

# Part B – Patches, retracted packages, PTFs and blacklisting

This part explains where the three "maintenance" kinds of special solvables come from and how they work: **patches** (from `updateinfo.xml`), **retracted** packages/patches, and **PTFs** (Program Temporary Fixes). They share one mechanism, libsolv's `SOLVER_BLACKLIST`. Earlier chapters list the APIs; this part focuses on how the metadata is turned into solvables and how the solver treats them. Short summaries that are not repeated here:
[Pool §11](03-sat-pool-and-resolvables.md#11-retracted-packages-ptfs-blacklisting),
[Pool §16.1 Patch](03-sat-pool-and-resolvables.md#161-patch-zyppzyppPatchhcc),
[Solver §3.1 job #3/#6, §6.1, §8, §19](04-solver.md),
[Commit §7/§8 (update scripts, history)](08-target-commit-and-rpm.md).

Paths are relative to `libzypp/` unless they start with `libsolv/` or `zypper/`.

---

## B.1 Patches: the metadata

### B.1.1 Where patches come from

A patch exists **only** as repository metadata. There is no patch rpm. In rpm-md repos it is the `updateinfo` entry of `repodata/repomd.xml` (`repodata/*-updateinfo.xml.gz`). libzypp does not parse it itself. `RepoManager` runs libsolv's `repo2solv` tool (`zypp/zypp/ng/repo/workflows/repomanagerwf.cc`), and that tool calls `repo_add_updateinfoxml()` (`libsolv/tools/repo2solv.c` → `read_rpmmd_repo` → `repomd_find(…,"updateinfo",…)`). The resulting solvables end up in the repo's `solv` cache. SUSE-tags (`content`/`packages`) repos have no patch format, and plaindir repos carry no patches either.

### B.1.2 The XML schema libsolv actually understands

The element/attribute set below is taken **only** from the state table and attribute parsing in `libsolv/ext/repo_updateinfoxml.c`. Anything not listed is silently ignored. Example (illustrative values, real names):

```xml
<!-- illustrative; element and attribute names verified against repo_updateinfoxml.c -->
<updates>
  <update from="maint-coord@suse.de" status="stable" type="security" version="1">
    <id>SUSE-SLE-Module-Basesystem-15-SP6-2026-1234</id>
    <title>Security update for zypper, libzypp</title>
    <severity>important</severity>
    <release>SUSE Updates SLE-Module-Basesystem 15-SP6 x86_64</release>  <!-- parsed, IGNORED -->
    <rights>Copyright SUSE LLC</rights>
    <issued date="1761200000"/>
    <updated date="2026-10-23 12:00:00"/>
    <references>
      <reference href="https://bugzilla.suse.com/show_bug.cgi?id=1234567"
                 id="1234567" title="zypper does X" type="bugzilla"/>
      <reference href="https://www.suse.com/security/cve/CVE-2026-0001/"
                 id="CVE-2026-0001" title="CVE-2026-0001" type="cve"/>
    </references>
    <description>This update for zypper fixes ...</description>
    <message>Please restart ...</message>                <!-- optional -->
    <pkglist>
      <collection short="...">                           <!-- 'short' ignored -->
        <name>...</name>                                  <!-- parsed, IGNORED -->
        <package name="zypper" epoch="0" version="1.14.80" release="150600.10.1"
                 arch="x86_64" src="zypper-1.14.80-150600.10.1.src.rpm">  <!-- 'src' ignored -->
          <filename>zypper-1.14.80-150600.10.1.x86_64.rpm</filename>
          <restart_suggested>True</restart_suggested>
        </package>
        <package name="libzypp" version="17.37.0" release="150600.3.1" arch="x86_64">
          <filename>libzypp-17.37.0-150600.3.1.x86_64.rpm</filename>
          <restart_suggested>True</restart_suggested>
        </package>
        <module name="..." stream="..." version="..." context="..." arch="..."/> <!-- Fedora modularity -->
      </collection>
    </pkglist>
  </update>
</updates>
```

### B.1.3 Mapping to the solvable

| XML | libsolv storage | libzypp accessor |
|---|---|---|
| `<id>` | `s->name = "patch:" + id` | `Patch::name()` (kind prefix stripped) |
| `update@version` | `s->evr` (taken as is, **no** epoch/release split) | `edition()` |
| *(fixed)* | `s->arch = noarch` | `arch()` |
| `update@from` | `s->vendor` | `vendor()` |
| `update@type` | `SOLVABLE_PATCHCATEGORY` | `Patch::category()` / `categoryEnum()` |
| `update@status` | `UPDATE_STATUS` (pool string) | `SolvAttr::updateStatus`, `Solvable::isRetracted()` |
| `<title>` | `SOLVABLE_SUMMARY` (trailing newlines stripped) | `summary()` |
| `<description>` | `SOLVABLE_DESCRIPTION` | `description()` |
| `<message>` | `UPDATE_MESSAGE` | `Patch::message()` |
| `<severity>` | `UPDATE_SEVERITY` | `Patch::severity()` / `severityFlag()` |
| `<rights>` | `UPDATE_RIGHTS` | **no `SolvAttr`** in libzypp |
| `<issued date>`, `<updated date>` | `SOLVABLE_BUILDTIME` = **max** of both (epoch seconds or `YYYY-MM-DD HH:MM:SS`) | `Patch::timestamp()` = `buildtime()` |
| `<reference …/>` | flexarray `UPDATE_REFERENCE` {`_HREF`,`_ID`,`_TITLE`,`_TYPE`} | `Patch::ReferenceIterator` |
| `<package …>` | flexarray `UPDATE_COLLECTION` {`_NAME`,`_EVR`,`_ARCH`} **+ synthesized conflicts** (below) | `Patch::contents()` |
| `<filename>` | `UPDATE_COLLECTION_FILENAME` | `SolvAttr::updateCollectionFilename` |
| `<reboot_suggested>` / `<restart_suggested>` / `<relogin_suggested>` | `UPDATE_REBOOT` / `_RESTART` / `_RELOGIN` set on **both** the package handle and the patch | `rebootSuggested()` / `restartSuggested()` / `reloginSuggested()` |
| `<collection>` | `UPDATE_COLLECTIONLIST` grouping packages and modules | not exposed |
| `<module>` | `UPDATE_MODULE_*` | not exposed |

Things to remember:

* **Patch `provides`:** at `</update>` libsolv adds `patch:<id> = <version>`. Nothing else is provided. In particular a patch does **not** get the `retracted-patch-package()` marker (B.2).
* **The reboot/restart/relogin flags are per package in the XML.** They are recognised **only inside `<package>`**. A `<reboot_suggested>` placed directly under `<update>` is ignored. If *any* package carries the flag, the whole patch gets it (`/* FIXME: this is per-package, the global flag should be computed at runtime */`). The value counts as true if it starts with `T`, `t` or `1`.
* **`SolvAttr::updateCollectionFlags` (`UPDATE_COLLECTION_FLAGS`) is never written by the updateinfo parser.** The per-package flags are stored as `UPDATE_REBOOT` etc. on the package handle instead. Do not rely on `updateCollectionFlags`.
* `update@version` is stored as the edition string. In SUSE metadata it is usually a small integer that increases when a patch is re-released under the same id (see B.1.7).
* `zypp/zypp/Patch.cc` reads the message as a translated attribute (`message(Locale)`), but updateinfo has no per-language messages, so in practice there is only one.

### B.1.4 The synthesized conflicts: the core of patch semantics

For every `<package>` the parser adds conflicts (comment in the source: `patch.conflicts: {name} < {version}.{release}`):

```c
/* libsolv/ext/repo_updateinfoxml.c, STATE_PACKAGE (abridged) */
if (a && a != ARCH_NOARCH) {
    conflicts += (name . arch)   <  evr     /* REL_ARCH then REL_LT */
    conflicts += (name . noarch) <  evr
} else
    conflicts += name < evr                 /* arch-less or noarch entries */
```

So `patch:SUSE-…-1234` from the example above ends up as:

```
Provides:  patch:SUSE-SLE-Module-Basesystem-15-SP6-2026-1234 = 1
Conflicts: zypper.x86_64 < 1.14.80-150600.10.1
           zypper.noarch < 1.14.80-150600.10.1
           libzypp.x86_64 < 17.37.0-150600.3.1
           libzypp.noarch < 17.37.0-150600.3.1
```

Consequences (all of them have been reported as "bugs" at some point):

* **A patch never *requires* anything.** "Installing" it means no older version of these packages may stay installed. The solver normally meets that by updating them. It can also meet it by **removing** them, or by picking any newer version, not necessarily the one listed. zypper's man page warns that patches update affected packages to the latest version (zypper changes, bsc#1187466). Problems involving patches are always conflict style ("the to be installed patch:… conflicts with 'foo.x86_64 < 1.2' provided by the installed foo-1.1").
* The `.noarch` conflict covers packages whose arch changed between arch and noarch.
* Packages that are **not installed** are unaffected, because a conflict only applies to something installed or about to be installed. That is why a patch is "not relevant" when none of its package names is installed ([Solver §19.2](04-solver.md#192-status-computation-solvable_trivial_installable_map-libsolvsrcsusec)).
* The `<package>` list is the *only* place where the patch connects to packages. `Patch::contents()` reads the `UPDATE_COLLECTION` copy, not the conflicts, and filters it to installed name+arch ([Pool §16.1](03-sat-pool-and-resolvables.md#161-patch-zyppzyppPatchhcc)).

### B.1.5 Categories and severities (libzypp view)

`zypp/zypp/Patch.{h,cc}`. `Patch::Category` is a flag enum:

| Metadata `type` (case-insensitive) | Enum | Notes |
|---|---|---|
| `yast` | `CAT_YAST` | YaST self-update patches |
| `security` | `CAT_SECURITY` | |
| `recommended`, RHN `bugfix` | `CAT_RECOMMENDED` | |
| `optional`, `feature`, RHN `enhancement` | `CAT_OPTIONAL` | zypper skips these unless `--with-optional` / `--with-update` (bsc#1102261) |
| `document` | `CAT_DOCUMENT` | |
| anything else, including empty | `CAT_OTHER` | logged at INT level |

Severity: `low`/`moderate`/`important`/`critical`, `unspecified` or empty → `SEV_NONE`, anything else → `SEV_OTHER` (`asString` = `"unknown"`).

`Patch::InteractiveFlag`: `Reboot` (reboot suggested), `Message` (non-empty message), `License` (patch **or any `contents()` package** has `licenseToConfirm()`). Updateinfo has no license element, so in practice `License` comes from packages. zypper's `--with-interactive`/`--skip-interactive` and the "reboot is not interactive" config option are built on `interactiveWhenIgnoring()` (zypper `src/Config.cc`).

### B.1.6 "Update stack" patches

There is no dedicated "package manager patch" type. A patch counts as an update-stack patch **iff `restartSuggested()`**, i.e. at least one `<package>` carries `<restart_suggested>True</restart_suggested>`. SUSE sets this on patches that update zypper, libzypp, libsolv, rpm and friends. All the policy lives in zypper:

```
zypper patch [--updatestack-only] [--with-update]
  SolverRequester::updatePatches()                         zypper/src/SolverRequester.cc
    pass 1 (ignore_pkgmgmt=0): installPatch() only accepts BROKEN && restartSuggested()
       └─ any selected? → stop; --with-update is dropped for this run
    pass 2 (ignore_pkgmgmt=1, not with --updatestack-only): all needed patches
  solve_and_commit()
    Summary: patch->rebootSuggested() → exit 102 (ZYPPER_EXIT_INF_REBOOT_NEEDED)
             else restartSuggested()  → exit 103 (ZYPPER_EXIT_INF_RESTART_NEEDED)
```

* Exit codes 102/103 depend **only on patches** in the transaction (zypper#237, `zypper/src/Summary.h`). A package marked `isNeedreboot()` triggers a summary hint but not exit code 102. Reboot takes precedence over restart (`else if` in `Summary.cc`).
* `zypper list-patches --updatestack-only` and `patch-check` read `restartSuggested()` from the *established* states (`zypper/src/update.cc`) and do no solving.
* **Quirk in zypper's XML output** (`zypper/src/update.cc`): the patch attribute `pkgmanager` is `restartSuggested()` and the attribute **`restart` is `rebootSuggested()`**. Front-ends that parse `zypper -x lp` depend on this naming. Don't "fix" it.

### B.1.7 Patch versions, "superseding" and candidates

Neither libsolv nor libzypp has a superseding mechanism (no obsoletes are generated). A re-released patch keeps its `<id>` and gets a higher `version`. Both versions then end up as `patch:<id>` solvables in one `ui::Selectable`. `zypper patch` only looks at **`selPtr->candidateObj()`** per patch ident (`SolverRequester::updatePatches`):

* The candidate is the first item in `AVOrder`: non-blacklisted first, then repo priority, then edition ([Pool §12](03-sat-pool-and-resolvables.md#12-uiselectable-and-respoolproxy-how-yast-sees-the-pool)). A *retracted* patch version is blacklisted (`Solvable::isBlacklisted()` is true for a patch with `status="retracted"`), so it sorts last. An older non-retracted version of the same id becomes the candidate.
* With `--date` (bnc#919709) zypper walks `available()` and picks the first version issued before the limit.
* Patches with *different* ids that fix the same package are independent. Once the newer one is applied, the older one is SATISFIED automatically, because its conflicts no longer match.

### B.1.8 Status: "needed", "applied", "not needed"

A patch has no stored state. Its status is the **establish** result, `ResStatus::ValidateValue`, computed by libsolv's `solver_trivial_installable()` in an empty solver run ([Solver §8.1, §19.2](04-solver.md#81-pseudo-installed-items)):

```
                 any conflict hits an installed pkg?
                 ┌─────── yes ───────┐                ┌──── no ────┐
 status=retracted or vendor-irrelevant?           any conflict NAME installed?
      yes → NONRELEVANT ("not needed")              yes → SATISFIED ("applied")
      no  → BROKEN ("needed"; "unwanted" if locked) no  → NONRELEVANT ("not needed")
```

zypper's display strings (`zypper/src/utils/misc.cc` `i18nPatchStatus`/`textPatchStatus`): `retracted` is checked **first**, before the validate value. Then `needed` / `unwanted` (broken and locked) / `optional` (broken, `CAT_OPTIONAL`, and `exclude_optional_patches`) / `applied` / `not needed` / `undetermined`. The search status column uses `!` for broken, `i` for satisfied, and appends `R` for retracted (B.2.5).

### B.1.9 What happens at commit

* Patches are **pseudo-installed** (`traits::isPseudoInstalled(ResKind::patch)` is the only `true` case). In `sat::Transaction` they are `TRANSACTION_IGNORE`, so rpm never sees them. Only the packages the solver chose in order to satisfy the conflicts are committed.
* After commit, `logPatchStatusChanges()` (`zypp/zypp/target/TargetImpl.cc`) compares `ResPool::changedPseudoInstalled()` with the initial `EstablishedStates`. For every patch whose state changed it writes a history line (`HistoryLog::patchStateChange`, `zypp/zypp/HistoryLog.cc`):
  `date|patch  |name|evr|arch|repoalias|severity|category|oldstate|newstate|userdata|`
  This is how "patch X was installed" is recorded, and it also catches patches that became satisfied *as a side effect* of a plain package update. If the commit was incomplete, the target is reloaded and the states are recomputed first (`WAR "Need to recompute the patch status changes…"`). Parsed back by `HistoryLogPatchStateChange` (`zypp/zypp/HistoryLogData.cc`).
* **Update scripts and messages are not tied to patches anymore.** `RunUpdateScripts`/`RunUpdateMessages` pick up `/var/adm/update-{scripts,messages}/<name>-<version>-<release>[-*]` for every successfully installed **package** ([Commit §7](08-target-commit-and-rpm.md#7-needreboot-update-scripts-and-update-messages)). The callback is still called `PatchScriptReport` for historical reasons (the old Code10/11 patch-script mechanism). The patch's own `<message>` is only *shown* by front-ends before commit (the `Message` interactive flag). It is not executed or written to disk by libzypp.

### B.1.10 Establish vs vendor (why a patch is "not needed" although old packages are installed)

`solvable_is_irrelevant_patch()` (`libsolv/src/suse.c`) also returns "irrelevant" when the patch repo has fixing packages but none of them is a legal vendor change for the installed package. libzypp's `VendorAttr` equivalence is used through `pool_set_custom_vendorcheck`. Typical report: "patch not needed although my foo is older". The installed foo comes from a third-party vendor (e.g. OBS home repo) that is not equivalent to SUSE. That is by design.

---

## B.2 Retracted packages and patches

### B.2.1 What "retracted" means

A maintenance team can **retract** a released update: the update stays in the repo (removing files from a published repo breaks mirrors and caches), but its `<update status="retracted">` tells clients not to install it automatically (jsc#SLE-8770). The retracted packages are still visible, marked `R`, and can be installed by explicit request.

### B.2.2 How the marker gets into the pool

```
updateinfo.xml  status="retracted"
      │ repo_add_updateinfoxml()          → UPDATE_STATUS="retracted" on patch:<id>
      ▼
repo2solv (SUSE build only)
      │ repo_mark_retracted_packages(repo, "retracted-patch-package()")   libsolv/tools/repo2solv.c
      ▼
packages of THE SAME REPO whose name+evr equal a "<name>[.arch] < <evr>" conflict
of a retracted patch get an extra provides:  retracted-patch-package()
      ▼
solv cache (raw/solv/<alias>/solv)
```

`repo_mark_retracted_packages()` (`libsolv/ext/repo_updateinfoxml.c`, `#ifdef SUSE`) in detail:

1. It collects every non-patch, non-src solvable **of this repo** as `(p, name, evr, arch)`. libsolv commit 931e4928 "Make repo_mark_retracted_packages only look at the provided repo".
2. For each patch with `UPDATE_STATUS == "retracted"`, it collects each `REL_LT` conflict as `(0, name, evr, arch)`.
3. It sorts by `(name, evr, p)` and marks packages that follow a retracted `(name, evr)` entry. **Arch is collected but not part of the comparison.** So every arch of `name-evr` in the repo is marked, including `noarch`. This doesn't matter in practice because the conflicts cover both `arch` and `noarch` anyway.

Consequences:

* **The marker is baked into the `solv` cache at build time.** Marking or unmarking only takes effect after the cache is rebuilt (the repo metadata changes, so the checksum changes and `repo2solv` runs again). If `repo2solv` arguments or marker logic ever change while metadata stays the same, bump the `magic` in `RepoStatus::Impl::assignFromCtor` (`zypp/zypp/RepoStatus.cc`, currently `"43"`) to invalidate all caches.
* A retracted patch in repo A does **not** mark identical packages in repo B (e.g. a mirror under another alias, or a package in an SP-update repo and a module repo).
* **Patches themselves never get the marker.** libzypp therefore uses a separate check for patches: `Solvable::isRetracted()` = `updateStatus == "retracted"` for `ResKind::patch` and `_retractedSpec.contains()` for packages (`zypp/zypp/sat/Solvable.cc`). For all other kinds it returns false. Changelog 17.21+: "Get retracted patch status from updateinfo data (jsc#SLE-8770) – libsolv injects the indicator provides into packages only."
* The libsolv side also makes a retracted patch *irrelevant* (`solvable_is_irrelevant_patch`, libsolv 0.7.11 "treat retracted patches as irrelevant"), so it is never "needed".

### B.2.3 Installed retracted packages

The rpmdb copy of a package never has the marker (it is metadata, not rpm content). To still flag it, `sat::detail::PoolImpl` sets `_retractedSpec.addIdenticalInstalledToo(true)` (`zypp/zypp/sat/detail/PoolImpl.cc`). When `SolvableSpec` builds its cache (`zypp/zypp/sat/SolvableSpec.cc`), it also adds `Selectable::identicalInstalledObj()` for each available match. "Identical" means libsolv `solvable_identical()`: same name, arch, evr and vendor, and the same buildtime (or the same requires hash when a buildtime is missing).

* If the installed copy was rebuilt (different buildtime) or the retracted copy's repo is disabled, the installed package is **not** shown as retracted.
* **NG gap:** `zyppng/lib/zypp/ng/sat/components/packagepolicycomponent.cc` registers the same three provides specs but never enables identical-installed matching. `addIdenticalInstalledToo` exists nowhere in `zyppng/` or `zypp-logic/`. Installed retracted packages are therefore *not* flagged in NG. This looks like a parity bug.

The solver does **not** remove or downgrade an installed retracted package:

* The blacklist job is `SOLVER_BLACKLIST|SOLVER_SOLVABLE_PROVIDES retracted-patch-package()`. libsolv skips installed solvables when creating black rules (`solver_addblackrules`, `libsolv/src/rules.c`), and the installed copy doesn't match the provides anyway.
* `zypper dup` keeps it too: `solver_addduprules` lets an installed package stay if it is `solvable_identical` to a package in the dup repos, and that check doesn't consult the blacklist.
* Getting rid of it is up to the user (e.g. `zypper in --oldpackage foo-<previous>`). zypper only shows `iR`/`vR` in the status column.

### B.2.4 Example walk-through (from `libsolv/test/testcases/blacklist/retracted.t`)

```
@System:   B-1-1
available: patch-1-1  Conflicts: B < 2-1
           B-2-1      Provides: retracted-patch-package()

job blacklist provides retracted-patch-package()
job install name patch
→ problem: patch-1-1 conflicts with B < 2-1 provided by B-1-1
    solution: deljob install name patch
    solution: allow B-2-1@available          ← SOLVER_SOLUTION_BLACK
    solution: erase B-1-1@system

job install pkg B-2-1.noarch@available       ← direct (SOLVER_SOLVABLE) request
→ upgrade B-1-1 → B-2-1                       ← blacklist lifted
```

In libzypp the "allow" solution becomes a `ProblemSolution` with an `INSTALL` action and the text "install %1% although it has been retracted" (`SATResolver::problems()`, `SOLVER_SOLUTION_BLACK` branch). Note that `zypper in patch:<retracted-id>` doesn't even get this far: `SolverRequester::installPatch()` only selects BROKEN patches, and a retracted patch is NONRELEVANT, so zypper reports "patch … is not needed" (`Feedback::PATCH_NOT_NEEDED`).

### B.2.5 UI surface

| Where | What |
|---|---|
| `PoolItem`/`sat::Solvable` | `isRetracted()`, `isBlacklisted()` (`zypp/zypp/sat/SolvableType.h`) |
| `ui::Selectable` | `hasRetracted()` (available), `hasRetractedInstalled()`, `hasBlacklisted*()`. `candidateObj()` may be blacklisted only if nothing else is available or it was set explicitly; `candidateObjFrom()`, `updateCandidateObj()` and `highestAvailableVersionObj()` avoid blacklisted items (changelog: "Selectable: Fix highestAvailableVersionObj if only retracted packages are available", jsc#SLE-8770) |
| `misc::poolInstallState` | flag `Retracted` = `isBlacklisted()`, so **PTFs are reported as Retracted too** (`zypp/zypp/misc/PoolInstallState.cc`; see [Pool chapter](03-sat-pool-and-resolvables.md)) |
| zypper `search`/`info` status column | 2nd char `R` (retracted) or `P` (PTF); blacklist beats lock (`l` is not shown if the item is also `R`/`P`) — `zypper/src/utils/misc.cc` `computeStatusIndicator` |
| zypper `list-patches`/`info` | patch status `retracted` (zypper `c3867541`, jsc#SLE-8770) |

---

## B.3 PTFs (Program Temporary Fixes)

### B.3.1 What a PTF is

A PTF is a **customer-specific hotfix** that SUSE support builds for one support case before (or instead of) an official maintenance update. Since 2022 PTFs are delivered as rpm-md **PTF repositories** that zypper can use like any other repo. The building blocks:

```
ptf-22333-1-0.noarch                ← PTF "master" (proxy) package; the number is part of the NAME
   Provides: ptf()                  ← sat::Solvable::ptfMasterToken
   Requires: <the patched package versions>
salt-3000-30.1.22333.0.PTF.1047068.x86_64      ← PTF packages (patched builds)
   Provides: ptf-package()          ← sat::Solvable::ptfPackageToken
   (ties back to the master)
```

*Verified:* the tokens `ptf()` / `ptf-package()` (`zypp/zypp/sat/Solvable.cc`), the `ptf-` name prefix that libsolv special-cases (`suse_isptf()` in `libsolv/src/rules.c` and `problems.c`), and that the master pulls in the PTF packages (the SUSE "Easier delivery of hot fixes" blog example: `zypper install ptf-22333` upgrades `salt`, `salt-minion`, `python3-salt` to `…PTF…` releases).
*Not verified here* (the SUSE PTF build tooling is not in these repos): the exact dependency spelling. Given libsolv's hack (B.3.3), the master most likely uses rich requires such as `(salt = 3000-30.1.22333.0.PTF.1047068 if salt)`, and the PTF packages most likely require their master. Check a real PTF rpm (`rpm -qp --provides --requires`) before you rely on this.

### B.3.2 How libzypp classifies PTF solvables

`zypp/zypp/sat/detail/PoolImpl.{h,cc}` keeps two `SolvableSpec`s, initialised lazily in `setDirty()` (they cannot be built in the ctor because of `IdString` static-init order):

| Spec | Matches | Accessor |
|---|---|---|
| `_ptfMasterSpec` | providers of `ptf()` | `Solvable::isPtfMaster()` |
| `_ptfPackageSpec` | providers of `ptf-package()` | `Solvable::isPtfPackage()` |
| — | either | `Solvable::isPtf()` |
| — | `isPtf() \|\| isRetracted()` | `Solvable::isBlacklisted()` (bsc#1186503) |

Unlike the retracted marker, the PTF provides are **real rpm provides**, so installed PTF packages match directly and need no identical-installed lookup. `sat::AllPTFs` (`zypp/zypp/sat/WhatProvides.h`) = `WhatProvides(Capability("ptf()"))`, i.e. all PTF masters, installed and available. Unit test: `zypp/tests/sat/Blacklisted_test.cc` with `tests/sat/BlacklistedPool/` (a package providing all three markers shows `BRPmp`). zyppng mirrors this in `PackagePolicyComponent`.

### B.3.3 Solver treatment (libzypp + libsolv)

1. **Masters are blacklisted, PTF packages are not.** `SATResolver::solverInit` adds `SOLVER_BLACKLIST|SOLVER_SOLVABLE_PROVIDES ptf()`. Until 17.27.x it also blacklisted `ptf-package()`. That was removed in commit 2b32af90a "Fix solver jobs for PTFs (bsc#1186503)": if the packages are blacklisted, installing the master cannot pull them in through its requires. libsolv's own `test/testcases/blacklist/ptf.t` test 1 shows exactly that failure ("A requires ptf-1, but none of the providers can be installed"). PTF packages stay out of normal updates because they are tied to the (blacklisted) master and have unusual releases. That reasoning is inferred, not enforced by code.
2. **libsolv: requires of `ptf-*` packages match names only.** In `add_complex_deprules()` (`libsolv/src/rules.c`), a rich requires of a solvable whose name starts with `ptf-` is expanded with `CPLXDEPS_NAME` (`/* do not match provides */`). libsolv commit b8c3b209 "complex requires in ptf packages should only match package names" (2019). So `(foo = X if foo)` in a PTF master is triggered by a *package named* foo, not by something that merely provides `foo`.
3. **libsolv: never auto-uninstall PTFs.** `solver_autouninstall()` (`libsolv/src/problems.c`) skips update rules of `ptf-*` packages (libsolv 0.7.23, commit 11754314). With `allowuninstall`, libsolv will therefore not silently drop an installed PTF master to resolve a conflict.
4. **Installed PTFs can be updated.** In `solver_addblackrules()`, if an *installed* solvable matches a blacklist selection, every available solvable **with the same name** is exempt from the blacklist ("installed package with same name is already blacklisted"). So `ptf-22333-1` → `ptf-22333-2` (a respin) is a normal update (ptf.t test 4). This applies to every blacklist job, so an installed retracted package with the marker would get the same exemption. That can't happen for rpmdb solvables, but it can for testcase `@System` repos.
5. **No repos → protect PTFs (bsc#1203248, 17.31.7).** If `sat::Pool::reposSize() == 1` (only `@System`), each installed PTF master gets a `SOLVER_INSTALL|SOLVER_SOLVABLE` job and `_protectPTFs` is set. Without repos, removing the master would *remove* its PTF packages instead of replacing them with official versions. As a result, a removal request turns into a problem whose solution text is **"remove lock to allow removal of ptf-…"**, which is misleading because there is no lock. The prepended bsc#1203248 hint explains that and suggests `zypper removeptf`.
6. **Problem hints (bsc#1194848).** `PtfPatchHint` in `SATResolver.cc` collects (a) patches with a "do not install" solution and (b) PTF masters with a removal solution. If any PTF is involved, the problem description is *prefixed* with one of three texts:
   * a patch is involved: "%1% is not yet fully integrated into %2%. Typically you want to keep the PTF and choose to not install the maintenance patches."
   * `_protectPTFs`: "Removing the installed %1% in this context will remove (not replace!) the included PTF-packages too. The PTF should be removed by calling 'zypper removeptf %1%'…"
   * otherwise: "The installed %1% blocks the desired action. Typically you want to keep the PTF and choose to cancel the action."
   The original description follows in parentheses.
7. **Explicitly allowing a PTF**: a `SOLVER_SOLUTION_BLACK` solution on a PTF reads "allow installing the PTF %1%".

### B.3.4 Life cycle from the user's view

```
install:  zypper in ptf-22333                       → SOLVER_INSTALL|SOLVABLE on the master
                                                    → blacklist lifted for it (SETEVR, B.4.2)
                                                    → master requires → PTF packages (not blacklisted)
patch:    zypper patch                              → patch conflicts "salt < newer"; PTF master
                                                      pins the PTF salt → problem + bsc#1194848 hint
                                                      → user keeps PTF / skips patch
fix ships: official update with the fix released    → user removes the PTF:
          zypper removeptf ptf-22333   (= zypper install -- -ptf-22333)
                                                    → master erased; PTF packages lose their tie
                                                      and are *replaced* by the newest official
                                                      versions (normal install semantics,
                                                      force-resolution off)
```

`removeptf`/`rmptf` (`zypper/src/commands/installremove.cc`, `RemovePtfCmd`, zypper 1.14.59, bsc#1203249) is a thin wrapper around `InstallCmd`: every positional argument without a leading `+~-!` gets a `-` prefix. It accepts all install options. Why it behaves differently from `zypper rm`:

* `zypper remove` defaults to **force-resolution = true** in interactive mode (`set_force_resolution()` in `zypper/src/solve-commit.cc`, bnc#369980). The solver may then resolve the broken requires by also removing the PTF packages.
* `install -- -foo` keeps force-resolution off. The solver has to find a non-removing solution if one exists, which is to update the PTF packages to their official versions.

This only works if the official repos are enabled. That is the reason for B.3.3 item 5.

Other zypper bits: the `P` status flag (`cdf321f9`, bsc#1186503). `zypper info` used to crash for not-installed PTFs because `updateCandidateObj()` is empty when everything available is blacklisted (bsc#1196317, zypper `a627a016`). `theInterestingPoolItems()` in `zypper/src/info.cc` now falls back to `candidateObj()`. Any front-end code that assumes "available ⇒ `updateCandidateObj()` non-null" has the same bug.

### B.3.5 Multiversion and PTFs

Nothing in libzypp or libsolv handles the combination specifically (verified by grep). A kernel PTF uses multiversion packages, so it is installed *next to* the official kernel (NOOBSOLETES jobs from `multiversionList()`), and `PurgeKernels` treats it like any other kernel flavour/version. Whether a kernel PTF master pins one exact kernel version is decided by the PTF package dependencies, not by libzypp. (Unverified in practice; check a real kernel PTF before documenting more.)

---

## B.4 Blacklisting in general

### B.4.1 libsolv `SOLVER_BLACKLIST` semantics

Added in libsolv 0.7.8 (commit 4120051b, Nov 2019): *"it is not possible to pull in a blacklisted package via a dependency, they can only be installed by a job that directly addresses them (the SETEVR bit is set)."*

* For each blacklist job, `solver_addblackrules()` adds a unit rule `-p` (`SOLVER_RULE_BLACK`) for every **non-installed** match, except names that have an installed match (B.3.3 item 4).
* The rule-info text for a violated black rule is "package %s can only be installed by a direct request". libzypp's `SATproblemRuleInfoString` does not handle `SOLVER_RULE_BLACK`, so this text comes out **untranslated** through the default branch ([Solver §16.2](04-solver.md#162-rule--text-mapping-satproblemruleinfostring)).
* Solutions: `SOLVER_SOLUTION_BLACK` = "allow this one", which libzypp maps to `INSTALL` (B.2.4/B.3.3).
* `solver_findproblemrule()` knows about black rules since 0.7.14.

### B.4.2 What counts as a "direct request"

`jobtodisablelist()` (`libsolv/src/rules.c`) disables a black rule (`DISABLE_BLACK`) only for `SOLVER_INSTALL` jobs that carry `SOLVER_SETEVR`:

| Job | SETEVR? | Blacklist lifted? |
|---|---|---|
| `SOLVER_INSTALL\|SOLVER_SOLVABLE` (an exact solvable) | automatically yes | **yes**. libzypp issues this for every item set `toBeInstalled` by USER or APPL_HIGH (`SATCollectTransact`; APPL_LOW and SOLVER results are dropped and re-solved), e.g. `zypper in ptf-22333`, YaST selecting a version, or applying a BLACK solution |
| `SOLVER_INSTALL\|SOLVER_SOLVABLE_NAME` with `name = evr` containing a release | yes | yes (not used by libzypp) |
| `SOLVER_INSTALL\|SOLVER_SOLVABLE_NAME` with only a version, or plain name | SETEV / none | no (ptf.t test 2) |
| `SOLVER_INSTALL\|SOLVER_SOLVABLE_PROVIDES` | not set by the auto analysis | **no**. That is what `Resolver::addRequire()` produces |

So `Resolver::addRequire(Capability("foo = 2-1"))` does **not** lift the blacklist. It produces a problem with an "install … although it has been retracted" solution (derived from the libsolv code, not tested). zypper turns requests for **locked** items into `addRequire` ([Solver §15.2](04-solver.md#152-zypper-locked-items-become-requiresconflicts)), so a locked and retracted package behaves differently from an unlocked one.

### B.4.3 What libzypp blacklists

Exactly two hard-coded jobs, added unconditionally in `SATResolver::solverInit()`. There is no config option, no file and no env var:

```cpp
SOLVER_BLACKLIST|SOLVER_SOLVABLE_PROVIDES  retracted-patch-package()
SOLVER_BLACKLIST|SOLVER_SOLVABLE_PROVIDES  ptf()
// bsc#1186503: ptfPackageToken should not be blacklisted
```

The jobs end up verbatim in `solver-test.t` testcases ([Solver §17](04-solver.md#17-test-case-format-in-depth)). The word "blacklist" also appears elsewhere in libzypp, but with unrelated meanings: the `zypper ps` / `CheckAccessDeleted` path blacklist (bsc#1226014, bsc#1079991) and libsolv's install-media blacklisting. Neither has anything to do with solvables.

`Solvable::isBlacklisted()` is the libzypp-side mirror of these two jobs **plus** `ptf-package()`. It is used for UI ordering and candidate selection, not for building jobs. That asymmetry is deliberate: a PTF package is "blacklisted" for display/candidate purposes but can be pulled in by its master.

---

## B.5 Other special packages

Only items with code behind them are listed.

| Item | Mechanism | Where |
|---|---|---|
| **Kernel live patches** (`kernel-livepatch-*`, `kernel-*-livepatch*`, legacy `kgraft-patch-*`) | No solver special-casing. Only `PurgeKernels` allows the solver to remove them as a side effect of purging a kernel: the regex `kernel-syms…\|kgraft-patch…\|kernel-(.*)-livepatch…\|kernel-livepatch…\|.*-kmp…`. "kernel-*-livepatch packages are supposed to serve as a stable handle for the ephemeral kernel livepatch packages (FATE#320268) … should not block the purge-kernels step" (bsc#1190815) | `zypp/zypp/PurgeKernels.cc`, tests in `zypp/tests/zypp/PurgeKernels_test.cc`. Live-patch *updates* reach the user as ordinary patches, usually without `reboot_suggested` |
| **Multiversion** | `zypp.conf` `multiversion = provides:multiversion(kernel)` (vendor config). Matching solvables get `SOLVER_NOOBSOLETES` jobs, and establish uses the same jobs, so a newer kernel satisfies a kernel patch while the old kernel stays | [Config](09-configuration-and-environment.md), [Solver §19.2](04-solver.md#192-status-computation-solvable_trivial_installable_map-libsolvsrcsusec) |
| **needreboot** | Packages providing `installhint(reboot-needed)` or `kernel`, plus specs from `/etc/zypp/needreboot` and `needreboot.d/*` → `Solvable::isNeedreboot()` → `/run/reboot-needed`. Only patches drive zypper exit code 102 (B.1.6) | `TargetImpl.cc` (~l.1362), [Commit §7](08-target-commit-and-rpm.md#7-needreboot-update-scripts-and-update-messages) |
| **Update stack** | Not a package property but the patch flag `restart_suggested` (B.1.6) | zypper only |

---

## Part B pitfalls

* A patch only *conflicts* with older versions (`name.arch < evr` and `name.noarch < evr`). It never requires anything, so the solver may also satisfy it by removing packages or by installing a newer version than the one listed.
* `reboot/restart/relogin_suggested` are recognised only **inside `<package>`**. One flagged package flags the whole patch. `UPDATE_COLLECTION_FLAGS` is never filled by the rpm-md parser.
* `<release>`, `<collection><name>`, `collection@short` and `package@src` are parsed but thrown away. `<rights>` is stored but not exposed by libzypp.
* `Patch::timestamp()` is the **later** of `issued` and `updated`, not the issue date.
* The retracted marker is computed by `repo2solv` at **cache build time** and only for packages in the **same repo** as the retracted patch. Any arch of the same name-evr is marked.
* Patches never get `retracted-patch-package()`. Patch retraction comes from `UPDATE_STATUS` and makes the patch NONRELEVANT. `zypper in patch:<retracted>` says "not needed".
* Installed retracted packages are recognised only through `solvable_identical` with a repo copy (same vendor and buildtime). The solver never removes or downgrades them automatically, and `dup` keeps them.
* NG (`PackagePolicyComponent`) lacks the identical-installed logic, so installed retracted packages are not flagged there.
* `isBlacklisted()` covers `ptf-package()`, but the solver does **not** blacklist `ptf-package()` (bsc#1186503). Re-adding that job breaks PTF installation.
* A blacklist is lifted only by an exact-solvable install job (or NAME with full `=evr-rel`). `addRequire()` (PROVIDES), which zypper uses for locked items, does not lift it.
* Any installed match exempts **all** available solvables of the same name from a blacklist job. That is how PTF respins update.
* libsolv special-cases the **name prefix `ptf-`**: rich requires match names only, and such packages are never auto-uninstalled. A non-PTF package named `ptf-…` would be affected too.
* With only `@System` loaded, installed PTF masters are protected by install jobs (bsc#1203248). The resulting solution text says "remove lock …" although there is no lock.
* `misc::poolInstallState()` reports PTFs as `Retracted`. In zypper's status column, `R`/`P` hide the lock flag `l`.
* `updateCandidateObj()` is null when everything available is blacklisted, which crashed `zypper info` (bsc#1196317). Front-ends must handle this.
* zypper exit codes 102/103 depend on patch flags only, and its XML `restart` attribute means *reboot*.
* `zypper remove` uses force-resolution and may drop PTF packages. `zypper removeptf` (= `install -- -X`) replaces them, but only if the official repos are enabled.

## Where to start when debugging special solvables

**Get the raw data first, before suspecting the solver:**

* `zypper -x lp --all` / `zypper info -t patch <id>`: category, severity, `pkgmanager` (= restart) and `restart` (= reboot) flags, status. `zypper se -s <pkg>`: status column `R`/`P`/`l`.
* Dump a solvable from the solv cache: `dumpsolv /var/cache/zypp/solv/<alias>/solv | less`. Search for `patch:<id>` and check its `conflicts`, `update:status`, `update:collection`, `update:reboot`/`restart`. For packages, check whether `retracted-patch-package()` is in `provides`. If the marker is missing although updateinfo says retracted: same repo? cache rebuilt? libsolv built with `SUSE`?
* In code: `sat::LookupAttr q(sat::SolvAttr::allAttr, solvable)` prints every attribute; `PoolItem::isBlacklisted/isRetracted/isPtfMaster/isPtfPackage`; `sat::AllPTFs()`.
* Products, patterns, applications, srcpackages (Part A): check the `product()`/`pattern()` marker provides and the `@System` view in the same `dumpsolv` output. For products, also `/etc/products.d/*.prod` vs the `-release` package buddy ([Pool §16.3](03-sat-pool-and-resolvables.md#163-product-zyppzyppProducthcc)).

**Solver side:**

* `zypper --debug-solver <cmd>` writes `/var/log/zypper.solverTestCase/`. In `solver-test.t`, look for the two `job blacklist provides …` lines, `job install pkg …` lines for installed PTF masters (PTF protection, bsc#1203248), and `job noobsoletes` for multiversion. Replay with `testsolv -r solver-test.t` and try variants (ptf.t-style `[setevr]`, removing the blacklist line) ([Solver §17](04-solver.md#17-test-case-format-in-depth)).
* The libsolv reference cases are `libsolv/test/testcases/blacklist/{ptf,retracted}.t`. Run them after any libsolv bump.
* `/var/log/zypp/zypper.log`: `SATResolver` logs every solution description at MIL ("install … although it has been retracted", "allow installing the PTF …"). `INT` lines from `Patch::categoryEnum`/`severityFlag` indicate unknown category or severity values in metadata.
* "Patch needed/not needed is wrong" → check `EstablishedStates` (computed when the pool serial changes, not per solve). Then check the vendor relevance rule (B.1.10) and multiversion. History `patch` lines (`/var/log/zypp/history`) show recorded state transitions.

**Tests:** `zypp/tests/sat/Blacklisted_test.cc` (`BlacklistedPool/`), `zypp/tests/zypp/PurgeKernels_test.cc` (livepatch regex), `zypp/tests/parser/HistoryLogReader_test.cc` (patch history lines). There is no libzypp unit test for `PtfPatchHint` or `_protectPTFs`. Use a hand-made testcase with only `@System` plus an installed `ptf()` provider.
