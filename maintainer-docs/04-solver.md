# The Dependency Solver Layer

This chapter covers how libzypp turns `PoolItem` status bits into a libsolv job queue, runs libsolv, and writes the result back into the pool. It also covers how solver problems become user-visible `ResolverProblem`/`ProblemSolution` objects and how all the solver flags and modes work (verify, update, dist upgrade, upgrade repos, patches). The last parts deal with test cases, SystemCheck, PTF/retracted handling, locks and `PurgeKernels`.

All solver code lives in the **legacy tier** (`zypp/zypp/...`). A few small enum headers live in `zypp-logic/zypp/`. There is **no NG solver yet**: `zyppng/` only has `zyppng/lib/zypp/ng/resolvernamespace.cppm` and sat components such as the autoinstalled component. Per `AGENTS.md`, the legacy solver is frozen except for bug fixes. Pool/`ResStatus` internals are covered in [SAT pool & resolvables](03-sat-pool-and-resolvables.md), commit in [Target, commit & rpm](08-target-commit-and-rpm.md), and `zypp.conf` handling in [Configuration](09-configuration-and-environment.md).

---

## 1. File map

| File | Role |
|---|---|
| `zypp/zypp/Resolver.{h,cc}` | Public API `zypp::Resolver` (ref-counted, `RW_pointer` pimpl to `solver::detail::ResolverInternal`). Pure forwarding. |
| `zypp/zypp/solver/detail/Types.h` | `using ResolverInternal = solver::detail::Resolver;`, `ItemCapKindList`, `SolverQueueItemList`, `SolutionActionList`. |
| `zypp/zypp/solver/Types.h` | Pulls in `ResPool.h`, `ResolverFocus.h`, `ResolverNamespace.h`, `ResolverProblem.h` for the public API. |
| `zypp/zypp/solver/detail/Resolver.{h,cc}` | "Mid layer" that holds the mode flags (upgrade/update/verify), extra requires/conflicts, upgrade repos, weak items, `_applyDefault_*` bookkeeping and "who installed what" info. It owns the `SATResolver*` as a raw pointer. The header says: *"A mid layer class we should remove … Logic and data are horribly distributed between this and SATResolver."* |
| `zypp/zypp/solver/detail/SATResolver.{h,cc}` | The real work: libsolv solver creation, job queue construction, solving, copy-back, problem→solution translation, the `establish()` helper for patches. |
| `zypp/zypp/solver/detail/SolverQueueItem*.{h,cc}` | Request objects for `resolveQueue()` (Install/Delete/Update/InstallOneOf/Lock). |
| `zypp/zypp/solver/detail/SolutionAction.{h,cc}` | `TransactionSolutionAction` (KEEP/INSTALL/REMOVE/UNLOCK/LOCK/REMOVE_EXTRA_*/ADD/REMOVE_SOLVE_QUEUE_ITEM), `InjectSolutionAction` (WEAK). |
| `zypp/zypp/solver/detail/ProblemSolutionCombi.{h,cc}` | A solution made of several actions. Builds its description incrementally. |
| `zypp/zypp/solver/detail/ProblemSolutionIgnore.{h,cc}` | The "break %s by ignoring some of its dependencies" solution. |
| `zypp/zypp/solver/detail/SystemCheck.{h,cc}` | Reads `/etc/zypp/systemCheck` and `/etc/zypp/systemCheck.d/*.check`. |
| `zypp/zypp/solver/detail/Testcase.{h,cc}` | Writes solver test cases (libsolv `testcase.t` plus `zypp-control.yaml`). |
| `zypp/zypp/solver/detail/Testcase_helix.cc` | Old helix XML writer. **Not compiled** (absent from `zypp/zypp/CMakeLists.txt`). Dead code, kept for reference. |
| `zypp/zypp/solver/detail/ItemCapKind.h` | Struct for `isInstalledBy()`/`installs()` queries. |
| `zypp/zypp/ResolverProblem.{h,cc}`, `zypp/zypp/ProblemSolution.{h,cc}`, `zypp/zypp/ProblemTypes.h` | Public problem/solution objects. |
| `zypp-logic/zypp/ResolverFocus.{h,cc}` | `enum class ResolverFocus { Default, Job, Installed, Update }`. |
| `zypp-logic/zypp/ResolverNamespace.h` | `enum class ResolverNamespace { language, modalias, filesystem }` and its libsolv IDs. |
| `zypp/zypp/PurgeKernels.{h,cc}` | `zypper purge-kernels` logic (keep spec). |
| `zypp/zypp/misc/LoadTestcase.*`, `zypp/zypp/misc/TestcaseSetup.*` | **Reading** test cases (Helix or YAML) for tests/tools (`ZYPP_API_DEPTESTOMATIC`). |
| `zypp/systemCheck` | Installed as `/etc/zypp/systemCheck`. Contains just `requires:glibc`. |

Defines worth knowing:

- `ZYPP_USE_RESOLVER_INTERNALS` must be defined before including any `solver/detail/*.h`, or you get `#error Do not directly include this file!`. Even so, these headers **are installed** (`DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/solver/detail`) and exported with `ZYPP_API_DEPTESTOMATIC` (= `ZYPP_API`, see `zypp-logic/zypp-core/Globals.h`) for the testcase runner (`zypp-solver-testcase-runner/src/deptestomatic.cc` in the stack). Treat their ABI as semi-public.
- The log group in solver files is `zypp::solver` (`ZYPP_BASE_LOGGER_LOGGROUP`). `PurgeKernels.cc` uses `PurgeKernels`.

---

## 2. Architecture and object lifetime

```
 Application (zypper / YaST / PackageKit)
        │  getZYpp()->resolver()  or  ResPool::instance().resolver()
        ▼
 zypp::Resolver  (public, Resolver.h)           one instance per ZYppImpl
        │  _pimpl (RW_pointer)                  created in zypp_detail/ZYppImpl.cc:118
        ▼
 solver::detail::Resolver  ("ResolverInternal")
   - _upgradeMode / _updateMode / _verifying / _solveSrcPackages / _ignoreAlreadyRecommended
   - _extra_requires / _extra_conflicts   (addRequire/addConflict)
   - _upgradeRepos                        (addUpgradeRepo)
   - _addWeak                             (from "ignore" solutions)
   - _added/_removed_queue_items          (from solutions, resolveQueue only)
   - _applyDefault_<flag> bits            (does flag follow zypp.conf?)
        │  SATResolver* _satResolver (owned, raw new/delete)
        ▼
 solver::detail::SATResolver
   - most solver flags as public bitfields (_allowdowngrade, _onlyRequires, ...)
   - CSolver* _satSolver  (re-created on every solver run)
   - CQueue   _jobQueue   (re-created on every solver run)
        │
        ▼
 libsolv  (solver_create / solver_solve / solver_get_decisionqueue / problems API)
```

Key facts:

