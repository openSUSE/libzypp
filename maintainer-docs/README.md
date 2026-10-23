# libzypp Maintainer Handbook

> [!WARNING]
> **AI-generated content: verify before relying on it.**
> This handbook was generated with the help of an AI assistant (October 2026).
> The assistant analysed the libzypp source code, the git history,
> `zypp/package/libzypp.changes` and, for some chapters, related projects
> (libsolv, zypper, public OBS/GitHub data). The text was not written or fully
> reviewed by a human expert.
>
> * It **may contain hallucinations**: wrong statements, misread code,
>   incorrect bug numbers or invented details that sound plausible.
> * File, class and function references can be wrong or outdated. Always check
>   them against the actual code before acting on them.
> * Items marked *"unverified"*, *"likely bug"* or *"suspected bug"* are
>   **hypotheses from code reading only**. None of them were reproduced.
> * Treat the source code as the only source of truth. Use this handbook as a
>   map for finding your way around, not as a specification.
>
> If you find an error, please fix it in the affected chapter.

This directory is a maintainer-focused guide to the libzypp source tree. It is
for people who need to fix bugs, review pull requests, cut releases and steer
the ongoing migration to the next-generation (NG) code. It is not user
documentation (the man pages and the doxygen docs in `zypp/doc/` cover that). It
explains **how the code is built and why**, plus the many historic special
cases you will run into.

All paths in this handbook are relative to the `libzypp/` source root unless
stated otherwise.

## Chapters

| # | Chapter | What you learn |
|---|---------|----------------|
| 00 | [Big picture](00-big-picture.md) | What libzypp is, who uses it, the end-to-end flow of a `zypper install`, the main objects |
| 00a | [Concepts primer & glossary](00a-concepts-and-glossary.md) | Every core concept explained from first principles (NEVRA, Edition, Arch, solvable vs PoolItem vs Selectable, capabilities, patch/pattern/product, the three meanings of "buddy", ResStatus causers, locks, vendor stickiness, DUP vs update, keyrings, media/mirrors, tiers) plus a glossary of about 260 terms |
| 01 | [Layout, build & dependencies](01-layout-build-and-dependencies.md) | Directory map, CMake options, targets, installed helpers, devel builds |
| 02 | [Core infrastructure](02-core-infrastructure.md) | Logging, exceptions, smart pointers, PIMPL, Url/Pathname, ExternalProgram, callbacks/reports |
| 03 | [Sat pool & resolvables](03-sat-pool-and-resolvables.md) | libsolv integration, IdString, Capability, Edition, Arch, ResPool, PoolItem, ResStatus, Selectable, locks, vendors |
| 03a | [Special solvables](03a-special-solvables.md) | How products, patterns, applications, srcpackages, patches, retracted packages and PTFs are **defined** (release-package provides, `.prod` files, pattern packages, `updateinfo.xml`, `ptf()`/`ptf-package()`), how libsolv converts them, how libzypp models them, blacklisting, quirks |
| 03b | [Modalias](03b-modalias.md) | Hardware-dependent packages: drivers, firmware, KMPs, CPU vendor (AMD/Intel) and CPU-feature packages. Spec syntax, libsolv rewriting, matching rules, solver behaviour, cookbook, pitfalls |
| 03c | [ResPool, PoolItem & Selectable](03c-respool-and-selectables.md) | How to **work** with the pool: the three layers and who uses which, ResPool lookups, PoolItem status and causers, Selectable candidates and status, worked zypper examples, invalidation |
| 04 | [Solver](04-solver.md) | Resolver, SATResolver, job building, solver flags, update/dup/verify, problems & solutions, testcases |
| 05 | [Repositories & services](05-repositories-and-services.md) | RepoManager, RepoInfo, services, repo variables, refresh & caches, metadata parsers |
| 06 | [Security, GPG & keyrings](06-security-gpg-keyring.md) | KeyRing, gpgme, repo/pkg GPG checks, rpm key import, checksums, credentials |
| 07 | [Media & downloads](07-media-and-downloads.md) | MediaManager and handlers, MediaCurl vs MediaCurl2, mirrors/metalink, credentials/proxy, NG provide workers |
| 08 | [Target, commit & rpm](08-target-commit-and-rpm.md) | TargetImpl, RpmDb, zypp-rpm single transaction, history, lock, plugins |
| 09 | [Configuration & environment](09-configuration-and-environment.md) | ZConfig / zypp.conf, every environment variable, the new NG config system |
| 10 | [zyppng (next generation)](10-zyppng-next-generation.md) | NG architecture, async/sync logic, C++20 modules, zypp-glib, migration state |
| 11 | [Testing](11-testing.md) | Test suite layout, running tests, test webserver, solver testcases, reproducing bugs |
| 12 | [Release, packaging & compatibility](12-release-packaging-and-compat.md) | Versioning, changelog, spec, symbol map, ABI rules, release checklist |
| 13 | [Pitfalls & special cases (cross-cutting)](13-pitfalls-and-special-cases.md) | Condensed list of the gotchas from all chapters |
| 14 | [Consumers & API surface](14-consumers-and-api-surface.md) | How zypper, YaST, libyui, PackageKit, Agama, snapper and connect-ng use libzypp; what must never change |
| 15 | [Bug triage guide](15-bug-triage-guide.md) | Symptom → subsystem → log strings to grep → likely causes → data to request; historic bug clusters; escalation map |
| 16 | [Solver crash course](16-solver-crash-course.md) | Exercise-driven learning path for triaging and fixing solver bugs, using real testcases |
| 17 | [SLE migration](17-sle-migration.md) | `zypper migration` and YaST migration end to end: who does what (SCC/RMT, connect-ng, zypper, libzypp, libsolv), the exact command sequence, migration-specific DUP features, SLE 15 → 16 transitions, failure catalogue with real bugs, reproduction |
| 18 | [patch vs update vs dist-upgrade](18-patch-update-dup.md) | What `zypper patch`, `update`, `dist-upgrade` and `install` do differently: libzypp entry points, a tested behaviour matrix for retracted packages, PTFs, vendor, blocked updates; zypper's patch policy; exit codes; triage FAQ |

