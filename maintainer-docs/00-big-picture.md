# 00 – The Big Picture

This chapter explains what libzypp is, who depends on it, how the source tree
is organised into three tiers, and how one typical operation (`zypper install
foo`) runs through the library from start to finish. The later chapters go
into each step in detail.

## What libzypp is

libzypp is the package management library of openSUSE and SUSE Linux
Enterprise (SLE). It provides:

* **Repository management**: `.repo`/`.service` files, metadata download and
  refresh, and caching of the metadata as libsolv `.solv` files.
  See [05](05-repositories-and-services.md).
* **A resolvable object model** on top of libsolv's pool (packages, patches,
  patterns, products, source packages, applications).
  See [03](03-sat-pool-and-resolvables.md).
* **Dependency resolution** by driving the libsolv solver, with
  SUSE-specific policies (vendor stickiness, locks, multiversion kernels, PTFs,
  retracted packages, patches). See [04](04-solver.md).
* **Media access** for http(s)/ftp, mirrors/metalink, CD/DVD, ISO, NFS, CIFS,
  local dirs and plugins. See [07](07-media-and-downloads.md).
* **Trust**: GPG verification of repo metadata and packages, key management,
  and rpm key import. See [06](06-security-gpg-keyring.md).
* **Commit** (installing and removing rpms), either per package or as a single
  rpm transaction through the `zypp-rpm` helper. Commit also covers history
  logging, update messages and the reboot-needed hint.
  See [08](08-target-commit-and-rpm.md).

## Who depends on it (your compatibility surface)

| Consumer | How it uses libzypp | Why you care |
|----------|--------------------|--------------|
| **zypper** (`../zypper` in the stack) | Main CLI. It uses almost the entire legacy API, including callbacks/reports and `zypp-tui`. | Every behaviour change shows up here first. Release libzypp and zypper together when the API changes. |
| **YaST** (`yast2-pkg-bindings`, libyui package selector) | Uses `ui::Selectable`, `ResPoolProxy`, `ResStatus` causers (`USER` vs `APPL_*`), callbacks. | Very sensitive to `ResStatus` and Selectable semantics. ABI must stay stable within a codestream. |
| **PackageKit** zypp backend | Uses the legacy API (repo management, resolver, commit). | It is a long-running daemon, so watch out for singletons, the lock and stale caches. |
| Others (e.g. `snapper` zypp plugin, `zypp-plugin-*`, SUSEConnect via services, Salt/Ansible via zypper) | Commit plugins, services, the zypper CLI. | Changes to plugin protocols, the service format and the lock affect them. |
| **libzypp-glib** (`zyppng/api/zypp-glib`) | The future GObject API built on NG code. | It is experimental. See [10](10-zyppng-next-generation.md). |

Both **SLE maintenance codestreams** and **Tumbleweed/Factory** are built from
this code. Distro-dependent behaviour is switched in the spec file and through
`LIBZYPP_CODESTREAM`. See [12](12-release-packaging-and-compat.md).

## The three tiers in the source tree

```
libzypp/
├── zypp/        Tier 3 – LEGACY libzypp.so (C++17 impl, C++11 public headers). Frozen, ABI-stable.
├── zypp-logic/  Tier 2 – shared sources compiled into BOTH legacy and NG.
│                        Frozen except the */ng/ subtrees.
└── zyppng/      Tier 1 – NG rewrite, C++20 modules, libzypp-glib, media worker tools.
```

* The shipped product today is still **legacy libzypp** (`zypp/`), but much
  of the code it compiles physically lives in `zypp-logic/`. Basic types
  (`Url`, `Pathname`, `Arch`, `Edition`, `IdString`, ...), parts of the sat
  layer, curl/media helpers, the NG async framework (`zypp-logic/zypp-core/ng`)
  and the NG network stack all live there.
