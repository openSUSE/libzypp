# Core Infrastructure & Base Idioms

This chapter covers the low-level building blocks that every other part of libzypp uses: logging, exceptions,
smart pointers and PIMPL, string/path/URL value types, filesystem helpers, the external-process launcher, the
INI/XML parsers, and the callback (report) system that zypper and YaST use to get progress and ask the user
questions. Later sections cover progress reporting, the stream and compression stack, more base helpers, JSON,
the zypp-tui terminal library, gettext, and how process-wide singletons behave at startup and shutdown. It
also lists the coding conventions you will see across the codebase.

---

## 1. Where the code physically lives

Most "base" code has moved out of `zypp/zypp/base` into the shared tier, `zypp-logic/zypp-core`. The public
include paths `<zypp/base/Foo.h>` that zypper/YaST use are kept working by **one-line forwarding headers** in
`zypp/zypp-compat/zypp/...` (for example `zypp/zypp-compat/zypp/base/Logger.h` contains only
`#include <zypp-core/base/Logger.h>`). These are installed into `${INCLUDE_INSTALL_DIR}/zypp` by
`zypp/zypp-compat/CMakeLists.txt`.

| Topic | Real implementation | Legacy include kept via |
|---|---|---|
| Logger, LogControl, LogTools | `zypp-logic/zypp-core/base/{Logger.h,LogControl.{h,cc},LogTools.h}` | `zypp/zypp-compat/zypp/base/*.h` |
| Exception | `zypp-logic/zypp-core/base/Exception.{h,cc}` | compat |
| PtrTypes, ReferenceCounted, NonCopyable | `zypp-logic/zypp-core/base/` | compat; `zypp/zypp/base/ReferenceCounted.h` is a stub include |
| DefaultIntegral, Flags, EnumClass, Easy.h, String/StringV, Regex, Gettext, Iterable/Iterator, InputStream, GzStream, ZckStream, IOTools | `zypp-logic/zypp-core/base/` | compat |
| AutoDispose, TriBool, Date, ByteCount, CheckSum, Digest, Pathname, Url, ExternalProgram, UserData, KVMap, ShutdownLock, Globals.h, APIConfig.h | `zypp-logic/zypp-core/*` | compat `zypp/zypp-compat/zypp/*.h` |
| PathInfo / filesystem::, TmpPath | `zypp-logic/zypp-core/fs/{PathInfo,TmpPath}.{h,cc}` | `zypp/zypp/PathInfo.h` (adds `dirForEach(…, StrMatcher …)` + `matchNoDots()`), `zypp/zypp/TmpPath.h` (only includes the core header) |
| UrlBase, UrlUtils, UrlException | `zypp-logic/zypp-core/url/` | compat `zypp/url/*` |
| IniParser, IniDict, EconfDict, Sysconfig, ParseException, json, XmlEscape | `zypp-logic/zypp-core/parser/` | compat `zypp/parser/*` |
| ProgressData | `zypp-logic/zypp-core/ui/progressdata.{h,cc}` | compat `zypp/ProgressData.h` |
| PluginFrame (stomp-like) | `zypp-logic/zypp-core/rpc/` | – |
| Callback.h, ZYppCallbacks.h, KeyRing reports, DigestReport | `zypp/zypp/Callback.h`, `zypp/zypp/ZYppCallbacks.h`, `zypp/zypp/KeyRing.h`, `zypp/zypp/Digest.h` | (legacy only) |
| NamedValue, Measure, Signal, Backtrace, Counter, Collector, ValueTransform, LocaleGuard, Random | `zypp/zypp/base/` | (legacy only) |
| Json.h | `zypp/zypp/base/Json.h` is now only `#include <zypp-core/parser/json.h>` (see §16) | – |
| Glob, ManagedFile | `zypp/zypp/Glob.{h,cc}`; `zypp-logic/zypp-core/ManagedFile.h` | `zypp/zypp/ManagedFile.h` forwards |
| WatchFile, CleanerThread_p, DtorReset, Functional, Function, TypeTraits, IOStream, fXstream, SimpleStreambuf, FileStreamBuf | `zypp-logic/zypp-core/{fs,base}/` | compat |
| zypp-tui (`ztui::`) | `zypp/zypp-tui/` (static lib `libzypp-tui.a`) | installed as `<zypp-tui/...>` (§17) |
| xml::Reader (libxml2 xmlTextReader wrapper), ParseDef | `zypp/zypp/parser/xml/` | (legacy only) |
| SerialNumber, SetTracker, StrMatcher | `zypp-logic/zypp/base/` | – |

**File-name convention in zypp-logic:** newer files use a lower-case header (`defaultintegral.h`, `kvmap.h`,
`inputstream.h`, `iniparser.h`, `progressdata.h`) plus a Qt-style CamelCase header **without an extension**
(`DefaultIntegral`, `KVMap`, `InputStream`, `IniParser`, `ProgressData`) that only does
`#include "defaultintegral.h"`. Include the CamelCase form, and when you look for a class, grep both forms.

---

## 2. Coding conventions

* **Namespaces:** `zypp` (public API), `zypp::base` (LogControl, ReferenceCounted, Flags, NonCopyable),
  `zypp::str` (string helpers), `zypp::strv` (string_view helpers, `StringV.h`), `zypp::filesystem` (Pathname,
  PathInfo, TmpPath; `Pathname`/`PathInfo` are also pulled into `zypp`), `zypp::url`, `zypp::parser`,
  `zypp::callback`, `zypp::debug`, `zypp::log`, `zyppng` (NG; parts of it live in `zypp-logic/*/ng` and are used
  by legacy code too, e.g. the log thread and the spawn engines). Implementation-only helpers go in
  `*_detail` / `detail` namespaces (`exception_detail`, `rw_pointer`, `str::detail`) or anonymous namespaces.
* **Naming:** classes are `CamelCase`. Members are `_name`. Function parameters carry the `_r` suffix
  (`const Pathname & file_r`); older code also uses `_tv` (e.g. `Pathname::cat`). Template parameters are `Tp`,
  `TEnum`, `TIterator`, … (the older `_Tp` style was renamed because it is a reserved identifier).
* **Output:** every value type gets `std::ostream & operator<<`. Polymorphic hierarchies route it through a
  `protected virtual std::ostream & dumpOn( std::ostream & ) const` (`Exception`, `ReferenceCounted`).
  Types usually also provide `asString()` (member or free function `zypp::asString(obj)`). User-visible
  translated text comes from `asUserString()` (`zypp::asUserString(val)` calls `val.asUserString()`,
  `String.h:37`). Some types add `dumpAsXmlOn`.
* **Forward headers:** `*_fwd.h` / `DEFINE_PTR_TYPE(NAME)` (`PtrTypes.h:639`) forward-declare `NAME` together
  with `NAME_Ptr`/`NAME_constPtr` intrusive_ptr typedefs and `intrusive_ptr_add_ref/release` declarations.
  `IMPL_PTR_TYPE(NAME)` (`ReferenceCounted.h`) emits the matching definitions in the `.cc`.
* **PIMPL:** a class declares `class Impl;` / `struct Impl;` and holds `RW_pointer<Impl>` or
  `RWCOW_pointer<Impl>` named `_pimpl`. Copy-on-write classes need `Impl * clone() const`
  (`rwcowClone()` calls `rhs->clone()` and can be specialised). Common helper: `Impl * clone() const { return new Impl( *this ); }`.
* **Visibility / ABI:** see §3.3. Anything an application uses must be marked `ZYPP_API`.
* **Easy.h macros** (`zypp-logic/zypp-core/base/Easy.h`): `for_(IT,BEG,END)`, `arrayBegin/arraySize/arrayEnd`,
  `defConstStr(FNC,STR)`, `NON_COPYABLE(C)`, `NON_MOVABLE(C)`, `DEFAULT_COPYABLE(C)`, `DEFAULT_MOVABLE(C)`,
  `NON_COPYABLE_BUT_MOVE(C)`, `disable_use_as_copy_ctor<>` (stops a forwarding ctor from being picked as the
  copy ctor).
* **EnumClass** (`base/EnumClass.h`): the pre-C++11 `enum class` emulation
  `struct EFooDef { enum Enum {…}; }; using Foo = base::EnumClass<EFooDef>;`. It is still used in public API
  (`Date::DateFormat`, `JobReport::MsgType`).
* **Language level:** AGENTS.md says public headers must be C++11-compatible. In practice the core headers
  already need C++17: `Exception.h` uses `std::is_base_of_v` and `if constexpr`, and `AutoDispose.h` uses
  `std::is_pointer_v`. That line in AutoDispose even carries a "bsc#1194597 must be c++11" comment while being
  C++17 code. Check what consumers actually compile with before you "fix" either side.
