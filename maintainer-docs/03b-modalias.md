# 03b – Modalias: Hardware-Dependent Packages

This chapter explains how a package can say "install me if the machine has this
hardware": drivers, firmware, KMPs, CPU-vendor or CPU-feature specific packages,
and laptop/platform support. It follows a `modalias(...)` dependency from the
spec file through libsolv's metadata rewriting to libzypp's matching against
the kernel's modalias strings. It also covers what the mechanism **cannot** do
(hard requirements), with a spec-file cookbook and the known pitfalls.

> **How the facts were checked (Oct 2026).** The libsolv side (metadata
> rewriting and solver behaviour with libzypp's flag combinations) was verified
> by running the stack's libsolv tools (`rpmmd2solv`, `dumpsolv`, `testsolv`) on
> crafted test repos. Glob matching was checked against this machine's real CPU
> alias. The libzypp side (`nsCallback`, hex decoding, `fnmatch`) comes from
> reading the code; an end-to-end run through libzypp was **not** done. Real
> package examples come from the openSUSE Tumbleweed `repo-oss` solv cache.

---

## 1. What a modalias is

The kernel exposes a **modalias** string for many devices, in a file called
`modalias` somewhere under `/sys`. It describes the device in a fixed,
bus-specific format. Kernel modules declare which modaliases they handle (that
is how udev autoloads drivers), and packages can declare the same patterns to
get installed automatically.

| Bus / type | Example alias | Meaning |
|---|---|---|
| PCI | `pci:v00008086d0000265Asv00008086sd00004556bc0Csc03i00` | vendor, device, subvendor, subdevice, class, subclass, interface |
| USB | `usb:v046Dp0825d0010dcEFdsc02dp01ic0Eisc01ip00in00` | vendor, product, device release, classes |
| Device tree | `of:NfirmwareT(null)Craspberrypi,bcm2835-firmware` | node name, type, compatible |
| ACPI / DMI | `acpi:…`, `dmi:bvn…:bvr…:svn…:pn…:` | firmware IDs, board/system vendor and product |
| Virtual / platform | `virtio:d00000003v00000000`, `platform:rtc_cmos` | virtual or platform devices |
| **CPU (x86)** | `cpu:type:x86,ven0002fam0019mod0075:feature:,0000,0001,…,0125,…` | vendor, family, model, then CPU feature numbers |

### The x86 CPU alias in detail

There is exactly one CPU alias, `/sys/devices/system/cpu/modalias`, printed by
the kernel (`drivers/base/cpu.c` `print_cpu_modalias`, format
`CPU_FEATURE_TYPEFMT` in `arch/x86/include/asm/cpufeature.h`):

```
cpu:type:x86,ven%04Xfam%04Xmod%04X:feature:,%04X,%04X,…
```

* **Vendor** (`arch/x86/include/asm/processor.h`): `0000` Intel, `0002` AMD,
  `0009` Hygon, `0005` Centaur, `000A` Zhaoxin, …
* **Family / model**: uppercase hex, e.g. `fam0019` = AMD family 25 (Zen 3/4).
* **Features**: an ascending, comma-separated list of the kernel's
  `X86_FEATURE_*` numbers (word × 32 + bit). Examples: `003D` = long mode
  (64-bit capable), `0125` = AVX2, `0130` = AVX512F. Only the boot CPU is
  described.
* Older kernels used a different format, `x86cpu:vendor:XXXX:family:…`. Packages
  that still support old kernels carry both patterns (see `ucode-*` in §6).

### How to see the aliases libzypp would use

There is no zypper command or libzypp tool for this. Use the same scan libzypp
does (see the comment in `zypp/zypp/target/modalias/Modalias.cc`):

```bash
find /sys -type f -name modalias -print0 | xargs -0 cat | sort -u
```

---

## 2. Use cases (what real packages do)

In the openSUSE Tumbleweed `repo-oss` metadata (Oct 2026) there were about
**7,500** modalias supplements:

| Alias type | Count | Typical packages |
|---|---|---|
| `pci` | ~4,170 | firmware (`kernel-firmware-*`, `adaptec-firmware`), graphics/network KMPs |
| `usb` | ~2,020 | wifi/bluetooth firmware, USB device drivers |
| `of` (device tree) | ~730 | ARM/embedded firmware (`kernel-firmware-ath10k`, Raspberry Pi firmware) |
| `pcmcia`, `sdio`, `ssb`, `bcma` | ~450 | legacy and embedded bus drivers/firmware (`b43-fwcutter`) |
| `acpi`, `dmi` | ~50 | laptop/handheld platform KMPs (`framework-laptop-kmp-default`, `ayaneo-platform-kmp-default`), `biosdevname` |
| `hid`, `gip`, `hdaudio`, `i2c`, `mhi`, … | small | special device drivers, sound/media/modem firmware |
| `cpu` | a few | **CPU vendor**: `ucode-amd`, `ucode-intel` (microcode). **CPU features**: `patterns-glibc-hwcaps-x86_64_v3` (optimised glibc) |

The main use cases are:

1. **Drivers and KMPs:** install the kernel module package for detected
   hardware.
2. **Firmware:** install firmware blobs only on machines that have the device.
3. **Platform support:** laptop- or board-specific packages via DMI, ACPI or
   device-tree aliases.
4. **CPU vendor:** AMD vs Intel specific packages, such as microcode.
5. **CPU features / microarchitecture:** packages optimised for a CPU feature
   level (`x86-64-v3` glibc hwcaps).

All of them are **soft**: the package is *recommended* when the hardware
matches. See §5.

---

## 3. The pipeline

```
 spec file                Supplements: modalias(cpu:type%%3Ax86*ven0002*)
     │ rpmbuild           (%% → %)
     ▼
 rpm header / metadata    supplements: modalias(cpu:type%3Ax86*ven0002*)
     │ repo2solv / rpmdb2solv  (libsolv parsers call repo_rewrite_suse_deps())
     ▼
 solv cache               namespace:modalias(cpu:type%3Ax86*ven0002*)     ← REL_NAMESPACE dep
     │ solve: libsolv asks the namespace callback once per value (cached)
     ▼
 libzypp                  sat::detail::PoolImpl::nsCallback()             (zypp/zypp/sat/detail/PoolImpl.cc)
                            → target::Modalias::instance().query( str::hexdecode(value) )
                            → fnmatch( "cpu:type:x86*ven0002*", <each system alias>, 0 )
     │ 1 = "system provides it", 0 = not
     ▼
 libsolv                  supplements satisfied → package becomes a recommendation candidate
```

### 3.1 Metadata time: libsolv rewrites the dependency

rpm only knows plain names, so a supplement `modalias(…)` arrives as an ordinary
string. When libsolv parses metadata it rewrites it into a namespace
dependency. This is done by `repo_rewrite_suse_deps()` → `repo_fix_supplements()`
→ `fix_namespace_dep()` in `libsolv/src/suse.c`. It is called by all the
relevant parsers: `ext/repo_rpmmd.c` (rpm-md repos), `ext/repo_rpmdb.c`
(installed system), `ext/repo_susetags.c`, `ext/repo_content.c`,
`ext/repo_helix.c` and `ext/repo_testcase.c`.

Rules (from `fix_namespace_dep_norel`, verified with `rpmmd2solv` + `dumpsolv`):

| Dependency in the package | Result in the solv data |
|---|---|
| `Supplements: modalias(ALIAS)` | `namespace:modalias(ALIAS)` |
| `Supplements: modalias(PKG:ALIAS)` (see the colon rule below) | `PKG and namespace:modalias(ALIAS)` |
| `Supplements: (modalias(ALIAS) and kernel)` (rich dep with and/or/if/unless/else) | `(namespace:modalias(ALIAS) and kernel)`, **only with libsolv ≥ 0.7.36** (commit `7861bad4`, bsc#1258193) at the time the cache is built |
| `Requires:` / `Recommends:` / `Conflicts:` `modalias(…)` | **not rewritten**. It stays a plain name nobody provides. A `Requires:` makes the package uninstallable ("nothing provides modalias(…)"), verified with `testsolv` |

**The colon rule.** libsolv treats the text before the first `:` as a package
name **only if the value contains a second literal `:`**. So:

* `modalias(foo:pci:v00001234…)` → package prefix `foo`, alias `pci:v00001234…`.
* `modalias(cpu:type:x86*ven0002*)` → prefix `cpu` (!), which is a package
  nobody has, so the supplement can never match.
* `modalias(cpu:type%3Ax86*ven0002*)` → only one literal colon, no prefix. This
  is why real packages hex-encode the colons inside CPU aliases.

### 3.2 Solve time: libzypp answers the namespace question

`PoolImpl::nsCallback` handles three namespaces (language, modalias,
filesystem; see [03 §9](03-sat-pool-and-resolvables.md)). For
`NAMESPACE_MODALIAS` it calls
`target::Modalias::instance().query( str::hexdecode(value) )`:

* **Hex decoding** (`str::hexdecode`, `zypp-logic/zypp-core/base/String.cc`):
  every valid `%XX` becomes the byte `0xXX`. Anything else, including a `%`
  without two hex digits, is copied verbatim.
* **Matching** (`Modalias::Impl::query`, `zypp/zypp/target/modalias/Modalias.cc`):
  `fnmatch(pattern, alias, 0)` against every alias in the list. Any match
  returns true. Flags 0 means:
  * case-sensitive;
  * `*` also matches `/` and `:`;
  * `?`, `[…]` and backslash escapes work;
  * the pattern must match the **whole** alias, so a trailing `*` is almost
    always needed.
* libsolv caches the answer per value in its whatprovides data. libzypp does
  not invalidate it for modalias (there is no tracker like the one for locales).

### 3.3 Where the alias list comes from

`target::Modalias` is a process-wide singleton (`Modalias::instance()`). Its
data (`Impl::nullimpl()`) is built **once**, on first use:

| Situation | Source |
|---|---|
| default | Recursive scan of `/sys` for regular files named `modalias`. The **first line** of each is used, and the result is de-duplicated (`std::set`). `/sys/devices/system/memory` is skipped (bnc#824110: huge tree on large-RAM systems). |
| `ZYPP_MODALIAS_SYSFS=<file>` | One alias per line, used as is (not de-duplicated). The `/sys` scan is skipped completely. |
| `ZYPP_MODALIAS_SYSFS=<dir>` | That directory is scanned instead of `/sys`. |
| API | `Modalias::instance().modaliasList( list )` replaces the list. Testcase loading uses this. |

There is no root parameter: with `--root`/`--installroot`, in image builds and
in containers, **the host's `/sys` is used**.

---

## 4. What the mechanism cannot do

* **There is no hard hardware requirement.** Only `Supplements` (and legacy
  `Freshens`) are turned into namespace dependencies. A package cannot say "only
  installable on AMD" or "requires AVX2" through modalias.
* **There is no negation.** `fnmatch` has no "not". "Not Intel" is not the same
  as AMD (Hygon, Zhaoxin, …), so write one positive pattern per target.
* **Hard alternatives that do exist** (see [04](04-solver.md) for details):

| Need | Mechanism | Strength |
|---|---|---|
| CPU architecture (x86_64 vs aarch64) | rpm arch / `ExclusiveArch` | hard |
| x86-64 feature level | rpm archs `x86_64_v2/v3/v4`. libzypp detects the level via CPUID (`zypp-logic/zypp/Arch.cc` `get_x86_64_level`). | hard |
| Machine-specific policy | `/etc/zypp/systemCheck.d/*.check` (`requires:` / `conflicts:`), written by an admin or provisioning tool | hard, per machine |
| Behaviour inside one binary | runtime dispatch (ifunc, glibc-hwcaps, function multiversioning) | not a packaging matter |

---

## 5. Solver behaviour

A satisfied modalias supplement makes the package a **recommendation
candidate**. Whether it actually gets installed depends on the solver mode.
Verified with `testsolv` using libzypp's flag combinations (libzypp always sets
`IGNORE_RECOMMENDED=false` and maps `onlyRequires` to
`ONLY_NAMESPACE_RECOMMENDED`, `SATResolver.cc` ~l.660–669):

| Situation (machine = AMD) | Result |
|---|---|
| A solve without any request and without "add already recommended" | nothing new is installed |
| "Add already recommended" (`setIgnoreAlreadyRecommended(false)`, i.e. `zypper inr`) | all uninstalled packages whose supplements match are installed (`ucode-amd`, `foo-amd` for an installed `foo`) |
| Installing `bar` (job) | `bar-amd` with `modalias(bar:…ven0002*)` is pulled in as well; `bar-intel` is not |
| Installing `bar` with `onlyRequires` / `--no-recommends` | `bar-amd` is **still** pulled in: namespace supplements are honoured |
| Explicit install of a non-matching variant (`bar-intel`) | allowed; modalias never blocks |
| Package with `Requires: modalias(…)` | uninstallable: "nothing provides modalias(…)" |

zypper usage:

* `zypper inr` installs new recommendations, including hardware matches.
* `zypper inr --no-recommends` restricts that to hardware, language and
  filesystem packages (zypper's own help text, `zypper/src/commands/inrverify.cc`).
  This is the documented way to pick up support for **new hardware**.
* Which other commands run in "add already recommended" mode (e.g. `dup`) is in
  [04 §18.1](04-solver.md).

---

## 6. Spec-file cookbook

Rules first:

* In a spec file, write `%%` to get a literal `%` (rpm macro expansion may otherwise touch `%2…`/`%3…`). The official example in `Modalias.doc` uses a single `%`; which spellings rpm leaves alone was not verified. Always check the built rpm.
* Encode `,` as `%2C`, because rpm uses commas to separate dependencies. Encode
  spaces as `%20`.
* Encode **colons inside the alias** as `%3A` whenever a literal second colon
  would trigger the package-prefix rule (§3.1). This always applies to CPU
  aliases.
* Hex digits in aliases are uppercase (`mod00AF`, `0125`). Matching is
  case-sensitive.
* End patterns with `*` unless you mean the complete alias.
* Check the result: `rpm -qp --supplements foo.rpm` must show the `%XX` form,
  with no `%%` and no `%25`.

### Driver / firmware for a PCI device

```spec
# any device with vendor 0x1002, device 0x1304
Supplements:    modalias(pci:v00001002d00001304sv*sd*bc*sc*i*)
```

### Device-tree firmware (from `zypp/doc/autoinclude/Modalias.doc`)

```spec
#                     of:NfirmwareT*Craspberrypi,bcm2835-firmwareC*
Supplements:    modalias(of:NfirmwareT*Craspberrypi%%2Cbcm2835-firmwareC*)
```

### Only together with another package (package prefix)

```spec
# recommended only if "foo" is installed or being installed, on AMD CPUs
Supplements:    modalias(foo:cpu:type%%3Ax86*ven0002*)
```

The prefix must be a package that gets installed anyway (`foo`, `kernel-default`,
`glibc`). Using the package's own name never triggers.

### CPU vendor: AMD vs Intel (as the real `ucode-*` packages do it)

```spec
# AMD
Supplements:    modalias(cpu:type%%3Ax86*ven0002*)
# Intel, additionally requiring a kernel (rich dep: needs libsolv >= 0.7.36)
Supplements:    (modalias(cpu:type%%3Ax86*ven0000*) and kernel)
```

Real metadata (Tumbleweed, Oct 2026):

| Package | Supplements |
|---|---|
| `ucode-amd` | `modalias(cpu:type%3Ax86*ven0002*)`, `modalias(x86cpu:vendor%3A0002%3Afamily%3A*%3Amodel%3A*%3Afeature%3A*)` (old kernel format) |
| `ucode-intel` | `(modalias(cpu:type%3Ax86*ven0000*) and kernel)`, `(modalias(x86cpu:vendor%3A0000…) and kernel)` |

Variant subpackages of one package:

```spec
%package amd
Requires:       foo = %{version}
Conflicts:      foo-intel
Supplements:    modalias(foo:cpu:type%%3Ax86*ven0002*)

%package intel
Requires:       foo = %{version}
Conflicts:      foo-amd
Supplements:    modalias(foo:cpu:type%%3Ax86*ven0000*)
```

### CPU family or features

```spec
# AMD family 0x19 (Zen 3/4)
Supplements:    modalias(cpu:type%%3Ax86%%2Cven0002fam0019*)
# any x86 CPU with AVX2 (0125); the feature list is ascending, so several
# features can be chained with '*'
Supplements:    modalias(cpu:type%%3Ax86*%%3Afeature%%3A*0125*)
```

The real `patterns-glibc-hwcaps-x86_64_v3` package uses
`modalias(glibc:cpu:type%3Ax86*%3Afeature%3A*0080*0089*…*0125*0128*)`, i.e. the
list of features that make up x86-64-v3, plus a `glibc` prefix.

---

## 7. API

| API | Notes |
|---|---|
| `target::Modalias` (`zypp/zypp/target/modalias/Modalias.h`) | `instance()`, `query(pattern)`, `modaliasList()`, `modaliasList(newList)`. Marked `ZYPP_API_DEPTESTOMATIC`: exported for the testcase runner, not meant as general application API. |
| `Capability( ResolverNamespace::modalias, "value" )` | Builds a `namespace:modalias(value)` capability, e.g. for tests or queries (example in the `dumpCap` doc comment in `zypp/zypp/Capability.h`). |
| `ResolverNamespace` / `ResolverNamespaces` (`zypp-logic/zypp/ResolverNamespace.h`) | `language`, `modalias`, `filesystem` flags |
| NG: `ModaliasNamespaceProvider` (`zyppng/lib/zypp/ng/sat/namespaces/modalias.cc`) | Takes an injected query function; does not scan `/sys` yet (`@TODO`). NG development is paused ([10](10-zyppng-next-generation.md)). |

---

## 8. Inspecting and testing

| Question | How |
|---|---|
| Which aliases does this machine have? | `find /sys -type f -name modalias -print0 \| xargs -0 cat \| sort -u` |
| What does a built package declare? | `rpm -qp --supplements foo.rpm` (installed: `rpm -q --supplements foo`) |
| Was the dependency rewritten in the cache? | `dumpsolv /var/cache/zypp/solv/<alias>/solv \| grep -A3 'name: foo'`. Expect `namespace:modalias(…)`. A plain `modalias(…)` is dead (§9). |
| Pretend to be other hardware | Write aliases to a file (edit e.g. the CPU line to `ven0000`) and run with `ZYPP_MODALIAS_SYSFS=/path/to/file` |
| Reproduce a reporter's hardware | Solver testcases (`zypper --debug-solver`) record the alias list in `zypp-control.yaml` (`modalias:` list, or a separate `zypp-modalias.yaml` with more than 10 entries; written by `Testcase.cc`). A testcase can also name a `hardwareInfo:` file, which sets `ZYPP_MODALIAS_SYSFS` (`misc/TestcaseSetup.cc`). |

> **Side note: libsolv's `testcase.t`.** Lines such as
> `namespace namespace:modalias(cpu:type%3Ax86*ven0002*) @SYSTEM` are **not**
> hardware aliases. They are the supplement patterns that evaluated to true when
> the testcase was written (`testcase_write` dumps every satisfied namespace
> dependency). Replaying the libsolv testcase uses these recorded answers and
> never consults hardware.

---

## 9. Pitfalls & special cases

* **Soft only.** Modalias never blocks or enforces anything. Explicit installs
  of non-matching packages succeed, and moving a disk to other hardware swaps
  nothing.
* **Host hardware decides.** `--root`, image builds (kiwi/OBS) and containers
  match against the **host's** `/sys`. Without `/sys` (some chroots), nothing
  matches.
* **Read once per process.** Hot-plugged hardware is not seen until the next
  process (a long-running PackageKit daemon included).
* **`ZYPP_MODALIAS_SYSFS` replaces, it doesn't add.** A file set this way is the
  complete list.
* **Colon rule.** An unencoded second colon makes libsolv take the first word as
  a package name. `modalias(cpu:type:x86*…)` therefore never matches. Encode it
  as `%3A`.
* **Only Supplements work.** `Requires/Recommends/Conflicts: modalias(…)` are not
  rewritten. A Requires makes the package uninstallable.
* **Stale caches for rich deps.** `(modalias(…) and …)` is only rewritten by
  libsolv ≥ 0.7.36. libsolv's `LIBSOLV_TOOLVERSION` stayed `"1.2"` (unchanged
  since 2022), and libzypp's `RepoStatus` magic was not bumped either. So solv
  caches built by an older libsolv keep the dead plain `modalias(…)` form until
  the repo metadata changes or `zypper refresh -f` runs. This was observed on a
  real cache (`ucode-intel`).
* **Case-sensitive, whole-string `fnmatch`.** `mod00af` does not match `mod00AF`,
  and a missing trailing `*` usually means no match.
* **CPU alias specifics.** Only the boot CPU is described. Hypervisors may
  report a different vendor than the host. Old kernels use the `x86cpu:` format.
  Vendor is a weak proxy for "optimised for"; feature numbers are more precise.
* **`zypper up` doesn't pick up new hardware.** New hardware support needs
  `zypper inr` (or `inr --no-recommends`), or arrives with a `dup` / package
  install in "add already recommended" mode.
* **NG doesn't scan `/sys`.** The NG namespace provider only uses an injected
  query function.

## 10. Where to start when debugging

1. **"Package X was (not) installed automatically":**
   * check the package's supplements (`rpm -q --supplements`, or `dumpsolv` on
     the solv cache);
   * check the machine's aliases (the `find` one-liner);
   * test the decoded pattern against the aliases (`fnmatch` semantics, e.g. a
     two-line Python check with `fnmatch.fnmatchcase`);
   * check the solver mode (§5).
2. **The dependency shows as plain `modalias(…)` in the cache:** this is the
   colon rule or a stale cache. Try `zypper refresh -f` and check again.
3. **Reproduce:** get the reporter's testcase. Its `zypp-control.yaml` carries
   their aliases. Or fake aliases with `ZYPP_MODALIAS_SYSFS`.
4. **Logs:** `Modalias.cc` logs which source was used ("Using /sys directory." or
   "Using $ZYPP_MODALIAS_SYSFS …"). These are `DBG` lines, so they appear in log
   group `MODALIAS++` ([02](02-core-infrastructure.md)). `ZYPP_FULLLOG=1` adds
   libsolv details.
5. **Related chapters:** [03 §9](03-sat-pool-and-resolvables.md) (namespaces),
   [04 §18](04-solver.md) (recommends, supplements, rich deps),
   [08](08-target-commit-and-rpm.md) (target), [09](09-configuration-and-environment.md)
   (`ZYPP_MODALIAS_SYSFS`).