* **NG code already runs in production inside legacy.** `zypp::RepoManager` is
  a thin wrapper around `zyppng::RepoManager` (in `zypp/zypp/ng/`, run
  synchronously). The default HTTP backend on 16/Tumbleweed (`MediaCurl2`) uses
  the NG `NetworkRequestDispatcher`, and logging, the commit preloader and
  zypp-rpm communication use the NG event loop. Legacy does **not** use the
  out-of-process `zypp-media-*` workers. Those ship only with libzypp-glib. See
  [10](10-zyppng-next-generation.md).
* When you look for a class, **search both `zypp/zypp/` and `zypp-logic/`**.
  The include path (`<zypp/Arch.h>`, `<zypp-core/Url.h>`) does not tell you
  which directory the file is in.
* `AGENTS.md` in the source root defines the tier rules: no new features in
  legacy, new logic goes into `zyppng/`, and `// LEGACY-REQUIRED` tags. Enforce
  these rules in reviews.

## The main objects (legacy API)

```
                         ZYppFactory::instance().getZYpp()   (acquires the zypp lock)
                                        │
                                        ▼
                                   ZYpp (singleton, ZYppImpl)
              ┌──────────────┬──────────┼───────────────┬───────────────┐
              ▼              ▼          ▼               ▼               ▼
           Target         ResPool    Resolver        KeyRing         ZConfig (singleton)
     (installed system,  (all items  (solver         (trusted/       (zypp.conf, arch,
      RpmDb, @System      from all    front-end)      general keys)   paths, env)
      repo, commit)       repos)
                             ▲
                             │ filled by
                         RepoManager  ── RepoInfo / ServiceInfo (.repo/.service files)
                             │
                     MediaSetAccess / Fetcher / MediaManager  ── network, CD, ISO, ...
```

* `sat::Pool` is the libsolv pool wrapper and the single source of truth for
  solvables. `ResPool` is the libzypp view on top of it. It pairs every solvable
  with a `ResStatus` (as a `PoolItem`) and provides `ui::Selectable` (grouping
  by kind and name) for UIs.
* Most "value" classes are cheap handles. Some wrap libsolv ids (`IdString`,
  `Capability`, `sat::Solvable`). Others are copy-on-write PIMPLs
  (`RWCOW_pointer`). See [02](02-core-infrastructure.md).
* Applications receive progress and questions through **reports/callbacks**
  (`zypp/zypp/ZYppCallbacks.h`, `zypp/zypp/Callback.h`). zypper and YaST
  install receivers for them. Without a receiver, every question gets its
  default answer. That is a frequent source of surprises in tools and tests.

## End-to-end: what happens during `zypper install foo`

```
 1. getZYpp()                    ZYppFactory: global lock <lockfileDir>/zypp.pid
                                 (wait governed by ZYPP_LOCK_TIMEOUT, bsc#1184399,1213231)
                                 also checks zypp-rpm.pid so a running commit is not disturbed
 2. ZConfig::instance()          reads zypp.conf (ZYPP_CONF / /etc / /usr/etc + .d drop-ins)
 3. KeyRing / Target init        zypp->initializeTarget("/")  → TargetImpl, RpmDb
                                 target->load(): build/load @System solv cache from the rpmdb
                                 (rebuilt when the rpmdb cookie changes)
 4. RepoManager                  for each enabled repo:
       refreshMetadata()         (if autorefresh and expired, see repo.refresh.delay)
                                 download repomd.xml/content + signature
                                 → KeyRing verify → raw cache /var/cache/zypp/raw/<alias>
       buildCache()              repo2solv → /var/cache/zypp/solv/<alias>/solv
       loadFromCache()           sat::Pool::addRepoSolv()
 5. Select                       PoolItem/Selectable status: setToBeInstalled(ResStatus::USER)
                                 (zypper turns "foo" into a solver job, see 04)
 6. Resolve                      zypp->resolver()->resolvePool()
                                 Resolver → SATResolver: builds the libsolv job queue from
                                 ResStatus + locks + flags, runs solver_solve(),
                                 writes the result back as SOLVER-caused transactions,
                                 or problems → ResolverProblem/ProblemSolution
 7. Commit                       zypp->commit(ZYppCommitPolicy)
                                 TargetImpl::commit: commit plugins, compute the
                                 order (sat::Transaction), download via
                                 RepoProvidePackage/PackageProvider (+ delta rpm, cache),
                                 check signatures, then either
                                   a) classic: one rpm call per package via RpmDb, or
                                   b) single transaction: spawn zypp-rpm, stream packages
                                      and steps over the commit protocol
                                 history log (/var/log/zypp/history), update messages,
                                 needreboot, %posttrans handling, AutoInstalled update
 8. Exit                         lock released, Target and solv cache written
```