## Suggested reading order

1. Read **00**, then **00a** (concepts; come back to its glossary whenever a
   term is unclear), then **01** to get oriented.
2. Skim **02** so the idioms (`ZYPP_THROW`, `RW_pointer`, `MIL <<`, reports) make sense.
3. Read **03c**, then **03 → 04 → 05 → 08**. That is the core data flow: pool →
   solver → repos → commit.
4. Read **13** and **14** and keep them at hand when reviewing PRs.
5. Before your first release, read **12** and **11**.
6. Before your first change, read [CONTRIBUTING.md](../CONTRIBUTING.md). If you
   work on the (paused) NG code, read **10** together with
   [zyppng/CONTRIBUTING.md](../zyppng/CONTRIBUTING.md).

## Reading paths by task

Each path lists the sections to read, in order. The chapters stay organised by
subsystem; these paths cut across them.

**Fix a security issue (CVE)**
1. Earlier CVEs and the code areas they touched: [06 §17](06-security-gpg-keyring.md#17-cve-history-from-zypppackagelibzyppchanges)
2. The affected flow: repo metadata [06 §12](06-security-gpg-keyring.md#12-end-to-end-repo-metadata-gpg-decision-flow), packages [06 §14](06-security-gpg-keyring.md#14-package-signature-verification-end-to-end), TLS/credentials [07 §7.16](07-media-and-downloads.md#716-http-authentication-end-to-end), plugins [08 §9](08-target-commit-and-rpm.md#9-plugins)
3. Consumers that depend on callback defaults: [14 §7](14-consumers-and-api-surface.md#7-never-change-behaviours-without-a-coordinated-plan)
4. Write a regression test: [11 §12](11-testing.md#12-writing-tests-step-by-step-with-skeletons)
5. Release it: [12 §11](12-release-packaging-and-compat.md#11-how-to-do-a-release-checklist)

**Triage a repository refresh / download failure**
1. Symptoms and log strings: [15 §2](15-bug-triage-guide.md#2-symptom-catalogue)
2. Refresh logic and caches: [05 §5](05-repositories-and-services.md#5-refresh-deciding-downloading-caching)
3. Signature decisions: [06 §12](06-security-gpg-keyring.md#12-end-to-end-repo-metadata-gpg-decision-flow)
4. HTTP backend, mirrors, auth, proxy: [07 §7.2](07-media-and-downloads.md#72-selecting-the-http-backend), [07 §7.5](07-media-and-downloads.md#75-mirrors-in-the-media-layer-mirroredorigin), [07 §7.16](07-media-and-downloads.md#716-http-authentication-end-to-end), [07 §7.17](07-media-and-downloads.md#717-proxy-variants-and-curlconfig-details-to-78)
5. Repo variables (`$releasever` …): [05 §6](05-repositories-and-services.md#6-repo-variables-zyppzyppreporepovariableshcc)

**Triage a solver / dependency bug**
1. Get and replay a testcase: [11 §15](11-testing.md#15-bug-reproduction-workflow), format in [04 §17](04-solver.md#17-test-case-format-in-depth)
2. Learn the method: [16 Solver crash course](16-solver-crash-course.md)
3. Concepts: [00a §A.7](00a-concepts-and-glossary.md#a7-pool-and-status), [00a §A.8](00a-concepts-and-glossary.md#a8-solver-concepts)
4. Mechanics: [04 §3](04-solver.md#3-a-solver-run-step-by-step-resolvepool), [04 §5](04-solver.md#5-modes), [04 §14](04-solver.md#14-end-to-end-flag-and-job-reference), [04 §16](04-solver.md#16-problems-and-solutions-in-depth)
5. Special solvables (products, patches, PTFs, retracted): [03a](03a-special-solvables.md); hardware packages: [03b](03b-modalias.md)

**Why wasn't X updated? (patch / update / dup questions)**
1. Compare the operations: [18 §1](18-patch-update-dup.md#1-the-operations-at-a-glance), [18 §3](18-patch-update-dup.md#3-behaviour-matrix-special-cases)
2. FAQ with owners: [18 §6](18-patch-update-dup.md#6-faq-for-triage)
3. Concepts: patches [03a §B.1](03a-special-solvables.md#b1-patches-the-metadata), retracted [03a §B.2](03a-special-solvables.md#b2-retracted-packages-and-patches), PTFs [03a §B.3](03a-special-solvables.md#b3-ptfs-program-temporary-fixes)

**Triage a migration bug (`zypper migration`, SP or major upgrade)**
1. Who owns which step: [17 §2](17-sle-migration.md#2-who-does-what), the step sequence: [17 §3](17-sle-migration.md#3-zypper-migration-step-by-step)
2. Symptom → owner: [17 §8](17-sle-migration.md#8-failure-catalogue)
3. Solver side: [17 §5](17-sle-migration.md#5-dup-features-that-exist-because-of-migrations), [04 §5.3](04-solver.md#53-doupgrade-dist-upgrade); why fixes must reach the *source* system: [17 §7](17-sle-migration.md#7-a-lesson-from-bsc1279541-the-old-systems-solver-does-the-work)
4. Reproduce: [17 §9](17-sle-migration.md#9-reproducing-and-debugging)

**Triage a commit / rpm failure**
1. Symptoms: [15 §2](15-bug-triage-guide.md#2-symptom-catalogue)
2. Which rpm backend ran: [08 §5](08-target-commit-and-rpm.md#5-backend-selection-classic_rpmtrans-vs-single_rpmtrans)
3. The pipeline and download during commit: [08 §6](08-target-commit-and-rpm.md#6-the-commit-pipeline-in-detail-targetimplcommitrespool-policy), [08 §15](08-target-commit-and-rpm.md#15-package-provisioning-during-commit-in-depth)
4. How results and errors are reported: [08 §16](08-target-commit-and-rpm.md#16-commit-result-evaluation-and-rpm-problem-reporting)

**Add or change a zypp.conf option**
1. How the config is located and merged: [09 §2](09-configuration-and-environment.md#2-locating-and-merging-zyppconf)
2. How ZConfig reads it and handles root changes: [09 §3](09-configuration-and-environment.md#3-zconfig-legacy-architecture), [09 §4](09-configuration-and-environment.md#4-zconfig-and-root-changes)
3. Existing keys and docs that must be updated with it: [09 §5](09-configuration-and-environment.md#5-all-zyppconf-keys-read-by-zconfig--mediaconfig), [09 §11](09-configuration-and-environment.md#11-zyppconf-key-audit-parser-vs-documentation)
4. The NG config registry: [09 §15](09-configuration-and-environment.md#15-ng-config-complete-key-registry-and-branch-status)
5. ABI impact of new getters: [12 §12](12-release-packaging-and-compat.md#12-reviewing-a-pr-for-abi-safety-checklist)

**Review a pull request**
1. ABI checklist: [12 §12](12-release-packaging-and-compat.md#12-reviewing-a-pr-for-abi-safety-checklist), symbol export: [12 §5](12-release-packaging-and-compat.md#5-symbol-visibility-and-export-control)
2. Who could break: [14 §6](14-consumers-and-api-surface.md#6-api-area--consumers--risk), [14 §8](14-consumers-and-api-surface.md#8-checklist-before-changing-x-check-y)
3. Known traps in the touched area: [13](13-pitfalls-and-special-cases.md) and the chapter's own Pitfalls section
4. Rules for changes: [CONTRIBUTING.md](../CONTRIBUTING.md); NG tier rules: [zyppng/CONTRIBUTING.md](../zyppng/CONTRIBUTING.md#design-three-tiers), [10 §11](10-zyppng-next-generation.md#11-migration-rules-and-status)

**Cut a release**
1. Checklist: [12 §11](12-release-packaging-and-compat.md#11-how-to-do-a-release-checklist), a real example: [12 §14](12-release-packaging-and-compat.md#14-anatomy-of-a-real-release-git-evidence)
2. Version and soname rules: [12 §2](12-release-packaging-and-compat.md#2-versioning-zyppversioncmake)
3. OBS submission: [12 §13](12-release-packaging-and-compat.md#13-obs-projects-and-the-submission-pipeline), zypper coupling: [12 §17](12-release-packaging-and-compat.md#17-coordinating-with-zypper-release-coupling)

**Work on the NG code**
1. What already runs in production: [10 §6](10-zyppng-next-generation.md#6-how-legacy-libzypp-uses-ng-code-today-production-paths)
2. Async framework and the sync/async pattern: [10 §3](10-zyppng-next-generation.md#3-the-async-framework-zypp-logiczypp-coreng), [10 §4](10-zyppng-next-generation.md#4-logic-written-once-run-sync-or-async), [10 §17](10-zyppng-next-generation.md#17-worked-example-one-source-sync-in-legacy-async-in-ng)
3. Building it: [10 §10](10-zyppng-next-generation.md#10-building-ng), [10 §18](10-zyppng-next-generation.md#18-toolchain-details-for-the-module-build)

## Topic index (A–Z)

For topics that several chapters touch, **Home** is the one place that explains
them fully. Other chapters should keep only the detail they need and link
there.

| Topic | Home | Also relevant |
|---|---|---|
| ABI, symbol export, `libzypp.map`, `ZYPP_API` | [12 §5](12-release-packaging-and-compat.md#5-symbol-visibility-and-export-control) | [12 §12](12-release-packaging-and-compat.md#12-reviewing-a-pr-for-abi-safety-checklist), [14](14-consumers-and-api-surface.md) |
| Arch detection, `x86_64_v2/v3/v4` | [09 §6](09-configuration-and-environment.md#6-architecture-detection) | [03 §4](03-sat-pool-and-resolvables.md#4-value-types-built-on-the-pool) |
| AutoInstalled, unneeded, orphaned | [00a §A.8](00a-concepts-and-glossary.md#a8-solver-concepts) | [04 §3.4](04-solver.md#34-gettransaction-and-autoinstalled-tracking), [08 §17](08-target-commit-and-rpm.md#17-persistent-state-files-at-commit-in-depth) |
| Buddy (all meanings) | [00a §A.5](00a-concepts-and-glossary.md#a5-buddy--all-meanings) | [03a §A2](03a-special-solvables.md#a2-products) |
| Build system, CMake options | [01](01-layout-build-and-dependencies.md) | |
| Callbacks / reports | [02 §11](02-core-infrastructure.md#11-callback--report-system-zyppzyppcallbackh) | [14](14-consumers-and-api-surface.md) |
| Capability, dependencies, rich deps | [00a §A.3](00a-concepts-and-glossary.md#a3-dependencies) | [03 §4](03-sat-pool-and-resolvables.md#4-value-types-built-on-the-pool), [04 §18](04-solver.md#18-recommends-supplements-namespaces-and-rich-dependencies) |
| Commit pipeline | [08 §6](08-target-commit-and-rpm.md#6-the-commit-pipeline-in-detail-targetimplcommitrespool-policy) | [08 §15](08-target-commit-and-rpm.md#15-package-provisioning-during-commit-in-depth), [08 §16](08-target-commit-and-rpm.md#16-commit-result-evaluation-and-rpm-problem-reporting) |
| Consumers (zypper, YaST, PackageKit, …) | [14](14-consumers-and-api-surface.md) | |
| Credentials, HTTP auth | [07 §7.16](07-media-and-downloads.md#716-http-authentication-end-to-end) | [06 §9](06-security-gpg-keyring.md#9-media-level-security) |
| CVE history | [06 §17](06-security-gpg-keyring.md#17-cve-history-from-zypppackagelibzyppchanges) | |
| Delta rpms, package cache | [05 §10](05-repositories-and-services.md#10-packageprovider-in-depth-zyppzypprepopackageproviderhcc) | |
| Disk usage | [08 §12](08-target-commit-and-rpm.md#12-disk-usage-estimation-diskusagecounter) | |
| Download settings, HTTP backend (MediaCurl / MediaCurl2) | [07 §7.2](07-media-and-downloads.md#72-selecting-the-http-backend) | [07 §7.9](07-media-and-downloads.md#79-download-settings-zyppconf-and-env-vars) |
| Environment variables | [09 §8](09-configuration-and-environment.md#8-environment-variables-complete-list) | [09 §12](09-configuration-and-environment.md#12-environment-variables-libzypp-sets-and-other-env-side-effects) (variables libzypp sets) |
| Exceptions | [02 §4](02-core-infrastructure.md#4-exceptions-zypp-logiczypp-corebaseexceptionhcc) | |
| History log | [08 §8](08-target-commit-and-rpm.md#8-historylog-varlogzypphistory) | |
| Keyring, GPG keys | [06](06-security-gpg-keyring.md) | [06 §15](06-security-gpg-keyring.md#15-key-lifecycle) |
| Locks (package locks, `/etc/zypp/locks`) | [03 §10](03-sat-pool-and-resolvables.md#10-locks-poolquery-vendorattr-multiversion) | [04 §11](04-solver.md#11-locks-and-the-solver), [08 §17](08-target-commit-and-rpm.md#17-persistent-state-files-at-commit-in-depth) |
| Lock, global (`/run/zypp.pid`) | [08 §2](08-target-commit-and-rpm.md#2-zypp-singleton-and-the-global-lock) | [14](14-consumers-and-api-surface.md) |
| Logging and debug switches | [02 §3](02-core-infrastructure.md#3-logging) | [15 §1](15-bug-triage-guide.md#1-triage-workflow) |
| Media handlers (CD, ISO, NFS, …) | [07 §7.6](07-media-and-downloads.md#76-the-concrete-non-network-handlers-short-reference) | [07 §7.19](07-media-and-downloads.md#719-mount-based-handlers-in-depth) |
| Mirrors, metalink, GeoIP | [07 §7.5](07-media-and-downloads.md#75-mirrors-in-the-media-layer-mirroredorigin) | [05 §11](05-repositories-and-services.md#11-repomirrorlist-in-depth-zyppzyppreporepomirrorlisthcc) |
| Migration (SLE SP/major, `zypper migration`) | [17](17-sle-migration.md) | [04 §5.3](04-solver.md#53-doupgrade-dist-upgrade) |
| Modalias, hardware packages | [03b](03b-modalias.md) | |
| Multiversion, PurgeKernels | [04 §13](04-solver.md#13-purgekernels-zyppzypppurgekernelshcc) | [03 §10](03-sat-pool-and-resolvables.md#10-locks-poolquery-vendorattr-multiversion) |
| Namespaces (language, modalias, filesystem) | [03 §9](03-sat-pool-and-resolvables.md#9-namespaces-language-modalias-filesystem) | [04 §12](04-solver.md#12-namespaces-resolvernamespace), [03b](03b-modalias.md) |
| NG code in production | [10 §6](10-zyppng-next-generation.md#6-how-legacy-libzypp-uses-ng-code-today-production-paths) | |
| Patches (status, update stack) | [03a §B.1](03a-special-solvables.md#b1-patches-the-metadata) | [04 §8](04-solver.md#8-patches-patterns-and-the-satisfiedbrokenneeded-status), [04 §19](04-solver.md#19-patches-from-libsolvs-point-of-view) |
| Patterns | [03a §A3](03a-special-solvables.md#a3-patterns) | |
| patch vs update vs dist-upgrade (operations) | [18](18-patch-update-dup.md) | [04 §5](04-solver.md#5-modes) |
| Plugins (commit, services, verification, …) | [08 §9](08-target-commit-and-rpm.md#9-plugins) | [06 §11](06-security-gpg-keyring.md#11-metadata-verification-plugins), [05 §7](05-repositories-and-services.md#7-services) |
| PoolQuery | [03 §13](03-sat-pool-and-resolvables.md#13-poolquery-in-depth) | |
| Products, base product, `/etc/products.d` | [03a §A2](03a-special-solvables.md#a2-products) | [08 §13](08-target-commit-and-rpm.md#13-products-on-the-target-etcproductsd-baseproduct-releasever) |
| Progress reporting | [02 §13](02-core-infrastructure.md#13-progress-reporting-progressdata-combinedprogressdata-progressobserver) | |
| PTFs, retracted, blacklisting | [03a §B.3](03a-special-solvables.md#b3-ptfs-program-temporary-fixes) | [03a §B.2](03a-special-solvables.md#b2-retracted-packages-and-patches), [03a §B.4](03a-special-solvables.md#b4-blacklisting-in-general) |
| Release process | [12 §11](12-release-packaging-and-compat.md#11-how-to-do-a-release-checklist) | |
| Repo GPG checks (`repo_gpgcheck` …) | [06 §7](06-security-gpg-keyring.md#7-gpgcheck-semantics-repoinfo--zconfig) | [06 §12](06-security-gpg-keyring.md#12-end-to-end-repo-metadata-gpg-decision-flow) |
| Repo refresh, raw/solv caches | [05 §5](05-repositories-and-services.md#5-refresh-deciding-downloading-caching) | |
| Repo variables | [05 §6](05-repositories-and-services.md#6-repo-variables-zyppzyppreporepovariableshcc) | |
| ResPool, PoolItem (using them) | [03c](03c-respool-and-selectables.md) | [03 §7](03-sat-pool-and-resolvables.md#7-respool-poolitem-resstatus) (internals) |
| ResStatus, causers | [03 §7](03-sat-pool-and-resolvables.md#7-respool-poolitem-resstatus) | [03c §3](03c-respool-and-selectables.md#3-poolitem), [00a §A.7](00a-concepts-and-glossary.md#a7-pool-and-status) |
| rpm backends (classic / single transaction, `zypp-rpm`) | [08 §5](08-target-commit-and-rpm.md#5-backend-selection-classic_rpmtrans-vs-single_rpmtrans) | |
| Selectable, ResPoolProxy | [03c §4](03c-respool-and-selectables.md#4-uiselectable) | [03 §12](03-sat-pool-and-resolvables.md#12-uiselectable-and-respoolproxy-how-yast-sees-the-pool) (internals), [00a §A.7](00a-concepts-and-glossary.md#a7-pool-and-status) |
| Services (RIS, plugin services) | [05 §7](05-repositories-and-services.md#7-services) | |
| Solver flags | [04 §14](04-solver.md#14-end-to-end-flag-and-job-reference) | [04 §4](04-solver.md#4-solver-flags) |
| Solver history (built-in → satsolver → libsolv), per codestream | [04 §2.1](04-solver.md#21-solver-history-old-solver--satsolver--libsolv) | [12 §18](12-release-packaging-and-compat.md#18-maintenance-branches-verified-ref-listing) |
| Solver modes (update, dup, verify) | [04 §5](04-solver.md#5-modes) | |
| Solver problems and solutions | [04 §16](04-solver.md#16-problems-and-solutions-in-depth) | |
| Solver testcases | [04 §17](04-solver.md#17-test-case-format-in-depth) | [11 §15](11-testing.md#15-bug-reproduction-workflow) |
| SystemCheck | [04 §10](04-solver.md#10-systemcheck) | |
| Tests (running, writing) | [11](11-testing.md) | |
| Translations | [12 §15](12-release-packaging-and-compat.md#15-translation-workflow-in-detail) | [02 §18](02-core-infrastructure.md#18-gettext-and-translations-in-code) |
| Url, Pathname | [02 §8](02-core-infrastructure.md#8-url-zypp-logiczypp-coreurlhcc-urlurlbasehcc-urlurlutilshcc) | [02 §7](02-core-infrastructure.md#7-pathname-pathinfo-tmppath) |
| Vendor equivalence | [03 §10](03-sat-pool-and-resolvables.md#10-locks-poolquery-vendorattr-multiversion) | |
| zypp.conf keys | [09 §5](09-configuration-and-environment.md#5-all-zyppconf-keys-read-by-zconfig--mediaconfig) | |

## Keeping this handbook alive

* These documents were derived from the source at the time of writing (branch
  `ng-config`, Oct 2026). Code moves fast, especially under `zyppng/` and
  `zypp-logic/*/ng/`. When you change a mechanism described here, update the
  chapter in the same PR.
* Statements are backed by file references. If a reference no longer exists,
  treat the paragraph as suspect.
* Each chapter's table of contents is generated. After changing headings, run
  `tools/toc.py`. Before committing, run `tools/check-links.py`, which reports
  broken links and anchors and warns about `§` references without a matching
  heading.
