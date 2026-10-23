# 16. Solver crash course (hands-on)

> **Audience:** a libzypp developer who knows other parts of the code base (e.g. the media backends or the
> NG async code) but has only basic solver knowledge. **Goal:** in about 4–6 weeks part-time, get to the point where you can triage and fix typical
> solver bugs on your own.
>
> This chapter is a **learning path**, not a reference. It adds no new facts about the solver. Instead it points to the
> reference chapters and gives you exercises to work through:
> [00a concepts](00a-concepts-and-glossary.md) · [03 sat pool](03-sat-pool-and-resolvables.md) ·
> [03a special solvables](03a-special-solvables.md) · [04 solver](04-solver.md) ·
> [11 testing §15](11-testing.md#15-bug-reproduction-workflow) · [15 triage](15-bug-triage-guide.md).
>
> Items marked **(unverified)** were not checked against the code or by running them while this chapter was written.
> Check them the first time you use them, then remove the mark.

---

## 0. How to use this course

**Suggested schedule** (about 8–10 h per week):

| Week | Sections | Outcome |
|---|---|---|
| 1 | §1, §2, exercises E1–E4 | You can read and run a testcase and change jobs and flags |
| 2 | E5–E10 | Locks, update vs dup, orphans, focus, cleandeps, recommends |
| 3 | E11–E14, §4 (code reading day) | Multiversion, patches, PTF/retracted, problems and solutions; you know where everything is in `SATResolver.cc` |
| 4 | E15–E18 | You can tell a libzypp-layer bug from a libsolv bug, and you have read three historic fixes |
| 5–6 | §5, E19 on 2–3 real Bugzilla bugs, §6 quiz | You triage on your own and escalate cleanly |

Keep a scratch directory: `mkdir -p /tmp/solver-course/sc && cd /tmp/solver-course/sc`. **Copy** testcases there before editing
them. Never edit the files in `libsolv/test/testcases/` in place.

### The three questions every solver triage must answer

1. **Where is the cause?** There are four possibilities:
   - **libzypp**: wrong jobs or flags are passed to libsolv, the result is mapped back wrongly, or the problem or
     solution text is wrong.
   - **libsolv**: the solver algorithm or rule generation is wrong.
   - **Metadata**: the packager wrote bad dependencies, vendors or patches, or a repo is missing.
   - **Expected behaviour**: the user's mental model is wrong. This is the most common case.
2. **Can I reproduce it from a testcase?** Get `/var/log/zypper.solverTestCase` or `/var/log/updateTestcase-*`
   (see §2.3). If you can't reproduce it, you can't fix it. Ask the reporter for the testcase first.
3. **Targeted fix or escalate?** Fix it yourself if the bug is in the libzypp mapping or job layer. Escalate to libsolv
   upstream if the bug is in the algorithm (§5.3). Close the bug with an explanation if it is metadata or expected
   behaviour, and reassign metadata bugs to the package or maintenance team.

> **Rule of thumb:** if `testsolv` on the testcase's `testcase.t` gives the **same** result as zypper, the
> libzypp layer is probably not at fault. `testcase.t` already contains the jobs and flags that libzypp *generated*.
> If those jobs look right, the remaining question is libsolv vs. metadata vs. expected behaviour. If the jobs in
> `testcase.t` look **wrong** for what the user asked for, the bug is on the libzypp side, in the job creation.

---

## 1. Mental model (one page)

```
 zypper / YaST / PackageKit
      │  set ResStatus on PoolItems (toBeInstalled/Uninstalled, by USER/APPL_*),
      │  locks, Resolver flags, extra request queues (addRequire/addConflict/...)
      ▼
 zypp::Resolver ──► solver::detail::Resolver  (Resolver.cc: modes, upgrade/dup/verify, testcase)
      │
      ▼
 SATResolver (zypp/zypp/solver/detail/SATResolver.cc)
   solverInit():   flags  ─► solver_set_flag(...)
                   PoolItem status ─► _jobQueue   (SOLVER_INSTALL / ERASE / LOCK / UPDATE / DISTUPGRADE ...)
                   locks, system requirements, blacklist (retracted/ptf), multiversion
      │
      ▼
 libsolv: solver_solve(job queue)
   pool + jobs ─► rules (pkg rules, update rules, job rules, choice, best, ...) ─► SAT ─► decisions
      │                                                        │
      │ solved                                                 │ unsolvable
      ▼                                                        ▼
 SATResolver::solving(): walk decisions,             SATResolver::problems():
   SATSolutionToPool(item, status, SOLVER)            solver_problem_count / solutions
   ─► PoolItems get transact-by-SOLVER status         ─► ResolverProblem + ProblemSolution
      │                                                        │  (user picks one)
      ▼                                                        ▼
 commit (Target, ch. 08)                        applySolutions() ─► changes jobs/status ─► resolve again
```

There are two key ideas:

- **Inputs are statuses, not commands.** The UI never says "install X". It sets X's `ResStatus` to *to-be-installed by
  USER*. `solverAddJobsFromPool` turns those statuses into libsolv jobs.
- **Outputs are statuses too.** libsolv's decisions are written back as transactions caused by `SOLVER`. User-set
  statuses are never overwritten. See [03 §7](03-sat-pool-and-resolvables.md) and [04 §3](04-solver.md).

### The functions in `SATResolver.cc` that matter

The function names were verified by grep (the file has about 1700 lines). Line numbers are not stable, so grep for
the names.

| Function | What it does |
|---|---|
| `resolvePool()` | Main entry point for a normal solver run: `solverInit` → add pool jobs → `solving` → `solverEnd` |
| `resolveQueue(...)` | Same as `resolvePool`, but uses an explicit request queue (`SolverQueueItem`s, the old YaST queue API) |
| `doUpdate()` | `zypper up`-style "update all installed" run. It writes the result back differently from `resolvePool` |
| `solverInit(...)` | Creates the libsolv `Solver`, resets `_jobQueue`, adds weak/blacklist jobs (retracted, ptf master), calls the three `solverInitSet*` functions |
| `solverInitSetSystemRequirements()` | Jobs from `/etc/zypp/systemCheck` and requested locales |
| `solverInitSetLocks()` | Turns locked PoolItems into `SOLVER_LOCK` jobs (installed and available) |
| `solverInitSetModeJobsAndFlags()` | Mode jobs (`SOLVER_UPDATE`/`SOLVER_DISTUPGRADE`/`SOLVER_VERIFY`, droporphaned, …) and all `solver_set_flag` calls (allowVendorChange, focus, onlyRequires, …). **The first place to look for "wrong flag" bugs** |
| `solverAddJobsFromPool()` | Turns PoolItem statuses (to-be-installed/uninstalled by user or app) into install and erase jobs |
| `solverAddJobsFromExtraQueues()` | `addRequire`/`addConflict`/upgradeRepos/extra queues → jobs |
| `solving(...)` | Calls `solver_solve`. If there are no problems, it walks the transaction and writes results back with `SATSolutionToPool` (a free function near the top of the file), and collects recommended/suggested/orphaned/unneeded |
| `solverEnd()` | Frees the solver. Jobs are kept so that a testcase can be written |
| `problems()` | Turns libsolv problems and solutions into `ResolverProblem`/`ProblemSolution` objects (the zypper "Problem: … Solution 1: …" text) |
| `SATproblemRuleInfoString()` / `SATgetCompleteProblemInfoStrings()` | Turn problem rules into human-readable text. This is where "wrong or misleading problem message" bugs live |
| `applySolutions()` | Applies the solutions the user picked |
| `autoInstalled()` / `userInstalled()` | Autoinstalled bookkeeping (feeds cleandeps / `zypper packages --unneeded`) |

The surrounding layer: `zypp/zypp/solver/detail/Resolver.cc` (modes, `upgrade()`, `doUpdate()`,
`createSolverTestcase()`), `zypp/zypp/solver/detail/Testcase.cc` (testcase writer),
`zypp/zypp/solver/detail/ProblemSolution*.cc` / `SolutionAction.cc` (how a picked solution changes the pool).
The full map is in [04 §1](04-solver.md#1-file-map).

---

## 2. Tooling setup

### 2.1 Binaries (do not build; use what exists)

Already built (verified) in each preset dir under `zypp-stack/build/`: `Desktop_GCC-Debug`, `Desktop_Clang-Debug`,
`Desktop_GCC-RelWithDebInfo`, `OpenSUSE_Tumbleweed_Dev_Environment_Debug`:

```bash
S=/path/to/zypp-stack
B=$S/build/Desktop_GCC-Debug
alias testsolv=$B/libsolv/tools/testsolv
alias deptestomatic=$B/zypp-solver-testcase-runner/deptestomatic
TC=$S/libsolv/test/testcases
```

Distro alternatives: `testsolv` from the `libsolv-tools` package, and `/usr/lib/zypp/testsuite/bin/deptestomatic` from
`libzypp-testsuite-tools`. These are useful for "old vs new libzypp" diffs ([11 §15.2](11-testing.md)).

### 2.2 CLI reference (verified from the sources)

**`testsolv`** (`libsolv/tools/testsolv.c`, `getopt "vmrhL:l:s:T:W:P"`). `usage()` only prints
`Usage: testsolv <testcase>`, so this is the real list:

| Option | Meaning |
|---|---|
| `testsolv FILE.t ...` | Run each testcase. If the file has a `result ... <inline>` block or a result file, the output is **compared** to it and printed only on mismatch (exit ≠ 0). On success it prints nothing |
| `-r` | **Write the result** instead of comparing. Use this all the time while exploring: `testsolv -r x.t` |
| `-v` (repeatable) | Increase libsolv debug level. `-v -v` shows rule creation and solving, which is a lot of output |
| `-P` | Show a *proof* (why a decision or problem happened). Same as the `proof` result flag |
| `-W PKGSTR` | Show *why* a package is in the result |
| `-l SELECTION` / `-L SELECTION` | List the packages matching a selection (with `-L`: including their deps). Does not solve |
| `-s N` / `-s P:N` | Apply solution N (of problem P, default 1) and re-solve. This is the testsolv equivalent of picking "Solution N" in zypper |
| `-T DIR` | Write the (possibly modified) testcase back out to DIR |
| `-m` | Exercise the result callback (internal; rarely needed) |

`libsolv/test/runtestcases.sh <testsolv> [dir]` runs all `*.t` files and prints Passed/Failed.

**`deptestomatic`** (`zypp-solver-testcase-runner/src/deptestomatic.cc` `main()`):

| Invocation | Meaning |
|---|---|
| `deptestomatic [DIR]` | Load the libzypp testcase in DIR (default `.`) through `zypp::misc::testcase::LoadTestcase`, then run each trial. Result lines are prefixed `>!> ` ([11 §15.2](11-testing.md)) |
| `-v` | libzypp log to stdout |
| `-vv` | Same, with timestamp-free "diffable" log lines (use for old/new diffs) |
| `-h` | List of known trial tags (`install`, `uninstall`, `lock`, `distupgrade`, `update`, `verify`, `allowVendorChange`, `onlyRequires`, `reportproblems`, `takesolution`, `showpool`, `showstatus`, `showselectable`, `whatprovides`, `createTestcase`, …) |
| `deptestomatic 'cycle: --> …'` | Decode a PREREQ cycle line from zypper.log |

Run deptestomatic **as a normal user**. As root it tries to take the global zypp lock and fails with
"Can't aquire ZYpp lock" ([11 §15.2](11-testing.md)).

### 2.3 Getting a testcase from a bug

- **Interactive zypper run:** ask the reporter to repeat the exact command with `zypper --debug-solver <command>`. This
  writes `/var/log/zypper.solverTestCase/` (path hard-coded in `zypper/src/solve-commit.cc`
  `make_solver_test_case`) without committing anything. They should `tar cJf` it and attach it.
- **Upgrade / dup:** libzypp keeps automatic `/var/log/updateTestcase-YYYY-MM-DD-HH-MM-SS/` dirs
  (`TargetImpl.cc` `writeUpgradeTestcase`; the number kept is set by `solver.upgradeTestcasesToKeep` in zypp.conf).
  These are often already present on the reporter's machine, so ask for the one matching the timestamp of the bad
  upgrade.
- **YaST:** `/var/log/YaST2/solverTestcase` (default `dumpPath` in `Testcase.cc`), usually inside `save_y2logs`.
- What to ask for in addition: [11 §15.1](11-testing.md).

### 2.4 Files in a testcase directory (written by `zypp/zypp/solver/detail/Testcase.cc`)

| File | Content | Edit it? |
|---|---|---|
| `testcase.t` | **libsolv-format** testcase: `repo` lines, `system`, `solverflags`, `job` lines as libzypp generated them, `result transaction,problems solver.result` | **Yes.** This is your main tool. Remove jobs, toggle `solverflags`, add `job lock …` |
| `solver.result` | libsolv's result at the time the testcase was written | Compare with it; regenerate with `testsolv -r` |
| `*.repo.gz`, `@System.repo.gz` | Repo contents in libsolv `testtags` format (`#>=Pkg:`, `#>=Req:`, …) | Rarely. Minimizing (deleting packages) is done here |
| `zypp-control.yaml` | libzypp testcase: channels, arch, locales, `resolverFlags` (`allowVendorChange`, `onlyRequires`, `focus`, `dupAllow*`, …), `trials` with jobs (`install`, `uninstall`, `distupgrade`, `verify`, …) | **Yes**, for deptestomatic. This is where you change *libzypp-level* intent |
| `zypp-locks.yaml`, `zypp-<name>.yaml` | Lists that grew to more than 10 entries (locks, autoinst, …) are split out of the control file | As needed |
| `y2log` | libzypp log of the run, if written | Read it |
| `solver-test.xml`, `solver-system.xml.gz` | Old *helix* format (pre-yaml); `LoadTestcase` can still read it (`HelixHelpers.h`) **(unverified for current writers; they only write yaml)** | Old bugs only |

**Two replays, two questions:**
`testsolv testcase.t` → "given these libsolv jobs, what does libsolv do?"
`deptestomatic DIR` → "given this libzypp intent, what jobs does libzypp generate, and what does it show?"

---

## 3. Exercises

Conventions: `$TC` = `libsolv/test/testcases`, and the aliases are the ones from §2.1. Always do
`cp $TC/<dir>/<file>.t /tmp/solver-course/sc/` first.

**Testcase syntax cheat sheet** (full spec: [04 §17](04-solver.md#17-test-case-format-in-depth); keywords
verified from `libsolv/ext/testcase.c`):

```
repo <name> <prio> testtags <inline>        # following "#>=Pkg: N V R ARCH", "#>=Req:", "#>=Prv:", "#>=Con:",
                                            # "#>=Obs:", "#>=Rec:", "#>=Sup:", "#>=Vnd:" lines
system <arch> rpm system                    # arch, disttype, name of the installed repo
solverflags <flag> ...                      # allowvendorchange allowdowngrade allowarchchange ignorerecommended
                                            # dupallowvendorchange keeporphans focusinstalled focusbest ...
poolflags <flag> ...                        # e.g. noobsoletesmultiversion
job <how> <select> [flags]                  # how: install erase update distupgrade verify lock multiversion
                                            #      droporphaned userinstalled allowuninstall favor disfavor
                                            #      blacklist excludefromweak
                                            # select: name X | name X = 2-1 | provides X | pkg N-V-R.A@repo
                                            #         | all packages | repo R
                                            # flags: [cleandeps] [forcebest] [weak] [targeted] [setevr] ...
result transaction,problems[,cleandeps,recommended,orphaned,unneeded,proof] <inline>
nextjob                                     # start a new sub-test in the same file
```

---

### E1 — Reading and running a testcase

- **Goal:** get comfortable with the format and the compare/`-r` workflow.
- **Setup:** `$TC/focus/normal.t`.
- **Steps:**
  1. Read it. Draw the dependency graph on paper: B-1 requires A=1, B-2 requires A=2, C requires B2, …
  2. Run `testsolv normal.t; echo $?`. It prints nothing and returns 0, which means the result matches the inline
     result (verified).
  3. Run `testsolv -r normal.t`. This prints the four transaction lines (verified):
     `install B-2-1`, `install B2-1-1`, `install C-1-1`, `upgrade A-1-1 → A-2-1`.
  4. Change one expected line in the `result` block and run without `-r`. testsolv now prints a diff and returns a
     non-zero exit code.
- **Observation:** the solver picks the *best* versions (B-2, which drags A to 2) but B2-1. That is because A2 is
  installed and the default focus avoids touching it more than needed (see E8).
- **Learned:** testcase anatomy and the compare vs. write modes. See [04 §9, §17](04-solver.md#9-solver-test-cases).

### E2 — Install and remove jobs

- **Goal:** see how jobs are expressed and what selects what.
- **Setup:** your copy of `focus/normal.t`.
- **Steps:**
  1. Replace the jobs with `job install name B = 1-1`. Run `-r`. Note that A stays at 1.
  2. Add `job erase name A`. Run `-r`. You get a problem: B requires A.
  3. Run `testsolv -P` on the same file to see the proof, and try `testsolv -s 1 -r` / `-s 2 -r` to apply the
     solutions.
  4. Try the selections `job install provides A`, `job install pkg A-1-1.noarch@available`.
- **Observation:** an erase job never removes dependents silently. libsolv reports a problem instead. In libzypp,
  `zypper rm` uses `SOLVER_ERASE` *with* dependents removed through solutions or `--clean-deps` **(unverified exact
  zypper default: see `solverAddJobsFromPool` and zypper's remove command)**.
- **Learned:** job vs. selection, and problems and solutions in their rawest form. See
  [04 §15](04-solver.md#15-request-apis-in-detail).

### E3 — Versions and architecture

- **Goal:** understand arch change rules.
- **Setup:** `$TC/distupgrade/dup_noarchchange.t`.
- **Steps:**
  1. Run `-r`, read the jobs and flags, and see which packages keep their arch.
  2. Toggle `solverflags dupallowarchchange` (add or remove it) and compare.
  3. Write a two-package mini testcase: installed `x 1 1 i586`, available `x 2 1 x86_64` with `system x86_64 rpm
     system`, `job update all packages`. Then try again with `solverflags allowarchchange`.
- **Observation:** update and dup have **separate** flag families (`allow*` vs `dupallow*`). libzypp maps
  `solver.dupAllowArchChange` etc. in `solverInitSetModeJobsAndFlags` (see `zypp-control.yaml` `resolverFlags`).
- **Learned:** [04 §4 Solver flags](04-solver.md#4-solver-flags), [00a glossary: arch/noarch].

### E4 — Vendor stickiness and allowVendorChange

- **Goal:** the most common "why doesn't it update?!" cause.
- **Setup:** write `/tmp/solver-course/sc/vendor.t`:
  ```
  repo system 0 testtags <inline>
  #>=Pkg: foo 1 1 x86_64
  #>=Vnd: openSUSE
  repo oss 0 testtags <inline>
  #>=Pkg: foo 2 1 x86_64
  #>=Vnd: obs://build.opensuse.org/home:joe
  system x86_64 rpm system
  job update all packages
  result transaction,problems <inline>
  ```
- **Steps:** run `-r` (expect: no transaction). Add `solverflags allowvendorchange` and run again (expect: upgrade).
  Then try `job install name foo = 2-1`, which is an explicit request: what happens without the flag?
  Also look at `multiversion/update2.t`, which contains vendors.
- **Observation:** update keeps the vendor, but an *explicit* install request may cross vendors. libzypp vendor
  *equivalence* (`/etc/zypp/vendors.d`, `VendorAttr`) is applied through libsolv's vendor check callback or vendor
  classes **(unverified: check how `Testcase.cc` / `testcase_write` encodes VendorAttr, e.g. `vendorclass` lines,
  in a real zypper testcase)**.
- **Learned:** [03 §10 VendorAttr](03-sat-pool-and-resolvables.md#10-locks-poolquery-vendorattr-multiversion),
  [15 §2 symptom catalogue](15-bug-triage-guide.md#2-symptom-catalogue).

### E5 — Locks

- **Goal:** locks are jobs, and they "trump" dup/update.
- **Setup:** `$TC/lock/dup.t`, `$TC/lock/best.t`.
- **Steps:**
  1. `testsolv -r dup.t`: the first sub-test locks `a` by name. dup gives an empty transaction and no problem.
  2. The second sub-test locks only `a = 2-1` (the available one). Read the comment: you still get a problem.
     Explain why. (Hint: dup wants to *reach* a-2, which is locked.)
  3. Read `solverInitSetLocks()` in `SATResolver.cc`. How are libzypp locks (`/etc/zypp/locks`, `zypper al`) turned
     into `SOLVER_LOCK` jobs for installed vs. available items?
- **Learned:** [04 §11 Locks and the solver](04-solver.md#11-locks-and-the-solver),
  [03 §10](03-sat-pool-and-resolvables.md#10-locks-poolquery-vendorattr-multiversion). Historic fix to skim:
  `git -C libzypp show ca4d428cd` (conflict message for locked packages).

### E6 — Update vs dist-upgrade

- **Goal:** know the semantic difference by heart.
- **Setup:** `$TC/distupgrade/dup_orphan1.t`, copied twice.
- **Steps:**
  1. Run it as is (`job distupgrade all packages` / `job distupgrade repo available`).
  2. In the copy, replace the jobs with `job update all packages`. Add an available `c 2` that requires something
     new, and a downgrade-only candidate `b 0.5` while making b-1 unavailable. Compare update vs dup.
  3. Look at `focus/installed.t` and `targeted/targeted_up.t` vs `targeted/targeted_dup.t`.
- **Observation:** update only moves installed packages *forward* within their vendor and arch, and never removes
  them. dup moves the system to "what the repos offer": it may downgrade, change vendor (with the `dupallow*`
  defaults), and replace packages.
- **Learned:** [04 §5 Modes](04-solver.md#5-modes). In libzypp: `Resolver::doUpdate()` → `SATResolver::doUpdate()`
  vs `Resolver::upgrade()` → `resolvePool()` in dup mode (`solverInitSetModeJobsAndFlags`).

### E7 — dup with dropped packages and orphans

- **Goal:** understand orphans. This is the classic "dup removed / did not remove my package" bug.
- **Setup:** `$TC/distupgrade/dup_orphan1.t`, `dup_orphan2.t`, `dup_orphan3.t`, `dup_allowuninstall.t`.
- **Steps:** run each with `-r` and read the inline comments ("dup should leave orphaned a installed", the
  `keeporphans` part). Add `result ...,orphaned` to the result line to *see* the orphan list. Add `job droporphaned
  all packages` and compare.
- **Observation:** an orphan is an installed package that no repo provides anymore. By default dup keeps orphans
  unless they get in the way. libzypp's `removeOrphaned` resolver option (commit `2a67a5f8e`, bsc#1221525) maps to
  droporphaned, and products can drop packages (E18).
- **Learned:** [04 §5](04-solver.md#5-modes), [03a A2 Products](03a-special-solvables.md#a2-products).

### E8 — Focus

- **Goal:** understand `solver.focus` = Job / Installed / Update.
- **Setup:** `$TC/focus/normal.t`, `installed.t`, `best.t`, `new1.t`, `new2.t`.
- **Steps:** diff the files (`diff normal.t installed.t`). The difference is mostly a `solverflags focusinstalled` /
  `focusbest` line. Run each with `-r` and explain each changed result line.
- **Observation:** focus decides whether the solver resolves the *job* with minimal changes (default), prefers to keep
  installed packages (`focusinstalled`), or updates whatever it touches to the best version (`focusbest`).
  libzypp: zypp.conf `solver.focus`, commit `f22aebbf3` (bsc#1146415), zypper `--force-resolution`/`--solver-focus`
  **(unverified zypper option name)**.
- **Learned:** [04 §4](04-solver.md#4-solver-flags).

### E9 — cleandeps and autoinstalled

- **Goal:** understand why `zypper rm --clean-deps` and "unneeded" depend on the autoinstalled list.
- **Setup:** `$TC/cleandeps/cleandeps_in.t`, `cleandeps_up.t`, `cleandeps_dup.t`, `mistake.t`.
- **Steps:**
  1. `cleandeps_in.t`: `job install name A = 2 [cleandeps]`. The result block includes `cleandeps B1` and
     `erase B1` (verified). Remove `[cleandeps]` and see B1 stay.
  2. Read `mistake.t` and work out what "mistake" means there.
  3. Add `job userinstalled pkg B1-1-1.noarch@system` to `cleandeps_in.t` and see B1 survive cleandeps
     **(unverified selector form; check `mistake.t`/testcase.c for the exact `userinstalled` syntax)**.
- **Observation:** cleandeps only removes packages that are *not* user-installed. In libzypp the user-installed set
  comes from the autoinstalled database (`/var/lib/zypp/AutoInstalled`), see `SATResolver::autoInstalled()` /
  `userInstalled()`. A corrupted or empty autoinstalled list explains many "zypper removed half my system" reports.
- **Learned:** [04 §7](04-solver.md#7-who-installed-what-itemcapkind), [00a](00a-concepts-and-glossary.md)
  (autoinstalled), resolver option `removeUnneeded` (commit `c99962479`, bsc#1175678).

### E10 — Recommends and onlyRequires

- **Goal:** weak dependencies.
- **Setup:** `$TC/recommendations/recommended_oldversion.t`, `recommended_multirepo.t`, `recommended_targeted.t`;
  `$TC/favor/recommends.t`; libzypp data `zypp/tests/data/TCNamespaceRecommends/`.
- **Steps:**
  1. Run each with `-r`, then add `solverflags onlynamespacerecommended` and compare. **This, not
     `ignorerecommended`, is what libzypp's `onlyRequires` maps to** (verified, `solverInitSetModeJobsAndFlags`:
     `SOLVER_FLAG_IGNORE_RECOMMENDED` is always `false` and `SOLVER_FLAG_ONLY_NAMESPACE_RECOMMENDED = _onlyRequires`).
     So with `--no-recommends`, language and modalias namespace recommends are still resolved. Try
     `ignorerecommended` as well to see the difference.
  2. Add `,recommended` to the result line to see what *would* be recommended.
  3. Run `deptestomatic $S/libzypp/zypp/tests/data/TCNamespaceRecommends` and read its `zypp-control.yaml`. Toggle
     the `onlyRequires` resolver flag (see the `resolverFlags` keys in §2.4) and compare.
- **Observation:** recommends are only honoured for *newly installed* packages unless
  `addalreadyrecommended` (libzypp: `ignorealreadyrecommended=false`) is set. Namespace recommends (language,
  modalias) behave specially.
- **Learned:** [04 §18](04-solver.md#18-recommends-supplements-namespaces-and-rich-dependencies). Historic fix:
  E17.

### E11 — Multiversion (kernels)

- **Goal:** understand why kernels are installed side by side, and what removes old ones.
- **Setup:** `$TC/multiversion/multiversion.t`, `update.t`, `update2.t`; `$TC/distupgrade/dup_multiversion1.t`–`3.t`.
- **Steps:**
  1. `multiversion.t`: `job multiversion name A` + `job install name A = 2`. Read the four sub-tests, including the
     ones with `keepexplicitobsoletes` and `poolflags noobsoletesmultiversion`, and explain each erase line.
  2. Remove the `job multiversion` line and see A-1 being *upgraded* instead of kept.
  3. Read `dup_multiversion1.t`: what does dup do with an installed multiversion package that is no longer
     available?
- **Observation:** libzypp turns `multiversion = provides:multiversion(kernel)` from zypp.conf into
  `job multiversion` entries. Removing old kernels is **not** done by the solver but by `PurgeKernels`
  ([04 §13](04-solver.md#13-purgekernels-zyppzypppurgekernelshcc)).
- **Learned:** [03 §10](03-sat-pool-and-resolvables.md#10-locks-poolquery-vendorattr-multiversion).

### E12 — Patches and "needed"

- **Goal:** understand that a patch is just a solvable with conflicts, and that "needed" is computed by libzypp.
- **Setup:** `$TC/blacklist/retracted.t` (it uses a package literally named `patch` with `Con: B < 2-1`, verified).
- **Steps:**
  1. Run `-r`. Read the problem lines: installing `patch` conflicts with the installed B-1, and B-2 is blacklisted
     (retracted).
  2. Copy it, remove the blacklist job and `Prv: retracted-patch-package()`, and run again: the patch install now
     upgrades B.
  3. Rename the solvable to `patch:foo` (that is how libzypp names patch solvables, kind prefix) and see that
     libsolv doesn't care about the name.
  4. Read [03a B.1](03a-special-solvables.md#b1-patches-the-metadata) and [04 §8, §19](04-solver.md#8-patches-patterns-and-the-satisfiedbrokenneeded-status).
     Find where libzypp computes *satisfied / broken / needed* (`ResStatus` establish bits, set after solving)
     **(unverified function name; grep `isNeeded\|setBroken\|establish` in `zypp/zypp/solver/detail/` and `ResStatus.h`)**.
- **Observation:** a patch is "needed" when it is relevant (some package it fixes is installed in an older version)
  and not yet satisfied. `zypper lp` / `zypper patch` bugs usually live in that status computation or in metadata, not
  in libsolv.
- **Learned:** [03a B.1](03a-special-solvables.md#b1-patches-the-metadata).

### E13 — PTFs, retracted and blacklist

- **Goal:** the special packages that SUSE support cares about most.
- **Setup:** `$TC/blacklist/ptf.t`, `$TC/blacklist/retracted.t`.
- **Steps:**
  1. `ptf.t` test 1: `job blacklist provides ptf-package()` + `job install name A` where A requires ptf-1. Result:
     problem "requires ptf-1, but none of the providers can be installed" (verified).
  2. Test 2: `job install name ptf-1` is an *unspecific* job, so the problem is "can only be installed by a direct
     request". Test 3: `[setevr]` (specific version) works. Explain the difference.
  3. In `SATResolver::solverInit` find the blacklist jobs for `retractedToken` and `ptfMasterToken`.
     Then do E16.
- **Observation:** blacklisted packages can only be installed by a *specific* request. libzypp blacklists retracted
  packages and PTF *masters* (not the PTF packages themselves; see E16).
- **Learned:** [03a B.2–B.4](03a-special-solvables.md#b2-retracted-packages-and-patches).

### E14 — Reading problems and solutions

- **Goal:** turn libsolv problem output into the zypper text and back.
- **Setup:** `$TC/allowuninstall/conflict.t`, `$TC/blacklist/ptf.t`, any zypper-generated testcase.
- **Steps:**
  1. `testsolv -r conflict.t`, then `-P` for the proof.
  2. Use `-s 1 -r` / `-s 2 -r` to apply each solution and look at the new transaction.
  3. Map libsolv's solution kinds (`deljob`, `allow`, `erase`, `allowuninstall`, …) to zypper's "Solution N:" texts in
     `SATResolver::problems()` and `SATproblemRuleInfoString()`.
  4. With deptestomatic: add the trial steps `reportproblems` and `takesolution` (`problem: 0`, `solution: 0`, see
     the `deptestomatic -h` example) to a `zypp-control.yaml`.
- **Observation:** many "misleading message" bugs need **no** solver change, only a text change in
  `SATproblemRuleInfoString` (historic examples: `33f5992bc`, `dd63a6b1b`, `31b55ebfb`, `7299e4bb9`).
- **Learned:** [04 §6, §16](04-solver.md#16-problems-and-solutions-in-depth).

### E15 — libzypp layer vs libsolv: the two-replay test

- **Goal:** practise the rule of thumb from §0.
- **Setup:** `zypp/tests/data/TCdup/` (`@System.repo`, `update.repo`, `zypp-control.yaml`; used by
  `zypp/tests/zypp/Dup_test.cc`).
- **Steps:**
  1. `deptestomatic $S/libzypp/zypp/tests/data/TCdup` and save the `>!>` lines.
  2. Add the trial step `createTestcase` with a `path` property (e.g. `/tmp/solver-course/sc/tcdup-out`; the default is
     `./solverTestcase`; verified in the handler in `deptestomatic.cc`. The exact yaml spelling of the property is
     **unverified**, so compare with how `LoadTestcase`/`YamlTestcaseHelpers.h` parse trial nodes). This gives you a
     libzypp-generated `testcase.t`.
  3. `testsolv -r /tmp/solver-course/sc/tcdup-out/testcase.t` and compare it with the deptestomatic result.
  4. Read `Dup_test.cc` and work out what it asserts.
  5. Change a `resolverFlags` entry in the yaml (`dupAllowVendorChange: false`), repeat 1–3, and check how the
     `solverflags` line in the new `testcase.t` changed.
- **Observation:** if a yaml flag change does not show up in `testcase.t`, you have found a libzypp mapping bug.
  That is exactly the class of bugs fixed in `688366af8` (dup_allow* options) and `84aa963d9` (missing solver
  flags in testcase).
- **Learned:** [11 §15](11-testing.md#15-bug-reproduction-workflow), [04 §9](04-solver.md#9-solver-test-cases).

### E16 — Historic fix #1: PTF packages were blacklisted (bsc#1186503, `2b32af90a`)

- **Read:** `git -C libzypp show 2b32af90a` (1 line removed in `SATResolver::solverInit`). Also read the companion
  `fff506a4b` (Add `Solvable::isBlacklisted`).
- **The fix:** libzypp pushed `SOLVER_BLACKLIST|SOLVER_SOLVABLE_PROVIDES` for **both** `ptfMasterToken` and
  `ptfPackageToken`. The fix keeps the master blacklisted and stops blacklisting the packages.
- **Explain (write down):** using E13's `ptf.t` semantics, what breaks when every package *inside* a PTF is
  blacklisted? (Hint: the master requires its packages at a specific version. Are those "direct requests"?)
  Reproduce it with a mini `.t`: a master `ptf-1` providing `ptf()`, which requires `foo = 1-1.ptf`, where foo
  provides `ptf-package()`. Blacklist both provides, then only the first.
- **Lesson:** "wrong job generated by libzypp" bugs are 1–3 line fixes in `solverInit*`, and a `.t` file proves them.

### E17 — Historic fix #2: `--no-recommends` ignored in update (bsc#1201972, `dc1a0327b`)

- **Read:** `git -C libzypp show dc1a0327b` (`Resolver.cc`/`Resolver.h`, -16 lines).
- **The fix:** `Resolver` kept *local copies* of `_onlyRequires` and `_cleandepsOnRemove` and pushed them into
  `SATResolver` only in `Resolver::solverInit`. The update path (`doUpdate`) did not go through that, so the flag
  set by `zypper up --no-recommends` never reached libsolv. The fix removes the copies so the setters write directly
  into `SATResolver`.
- **Explain:** why does the bug show up in `zypper up` but not in `zypper in`? Which libsolv flag is involved?
  (`SOLVER_FLAG_ONLY_NAMESPACE_RECOMMENDED`, set from `_onlyRequires` in `solverInitSetModeJobsAndFlags`; see E10.)
- **Lesson:** settings must have **one** owner. When a flag "doesn't work in mode X", trace the setter → field →
  `solver_set_flag` path for *that* mode. E15's yaml-vs-`testcase.t` comparison catches this.

### E18 — Historic fix #3: dup and orphans dropped by products (bsc#1155819, `965a67982`)

- **Read:** `git -C libzypp show 965a67982` (`SATResolver.cc`, +47/-33).
- **Background:** products can declare packages they *drop* (obsolete) ([03a A2](03a-special-solvables.md#a2-products)).
  During dup, orphaned packages that a to-be-installed product drops should be removed.
- **Explain:** where in the dup flow (`solverInitSetModeJobsAndFlags` / `solving`) is this handled? Why can't
  libsolv do it alone? (libsolv knows nothing about the product "drop" semantics. libzypp must turn them into
  erase jobs.)
- **Optional fourth:** `9dcfd89e5` (bsc#1203248, removing a PTF without enabled repos must fail) adds a check in
  `SATResolver` that refuses a job instead of letting libsolv remove the dependent PTF packages.
- **Lesson:** SUSE-specific semantics (products, PTFs, retracted, patches) live in libzypp. Bugs there are yours,
  not libsolv's.

### E19 — Capstone: a real bug, end to end

- **Goal:** apply §5 to a live report.
- **Steps:**
  1. Pick an open libzypp/zypper bug with a solver testcase attached (Bugzilla quicksearch, e.g. product openSUSE
     or SUSE Linux Enterprise, component libzypp, summary/comment containing "solverTestCase" or "updateTestcase").
  2. Run the §5 decision tree. Write the triage comment (what you ran, the result, the conclusion).
  3. Minimize: delete jobs and packages from `testcase.t` until the behaviour disappears, then add back the last
     thing you deleted. Aim for fewer than 20 lines.
  4. If it is a libzypp bug: write the fix, add a regression testcase under `zypp/tests/data/TC<name>` plus a test
     in `zypp/tests/zypp/Resolver_test.cc` or `Dup_test.cc` ([11](11-testing.md)).
  5. If it is a libsolv bug: write the upstream issue (§5.3).
- **Do this 2–3 times** before you consider the course done.

---

## 4. Reading the code (about one day)

Read in this order. Keep [04 §3](04-solver.md#3-a-solver-run-step-by-step-resolvepool) open next to the code. The
paths are verified, but line numbers drift, so grep for the names.

| # | File (under `libzypp/zypp/zypp/`) | Time | What to look for |
|---|---|---|---|
| 1 | `ResStatus.h` | 1 h | The bit fields: `StateField` (installed/uninstalled), `ValidateField` (satisfied/broken/nonrelevant: the patch status of E12), `TransactField` (transact/locked/keep), `TransactByField` (**SOLVER < APPL_LOW < APPL_HIGH < USER**). Rule: a lower causer can't override a higher one. Find `setTransact`, `resetTransact`, `setLock`, `isUnneeded`. Everything the solver reads and writes is here |
| 2 | `Resolver.cc` (public facade) | 15 min | Just delegation to `solver::detail::Resolver`. Note which setters exist (`setOnlyRequires`, `setFocus`, `dupSetAllow*`, `setCleandepsOnRemove`, …) |
| 3 | `solver/detail/Resolver.cc` (~700 lines) | 1.5 h | `resolvePool()`, `resolveQueue()`, `doUpdate()`, `upgrade()`, `verifySystem()`, `solverInit()`, `createSolverTestcase()`. Question to answer: **which state lives here and which in SATResolver?** (After E17 the answer is "settings live in SATResolver".) Find where the upgrade/verify/update modes are set |
| 4 | `solver/detail/SATResolver.cc` top: `SATSolutionToPool`, `dumpOn`, ctor | 30 min | How a libsolv decision becomes `ResStatus` with causer `SOLVER`. The `dumpOn` output is what you see in zypper.log: learn to recognise it |
| 5 | `SATResolver::solverInit*` (three helpers) | 2 h | **Most important.** List every `queue_push` and every `solver_set_flag`, and write down a table "libzypp setting → libsolv job/flag". Compare it with [04 §14](04-solver.md#14-end-to-end-flag-and-job-reference) and fix the handbook if they differ |
| 6 | `solverAddJobsFromPool` / `solverAddJobsFromExtraQueues` | 45 min | Status → job mapping. Note the `MAYBE_CLEANDEPS` macro and how *by-USER* vs *by-APPL* statuses are treated |
| 7 | `solving()` and `solverEnd()` | 1 h | The write-back loop, and how recommended/suggested/orphaned/unneeded lists are collected (they feed `zypper` output) |
| 8 | `problems()`, `SATproblemRuleInfoString`, `applySolutions` + `ProblemSolution*.cc`, `SolutionAction.cc` | 1.5 h | How solutions become `SolutionAction`s that change `ResStatus` or jobs, then trigger a re-solve |
| 9 | `sat/detail/PoolImpl.cc` | 45 min | How repos, the installed repo, arch policy, multiversion, retracted/ptf tokens and vendor checks are set up on the libsolv pool *before* the solver runs. Bugs here affect both solving and `zypper search/info` |
| 10 | `solver/detail/Testcase.cc` | 30 min | What is (and isn't!) written into a testcase. If a setting isn't written, testcases can't reproduce bugs that depend on it (historic: `84aa963d9`, `ab8aa1d0e`) |

**Deliverable:** a one-page "flag/job table" from step 5, plus three questions for which you found no answer. Use them
to improve [04](04-solver.md).

---

## 5. Triage playbook for solver bugs

### 5.1 Decision tree

```
 Report: "zypper did X, I expected Y"
   │
   ├─ No testcase? ──► request `zypper --debug-solver <same cmd>` tarball or /var/log/updateTestcase-*,
   │                   plus zypper.log & `zypper lr -d`  ([11 §15.1])            ──► WAIT
   │
   ▼
 Replay: deptestomatic DIR  ──► same result as reporter?
   │ no ──► environment difference: missing repo in testcase? locks/autoinstalled split files? zypp.conf
   │        settings not captured in testcase (multiversion, vendors.d)? ──► ask / fix Testcase writer
   ▼ yes
 Read testcase.t jobs + solverflags.  Do they express what the user asked for?
   │ no ──► LIBZYPP job/flag mapping bug (solverInit*, solverAddJobsFromPool, Resolver setters)
   │        ──► fix + regression TC (E16/E17 pattern)
   ▼ yes
 testsolv testcase.t  ──► same result?
   │ no ──► libzypp-layer difference (write-back, PoolImpl setup, namespace callbacks, vendor equivalence)
   │        ──► compare deptestomatic -vv logs; usually a libzypp bug
   ▼ yes   (libzypp layer most likely OK)
 Minimize testcase.t. Is the result logically forced by the metadata?
   │ yes ──► EXPECTED or METADATA: explain with `testsolv -P` / -W proof;
   │         bad deps/vendor/patch ──► reassign to package/maintenance team
   ▼ no (solver picks a valid but clearly worse solution, or claims unsolvable when it isn't)
 Try flags (focus, allowvendorchange, forcebest) to see whether it is policy, not a bug
   │ policy ──► EXPECTED (document; maybe zypp.conf default discussion)
   ▼ still wrong
 LIBSOLV bug ──► escalate (5.3)
```

### 5.2 Typical bug classes and the first thing to check

| Symptom | First thing to check | Exercise / reference |
|---|---|---|
| "Package not updated" / "update keeps old version" | Vendor (`zypper se -s --details`, `#>=Vnd:` in testcase), locks, `allowvendorchange`, arch | E4, E5, E3 |
| "dup removed my package" | Orphan? dropped by product? `removeOrphaned`? a conflict with a new package? | E7, E18 |
| "dup does not remove obsolete package" | Orphans are kept by default; product drop list | E7 |
| "zypper rm removed too much / --clean-deps" | Autoinstalled list (`/var/lib/zypp/AutoInstalled`), `userinstalled` in testcase | E9 |
| "Unwanted recommended packages" / "--no-recommends ignored" | `onlyRequires` → `onlynamespacerecommended`, `ignorealreadyrecommended`, mode (update path!) | E10, E17 |
| "Old kernels not removed / too many kernels" | Multiversion config; `PurgeKernels`, not the solver | E11, [04 §13](04-solver.md#13-purgekernels-zyppzypppurgekernelshcc) |
| "Patch shown as needed but nothing to install" / vice versa | Patch conflicts in metadata; ResStatus validate bits; retracted | E12, [03a B.1](03a-special-solvables.md#b1-patches-the-metadata) |
| "Can't remove PTF" / "PTF conflicts" | Blacklist jobs; `removeptf`; hint texts | E13, E16, [03a B.3](03a-special-solvables.md#b3-ptfs-program-temporary-fixes) |
| "Retracted package installed / offered" | Blacklist job present? specific request (then allowed by design) | E13 |
| "Problem message is confusing" | `SATproblemRuleInfoString` text only | E14 |
| "Solution N does something else than it says" | `problems()` → `SolutionAction` mapping, `applySolutions` | E14, §4 step 8 |
| "Different result in YaST vs zypper" | Different resolver flags/modes/causers (APPL vs USER); compare both testcases | §4 step 3 |
| "Solver takes forever / crashes" | Huge job queue? namespace callbacks? segfault without system ( `df2b31c31`) | [04 §20](04-solver.md#20-performance-and-diagnostics) |

More symptoms: [15 §2](15-bug-triage-guide.md#2-symptom-catalogue).

### 5.3 Escalating to libsolv upstream

- **Where:** <https://github.com/openSUSE/libsolv/issues>. For SUSE products, also link the Bugzilla bug and keep the
  bug assigned to you until upstream responds ([15 §5 escalation map](15-bug-triage-guide.md#5-escalation-map)).
- **What to attach:** a **minimal `testsolv` testcase** (`.t`, ideally fewer than 30 lines, inline `testtags` repos,
  no zypp yaml), containing:
  - the `job` and `solverflags` lines exactly as libzypp generated them, after minimizing,
  - a `result transaction,problems <inline>` block holding what testsolv **does** produce (`testsolv -r`), and in the
    issue text, what you **expected** and why,
  - the libsolv version or commit (`git -C libsolv describe`) and whether the zypp-stack `libsolv.patch` was applied.
- **Before you file:** confirm the behaviour with the *distro* `testsolv` (unpatched libsolv) too. If only the
  stack-patched build shows it, the bug is in `libsolv.patch`.
- **Minimizing tip:** `testsolv -T outdir x.t` writes the testcase back out, which is handy after `-s` or when
  starting from a big zypper testcase. Delete repos and packages in chunks and re-run until the behaviour vanishes.

---

## 6. Self-check quiz

1. Which libzypp file and function would you look at first if `zypper dup --allow-vendor-change` seems to have no effect?
2. What does `testsolv` print when a testcase *passes*? How do you see the actual result?
3. A user's `testcase.t` contains `job install name foo`, the user ran `zypper in foo`, and `testsolv` gives the same
   (bad) result as zypper. Which layer do you suspect, and which layer do you probably clear?
4. What is the difference between `job update all packages` and `job distupgrade all packages` regarding vendor
   changes and removals?
5. What is an orphan, and what does dup do with it by default?
6. Which libsolv flag does libzypp's `onlyRequires` (`--no-recommends`) set?
7. Why can a retracted package still be installed with `zypper in foo-1.2-3`?
8. Which PTF token does libzypp blacklist, and why not the other one?
9. Name the four `TransactByField` causers in order. Can the solver override a USER choice?
10. Where do the user-installed vs auto-installed sets come from, and which feature breaks if they are wrong?
11. Who removes old kernels, the solver or something else?
12. Which zypper option produces a solver testcase without committing, and where is it written?
13. Which file in a zypper testcase do you edit to change *libzypp* intent, and which to change *libsolv* jobs?
14. What do you attach to a libsolv upstream issue?
15. A problem text is misleading but the solutions are correct. Where is the fix, and do you need libsolv changes?

<details>
<summary>Answers</summary>

1. `SATResolver::solverInitSetModeJobsAndFlags()` (the `solver_set_flag(... DUP_ALLOW_VENDORCHANGE ...)` line) and the
   setter path in `Resolver.cc`; also check the `solverflags` line in the generated `testcase.t` (E15).
2. Nothing (exit 0). Use `testsolv -r`.
3. The job is what the user asked for, and testsolv agrees with zypper, so the libzypp layer is probably fine. Suspect
   metadata, expected behaviour, or (rarely) libsolv.
4. update never changes vendor (unless `allowvendorchange`) and never removes packages. dup follows the `dupallow*`
   flags (vendor change allowed by default in libzypp **(unverified default; check `ZConfig` `solver.dupAllowVendorChange`)**)
   and may replace or remove packages.
5. An installed package that no repo provides. dup keeps it unless it conflicts, or unless droporphaned
   (`removeOrphaned`) is set.
6. `SOLVER_FLAG_ONLY_NAMESPACE_RECOMMENDED` (`IGNORE_RECOMMENDED` stays false so namespace recommends still work).
7. Blacklisting only blocks *unspecific* selection. A specific (version-qualified) request is allowed (ptf.t test 3).
8. `ptfMasterToken` is blacklisted. `ptfPackageToken` is not, because the master pulls its packages in through
   versioned requires, and blacklisting them broke PTF installation (bsc#1186503, `2b32af90a`).
9. SOLVER < APPL_LOW < APPL_HIGH < USER. No: a lower causer can't override a higher one.
10. `/var/lib/zypp/AutoInstalled` (`SATResolver::autoInstalled/userInstalled`). cleandeps and "unneeded" break.
11. `PurgeKernels` (`zypp/zypp/PurgeKernels.cc`, run by `zypper purge-kernels`), not the solver.
12. `zypper --debug-solver …` writes `/var/log/zypper.solverTestCase`.
13. `zypp-control.yaml` (deptestomatic) for libzypp intent, and `testcase.t` (testsolv) for libsolv jobs.
14. A minimal inline `.t`, the actual vs. expected result, and the libsolv version (plus the patched/unpatched check).
15. `SATproblemRuleInfoString()` / `problems()` in `SATResolver.cc`, which is text only. No libsolv change is needed.

</details>

---

## 7. Progress checklist

- [ ] §0 Can state the three triage questions and the testsolv rule of thumb from memory
- [ ] §1 Can draw the pipeline diagram from memory and name the job of each `SATResolver` function
- [ ] §2 Ran `testsolv` and `deptestomatic` from the stack build; know `-r`, `-P`, `-s`, `-T`, `-vv`
- [ ] §2 Know the files in a zypper testcase dir and which to edit for which question
- [ ] E1 reading · E2 install/erase · E3 arch · E4 vendor
- [ ] E5 locks · E6 update vs dup · E7 orphans · E8 focus
- [ ] E9 cleandeps/autoinstalled · E10 recommends/onlyRequires · E11 multiversion
- [ ] E12 patches/needed · E13 PTF/retracted/blacklist · E14 problems/solutions
- [ ] E15 two-replay test on `TCdup`
- [ ] E16, E17, E18 historic fixes explained in writing
- [ ] §4 code reading done; flag/job table written and compared to [04 §14](04-solver.md#14-end-to-end-flag-and-job-reference)
- [ ] §5 decision tree applied to ≥ 2 real bugs (E19); one minimized testcase produced
- [ ] §6 quiz: ≥ 13/15 without looking
- [ ] Removed or confirmed every **(unverified)** mark in this chapter
- [ ] Fed corrections back into [04](04-solver.md), [03a](03a-special-solvables.md), [15](15-bug-triage-guide.md)