The same flow is used by YaST and PackageKit. They only drive steps 4–7 through
their own UI. The detailed mechanics are in chapters 03–08.

## Where the important on-disk state lives

| Path (default) | Owner chapter | Content |
|----------------|---------------|---------|
| `/etc/zypp/zypp.conf`, `/usr/etc/zypp/zypp.conf`, `*.conf.d/` | 09 | Configuration |
| `/etc/zypp/repos.d/*.repo`, `/etc/zypp/services.d/*.service` | 05 | Repo and service definitions |
| `/etc/zypp/vars.d/` | 05 | Custom repo variables |
| `/etc/zypp/locks` | 03/04 | Package locks |
| `/etc/zypp/vendors.d/` | 03 | Vendor equivalence |
| `/etc/zypp/credentials.d/`, `~/.zypp/credentials.cat` | 06 | Repo credentials |
| `/etc/zypp/systemCheck`, `systemCheck.d/` | 04 | Extra requirements checked by verify |
| `/etc/zypp/needreboot`, `needreboot.d/` | 08 | Packages that trigger the reboot hint |
| `/var/cache/zypp/raw/`, `solv/`, `packages/`, `pubkeys/` | 05/06 | Metadata, solv caches, kept packages, key cache |
| `/var/lib/zypp/AutoInstalled`, `RequestedLocales`, `LastDistributionFlavor`, `AnonymousUniqueId` | 03/08 | Persistent target state (an obsolete `SoftLocks` file is deleted on the fly by `TargetImpl`) |
| `/var/log/zypp/history` | 08 | Machine-readable transaction history |
| `/var/log/zypper.log` (zypper) / y2log (YaST) | 02 | Debug logs written by the logger |
| `/run/zypp.pid`, `/run/zypp-rpm.pid` | 08 | Global locks (`$ZYPP_LOCKFILE_ROOT/run/…`, see `ZYppFactory::lockfileDir()`) |
| `/usr/lib/zypp/plugins/{commit,services,system,urlresolver}` | 08/05 | Plugin directories |

(Exact defaults depend on `ZConfig` and the target root. Check chapter 09.)

## How to get productive quickly

1. Build the stack (see the parent `zypp-stack` README / `GEMINI.md`) with
   `ENABLE_DEVEL_BUILD=ON`, so libzypp uses helpers from the build dir and not
   the system ones ([01](01-layout-build-and-dependencies.md)).
2. Run `zypper` from the build tree against a test root (`--root /tmp/x`) to
   avoid touching your system.
3. Logging: zypper writes `/var/log/zypper.log`. Set `ZYPP_LOGFILE=-` /
   `ZYPP_FULLLOG=1` for more ([02](02-core-infrastructure.md),
   [09](09-configuration-and-environment.md)).
4. For solver bugs, always ask for the solver testcase produced by
   `zypper --debug-solver` ([04](04-solver.md), [11](11-testing.md)).
5. Read [13](13-pitfalls-and-special-cases.md) before reviewing your first PR.
6. If a term in any chapter is unclear (buddy, causer, PTF, RIS, droplist, …),
   look it up in the [concepts primer & glossary](00a-concepts-and-glossary.md).
7. When a bug comes in, start with the [triage guide](15-bug-triage-guide.md).
