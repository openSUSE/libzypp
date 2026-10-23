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

## Suggested reading order

1. Read **00**, then **00a** (concepts; come back to its glossary whenever a
   term is unclear), then **01** to get oriented.
2. Skim **02** so the idioms (`ZYPP_THROW`, `RW_pointer`, `MIL <<`, reports) make sense.
3. Read **03 → 04 → 05 → 08**. That is the core data flow: pool → solver →
   repos → commit.
4. Read **13** and **14** and keep them at hand when reviewing PRs.
5. Before your first release, read **12** and **11**.
6. If you are working on the migration, read **10** together with
   `AGENTS.md` in the source root.

## Keeping this handbook alive

* These documents were derived from the source at the time of writing (branch
  `ng-config`, Oct 2026). Code moves fast, especially under `zyppng/` and
  `zypp-logic/*/ng/`. When you change a mechanism described here, update the
  chapter in the same PR.
* Statements are backed by file references. If a reference no longer exists,
  treat the paragraph as suspect.