* **LEGACY(#)** (`Globals.h`): `#if LEGACY(17)`-style guards keep deprecated symbols for older
  major/soversion/numversion. Under `ZYPPNG` the macro is always 0.
  `ZYPP_DEFINE_GLOBAL_CONSTANT/CONSTEXPR` gives namespace-scope constants the right linkage in both
  legacy (`static const`) and module builds (`inline constexpr`).
* Header banner: the ASCII-art "ZYPP" comment block plus `/** \file zypp/... */` (the path in `\file` is often
  the *old* location).

---

## 3. Logging

### 3.1 Macros and levels (`zypp-logic/zypp-core/base/Logger.h`)

```
XXX DBG MIL WAR ERR SEC INT USR   -> L_xxx( ZYPP_BASE_LOGGER_LOGGROUP )
L_XXX(g) .. L_USR(g)              -> zypp::base::logger::getStream( group, level, L_BASEFILE, __FUNCTION__, __LINE__ )
```

| Macro | Level enum | Numeric in log line | Notes |
|---|---|---|---|
| `XXX` | `E_XXX` | 999 | Written only in "excessive" mode (`ZYPP_FULLLOG` or `LogControl::TmpExcessive`) |
| `DBG` | **`E_MIL`** in group `"<group>++"` | 1 | **Not `E_DBG`.** `L_DBG(GROUP)` expands to `ZYPP_BASE_LOGGER_LOG( GROUP"++", E_MIL )`, so debug lines show up as `<1>` in group `[zypp++]` |
| `MIL` | `E_MIL` | 1 | milestone |
| `WAR` | `E_WAR` | 2 | |
| `ERR` | `E_ERR` | 3 | |
| `SEC` | `E_SEC` | 4 | security |
| `INT` | `E_INT` | 5 | internal error. **All exception THROW/CAUGHT/RETHROW lines use INT** |
| `USR` | `E_USR` | 6 | TRACE/OSD go here |

`E_DBG = 0` exists but none of the standard macros use it. To find debug output in a log, grep for
`[zypp++]`, `[zypp-core++]` and similar groups.

**Log groups** come from the compile definition `ZYPP_BASE_LOGGER_LOGGROUP`. The root `CMakeLists.txt` defines
`SET_LOGGROUP(group files...)`, which sets `-DZYPP_BASE_LOGGER_LOGGROUP="group"` on each file:

* `"zypp-core"` (`zypp-logic/zypp-core/zypp-core.cmake:481`), `"zypp"` (`zypp-logic/zypp/zypp.cmake`,
  `zypp-common.cmake`), `"zypp-media"`, `"zypp-curl"`, `"zyppng"` (zypp-glib).
* Many files override it locally with `#undef/#define ZYPP_BASE_LOGGER_LOGGROUP`. Examples:
  `"zypp::exec"` (spawn engines), `"zypp::repomanager"`, `"zypp::plugin"`, `"zypp::solver"`,
  `"zypp::KeyRing"`, `"zypp::gpg"`, `"zypp::fetcher"`, `"librpmDb"`, `"parser::yum"`, `"zconfig"`,
  `"PoolQuery"`, `"Measure"`.
* If nothing defines it, the group is `"DEFINE_LOGGROUP"`. Seeing that in a log means a file is missing from
  its SET_LOGGROUP list.
* A null or empty group means "don't log" (`getStream` returns a null stream). `L_ENV_CONSTR(ENV,GROUP,LEVEL)`
  uses this to log only when env var `ENV` is set: `L_ENV_CONSTR_DEFINE_FUNC(ENV)` defines
  `zypp::log::empty_or_group_if_ENV()`.

Extra debugging helpers in `Logger.h`:

* `BLOCKTRACE(msg)` writes `+++ (depth)` / `--- (depth)` as MIL into group `"BLOCK"`.
* `TRACE(msg)` / `XTRACE` and `OSD << …` (not compiled with `ZYPP_NDEBUG`) write colored output to **stderr**
  and also to group `"TRACE"`/`"OSD"` at USR. They are for local debugging only; don't commit them.
* `LogTools.h` adds `dumpRange`, `dumpRangeLine`, `operator<<` for std containers, `pMIL(a,b,c)` (line format),
  `wMIL` (words), and `str::Format*` join formats.

### 3.2 Line format (`LogControl::LineFormater::format`, `LogControl.cc:839`)

```
2026-10-22 10:11:12 <1> host(4711) [zypp::repomanager] RepoManager.cc(refreshMetadata):123 message
2026-10-22 10:11:12 <1> host(4711) [zypp-core] ExternalProgram.cc(start_program):170 {T:Zypp-xyz} message
```

* `<N>` is the numeric `LogLevel`. Lines from a level-999 message start with `<999>`.
* The `{T:name}` thread tag is left out only on the thread named `"Zypp-main"`. That is the thread which first
  created the `LogThread`; it is named on the assumption that this is the main thread.
* In *log forwarding mode* (`LogControl::enableLogForwardingMode()`, used by internal helper tools such as the
  zypp-rpm/media workers that write into their parent's log), the PID printed is the **parent's**
  (`getppid()`) and the thread tag is always shown.
* `JournalLineFormater` uses the same format without date, host and PID.
* `ZYPP_PROFILING` set → `ProfilingFormater` (`base/ProfilingFormater.cc`) is installed instead.

### 3.3 Architecture: log thread + per-thread LogControlImpl

```
any thread                        LogControlImpl (thread_local singleton)
 MIL << "x" << endl  ──►  Loglinestream/Loglinebuf (per group+level, buffers until '\n')
                               │ putStream → LineFormater::format()
                               ▼
                         LogClient::pushMessage  ── AF_UNIX abstract socket "zypp-logsocket-<pid>" ──►
                                                                       LogThread ("Zypp-Log", own EventLoop)
                                                                         └► LineWriter::writeOut()
                                                                            (FileLineWriter / Stderr / custom)
```

* `LogThread` (`LogControl.cc:84`) is a process-wide singleton. It starts a `std::thread` with its own
  `zyppng::EventLoop` that listens on an **abstract** unix socket `zypp-logsocket-<pid>` (so there is no file to
  clean up). Each thread's `LogClient` connects to it and sends whole lines. Lines coming from the log thread
  itself are written directly. The log thread blocks all signals.
* The `LineWriter` is stored in the LogThread behind a `SpinLock` (an atomic_flag, so it is safe in signal
  handlers). That makes **the LineWriter global**, while the formatter, the "excessive" flag and the stream table
  live in the **thread_local** `LogControlImpl`. Consequence: `setLineFormater()` and `TmpExcessive` affect
  **only the calling thread**, but `logfile()`/`setLineWriter()` affect every thread.
* `LogControlImpl` is constructed lazily. Its ctor reads env vars once per process (`std::call_once`):
  `ZYPP_LOGFILE` → `logfile(path)` and `ZYPP_PROFILING` → profiling formatter. `ZYPP_FULLLOG` is read in each
  thread's ctor and turns excessive (`XXX`) logging on.
* **No logging by default.** With no LineWriter, `getStream` returns a null stream and nothing gets formatted.
  The application has to call `LogControl::instance().logfile(...)` or set `ZYPP_LOGFILE`.
* `logfile(path, mode=0640)`: an empty path turns logging off, `"-"` means stderr. The file is opened in append
  mode with buffering off (`pubsetbuf(0,0)`). If `mode` is non-zero the file is first created with
  `O_CREAT|O_EXCL` at that mode; it deliberately avoids `filesystem::assert_file`, because that function logs.
  The log file is **never rotated** by libzypp; that is logrotate's job.
* Shutdown and fork safety: `logControlValidFlag()` is a POD `thread_local int`. It is set by the ctor and
  cleared by the dtor, so code that logs from static destructors (after TLS destructors ran) gets a null stream
  instead of crashing. `pthread_atfork(…, child: LogControl::notifyFork)` sets the flag to 0, so **a forked
  child logs nothing** unless it re-initialises. `emergencyShutdown()` stops the log thread.
* The log thread uses `zyppng::EventLoop`, `Socket` and `Wakeup` from `zypp-logic/zypp-core/ng/`. Breaking
  those breaks logging for legacy libzypp too.

### 3.4 How applications configure logging

* **zypper** (`zypper/src/main.cc`): `logfile = getenv("ZYPP_LOGFILE") ?: ZYPPER_LOG` where
  `ZYPPER_LOG = "/var/log/zypper.log"` (`zypper/src/main.h:48`). `zypp-refresh` uses `ZYPP_REFRESH_LOG`.
  zypper's SIGINT handler deliberately doesn't log, because building a log line can malloc.
* **YaST / PackageKit** call `LogControl::instance().logfile(...)` or install their own `LineWriter`, and may use
  `JournalLineFormater`. Their code is outside this repo; I did not verify it here.
* `LogControl::TmpLineWriter` and `TmpExcessive` are RAII helpers, mostly used in tests and tools.

### 3.5 Logging-related environment variables (verified `getenv` calls)

| Var | Effect | Where |
|---|---|---|
| `ZYPP_LOGFILE` | log target; `-` = stderr | `LogControl.cc`, zypper `main.cc` |
| `ZYPP_FULLLOG` | enable `XXX` (level 999) lines | `LogControl.cc` and 2 other places |
| `ZYPP_PROFILING` | ProfilingFormater | `LogControl.cc` |
| `ZYPP_LIBSOLV_FULLLOG`, `ZYPP_LIBSAT_FULLLOG` | verbose libsolv debug into the log | sat pool (see [SAT pool](03-sat-pool-and-resolvables.md)) |
| `ZYPP_MEDIA_CURL_DEBUG`, `ZYPP_METALINK_DEBUG` | curl/metalink tracing | see [Media](07-media-and-downloads.md) |
| `ZYPP_RPM_DEBUG`, `ZYPP_PLUGIN_DEBUG` | rpm / plugin verbosity | see [Commit](08-target-commit-and-rpm.md) |
| `ZYPP_FORK_BACKEND` | `pfork` (default/`auto`) or `gspawn` | `ng/io/abstractspawnengine.cc` |
| `ZYPPTMPDIR` | base dir for TmpPath | `fs/TmpPath.cc` |

The full env var list is in [Configuration & environment](09-configuration-and-environment.md).

---

## 4. Exceptions (`zypp-logic/zypp-core/base/Exception.{h,cc}`)

`zypp::Exception : std::exception` holds:

* `_msg`: the message from the ctor. `what()` returns it, but `what()` is **private** in `Exception`. Use
  `msg()`, `asString()` (= `dumpOn`) or `asUserString()`.
* `_where`: `CodeLocation` (file basename, function, line). It is **mutable** and updated by `relocate()`.
* `_history`: `std::list<std::string>` of messages, newest first. The exception's own message is **not** part
  of its history.

Macros (they all log at **INT** level in the group of the file that uses them):

| Macro | Log prefix | Behaviour |
|---|---|---|
| `ZYPP_THROW(e)` | `THROW:` | relocate to here, log, `throw e` (copy of the *static* type of `e`) |
| `ZYPP_CAUGHT(e)` | `CAUGHT:` | only logs; also accepts `std::exception_ptr` |
| `ZYPP_RETHROW(e)` | `RETHROW:` | log, relocate, `throw;` (keeps the dynamic type); also accepts `exception_ptr` |
| `ZYPP_EXCPT_PTR(e)` | `THROW (EXCPTR):` | log and return `std::make_exception_ptr` (NG / async code) |
| `ZYPP_FWD_EXCPT(p)` / `ZYPP_FWD_CURRENT_EXCPT()` | `RETHROW (FWD) EXCPTR:` | relocate an `exception_ptr` and forward it |
| `ZYPP_THROW_MSG`, `ZYPP_THROW_ERRNO[1]`, `ZYPP_THROW_ERRNO_MSG[1]` | | build a message from `Exception::strErrno(errno[,msg])` |

`Exception::log()` writes `where_r prefix asUserHistory()`, so every THROW line also contains the complete
history. Non-`Exception` types can be passed to THROW/CAUGHT and are logged by `typeid().name()`.

**History / remember:**

* `remember(const Exception & old)` puts `old.asUserString()` in front of `old`'s history, and the result
  **replaces** `this->_history`. Self-remember is ignored.
* `remember(std::exception_ptr)` handles `Exception`, `std::exception` (`what()`) and unknown types.
* `remember(std::string)` is intentionally just `addHistory()`, so a plain string doesn't discard existing
  history.
* There are ctors `Exception(msg, const Exception & history)`, and `addToHistory`/`moveToHistory` (oldest first).
* `historyAsString()` gives `"History:\n - msg\n   continuation…"` (translated header).
* `asUserHistory()` = `asUserString()` + `\n` + `historyAsString()`. zypper prints this to users.

`asUserString()` runs the `dumpOn()` output through `_()`. That only translates when `dumpOn` produces exactly
a msgid. Messages that are composed, or already translated, pass through unchanged. That is harmless, but don't
rely on it for translation.

Exception subclasses for each subsystem (MediaException, RepoException, ParseException, SolverException,
UrlException, KeyRingException, …) override `dumpOn` and are covered in their chapters. The generic ones here:
`parser::ParseException` (`zypp-logic/zypp-core/parser/parseexception.h`), `UserRequestException`
(`base/userrequestexception.h`; ABORT/RETRY/SKIP/IGNORE are used to carry a user decision out of a callback),
`url::UrlException` family (`url/UrlException.h`), and `xml::Reader`'s own internal `ParseException`
(`zypp/zypp/parser/xml/Reader.cc:69`).

NG note (AGENTS.md): new data-dependent errors in `zyppng/` must use `expected<>`, not exceptions. The
`ZYPP_EXCPT_PTR` family is the bridge for storing legacy exceptions in `expected<T, std::exception_ptr>`.

---

## 5. Smart pointers, reference counting, PIMPL

`zypp-logic/zypp-core/base/PtrTypes.h`:

* `zypp::shared_ptr`, `weak_ptr`, `scoped_ptr`, `intrusive_ptr` and the casts are **`boost::`** types, not
  `std::`. This is part of the ABI: `LogControl::setLineWriter(shared_ptr<…>)` takes a boost shared_ptr. The log
  thread comment says boost's lock-free refcounting is relied on in signal context.
* `NullDeleter` is a shared_ptr deleter that does nothing.
* `operator<<` for shared/intrusive pointers prints the pointee, or `"NULL"`.
* **`RW_pointer<D, DTraits = rw_pointer::Shared<D>>`**: a const-correct wrapper. `operator->` gives `const D*`
  in const member functions and `D*` otherwise, so a const interface method can't accidentally call a non-const
  Impl method. Traits: `rw_pointer::Shared`, `Intrusive`, `Scoped`. **Copying an `RW_pointer` shares the Impl**
  (shallow copy).
* **`RWCOW_pointer`**: copy-on-write. Every non-const access (`operator->`, `operator*`, `get()`, `getPtr()`)
  calls `assertUnshared()`, which clones via `rwcowClone(ptr)` → `ptr->clone()` when `use_count > 1`. The
  pitfall: calling a non-const method on a non-const object **clones even when you only read**. Use
  `const` access or `cgetPtr()` in hot paths.
* `base::ReferenceCounted` (`base/ReferenceCounted.{h,cc}`): intrusive refcount for `intrusive_ptr` (all
  resolvable kinds use it via `DEFINE_PTR_TYPE`). The counter is a plain `mutable unsigned` (**not atomic, not
  thread-safe**; the header has a `\todo` saying so). A copy starts at 0. The dtor throws if the count is
  non-zero (the header doc says "\throw std::out_of_range"), and `unref()` at 0 calls `unrefException()`.
  `ref_to/unref_to` are virtual hooks.
* `base::NonCopyable` = `boost::noncopyable` (inherit privately). Newer code uses the `NON_COPYABLE(C)` macro.
* `ProvideNumericId<Derived, NumT>` (`base/providenumericid.h`) gives each instance a unique id
  (`ProgressData` uses it).

---

## 6. Small value types and helpers

### DefaultIntegral (`base/defaultintegral.h`)
`DefaultIntegral<Tp, TInitial>` is an integral that is initialised to `TInitial` by default (handy in
`std::map<K,Counter>`). Aliases: `TrueBool`, `FalseBool`, `ZeroInit<T>`. It converts implicitly to `Tp&`.

### Flags (`base/Flags.h`)
`base::Flags<TEnum>` is QFlags-style. Declare with `ZYPP_DECLARE_FLAGS(Name,Enum)` inside the class and
`ZYPP_DECLARE_OPERATORS_FOR_FLAGS(Name)` **outside** it, or use both at once with
`ZYPP_DECLARE_FLAGS_AND_OPERATORS`. Mixing values from a different enum doesn't compile. `none()`, `all()`,
`testFlag`, `setFlag`, etc. are available.

### NamedValue (`zypp/zypp/base/NamedValue.h`)
A bidirectional value↔name map with aliases: `clist( CMD_1 ) | "cmd1" | "alias";`. Used for parsing enums
from config strings.

### Iterable / Iterator (`base/Iterable.h`, `base/Iterator.h`)
`Iterable<TIterator>` wraps a begin/end pair so it works in range-for (`makeIterable(b,e)`). `Iterator.h` has
boost-based filter/transform/map-key iterators (`make_filter_begin`, `MapKVIteratorTraits`, …), which are used
heavily by the pool APIs.

### str:: utilities (`base/String.h`, `base/StringV.h`, `base/Regex.h`)
* `str::form(fmt,...)` (printf), `str::Str() << …` (ostringstream builder), and `str::Format("%1% %2%") % a % b`
  (boost::format; zypper uses it for translatable strings, which is safer than `form` because the order of
  arguments can differ between languages).
* `C_Str`: one parameter type for `const char*`/`std::string`.
* `numstring/hexstring/octstring/binstring`, `strtonum<T>` (base 0, so `"010"` is octal), `strToTrue`/
  `strToFalse`/`strToBool`/`strToTriBool` (true = `1,true,yes,on,always` or a non-zero number; false =
  `0,false,no,off,never`).
* `split`, `splitEscaped`, `splitFields[Escaped]`, `join`, `joinEscaped`, `trim/ltrim/rtrim`,
  `gsub/replaceAll`, `escape`, `bEscape`, `rxEscapeStr/Glob`, `hexencode/hexdecode`, `toLower/toUpper`,
  `hasPrefix/stripPrefix/hasSuffix/startsWith/endsWith/contains`, `printIndented`, `autoPrefix`, UTF-8 helpers
  (`validateUtf8`, `codepointToUtf8String`).
* `str::strerror(errno)`, which `Exception::strErrno` uses.
* `strv::split(sv, sep, callback(line, idx, last))` is the `string_view` splitter used in newer code
  (e.g. `historyAsString`).
* `str::regex` / `smatch` wrap POSIX `regcomp` (not std::regex).

### Gettext (`base/Gettext.{h,cc}`)
* **Inside libzypp** (compiled with `-DZYPP_DLL`): `_()` → `zypp::gettext::dgettext(msg)` using
  `TEXTDOMAIN="zypp"` and `LOCALEDIR=${prefix}/share/locale`. Every zypp-logic `*.cmake` sets these with
  `ADD_DEFINITIONS(-DLOCALEDIR=… -DTEXTDOMAIN="zypp" -DZYPP_DLL)`. The domain is bound lazily with codeset
  UTF-8 on first use.
* **In applications** (no `ZYPP_DLL`): `_()` falls back to plain `::gettext` (the application's own domain),
  and is only defined if it isn't already defined. So including `zypp/base/Gettext.h` in zypper does *not*
  translate with the zypp catalog.
* `N_()` only marks a string for extraction. `PL_(s,p,n)` handles plurals.
* The `.po` files live in-tree in `libzypp/po/*.po` (domain/POT name `zypp`). Details in §18.

### AutoDispose (`AutoDispose.h`)
`AutoDispose<Tp>` is a shared handle that runs a `Dispose` function when the last copy goes away.
`resetDispose()` cancels it, `setDispose()` replaces it. Related types: `AutoDispose<void>` =
`OnScopeExit`, `Deferred` (scope guard), `AutoFD` (closes an int fd), `AutoFILE` (fclose), `AutoFREE<T>`
(free). Because it is **refcounted, copies share the dispose**. Constness doesn't propagate: use
`AutoDispose<const Tp>` if the value should be immutable. `ManagedFile` = `AutoDispose<const Pathname>`.

### TriBool (`TriBool.h`)
`boost::logic::tribool`. **`indeterminate == indeterminate` is not `true`**; it evaluates to `indeterminate`.
Test with `indeterminate(x)`. Helpers: `tri2bool(x, default)`, `asString(x, istr, tstr, fstr)`. This type is
used everywhere for "not configured, use default" settings.

### Date (`Date.{h,cc}`)
A `time_t` wrapper with constants (`Date::day`, `month`=month30, `year`=year365), `now()`, `form(fmt, TimeBase)`,
`print(DateFormat, TimeFormat, TimeZoneFormat)`, `printISO`/`printDate`/`printTime`. `print()` with default
arguments gives `'2014-02-07 07:06:41 CET'`, but **`operator<<` and `asString()` use `form("%c")`**, the
locale's preferred format, so log/console output differs by locale. The one-argument `Date(string)` ctor expects
a time_t number as a string; `Date(str, fmt[, TimeBase])` uses `strptime` and throws `DateFormatException`.
More in §15.

### ByteCount (`ByteCount.{h,cc}`)
Units come in two families: IEC `K, M, G, T` (=KiB…, 1024-based) and SI `kB, MB, GB, TB` (1000-based).
`bestUnit()` vs `bestUnit1000()`. `fillBlock/fullBlocks/blocks(blocksize=K)`. `ByteCount::K` is **1024**,
not 1000.

### CheckSum / Digest (brief)
`zypp-logic/zypp-core/CheckSum.{h,cc}` stores a (type, hex string) pair. The type is detected from the hash
length when not given (md5/sha1/sha224/sha256/sha384/sha512). `Digest.{h,cc}` is the OpenSSL-backed hasher.
`zypp/zypp/Digest.h` adds `DigestReport` (a callback that asks the user about missing or unknown checksums).
`filesystem::md5sum/sha1sum/checksum/is_checksum` compute file checksums. Security semantics are in
[Security](06-security-gpg-keyring.md).

### KVMap (`kvmap.h`)
`std::map<string,string>` that parses and joins with a policy: `KVMapPolicy(kvsplit, fsplit[, kvjoin, fjoin])`
or `kvmap::KVMapBase::CharSep<'=', ','>`. Used for small option strings, e.g. in media and mount code.

### UserData (`UserData.h`, `ContentType.h`)
`callback::UserData` is a `ContentType` (type/subtype string pair) plus a lazily allocated, shared
`std::map<string, boost::any>`.

* `set(key, val)` on a **const** UserData only fills **empty or new** keys and returns false otherwise. That is
  how a receiver can hand values back to the sender through a `const UserData &`.
* `get<T>(key)` throws `boost::bad_any_cast` when the type doesn't match **exactly**; `get(key, default)` and
  `get(key, T&)` are the non-throwing variants.
* Copies share the map (shallow).

---

## 7. Pathname, PathInfo, TmpPath

### Pathname (`zypp-logic/zypp-core/Pathname.{h,cc}`)
* The stored path is always normalised by `_assign`: no inner `.` or `..`, no `//`. **A relative path is stored
  with a leading `./`**: `Pathname("foo").asString() == "./foo"`, `Pathname("..") == "./.."`,
  `Pathname("./") == "."` (see `zypp/tests/zypp/Pathname_test.cc`). A `..` above `/` is dropped
  (`"/.." → "/"`).
* `/` and `cat()` insert exactly one separator, and the right-hand side is *appended even if it is absolute*
  (`"foo" / "/baa" == "foo/baa"`). `operator+=` is deprecated.
* `extend(".h")` appends to the last component. `dirname`, `basename`, `extension`, `absolutename`,
  `relativename`, `realpath`.
* `assertprefix(root, path)` prefixes `root` unless `path` is already below it, and handles the pathological
  `root == "."` case so that `..` can't escape. `stripprefix(root, path)` is the inverse.
  `showRoot`/`showRootIf` give the `"(root)/path"` form used in logs.
* `asUrl(scheme)`, `asDirUrl()`, `asFileUrl()`.

### PathInfo & filesystem:: (`zypp-logic/zypp-core/fs/PathInfo.{h,cc}`)
* `PathInfo(path[, STAT|LSTAT])` wraps `stat`: `isExist/isFile/isDir/isLink`, permissions, size, owner,
  `userMayRX` and so on. The stat runs in the ctor, so call `stat()` again to refresh.
* **The free functions return `0` on success or the `errno` value; they do not throw.** Each result is logged
  by `logResult()` (`PathInfo.cc:285`). Functions: `mkdir`, `assert_dir` (mkdir -p), `rmdir`,
  `recursive_rmdir`, `clean_dir`, `copy_dir[_content]`, `readdir`/`dirForEach[Ext]`, `is_empty_dir`,
  `assert_file[_mode]`, `touch`, `unlink`, `rename`, `exchange`, `copy`, `symlink`, `hardlink`,
  `hardlinkCopy`, `readlink`, `expandlink`, `copy_file2dir`, `md5sum`/`sha1sum`/`checksum`/`is_checksum`,
  `chmod`/`chmodApplyUmask`/`addmod`/`delmod`, `erase`, `df`, `zipType`, `getUid/Gid`.
* `rename()`'s doc mentions retrying on `EXDEV` (OverlayFS layers). `exchange()` swaps two paths via
  `::rename`, so both must be on the same filesystem; the typical pattern is
  `TmpFile::makeSibling(target)` followed by `exchange`.
* `zypp/zypp/PathInfo.h` adds a `dirForEach(dir, StrMatcher, fnc)` overload and `matchNoDots()`. They live
  there because StrMatcher is in `zypp-logic/zypp/base`.

### TmpPath / TmpFile / TmpDir (`fs/TmpPath.{h,cc}`)
* RAII temporary paths, refcounted (copies share them). By default the path is deleted when the last copy
  goes away: `recursive_rmdir` for directories, or `clean_dir` with `KeepTopdir`. Errors are logged at INT.
  `autoCleanup(false)` keeps the path.
* `defaultLocation()` = `$ZYPPTMPDIR` or `/var/tmp`, **plus `/zypp.tmp` when running as root**. bsc#1249435
  added this fixed component so SELinux policies can label it. The value is computed once (static).
* `makeSibling(path[, mode])` creates the temp object next to `path`, on the same filesystem, for atomic
  `rename`/`exchange`.

---

## 8. Url (`zypp-logic/zypp-core/Url.{h,cc}`, `url/UrlBase.{h,cc}`, `url/UrlUtils.{h,cc}`)

* `Url` is a handle around a `url::UrlRef` (a shared `UrlBase`). Each scheme is handled by a configured
  `UrlBase` prototype in a scheme repository (`Url.cc` ~l.190–240). `Url::registerScheme()` lets callers add
  their own.

| Scheme group | Config |
|---|---|
| `ldap`, `ldaps` | `LDAPUrl` subclass: query string is `attrs?scope?filter?exts` |
| `hd cd dvd dir iso mailto urn plugin` | `with_authority=n`, path required (`m`) |
| `file` | authority allowed but no port, no user/password |
| `nfs nfs4 smb cifs http https ftp sftp tftp` | host mandatory. `ftp`/`sftp`/`tftp` set `path_encode_slash2=y` (the second slash is always encoded) |

* Scheme predicates (`Url.cc:475+`): `schemeIsLocal` (cd dvd dir hd iso file), `schemeIsRemote` (http https
  nfs nfs4 smb cifs ftp sftp tftp), `schemeIsVolatile` (cd dvd), `schemeIsDownloading` (http-like or
  ftp-like), `schemeIsHttpLike`, `schemeIsFtpLike`, `schemeIsPlugin`.
* **ViewOptions** decide which parts `asString()` prints. `DEFAULTS` = scheme, username, host, port, path, query,
  fragment, empty authority, empty path name. **`WITH_PASSWORD` is not in the defaults, so passwords are hidden
  in `asString()` and `operator<<`.** `asCompleteString()` includes everything, password too.
* **`proxypass=` query parameter hiding** (bsc#1050625, CVE-2017-9271, also bsc#1177583; `UrlBase.cc:115-173`):
  the query string is kept in two forms, full and safe (with `proxypass=` removed). The safe form is printed
  unless `WITH_PASSWORD` or the hidden option `ViewOptions::hotfix1050625` (0x8000) is set. When code needs the
  credentials back without exposing the password in other contexts (writing `.repo`/`.service` files,
  repo-variable expansion), it calls `zypp::hotfix1050625::asString(url)`: see `zypp/zypp/RepoInfo.cc:1103`,
  `ServiceInfo.cc:179`, `repo/RepoVariables.cc:575`.
* **`operator==` and `operator<` compare `asCompleteString()`**, so two Urls that differ only in password are
  not equal. Query parameter order matters as well.
* Encoding: `url::encode/decode`, `UrlUtils` split/join helpers. Every component has `get…(EEncoding)` to choose
  encoded or decoded form. Invalid components throw `UrlNotAllowedException`, `UrlBadComponentException`,
  `UrlParsingException`, ….
* RepoVariables expansion of `$releasever` and similar (raw vs expanded URLs) is covered in
  [Repositories](05-repositories-and-services.md).

---

## 9. ExternalProgram (`zypp-logic/zypp-core/ExternalProgram.{h,cc}`)

* Runs a child process with `stdin`/`stdout` pipes (it inherits `ExternalDataSource`, giving `receiveLine()`,
  `send()`, `operator>>`).

```
ExternalProgram( argv, [Environment], Stderr_Disposition = Normal_Stderr,
                 use_pty=false, stderr_fd=-1, default_locale=false, root="" )
```

* `Stderr_Disposition`: `Normal_Stderr` (inherit), `Discard_Stderr` (`/dev/null`), `Stderr_To_Stdout`,
  `Stderr_To_FileDesc` (to `stderr_fd`).
  * `ExternalProgramWithStderr` uses an `EarlyPipe` to read stderr separately (`stderrGetline`).
  * `ExternalProgramWithSeperatePgid` starts the child in its own process group (ShutdownLock uses it).
* `default_locale=true` → `LC_ALL=C` in the child. Use it whenever you **parse** the child's output.
* `root` (chroot):
  * empty: no chroot.
  * `"/"`: **no chroot, only `chdir("/")`**.
  * otherwise `chroot(root)` and `chdir`. Failures give translated messages such as "Can't chroot to '%s'".
    This needs root privileges.
* **Backend:** `zyppng::AbstractSpawnEngine::createDefaultEngine()` (`zypp-logic/zypp-core/ng/io/abstractspawnengine.cc`).
  * `ForkSpawnEngine` (posix fork/exec) is the default, and is forced when `use_pty` is set.
  * `GlibSpawnEngine` only when `ZYPP_FORK_BACKEND=gspawn` (and when built with `ZYPP_HAS_GLIBSPAWNENGINE`).
  * The choice is cached in a static.
* `forkspawnengine.cc` (log group `zypp::exec`):
  * In the child, every fd except the mapped ones is closed. The list of fds comes from `/proc/self/fd`
    because looping up to the fd limit was too slow (bsc#1191324).
  * pty mode calls `setsid()`; the "separate pgid" mode calls `setpgid(0,0)`.
  * Exec failures are sent back to the parent through a pipe (`childErrno`).
* Exit status (`AbstractSpawnEngine::checkStatus`):
  * a normal exit returns the status.
  * a signal returns **128+signo**, with `execError()` = "Command was killed by signal %d (%s)." For SIGKILL it
    adds "; Out of memory?".
  * exec/fork failures report 127/128.
  * `execError()` is empty on success. `close()` waits for the child and returns the status. `kill([sig])`
    is also available.
* `ExternalProgram` loads `default_locale` and env into the child, **not** the libzypp log setup. The child
  does not inherit logging, because of the atfork hook (§3.3).

### ShutdownLock (`ShutdownLock_p.h`, private header)
`ShutdownLock(who, reason)` starts `/usr/bin/systemd-inhibit --what=sleep:shutdown:idle …` as an
`ExternalProgramWithSeperatePgid` (stderr discarded) and terminates it in the dtor (SIGTERM, then SIGKILL with
a warning). `ShutdownLockCommit(who)` uses the reason "Zypp commit running." and is held during commit. If
systemd-inhibit is missing, the lock silently does nothing.

---

## 10. Parsers

### IniParser / IniDict (`zypp-logic/zypp-core/parser/iniparser.*`, `inidict.*`)
* Line-based. Whitespace is trimmed; lines starting with `;` or `#` are comments; `[section]` uses the
  **last** `]`. `key = value` splits at the first `=`, rtrims the key and ltrims the value.
* A line is "garbage" if it has no `=`, **or if one of `: / ? | , \` appears in the key part**. That calls
  the virtual `garbageLine()`, which throws `ParseException("…: Section [..]: Line N contains garbage …")` by
  default.
* `zypp/zypp/parser/RepoFileReader.cc:76` overrides `garbageLine` to support multi-line values (e.g. several
  `baseurl` lines) and calls the base class (which throws) otherwise. Put any "continuation line" behaviour in a
  subclass, not in IniParser.
* Hooks: `beginParse`, `consume(section)`, `consume(section,key,value)`, `endParse`. Progress is reported
  through `ProgressData::ReceiverFnc`.
* `IniDict` stores `section → key → value`. A repeated key **overwrites** the earlier value; RepoFileReader
  handles multiple URLs itself.
* `EconfDict` (`econfdict.h`): UAPI.6 config file and drop-in merging (`/usr/etc/zypp/zypp.conf` vendor file +
  `/etc/zypp/zypp.conf` + `.d/*.conf`). See [Configuration](09-configuration-and-environment.md).
* `Sysconfig` (`sysconfig.h`): reads `/etc/sysconfig`-style `KEY="value"` files (used for proxy settings).
* `json.h` / `json/`: JSON value model, writer and parser; see §16.

### XML
* `zypp/zypp/parser/xml/Reader.{h,cc}` wraps libxml2 `xmlTextReader` (opened with `XML_PARSE_PEDANTIC`, using a
  structured error handler that logs). It offers `foreachNode`, `foreachNodeAttribute` and `seekToNode`; read
  errors throw its internal `ParseException`. `ParseDef*` is a declarative node-callback layer built on top of
  it, used by the repomd/susetags/yum parsers ([Repositories](05-repositories-and-services.md)).
* `zypp-logic/zypp-core/parser/xml/XmlEscape.*` escapes text for XML output; `base/Xml.h` writes XML nodes (for
  `dumpAsXmlOn`).

### InputStream (`base/inputstream.h`)
Combines a `std::istream`, a name used in messages, and a size hint. It can be constructed from an `istream`
or from a path. For paths it opens `.gz` and `.zck` files transparently, but **not** `.zst`, `.bz2` or `.xz`.
The details and the stream class stack are in §14. Line iteration (`iostr::EachLine`, `forEachLine`,
`simpleParseFile`) is in `base/IOStream.h`, not `IOTools.h`.

---

## 11. Callback / report system (`zypp/zypp/Callback.h`)

"Callbacks light": **each report type has a single global receiver**.

```
 libzypp (sender)                                   application (receiver)
 ───────────────                                    ──────────────────────
 callback::SendReport<DownloadProgressReport> r;    struct MyDL : callback::ReceiveReport<DownloadProgressReport> {
   ctor  → Distributor::instance()->reportbegin()       bool progress(int, const Url&, double, double) override;
   r->start(...); r->progress(...)  ───────────────►    void reportbegin() override; void reportend() override; };
   dtor  → reportend()                              MyDL dl; dl.connect();   // replaces any previous receiver
                         DistributeReport<T>::instance()   (function-local static, holds _receiver or _noReceiver)
```

* `TReport` (e.g. `ProgressReport`) derives from `callback::ReportBase`. It **must not hold data**; it has only
  virtual methods whose default bodies return the value used when no receiver is connected.
  `ReportBase::report(const UserData&)` is the generic extension point.
* `ReceiveReport<T>` adds `connect()`, `disconnect()` (also called from the dtor), `connected()`,
  `whoIsConnected()`, and the virtual `reportbegin()`/`reportend()`.
* `SendReport<T>` is non-copyable. **Creating and destroying it already calls `reportbegin()`/`reportend()`**,
  so only create one when a task really starts. Destruction during exception unwinding still delivers
  `reportend()`.
* `DistributeReport<T>::instance()` is a function-local static inside a header template. Sender (libzypp) and
  receiver (application) must resolve to **the same instance**. That only works because the template is marked
  `ZYPP_API` (default visibility) and the version script exports `zypp::*`
  (`zypp/zypp/libzypp.map`). If you change visibility or the version script, report connections can silently
  stop working.
* `TempConnect<T>` temporarily replaces the receiver and restores the old one. Its default ctor means "no
  receiver"; `media::ScopedDisableMediaChangeReport` is a similar helper.
* There is no locking. Treat connect/disconnect and sending as main-thread only.

**Report types in `zypp/zypp/ZYppCallbacks.h`:**
* `ProgressReport` (generic, used with `ProgressData`)
* `repo::DownloadResolvableReport`
* the deprecated `ProbeRepoReport`, `RepoCreateReport` and `RepoReport`
* `media::MediaChangeReport`, `DownloadProgressReport`, `CommitPreloadReport`, `AuthenticationReport`
* `target::PatchMessageReport`, `PatchScriptReport`, `FindFileConflictstReport` (sic), `CommitActiveReport`
* `target::rpm::InstallResolvableReport`, `RemoveResolvableReport`, `RebuildDBReport`, `SingleTransReport`
* the `…ReportSA` variants (`TransactionReportSA`, `InstallResolvableReportSA`, `RemoveResolvableReportSA`,
  `CleanupPackageReportSA`, `CommitScriptReportSA`) used by the single-transaction zypp-rpm backend
* `CleanEmptyLocksReport`, `SavingLocksReport`, `JobReport`

Elsewhere: `KeyRingReport`/`KeyRingSignals` (`zypp/zypp/KeyRing.h`) and `DigestReport` (`zypp/zypp/Digest.h`).

**ABI rule (from the Callback.h doc):** adding virtual methods to an existing report breaks binary
compatibility. New information goes through `report(const UserData&)` or through UserData arguments,
identified by a `ContentType`. Examples:
* `InstallResolvableReport::contentRpmout`: rpm output lines are sent as UserData.
* `SingleTransReport::contentLogline`, with `loglevel {dbg,msg,war,err,crt}`.
* `JobReport::repoRefreshMirrorlist`.

Receivers have to check `userData.type()` and ignore types they don't know.

**JobReport** is a generic message channel: `JobReport::info/warning/error/important/debug/data(msg, userData)`
goes to `instance()->message(MsgType, msg, userData)`. `instance()` is implemented in ZYppImpl.cc.
`UserDataJobReport` is a UserData that sends itself along with every message. zypper receives it in
`zypper/src/callbacks/job.h`; its other receivers are in `zypper/src/callbacks/{media,rpm,repo,keyring,locks}.h`.
YaST connects its receivers through yast2-pkg-bindings (outside this repo, not verified here).

Receivers return enums such as `Action { ABORT, RETRY, IGNORE, … }` to steer the sender. Some senders turn
ABORT into a `UserRequestException`.

### ProgressData
Short summary: a value/range counter with an optional `ReceiverFnc` that is called at a throttled rate. The
receiver returns `false` to ask for an abort. Full description, throttling rules and the NG counterpart are in
§13.

---

## 12. Misc legacy helpers in `zypp/zypp/base/`

* `Measure` (`Measure.{h,cc}`, log group "Measure"): a stopwatch for wall and process time. `elapsed()` logs
  intermediate times, `stop()` (or the dtor) logs the total. Details and caveats in §15.
* `Signal.h`: `SignalSaver` (sets a handler with `::signal`, restores it in the dtor) and `SigprocmaskSaver`
  (saves the mask; `block(sig)`/`unblock(sig)`/`pending(sig)`/`suspend()`; restores it in the dtor). The only
  in-tree user is `zypp/zypp/PluginScript.cc:334` (`SignalSaver sigsav( SIGPIPE, SIG_IGN )` while talking to a
  plugin). `SigprocmaskSaver` has no in-tree user. Separately, the log thread and the cleaner thread block all
  signals for themselves (`zyppng::blockAllSignalsForCurrentThread()`).
* `Backtrace.{h,cc}`: `dumpBacktrace` manipulator. libzypp itself installs SIGSEGV/SIGABRT handlers that log a
  backtrace (§19).
* `Counter.h` (`Counter<TInt>`, an integral starting at 0; similar to `DefaultIntegral`), `Collector.h`,
  `ValueTransform.h` (raw/transformed value pair, used by RepoVariables), `LocaleGuard.h` (RAII
  `setlocale(category, value)`), `Random.h`, `Algorithm.h` (`invokeOnEach(begin, end[, filter], fnc)`: returns
  the number of calls, **negative** if `fnc` returned false and stopped the loop), `Debug.h`.

---

## 13. Progress reporting: ProgressData, CombinedProgressData, ProgressObserver

### 13.1 ProgressData (`zypp-logic/zypp-core/ui/progressdata.{h,cc}`, log group `"Progress"`)

```
ProgressData : base::ProvideNumericId<ProgressData,unsigned>      // numericId() unique per *object*
  RWCOW_pointer<Data> _d { State{INIT,RUN,END}, name, min, max, val, ReceiverFnc, last_val, last_send }
  value_type = long long
  ReceiverFnc = zypp::function<bool(const ProgressData &)>         // boost::function (base/Function.h)
```

* **Ranges:** `ProgressData()` = `[0,0]`, which means "no range" (only *still-alive* ticks).
  `ProgressData(max)`, `(min,max)`, `(min,max,val)`. `hasRange()` is just `min != max`. `reportPercent()` ==
  `hasRange()`, `reportAlive()` == `!hasRange()`. `reportValue()` gives a percent value, or `-1` without a range.
* **Producer API** (each one can call the receiver; all return the receiver's bool): `set(v)`, `set(other)`
  (copies min/max/val), `incr(n=1)`, `decr(n=1)`, `toMin()`, `toMax()` (only does a `tick()` when there is no
  range), `tick()` (value unchanged). `range()`, `noRange()`, `name()`, `sendTo()` and `noSend()` never report.
* **Throttling** (`ProgressData::report()`, `progressdata.cc:33`):

  | Condition | Sent? |
  |---|---|
  | state is INIT (first report) or END (dtor) | **always** (bsc#1206949, libzypp 17.31.8: "enforce reporting the INIT\|\|END state") |
  | less than 100 ms since the last send | no, and returns `true` without calling the receiver |
  | with a range: integer percent changed, or more than 1 s since the last send | yes |
  | without a range: more than 1 s since the last send | yes |

  So a receiver gets at most about 10 calls per second, and at least one per second while the producer keeps
  calling. If the producer stops calling, nothing is sent: there is no timer. The first send moves the state
  INIT→RUN and logs `{#id|name} START` (DBG). The **dtor** sends a final report only if the state is RUN.
  `finalReport()` is true only during that call, and END is logged. An object that never reported sends no END.
* `false` from the receiver is logged as `WAR "User request to ABORT pending action."` and passed back to the
  caller. **Nothing forces the producer to stop**; each call site must check the bool.
* `makeProgressData(InputStream)` names the object after the stream and uses `InputStream::size()` as the
  range. For compressed files `size()` is -1 (§14), so you get still-alive ticks instead of percent.
* Copying shares `Data` (RWCOW): a copy keeps the same receiver and throttle state until it writes. The
  `numericId` is assigned per object (`ProvideNumericId`), so it is *not* shared by copies.

### 13.2 CombinedProgressData

`CombinedProgressData(ProgressData & parent, weight)` is a functor you can hand to a sub-task as its
`ReceiverFnc`. Each sub-report increases the parent by
`(parent.max-parent.min) * weight/(parent.max-parent.min) * Δsub/(sub.max-sub.min)` = `weight * Δsub_fraction`
(`progressdata.cc:142`). If the sub-task or the parent has no range, or the weight is 0, it only does
`parent.tick()`. It holds a **reference** to the parent, so the parent must outlive every sub-task that uses
it. In-tree it is only used in `zypp/tests/zypp/ProgressData_test.cc`.

### 13.3 How progress reaches zypper / YaST

```
legacy producer                              receiver
ProgressData pd; pd.sendTo( fnc ) ─────────► fnc(pd)         (API arg  "const ProgressData::ReceiverFnc & progressrcv")
     │ ProgressReportAdaptor(fnc, report)                     (zypp/zypp/ZYppCallbacks.h:59)
     └──► SendReport<ProgressReport>: start(pd) on 1st call, progress(pd) each call, finish(pd) if pd.finalReport()
                                     └────► zypper ProgressReportReceiver (zypper/src/callbacks/repo.h:267)
                                            → Out::progressStart/progress/progressEnd(numericId, name, reportValue)
```

* `ProgressReport` (`ZYppCallbacks.h:40`) has three virtual methods: `start`, `progress` (return false to
  abort) and `finish`. `ProgressReportAdaptor` calls both the global receiver and an optional extra
  `ReceiverFnc`, and **ANDs** their results. Its only in-tree user is `zypp/zypp/target/RpmPostTransCollector.cc:199`.
* Most legacy `RepoManager` methods (`buildCache`, `addRepository`, `removeRepository`, …;
  `zypp/zypp/RepoManager.cc:155+`) now forward to NG workflows. They bridge with
  **`zyppng::ProgressObserverAdaptor`** (`zypp/zypp/ng/progressobserveradaptor.{h,cc}`): a `ProgressObserver`
  whose signals call `update()`, which builds a **new** `ProgressData(0,100,observer->progress())` every time and
  calls `start`/`progress`/`finish` plus the caller's `progressrcv`. Consequences: (a) the value returned by
  `update()` is thrown away in the signal lambdas, so **a receiver returning false does not abort** NG-backed
  operations; (b) every update carries a **different `numericId()`**. zypper's `OutNormal` ignores the id,
  but `OutXML::progress` writes it into the XML progress tag (`zypper/src/output/OutXML.cc:108`), so
  `--xmlout` consumers see ids that change between updates; (c) these ProgressData objects never report
  themselves, so `finalReport()` is always false and the end is signalled only by `finish()`.
* YaST receives the same `ProgressReport` through yast2-pkg-bindings (outside this repo; not verified).

### 13.4 NG: `zyppng::ProgressObserver` (`zypp-logic/zypp-core/ng/ui/progressobserver.{h,cc}`)

* A tree of `ProgressObserver` objects (`Base` subclass, created with `ProgressObserver::create(label, steps=100)`).
  Values are `double`. `setBaseSteps`, `setCurrent`, `inc(n, newLabel)`, `start()`, `setFinished(Success|Error)`,
  `progress()` (percent). `makeSubTask(weight, label, steps)` / `registerSubTask(child, weight)` build the tree,
  and a child's progress adds to the parent in proportion to its weight.
* Static null-safe helpers (`ProgressObserver::start/increase/setCurrent/finish(ref, …)`) and pipeline
  operators in `zyppng::operators` let workflows report progress without checking for a null observer.
* Signals: `sigStarted`, `sigLabelChanged`, `sigStepsChanged`, `sigValueChanged`, `sigProgressChanged`,
  `sigFinished`, `sigNewSubprogress`. There is **no time-based throttling** like in ProgressData.
* `makeProgressDataReceiver()` (`progressobserver.cc:344`) turns legacy ProgressData reports into observer
  updates: steps=`max-min`, current=`val-min`, label=`name`, finished on `finalReport()`. It **always returns
  true** (no abort path) and captures a strong reference to the observer.
* The observer is a signal-based (sigc++) `Base` object. Like other NG objects it belongs to the thread that
  created it; it has no locking of its own (see [zyppng](10-zyppng-next-generation.md)).

---

## 14. Streams: InputStream, fXstream, compressed I/O, line helpers

### 14.1 Class stack

```
std::istream / std::ostream
  └─ detail::fXstream<TBStream, TStreamBuf>          base/fxstream.h   (open/close/is_open/zError/getbuf)
       TStreamBuf = detail::SimpleStreamBuf<Impl>    base/simplestreambuf.h  (std::streambuf on a vector<char>, default 512 B)
           Impl = gzstream_detail::gzstreambufimpl   base/gzstream.{h,cc}    zlib gzdopen(fd,"rb"/"wb")
                | detail::zckstreambufimpl           base/zckstream.{h,cc}   libzck zck_init_read/zck_init_write  (ENABLE_ZCHUNK_COMPRESSION)
                | detail::FdStreamBufImpl            base/filestreambuf.{h,cc} plain fd (AutoFD)
ifgzstream / ofgzstream   = fXstream<istream|ostream, fgzstreambuf>
ifzckstream / ofzckstream = fXstream<istream|ostream, ZChunkStreamBuf>
FdStreamBuf               = SimpleStreamBuf<FdStreamBufImpl>   (streambuf only, no fXstream typedef)
```

* `SimpleStreamBuf` needs an Impl with `isOpen/canRead/canWrite/canSeek/readData/writeData/seekTo/tell/
  openImpl/closeImpl/error_type` (the contract is documented at the top of `simplestreambuf.h`). A stream is
  opened read-only or write-only, never both. `close()` syncs the put area first.
* `gzstreambufimpl` reads uncompressed files too: zlib's `gzread` passes non-gzip data through unchanged. Seek:
  `beg`/`cur` only; seeking backwards in read mode is expensive (zlib rewinds). `compressed_tell()` gives the
  position in the compressed file. Write mode uses zlib's default compression level (`"wb"`).
* `zckstreambufimpl` cannot seek (`canSeek` returns false, `seekTo` returns -1), so `tellg()` fails on it.
* `FdStreamBuf` wraps an existing fd: `zypp/tools/zypp-rpm/main.cc:187` uses it to write messages to
  `MessageFd`, with fd closing switched off.
* Writers in-tree: `ofgzstream` only in `zypp/zypp/solver/detail/Testcase_helix.cc:254` (solver testcases are
  written gzip-compressed). `ofzckstream` has no in-tree user.

### 14.2 InputStream (`base/inputstream.{h,cc}`)

| Ctor | Stream | `name()` | `size()` |
|---|---|---|---|
| `InputStream()` | `std::cin` (NullDeleter) | `"STDIN"` | -1 |
| `InputStream(istream &, name)` | borrowed, **not** owned: the caller keeps it alive | name | -1 |
| `InputStream(path[, name])` (Pathname / string / `const char*`) | `ifzckstream` if `zipType()==ZT_ZCHNK` (and zchunk support was built), otherwise `ifgzstream` | path or name | file size if a regular file **and** not compressed, otherwise -1 |

* Compression is detected by **magic bytes**, not by file name (`filesystem::zipType`, `PathInfo.cc:1148`):
  `1f 8b` → ZT_GZ, `BZh` → ZT_BZ2, `\0ZCK1` → ZT_ZCHNK. **There is no zstd or xz detection or stream class in
  libzypp.** A `.zst`/`.xz` file gets ZT_NONE and is read through `ifgzstream` in pass-through mode, which
  returns the compressed bytes. `ZT_BZ2` is detected but there is no bz2 stream, so bzip2 data is also returned
  raw. The `ENABLE_ZSTD_COMPRESSION` CMake option only adds `${ZSTD_LIBRARY}` to the link interface
  (`zypp-core.cmake:514`), because libsolv's `solv_xfopen` may need it.
* **Where compressed metadata is actually handled:** `primary.xml.zst`, `.gz` and susetags `packages.gz` are
  turned into `.solv` by libsolv's **repo2solv** child process (`zypp/zypp/ng/repo/workflows/repomanagerwf.cc:541`),
  which uses libsolv's `solv_xfopen` (gz/xz/zstd/bz2/zck depending on how libsolv was built).
  `Repository::addHelix/addTesttags` (`zypp/zypp/Repository.cc:349,368`) also use `solv_xfopen`. libzypp's own
  parsers (`xml::Reader` via `xmlReaderForIO` on the InputStream, `RepomdFileReader`, susetags `ContentFileReader`,
  `RepoMirrorList`, IniParser, json) only get gz/zck/plain. See [Repositories](05-repositories-and-services.md).
* The `InputStream` holds a `shared_ptr<std::istream>`, so copies share one stream position.

### 14.3 Line helpers (`base/IOStream.{h,cc}`, namespace `zypp::iostr`)

* `iostr::getline(istream&)`: reads a whole line of any length into a `std::string` (in 1024-byte chunks).
  The newline is removed; a `\r` is **not**. ⚠ It reads through a **function-static `char tmpBuff[1024]`**
  (`IOStream.cc:34`), so it is **not thread-safe**. Two threads parsing files at the same time can mix up each
  other's lines.
* `EachLine in(str[, startLineNo]); for(; in; in.next()) { *in; in.lineNo(); in.lineStart(); }`: it is
  non-copyable and reads the first line in its ctor. `lineStart()` is a `tellg()` result (-1 on zck streams).
* `int forEachLine(istream&, function<bool(int lineNo, std::string line)>)` returns the number of lines
  consumed, **negative** if the callback returned false. A last line without a trailing newline is still
  delivered.
* `simpleParseFile(istream&, ParseFlags, fnc)` with `PF_LTRIM | PF_RTRIM (PF_TRIM) | PF_SKIP_EMPTY |
  PF_SKIP_SHARP_COMMENT`. The 2-argument overload uses all four flags. Comments only count when `#` is the first
  non-blank character.
* `copy(in,out)`, `copyIndent(in,out,"> ")`, `tee(in,out)`; `IosFmtFlagsSaver` = `boost::io::ios_base_all_saver`.
* Separate low-level fd helpers are in `base/IOTools.h` (`zypp::io`): `setFDBlocking`, `writeAll`, `readAll`,
  `receiveUpto(FILE*, char, timeout)` (throws `TimeoutException`), `peek_data_fd`. ExternalProgram and the
  media workers use them.

---

## 15. More base helpers

### Functors (`base/Functional.h`, namespace `zypp::functor`) and `base/Function.h`
* `Function.h` only does `using boost::function;`. **`zypp::function` is `boost::function`**. Every public
  callback type (`ProgressData::ReceiverFnc`, `dirForEach` callbacks, `forEachLine`) is part of the ABI as a boost
  type. A `std::function` converts implicitly when you pass it in, but storing and returning these types stays
  boost.
* Predicates for the old pool query style: `Constant<T>`/`constant(v)`, `True`/`true_c()`, `False`/`false_c()`,
  `Not`/`not_c(c)`, `Or`/`or_c(a,b)`, `Chain`/`chain(a,b)` (logical **AND**, short-circuit).
* Action functors to use with `invokeOnEach` (`zypp/zypp/base/Algorithm.h`): `getFirst(var)` stores the value and
  **returns false** (stops the loop), `getLast(var)` stores and continues, `getAll(outputIterator)`.

### TypeTraits (`base/TypeTraits.h`)
This header puts back-ports **into namespace `std`**: `is_base_of_v`, `is_integral_v`, `is_pointer_v` for
`< C++17`, and `conditional_t`, `decay_t`, `enable_if_t`, `remove_reference_t`, `result_of_t`,
`underlying_type_t` for `< C++14`. It is included via `Easy.h`. The `_v` polyfills are declared
`inline constexpr`, which is itself C++17 syntax, so they would not help a real C++11 compiler. This is one more
reason the "public headers are C++11" claim (§2) doesn't hold.

### DtorReset (`base/dtorreset.h`)
`DtorReset x(var)` restores `var` to its value at construction when it goes out of scope.
`DtorReset x(var, val)` assigns `val` instead. The implementation is a type-erased `shared_ptr<void>`, so copies
share it and the restore happens when the last copy dies. Typical use is re-entrancy flags. `DtorReset(str,
"literal")` doesn't compile (it would assign `char[]`; see the `\todo`).

### CleanerThread (`base/CleanerThread_p.h`, `.cc`, private, `ZYPP_LOCAL`)
* Purpose: **reaping zombies**. When an `AbstractDirectSpawnEngine` (and therefore an `ExternalProgram`) is
  destroyed while its child is still running, `~AbstractDirectSpawnEngine` (`ng/io/forkspawnengine.cc:35`) calls
  `CleanerThread::watchPID(pid)` instead of blocking in `waitpid`.
* `CleanerData::instance()` is an **intentionally leaked** heap singleton (`new`, never deleted) holding a
  **detached** `std::thread` named `"Zypp-Cleaner"`. The thread blocks all signals. While it has PIDs, it polls
  `waitpid(pid, WNOHANG)` every 100 ms; otherwise it waits on a condition variable. A PID is dropped when it has
  exited or `waitpid` fails.
* Consequences: the thread is created by the first `watchPID` call and is never joined, so it shows up in
  thread dumps and at process exit. The child is **not killed**; it is only reaped when it exits by itself.
  It is created lazily, so it is not inherited across `fork()` (as usual, only the forking thread exists in the
  child).

### Measure (`zypp/zypp/base/Measure.{h,cc}`)
* Output format: `START MEASURE(id)`, `ELAPSED(id) real (u user s sys c children) [delta]`,
  `MEASURE(id) …` on stop. Real time comes from `::time()`, so it has **one-second resolution**. User, system and
  children times come from `::times()`.
* Nesting is shown by a **static** `std::string _glevel` that grows by `".."` per active Measure. That is shared
  mutable state, not thread-safe. Output goes to the `"Measure"` log group at DBG, or to a custom
  `std::ostream`.

### WatchFile (`zypp-logic/zypp-core/fs/watchfile.h`)
* Remembers `size` and `mtime` (via `PathInfo`, **whole seconds**). `hasChanged()` compares and updates the
  stored values; `isDirty()` only compares. `WatchFile(path, NO_INIT)` acts as if the file did not exist, so
  the first `hasChanged()` is true when it does exist.
* An edit that keeps the size and happens within the same second as the previous check is **not detected**.
* Users: `sat/detail/PoolImpl.cc:286` and NG `zyppng/lib/zypp/ng/sat/namespaces/filesystem.cc:39`
  (`/etc/sysconfig/storage` for the filesystem namespace), `zypp-curl/proxyinfo/proxyinfolibproxy.cc:134`
  (`/etc/sysconfig/proxy`), `target/TargetImpl.{h,cc}` (base product cache), `repo/PluginRepoverification.cc`.
  Several of these are **function-local `static WatchFile`s**, i.e. process-wide and not thread-safe. The keyring
  cache uses an explicit `setDirty()` in addition (`zypp-logic/zypp-common/private/keyring_p.cc:55`: "don't
  remember why … WatchFile was not enough").

### Glob (`zypp/zypp/Glob.{h,cc}`, legacy only)
`filesystem::Glob` wraps `glob(3)`: `Flags` = `kErr, kMark, kNoSort, kNoCheck, kNoEscape, kPeriod,
kAltDirFunc, kBrace, kNoMagic, kTilde, kOnlyDir, kTildeCheck`. `add(pattern[, flags])` adds `GLOB_APPEND`
automatically and returns `::glob()`'s result. It is iterable (`begin/end` yield `const char *`). There is a
static `Glob::collect(pattern[, flags], outIt)`. `collect` needs an output iterator with a `container_type`
(i.e. `std::back_inserter`), and it adds nothing if glob fails. Used by `RepoInfo.cc:156`.

### ManagedFile (`zypp-logic/zypp-core/ManagedFile.h`)
`using ManagedFile = AutoDispose<const Pathname>;`. Common patterns:
`ManagedFile(path, filesystem::unlink)` (delete when done: `PackageProvider.cc:654`, `MediaCurl.cc:635`,
NG `provide.cc:534`), `ManagedFile(path)` (no dispose: just a path), `TmpFile::asManagedFile(...)`, and
`MediaSetAccess::provideFileFromUrl()`/`provideOptionalFileFromUrl()`. (Plain `MediaSetAccess::provideFile`
returns a `Pathname`.) Copies share the dispose, so the
file lives until the last copy is gone. Call `resetDispose()` to keep the file.

### Filesystem helpers worth knowing (`fs/PathInfo.{h,cc}`)

| Function | Real behaviour |
|---|---|
| `assert_dir(p, mode=0755)` | `mkdir -p`; 0 if it already exists |
| `recursive_rmdir(p)` | own `opendir`/`lstat` walk (it does **not** follow symlinks). Failures of inner `unlink`/`rmdir` are **ignored**; only the final `rmdir` of `p` reports errno. A non-existent `p` is a success |
| `clean_dir(p)` | same walk, but keeps `p` (2015: "Avoid ExternalProgram in clean_dir") |
| `copy_dir(src, dst)` | spawns **`/bin/cp -dR -- src dst`**. EEXIST if `dst/basename(src)` already exists |
| `copy(file, dest)` | spawns **`/bin/cp --remove-destination -- file dest`**; EINVAL if not a file, EISDIR if dest is a dir. **Returns cp's exit status, not errno** |
| `hardlinkCopy(old, new)` | if `old` is a symlink → `copy()` (dereferences it). Otherwise unlink `new`, then `::link`; on **EPERM** (protected_hardlinks, 2014) or **EXDEV** → `copy()` |
| `rename(old, new)` | `::rename`; on **EXDEV** falls back to `/usr/bin/mv` (OverlayFS layers; changes entry "rename: handle EXDEV error on OverlayFS") |
| `exchange(l, r)` | swaps two paths with `::rename` (same filesystem); creates parent dirs |
| `readdir(list<string>/list<Pathname>/DirContent, dir, dots=true)`, `dirForEach`, `dirForEachExt` | `DirEntry{name, FileType}` |
| `checksum(file, algo)`, `md5sum`, `sha1sum` | return an empty string on error. `is_checksum(file, CheckSum)`: **an empty CheckSum always matches** |
| `zipType(file)` | magic-byte detection, see §14.2 |
| `df(path)` | free space as `ByteCount`, -1 on error |

**Not exported:** several declarations in `PathInfo.h` have no `ZYPP_API`: `exchange`, `readlink`,
`copy_file2dir`, `md5sum`, `sha1sum`, `checksum`, `is_checksum`, `chmod*`, `addmod`, `delmod`, `zipType`,
`erase`, `df` and more. With `ENABLE_VISIBILITY_HIDDEN=ON` (the default) these are probably **not callable from
zypper/YaST**. Unverified: check with `nm -DC libzypp.so` before using one of them in an application.

### Date formatting (`zypp-logic/zypp-core/Date.{h,cc}`)
* `form(fmt[, TB_LOCALTIME|TB_UTC])` = `strftime`. `Date(0)` always prints `"0"`. A trailing `+0100`-style
  offset is shortened to `+01`.
* `print(DateFormat, TimeFormat, TimeZoneFormat, TimeBase)`: DateFormat `none|calendar(%Y-%m-%d)|month|year|
  week(%G-W%V)|weekday|ordinal(%Y-%j)`, TimeFormat `none|seconds|minutes|hours`, TimeZoneFormat
  `none|name(%Z)|offset`. `printISO(...)` builds the ISO 8601 form `2014-02-07T07:06:41+01` (a TZ name becomes an
  offset). `asSeconds()` = `%s`. `dumpAsXmlOn()` writes `time_t` plus a text form.
* The internal `LocaleGuard` (`Date.cc:32`) switches **`LC_TIME` with the process-global `setlocale()`** to
  the `.UTF-8` variant of the current locale while formatting/parsing. `doForm()` uses a **static
  `char buf[512]`** and the non-reentrant `localtime`/`gmtime`. **Date formatting and parsing are therefore not
  thread-safe**, and `setlocale` affects every thread at once.

---

## 16. JSON (`zypp-logic/zypp-core/parser/json.{h,cc}`, `json/`)

* **Value model** (`json/JsonValue.h`, namespace `zypp::json`): `Value` is a `std::variant` of
  `Null, Bool, String, Int (int64), UInt (uint64), Number (double), Object, Array`. Implicit ctors exist from
  C++ scalars and strings. `type()`, `isNull()`, `asBool()/asString()/asInt()/…/asObject()/asArray()` (these use
  `std::get`, so a wrong type **throws `std::bad_variant_access`**), `asJSON()` (serialise), `operator==`.
  `Object` is a **`std::multimap<String,Value>`**: duplicate keys are kept. `contains(k)`, `values(k)` (all
  values), `equal_range`, and `value(k)`. **`value(k)` dereferences `find()` without checking for `end()`**
  (`JsonValue.cc:97`), so asking for a missing key is undefined behaviour. Call `contains()` first. `Array` is a
  vector. `toJSON(T&&)` customisation helpers.
* **Parser** (`json/private/jsonparser_p.{h,cc}`, `json::detail::Parser`): a hand-written recursive-descent
  tokenizer on an `InputStream`. It returns `zyppng::expected<Value>` and turns errors into
  `parser::ParseException` exception_ptrs. It handles `\uXXXX` escapes including UTF-16 surrogate pairs, and
  limits nesting to **1000** levels (to avoid stack exhaustion on hostile input). Numbers go through
  `str::strtonum` into Int/UInt/Number.
* **API split** (`json.h`): applications (no `ZYPP_DLL`) see the value types plus `ZYPP_API Value
  parseDocument(const InputStream&)`, which throws. Code inside the library also sees
  `parseDocumentExpected()` and the parser class. Those are deliberately kept out of the ABI because
  `zyppng::expected`'s layout is not frozen.
* `json.h` is a public header that needs C++17 (`std::variant`, `if constexpr`).
* **Users:** `zypp/zypp/repo/RepoMirrorList.cc:194` (JSON mirror lists: a file starting with `[` is treated as
  JSON), `zypp/zypp/target/TargetImpl.cc:152-242` (serialises the `TransactionStepList` for commit plugins as
  a `PluginFrame` body), and `zypp/tests/parser/JsonParser_test.cc`. `zypp/zypp/base/Json.h` is only a forward
  include today. See [Repositories](05-repositories-and-services.md) for mirror lists.

---

## 17. zypp-tui (`zypp/zypp-tui/`, namespace `ztui`)

**What it is:** the terminal UI code that was moved out of zypper so other tools could share it. It is built as
a **static library `libzypp-tui.a`** (`zypp/zypp-tui/CMakeLists.txt`, `-ffat-lto-objects`, links readline),
installed with its headers under `${INCLUDE_INSTALL_DIR}/zypp-tui/` and shipped in the `libzypp-devel`
package (`Provides: libzypp-tui-devel = 1`, `zypp/libzypp.spec.cmake:289,519`).

| Header | Content |
|---|---|
| `Application`/`application.h` | `ztui::Application`: a **thread_local** "current application" pointer, set in the ctor and cleared in the dtor. `Application::instance()` **throws** if none exists. Owns a `Config` and an `Out` (default `OutNormal`); `setOutputWriter()`, `exitCode()` |
| `Config`/`config.h` | `do_ttyout`, `do_colors`, and one `ansi::Color` per semantic role (`color_result`, `color_msgError`, `color_prompt`, `color_positive`, …) |
| `output/Out.h` | abstract `ztui::Out`: `Verbosity`, `Type` flags (`TYPE_NORMAL`, `TYPE_XML`), `info/warning/error`, `progressStart/progress/progressEnd(id, label, …)`, `dwnldProgress*`, `prompt*`, `Out::ProgressBar` RAII, `XmlNode`/`TitleNode`, `TermLine`, list/table writers (`ListFormater`, `TableFormater`, `ListLayout`, `TableLayout`) |
| `output/OutNormal.h` | human-readable terminal implementation. The XML writer (`OutXML`) is **still in zypper** (`zypper/src/output/OutXML.cc`) |
| `output/AliveCursor.h` | the `- \ \| /` spinner |
| `output/PromptOptions` | parses option strings such as `"y/n/p"` with a default and per-option help; `isYesNoPrompt()` compares with translated `_("yes")`/`_("no")` |
| `Table.h` | `Table`, `TableRow`, `TableHeader`, `PropertyTable`, line styles, column styles, sorting |
| `utils/richtext.h` | `processRichText`/`printRichText`: converts a small HTML subset (`br`, `p`, `ul`, `li`, …) from patch/pattern descriptions to plain text. It keeps state in namespace-scope globals (`_rtTagmap`, `count_list_items`), so it is not reentrant |
| `utils/{ansi,colors,console,text}.h`, `output/Utf8.h` | SGR colour strings, `get_screen_width()` (isatty, `$COLUMNS`, ioctl), `readline_getline()`, UTF-8-aware text wrapping (`mbs::MbsIterator…`) |

* **Consumers:** **zypper** is the main one. `class Zypper : public ztui::Application` (`zypper/src/Zypper.h:130`),
  `struct Config : public ztui::Config`, and zypper's `src/output/Out.h`, `Table.h`, `utils/*.h` are mostly
  `#include <zypp-tui/…>` plus `using ztui::…` re-exports. In libzypp only the opt-in devtool
  `zypp/tools/refresh-ng` (`ENABLE_DEVTOOL_REFRESHNG`, default OFF) uses it. That tool includes
  `<zypp-core/zyppng/ui/ProgressObserver>`, a path that no longer exists, so it has probably bit-rotted
  (not built here).
* **Linking:** `zypp/zypp/CMakeLists.txt:796` also links `zypp-tui` into libzypp ("link zypp-tui.a at last, to
  prevent undefined references in case tui functions are used in libzypp"). zypper links its own copy through
  `ZYPP_TUI_LIBRARY` (`cmake/modules/ZyppConfig.cmake`). No `ztui` symbol is marked `ZYPP_API`.
* **Stability:** every header says *"This file contains private API, this might break at any time between
  releases. Strictly for internal use!"*. There is no ABI promise because it is a static lib. In practice,
  zypper must be rebuilt against a matching libzypp-devel (zypper's spec has `BuildRequires: libzypp-tui-devel
  >= 1`).
* **Translations:** zypp-tui is compiled with `-DZYPP_DLL -DTEXTDOMAIN="zypp"`, and `po/CMakeLists.txt` scans
  `zypp/zypp-tui/*`. So strings like `_("Note:")` in zypp-tui are translated through the **libzypp** catalog
  (via `zypp::gettext::dgettext`), even when the code runs inside zypper.

---

## 18. Gettext and translations in code

* **Macros** (`base/Gettext.h`): inside the library (`ZYPP_DLL`), `_()` → `zypp::gettext::dgettext`,
  `PL_(s,p,n)` → `zypp::gettext::dngettext`, and `N_()` is a no-op marker. `Gettext.cc` binds `TEXTDOMAIN` to
  `LOCALEDIR` with codeset UTF-8 on the first call (a plain `static bool initialized`, with a benign race).
  Every zypp-logic `*.cmake`, `zypp/zypp-tui` and `zyppng/api/CMakeLists.txt` define
  `-DTEXTDOMAIN="zypp" -DLOCALEDIR=… -DZYPP_DLL`.
* **Inline code in installed headers is expanded in the application's context.** There, `ZYPP_DLL` is not
  defined, so `_()` becomes `::gettext` (the application's default domain) unless the app defined its own `_`.
  A `_("...")` inside an inline function or template in a public header is therefore looked up in **zypper's**
  catalog, not zypp's. Put translatable strings in `.cc` files.
* **Plurals:** use `PL_("%d package", "%d packages", n)` and then format with `str::form`/`str::Format`. Don't
  build plurals with `n == 1 ? _("…") : _("…")`, because many languages have more than two forms.
* **Deferred translation:** `N_("…")` marks a string for extraction and stores the msgid. Translate it later with
  `_(msgid)` (e.g. the language/country name tables in `zypp-logic/zypp/LanguageCode.cc:149+`).
  `Exception::asUserString()` runs `_()` on the `dumpOn` text, so an exception message that is an exact
  `N_()` msgid gets translated when it is shown (§4).
* **Translator comments:** `po/CMakeLists.txt` calls xgettext with `--add-comments` (no tag), so **the comment
  immediately before the string is copied into the .pot**. The code uses `// translators:`, `// Translator:`
  and similar, without a fixed spelling. Keep the comment directly above the line with `_()`, and explain the
  placeholders.
* **Extraction:** `xgettext -L C++ --boost --keyword=_ --keyword=_:1,2 --keyword=__ --keyword=N_
  --keyword=PL_:1,2` over `zypp/zypp`, `zypp/zypp-common`, `zypp/zypp-tui`, `zypp/shared`, `zypp-logic` and
  `zyppng` `*.h`/`*.cc`. **`.cppm` module files are not in the glob.** No `.cppm` contains `_("` yet, but once one
  does, its strings won't be extracted.
* **Files:** `po/zypp.pot` (committed) and `po/<lang>.po` (~63 languages) in the libzypp tree. The `translations`
  target (always built when `ENABLE_BUILD_TRANS=ON`) msgmerges each `.po` with the freshly generated POT and runs
  msgfmt into `share/locale/<lang>/LC_MESSAGES/zypp.mo`. `potfile_update` checks for and commits POT changes
  (`PotfileDiff.sh`/`PotfileUpadte.sh`). The upstream translation workflow (e.g. Weblate sync) is not visible
  here (unverified).
* zypper has its own domain and catalog (`zypper/po`). Strings from libzypp, `Exception::asUserString`, or
  zypp-tui that reach the user are translated with the zypp catalog.

---

## 19. Process-wide singletons, static init and shutdown (legacy)

Almost all legacy singletons are **function-local statics** ("magic statics", constructed on first use). Their
destruction order at `exit()` is the **reverse of construction-completion order**, so it depends on what the
application touched first.

| Singleton | Where | Lifetime notes |
|---|---|---|
| `ZConfig::instance()` | `ZConfig.cc:796` `static ZConfig _instance` | Built on first use; parses zypp.conf (`MIL "ZConfig singleton created."`). Holds `MediaConfig & _mediaConf = MediaConfig::instance()`, so MediaConfig finishes construction first and is destroyed after ZConfig, which is safe |
| `MediaConfig::instance()` | `zypp-logic/zypp-media/mediaconfig.cc:46` | function static |
| ZYpp | `ZYppFactory.cc:313-456` | `static weak_ptr<ZYpp> _theZYppInstance` + `static scoped_ptr<ZYppGlobalLock> _theGlobalLock` (namespace scope). `getZYpp()` creates `ZYpp` around a function-static `ZYpp::Impl_Ptr _theImplInstance` ("for now created once"). `~ZYpp` only **releases the zypp.pid lock**; the Impl (Target, `KeyRing` created on `tmpPath()` in `ZYppImpl.cc:126`, …) **survives until static destruction**, and a later `getZYpp()` reuses it |
| `sat::Pool` / `PoolImpl` | `sat/detail/PoolMember.cc` | `static PoolImpl _global; static Guard _guard;` The Guard is built after the pool and destroyed before it; its dtor sets `poolDestroyed()`. After that, `PoolMember::myPool()` **throws** "Global PoolImpl instance used after it was destroyed!", and `poolValid()` lets dtor-time code skip the pool (`ZConfig.cc:1136`, `TargetImpl.cc:145` `sigMultiversionSpecChanged`). Added in commit c86cdf606 ("fix the problematic crash") |
| `ResPool::instance()` | `ResPool.h` | explicitly shared handle on the PoolImpl |
| `MediaManager` | `media/MediaManager.cc:299` | **class-static** `RW_pointer<MediaManager_Impl> m_impl`, filled by the first `MediaManager()`. Every `MediaManager` object is a handle. `~MediaManager_Impl` detaches dependent (ISO) handlers first and then all media (umount), so **media are released at static destruction** if the app didn't close them. Its position in the destruction order relative to function statics depends on the boost shared_ptr init kind (unverified) |
| `KeyRing` | owned by `ZYpp::Impl::_keyring` | see the ZYpp row |
| `Locks::instance()` | `Locks.cc:38` | function static |
| `RepoVarsMap::instance()` | `repo/RepoVariables.cc:396` | function static. The comment says it should be per context and "reflects the weakness of the classic ZConfig singleton". `repoVariablesReset()` runs in every `ZYpp` ctor |
| `Modalias` | `target/modalias/Modalias.cc:225` | function static |
| `LogThread` | `LogControl.cc:95` | function static; its dtor `stop()`s the thread. `LogControlImpl` is `thread_local` (§3.3) |
| `CleanerData` | `CleanerThread.cc` | **leaked on purpose** (`new`, never deleted), detached thread (§15) |
| `DistributeReport<T>::instance()` | `Callback.h:204` | function static per report type (§11) |
| `ztui::Application` | `zypp-tui/application.cc:17` | `thread_local` raw pointer |

**Code that runs at load time (static init) of libzypp.so:**
* `ZYppFactory.cc:52-71`: `SigBacktraceHandler<SIGSEGV>` and `<SIGABRT>` are explicit template instantiations
  whose static member initialiser calls `::signal()`. **Loading libzypp installs SIGSEGV and SIGABRT handlers**
  that log `INT "Error: signal N"` plus `dumpBacktrace`, call `LogControl::emergencyShutdown()` and then restore
  the previous handler (changes: "Add SIGSEGV handler trying to log a stack trace", "Log backtrace on SIGABRT
  too", 2022). An application that installs its own handler *before* dlopen/load gets it replaced (the old one is
  re-armed only after the first signal). One that installs its handler afterwards overrides libzypp's.
* `Measure::Impl::_glevel`, `MediaManager::m_impl`, `_theZYppInstance`/`_theGlobalLock` and zypp-tui's
  `_rtTagmap` are namespace/class-scope objects with dynamic init or non-trivial dtors. Using them from another
  TU's static initialiser is subject to the static-init-order fiasco. Historic fixes: "Static initialization
  problem fixes (by Harald Fernengel)", "Protect against race when destructing globals" (14.29.4, 2014), "Do
  not fork in a global destructor, perl dislikes it (#182672)".

**Shutdown rules of thumb:**
* Logging from static destructors is safe (null stream, §3.3). Using `sat::Pool` after it is destroyed throws.
  Other singletons don't guard against use after destruction.
* Don't spawn processes or touch media from global destructors (see #182672). `MediaManager_Impl`'s dtor still
  umounts, and that can run external `umount`.
* Applications should drop their `ZYpp::Ptr`, call `MediaManager` release/close and stop their own threads
  before `exit()`. Calling `exit()` from a non-main thread while the cleaner or log thread is running is
  untested (unverified).
* A forked child inherits the singletons' memory but not their threads. The log thread is handled via
  `pthread_atfork`; the cleaner thread and MediaManager state are not.

---

## Pitfalls & special cases

* `DBG` is **not** level 0: it logs at `E_MIL` (`<1>`) into group `"<group>++"`. Grep for `++]` to find debug
  lines. `E_DBG` is never used by the macros.
* No log file is written unless the application calls `LogControl::logfile()` or `ZYPP_LOGFILE` is set. zypper
  defaults to `/var/log/zypper.log`.
* The LineWriter is process-global (owned by the log thread), but the LineFormater, `TmpExcessive` and the
  `ZYPP_FULLLOG` state are per thread (`thread_local LogControlImpl`).
* After `fork()` the child logs nothing (`pthread_atfork` → `notifyFork`). Code that logs during static
  destruction gets a null stream (`logControlValidFlag`).
* Logging goes through an abstract unix socket `zypp-logsocket-<pid>` to the "Zypp-Log" thread. Changes in
  `zypp-logic/zypp-core/ng/` (EventLoop/Socket) can break all logging.
* Every `ZYPP_THROW`/`CAUGHT`/`RETHROW` writes an **INT** line including the full history. INT lines don't
  necessarily mean bugs, and log size grows with deep exception chains.
* `ZYPP_THROW(e)` throws a copy of the **static** type of `e`. Inside a `catch (Exception & e)` use
  `ZYPP_RETHROW(e)`, or the exception gets sliced.
* `Exception::remember(old)` **replaces** the current history with old.asUserString() + old.history.
  `remember(std::string)` only appends.
* `Exception::what()` is private; use `msg()`, `asString()` or `asUserHistory()`.
* `ReferenceCounted`'s counter is not atomic. Don't share `intrusive_ptr` objects across threads.
* `RWCOW_pointer`: any non-const access clones a shared Impl, so take a const reference for read-only paths.
* `zypp::shared_ptr` is `boost::shared_ptr` and is part of the ABI (e.g. `LogControl::setLineWriter`).
* `Pathname("foo")` is stored as `"./foo"`, and `"a" / "/b" == "a/b"` (an absolute right-hand side does not
  reset the path).
* `filesystem::*` functions return errno instead of throwing; always check the int result.
* `TmpPath::defaultLocation()` is `/var/tmp/zypp.tmp` for root (bsc#1249435) and plain `$ZYPPTMPDIR`//var/tmp
  otherwise. It is computed once.
* Url passwords and `proxypass=` are hidden in `asString()`/logs (bsc#1050625, CVE-2017-9271). Writing config
  files needs `hotfix1050625::asString()`. `operator==` compares the complete string, password included.
* `ExternalProgram` with `root == "/"` only does `chdir("/")` and no chroot. Pass `default_locale=true` when you
  parse output. A child killed by a signal returns 128+sig.
* `ZYPP_FORK_BACKEND=gspawn` switches to the glib spawn engine (but `use_pty` always forces fork).
* `IniParser` rejects keys containing `: / ? | , \`. Duplicate keys overwrite each other in `IniDict`.
* Callback receivers are global singletons per report type, with no locking. Their cross-DSO identity depends on
  `ZYPP_API` visibility and `libzypp.map`. Adding a virtual method to a report breaks ABI; use UserData /
  `ContentType` instead.
* `SendReport` construction and destruction already fire `reportbegin`/`reportend`, so don't create one
  speculatively.
* `UserData::get<T>` needs the exact stored type (`bad_any_cast` otherwise). A const `set()` only fills empty
  keys.
* `_()` translates with the "zypp" domain only inside the library (`ZYPP_DLL`). In applications it is the
  application's own gettext.
* `TriBool`: `indeterminate == indeterminate` is not true. Use `indeterminate(x)`.
* Public headers claim C++11 but already require C++17 (`is_base_of_v`, `if constexpr`, `is_pointer_v`).
* `ProgressData` sends at most once per 100 ms, and only when the integer percent changes or 1 s has passed.
  It has no timer: a stalled producer sends nothing. INIT and END are always sent (bsc#1206949). The dtor
  sends END only if a report was sent before.
* `ProgressData::reportValue()` computes `val*100/(max-min)` and **ignores `min`**. A range like `[50,150]`
  gives 50…150 "%". `CombinedProgressData` likewise assumes the sub-task starts at 0 (`_last_value` starts at
  0, not `min`), and it truncates each increment to an integer. Many small sub-steps on a small parent range
  add `0` every time, so the parent never moves.
* A receiver returning `false` only aborts where the producer checks the bool. NG-backed legacy calls
  (`RepoManager` via `ProgressObserverAdaptor`, `ProgressObserver::makeProgressDataReceiver`) **ignore it**.
  The NG adaptor also creates a new `ProgressData`, and so a new `numericId()`, on every update, which shows up
  in `zypper --xmlout` progress ids.
* `InputStream(path)` decompresses only gzip and zchunk. `.zst`, `.xz` and `.bz2` are read **raw** without
  an error. Compressed repo metadata reaches libsolv through repo2solv/`solv_xfopen`, not through InputStream.
  `InputStream::size()` is -1 for compressed files, so progress shows still-alive ticks instead of percent.
* `iostr::getline` (used by `EachLine`, `forEachLine`, `simpleParseFile`, IniParser and others) reads through a
  function-static buffer, so **parallel line parsing in two threads is a data race**. It also keeps a trailing
  `\r`.
* `Date` formatting/parsing is not thread-safe (static buffer, `localtime`, process-global
  `setlocale(LC_TIME)`). `operator<<(Date)` is locale-dependent (`%c`), unlike `print()`.
* `json::Object::value(key)` with a missing key is UB (dereferences `end()`). `Value::asX()` throws
  `std::bad_variant_access` on a type mismatch. Objects keep duplicate keys.
* `filesystem::copy`/`copy_dir` spawn `/bin/cp` and return **cp's exit code**, not an errno. `recursive_rmdir`
  ignores failures on inner entries. `is_checksum(file, CheckSum())` is always true. Several `PathInfo.h`
  functions lack `ZYPP_API`.
* `WatchFile` works with 1-second mtime resolution plus size. Same-size rewrites within a second go unnoticed.
* `zypp::function` is `boost::function`: callback types in the public API are boost types.
* Loading libzypp.so installs SIGSEGV/SIGABRT handlers at static-init time (`ZYppFactory.cc:52`).
* `ZYpp::Ptr` going away releases only the lock; Target, KeyRing and the pool live on until static
  destruction. `sat::Pool` use during/after static destruction throws.
* zypp-tui is a static, explicitly *private* API. zypper must be rebuilt with the matching libzypp-devel.
  `ztui::Application::instance()` throws if no Application exists on **this thread** (thread_local).
* `_()` inside inline code of an installed header is looked up in the *application's* text domain. Strings
  in `.cppm` files are not extracted into `zypp.pot`.
* `CleanerThread` only reaps children of destroyed spawn engines, it never kills them. A hung child stays
  around as a running process.

## Where to start when debugging

* **Get a full log:** `ZYPP_LOGFILE=/tmp/z.log ZYPP_FULLLOG=1 zypper …` (or `ZYPP_LOGFILE=-` for stderr). Note: zypper's `-v` only affects console output, not the log.
  Add `ZYPP_LIBSOLV_FULLLOG=1` for solver internals and `ZYPP_MEDIA_CURL_DEBUG=1` for downloads.
* **Read a log:** filter by group (`grep '\[zypp::exec\]'` for every spawned command and its exit status,
  `\[zypp::repomanager\]`, `\[zypp-core++\]` for debug) and level (`'<5>'` = INT, which includes all exception
  throw/catch lines). Follow an exception by its code location; THROW, RETHROW and CAUGHT lines show how it
  travelled.
* `{T:…}` tags show work done off the main thread (media workers, log thread). A PID different from zypper's
  means a forwarded log from a helper tool in forwarding mode (ppid).
* A `[DEFINE_LOGGROUP]` group means a source file missing from its `SET_LOGGROUP(...)` list in the
  `*.cmake` file.
* For temporary local tracing, use `BLOCKTRACE("x")` / `TRACE("x")` / `OSD << …`. They print colored output to
  stderr; remove them before committing.
* Unit tests for this layer are in `zypp/tests/zypp/` and `zypp/tests/zypp/base/` (e.g. `Pathname_test.cc`,
  `Url_test.cc`, `ExternalProgram_test.cc`, `Flags_test.cc`, `UserData_test.cc`, `PtrTypes_test.cc`). Run them
  with `ctest -R Pathname` and similar from the build directory. See [Testing](11-testing.md).
* When a callback never fires: check that the receiver called `connect()` and is still alive (its dtor
  disconnects), whether a `TempConnect` in libzypp replaced it, and whether the symbols are exported
  (`nm -DC libzypp.so | grep DistributeReport`).
* When a URL looks wrong in a log: remember it is printed without the password or `proxypass`. Use
  `asCompleteString()` only in local debugging and never log it.
* **Progress looks stuck or jumpy:** grep the log for `[Progress++]` lines `{#id|name} START` / `END`, which
  bracket each ProgressData's life. "User request to ABORT pending action." at `<2>` means a receiver returned
  false. Then check whether the call site honours the result (NG paths don't). Look at
  `zypp/tests/zypp/ProgressData_test.cc` for expected sequences.
* **Parser reads garbage from a compressed file:** run `filesystem::zipType()` on it, or check the magic bytes
  with `od -c | head -1`. zstd/xz aren't supported by InputStream. Parse through libsolv, or decompress first.
* **Odd results under threads:** suspect the known static-state helpers first: `iostr::getline`, `Date::form`,
  `Measure`, function-static `WatchFile`s, `ReferenceCounted`, `richtext`.
* **Crash at exit:** check the singleton table in §19. Typical causes are using `sat::Pool` or `MediaManager`
  from a global destructor, or a callback receiver object that is destroyed after `DistributeReport`.
  Run with `ZYPP_LOGFILE=-` to see the last `ZYpp is off...` / `INT Error: signal` lines and the backtrace
  that libzypp's own SIGSEGV/SIGABRT handler logs.
* **Missing translation:** check whether the string is in `po/zypp.pot` (rebuild with `make translations` and
  `ENABLE_BUILD_TRANS=ON`), whether the code runs with `ZYPP_DLL` (library) or in an app header inline, and
  whether `share/locale/<lang>/LC_MESSAGES/zypp.mo` is installed. Use `LANGUAGE=de zypper …` to test.
* **zypp-tui changes:** build zypper from the same stack (`zypp-stack` top-level build) because it statically
  links `libzypp-tui.a`. There is no ABI check for zypp-tui.