- There is **one** `zypp::Resolver` per `ZYppImpl` (`_resolver( new Resolver( ResPool::instance()) )`). `ResPool::resolver()` returns the same object.
- `SATResolver::solverInit()` calls `solverEnd()` first. So **each solver run frees the previous libsolv solver**, including its problems, decisions and rules. Call `problems()` after a failed run and **before** the next run, because the problem→solution mapping indexes into the current `_jobQueue` (`_jobQueue.elements[rp]`).
- `Resolver::get()` / `zypp::Resolver::get()` is the "expert backdoor" that returns the raw `CSolver*`. It is `NULL` before the first run. `Testcase::createTestcase` checks this (libzypp issue #317 / "Fix segv if ZYPP_FULLOG is set").
- Unused leftovers: `MAXSOLVERRUNS` (defined, unused), `_poolchanged` (initialized, never queried), `checkUnmaintainedItems()` (declared, never defined).

---

## 3. A solver run step by step (`resolvePool`)

`zypp::Resolver::resolvePool()` → `detail::Resolver::resolvePool()`:

```cpp
ScopedAutoTestCaseWriter _raiiGuard( *this );   // ZYPP_FULLLOG=1 → autoTestcase (see §9)
solverInit();                                   // push mode flags down into SATResolver
return _satResolver->resolvePool(_extra_requires, _extra_conflicts, _addWeak, _upgradeRepos);
```

`detail::Resolver::solverInit()` copies `_upgradeMode → setDistupgrade`, `_updateMode → setUpdatesystem`, `_verifying → setFixsystem`, `solveSrcPackages`, and `ignoreAlreadyRecommended` into the SATResolver. It also clears the `ItemCapKind` maps.

`SATResolver::resolvePool()`:

1. `solverInit(weakItems)` builds the job queue (detailed below).
2. `solverAddJobsFromPool()` adds the user/application transactions.
3. `solverAddJobsFromExtraQueues()` adds `addRequire`/`addConflict`.
4. For each upgrade repo: `SOLVER_DISTUPGRADE | SOLVER_SOLVABLE_REPO, repoid` ("`dup --from`").
5. `solving()` calls `sat::Pool::prepare()`, then `solver_solve()`, an optional second DUP droplist run, and the copy-back.

### 3.1 Job queue construction (`SATResolver::solverInit` and helpers)

The jobs are added in this order. The order matters because problem solutions refer to job indexes.

| # | Job(s) | Source / reason |
|---|---|---|
| 0 | `pool_set_custom_vendorcheck(vendorCheck \| relaxedVendorCheck)` | Not a job. Uses the relaxed check only in DUP when a **non-installed** solvable provides `dup-vendor-relax(suse)` (bsc#1182629). Both callbacks delegate to `VendorAttr::instance().equivalent()` / `relaxedEquivalent()`. |
| 1 | `pool_add_userinstalled_jobs(..., sat::Pool::autoInstalled(), GET_USERINSTALLED_NAMES\|GET_USERINSTALLED_INVERTED)` | Tells libsolv which installed names were **not** auto-installed (`SOLVER_USERINSTALLED`). This is the basis for cleandeps and the `unneeded` computation. |
| – | `SATCollectTransact` over the pool | Sorts PoolItems into `_items_to_install/_remove/_lock/_keep` (see §3.2). **Resets all former SOLVER/APPL_LOW transactions** on the fly. |
| 2 | `SOLVER_WEAKENDEPS \| SOLVER_SOLVABLE` per `_addWeak` item | Comes from applied "ignore some dependencies" solutions. |
| 3 | `SOLVER_BLACKLIST \| SOLVER_SOLVABLE_PROVIDES` on `retracted-patch-package()` and `ptf()` | Retracted packages and PTF master packages are not picked unless explicitly requested. `ptf-package()` is deliberately **not** blacklisted (bsc#1186503). |
| 4 | `SOLVER_INSTALL \| SOLVER_SOLVABLE_PROVIDES namespace:language(xx)` for **added** tracked locales. `SOLVER_ERASE \| SOLVER_SOLVABLE_PROVIDES \| SOLVER_CLEANDEPS` for **removed** ones | Only *changes* to the requested locales generate jobs (`myPool().trackedLocaleIds().added()/removed()`). Erase always uses CLEANDEPS ("needs uncond. SOLVER_CLEANDEPS!"). |
| 5 | `SOLVER_NOOBSOLETES \| SOLVER_SOLVABLE` for every solvable in `myPool().multiversionList()` | Multiversion (e.g. kernels: `multiversion = provides:multiversion(kernel)`). |
| 6 | `SOLVER_INSTALL \| SOLVER_SOLVABLE` for every **installed** PTF master, but only if `sat::Pool::reposSize() == 1` (only `@System` loaded) | bsc#1203248: without repos, removing a PTF would remove its packages instead of replacing them. Sets `_protectPTFs`. |
| 7 | `solverInitSetSystemRequirements()` | SystemCheck `requires:` → `SOLVER_INSTALL\|SOLVABLE_PROVIDES`. `conflicts:` → `SOLVER_ERASE\|SOLVABLE_PROVIDES\|MAYBE_CLEANDEPS`. In DUP with `systemRoot() == "/"`, also pins the arch of the installed `rpm` (`SOLVER_INSTALL\|SOLVER_SOLVABLE_NAME\|SOLVER_ESSENTIAL` on `rpm.<arch>`). |
| 8 | `solverInitSetLocks()` | See §11 (locks). |
| 9 | `solverInitSetModeJobsAndFlags()` | `SOLVER_VERIFY\|SOLVABLE_ALL` if fixsystem. `SOLVER_UPDATE\|SOLVABLE_ALL` if updatesystem. `SOLVER_DISTUPGRADE\|SOLVABLE_ALL` (+ `SOLVER_DROP_ORPHANED\|SOLVABLE_ALL` if `_removeOrphaned`) if distupgrade. If `_removeUnneeded`, a weak `SOLVER_ERASE\|SOLVABLE_NAME` for each item currently flagged `isUnneeded()`. Then all `solver_set_flag()` calls (§4). |
| 10 | `solverAddJobsFromPool()` | `SOLVER_INSTALL\|SOLVER_SOLVABLE` per to-install item. `SOLVER_ERASE\|SOLVER_SOLVABLE\|MAYBE_CLEANDEPS` per to-remove item. |
| 11 | `solverAddJobsFromExtraQueues()` | `addRequire` → `SOLVER_INSTALL\|SOLVABLE_PROVIDES`. `addConflict` → `SOLVER_ERASE\|SOLVABLE_PROVIDES\|MAYBE_CLEANDEPS`. |
| 12 | upgrade repos | `SOLVER_DISTUPGRADE\|SOLVER_SOLVABLE_REPO`. |

`MAYBE_CLEANDEPS` is `(cleandepsOnRemove() ? SOLVER_CLEANDEPS : 0)`.

### 3.2 From PoolItem status to jobs (`SATCollectTransact`)

```
for each PoolItem:
  if status.isBySolver() || status.isByApplLow():
       status.resetTransact(APPL_LOW)      # drop previous solver result, NOT a request
       continue
  if SrcPackage && !solveSrcPackages: continue
  switch status.getTransactValue():
    TRANSACT   : isUninstalled() ? _items_to_install : _items_to_remove
    LOCKED     : _items_to_lock
    KEEP_STATE : _items_to_keep          # only APPL_HIGH / USER reach here
```

Consequences:

- **Only transactions caused by `APPL_HIGH` or `USER` are requests.** Anything set by `SOLVER` or `APPL_LOW` is cleared and recomputed on every run. That is why the copy-back marks items pulled in by `addRequire`/`addConflict` as `APPL_LOW`. They must not turn into requests for the next run (comment in `solving()`).
- `KEEP_STATE` with causer `APPL_HIGH`/`USER` is a **soft lock** ("taboo"). See §11.
- `resolveQueue()` is different: the jobs come from the `SolverQueueItem` list, **then** `solverAddJobsFromPool()` is added as well ("they do contain any problem resolutions"). Extra requires/conflicts and upgrade repos are **not** used by `resolveQueue()`.

### 3.3 Solving and copy-back (`SATResolver::solving`)

1. `sat::Pool::instance().prepare()` (whatprovides etc., see chapter 03).
2. `solver_solve()`.
3. **DUP droplist second pass** (bsc#1155819). This only runs if `_distupgrade`, the first solve had no problems, `!_removeOrphaned`, and `ZConfig::solverUpgradeRemoveDroppedPackages()` (default true). For every to-be-installed solvable that provides `product()` (the product *buddy* release package), it reads `valuesOfNamespace("weakremover")`, pushes `SOLVER_DROP_ORPHANED|SOLVER_SOLVABLE_NAME` per entry, **pins** the product with `SOLVER_INSTALL|SOLVER_SOLVABLE`, and solves again. Products of the *future* distribution carry the drop list, so it only becomes known after the first pass.
4. Copy-back:
   - Each positive decision that is not a system solvable → `SATSolutionToPool(item, toBeInstalled, SOLVER)` and `_result_items_to_install`.
   - Each system solvable with `solver_get_decisionlevel() <= 0` (i.e. not kept) is removed. `CheckIfUpdate` looks for a to-be-installed item with the same ident (non-multiversion, or same NVRA for multiversion) → `toBeUninstalledDueToUpgrade`. Otherwise `toBeUninstalled`, followed by a lazy `sat::WhatObsoletes` pass that marks `setToBeUninstalledDueToObsolete()`.
   - `solverCopyBackWeak()`: sets `RECOMMENDED`/`SUGGESTED` bits (`solver_get_recommendations`), `ORPHANED` (`solver_get_orphaned`, also cached in `_problem_items` and returned by `problematicUpdateItems()`), and `UNNEEDED` (`solver_get_unneeded(..., 1)`).
   - `solverCopyBackValidate()`: patch status via `solver_trivial_installable` (see §8).
   - Items providing an extra require (to-install) or conflict (to-remove) get `setTransactByValue(APPL_LOW)`.
5. The return value is `solver_problem_count() == 0`.

`SATSolutionToPool` always calls `resetTransact(causer)` and `resetWeak()` first. The weak bits are recomputed on every run.

### 3.4 `getTransaction()` and autoinstalled tracking

`detail::Resolver::getTransaction()` builds `sat::Transaction(loadFromPool)` and then injects `_satResolver->autoInstalled()` (`solver_get_userinstalled(..., NAMES|INVERTED)`). The code has a `// FIXME: That's an ugly way of pushing autoInstalled into the transaction.`

The round trip:

```
/var/lib/zypp/AutoInstalled  ──(TargetImpl load, TargetImpl.cc ~1355)──► sat::Pool::setAutoInstalled
        ▲                                                                  │
        │                                                     pool_add_userinstalled_jobs (job #1)
        │                                                                  ▼
 TargetImpl::commit: _autoInstalledFile.setData(                      libsolv decides
   result.rTransaction().autoInstalled() )  ◄── Resolver::getTransaction() ◄── solver_get_userinstalled
 (not on dryRun; TargetImpl.cc ~1637)
```

`TargetImpl::updateAutoInstalled()` writes the current `sat::Pool::autoInstalled()` back out if an application changed it directly. Only autoinstalled packages can become `unneeded` (see the `setRemoveUnneeded` doc in `Resolver.h`).

---

## 4. Solver flags

Flags live in two places:

- **In `detail::Resolver`** as bitfields: `_upgradeMode`, `_updateMode`, `_verifying`, `_solveSrcPackages`, `_ignoreAlreadyRecommended`.
- **In `SATResolver`** as public bitfields, set by the `ZOLV_FLAG_SATSOLV` macro in `solver/detail/Resolver.cc`.

### 4.1 Tri-state default mechanism

Most flags are set through a `TriBool`:

```cpp
void Resolver::setXxx( TriBool state_r ) {
  _applyDefault_xxx = indeterminate(state_r);          // follows zypp.conf?
  bool newval = _applyDefault_xxx ? ZVARDEFAULT : bool(state_r);
  ...
}
```

- The public `setDefaultXxx()` / `resetOnlyRequires()` pass `indeterminate`.
- `setDefaultSolverFlags(all_r=true)` resets everything to the defaults. `setDefaultSolverFlags(false)` only refreshes the flags that still follow the defaults. `ZYppImpl::changeTargetTo()` calls it with `false` after `ZConfig::notifyTargetChanged()`, so `--root` installs pick up the **target's** `zypp.conf` solver settings (bsc#1196368). `ZConfig` keeps `_initialTargetDefaults` (from `/`) and an optional `_currentTargetDefaults` (from the target root). See chapter 09.
- **Pitfall:** the `ZVARDEFAULT` values in `Resolver.cc` and the initializers in the `SATResolver` ctor must stay in sync. Both files have a comment saying so.

### 4.2 Flag table

| Public API | SATResolver var → libsolv flag | Default | zypp.conf key |
|---|---|---|---|
| `setFocus(ResolverFocus)` | `_focus` → `SOLVER_FLAG_FOCUS_INSTALLED` / `SOLVER_FLAG_FOCUS_BEST` | `ZConfig::solver_focus()` (Default ⇒ Job) | `solver.focus` = Job / Installed / Update |
| `setForceResolve` | `_allowuninstall` → `SOLVER_FLAG_ALLOW_UNINSTALL` | `false` (hard-coded) | – |
| `setOnlyRequires` | `_onlyRequires` → `SOLVER_FLAG_ONLY_NAMESPACE_RECOMMENDED` (and `SOLVER_FLAG_IGNORE_RECOMMENDED` is always 0) | `ZConfig` | `solver.onlyRequires` (false) |
| `setIgnoreAlreadyRecommended` | `_ignorealreadyrecommended` → `SOLVER_FLAG_ADD_ALREADY_RECOMMENDED = !value` | `true` (Resolver ctor, not configurable) | – |
| `setCleandepsOnRemove` | `_cleandepsOnRemove` → `SOLVER_CLEANDEPS` on erase jobs | `ZConfig` | `solver.cleandepsOnRemove` (false) |
| `setNoUpdateProvide` | `_noUpdateProvide` → `SOLVER_FLAG_NO_UPDATEPROVIDE` | `ZConfig` | `solver.noUpdateProvide` (false). Documented in `zypp/doc/zypp.conf.5.txt`, but not in the `zypp.conf.LEGACY` template. |
| `setAllowDowngrade` | `_allowdowngrade` → `SOLVER_FLAG_ALLOW_DOWNGRADE` | `false` | – |
| `setAllowNameChange` | `_allownamechange` → `SOLVER_FLAG_ALLOW_NAMECHANGE` | **`true`** (bsc#1071466) | – |
| `setAllowArchChange` | `_allowarchchange` → `SOLVER_FLAG_ALLOW_ARCHCHANGE` | `false` | – |
| `setAllowVendorChange` | `_allowvendorchange` → `SOLVER_FLAG_ALLOW_VENDORCHANGE` | `ZConfig` | `solver.allowVendorChange` (false) |
| `dupSetAllowDowngrade` | `_dup_allowdowngrade` → `SOLVER_FLAG_DUP_ALLOW_DOWNGRADE` | `ZConfig` | `solver.dupAllowDowngrade` (true) |
| `dupSetAllowNameChange` | `_dup_allownamechange` → `SOLVER_FLAG_DUP_ALLOW_NAMECHANGE` | `ZConfig` | `solver.dupAllowNameChange` (true) |
| `dupSetAllowArchChange` | `_dup_allowarchchange` → `SOLVER_FLAG_DUP_ALLOW_ARCHCHANGE` | `ZConfig` | `solver.dupAllowArchChange` (true) |
| `dupSetAllowVendorChange` | `_dup_allowvendorchange` → `SOLVER_FLAG_DUP_ALLOW_VENDORCHANGE` | `ZConfig` | `solver.dupAllowVendorChange` (**false**. It was true in Code12 / libzypp-16.x.) |
| `setRemoveOrphaned` | `_removeOrphaned` → `SOLVER_DROP_ORPHANED` job (DUP only) | `false` | – |
| `setRemoveUnneeded` | `_removeUnneeded` → weak erase jobs | `false` | – |
| `setSolveSrcPackages` | `_solveSrcPackages` | `false` | – |
| `setSystemVerification` | `_verifying` → `_fixsystem` → `SOLVER_VERIFY` | `false` | – |
| `setUpgradeMode` / `setUpdateMode` | `_distupgrade` / `_updatesystem` | `false` | – |
| (internal) | `_dosplitprovides` → `SOLVER_FLAG_SPLITPROVIDES` | `true` | – |

The `ZConfig` defaults are in `ZConfig::Impl::TargetDefaults` (`zypp/zypp/ZConfig.cc`). `solver.upgradeTestcasesToKeep` (2) and `solver.upgradeRemoveDroppedPackages` (true) are also target defaults.

Focus mapping (`solverSetFocus` in `SATResolver.cc`):

| ResolverFocus | FOCUS_INSTALLED | FOCUS_BEST |
|---|---|---|
| Default, Job | 0 | 0 |
| Installed | 1 | 0 |
| Update | 0 | 1 |

`Resolver::setFocus(Default)` sets `_applyDefault_focus` and stores `ZConfig::solver_focus()`. So `focus()` never reports `Default` after it has been set this way, unless zypp.conf itself is unset/Default.

### 4.3 Semantics that surprise people

- **onlyRequires still honours namespace recommends.** The public doc in `Resolver.h` says language and modalias packages are not regarded, but the code sets `ONLY_NAMESPACE_RECOMMENDED` and comments `// resolve recommended namespaces`. So `recommends: namespace:language(de)` / modalias recommends are still followed. `zypp/tests/zypp/Resolver_test.cc` (`TCNamespaceRecommends`) tests this together with `ignoreAlreadyRecommended`.
- `ignoreAlreadyRecommended == false` is "install new recommends" mode (`zypper inr`). Recommends of already-installed packages are re-evaluated.
- `forceResolve` (allow uninstall) is what `PurgeKernels::markObsoleteKernels()` turns on, and **never turns off again** (§13).
- `allowVendorChange` affects candidate selection outside the solver too. See the changelog entry "Don't cache defaultCandidate as it depends on solver::allowVendorChange flag (bnc#754286)" and `zypp/doc/autoinclude/SolverVendorChange.doc` (vendor protection rule, `/etc/zypp/vendors.d`).

---

## 5. Modes

### 5.1 `verifySystem()`

```cpp
_verifying = true;
UndoTransact resetting(ResStatus::APPL_HIGH);   // resets ALL transactions up to APPL_HIGH!
invokeOnEach(pool, ByTransact, resetting);
return resolvePool();
```

- This drops all `SOLVER`, `APPL_LOW` and `APPL_HIGH` transactions. Only `USER` requests survive.
- `_verifying` is **sticky**. It stays on for later `resolvePool()` calls until `setSystemVerification(false)`, `setDefaultSystemVerification()` or `reset()`.
- Adds `SOLVER_VERIFY|SOLVER_SOLVABLE_ALL` (libsolv checks and repairs all installed dependencies). zypper's `verify` command uses this.

### 5.2 `doUpdate()` ("update to newest, quietly")

- Sets `_updateMode = true` (**sticky**), runs `solverInit()`, then `SATResolver::doUpdate()`.
- `SATResolver::doUpdate()` is **a separate copy of the solve/copy-back code**. Its own comment says: *"doUpdate has no additional jobs. It does not include any pool jobs, and so it does not create any conflicts."* User selections, extra requires and upgrade repos are **ignored**. It returns `void`, does no problem reporting, no droplist pass, no `WhatObsoletes` pass, and no APPL_LOW marking. It still copies back weak and validate status.
- To combine user requests with "update all", use `setUpdateMode(true)` + `resolvePool()` (this adds `SOLVER_UPDATE|SOLVABLE_ALL` to the normal jobs). zypper's `patch --with-update` works this way (`runtimeData().solve_with_update` → `set_solver_flags()` → `setUpdateMode`). Plain `zypper update` **without arguments** does *not*: it sets `solve_update_only` (`zypper/src/commands/update.cc`) and `solve_and_commit()` calls `doUpdate()` (since bsc#1201972 after `set_solver_flags()`). So `zypper up` never shows a problem dialogue; packages that can't be updated are silently kept. `zypper up <pkg>` uses normal requests and `resolvePool()`. A `TODO: Try to join the following with solving()` remains in the code.

### 5.3 `doUpgrade()` (dist upgrade)

- Sets `_upgradeMode = true` (**sticky**; `setUpgradeMode(false)` to leave, bnc#709480) and calls `resolvePool()`.
- Adds `SOLVER_DISTUPGRADE|SOLVABLE_ALL`, the optional `SOLVER_DROP_ORPHANED`, the rpm arch pin (target root `/` only), the droplist second pass, and the relaxed-vendor option.
- The dup flag defaults differ from the normal ones (see table: downgrade/namechange/archchange allowed, vendor change not).
- `_removeOrphaned` is only honoured in DUP: *"By now libsolv supports orphan handling just in dup … in case libsolv changes and it's used in remove commands which have no repos enabled. I.e. everything would be orphaned."* `Resolver.h` warns that 3rd-party rpms without a repo count as orphaned. Putting them into a plaindir repo protects them.
- `problematicUpdateItems()` returns the orphaned items cached during copy-back.
- A commented-out block in `detail::Resolver::solverInit()` considers tying `setRemoveOrphaned` to `solverUpgradeRemoveDroppedPackages`. It was rejected as a behaviour change for old distros.

### 5.4 Upgrade repos (`zypper dup --from`)

`addUpgradeRepo(repo)` ignores null and system repos. Use it with plain `resolvePool()`, **not** `doUpgrade()` (header note). It generates `SOLVER_DISTUPGRADE|SOLVER_SOLVABLE_REPO`. `upgradingRepos()` is also checked at commit time to write the update test case (§9.3, bsc#1004096).

### 5.5 `resolveQueue()`

This is the queue-based API (mainly for UIs). Before solving, solution-generated `_removed_queue_items`/`_added_queue_items` are merged into the caller's queue via `SolverQueueItem::cmp()`, and both lists are cleared. *The application has to write those changes back to its own model.* Job translation (`SolverQueueItem*::addRule`):

| Item | Job |
|---|---|
| `SolverQueueItemInstall(name, soft)` | `SOLVER_INSTALL\|SOLVER_SOLVABLE_NAME [\|SOLVER_WEAK]` |
| `SolverQueueItemDelete(name, soft)` | `SOLVER_ERASE\|SOLVER_SOLVABLE_NAME [\|WEAK] [\|CLEANDEPS]`. `MAYBE_CLEANDEPS` here reads `pool().resolver().cleandepsOnRemove()`. |
| `SolverQueueItemUpdate(item)` | `SOLVER_UPDATE\|SOLVER_SOLVABLE` |
| `SolverQueueItemInstallOneOf(list)` | `SOLVER_INSTALL\|SOLVER_SOLVABLE_ONE_OF [\|WEAK]`, `pool_queuetowhatprovides(list)` |
| `SolverQueueItemLock(item, soft)` | installed: `SOLVER_INSTALL\|SOLVABLE [\|WEAK]`. Otherwise `SOLVER_ERASE\|SOLVABLE [\|WEAK]` |

### 5.6 `undo()` / `reset()`

- `undo()` resets transactions up to `APPL_LOW` (i.e. solver results and APPL_LOW marks). It also clears `_addWeak` and the solution queue items. **It does not clear extra requires/conflicts or modes.**
- `zypp::Resolver::reset()` calls `reset(false)`: `_verifying = false`, clears extra requires/conflicts and the `ItemCapKind` maps. It does **not** reset `_upgradeMode`/`_updateMode`, the upgrade repos or `_addWeak`.

---

## 6. Problems and solutions

### 6.1 Building `ResolverProblem`s (`SATResolver::problems()`)

For each libsolv problem:

1. `SATgetCompleteProblemInfoStrings()`:
   - `solver_findproblemrule()` gives the most relevant rule. Its text becomes `description()`, and its `detail`/`ignoreId` are kept.
   - `solver_findallproblemrules()` gives all rules. Generic `SOLVER_RULE_UPDATE`/`SOLVER_RULE_JOB` entries are filtered out if more specific ones exist, and duplicates are removed. The result is stored in `ResolverProblem::completeProblemInfo()`. zypper shows it as "detail", and since bsc#1277790 it is also logged.
2. `SATproblemRuleInfoString()` translates rule types to **translated** strings. These are duplicated from libsolv for gettext (`// @FIXME`). The texts distinguish "the installed X" from "the to be installed X". `mapSolvable()` maps a package to its **buddy**, so the user sees "product foo" rather than "release-package foo". Unknown rule types fall back to `solver_problemruleinfo2str()` (untranslated). `SOLVER_RULE_PKG_REQUIRES` also lists "deleted providers" and "not installable providers" in `detail`. `PKG_NOTHING_PROVIDES_DEP` and `PKG_REQUIRES` set `ignoreId = source`.
3. Each libsolv solution becomes one `ProblemSolutionCombi`. Solution elements map as follows:

| libsolv element | Condition | Action(s) | Description |
|---|---|---|---|
| JOB `INSTALL\|SOLVABLE` | item is installed | `REMOVE` | "remove lock to allow removal of %1%" (+ PTF hint if `_protectPTFs`) |
| JOB `INSTALL\|SOLVABLE` | not installed | `KEEP` | "do not install %1%" (+ patch→PTF hint) |
| JOB `ERASE\|SOLVABLE` | installed | `KEEP` | "keep %1%" |
| JOB `ERASE\|SOLVABLE` | not installed | `UNLOCK` | "remove lock to allow installation of %1%" |
| JOB `INSTALL\|SOLVABLE_NAME` | – | `REMOVE_SOLVE_QUEUE_ITEM(Install)` | "do not install %1%" |
| JOB `ERASE\|SOLVABLE_NAME` | – | `KEEP` on installed+transacting items **and** `REMOVE_SOLVE_QUEUE_ITEM(Delete)` | "keep %1%" (it cannot tell whether the job came from resolvePool or resolveQueue, so it handles both) |
| JOB `INSTALL\|SOLVABLE_PROVIDES` | – | `REMOVE_EXTRA_REQUIRE` | "do not ask to install a solvable providing %1%". If the cap is a **SystemCheck** require, the problem description becomes "This request will break your system!" and the solution "ignore the warning of a broken system (requires:…)" is put in front. |
| JOB `ERASE\|SOLVABLE_PROVIDES` | – | `REMOVE_EXTRA_CONFLICT` | analogous ("conflicts:") |
| JOB `UPDATE\|SOLVABLE` | installed | `KEEP` | "do not install most recent version of %1%" |
| `SOLVER_SOLUTION_INFARCH` | installed / not | `LOCK` / `INSTALL` | "keep/install %1% despite the inferior architecture" |
| `SOLVER_SOLUTION_DISTUPGRADE` | installed / not | `LOCK` / `INSTALL` | "keep obsolete %1%" / "install %1% from excluded repository" |
| `SOLVER_SOLUTION_BLACK` | – | `INSTALL` | "install %1% although it has been retracted" / "allow installing the PTF %1%" / "… blacklisted" |
| `p > 0, rp` | replacement | `INSTALL(itemTo)` | "downgrade of", "architecture change of", "install %1% (with vendor change)…", or "replacement of" (decided via `policy_is_illegal`) |
| `p > 0, rp == 0` | – | `REMOVE(itemFrom)` | "deinstallation of %1%" (+ PTF hint) |

4. Solutions with **more than one action are inserted in front** (`addSolution(..., actionCount() > 1)`).
5. If `ignoreId > 0`, a `ProblemSolutionIgnore` ("break %s by ignoring some of its dependencies") is **appended** last. It holds an `InjectSolutionAction(WEAK)` → `Resolver::addWeak()` → `SOLVER_WEAKENDEPS` job on the next run.
6. **PTF hint** (bsc#1194848, `PtfPatchHint`): if a solution removes a PTF, the problem description is prefixed with an explanation. There are three variants: PTF vs patch ("not yet fully integrated into …; typically keep the PTF and skip the patches"), the bsc#1203248 "use `zypper removeptf`" text, or a generic "The installed PTF blocks the desired action".

`ProblemSolution::pushDescriptionDetail()` behaviour: the first description goes into `description()`. On the second one, the first moves into `details()` and `description()` becomes "Following actions will be done:". `setDetails()` **appends** with a leading newline instead of replacing.

`ProblemSolution::skipsPatchesOnly()` / `getIfSkipsPatchesOnly()` returns true if *all* actions are `KEEP` on a patch. zypper uses this to offer "skip these patches". `getIfSkipsPatchesOnly` is guarded by `#ifdef __cpp_lib_optional` because YaST/PK compiled with C++11 until 15-SP3.

### 6.2 Applying solutions

`zypp::Resolver::applySolutions(list)` → `detail::Resolver::applySolutions` runs each solution's actions with `SolutionAction::execute(resolver)`. It **stops at the first failing solution**. Actions (`SolutionAction.cc`):

| Action | Effect on ResStatus |
|---|---|
| `KEEP` | `resetTransact(USER)` and then `setTransact(false, APPL_HIGH)`. This is a soft lock at APPL_HIGH, so it is not persisted. |
| `INSTALL` | if to-be-uninstalled: `setTransact(false, USER)`; else `setToBeInstalled(USER)` |
| `REMOVE` | if to-be-installed: un-transact **and `setLock(true, USER)`** ("no other dependency can set it again"). If installed: `setToBeUninstalled(USER)`. Otherwise: `setLock(true, USER)`. |
| `UNLOCK` | `setLock(false, USER)` |
| `LOCK` | `resetTransact(USER)` then `setLock(true, APPL_HIGH)` (not persisted) |
| `REMOVE_EXTRA_REQUIRE/CONFLICT` | `resolver.removeExtraRequire/Conflict(cap)` |
| `ADD/REMOVE_SOLVE_QUEUE_ITEM` | `resolver.addQueueItem/removeQueueItem` |
| `InjectSolutionAction WEAK` | `resolver.addWeak(item)` |

**Lock persistence:** `PoolImpl::getHardLockQueries()` (called at commit if `apply_locks_file`) diffs `isUserLocked()` (= LOCKED && by USER) against the remembered `UserLockQueryMatch` bit and adds or removes **name-based** queries in `/etc/zypp/locks`. So `UNLOCK` and the USER-level locks set by `REMOVE` *can end up persisted* by ident. `KEEP`/`LOCK` use `APPL_HIGH` on purpose so they don't. Keep this in mind when users report "phantom locks" after resolving conflicts.

`SATResolver::applySolutions()` exists but constructs a **temporary** `Resolver(_pool)` (with its own new SATResolver), so resolver-side actions (extra requires, weak, queue items) would be lost. Nothing in the public API reaches it. Do not use it.

---

## 7. "Who installed what" (`ItemCapKind`)

`isInstalledBy()`, `installs()`, `satifiedByInstalled()` (sic) and `installedSatisfied()` lazily call `collectResolverInfo()`. That function walks the REQUIRES (and, unless onlyRequires, RECOMMENDS and SUPPLEMENTS) of all `_result_items_to_install` and fills four multimaps. It is only valid after a solver run, the maps are reset by `solverInit()`/`reset()`, and it is O(n·deps) heuristics, not libsolv's `solver_describe_decision`. YaST uses it for "why is this selected".

---

## 8. Patches, patterns and the satisfied/broken/needed status

### 8.1 Pseudo-installed items

`traits::isPseudoInstalled(kind)` (`zypp-logic/zypp/ResTraits.h`) is **true only for `ResKind::patch`**. Patterns stopped being pseudo-installed in libzypp 14.7.0 (changelog: *"Patterns are no longer pseudo installed (Fate#309385)"*). Patterns are now real via their `pattern()`/`autopattern()` packages (`Pattern::autoPackage()`, `zypp/zypp/Pattern.cc`). Products share status with their reference package through the **buddy** mechanism (`PoolItem::setBuddy`, set in `pool/PoolImpl.h` when products are added).

A patch is "installed" when the solver considers its requirements satisfied. The `ResStatus::ValidateValue` field holds:

| ValidateValue | libsolv `solver_trivial_installable` result | Meaning |
|---|---|---|
| `UNDETERMINED` | other / not computed | packages are never validated |
| `BROKEN` | 0 | relevant and not satisfied, so it needs to be applied |
| `SATISFIED` | 1 | relevant and applied (or selected) |
| `NONRELEVANT` | -1 | none of its packages are installed |

The values are computed in two places:

1. **Initial "establish"**: `solver::detail::establish()` in `SATResolver.cc`. It runs an *empty* solver (only multiversion NOOBSOLETES jobs) and calls `solver_trivial_installable` on all patches. It is invoked from `ResPool::EstablishedStates::Impl` when the pool content changes (`pool/PoolImpl.h`, "Compute the initial status of Patches etc."). `EstablishedStates::changedPseudoInstalled()` compares current and initial values. Commit uses it to log patch status changes to the history (jsc#SLE-5116, chapter 08). The empty run logs `INT << "How can establish fail?"` if it ever fails.
2. **After each solver run**: `solverCopyBackValidate()` in `solving()` and `doUpdate()`.

### 8.2 Derived predicates

`zypp/zypp/PoolItem.cc`:

```cpp
isRelevant()  = !status().isNonRelevant();
isSatisfied() = status().isSatisfied();
isBroken()    = status().isBroken();
isNeeded()    = status().isToBeInstalled() || ( isBroken() && !status().isLocked() );
isUnwanted()  = isBroken() && status().isLocked();         // bnc#577118
```

`ui::Selectable` (`zypp/zypp/ui/Selectable.cc`) uses the candidate: `isNeeded() = fate()==TO_INSTALL || (!locked() && isBroken())`, `isUnwanted() = locked() && isBroken()`. Selected patches show as *satisfied* after solving, which is why `isNeeded` includes `isToBeInstalled`.

### 8.3 Patch updating: where the policy lives

libzypp only provides the data: `Patch::category()/categoryEnum()/isCategory()`, `severity()/severityFlag()/isSeverity()` (bnc#733723), `rebootSuggested()`, `restartSuggested()` (= affects the package manager / "update stack"), `reloginSuggested()`, `interactiveFlags()`, `interactiveWhenIgnoring()` (`zypp/zypp/Patch.{h,cc}`). There is no "update stack first" logic in libzypp. That lives in **zypper**:

- `zypper/src/SolverRequester.cc::updatePatches()` does two passes. The first only selects *broken* patches with `restartSuggested()`. If any were selected, it stops (and drops `--with-update`). With `--updatestack-only` it always stops after pass one.
- `installPatch()` checks `isBroken()` (bnc#506860), `isUnwanted()` (locked), optional category, interactive flags (bnc#221476), and CLI category/severity/date filters (bnc#919709).

A behaviour change in "needed" patches is therefore usually either a libsolv `solver_trivial_installable` change or a metadata change, rarely libzypp.

### 8.4 Retracted and PTF packages

Tokens (`zypp/zypp/sat/Solvable.cc`): `retractedToken = "retracted-patch-package()"`, `ptfMasterToken = "ptf()"`, `ptfPackageToken = "ptf-package()"`. The solver blacklists providers of the first two (job #3). Blacklisted solvables are not picked automatically but can be requested explicitly. Conflicts produce `SOLVER_SOLUTION_BLACK`, which becomes an `INSTALL` action. Retracted status comes from updateinfo (jsc#SLE-8770). `sat::detail::PoolImpl` sets `_retractedSpec.addIdenticalInstalledToo(true)`, so an installed package identical to a retracted one also counts as retracted. PTF removal protection and hints: job #6 and §6.1.

---

## 9. Solver test cases

### 9.1 Writer (`solver/detail/Testcase.cc`)

`Testcase(dir).createTestcase(resolver, dumpPool=true, runSolver=true)` does the following:

1. Returns false if `resolver.get()` is NULL (no solver run yet; issue #317).
2. Creates the dir. If it exists and `dumpPool`, it is **cleaned** (`clean_dir`).
3. If `runSolver`: temporarily redirects the log to `<dir>/y2log` with excessive logging (`LogControl::TmpLineWriter`, `TmpExcessive`) and calls `resolver.resolvePool()`. Note that this runs **resolvePool**, even if the app used `doUpdate()`.
4. `::testcase_write(solver, dir, TESTCASE_RESULT_TRANSACTION|TESTCASE_RESULT_PROBLEMS, "testcase.t", "solver.result")`. Repo files are named via `testcase_mangle_repo_names` (`<name>.repo.gz`).
5. Writes `zypp-control.yaml` (version 1.0) with:
   - `setup`: channels (alias, urls from `repoOrigins()`, path, type, generated/outdated timestamps, priority, file), arch, locales (with `fate: added/removed`), vendor lists from `VendorAttr`, `autoinst`, `modalias`, `multiversion` (lists with more than 10 entries go into separate `zypp-<name>.yaml`), and `resolverFlags` (focus, ignorealreadyrecommended, onlyRequires, forceResolve, cleandepsOnRemove, noUpdateProvide, allow*, dupAllow*). Missing flags were added in bsc#1041889.
   - `trials`: `install`/`keep`/`uninstall` jobs for all non-solver transactions, locks in `zypp-locks.yaml`, `addRequire`/`addConflict` (**including the SystemCheck caps**), `upgradeRepo`, `distupgrade`, `update`, `verify`.
6. Not written: `removeOrphaned`, `removeUnneeded` and `solveSrcPackages` modes, and weak items. (`forceResolve`/allowuninstall *is* recorded.) Keep this in mind when a test case does not reproduce.

The reader side is `zypp/zypp/misc/LoadTestcase.{h,cc}` (types `Helix` and `Yaml`) plus `TestcaseSetup`. It is used by `tests/lib/TestSetup.h` (`loadTestcaseRepos`, `LoadSystemAt`) and by `zypp-solver-testcase-runner` (deptestomatic). `misc/YamlTestcaseHelpers.h` parses the YAML. The plain libsolv `testcase.t` in the dir can also be run with libsolv's `testsolv`.

### 9.2 Locations

| Trigger | Directory |
|---|---|
| `zypp::Resolver::createSolverTestcase()` default | `/var/log/YaST2/solverTestcase` |
| `zypper … --debug-solver` (`zypper/src/solve-commit.cc::make_solver_test_case`) | `/var/log/zypper.solverTestCase` |
| `ZYPP_FULLLOG=1` (`ScopedAutoTestCaseWriter` in `detail/Resolver.cc`) | `/var/log/YaST2/autoTestcase`, written **after each** `resolvePool`/`doUpdate`/`resolveQueue`. The pool is dumped only on the first run (`static dumpPool`); later runs only update the control files with `runSolver=false`. bsc#1244042 fixed the missing first test case. Exceptions are swallowed. |
| Commit of a DUP or `dup --from` (`TargetImpl::commit` → `writeUpgradeTestcase()`, not on dry-run) | `<root>/var/log/updateTestcase-%Y-%m-%d-%H-%M-%S`. Keeps `solver.upgradeTestcasesToKeep` (default 2; 0 = none; `-1` parses into a huge unsigned = keep all). Written with `runSolver=false`. |

---

## 10. SystemCheck

`zypp/zypp/solver/detail/SystemCheck.cc`. This is a singleton (`SystemCheck::instance()`) whose state lives in **namespace-scope globals** (`zypp::_file`, `_dir`, `_require`, `_conflict`), not in members.

- On first use it loads `ZConfig::solver_checkSystemFile()` (default `{configdir}/systemCheck`, i.e. `/etc/zypp/systemCheck`) and then every `*.check` file in `solver_checkSystemFileDir()` (`/etc/zypp/systemCheck.d`). Other files are skipped with a log line. Paths get `Target::assertRootPrefix` if a target exists **at first use**.
- Format: one `requires:<cap>` or `conflicts:<cap>` per line, `#` comments. Anything else logs `ERR`.
- The caps become solver jobs (job #7) and are listed as extra `addRequire`/`addConflict` in test cases. Their problem solutions read "This request will break your system!" / "ignore the warning of a broken system".
- The shipped file only contains `requires:glibc`.
- Because of the static singleton, **changing the target root after the first use does not reload** SystemCheck (`setFile`/`setDir` exist but nothing in libzypp calls them on target change). Bug reports about `--root` + systemCheck may come from this.

---

## 11. Locks and the solver

How locks reach `ResStatus` is covered in chapter 03 (`/etc/zypp/locks`, `Locks`, `PoolImpl::setHardLockQueries/reapplyHardLocks`, bsc#1225267 buddy handling). Solver side:

| ResStatus | Solver job (`solverInitSetLocks`) |
|---|---|
| `LOCKED`, installed | `SOLVER_INSTALL\|SOLVER_SOLVABLE` (keep exactly this one) |
| `LOCKED`, not installed | `SOLVER_ERASE\|SOLVER_SOLVABLE [\|CLEANDEPS]` (never install this one) |
| `KEEP_STATE` by APPL_HIGH/USER (soft lock), and **no installed object with that name** | `SOLVER_ERASE\|SOLVER_SOLVABLE_NAME\|SOLVER_WEAK [\|CLEANDEPS]`, once per ident ("Keep NOT installed name") |

- Soft locks are applied **by name** (bnc#749418, bnc#765164). If any version is installed, the soft lock is ignored. Soft locks are not stored on disk (bnc#863275).
- Locks are **hard jobs**. A conflicting request produces a problem whose solution text is "remove lock to allow removal/installation of …". These solutions *really* change the USER lock status, and that can be persisted (see §6.2).
- `ResStatus::setLock` only accepts `USER`/`APPL_HIGH` causers. A transaction by a lower causer cannot override a lock (chapter 03).
- `PurgeKernels` refuses to touch user-locked kernels (`pi.status().isLocked()`).

---

## 12. Namespaces (`ResolverNamespace`)

`language`, `modalias`, `filesystem` map to libsolv's `NAMESPACE_LANGUAGE/MODALIAS/FILESYSTEM`. libsolv asks `sat::detail::PoolImpl::nsCallback` (`zypp/zypp/sat/detail/PoolImpl.cc`):

- language: is the locale in `trackedLocaleIds()` (requested locales, chapter 03)?
- modalias: `target::Modalias::instance().query(hexdecode(value))`. rpm can't hold `,` or space, so values are hex-encoded.
- filesystem: is the name in `requiredFilesystems()`?
- Anything else logs `WAR "Unhandled …"` and is unsupported.

Changing the requested locales creates explicit install/erase jobs (§3.1 job #4). The test case writer records modaliases and locales so runs can be reproduced on another machine.

---

## 13. PurgeKernels (`zypp/zypp/PurgeKernels.{h,cc}`)

This implements `zypper purge-kernels`. It replaced the old perl `purge-kernels` script (bsc#1155198) and was reworked in bsc#1173106. Usage: `PurgeKernels pk; pk.markObsoleteKernels();`. It only **marks** items. The caller still has to commit.

**Keep spec**: `ZConfig::multiversionKernels()` = `multiversion.kernels` in zypp.conf. The vendor `zypp/zypp.conf` ships `latest,latest-1,running`. The default when unset is empty, which means "Keep spec is empty, removing nothing". The comment in `zypp.conf.LEGACY` ("not evaluated by libzypp, but by the purge-kernels service") is **outdated**. Tokens (parsed in `Impl::parseKeepSpec`):

- `running`: the running kernel. Detected via `uname()`. `i?86` is normalized to `i586`. Flavour = text after the last `-` of `uname -r`. Edition variants handle `-rcN` as both `~rc` and `.rc` (bsc#1239718). If `running` is requested but `uname` failed, nothing is removed.
- `latest`, `latest-N`: only negative offsets are allowed.
- `oldest`, `oldest+N`: only positive offsets are allowed.
- An explicit version: anything after a second `-` is cut off (zypper#418). It is matched against the package's name-provides editions (bsc#1176740, bsc#1176192).
- If the spec has no words, it warns and keeps the defaults (`latest` and running).

**Algorithm** (`markObsoleteKernels`):

1. `pool.resolver().setForceResolve(true)`. This is **global and never reset**. Callers in a long-lived process (YaST/PK) inherit allow-uninstall.
2. `PoolQuery` for installed packages providing `multiversion(kernel)`, grouped as follows:
   - **Kernels** (provides `kernel-uname-r`): grouped by flavour from `^kernel-(.*)$`, cut at the first `-` ("No dashes in flavor names"), so `kernel-default-base` joins `default`.
   - **Sources**: every `noarch` package. Flavour is ignored when matching running.
   - **RelatedBinaries**: `kernel-syms(-.*)?` and `kernel(-.*)?-devel`, with the flavour derived from the name (`kernel-syms` = `default`).
   - Anything else is ignored.
   Within a group: Arch → Edition → solvables. The edition key is the "shortest" name-provides edition (fewest dots in release) to get around uname/rpm version mismatches and rebuild counters (bsc#1176740). `versionMatch` also strips a build counter, optionally with `.g<sha7>` (bsc#1175342).
3. `fillKeepList`: marks running, latest-N, oldest+N and explicit versions as keep. If the running kernel of the matching flavour/arch is **not installed**, the whole flavour/arch is kept.
4. For each package left to remove, `removePackageAndCheck`:
   - Resolve the pool first. If that fails, skip.
   - Skip locked packages.
   - `setToBeUninstalled(USER)` and resolve.
   - Every *additional* solver removal must be in the remove list, or a kmod (provides `kmod(*)`/`ksym(*)`, bsc#1185325), or match `kernel-syms…|kgraft-patch…|kernel-…-livepatch…|kernel-livepatch…|.*-kmp…` (bsc#1190815). Otherwise the removal is reverted.
   - Then the matching `-debugsource`/`-debuginfo` packages of everything removed are removed recursively.

Tests: `zypp/tests/zypp/PurgeKernels_test.cc` with data sets in `zypp/tests/zypp/data/PurgeKernels/{simple,arch,flavour,kernel-base,rckrnl,rebuild,fancybuildnr,withdeps}`. Tool: `zypp/tools/zypp-runpurge.cc` (`--uname`, `--keepSpec`, test case dir; default spec in the tool is `oldest,running,latest`).

---

## 14. End-to-end flag and job reference

### 14.1 Setter → libsolv → zypp.conf → zypper

This joins §4.2 with the zypper side (`zypper/src/solve-commit.cc::set_solver_flags()` and `zypper/src/commands/solveroptionset.cc`). zypper calls `set_solver_flags()` before **every** solver call (`resolve()`, `verify()`, `dist_upgrade()`, and the `doUpdate()` path).

| `zypp::Resolver` setter | libsolv | zypp.conf | zypper |
|---|---|---|---|
| `setFocus` | `SOLVER_FLAG_FOCUS_INSTALLED` / `SOLVER_FLAG_FOCUS_BEST` | `solver.focus` | `--solver-focus Job\|Installed\|Update` (SolverCommonOptionSet). zypper **always** calls `setFocus(_focus)`; with no option `_focus` is `Default`, so the zypp.conf value is used. |
| `setForceResolve` | `SOLVER_FLAG_ALLOW_UNINSTALL` | – | `--force-resolution` / `-R,--no-force-resolution`. Default comes from zypper.conf `solver.forceResolutionCommands` (default: `remove`). Always set explicitly. |
| `setOnlyRequires` | `SOLVER_FLAG_ONLY_NAMESPACE_RECOMMENDED` (`IGNORE_RECOMMENDED` always 0) | `solver.onlyRequires` | `--recommends` / `--no-recommends`. Default from zypper.conf `solver.installRecommends`, whose default is `!ZConfig::solver_onlyRequires()`. `remove` always forces only-requires. Always set **explicitly**, so the zypp.conf default mechanism (§4.1) never applies inside zypper. |
| `setIgnoreAlreadyRecommended` | `SOLVER_FLAG_ADD_ALREADY_RECOMMENDED = !v` | – | `false` only for `dup` (unless `--no-recommends`) and `inr`. `dup --from` sets it back to `true` (`dist_upgrade()`, `\todo` in the code). |
| `setCleandepsOnRemove` | `SOLVER_CLEANDEPS` bit on erase jobs | `solver.cleandepsOnRemove` | `-u,--clean-deps` / `-U,--no-clean-deps`, **only honoured for `remove`** (`set_clean_deps()`). |
| `setUpdateMode` | `SOLVER_UPDATE\|SOLVER_SOLVABLE_ALL` job | – | `patch --with-update` (`solve_with_update`). zypper resets `setUpgradeMode(false)`/`setUpdateMode(false)` after each command (`Zypper.cc`, "reset persistent solver jobs"), which matters in shell mode. |
| `setUpgradeMode` (via `doUpgrade`) | `SOLVER_DISTUPGRADE\|SOLVER_SOLVABLE_ALL` job | – | `dup` without `--from` |
| `addUpgradeRepo` | `SOLVER_DISTUPGRADE\|SOLVER_SOLVABLE_REPO` job | – | `dup --from <repo>` |
| `setSystemVerification` (via `verifySystem`) | `SOLVER_VERIFY\|SOLVER_SOLVABLE_ALL` job | – | `verify` |
| `setRemoveOrphaned` | `SOLVER_DROP_ORPHANED\|SOLVER_SOLVABLE_ALL` job | – | `dup --remove-orphaned` (only passed for `dup`) |
| `setRemoveUnneeded` | weak `SOLVER_ERASE\|SOLVER_SOLVABLE_NAME` per unneeded item | – | not used by zypper (verified: no caller in `zypper/src`) |
| `setAllowDowngrade` / `dupSetAllowDowngrade` | `SOLVER_FLAG_ALLOW_DOWNGRADE` / `SOLVER_FLAG_DUP_ALLOW_DOWNGRADE` | – / `solver.dupAllowDowngrade` | `--[no-]allow-downgrade` (SolverInstallsOptionSet, "Expert options"). For `dup` the `dupSet*` variant is called, otherwise the plain one. Only called if the option was given (TriBool). |
| `setAllowNameChange` / `dupSetAllowNameChange` | `..._ALLOW_NAMECHANGE` / `..._DUP_ALLOW_NAMECHANGE` | – / `solver.dupAllowNameChange` | `--[no-]allow-name-change` |
| `setAllowArchChange` / `dupSetAllowArchChange` | `..._ALLOW_ARCHCHANGE` / `..._DUP_ALLOW_ARCHCHANGE` | – / `solver.dupAllowArchChange` | `--[no-]allow-arch-change` |
| `setAllowVendorChange` / `dupSetAllowVendorChange` | `..._ALLOW_VENDORCHANGE` / `..._DUP_ALLOW_VENDORCHANGE` | `solver.allowVendorChange` / `solver.dupAllowVendorChange` | `--[no-]allow-vendor-change` |
| `setNoUpdateProvide` | `SOLVER_FLAG_NO_UPDATEPROVIDE` | `solver.noUpdateProvide` | – |
| `setSolveSrcPackages` | (only affects `SATCollectTransact`) | – | never called by zypper (verified by grep) |
| (internal, always on) | `SOLVER_FLAG_SPLITPROVIDES` | – | – |
| `addRequire` / `addConflict` | `SOLVER_INSTALL\|SOLVER_SOLVABLE_PROVIDES` / `SOLVER_ERASE\|SOLVER_SOLVABLE_PROVIDES [\|CLEANDEPS]` | – | capability arguments (`zypper in 'foo>1.0'`, `--capability`), `zypper rm` of a capability, and **selection of locked items** (§15.2) |
| (soft request) | `SOLVER_WEAK` bit | – | only from `SolverQueueItem*(…, soft=true)` and the soft-lock/removeUnneeded jobs |
| `addWeak` (via "ignore" solution) | `SOLVER_WEAKENDEPS\|SOLVER_SOLVABLE` | – | choosing the "break … by ignoring some of its dependencies" solution |

The `ZConfig` keys are parsed in `zypp/zypp/ZConfig.cc` (`solver.focus` … `solver.upgradeRemoveDroppedPackages`, plus `solver.checkSystemFile` and `solver.checkSystemFileDir`).

### 14.2 libsolv features libzypp does *not* use

A grep over `zypp/zypp` (verified) finds no use of: `SOLVER_FORCEBEST`, `SOLVER_FLAG_BEST_OBEY_POLICY`, `SOLVER_FLAG_FOCUS_NEW`, `SOLVER_FLAG_STRICT_REPO_PRIORITY`, `SOLVER_FLAG_INSTALL_ALSO_UPDATES`, `SOLVER_FLAG_KEEP_ORPHANS`/`BREAK_ORPHANS`, `SOLVER_FLAG_YUM_OBSOLETES`, `SOLVER_FLAG_NO_INFARCHCHECK`, `SOLVER_FLAG_NEED_UPDATEPROVIDE`, the `SOLVER_SETARCH/SETVENDOR/SETEV(R)/SETREPO/SETNAME` job bits, `SOLVER_LOCK`, `SOLVER_FAVOR`/`DISFAVOR`, `SOLVER_ALLOWUNINSTALL` (job form), `SOLVER_TARGETED`, `SOLVER_ORUPDATE`, `SOLVER_EXCLUDEFROMWEAK`. All of these keep the libsolv defaults. libzypp reads policy solution elements only through `solver_next_solutionelement()` as "replace p with rp" and works out the reason itself with `policy_is_illegal()` (§6.1). If a user asks for "install exactly this vendor/arch", the answer in libzypp is a NEVRA request (`SOLVER_INSTALL|SOLVER_SOLVABLE`) or a capability `addRequire`. There is no SET* equivalent.

Locks are implemented with plain `INSTALL`/`ERASE` jobs (§11), not with `SOLVER_LOCK`. This is why a lock conflict appears as "remove lock to allow …" solutions built from `INSTALL|SOLVABLE` / `ERASE|SOLVABLE` jobs.

---

## 15. Request APIs in detail

### 15.1 Three ways to ask

| Mechanism | Stored in | libsolv job | Lifetime |
|---|---|---|---|
| `PoolItem::status().setToBeInstalled/Uninstalled(USER\|APPL_HIGH)`, `setLock`, soft locks (`KEEP_STATE`) | `ResStatus` bits | `INSTALL\|SOLVABLE`, `ERASE\|SOLVABLE`, lock jobs (§3.2, §11) | until the app resets them, or `verifySystem()` (APPL_HIGH) |
| `Resolver::addRequire(cap)` / `addConflict(cap)` | `detail::Resolver::_extra_requires/_extra_conflicts` (`CapabilitySet`) | `INSTALL\|SOLVABLE_PROVIDES` / `ERASE\|SOLVABLE_PROVIDES\|MAYBE_CLEANDEPS` | until `removeRequire/removeConflict`, `reset()`, or a `REMOVE_EXTRA_*` solution. **Not** cleared by `undo()`. |
| `resolveQueue(SolverQueueItemList&)` | caller's list | per item (§5.5) | per call; solutions edit the list (see below) |

`addRequire` means "something providing this cap must be installed". libsolv picks the best provider. Providers that end up selected are marked `APPL_LOW` in copy-back so they do not become sticky user requests (§3.3). `getRequire()`/`getConflict()` return the sets (zypper uses `_requires`/`_conflicts` for its own feedback).

### 15.2 zypper: locked items become requires/conflicts

`zypper/src/SolverRequester.cc::setToInstall()/setToRemove()`: if the selectable `hasLocks()`, zypper does **not** set the status (that would silently override the lock, see chapter 03). Instead it adds `addRequire(Capability(arch, name, ==, edition, kind))` or `addConflict(...)`. The solver then reports the conflict with the lock job, and the user sees "remove lock to allow installation of …". Code comment: *"Basically the right way but zypp solver job API needs polishing as there are better jobs than 'addRequire'."* With `--force`, zypper calls `setToBeInstalled(USER)` directly.

### 15.3 `resolveQueue` round trip

```
app queue ──► detail::Resolver::resolveQueue(queue)
              1. solverInit()
              2. for each _removed_queue_items: erase first queue entry with cmp()==0
              3. for each _added_queue_items:   push_back if no cmp()==0 entry
              4. clear both lists
              5. SATResolver::resolveQueue(queue, _addWeak)
                    solverInit(weak) → item->addRule(_jobQueue) for each
                    → solverAddJobsFromPool() → solving()
```

- `SolverQueueItem::cmp()` compares type first (`compare()`), then: Install/Delete by **name string**, Update/Lock by `compareByNVRA(item)`, InstallOneOf by `==` of the PoolItem list (otherwise `-1`, "more evaluation would be not useful"). The `_soft` flag is **not** part of `cmp()`.
- Solutions create *new* `SolverQueueItemInstall/Delete` objects. `Resolver::removeQueueItem()` first looks for the **same pointer** in `_added_queue_items`. A fresh object never matches, so it always ends up in `_removed_queue_items`, and step 2 matches it by name.
- `resolveQueue` does not use `_extra_requires/_extra_conflicts/_upgradeRepos`, but `solverInit` still adds all pool-derived jobs (locks, locales, SystemCheck, mode jobs). Pool transactions are added after the queue jobs.
- `SolverQueueItemLock` erase jobs never get `CLEANDEPS`, unlike pool locks (`MAYBE_CLEANDEPS` in `solverInitSetLocks`). `SolverQueueItemDelete` reads `pool().resolver().cleandepsOnRemove()`.
- Users: in the stack only deptestomatic (`addQueueInstall`, `addQueueDelete`, `addQueueLock`, `addQueueUpdate`, `addQueueInstallOneOf` trial jobs). zypper never calls `resolveQueue`. Whether YaST still does is unverified.

### 15.4 `ResolverFocus` semantics

`zypp-logic/zypp/ResolverFocus.h`:

| Value | Doc string | libsolv effect |
|---|---|---|
| `Default` | "standard behavior (as defined in zypp.conf or 'Job')" | resolved to `ZConfig::solver_focus()` in `Resolver::setFocus` |
| `Job` | "installing the best version of the requested packages" | both flags 0. libsolv prefers to keep installed deps but installs the best version of what was requested. |
| `Installed` | "as little changes to the installed packages as needed" | `FOCUS_INSTALLED=1`. Requested packages may get an *older* version if that avoids touching installed packages. |
| `Update` | "updating requested packages and their dependencies as much as possible" | `FOCUS_BEST=1`. Dependencies of the job are updated as well. |

`fromString()` is case-insensitive, and an empty string means `Default`. zypper rejects invalid values with "Valid values: Job, Installed, Update". Introduced with zypp.conf `solver.focus` in 17.15.0 (bsc#1146415). The focus is written into `zypp-control.yaml` as `focus: <name>` and read back via `resolverFocusFromString` (an unknown name silently becomes `Default`).

---

## 16. Problems and solutions in depth

### 16.1 Lifecycle

```
resolvePool() == false
   │
   ▼
Resolver::problems()  ──► SATResolver::problems()        (recomputed on EVERY call!)
   │   for each libsolv problem:
   │     SATgetCompleteProblemInfoStrings → ResolverProblem(description, details, completeProblemInfo)
   │     for each libsolv solution → ProblemSolutionCombi (+ TransactionSolutionAction*)
   │     if ignoreId → ProblemSolutionIgnore (InjectSolutionAction WEAK)
   │     if PTF involved → prefix description with PtfPatchHint
   ▼
app picks ≤1 solution per problem ──► Resolver::applySolutions(list)
   │     per solution: execute actions in order; stop at first failing action / solution
   ▼
resolvePool() again (solutions only change ResStatus / extra caps / weak list / queue items)
```

- `zypp::Resolver::problems()` is non-const and forwards to `SATResolver::problems()`. That **rebuilds** all `ResolverProblem` objects from libsolv each time, including `solver_findallproblemrules` and the provider scans in `PKG_REQUIRES`. zypper's `show_problem()` calls `God->resolver()->problems().size()` once per displayed problem just to get the count. That is quadratic for big conflict sets (cosmetic, but visible with hundreds of problems).
- There is no undo. `applySolution()` executes actions one by one. If one fails (`setLock`/`setTransact` returned false, e.g. causer too low), the earlier actions **stay applied** and `applySolutions` stops at that solution. The return value is not visible through the public API (it returns `void`). Look for `WAR "apply solution action failed"` in the log.

### 16.2 Rule → text mapping (`SATproblemRuleInfoString`)

`s` = rule source, `s2` = target, both mapped to their buddy (`mapSolvable`). "installed" vs "to be installed" depends on `s.isSystem()` (and `s2.isSystem()`).

| `SolverRuleinfo` | Text (msgid) | `details` | `ignoreId` |
|---|---|---|---|
| `SOLVER_RULE_DISTUPGRADE` | "the installed %1% does not belong to a distupgrade repository and must be replaced" / "the to be installed %1% does not belong to a distupgrade repository" | | |
| `SOLVER_RULE_INFARCH` | "the installed/to be installed %1% has inferior architecture" | | |
| `SOLVER_RULE_UPDATE` | "problem with the installed %1%" | | |
| `SOLVER_RULE_JOB` | "conflicting requests" | | |
| `SOLVER_RULE_PKG` | "some dependency problem" | | |
| `SOLVER_RULE_JOB_NOTHING_PROVIDES_DEP` | "nothing provides the requested '%1%'" | "Have you enabled all the required repositories?" | |
| `SOLVER_RULE_JOB_UNKNOWN_PACKAGE` | "the requested package %1% does not exist" | same hint | |
| `SOLVER_RULE_JOB_UNSUPPORTED` | "unsupported request" | | |
| `SOLVER_RULE_JOB_PROVIDED_BY_SYSTEM` | "'%1%' is provided by the system and cannot be erased" | | |
| `SOLVER_RULE_PKG_NOT_INSTALLABLE` | "%1% is not installable" | | |
| `SOLVER_RULE_PKG_NOTHING_PROVIDES_DEP` | "nothing provides '%1%' needed by the installed/to be installed %2%" | | `source` |
| `SOLVER_RULE_PKG_SAME_NAME` | "cannot install both %1% and %2%" | | |
| `SOLVER_RULE_PKG_CONFLICTS` | "the [to be] installed %1% conflicts with '%2%' provided by the [to be] installed %3%" (4 variants) | | |
| `SOLVER_RULE_PKG_OBSOLETES`, `…_INSTALLED_OBSOLETES` | "… obsoletes '%2%' provided by …" (4 variants) | | |
| `SOLVER_RULE_PKG_SELF_CONFLICT` | "the [to be] installed %1% conflicts with '%2%' provided by itself" | | |
| `SOLVER_RULE_PKG_REQUIRES` | "the [to be] installed %1% requires '%2%', but this requirement cannot be provided" | "deleted providers: …" / "not installable providers: …" | `source` |
| anything else (e.g. `SOLVER_RULE_BLACK`, `SOLVER_RULE_PKG_RECOMMENDS`, `SOLVER_RULE_PKG_CONSTRAINS`, `SOLVER_RULE_YUMOBS`, `SOLVER_RULE_STRICT_REPO_PRIORITY`, `SOLVER_RULE_LEARNT`, `SOLVER_RULE_CHOICE`, `SOLVER_RULE_BEST`) | `solver_problemruleinfo2str()`, **untranslated** English from libsolv | | |

Note that a retracted or PTF package that conflicts because of the blacklist comes through the `default:` branch (`SOLVER_RULE_BLACK`) and is therefore untranslated. Only the *solution* text ("install %1% although it has been retracted", "allow installing the PTF %1%") is libzypp's.

PKG_REQUIRES provider lists: for each provider of the dep, an installed/uninstalled pair with the same NVR (`compareByNVR`) is skipped. The rest are split into installed ("deleted providers") and uninstalled ("not installable providers"). The first entry is printed with `itemToString(*iter)`, the following ones with `itemToString(mapItem(*iter))`, so only the first entry is *not* buddy-mapped (inconsistent, harmless).

**Complete problem info / "cleanup":** `SATgetCompleteProblemInfoStrings()` puts the `solver_findproblemrule()` text first. Of the remaining `solver_findallproblemrules()` entries, `SOLVER_RULE_UPDATE` and `SOLVER_RULE_JOB` classes are dropped **if any other class is present** (`nobad`), and duplicate strings are removed. The `detail`/`ignoreId` of the secondary rules are thrown away. Only the most relevant rule decides whether an "ignore" solution is offered.

### 16.3 Solution elements not covered by the §6.1 table

The job switch masks `SOLVER_SELECTMASK|SOLVER_JOBMASK`, so modifier bits (`WEAK`, `CLEANDEPS`, `ESSENTIAL`) are ignored when matching. Consequences:

- **`SOLVER_INSTALL|SOLVER_SOLVABLE_ONE_OF`** (from `SolverQueueItemInstallOneOf`), `SOLVER_DISTUPGRADE|…`, `SOLVER_VERIFY`, `SOLVER_UPDATE|SOLVABLE_ALL`, `SOLVER_DROP_ORPHANED`, `SOLVER_NOOBSOLETES`, `SOLVER_BLACKLIST`, `SOLVER_WEAKENDEPS` hit the `default:` branch. It logs `ERR "No valid solution available"` and adds **no action and no description**. A solution that consists only of such elements becomes an *empty* `ProblemSolutionCombi`. Applying it does nothing, so the next run gives the same problem. Not seen in practice for the mode jobs because libsolv doesn't usually propose dropping them. For `InstallOneOf` it is plausible (unverified).
- **The rpm arch pin** in DUP (`SOLVER_INSTALL|SOLVER_SOLVABLE_NAME|SOLVER_ESSENTIAL`, §3.1 job #7) is matched as `INSTALL|SOLVABLE_NAME`. That becomes `REMOVE_SOLVE_QUEUE_ITEM(Install "rpm.<arch>")` with the text "do not install rpm.<arch>". In `resolvePool()` the queue item lists are never consumed, so this solution is a no-op (likely bug). libsolv offers dropping `SOLVER_ESSENTIAL` jobs only when no other solution exists (`essentialok` in libsolv `src/problems.c`), so it is rare.
- **Locale jobs** (`INSTALL|SOLVABLE_PROVIDES namespace:language(xx)` and the CLEANDEPS erase) are matched as extra require/conflict. `REMOVE_EXTRA_REQUIRE/CONFLICT` then erases a cap that is not in `_extra_requires/_extra_conflicts`, so the solution is a no-op too (likely bug, unverified in the field). The text reads "do not ask to install a solvable providing namespace:language(xx)".
- `SOLVER_UPDATE|SOLVER_SOLVABLE` for a not-installed item logs `ERR` and adds nothing.
- `ERASE|SOLVABLE_NAME` adds a `KEEP` per installed+transacting item of that ident *and* a queue-item removal. In `resolvePool()` only the KEEPs matter.

### 16.4 Ordering, deduplication and descriptions

- `ResolverProblem::addSolution(sol, inFront)`: multi-action solutions are `push_front`, so among themselves they appear in **reverse** libsolv order. Single-action solutions keep libsolv order. `ProblemSolutionIgnore` is always last.
- Duplicate filter (bsc#985674, "HACK"): a solution is dropped if one with the same `description()`, `details()` **and number of actions** is already in the list. Actions themselves are not compared.
- `ProblemSolutionCombi::addDescription()` → `pushDescriptionDetail(d)`. `addFrontDescription()` → `pushDescriptionDetail(d, true)` (used for the SystemCheck "ignore the warning of a broken system" text). Once there are two descriptions, `description()` becomes "Following actions will be done:" and the individual texts go into `details()` (§6.1).
- `ProblemSolutionIgnore` has two ctors: per item ("break %s by ignoring some of its dependencies") and a list form ("generally ignore of some dependencies"). Only the single-item form is created by `SATResolver::problems()`.
- `InjectSolutionKind` has only `WEAK`. The class comment talks about "inject an artificial provides", but the real effect is `Resolver::addWeak(item)` → `SOLVER_WEAKENDEPS|SOLVER_SOLVABLE` on the next run. libsolv then treats the item's requires as weak. `_addWeak` lives until `undo()`; `reset()` doesn't clear it (§5.6). The weak list is **not** written into test cases (§9.1).

### 16.5 How zypper renders and applies

`zypper/src/solve-commit.cc`:

- `dumpProblem()`: `Problem: N <description()>`, then `details()`. In verbose mode it shows "Detailed information:" followed by every `completeProblemInfo()` line instead of the description. Verbose mode is the default when non-interactive (bsc#1277790) and can be toggled with `d`.
- `dumpSolution()`: ` Solution N: <description()>` plus the indented `details()`.
- Prompt: `1/2/…/s/r/c/d` if there is more than one problem (skip, retry, cancel, details), otherwise `1/…/c/d`. The default answer is *cancel*. Chosen solutions are collected and applied together with `applySolutions()` after the last problem (or on retry), followed by a new solver run.
- `patch --skip-not-applicable-patches`: before asking, every solution with `getIfSkipsPatchesOnly()` (all actions are `KEEP` on patches) is auto-applied. If at least one was auto-applied, zypper re-solves without asking and lists the skipped patches in the summary.
- `--debug-solver` skips the problem dialogue entirely and writes the test case even on failure.

---

## 17. Test case format in depth

### 17.1 Directory content

Written by `solver/detail/Testcase.cc::createTestcase()`:

| File | Written by | Content |
|---|---|---|
| `testcase.t` | libsolv `testcase_write()` | libsolv's testcase language: `repo <name> <prio> testtags <file>`, `system <arch> rpm @System`, `job …` lines (exactly the libzypp `_jobQueue`, including blacklist, multiversion, userinstalled jobs), `solverflags …`, `result transaction,problems <inline>` |
| `solver.result` | libsolv | expected result (`TESTCASE_RESULT_TRANSACTION\|TESTCASE_RESULT_PROBLEMS`) |
| `<mangled-name>.repo.gz` | libsolv | one per repo, including `@System`, in libsolv **testtags** format (susetags-like `=Pkg:`/`=Prv:` …). Names come from `testcase_mangle_repo_names()`. |
| `zypp-control.yaml` | libzypp (yaml-cpp) | `version: 1.0`, `setup:` and `trials:` (below) |
| `zypp-locks.yaml` | libzypp | `lock` jobs (only if there are locked items) |
| `zypp-autoinst.yaml`, `zypp-modalias.yaml`, `zypp-multiversion.yaml` | libzypp | only if the list has more than 10 entries; otherwise inline |
| `y2log` | `LogControl` redirect | excessive log of the re-run, only with `runSolver=true` |

`zypp-control.yaml` skeleton (keys verified against the writer):

```yaml
version: 1.0
setup:
  channels:
    - alias: repo-oss          # one per repo that has at least one solvable, incl. @System
      url: [ ... ]             # all repoOrigins() endpoints
      path: /
      type: rpm-md
      generated: 2026-10-01 12:00:00
      outdated: ...
      priority: 99
      file: repo-oss.repo.gz
  arch: x86_64
  solverTestcase: testcase.t
  solverResult: solver.result
  locales: [ {fate: added|"", name: de}, {fate: removed, name: fr} ]
  vendors: [ [suse, opensuse], ... ]   # VendorAttr equivalence lists
  autoinst: [...] | zypp-autoinst.yaml
  modalias: [...] | zypp-modalias.yaml
  multiversion: [...] | zypp-multiversion.yaml
  resolverFlags: { focus: Job, ignorealreadyrecommended: true, onlyRequires: false, forceResolve: false,
                   cleandepsOnRemove: false, noUpdateProvide: false, allowDowngrade: false, ... dupAllowVendorChange: false }
trials:
  - trial:
      - { job: install,   kind: package, name: foo, status: ..., channel: repo-oss, arch: x86_64, version: 1.2, release: 3.1 }
      - { job: keep,      ... }                    # KEEP_STATE (soft locks) not by solver
      - { job: uninstall, kind: package, name: bar, status: ... }   # NO channel/version/arch
      - { include: zypp-locks.yaml }
      - { job: addRequire,  name: "glibc" }        # extra requires AND SystemCheck caps
      - { job: addConflict, name: "..." }
      - { job: upgradeRepo, name: <alias> }
      - { job: distupgrade } | { job: update } | { job: verify }
```

Item lists are built from `isToBeInstalled()/isKept()/isToBeUninstalled()/isLocked()` with `!isBySolver()`. Untouched items have causer `SOLVER` and are therefore not listed.

### 17.2 Readers

- **libsolv `testsolv testcase.t`** replays the *raw* job queue: pure libsolv, no libzypp policy, no problem translation. It is the right tool to decide "libsolv bug or libzypp bug" and to attach to libsolv bug reports. `testsolv -r` regenerates the result.
- **`zypp-solver-testcase-runner` (deptestomatic)** reads `zypp-control.yaml` via `misc::LoadTestcase` (type `Yaml`; `Helix` for old XML). `YamlTestcaseHelpers.h` parses `setup` (also accepts legacy keys `system`, `hardwareInfo`, `sources`, `mediaid`, `systemCheck`, `setlicencebit`, and the old flag aliases `ignorealready`/`ignorerecommended`) and turns each `trial` entry into a `TestcaseTrial::Node` (`job` → name, scalars → properties, nested maps/sequences → children, `include:` → load another YAML file relative to the testcase dir). Channels with `file:` are loaded as `TestcaseRepoType::Testtags`.
- deptestomatic job names (`zypp-solver-testcase-runner/src/deptestomatic.cc`): `install` (props `channel name|package kind version release arch soft`), `install <spec>` (shortcut: `Capability::guessPackageSpec` → `addExtraRequire`, e.g. `job: install patch:openSUSE-SLE-15.4-2022-2831`), `uninstall` (props `name kind ver rel arch`, looked up in `@System`), `lock` (with version → `setLock(USER)` on that item, without → selectable set to Taboo), `keep`, `addRequire`/`addConflict` (comma-separated `name`, optional `kind`), `upgradeRepo`, `distupgrade` (calls `doUpgrade()` right away), `update`/`verify` (both → `setUpdateMode(true)` + `resolvePool()`, see "libzypp@ca9bcf16", and `vverify` for real verify), `reportproblems`, `takesolution problem=N solution=M`, `validate`, `showpool`, `showstatus`, `showselectable`, `whatprovides`, `availablelocales`, `instorder`, `addQueue{Install,Delete,Lock,Update,InstallOneOf}`, `createTestcase`, `note`. If no `distupgrade`/`update` job ran, a final `resolvePool()` (or `resolveQueue()` if queue jobs exist, or `verifySystem()` with `vverify`) is done at the end.

### 17.3 Editing a test case to try variants

- Flip flags in `resolverFlags` (e.g. `allowVendorChange: true`, `focus: Update`) and re-run deptestomatic. To get the pure libsolv equivalent, edit the `solverflags` line in `testcase.t`.
- Add a request: append `- { job: "install foo" }` or `- { job: addRequire, name: "foo >= 2" }` to the trial. Remove a request by deleting its line.
- To see problems and solution numbers, add `- { job: reportproblems }`. To apply one: `- { job: takesolution, problem: 1, solution: 2 }`, then another `reportproblems`.
- Shrink: delete whole channels from `setup.channels` (and their `.repo.gz`), or edit a `.repo.gz` (gunzip, edit `=Req:`/`=Prv:` blocks, gzip) to bisect metadata.
- **Caveats when a test case does not reproduce:** `removeOrphaned`, `removeUnneeded`, `solveSrcPackages` and the weak ("ignore") items are not recorded (§9.1). `uninstall` jobs carry no version/arch, so with several installed instances of one name (multiversion kernels) the runner's `get_poolItem("@System", name, …)` may pick a different instance (unverified which one). A test case written with `runSolver=false` (DUP commit, `ZYPP_FULLLOG` follow-ups) captures the state *after* the app applied its own solutions. And `doUpdate()` runs are replayed as `setUpdateMode`+`resolvePool`, so user requests suddenly count (see §5.2).

---

## 18. Recommends, supplements, namespaces and rich dependencies

### 18.1 How the flags combine

| zypper | `onlyRequires` → `ONLY_NAMESPACE_RECOMMENDED` | `ignoreAlreadyRecommended` → `ADD_ALREADY_RECOMMENDED` | What libsolv follows |
|---|---|---|---|
| `in` (default zypper.conf) | 0 | 0 (ignore=true) | recommends/supplements of **newly installed** packages, and supplements that become true because of the new installs |
| `in --no-recommends`, `rm` | 1 | 0 | only recommends/supplements that contain a `REL_NAMESPACE` (language/modalias/filesystem), for new installs |
| `inr` | 0 | 1 | recommends of **all** installed packages are re-evaluated, plus all fulfilled supplements |
| `dup` | 0 | 1 (unless `--no-recommends`) | as `inr`. `dup --from` resets to 0. |
| `up` / `patch` | follows zypper.conf `installRecommends` | 0 | as `in` |

libsolv details (`libsolv/src/solver.c`, `solver_util.c`): with `addalreadyrecommended == 0`, recommends of installed solvables are skipped, and supplemented packages are filtered with `solver_is_supplementing_alreadyinstalled()` (only supplements that weren't already fulfilled by the installed system count). With `only_namespace_recommended`, a recommends is skipped unless `solver_is_namespace_dep()` ("cheat: we just look if there is REL_NAMESPACE in the dep"), and a supplement only counts if the namespace part becomes fulfilled. This is the libzypp "no-recommends: Nevertheless consider resolver namespaces" change (FATE#325513, libsolv ≥ 0.7.2). The 2022 changelog entry "Resolver: Fix missing --[no]-recommends initialization in update" (zypper#459, bsc#1201972) is the reason `doUpdate()` now also goes through `set_solver_flags()`.

Recommended/suggested items show up as weak status bits after copy-back (`solverCopyBackWeak`, §3.3). They are **not** transactions, so the UI must not treat them as user requests.

### 18.2 Language and modalias packages

- `-lang` / translation packages carry `Provides: locale(foo:de;fr;…)` (and/or supplements such as `packageand(foo:bundle-lang-other)`). libsolv's `repo_fix_supplements()` turns the `locale(pkg:l1;l2)` provides into a **supplements** `foo AND (namespace:language(l1) OR namespace:language(l2))`. Installing `foo` while `de` is requested makes libsolv add the lang package through the supplements step, also with `--no-recommends` (namespace).
- Adding a requested locale later does **not** need a "solve all": libzypp turns the change into `SOLVER_INSTALL|SOLVABLE_PROVIDES namespace:language(de)` (job #4), so libsolv installs everything that supplements that namespace for installed packages. Removing a locale is an erase **with cleandeps**.
- modalias: `namespace:modalias(hexvalue)` supplements of kmp/firmware packages are answered by `target::Modalias` (§12). With no requested-locale change and no new install, modalias supplements of *uninstalled* packages are only picked up in "add already recommended" mode (`inr`, `dup`). That is why `zypper inr` is the documented way to pull in new hardware support (inferred from the flag semantics, not from a libzypp comment).

### 18.3 `packageand()` and rich (boolean) deps

- libzypp does not evaluate dependencies. All of this is libsolv. For `(a and b)`-style strings, `Capability(string)` ends up in `sat::detail::PoolImpl::parserpmrichdep()` → `::pool_parserpmrichdep()` (`zypp/zypp/sat/detail/PoolImpl.h`). Metadata parsed by libsolv already contains the `REL_AND/REL_OR/REL_COND/REL_UNLESS/REL_WITH/REL_WITHOUT` reldeps. `CapDetail` exposes them as `EXPRESSION` with `CapRel` (`zypp/zypp/Capability.h`).
- Old SUSE pseudo-syntax in **supplements** is rewritten by libsolv's `repo_fix_supplements()` (`libsolv/src/suse.c`) when the metadata is parsed: `packageand(a:b:c)` → `a AND b AND c` (with a special case for a `pattern:` prefix), `modalias(...)`/`language(...)`/`filesystem(...)` → `namespace:…`, and old `foo:/path` → `namespace:splitprovides(foo with /path)`. The exact callers (susetags/rpmmd/rpmdb parsers) are unverified. The libzypp changelog's "Fix packageand() in testcase generation" refers to the obsolete Helix writer.
- `SOLVER_FLAG_SPLITPROVIDES` (always on in libzypp) is what makes `splitprovides` work: a package that provides `oldpkg:/some/file` replaces `oldpkg` only if that file was installed.
- Patterns: since libzypp 14.7 patterns are backed by real packages identified by the `pattern()` provides (`Pattern.cc`, `indicator( "pattern()" )`, §8.1). Their contents are ordinary `requires`/`recommends`/`suggests` of the pattern package (rich deps are possible), and libsolv handles them like any other package deps. So `--no-recommends` also drops a pattern's recommended members. Other pattern-package tags (visibility, order, …) are covered in chapter 03 and are not used by the solver.
- Complex deps in **problem texts** are printed with `pool_dep2str`, e.g. "nothing provides '(a or b)' needed by …".

---

## 19. Patches from libsolv's point of view

### 19.1 The patch solvable

libsolv's updateinfo parser (`libsolv/ext/repo_updateinfoxml.c`) creates one `patch:<id>` solvable per update. Every `<package>` entry becomes a **conflict** `name.arch < evr` (for non-noarch, also `name.noarch < evr`). The comment there reads `patch.conflicts: {name} < {version}.{release}`. So "installing a patch" means "no installed package may be older than the fixed version". The solver satisfies that by updating (or, if impossible, by removing) the affected packages. A patch has no requires on the packages. So when the fixed package versions can't be installed, the problems are conflict-style ("the to be installed patch:… conflicts with 'foo.x86_64 < 1.2' provided by the installed foo-1.1…"), never "nothing provides".

### 19.2 Status computation (`solvable_trivial_installable_map`, `libsolv/src/suse.c`)

`SATResolver`'s `establish()` and `solverCopyBackValidate()` use `solver_trivial_installable()`, which calls it per patch:

- Any conflict matched by an installed package → **0** (BROKEN → *needed*), unless `solvable_is_irrelevant_patch()` → **-1**.
- No conflict hits, but some conflict *name* is installed → **1** (SATISFIED).
- None of the names installed → **-1** (NONRELEVANT).
- `solvable_is_irrelevant_patch()` returns true if the patch's `UPDATE_STATUS` is `retracted`. It also returns true if fixing packages exist in the patch's own repo but none of them is a legal vendor change from the installed package (`si->vendor == s2->vendor`, or `!pool_illegal_vendorchange()`). `pool_illegal_vendorchange()` calls libzypp's custom vendor check (`pool_set_custom_vendorcheck`, §3.1 job #0), so the `VendorAttr`/`/etc/zypp/vendors.d` equivalence **does** apply. If the patch repo has no fixing package at all, the patch is *not* irrelevant. So a retracted patch is never "needed", and neither is a patch for a package you installed from a non-equivalent vendor (e.g. a third-party build).

Multiversion packages are handled through the multiversion map (installing a newer kernel satisfies the patch without removing the old one).

### 19.3 libzypp side

- `establish()` creates a solver with only the multiversion `NOOBSOLETES` jobs, calls `solver_solve` (an empty run) and then `solver_trivial_installable` on all pseudo-installed items. It runs whenever the pool serial changes (`pool/PoolImpl.h`, `EstablishedStatesImpl`), i.e. after repo load/refresh, **not** per solver run.
- `PoolItem::isNeeded()` = to-be-installed or (broken and not locked). `isUnwanted()` = broken and locked (patch locks are the "skip this patch forever" mechanism, bnc#577118).
- `Patch::restartSuggested()` (metadata `restart_suggested`, i.e. affects the package manager) is the only "update stack" marker. There is no `Patch::isPackageManagementPatch`.

### 19.4 How `zypper patch` builds its request

1. `zypper/src/commands/patch.cc`: `--with-update` → `solve_with_update=true` and implicitly "with optional" (bsc#1102261). `--updatestack-only` excludes `--with-update`, `--bugzilla`, `--cve`.
2. `SolverRequester::updatePatches(updateStackOnly)`: pass 1 calls `installPatch(…, ignore_pkgmgmt=false)` for every patch, which only selects **broken patches with `restartSuggested()`**. If anything was selected, it stops, and if `--with-update` was given, `solve_with_update` is switched off (update stack first, everything else on the next run). Otherwise (and not `--updatestack-only`), pass 2 runs with `ignore_pkgmgmt=true`.
3. `installPatch()`: broken → reject unwanted (locked), optional (`skip_optional_patches`), too interactive (`interactiveWhenIgnoring(ignoreFlags)`, where `Reboot`/`License` can be ignored by config/`--auto-agree-with-licenses`), and CLI date/category/severity mismatches. Then `setToInstall()` → `Selectable::setOnSystem(pi, USER)` (or `addRequire` of the exact NEVRA if the patch is locked, §15.2). A satisfied patch with `zypper in patch:…` gives "already installed", a non-relevant one "not needed".
4. `solve_and_commit()` → `resolvePool()` with `SOLVER_INSTALL|SOLVER_SOLVABLE` per patch (+ `SOLVER_UPDATE|SOLVABLE_ALL` with `--with-update`). Conflicts with PTFs give the bsc#1194848 hint (§6.1). `--skip-not-applicable-patches` auto-applies "do not install patch" solutions (§16.5).

`zypper list-patches`/`patch-check` (`zypper/src/update.cc`) do no solving. They just read `isBroken()/isUnwanted()/restartSuggested()` from the established states.

---

## 20. Performance and diagnostics

### 20.1 Built-in statistics

By default `sat::detail::PoolImpl` sets `pool_setdebugmask(SOLV_DEBUG_JOB|SOLV_DEBUG_STATS)`. `logSat()` routes `SOLV_DEBUG_STATS` lines to `DBG` and other lines to `MIL` in log group **`libsolv`**, and filters noise ("job: drop orphaned", "job: user installed", "job: multiversion", "  - no rule created", "    next rules: 0 0"). So every `zypper.log` contains libsolv's timing lines for each solve (strings from `libsolv/src/solver.c`):

```
solver started
dosplitprovides=1, noupdateprovide=0, noinfarchcheck=0
allowuninstall=…, allowdowngrade=…, …            ← effective flags as libsolv sees them
dontinstallrecommended=…, addalreadyrecommended=… onlynamespacerecommended=…
added N pkg rules for installed solvables / for updaters / for packages involved in a job / because of weak dependencies
N of M installable solvables considered for solving
pkg rule creation took N ms
N pkg rules, 2 * N update rules, N job rules, N infarch rules, N dup rules, N choice rules, N best rules, N yumobs rules
N black rules, N recommends rules, N repo priority rules
overall rule memory used: N K
solver statistics: N learned rules, N unsolvable, N minimization steps
solver took N ms
final solver statistics: N problems, N learned rules, N unsolvable
solver_solve took N ms
create_solutions for problem #N took N ms     ← per problem, during problems()
```

These lines are the quickest way to verify which flags actually reached libsolv. Compare them with libzypp's own flag dump (`<resolver> ALLOW_DOWNGRADE = …`, `OUTS` macro in `SATResolver.cc`). `LIBSOLV_DEBUGMASK` overrides the default mask completely (§"Where to start").

### 20.2 What makes it slow

- **Rule creation scales with what is "involved"**: DUP (`SOLVER_DISTUPGRADE|SOLVABLE_ALL`), `update` (`SOLVER_UPDATE|SOLVABLE_ALL`) and `verify` touch every installed solvable. Many enabled repos with many versions of the same package (e.g. full update repos without cleanup) multiply update/dup rules. Check the "N of M installable solvables considered" line.
- **Multiversion**: every multiversion solvable gets a `SOLVER_NOOBSOLETES` job (job #5), and the `establish()` run repeats them. A broad `multiversion =` spec (e.g. `provides:multiversion(kernel)` *plus* custom globs) inflates the job queue and the install set.
- **DUP runs the solver twice** when a product carries a `weakremover` drop list (§3.3, bsc#1155819).
- **Problems are expensive**: `create_solutions` is done by libsolv for each problem; libzypp then calls `solver_findallproblemrules`, scans providers for every `PKG_REQUIRES` rule, and does `pool_queuetowhatprovides`/`WhatProvides` lookups. Callers that call `problems()` repeatedly (zypper's prompt, §16.1) pay this each time.
- **`establish()`** runs a full (empty-job) solver on every pool change. Applications that add/remove repos in a loop pay it per change, plus `sat::Pool::prepare()` (whatprovides) on the next solve.
- **Copy-back**: `solverCopyBackWeak` (`solver_get_recommendations`, `solver_get_orphaned`, `solver_get_unneeded(…,1)`) and the lazy `WhatObsoletes` pass are per run. `solver_get_unneeded` with `filtered=1` can be noticeable on large installed systems.
- **ZYPP_FULLLOG / `--debug-solver`**: the test case writer dumps all repos (gzip testtags) and re-runs `resolvePool()` with excessive logging into `y2log`. Never benchmark with these enabled.

### 20.3 Knobs that exist (and ones that don't)

- There is **no** retry limit, timeout or "max solver runs" setting. `MAXSOLVERRUNS` in `SATResolver.cc` is defined but unused (§2), and there is no `setSolverMaxRetries`.
- Real levers: fewer enabled repos / repo priorities (`zypper mr -p`), a narrower `multiversion` spec, `solver.focus=Installed` (fewer changes, smaller transaction), `--no-recommends` (skips the recommends phase except namespaces), and for DUP `solver.upgradeRemoveDroppedPackages=false` (skips the second pass; changes behaviour!).
- To measure: compare `solver_solve took` and `pkg rule creation took` in `zypper.log`. For libsolv-only profiling, run `testsolv` on the `testcase.t` from `--debug-solver` (it uses the same job queue).

---

## Pitfalls & special cases

- **Only `APPL_HIGH`/`USER` transactions are requests.** Every run clears `SOLVER`/`APPL_LOW` transactions in `SATCollectTransact`. Setting status with a low causer and expecting it to stick will silently fail.
- **`verifySystem()` drops all `APPL_HIGH` transactions** (it resets with `APPL_HIGH`). Only USER requests survive.
- **Modes are sticky**: `doUpgrade()`, `doUpdate()` and `verifySystem()` set flags that stay on for later `resolvePool()` calls. `reset()` only clears `_verifying` and the extras.
- **`doUpdate()` ignores all pool jobs**, extra requires and upgrade repos. It returns nothing and creates no problems. Use `setUpdateMode(true)` + `resolvePool()` to combine requests with "update all".
- `problems()` must be called before the next solver run. The libsolv solver and job queue are rebuilt every time, and solutions index into `_jobQueue`.
- **The flag defaults are duplicated** in `Resolver.cc` (`ZVARDEFAULT`) and in the `SATResolver` ctor. Change both.
- `allowNameChange` defaults to `true` (bsc#1071466). `ignoreAlreadyRecommended` defaults to `true`. Neither is configurable in zypp.conf.
- `dupAllowVendorChange` default changed true → false after Code12. Old bug reports may assume the old default.
- **onlyRequires still follows namespace recommends** (language/modalias). The doc comment in `Resolver.h` is misleading.
- DUP needs a **second solve** for product drop lists (bsc#1155819), unless `removeOrphaned` is on or `solver.upgradeRemoveDroppedPackages=false`.
- In DUP the installed `rpm` arch is pinned with `SOLVER_ESSENTIAL`, but only when `systemRoot()=="/"`.
- `dup-vendor-relax(suse)` provided by an available release package switches DUP to the relaxed vendor check (bsc#1182629).
- `removeOrphaned` is only honoured in DUP. Unrepo'd 3rd-party rpms count as orphans.
- `removeUnneeded` only works after a previous solver run (it uses the `UNNEEDED` bits from the last run). Only autoinstalled packages can be unneeded.
- The **autoinstalled list** goes `/var/lib/zypp/AutoInstalled` → `pool_add_userinstalled_jobs` → `solver_get_userinstalled` → `getTransaction()` (FIXME hack) → written on commit (not on dry-run).
- **PTFs**: `ptf()` and `retracted-patch-package()` providers are blacklisted, `ptf-package()` is not (bsc#1186503). With only `@System` loaded, installed PTFs are pinned (bsc#1203248). Problem descriptions get PTF hints (bsc#1194848).
- **Problem solutions can persist locks**: `REMOVE` on a to-be-installed item and `UNLOCK` act at `USER` level, and `getHardLockQueries()` writes them to `/etc/zypp/locks` by name on commit. `KEEP`/`LOCK` use `APPL_HIGH` and are not persisted.
- Soft locks (KEEP_STATE by APPL_HIGH/USER) are **by name** and ignored if any version is installed (bnc#749418).
- `SATResolver::applySolutions()` builds a throwaway `Resolver`. Never use it.
- Solutions with more than one action come first. The "ignore dependencies" solution always comes last. `ProblemSolution::setDetails()` appends instead of replacing.
- Only **patches** are pseudo-installed. Patterns are not (since Fate#309385). The patch status is set by an *empty* "establish" solver run on pool change and refreshed after every run.
- The "update stack first" policy (`restartSuggested()` patches) lives in **zypper**, not libzypp.
- `SystemCheck` is a static singleton with global state. It is loaded once and does not reload on target change. SystemCheck caps show up as `addRequire`/`addConflict` in test cases.
- `Testcase::createTestcase(runSolver=true)` re-runs **`resolvePool()`** (not `doUpdate`/`resolveQueue`). It also cleans the target dir when dumping the pool, and does not record `removeOrphaned`, `removeUnneeded`, `solveSrcPackages` or weak ("ignore") items.
- `Testcase_helix.cc` is not compiled. The reader still supports Helix test cases.
- `PurgeKernels::markObsoleteKernels()` sets `forceResolve(true)` on the global resolver and never resets it. If `running` is in the spec and `uname` fails, nothing is removed. If the running kernel isn't installed, the whole flavour is kept.
- The `multiversion.kernels` comment in `zypp.conf.LEGACY` claiming libzypp doesn't evaluate it is outdated.
- The `zypp::solver::detail` headers are installed and exported for deptestomatic (`ZYPP_API_DEPTESTOMATIC`). Changing them can break `zypp-solver-testcase-runner`.
- **`zypper up` (no args) uses `doUpdate()`**: no problems are ever shown, and blocked updates are silently kept. `zypper patch --with-update` and `zypper up <pkg>` use `resolvePool()`.
- zypper always sets `onlyRequires`, `forceResolve` and `ignoreAlreadyRecommended` **explicitly**, so the zypp.conf default/target-change mechanism (§4.1) only matters for those flags in other apps (YaST/PK). `--clean-deps` is only honoured by `zypper rm`.
- zypper turns selecting/removing a **locked** item into `addRequire`/`addConflict` of the exact NEVRA (§15.2). Such requires survive `undo()`, so long-lived apps doing the same must remove them.
- `Resolver::problems()` **recomputes** everything on each call (zypper calls it once per displayed problem). Cache the list.
- `applySolutions()` has no rollback. A failing action leaves earlier actions applied and stops silently (`void`).
- Solutions built from unhandled job types (InstallOneOf, the DUP rpm-arch pin, locale jobs) are empty or no-ops, so applying them reproduces the same problem (§16.3).
- Rule types other than the 17 handled ones (e.g. `SOLVER_RULE_BLACK` for retracted/PTF blacklists) give **untranslated** libsolv texts.
- Duplicate solutions are filtered by description+details+action count only (bsc#985674). Multi-action solutions appear in reverse libsolv order.
- Patches are `conflicts: name < evr` solvables. "Needed" = an installed package is older than a fixed one *and* a vendor-compatible fix exists in the patch's repo. Retracted patches are never needed (libsolv `solvable_is_irrelevant_patch`).
- In test cases, `uninstall` jobs have no version/arch, and SystemCheck caps appear as plain `addRequire`/`addConflict`. Replaying a test case on a machine with its own `/etc/zypp/systemCheck` adds those caps a **second** time through the runner's SystemCheck (unverified whether the runner disables it).
- `SolverQueueItemLock` erase jobs never get CLEANDEPS, but pool locks do (with `cleandepsOnRemove`).
- There is no solver timeout/retry knob. `MAXSOLVERRUNS` is unused.
- A libsolv version bump can change solver results. The spec file pins minimum libsolv versions (e.g. `>= 0.7.40` for bsc#1279541, SLES 16.1 migrations). Check `package/libzypp.changes` when results differ between builds.

## Where to start when debugging

- **Get a test case first.** Ask for `zypper --debug-solver <cmd>` (→ `/var/log/zypper.solverTestCase`), or for migrations the `/var/log/updateTestcase-*` dir that DUP commits leave behind. YaST: `/var/log/YaST2/solverTestcase`. For multi-run apps set `ZYPP_FULLLOG=1` → `/var/log/YaST2/autoTestcase`.
- **Replay it**: libsolv `testsolv testcase.t` (pure libsolv behaviour, independent of libzypp), or the stack's `zypp-solver-testcase-runner` (deptestomatic, which reads `zypp-control.yaml` through `zypp/zypp/misc/LoadTestcase`). In unit tests use `TestSetup::loadTestcaseRepos()` / `LoadSystemAt()` (`zypp/tests/lib/TestSetup.h`), as in `zypp/tests/zypp/Resolver_test.cc`.
- **Logs**: grep `zypp::solver` in `/var/log/zypper.log`. Useful markers:
  - `"-------------- Calling SAT Solver"`
  - `SATResolver::solverInit()`
  - every job (`Install …`, `Delete …`, `SYSTEM Requires …`, `Locked N installed items…`, `Keep NOT installed name …`, `Weaken dependencies of …`, `Upgrade repo …`, `Relaxed vendor check requested by …`)
  - the flag dump `<resolver> ALLOW_DOWNGRADE = …` before `Starting solving....`
  - `Checking droplists …`
  - `Solverrun finished with an ERROR`
  - `Encountered problems! Here are the solutions:`, followed by all problem rules and each solution
  - flag changes: `DBG <flag>: changed from X to Y | explicitly set/changed default`
  - `setDefaultSolverFlags all=0` after a target change
- **libsolv verbosity**: `LIBSOLV_DEBUGMASK=<int>` (passed to `pool_setdebugmask`). `ZYPP_LIBSOLV_FULLLOG` / `ZYPP_LIBSAT_FULLLOG` set debug level 3. `ZYPP_FULLLOG` sets level 2 and enables auto test cases. libsolv output goes into the zypp log via `logSat` (see `zypp/doc/autoinclude/EnvironmentVariables.doc`).
- **Patch status questions** (needed/satisfied): look for `Establish...` / `Establish DONE` in the log, check `PoolItem::isNeeded/isUnwanted` and the lock state, and check the patch's `restartSuggested()`/category in the metadata. History logging of patch status changes comes from `ResPool::EstablishedStates::changedPseudoInstalled()`.
- **Unexpected vendor/arch/downgrade decisions**: check the zypp.conf (and drop-ins, and the *target's* zypp.conf for `--root`), `/etc/zypp/vendors.d`, the `resolverFlags` section in `zypp-control.yaml`, and whether `dup-vendor-relax(suse)` is provided.
- **Lock-related surprises**: compare `/etc/zypp/locks` before and after. Look for `Locked N installed items and M NOT installed items` and `Apply/Re-apply N HardLockQueries` in the log.
- **purge-kernels**: log group `PurgeKernels` logs the detected running kernel, grouping, keep decisions and each refused removal. Reproduce with `zypp/tools/zypp-runpurge --uname <r> [--keepSpec …] <testcase-dir>`, and add a data set under `zypp/tests/zypp/data/PurgeKernels/`.
- **libsolv vs libzypp?** Run `testsolv testcase.t` from the test case dir. If libsolv alone gives the same transaction/problem, it's libsolv behaviour (or metadata). If not, diff `zypp-control.yaml` semantics vs the `job`/`solverflags` lines in `testcase.t`. `testsolv -r` prints the result in testcase syntax for comparison with `solver.result`.
- **Effective flags**: grep the `libsolv` log group for `allowuninstall=… allowdowngrade=…` and `dontinstallrecommended=… addalreadyrecommended=… onlynamespacerecommended=…`. That is what libsolv actually used (§20.1).
- **Slow solving**: look at `N of M installable solvables considered`, `pkg rule creation took`, `solver_solve took` and `create_solutions for problem #N took` in the `libsolv` log group. Count multiversion jobs and enabled repos (§20.2).
- **Weird problem texts**: find the rule type (`SOLVER_RULE_*`) in §16.2. Untranslated English means the `default:` branch. For a solution that "does nothing", check whether the underlying job is one of the unhandled kinds in §16.3 (`ERR "No valid solution available"` in the log).
- **Patch "needed" disputes**: check the patch's conflicts (`zypper info -t patch`, or the `=Con:` lines in the testtags repo), the installed vendor vs the patch-repo package vendor, and whether the patch is retracted (§19.2).
- **Tests to run after solver changes**: `zypp/tests/zypp/Resolver_test`, `PurgeKernels_test`, `Locks_test`, `Vendor_test`, `ExtendedPool_test`, plus a few real-world test cases through the testcase runner. See [Testing](11-testing.md).
