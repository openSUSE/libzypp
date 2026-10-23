# 10 – The Next Generation (NG) Code: zyppng, zypp-logic/*/ng, libzypp-glib

> **Status (Oct 2026): development of `zyppng/` and libzypp-glib is paused.**
> The NG code that already runs **inside legacy libzypp** (`zypp-logic/*/ng`,
> the `zypp/zypp/ng/` RepoManager/workflows, MediaCurl2 / NetworkRequestDispatcher,
> the logging transport, the zypp-rpm protocol) is production code and is
> maintained like legacy code. The rest of this chapter documents the design so
> that work can resume later.

This chapter covers the "NG" code. That is the async framework in `zypp-logic/*/ng`
(event loop, signals, IO, pipelines, coroutines), the NG workflows that legacy
libzypp already runs internally, the C++20-module rewrite in `zyppng/`, the
experimental `libzypp-glib` GObject API and the `zypp-media-*` worker
executables. It explains which parts are **in production** and which are
**experimental**. It also explains the "write the logic once, run it sync or
async" pattern and the migration rules from `AGENTS.md`.

> Status summary (verified against the tree, Oct 2026, libzypp 17.38.16):
> * **Production (inside `libzypp.so`)**: the `zypp-logic/zypp-core/ng`
>   framework (EventLoop, Timer, Process, AsyncDataSource, StompFrameStream,
>   …), `zypp-logic/zypp-curl/ng/network` (NetworkRequestDispatcher), the
>   *sync* NG workflows in `zypp/zypp/ng/` (RepoManager, refresh, keyring,
>   checksum, signature, services) and the `zyppng::sat::StringPool`.
> * **Experimental / not shipped with libzypp**: everything under `zyppng/`.
>   That is the C++20 module `zyppng` (NG sat Pool, Context, config), the async
>   `Provide` with worker processes, the `zypp-media-*` workers and
>   `libzypp-glib`. They are built only with `-DBUILD_LIBZYPPNG=ON` and are
>   shipped only by the separate `libzypp-glib` package (0.1.0, pre-1.0, no
>   stable ABI).

---

## 1. Why NG exists, and the tiers

`AGENTS.md` (repo root) defines three tiers. The dependency direction is strictly
enforced:

```
zyppng/      Tier 1  NG rewrite. C++20 (C++23-forward), single named module `zyppng`.
                     The ONLY place new application logic goes.
zypp-logic/  Tier 2  Shared sources compiled into BOTH trees. Frozen, EXCEPT the
                     */ng/ subtrees (zypp-core/ng, zypp-media/ng, zypp-curl/ng,
                     zypp/ng), which may change freely if tests stay green and
                     NG callers are updated.
zypp/        Tier 3  Legacy libzypp.so. Read-only: no features, no refactoring,
                     ABI/API stability for zypper/YaST/PackageKit.

zyppng → zypp-logic     OK
zypp   → zypp-logic     OK
zyppng → zypp           FORBIDDEN
zypp   → zyppng         FORBIDDEN
```

The root `CMakeLists.txt` explains why a single source tree produces two
libraries: *"we can share code … and also be able to recompile the shared code
with different compiler flags... e.g. coroutine support on/off"*. That last
point is the core trick of this chapter (see §4).

Other rules from `AGENTS.md` that you must enforce in reviews of NG code:

* Errors that depend on data (files, network, user input, config) use
  `zyppng::expected<T,E>` or `std::optional`. Do not add new exception-based
  error paths. Programmer errors may throw `std::logic_error`-family exceptions.
* No raw `new`/`delete`, `malloc`/`free`. Use `std::unique_ptr` /
  `std::shared_ptr`. (The existing `zypp-logic/*/ng` code still uses
  `new XPrivate` in its PIMPL constructors. That is grandfathered.)
* No new Boost in `zyppng/` where std or a header-only C++20 alternative
  exists. (`zyppng/lib/zypp-media/ng/provide.h` still uses `boost::any`.)
* NG components must be thread-aware.
* Every `zyppng/` file is part of the named module (see §7).

---

## 2. Directory map

| Path | Tier | Compiled into | Content |
|---|---|---|---|
| `zypp-logic/zypp-core/ng/base` | 2 | both | `Base`/`BasePrivate`, signals, `EventDispatcher` (glib), `EventLoop`, `Timer`, `SocketNotifier`, `UnixSignalSource`, `ThreadData`, `statemachine.h`, `zyppglobal.h` macros |
| `zypp-logic/zypp-core/ng/io` | 2 | both | `IODevice`, `AsyncDataSource`, `Process` (+ fork/gspawn engines), `Socket`, `SockAddr`, `IOBuffer` |
| `zypp-logic/zypp-core/ng/thread` | 2 | both | `Wakeup`, `AsyncQueue`, `AsyncQueueWatch` |
| `zypp-logic/zypp-core/ng/pipelines` | 2 | both | `expected`, `and_then`/`or_else`/`inspect`/`collect`, `mtry`, `transform`, `lift`, `redo`, `wait`/`join`. Sync parts here; async overlays are pulled in via `#ifdef ZYPP_ENABLE_ASYNC` |
| `zypp-logic/zypp-core/ng/meta` | 2 | both | `type_traits.h` (`is_instance_of_v`, `always_false_v`), `function_traits.h`, `functional.h` |
| `zypp-logic/zypp-core/ng/rpc` | 2 | both | `StompFrameStream`, the STOMP framing used by zypp-rpm and the media workers |
| `zypp-logic/zypp-core/ng/ui` | 2 | both | `UserInterface`, `UserRequest` (+ `ShowMessageRequest`, `ListChoiceRequest`, `BooleanChoiceRequest`), `ProgressObserver` |
| `zypp-logic/zypp-core/ng/core` | 2 | both | thin NG aliases: `ByteArray`, `Url`, `IdString`, `String`, `Env` |
| `zypp-logic/zypp-core/ng/config` | 2 | both | generic typed key/value `zyppng::config::Config` engine (NG zypp.conf). Compiled into legacy but not used by legacy logic |
| `zypp-logic/zypp-curl/ng/network` | 2 | both | `NetworkRequest`, `NetworkRequestDispatcher` (curl multi on the event loop), `ZckLoader`, `TransferSettings`, `AuthData` |
| `zypp-logic/zypp-media/ng` | 2 | both | `ProvideSpec` (`ProvideMediaSpec`/`ProvideFileSpec`), `LazyMediaHandle<T>`, `HeaderValueMap`, `providefwd.h`, `config/mediakeys` |
| `zypp-logic/zypp/ng` | 2 | both | only `sat/stringpool.{h,cc}` (**LEGACY-REQUIRED**) and `config/zyppconfig.{h,cc}` (domain key tokens) |
| `zypp/zypp-core/ng/async/*.h`, `zypp/zypp-core/ng/pipelines/operators.h` | 3 | legacy | **sync overlay headers** (`MaybeAwaitable<T> = T`, `zypp_co_await` = nothing) |
| `zypp/zypp/ng/` | 3 | legacy | sync `Context` facade, sync `Provide` facade over `MediaManager`, NG `RepoManager`, `repo/` (Downloader, RefreshContext, workflows rpmmd/susetags/plaindir/services/repomanager/repodownloader), `workflows/` (keyring, checksum, signaturecheck, download, repoinfo, `logichelpers.h`), `reporthelper`, `userrequest`, `progressobserveradaptor` |
| `zyppng/lib/zypp-core/ng/async`, `zyppng/lib/zypp-core/ng/pipelines/operators.h` | 1 | NG | **async overlay headers**: `Task<T>`, `Awaitable` concepts, `IOTaskAwaiter`, signal awaiter, async pipeline operators, `coro_poll`/`join` |
| `zyppng/lib/zypp-media/ng` | 1 | NG | async `Provide`, `ProvideItem`, `ProvideQueue`, `ProvideMessage` protocol, `MediaVerifier`, worker base classes `ProvideWorker`/`MountingWorker`/`DeviceDriver` |
| `zyppng/lib/zypp/ng/*.cppm, *.cc` | 1 | NG | the `zyppng` C++20 module: bridge partitions, NG `sat::Pool` and friends, `log/`, `Context`, `ContextConfig`, `ApplicationData` |
| `zyppng/lib/shared/tvm` | 1 | NG tests | settings for the test VM worker |
| `zyppng/api/zypp-glib` | 1 | `libzypp-glib.so` | GObject C API (currently only `ZyppApplication`) |
| `zyppng/tools/zypp-media-*` | 1 | worker executables | http, disc, nfs, smb, dir, disk, iso, copy, chksum, tvm (tvm is test-only, not installed) |
| `zyppng/tests` | 1 | ctest | async (Task, Pipelines), config, media (Provider), sat (PoolNg, PoolComponentSet) |
| `zypp/tests/zyppng` | 3 | ctest (legacy build) | EventLoop, IOBuffer, UnixSignalSource, io/{AsyncDataSource,Process,UnixSocket}, media/{EvDownloader,NetworkRequestDispatcher}, rpc/StompMessageStream |
| `zyppng/packaging` | – | – | `libzypp-glib.spec`, `.changes`, `libzypp-glib-version.inc` |

**Include path overlay**: `zypp-logic/logic-helper.cmake` (`zypp_logic_setup_includes`)
puts the *tier's own* source dir (`zypp/` or `zyppng/lib/`) **before**
`zypp-logic/` on the include path. So `#include <zypp-core/ng/async/task.h>` in a
shared file resolves to `zypp/zypp-core/ng/async/task.h` (sync) in the legacy
build, and to `zyppng/lib/zypp-core/ng/async/task.h` (async) in the NG build.
The same applies to `zypp-core/ng/pipelines/operators.h`. Remember this when
you "go to definition" in an IDE. It shows you only one of the two.

---

## 3. The async framework (`zypp-logic/zypp-core/ng`)

### 3.1 PIMPL and object macros (`ng/base/zyppglobal.h`)

These macros follow the Qt model:

* `ZYPP_DECLARE_PRIVATE(Class)` / `ZYPP_IMPL_PRIVATE(Class)` generate `d_func()`.
  `Z_D()` gives `auto const d = d_func()`. `ZYPP_DECLARE_PUBLIC` / `Z_Z()` are the
  back-pointer from the private class.
* `zyppng::Ref<T>` = `std::shared_ptr<T>`, `WeakRef<T>` = `std::weak_ptr<T>`.
  `ZYPP_FWD_DECL_TYPE_WITH_REFS(T)` declares `T`, `TRef`, `TWeakRef`.
* `ZYPP_ADD_CREATE_FUNC(Class)` + `ZYPP_DECL_PRIVATE_CONSTR[_ARGS]` /
  `ZYPP_IMPL_PRIVATE_CONSTR[_ARGS]` give a public `create(...)` that calls
  `make_shared` with a private tag type. That forces heap/shared_ptr ownership.

### 3.2 `Base` and signals (`ng/base/base.h`, `ng/base/signals.h`)

`zyppng::Base` derives from `sigc::trackable` and `enable_shared_from_this`.
It provides:

* A parent/child tree. The parent holds strong refs to its children, and each
  child holds a weak ref to its parent (`addChild`/`removeChild`/`findChildren<T>`).
* `threadId()`, the thread the object was created in.
* `setData/data/clearData(quark)`, used by the glib layer to attach the GObject
  wrapper.
* `connect(...)` / `connectFunc(..., trackers...)`. These wrap the slot in
  `internal::lock_shared`, which **locks a `shared_ptr` to the receiver (and any
  extra trackers) for the duration of the slot call**.

The golden rule, from the doc comment: *"Always own a reference to the object you
use"*. A slot may drop the last reference to the sender or receiver. If locking
fails (`bad_weak_ptr`, object already being destroyed), the emit is ignored with
an `ERR` and **`assert(false)` in non-NDEBUG builds**.

`Signal<R(T...)>` wraps `sigc::signal`. Its destructor asserts that it is not
being emitted. With `-DENABLE_SIGC_BLOCK_WORKAROUND=ON`
(`LIBZYPP_USE_SIGC_BLOCK_WORKAROUND`) it blocks the signal instead of clearing it
(GNOME bug 784550, segfault with old sigc++). `MemSignal` additionally locks the
host with `shared_from_this()` on `emit()`. `SignalProxy<>` exposes only
`connect`. A `sywu` backend exists behind `ENABLE_SYWU`, but nothing in CMake
enables it.

### 3.3 Event loop (`eventdispatcher_glib.cc`, `eventloop_glib.cc`, `threaddata.cc`)

```
ThreadData::current()  (thread_local)
   └─ weak_ptr<EventDispatcher>   ← ensureDispatcher(GMainContext* = nullptr) creates it
          │                          (MIL "Creating the Event Dispatcher for thread")
          ├─ GMainContext (own, or the one passed in — g_main_context_ref)
          ├─ GAbstractEventSource per AbstractEventSource (unix fds)
          ├─ GLibTimerSource per running Timer
          ├─ idle source: invokeOnIdle() queue + unrefLater() list
          ├─ g_timeout sources from invokeAfter(cb, ms)
          └─ child watches: trackChildProcess(pid, cb)
EventLoop::create()  → GMainLoop on that dispatcher's context.
   run(): push_thread_default(ctx); g_main_loop_run; on exit clearUnrefLaterList + pop
```

Key facts:

* There is **one dispatcher per thread**, and `ThreadData` holds it only by
  **`weak_ptr`**. Whoever holds the `EventDispatcherRef` (an `EventLoop`, the
  NG `Context`, `MediaNetworkRequestExecutor::_evDispatcher`, `ZyppApplication`)
  keeps it alive. When the last ref goes, the dispatcher and all its glib
  sources are destroyed.
* `EventDispatcher::instance()` returns null (with an `MIL`) if no dispatcher
  exists. `AbstractEventSourcePrivate` and `TimerPrivate` **throw**
  `"Creating ... without a EventDispatcher instance is not supported"`. They keep
  a *weak* ref to the dispatcher that was current when they were created. If
  that dispatcher dies, `Timer::start()` silently does nothing (`//if ev is null
  we are shutting down`).
* `ensureDispatcher(ctx)` ignores `ctx` if a dispatcher already exists (logs
  "This might be a bug!"). `setThreadDispatcher()` warns and ignores the call if
  a dispatcher exists.
* To integrate with a host glib main loop (GTK, Qt-glib, PackageKit), create the
  dispatcher first with the host's `GMainContext`. The constructor does **not**
  call `g_main_context_push_thread_default` (it is commented out, "Enable this
  again once we switch to a full async API").
* `run_once()` = `g_main_context_iteration(ctx, false)`. `waitForFdEvent()` is a
  static blocking `g_poll` helper that is EINTR-safe.
* `invokeAfter(cb, 0)` is how coroutines are resumed "on the next loop
  iteration" (see §5). The callback returns `bool` (true = run again).
* Removing fd watches during dispatch marks them orphaned (`tag=nullptr`). They
  are erased in `check()`, so the dispatch iterator stays valid.

### 3.4 IO, processes, threads

* `IODevice` → `AsyncDataSource` (read/write fds with channels) → `Process`
  (`sigStarted`, `sigFailedToStart`, `sigFinished(int)`, per-channel read
  lines). `Socket` is a unix/inet socket on top of `IODevice`.
* Spawn engine (`ng/io/abstractspawnengine.cc`): env **`ZYPP_FORK_BACKEND`** =
  `pfork` (default, also `auto`/empty) or `gspawn` (glib spawn). The glib engine
  only exists if `ZYPP_HAS_GLIBSPAWNENGINE` (`forkspawnengine_p.h`).
* `Wakeup` is a pipe-based, signal-safe cross-thread wakeup.
  `AsyncQueue<Msg>` is a mutex+condvar queue. `AsyncQueueWatch` turns new
  messages into an event-loop signal.
* `StompFrameStream` (`ng/rpc`) reads and writes `zypp::PluginFrame`s on an
  `IODevice`. It is the wire format of **zypp-rpm** (single-rpmtrans commit,
  chapter 08) and of the **media workers** (§8).
* `linuxhelpers_p.h`: `eintrSafeCall`, `blockSignalsForCurrentThread`, used
  widely in legacy (`ZYppImpl.cc`, `zypp-rpm/main.cc`, `MediaCurl2.cc`).

### 3.5 Pipelines and `expected` (`ng/pipelines`)

`zyppng::expected<T, E = std::exception_ptr>` is a home-grown pre-C++23
expected (based on Ivan Čukić's code). It has `success()`/`error()` factories,
`unexpected<E>`, `get()` (throws `logic_error` on error) and `unwrap()` (rethrows
the stored `exception_ptr`, or throws `E`). It is **`[[nodiscard]]`**.

Composition uses `operator|` from `zyppng::operators`:

```cpp
using namespace zyppng::operators;
return provider->attachMediaIfNeeded( medium )
     | and_then( [&]( MediaHandle h ){ return provider->provide( h, path, spec ); } )
     | and_then( maybeCopyResultToDest( "repodata/repomd.xml" ) )
     | or_else ( [&]( std::exception_ptr err ){ /* try next type */ } );
```

* `and_then`/`mbind`, `or_else`, `inspect`, `inspect_err`, `collect()` (a
  container of expected values → expected of a container),
  `transform_collect`, `transform` (container map), `mtry(f)` (exception →
  `expected<..., exception_ptr>`), `lift` (apply to `pair.first`), `redo_while`,
  `join()` (identity in sync builds, waits for all tasks in async builds).
* Every header ends with `#ifdef ZYPP_ENABLE_ASYNC #include
  <zypp-core/ng/async/pipelines/*.hpp>`. Those files exist **only** in
  `zyppng/lib`. They add `Task<>` overloads (e.g. `expected_result_helper<Task<expected<T,E>>>`,
  async `mtry`, async `transform_collect`).

`ZYPP_ENABLE_ASYNC` and `ZYPPNG=1` are defined only by `zyppng/CMakeLists.txt`
(`add_definitions`). Legacy never defines them.

---

## 4. "Logic written once, run sync or async"

### 4.1 Current mechanism: `MaybeAwaitable` + overlay headers

The same workflow source is written against these macros. Each tier supplies
its own `awaitable.h`:

| Symbol | Legacy (`zypp/zypp-core/ng/async/awaitable.h`) | NG (`zyppng/lib/zypp-core/ng/async/awaitable.h`) |
|---|---|---|
| `ZYPP_IS_ASYNC` | `false` | `true` |
| `MaybeAwaitable<T>` | `T` | `Task<T, coro_lazy_execution>` |
| `zypp_co_await` | *(empty)* | `co_await` |
| `zypp_co_return` | `return` | `co_return` |
| `makeReadyTask(v)` | returns `v` | coroutine `co_return v` |
| `operator\|` | plain `c(in)` | sync overload (`NotAwaitable` LHS) + two coroutine overloads for `Awaitable` LHS |

The typical workflow shape (e.g. `zypp/zypp/ng/workflows/keyringwf.cc`,
`repo/workflows/repomanagerwf.cc`):

```cpp
struct ImportKeyFromRepoLogic {
  MaybeAwaitable<bool> execute() {
    if ( ... ) return makeReadyTask(false);
    return RepoInfoWorkflow::fetchGpgKeys( _context, _repo )
         | [this]( expected<void> res ) { ... return importFromKnownKeyring(); };
  }
  ContextRef _context; ...
};
MaybeAwaitable<bool> provideAndImportKeyFromRepository( ContextRef ctx, std::string id, zypp::RepoInfo info ) {
  ImportKeyFromRepoLogic impl( ctx, std::move(id), std::move(info) );
  zypp_co_return zypp_co_await( impl.execute() );
}
```

The outer free function becomes a coroutine in NG, so `impl` (which the
`[this]` lambdas capture) lives in the coroutine frame. In legacy it is a plain
function returning a value. Some behaviour cannot be shared, mostly user
interaction. The `ReportHelper` classes (`zypp/zypp/ng/reporthelper.h`) cover
that: `detail::ReportHolder<Report, Context::isAsync>`. In the sync case it
holds a legacy `callback::SendReport<Report>`, so the questions reach zypper/YaST
through the classic `KeyRingReport`, `DigestReport`, `JobReport`, … receivers.
In the async case it is empty, because NG is meant to use
`UserInterface::sendUserRequest()` instead. Real async-only code is guarded by
`#ifdef ZYPP_ENABLE_ASYNC` (e.g. `runPlugin()` in `serviceswf.cc`,
`repo2Solv()` as a hand-written awaiter in `repomanagerwf.cc`).

### 4.2 Legacy remnant: `LogicBase` / `AsyncOp`

`zypp/zypp/ng/workflows/logichelpers.h` still documents the *old* design:
`LogicBase<Executor, OpType>`, `SyncOp`/`AsyncOp<T>`, `MaybeAsyncMixin`,
`ZYPP_ENABLE_LOGIC_BASE`, `SyncContextRef` vs `ContextRef` overloads. Commit
`b04dd3eed` ("Migrate existing code to coroutines", Dec 2025) **deleted
`asyncop.h` / `asyncresult.h`**. `AsyncOpRef`, `is_async_op_v` and
`SyncContext` are no longer defined anywhere. The header is not included by
anything. Leftover mentions in `ng/media/provide.h`, `ng/repo/refresh.h`,
`ng/repomanager.h` (`ZYPP_FWD_DECL_TYPE_WITH_REFS(SyncContext)`),
`ng/private/repomanager_p.h` and `tools/repomirror` are dead text or dead code.
Do not model new code on them. Today the split is done by **one `Context` type
per tier** (`Context::isAsync`), not by `SyncContext`/`AsyncContext` classes.

### 4.3 Sync-over-async in legacy (the pattern that actually runs)

Where legacy needs real async machinery (curl multi, child processes), it uses
a **local nested event loop**:

```cpp
// zypp/zypp/media/detail/MediaNetworkRequestExecutor.cc
MediaNetworkRequestExecutor()
  : _evDispatcher( zyppng::ThreadData::current().ensureDispatcher() ) // MUST be first member
  , _nwDispatcher( std::make_shared<zyppng::NetworkRequestDispatcher>() ) // creates a Timer → needs dispatcher
void executeRequest(...) {
  auto loop = zyppng::EventLoop::create();
  _nwDispatcher->run();
  req->sigFinished().connect( [&](...){ loop->quit(); } );
  _nwDispatcher->enqueue( req );
  loop->run();          // blocks until finished
}
```

The NG module `Context` has the same idea as `Context::execute(Task)` /
`joinPipeline(ctx, Task)` (`zyppng/lib/zypp/ng/context.cppm`). See the pitfalls
about their current state.

---

## 5. Coroutine support (NG only)

Files: `zyppng/lib/zypp-core/ng/async/{awaitable.h,task.h,iotask.h}`,
`async/pipelines/{await.h,wait.hpp,…}`.

* **Concepts**: `Awaitable<T>` = co_awaitable **and not copy-constructible**
  (single ownership). `NotAwaitable<T>`. `awaitable_res_type_t<T>`.
* **`Task<Result, Exec = coro_lazy_execution>`** is a move-only RAII owner of
  the coroutine handle. The destructor destroys the frame.
  * Lazy (default): `initial_suspend = suspend_always`. Nothing runs until it is
    `co_await`ed or `start()` is called. `final_suspend` does symmetric transfer
    to the continuation and calls the notify callback.
  * Eager (`coro_eager_execution`): runs at once, then suspends at the end.
  * `isReady()`, `get()` (rethrows a stored exception), `registerNotifyCallback(f)`
    (called *inside* `final_suspend`. **Never destroy the Task inside it**;
    post to the loop instead), `clearNotifyCallback()`.
  * `unhandled_exception()` logs `ERR "Unhandled ... exception in coro"`. If
    `Result` is an `expected`, it stores `Result::error(exception_ptr)`,
    otherwise the raw exception.
* **Awaiting signals**: the global `operator co_await(SignalProxy<R(Args...)>&&)`
  connects once and returns `void` or a `std::tuple` of the args (references
  become `reference_wrapper`). It resumes through `invokeAfter(...,0)`. Example:
  `co_await timer->sigExpired();`. Pipeline form: `await<Process>(&Process::sigFinished)`.
* **`IOTaskAwaiter<T>`** bridges callbacks to coroutines. `return_value()` resumes
  through `invokeAfter(…,0)` "to avoid cleaning up the coroutine stack while we
  are still in code belonging to part of that stack".
* **`coro_poll(vector<Task*>)`** and **`join()`** wait for many tasks.
* **Coroutine parameter rule** (commit `cf5bd904d`, "Avoid references as
  parameters to coroutines", and the comment in NG `operators.h`): coroutine
  functions take parameters **by value**. A reference stored in a suspended frame
  can point to a dead temporary. This is why `Provide::provide(MirroredOrigin
  origin, ProvideFileSpec request)` takes its arguments by value in NG and by
  `const&` in the sync facade.
* **Clang quirk** (NG `operators.h`): the sync `operator|` *must* have a deduced
  `auto` return type. With an explicit `invoke_result_t` return type, Clang ≤19
  instantiates user lambdas with the wrong argument type for an Awaitable LHS,
  and you get "baffling errors deep in user code". `static_assert`s guard the
  overload selection.
* Test helper: `ZYPP_CORO_TEST_CASE(name)` in `zypp-logic/tests/lib/TestTools.h`
  creates an `EventLoop`, starts the coroutine body, runs the loop until it
  finishes and then calls `get()` to rethrow.

---

## 6. How legacy libzypp uses NG code today (production paths)

```
zypp::RepoManager (zypp/zypp/RepoManager.cc)
   Impl → zyppng::RepoManager::create( Context::defaultContext(), opt ).unwrap()
   every public method → _ngMgr->xxx(...).unwrap()   (expected → exception)
   progress: zyppng::ProgressObserverAdaptor ↔ ProgressData / ProgressReport
        │
        ▼
zyppng::RepoManager (zypp/zypp/ng/repomanager.{h,cc})  — sync, returns expected<>
   RepoManagerWorkflow::{probeRepoType, readRepoFile, checkIfToRefreshMetadata,
                         refreshMetadata, buildCache, addRepository, refreshGeoIPData}
   RepoServicesWorkflow::{fetchRepoListfromService, probeServiceType, refreshService}
   RepoDownloaderWorkflow, RpmmdWorkflow, SuseTagsWorkflow, PlaindirWorkflow
        │ uses
        ▼
zyppng::Context (zypp/zypp/ng/context.{h,cc})  isAsync=false; static defaultContext()
   provider()  → zyppng::Provide  (zypp/zypp/ng/media/provide.cc: SYNC facade over
                 zypp::media::MediaManager / MediaSetAccess::rewriteUrl)
   keyRing()   → zypp::getZYpp()->keyRing()
   config()    → zypp::ZConfig::instance();  pool() → ResPool::instance()
```

Other legacy → NG uses, all in production:

| Legacy caller | NG piece |
|---|---|
| `zypp/zypp/repo/PackageProvider.cc` | `KeyRingWorkflow::provideAndImportKeyFromRepository(Context::defaultContext(), …)` |
| `zypp/zypp/FileChecker.cc` | `CheckSumWorkflow::verifyChecksum`, `SignatureFileCheckWorkflow::verifySignature` (each with a fresh `Context::create()`) → `KeyRingWorkflow::verifyFileSignature` |
| `zypp/zypp/RepoInfo.cc` | `zypp/ng/workflows/repoinfowf.h` (gpg key fetching) |
| `zypp/zypp/media/MediaCurl2.cc` + `detail/MediaNetworkRequestExecutor.cc` | `NetworkRequestDispatcher`, `NetworkRequest`, `ZckLoader` on a nested `EventLoop`. MediaCurl2 is the default HTTP backend unless built with `LIBZYPP_CONFIG_USE_LEGACY_CURL_BACKEND_BY_DEFAULT`. Env **`ZYPP_CURL2=0/1`** or URL `mediahandler=curl|curl2` (`MediaHandlerFactory.cc`). Changelog: "Code16: Enable curl2 backend and parallel package download by default" (17.37.x) |
| `zypp/zypp/target/commitpackagepreloader.cc` | parallel package preload with `NetworkRequestDispatcher` + `EventLoop`. Env **`ZYPP_PCK_PRELOAD`** |
| `zypp/zypp/target/TargetImpl.cc` (single rpm transaction) | `EventLoop`, `Process`, `Pipe`, `AsyncDataSource`, `StompFrameStream`, `UnixSignalSource` to drive `zypp-rpm` |
| `zypp/tools/zypp-rpm/main.cc` | `StompFrameStream`, `ByteArray`, `eintrSafeCall` |
| `zypp/zypp/repo/RepoMirrorList.cc` | `NetworkRequest` via `MediaNetworkRequestExecutor` for mirrorlists |
| `zypp/zypp/sat/detail/PoolImpl.cc`, `Capability.cc` | **the global libsolv `CPool` is created by `zyppng::sat::StringPool::instance()`** (`zypp-logic/zypp/ng/sat/stringpool.cc`, which also sets `DISTTYPE_RPM`, libzypp#726). `PoolImpl::~PoolImpl` does not free it. The static `StringPool` does `pool_free` at exit |

So "NG" bugs in these areas are **legacy production bugs**. Report them under
the normal libzypp changelog and release (chapter 12).

History: the workflows were first written in `zypp-logic/zypp/ng` for both
trees (async via `AsyncOp`). They were moved to coroutines (`b04dd3eed`), then
moved **into the legacy tree** (`d3a8b7441`, "Move zypp application logic into
legacy source tree", Jan 2026). An async build of RepoManager and the workflows
therefore **no longer exists**. The `#ifdef ZYPP_ENABLE_ASYNC` branches in
`zypp/zypp/ng/**` are never compiled (zyppng does not build `zypp/zypp/ng`).
Per `AGENTS.md` these workflows are to be *rewritten* inside `zyppng/`, not
moved back.

---

## 7. The `zyppng` C++20 module (experimental)

`zyppng/lib/zypp/CMakeLists.txt` (`cmake_minimum_required(VERSION 3.28)`,
`FILE_SET CXX_MODULES`) lists the interface units **in bottom-up dependency
order** (the order matters):

1. **Bridge partitions (`// LEGACY-BRIDGE`)** that re-export `zypp::` types
   under `zyppng::` until legacy is cut: `arch, bit, capmatch, countrycode, cpeid,
   dep, edition, idstring, idstringtype, languagecode, locale, range, rel,
   relcompare, reskind, resolvernamespace, restraits`. Example: `edition.cppm`
   does `export import :idstringtype;` because the free `operator==` etc. are
   attached to the global module fragment and are not reachable through ADL
   across the module boundary.
2. `base/` (`serialnumber`, `settracker`, `strmatcher`).
3. `sat/`: `stringpool` (LEGACY-REQUIRED thin wrapper), `poolconstants`,
   `solvattr`, `poolmember`, `capability`, `capabilities`, `queue`, `solvable`,
   `repository`, `lookupattr`, `lookupattrtools`, `preparedpool`,
   `solvableident`, `solvablespec`, `cap2str`, `namespaces/*`, `components/*`,
   `pool`. Design doc: `zyppng/lib/zypp/ng/sat/POOL_DESIGN.md`. In short: no
   global state in the API (explicit `Pool&`), `PreparedPool` guarantees a valid
   whatprovides index, policy lives in `PoolComponent`s, the `Pool` registers
   itself in `CPool::appdata`, and **only one Pool may exist** because
   `StringPool` is still a process singleton.
4. `log/` (formatting of sat types).
5. `context_private` (internal, not re-exported), `contextconfig`,
   `applicationdata`, `context`.
6. Aggregators `sat.cppm`, `log.cppm`, `base.cppm`, then the primary
   `ng/zyppng.cppm` (`export module zyppng;`, re-exports everything).

Partition names are flattened paths (`zyppng:sat_pool`,
`zyppng:log_sat_solvable`). Module implementation units are `.cc` with
`module zyppng;`. `#include` is only allowed in the global module fragment.

**Production vs test object libraries**:
* `zyppng-objlib` (OBJECT) → wrapped by `zyppng` (STATIC, `zypp/dummy.cc`) →
  linked **only** by `libzypp-glib`. No public C++ ABI, no version script.
* `zyppng-objlib-test` compiles the same sources with `ZYPPNG_TEST_BUILD=1`, so
  `ZYPPNG_TEST_EXPORT` (`ng/zypp_test_export.h`) becomes `export` and tests see
  module-internal symbols. Tests link this target, with
  `CXX_SCAN_FOR_MODULES ON`. (The comments mention a
  `zyppng_add_module_test()` helper, but it does not exist. Tests use the plain
  `ADD_TESTS` macro in `zyppng/tests/CMakeLists.txt`.)
* Link order matters: `zyppng-curl`, `zyppng-media` before `zyppng-core`
  (`zyppng_propagate_link_deps`).

**NG `Context`** (`context.cppm`, `context.cc`): `class Context : public
UserInterface`, `isAsync = true`, created via `Context::create(root)`. The
constructor calls `ThreadData::current().ensureDispatcher()`, creates and
`start()`s an async `Provide` in a `TmpDir`, then `registerZyppKeys(config)` +
`config.parse()`. Parse errors are logged as `WAR "zypp.conf: key … (using
compiled-in default)"`. `ContextConfig` = `zyppng::config::Config`. The typed
key tokens (`GpgCheck`, `RepoCachePath`, …) come from
`zypp-logic/zypp/ng/config/zyppconfig.h`. Recent work ("NG: config: …" commits)
makes it match legacy `ZConfig` (arch handling, numeric forms, tri-bools).
Chapter 09 covers the zypp.conf semantics.

What is **already in the module**: sat pool layer, base helpers, config,
Context skeleton, ApplicationData. **Not yet** (still only as sync legacy
workflows): RepoManager, refresh, keyring, solver, target/commit.

---

## 8. Async Provide and the media workers (experimental)

The async `Provide` lives in `zyppng/lib/zypp-media/ng/` (chapter 07 covers the
legacy media stack). The legacy build uses the *sync facade* of the same name
from `zypp/zypp/ng/media/provide.h`. `providefwd.h` forward-declares
`ProvideItem`/`ProvideRequest`/… only under `ZYPP_ENABLE_ASYNC`.

```
Provide (controller, one per Context)
  ├ items: ProvideItem (file provide / attach), each may spawn ProvideRequests
  ├ per-scheme queues → ProvideQueue → spawns worker
  │      <ZYPP_WORKER_PATH>/zypp-media-<scheme>   (Process, cwd = workDir/…)
  │      talks STOMP frames over stdin/stdout (StompFrameStream)
  ├ scheme aliases (_workerAlias in private/provide_p.h):
  │      ftp,tftp,https→http  cifs→smb  nfs4→nfs  cd,dvd→disc  file→dir  hd→disk
  ├ file cache with death timers, attached media list, pulse timer
  └ signals: sigIdle, sigMediaChangeRequested, sigAuthRequired
```

* Protocol (`private/providemessage_p.h`): codes 1xx informational (100
  ProvideStarted), 2xx success (200 ProvideFinished, 201 AttachFinished, 202
  AuthInfo, 203 MediaChanged, 204 DetachFinished), 300 Redirect, 4xx client
  errors (401 Unauthorized … 409 InvalidChecksum, 410 MountFailed, 411 Jammed,
  412/413 MediaChangeAbort/Skip, 414 NoAuthData, 415 NotAFile, 416
  MediumNotDesired), 5xx server errors (500 Internal, 501 ProtocolError), 6xx
  controller requests (600 Provide, 601 Cancel, 602 Attach, 603 Detach), 7xx
  worker→controller requests (700 Auth data, 701 Media change).
  `WorkerCaps`: type `Downloading|SimpleMount|VolatileMount|CPUBound`, flags
  `SingleInstance|Pipeline|ZyppLogFormat|FileArtifacts`, `protocol_version`.
* Worker executables (`zyppng/tools/zypp-media-*/main.cc`) block
  SIGPIPE/SIGINT and run a `ProvideWorker` subclass on `STDIN/STDOUT`. The http
  worker's `main.cc` has a `@TODO @WARNING @BUG` comment saying the user agent
  and anonymous id headers are not initialised. That is only half true.
  `NetworkProvider::initialize()` does read `zconfig://media/UserAgent`,
  `.../DistributionFlavor` and `.../AnonymousId`. But `ProvideQueue::doStartup()`
  sends only the user agent (`"ZYpp " LIBZYPP_VERSION_STRING`), `AttachPoint`
  and `ProviderRoot` (`// @TODO actually write real config data`). So the
  `X-ZYpp-DistributionFlavor`/`X-ZYpp-AnonymousId` headers are never sent.
  §14 has the details.
* Worker path: `ZYPP_WORKER_PATH` compile definition =
  `${libexecdir}/zyppng/workers`, or `<build>/zyppng/tools/workers` with
  `ENABLE_DEVEL_BUILD`. Override at runtime with `Provide::setWorkerPath()`
  (tests use `ZYPPNG_WORKERS_DIR`). A missing or non-executable worker logs
  `ERR "Failed to find worker for <scheme>"`.
* Debug logging: env **`ZYPP_MEDIA_PROVIDER_DEBUG`** enables the `*_PRV`
  macros, log group `ZYPP_MEDIA_PROVIDE` (`private/providedbg_p.h`).
* Known open TODOs in `provide.h`: bsc#1174011 (auth=basic ignored), URL
  rewriting like `MediaSetAccess::rewriteUrl`, merging custom headers.
* `zypp-media-tvm` is a "test VM" worker for `Provider_test` and is not
  installed. Legacy `libzypp.spec` installs **no** workers. Only
  `libzypp-glib.spec` ships them.

---

## 9. libzypp-glib (`zyppng/api`) – experimental

* Goal (header comment in `application.h`): a GObject/GIR C API usable from
  Python/JS/Vala that will eventually be "the only officially supported API",
  with zypper rewritten on top of it. PackageKit is the obvious consumer.
* **Reality today**: only `ZyppApplication` is built
  (`zypp_application_new(GMainContext*)`, `get_dispatcher`, `get_version`, and
  `set/has/get_user_data`, which are **stubs** returning nothing/false). All other
  sources (context, repomanager, repoinfo, expected, error, progressobserver,
  ui/*, callbacks) are commented out in `zyppng/api/CMakeLists.txt`. The Python
  example in the header (`Zypp.Context()`) does not work yet. The header claims
  `zypp_application_new` returns an existing instance. The code always calls
  `g_object_new`.
* `ZyppApplication` constructs the thread `EventDispatcher` (optionally on the
  caller's `GMainContext`, property `eventDispatcher`, construct-only) and
  **installs a process-wide `g_log_set_default_handler`** that forwards all
  GLib logging into the zypp log (once, via `g_once`).
* Wrapper plumbing: `private/globals_p.h` (`ZYPP_DECLARE/DEFINE_GOBJECT_BOILERPLATE`,
  `ZYPP_GLIB_*` macros, `registerWrapper` through `Base::setData(quark)`),
  `utils/{GLibMemory,GObjectMemory,GioMemory,RetainPtr,ResourcePtr,OpaqueTypePtr,GList,GHashTable}`.
* GIR: `gobject_introspection(FILENAME Zypp-1.0.gir NAMESPACE Zypp SYMBOL_PREFIXES zypp)`.
  Enums through `glib_mkenums` (`zyppenums.{h,c}.in`). Export header
  `zypp-glib/zypp-glib_export.h` (`LIBZYPP_GLIB_EXPORT`).
* **Versioning** has its own scheme. The single source of truth is
  `zyppng/packaging/libzypp-glib-version.inc` (`%define libzypp_glib_major/minor/patch/compatminor`,
  currently 0.1.0, compatminor 0). It is `%include`d by the spec and
  regex-parsed by `zyppng/api/VERSION.cmake`. The SONAME math mirrors libzypp:
  `SO_FIRST = MAJOR*100 + COMPATMINOR`, `AGE = MINOR - COMPATMINOR` →
  `libzypp-glib.so.<SO_FIRST>.<AGE>.<PATCH>`. "Pre-1.0: any release may break
  ABI." The bump rules are in the `.inc` comment (bump only right before a
  release; removal → `COMPATMINOR = MINOR`).
* **Packaging** (`zyppng/packaging/libzypp-glib.spec`): builds with
  `-DBUILD_LIBZYPP=OFF -DBUILD_LIBZYPPNG=ON`, **Clang 19 + lld19 + Ninja, CMake
  ≥ 3.28**, static libsolv, runs `ctest` in `%check` (needs nginx, vsftpd,
  squid, FastCGI). Subpackages: `libzypp-glib`, `-devel` (headers + .gir),
  `typelib-1_0-Zypp-1_0`. It does not link legacy `libzypp.so`. Changelog:
  "Initial release … Targets Tumbleweed and Leap/SLES 16+". OBS CI branches both
  `zypp:ci:libzypp` and `zypp:ci:libzypp-glib` on PRs (`.obs/workflows.yml`).
  Commit `880d4b587` split libzypp and libzyppng into separate spec files. The
  legacy `libzypp.spec.cmake` keeps a `%bcond_with build_libzyppng` (off by
  default).

---

## 10. Building NG

| Item | Requirement |
|---|---|
| CMake option | `-DBUILD_LIBZYPPNG=ON` (default OFF in libzypp. The zypp-stack root enables it, together with `BUILD_LIBZYPP`) |
| CMake version | 3.28 for `zyppng/lib/zypp` (module scanning). The rest of the project needs 3.17 |
| Compiler | C++20 with modules and coroutines. Packaging uses **Clang 19**. `zyppng/CMakeLists.txt` prefers the matching `ld.lld-<major>` for Clang. A GCC-only flag (`-fconcepts-diagnostics-depth=4`, Debug) exists, but I did not verify that GCC builds the modules. See §18 for details |
| Generator | Ninja (as in the spec. Module dependency scanning needs Ninja or a recent Makefile generator) |
| Extra deps | glib/gobject/gio, gobject-introspection, sigc++-2.0 (all builds), yaml-cpp |
| NG-only build | `-DBUILD_LIBZYPP=OFF -DBUILD_LIBZYPPNG=ON` (skips Boost program_options) |
| Devel build | `ENABLE_DEVEL_BUILD=ON` → workers from `<build>/zyppng/tools/workers`, `ZYPPNG_RPM_BINARY` from the build tree, repo2solv/rpmdb2solv from the stack's libsolv build when `ZYPP_STACK_BUILD` (prints a "do not do this in production" warning) |

```bash
cmake -G Ninja -S . -B build -DCMAKE_CXX_COMPILER=clang++-19 -DCMAKE_C_COMPILER=clang-19 \
      -DBUILD_LIBZYPPNG=ON -DENABLE_BUILD_TESTS=ON -DENABLE_DEVEL_BUILD=ON
ninja -C build && ctest --test-dir build/zyppng -R 'Task|Pipelines|PoolNg|Config|Provider'
```

The shared `zypp-logic` targets are built twice: once with the `zypp` prefix and
`zypp_lib_compiler_flags` (C++17, no `ZYPP_ENABLE_ASYNC`), and once with the
`zyppng` prefix (`ZYPP_LOGIC_TARGET_PREFIX "zyppng"`, C++20,
`-fvisibility=hidden`, `ZYPP_ENABLE_ASYNC`, `ZYPPNG=1`). A change under
`zypp-logic/*/ng` must compile in **both** modes. That means C++11/17-compatible
headers if legacy includes them, and correct code with both overlay variants of
`MaybeAwaitable`. `zypp-common` is currently not built for NG (commented out in
`zyppng/lib/CMakeLists.txt`).

---

## 11. Migration rules and status

From `AGENTS.md` ("Migration Protocol for `zypp-logic/zypp/ng/*` → `zyppng/`"):

1. Pick the class.
2. Check whether anything under `zypp/` includes it, directly or transitively.
   * **Yes** → tag the header top with `// LEGACY-REQUIRED`, leave it in place,
     write the NG version in `zyppng/`, and switch NG consumers to the module
     import.
   * **No** → move the implementation to `zyppng/`, switch consumers to
     `import zyppng[:partition];` and delete the old file.
3. Leave **no forwarding headers** behind. Either delete the file or replace it
   with a `static_assert` **tombstone** that tells consumers where the code went.

Current state (from grep):

* `LEGACY-REQUIRED` appears **only** on `zypp-logic/zypp/ng/sat/stringpool.{h,cc}`,
  the wrapper `zyppng/lib/zypp/ng/sat/stringpool.cppm`, and the related comments
  in `zypp-logic/zypp/zypp.cmake` / `zyppng/lib/zypp/CMakeLists.txt`.
* **No tombstones exist** in the tree. Migrated headers (e.g. `zypp-logic/zypp/ng/{bit,dep,rel,…}.h`,
  `sat/poolconstants.h`, `sat/poolmember.h`) were simply deleted in
  `b5849e009` ("Migrate zyppng to modules").
* `zypp-logic/zypp/zypp.cmake` notes: *"All ng/sat/\*.cc have been migrated to
  zyppng/ as C++20 module impl units. Only stringpool.cc remains here."*
* `LEGACY-BRIDGE` partitions (§7) are the reverse direction. They are
  temporary re-exports of legacy `zypp::` value types. Replace them with native
  types only once legacy is severed.
* Stale dev tools: `zypp/tools/refresh-ng` (includes the non-existent
  `<zypp-core/zyppng/...>`) and `zypp/tools/repomirror` (uses `AsyncOpRef`).
  Both are behind `ENABLE_DEVTOOL_REFRESHNG` / `ENABLE_DEVTOOL_REPOMIRROR` (OFF)
  and will not build as they are.

Review checklist for NG PRs: correct tier; no `zypp/` include from `zyppng/`;
`expected` instead of throwing; coroutine parameters by value; no
`#include` after `export module`; module file listed in the right position of
`ZYPPNG_MODULE_INTERFACE_UNITS`; legacy build still compiles if `zypp-logic/*/ng`
was touched; tests in `zyppng/tests` (module-internal) or `zypp/tests/zyppng`
(shared framework, legacy mode).

---

## 12. Module inventory: every `.cppm` in `zyppng/lib`

All interface units live in `zyppng/lib/zypp/ng/`. No other directory under
`zyppng/lib` contains `.cppm` files: `zypp-core/ng`, `zypp-media/ng` and
`shared/` are still classic headers and `.cc` files, compiled into the
`zyppng-core`/`zyppng-media`/`zyppng-curl` object libraries. Status legend:
**B** = `// LEGACY-BRIDGE` (re-exports a `zypp::` type), **R** =
`LEGACY-REQUIRED`, **N** = native NG code, **I** = internal (module purview
only, not exported to consumers).

| File (under `ng/`) | Partition | Purpose | Status |
|---|---|---|---|
| `zyppng.cppm` | `zyppng` (primary) | `export import`s the 3 aggregators plus 20 leaf partitions | N |
| `sat.cppm`, `log.cppm`, `base.cppm` | `:sat`, `:log`, `:base` | aggregators: 25 / 8 / 3 `export import`s | N |
| `arch.cppm` | `:arch` | `zypp::Arch` + `Arch::CompatSet` | B |
| `bit.cppm` | `:bit` | `zypp::bit` range/mask helpers | B |
| `capmatch.cppm` | `:capmatch` | `CapMatch` tri-state | B |
| `countrycode.cppm`, `languagecode.cppm`, `locale.cppm` | `:countrycode`, `:languagecode`, `:locale` | ISO codes, `Locale` (re-exports `:idstringtype`) | B |
| `cpeid.cppm` | `:cpeid` | `CpeId` | B |
| `dep.cppm`, `rel.cppm`, `relcompare.cppm`, `range.cppm` | `:dep`, `:rel`, `:relcompare`, `:range` | dependency kinds, `Rel` operators, `compareByRel`, `Range<>` | B |
| `edition.cppm` | `:edition` | `Edition` (re-exports `:idstringtype` so the GMF-attached operators reach consumers) | B |
| `idstring.cppm`, `idstringtype.cppm` | `:idstring`, `:idstringtype` | `IdString`, CRTP `IdStringType<>` | B |
| `reskind.cppm`, `restraits.cppm`, `resolvernamespace.cppm` | `:reskind`, `:restraits`, `:resolvernamespace` | `ResKind`, `traits::`, `Package`/`Patch`/… type names, `ResolverNamespace` flags | B |
| `base/serialnumber.cppm`, `base/settracker.cppm`, `base/strmatcher.cppm` | `:base_serialnumber`, `:base_settracker`, `:base_strmatcher` | `SerialNumber(Watcher)`, `SetTracker<>`, `StrMatcher`/`Match` (with ADL operator wrappers) | B |
| `sat/stringpool.cppm` | `:sat_stringpool` | thin GMF wrapper around `zypp-logic/zypp/ng/sat/stringpool.h` (process-wide `CPool`) | R |
| `sat/poolconstants.cppm` | `:sat_poolconstants` | `detail::CPool/CRepo/CQueue…` aliases, `IdType`, `noId`, `PoolInvalidation` | N (mostly I) |
| `sat/solvattr.cppm` | `:sat_solvattr` | `SolvAttr` ids | B |
| `sat/poolmember.cppm` | `:sat_poolmember` | CRTP `PoolMember<T>`, undefined `poolFromType<T>` primary templates | N |
| `sat/capability.cppm`, `sat/capabilities.cppm` | `:sat_capability`, `:sat_capabilities` | NG `Capability`, `CapDetail`, `Capabilities` iterator | N |
| `sat/queue.cppm` | `:sat_queue` | libsolv `Queue` wrapper (`SolvableQueue`, `StringQueue`) | N |
| `sat/solvable.cppm`, `sat/repository.cppm` | `:sat_solvable`, `:sat_repository` | thin handles (`PoolMember`) | N |
| `sat/lookupattr.cppm`, `sat/lookupattrtools.cppm` | `:sat_lookupattr`, `:sat_lookupattrtools` | `LookupAttr`/`LookupRepoAttr`, C++20 `TransformIterator` (replaces the Boost one) | N |
| `sat/preparedpool.cppm` | `:sat_preparedpool` | move-only `PreparedPool` view (whatprovides queries) | N |
| `sat/solvableident.cppm`, `sat/solvablespec.cppm` | `:sat_solvableident`, `:sat_solvablespec` | `SplitIdent`; pure-data `SolvableSpec` + `EvaluatedSolvableSpec` | N |
| `sat/cap2str.cppm` | `:sat_cap2str` | `cap2str()` (static buffer, not reentrant) | I |
| `sat/namespaces/*.cppm` | `:sat_namespaces_{namespaceprovider,filesystem,language,modalias}` | `NamespaceProvider` base + the 3 libsolv namespace providers | N |
| `sat/components/*.cppm` | `:sat_components_{poolcomponents,architecturecomponent,autoinstalledcomponent,namespacecomponent,packagepolicycomponent}` | component interfaces, `PoolComponentSet`, the 4 concrete components (§13) | N |
| `sat/pool.cppm` | `:sat_pool` | `sat::Pool` (owner, lifecycle, component registry) | N |
| `log/format.cppm` | `:log_format` | `log::formatter<T>` trait, `log::view(obj)` / `log::view(ctx,obj)`, catch-all `operator<<` | N |
| `log/sat/*.cppm` | `:log_sat_{capabilities,capability,lookupattr,queue,repository,solvable,solvablespec}` | `formatter<>` specialisations for sat types | N |
| `context_private.cppm` | `:context_private` | `ContextPrivate` (dispatcher, provider TmpDir, `Provide`, config). Not re-exported | I |
| `contextconfig.cppm` | `:contextconfig` | `ContextConfig = config::Config` + re-exported parse helpers and `registerMediaKeys` | N |
| `applicationdata.cppm` | `:applicationdata` | static `ApplicationData` string (for logs, history, plugins) | N |
| `context.cppm` | `:context` | NG `Context` (`UserInterface` subclass), `execute`/`joinPipeline` | N (broken helpers, see Pitfalls) |

Facts you need when you read the module:

* **The NG `Context` does not own a `sat::Pool`.** `ContextPrivate` holds only
  `_eventDispatcher`, `_providerDir` (TmpDir), `_provider` and `_config`. The
  comment in `pool.cppm` ("concrete components wired by Context") describes
  the plan, not the code. Nothing outside the tests creates a `sat::Pool`.
* `log::view(ctx, obj)` returns a proxy that holds `const T&` and a
  `ContextWeakRef`. Use it only inside one stream expression. If you store it
  (`auto v = log::view(ctx, s.name());`), the reference dangles. If the
  Context is gone, it prints `<<Error: Context Required for streaming …>>`.
* `cap2str()` writes into a libsolv temp buffer ("can be overridden when
  cap2str is called again"). Copy the result before you call it again.

---

## 13. NG sat layer: the component-based Pool vs. legacy `PoolImpl`

Source: `zyppng/lib/zypp/ng/sat/{pool,preparedpool}.{cppm,cc}`,
`components/*`, `namespaces/*`. Read `POOL_DESIGN.md` first. This section
summarises it and adds what the code actually does today.

```
            StringPool::instance().getPool()   (LEGACY-REQUIRED singleton CPool)
                              │  appdata = this   (ZYPP_PRECONDITION: only one Pool)
                              ▼
 sat::Pool ── PoolComponentSet ─┬─ _basicComponents  (checkDirty/onInvalidate/onRepoAdded/Removed/onReset)
   │  _serial, _serialIDs       ├─ _initComponents[InitStage]         IPoolComponent      prepare(Pool&)
   │  _preparing (DtorReset)    └─ _preparedComponents[PreparedStage] IPreparedPoolComponent prepare(PreparedPool&)
   │
   └─ prepare():  P1 notifyCheckDirty → [serial changed] P2 notifyPrepare
                  → [whatprovides==nullptr] P3 pool_addfileprovides+pool_createwhatprovides
                  → [serial changed || index rebuilt] P4 notifyPrepareWithIndex → PreparedPool
```

| Component (`components/`) | Interface / stage | What it does | Legacy equivalent |
|---|---|---|---|
| `ArchitectureComponent` | `IPoolComponent`, `Environment`, prio 0 | `prepare()` calls `pool_setarch()` once per serial (`SerialNumberWatcher`). `onRepoAdded()` frees solvable blocks with incompatible arch from non-system repos (`repo_free_solvable_block`, keeps `src`/`nosrc`). `arch()` = provider callback, or `Arch::detectSystemArchitecture()` | `PoolImpl::prepare()` arch handling + `_addSolv` arch filtering, `ZConfig::systemArchitecture()` |
| `AutoInstalledComponent` | `IPoolComponent` | holds a `StringQueue` of autoinstalled idents. `isOnSystemByUser/Auto(IdString)` | `PoolImpl::_autoinstalled` |
| `NamespaceComponent` | `IPoolComponent` | registry `IdType → shared_ptr<NamespaceProvider>`. `attach()` installs `CPool::nscallback = libsolv_callback` (returns 1 = system property, 0 = unsupported) and forwards `checkDirty/prepare/onReset` to the providers | static `PoolImpl::_ns_callback` hardwired switch |
| `PackagePolicyComponent` | `IPreparedPoolComponent`, `Policy` | hardwired specs `retracted-patch-package()`, `ptf()`, `ptf-package()` plus a user `setNeedrebootSpec()`. Evaluated into `EvaluatedSolvableSpec` in `prepare(PreparedPool&)`, dropped on `onInvalidate(Data)`/`onReset` | `PoolImpl::_retractedSpec/_ptfMasterSpec/_ptfPackageSpec/_needrebootSpec` |

| Namespace provider (`namespaces/`) | Answers | Dirty source |
|---|---|---|
| `LanguageNamespaceProvider` | `NAMESPACE_LANGUAGE` from the requested locales (with fallbacks) | `notifyDirty(PoolInvalidation::Dependency, …)` when locales change |
| `FilesystemNamespaceProvider` | `NAMESPACE_FILESYSTEM` | `checkDirty()` watches `/etc/sysconfig/storage`, then `setDirty(Data)` |
| `ModaliasNamespaceProvider` | `NAMESPACE_MODALIAS` | (none in the provider itself) |

How it differs from legacy `zypp/zypp/sat/detail/PoolImpl`:

| Aspect | Legacy `PoolImpl` | NG `sat::Pool` |
|---|---|---|
| Access | global `myPool()` singleton, `sat::Pool::instance()` | explicit `Pool&`. Handles recover it through `PoolMember<T>`/`CPool::appdata` |
| Policy state | all members of one class (locales, autoinstalled, specs, arch) | one `PoolComponent` per concern, created lazily by `pool.component<T>()` (keyed by `typeid`) |
| Index validity | `prepare()` is called implicitly by many queries | `whatprovides` queries exist **only** on `PreparedPool` (debug builds assert the serial is unchanged) |
| Invalidation | `setDirty()` / `depSetDirty()` | `setDirty(PoolInvalidation::Data | Dependency, {reasons…})`. **`ZYPP_PRECONDITION(!_preparing)`**: setDirty during passes 2–4 terminates the process, also in release builds (POOL_DESIGN.md says "asserted in debug builds", which understates it) |
| Reset | `PoolImpl::clear` | `clear()` fires `onReset` only (not `onInvalidate`), bumps `_serial` and `_serialIDs`. Components and user config survive |
| Namespace callback | static function on the legacy pool | per-component `nscallbackdata = this` |

Status and caveats:

* **`attach(Pool&)` is never called.** `IPoolComponent::attach`,
  `IPreparedPoolComponent::attach` and `NamespaceProvider::attach` exist, but
  neither `PoolComponentSet::assertComponent()` nor `Pool` calls them (grep:
  the only caller is `NamespaceComponent::attach` → providers). As a result:
  `NamespaceComponent` never installs the libsolv `nscallback`.
  `ArchitectureComponent::setProvider()` never calls `setDirty` (its `_pool` stays
  null). `NamespaceProvider::notifyDirty()` logs `ERR "NamespaceProvider not
  attached…"` and drops the invalidation. Until this is wired up (most likely
  in `assertComponent`, which has no `Pool&` today), call `attach(pool)` by hand
  after `pool.component<T>()`. `PackagePolicyComponent::attach` asserts that it
  is attached only once.
* Tests: `zyppng/tests/sat/PoolComponentSet_test.cc` covers registration
  idempotency, stage/priority ordering and stale-sort buckets.
  `PoolNg_test.cc` covers the pool. Neither exercises `attach`.

---

## 14. Async Provide internals (deep dive)

Files: `zyppng/lib/zypp-media/ng/{provide,provideitem,providequeue,providemessage,mediaverifier,attachedmediainfo,provideres}.cc`
and `private/*_p.h`. Spec types (`ProvideFileSpec`, `ProvideMediaSpec`,
`isSameMedium`) are in `zypp-logic/zypp-media/ng/providespec.{h,cc}`.

### 14.1 Object model

```
Provide (Base, one per NG Context)            ProvideStatus (optional observer: itemStart/Done, requestStart/Redirect, pulse)
 └ ProvidePrivate
    ├ _items      : list<ProvideItemRef>      ProvideFileItem | AttachMediaItem   (each owns ≤1 running ProvideRequest)
    ├ _queues     : deque<{scheme, deque<ProvideRequestRef>}>   ← not yet assigned to a worker
    ├ _workerQueues: map<name, ProvideQueueRef>                 ← one per running worker process
    │     name = "http://<host>" (Downloading) | "chksum#N" (CPUBound) | "<scheme>" (mounting / SingleInstance)
    ├ _schemeConfigs: scheme → WorkerCaps (cached handshake result)
    ├ _attachedMediaInfos: vector<AttachedMediaInfo_Ptr>  (intrusive refcount, ProvideMediaHandle holds refs)
    ├ _fileCache  : path → {ManagedFile(unlink), deathTimer}
    ├ _scheduleTrigger (Timer 0 ms, coalesces schedule() calls) ; _pulseTimer (5000 ms)
    └ signals: sigIdle, _sigMediaChange, _sigAuthRequired
ProvideQueue (Base) ── Process "zypp-media-<scheme>" (cwd = workDir/<scheme>[/<host>]) ── StompFrameStream
   _waitQueue (deque<Item>) → _activeItems (list<Item>, state Pending/Queued/Running/Cancelling/Finished)
```

`ProvideRequest` (`provideitem_p.h`) wraps the `ProvideMessage` that is sent,
the `MirroredOrigin`, `_pastRedirects` (loop guard) and a weak ref to the
current queue. The "active URL" is stored in the message's `url` field.
Request ids are **controller-wide** (`ProvidePrivate::nextRequestId()`),
assigned in `ProvideQueue::enqueue()`.

### 14.2 Lifecycle of `provide()`

```
co_await prov->provide(origin, spec)            (coroutine, params by value)
  sanitizeUrls(): schemeConfig() for authority + each mirror → drop mirrors whose
                  worker_type differs from the authority's (WAR "...different worker type")
  IOTaskAwaiter<expected<ProvideRes>> p;  ProvideFileItem::create(p, …);  queueItem()
     → schedule(EnqueueItem) → Timer(0) → doSchedule():
          1. detach idle media (14.4)   2. initialize() Uninitialized items → item enqueues ProvideRequest into _queues
          3. per scheme: dispatch to worker queues (14.3)
  ProvideQueue::scheduleNext(): send 600 Provide while canScheduleMore()   (Pipeline flag → unlimited)
  worker → 100 ProvideStarted (informal) → 200 ProvideFinished{local_filename, cacheHit}
  ProvideQueue: Downloading → addToFileCache(); cache hit but file gone → cacheMiss(), re-queue
  ProvideFileItem::finishReq → p.return_value(ProvideRes) → coroutine resumes on next loop iteration
```

If the awaiting Task is destroyed early, `registerDestroyCallback` calls
`item->released()`, which cancels the request (601 Cancel to the worker).
`checksumForFile()` and `copyFile()` are plain `provide()` calls on
`chksum:///<path>` (custom header `chksumType`; result in a response header
named after the algorithm) and `copy:///<src>` (`setDestFilenameHint(target)`).

### 14.3 Scheduling per worker type (`ProvidePrivate::doSchedule`)

| Worker type (from `WorkerCaps`) | Queue naming | Rules |
|---|---|---|
| `Downloading` (http) without `SingleInstance` | `scheme://host`, workdir `workDir/scheme/host` | Attach/Detach requests are rejected ("Downloading Queues do not support…"). Per request: collect mirrors the item `canRedirectTo`. Stop the whole scheme pass if Σ active requests ≥ **10** (`DEFAULT_ACTIVE_CONN`). If fewer than **20** download workers exist (`DEFAULT_MAX_DYNAMIC_WORKERS`), start a new worker for the first unused mirror host. Else push to the existing host worker with the fewest active requests, if its `requestCount() < 5` (`DEFAULT_ACTIVE_CONN_PER_HOST`). Else decommission the longest-idle worker and start a new one. Else defer |
| `CPUBound` (chksum, copy) | `scheme#N`, workdir `workDir/scheme` | first URL of a matching scheme only, no mirrors. Prefer an idle worker of this scheme. Else start a new one while total CPU workers < `2 × nproc` (`sysconf`, fallback 4). Else the least-busy existing one. Else replace the laziest idle CPU worker. A start failure fails the request |
| `SimpleMount`, `VolatileMount`, or any `SingleInstance` | `<scheme>` | exactly one worker per scheme, first compatible URL |

`effectiveScheme()` strips the `-media` suffix and applies `_workerAlias`
(§8). `schemeConfig()` learns a scheme's caps by **spawning the worker
synchronously** (a stack `ProvideQueue`, handshake, then shutdown) and
caching the result. The first `provide()`/`attachMedia()` for a new scheme
therefore blocks the event loop for one process start.

**Mirror failover** (`ProvideFileItem::finishReq`): on `PeerCertificateInvalid`,
`NotFound`, `ConnectionFailed`, `InvalidChecksum` (but **not** `Timeout`, which is
commented out), the request is re-enqueued if any mirror passes
`canRedirectTo`. `safeRedirectTo` records each URL in `_pastRedirects`, so a URL
is tried only once. A 300 Redirect from the worker goes through
`ProvideItem::finishReq`, and a repeated URL gives `"Redirect Loop"`.

### 14.4 Attach / detach semantics

* `prepareMedia()` returns a `LazyMediaHandle` (no I/O).
  `attachMediaIfNeeded()`/`provide(LazyMediaHandle,…)` attach on first use.
* `attachMedia()` first reuses an entry in `_attachedMediaInfos` that
  `isSameMedium()`. Otherwise `AttachMediaItem::initialize()` checks again, then
  looks for a running `AttachMediaItem` for the same medium and **piggybacks** on
  its `sigReady` (`_masterItemConn`).
* `ProvideMediaSpec::isSameMedium` (`providespec.cc`): if both specs have the
  same verify-file path that is not empty → same iff `medianr` is equal. If
  both are empty → indeterminate → "same" iff the two origins share any URL.
* **Downloading schemes** have no real attach. With an empty `mediaFile()` it
  "pretends attach worked". Otherwise it downloads `<url>/media.<nr>/media` and
  compares it with the local copy through `MediaDataVerifier("SuseMediaV1")`
  (`mediaverifier.cc`: 3 lines vendor/ident/totalMedia, `matches()` compares
  vendor+ident). A mismatch gives `MediaNotDesiredException`. A `NotFound` is
  tolerated when `totalMedia()==1`.
* **Mounting schemes** send 602 Attach `{url, attach_id=<random uuid>, label,
  verify_type, verify_data, media_nr, custom headers}`. The worker answers 201
  `{local_mountpoint}`. Later files are requested as
  `<scheme>-media://<attachId>/<path>`. The suffix exists because `zypp::Url`
  refuses an authority for some schemes. The `MountingWorker` resolves this as
  `dev->_mountPoint / attachRoot / path` and returns the path **in place**
  (`cacheHit=false`, no copy, no cleanup).
* **Release**: each `ProvideMediaHandle` holds an intrusive ref. When the count
  drops back to 1 (only `Provide` holds it), `_idleSince` is set. The next
  `doSchedule()` run detaches it: mounting media at once (603 Detach to the
  backing queue, worker `DeviceDriver::detachMedia` + `releaseIdleDevices`).
  Downloading media are kept for **1 hour**. Detach only happens when something
  triggers a schedule run. No timer drives it.
* `ejectDevice(queueRef, device)` calls `CDTools::openTray(device)` **in the
  controller process** if `queueRef` is non-empty. The worker is not involved.

### 14.5 File cache and pulse

Downloading workers write to `<workdir>/staging/<md5(path)>.part`. On
completion they rename the file to `<workdir>/cache/<url-path>`. A later request
for an existing cache path returns `cacheHit=true`. The controller wraps every
result file in a `ManagedFile(unlink)` in `_fileCache`. The 5-s pulse
(`onPulseTimeout`) is meant to drop files that are referenced only by the cache
after a 20-s "death timer". See the Pitfalls for the inverted comparison. The
worker-side cache key is only the URL **path** (per host directory). The query
string is ignored.

### 14.6 Controller round-trips (auth, media change) and worker death

* 700 AuthDataRequest → `ProvideItem::authenticationRequired()` →
  `CredentialManager` (with `_credManagerOptions`), then `sigAuthRequired`. The
  reply is 202 AuthInfo, or 414 NoAuthData ("Item was cancelled", "No auth given
  by user.", …). The `AttachMediaItem` strips `/media.N/media` before the lookup.
* 701 MediaChangeRequest (attach requests only) → `sigMediaChangeRequested(queueName,
  label, mediaNr, freeDevices, desc)`. `RETRY` gives 203 MediaChanged. `ABORT` or no
  handler gives 412. `SKIP` gives 413. Do not shut down the provider inside the
  handler (header note).
* A 6xx code from a worker, an unknown 7xx code, or a send failure →
  `fatalWorkerError()` → `immediateShutdown()`: all waiting and active items
  fail, then `flush()`, `closeWriteChannel()`, `waitForExit()` (blocking), and the
  remaining stderr is drained. An unexpected exit gives "Unexpected queue worker
  exit!". The restart logic (`_crashCounter`, max 3) is `#if 0`.
* Worker stderr is forwarded line by line. With the `ZyppLogFormat` flag
  (always set by `ProvideWorker`), lines go raw into the log through
  `LogControl::logRawLine`. Otherwise they appear as `"Message from worker: <name>:…"`.

---

## 15. Worker side, wire protocol and the `zypp-media-*` tools

### 15.1 Wire format

There is no protobuf or other IDL. Each message is a **STOMP frame**
(`zypp::PluginFrame`, `zypp-logic/zypp-core/rpc/PluginFrame.h`) on
stdin/stdout. `rpc::prepareFrame<T>()` sets the frame command to
`T::typeName`: `ProviderConfiguration`, `WorkerCaps` or `ProvideMessage`. A
`ProvideMessage` carries headers `requestCode` and `requestId` plus typed fields
(`ProvideMessage::FieldVal` = `HeaderValue` variant). Every value is written as
a string (`zypp::str::asString`). **Empty strings are dropped**
(`if ( strVal.empty() ) continue;`). The receiver parses known fields per
code with `PARSE_REQ_FIELD`/`PARSE_OPT_FIELD` (`providemessage.cc`). A missing
required field rejects the whole message. Unknown headers are kept as strings,
which is how custom headers (`chksumType`, `zypp-req-specific-devices`) pass
through. Field names use `_`, not `-`. Old python `zypp-plugin` rejects `-` in
STOMP headers (bsc#1227793 changelog entry). History: "Fix int overflow in
Provider" (#559, 17.35.3: `safe_strtonum` overflowed on timestamps), "zyppng:
Update Provider to current MediaCurl2 download approach, drop Metalink" (#682,
17.38.0).

### 15.2 Handshake and lifetime

```
controller (ProvideQueue::doStartup)              worker (ProvideWorker::run)
 spawn <workerPath>/zypp-media-<scheme>, cwd=workdir
 ── ProviderConfiguration {zconfig://media/UserAgent, AttachPoint=cwd, ProviderRoot} ─▶
                                                   executeHandshake(): keys "zconfig://main/<k>" →
                                                   MediaConfig::setConfigValue (none are sent today)
                                                   initialize(conf) → WorkerCaps; base adds ZyppLogFormat
 ◀── WorkerCaps {protocolVersion=1, workerType, cfgFlags, workerName} ──
 nextMessageWait() blocks; wrong/missing → "Worker did not sent a capabilities message"
 then connects sigMessageReceived / sigChannelReadyRead / sigFinished
```

`protocol_version` is sent but never checked by the controller. The worker
exits when its stdin or stdout is closed (`readFdClosed`/`writeFdClosed` →
`maybeDelayedShutdown`, which is delayed while `_inControllerRequest`). Every
worker must do this, otherwise `ProvideQueue::immediateShutdown` blocks in
`waitForExit()`.

### 15.3 `ProvideWorker` base (`worker/provideworker.{h,cc}`)

* The constructor enables `LogControl::enableLogForwardingMode(true)` and sets
  the thread name. `initLog()` logs to stderr by default.
* Incoming frames → `_pendingMessages` → `messageLoop()` (single-shot 0-ms
  timer). Controller codes (6xx) become `ProvideWorkerItem`s in
  `requestQueue()` (overridable `makeItem()`), then `provide()` is called.
  `ONLY_NEW_PROVIDES` (http) vs `QUEUE_NOT_EMTPY` (default: called again until
  the queue is empty). Cancel: a `Pending` item is answered with 408
  `Cancelled` and removed. A `Running` item → `cancel(it)`.
* `requireAuthorization()` / `requestMediaChange()` are **blocking**
  (`sendAndWaitForResponse`). Other messages that arrive in the meantime are
  stashed and replayed later.
* Senders: `provideStart`, `provideSuccess(id, cacheHit, file, extraHeaders)`,
  `provideFailed`, `attachSuccess`, `detachSuccess`, `redirect`.
* `MountingWorker` + `DeviceDriver` (`worker/mountingworker.cc`,
  `devicedriver.{h,cc}`): `provide()` pops **one** item per loop iteration and
  handles Attach → `driver->mountDevice()`, Detach → `detachMedia(attachId)`
  + `releaseIdleDevices()`, Prov → path lookup on the attached device. Cancel is
  "never called". `DeviceDriver::initialize` requires `AttachPoint` (realpath)
  and always reports caps `Pipeline|ZyppLogFormat|SingleInstance`. Helpers:
  `isDesiredMedium()` (verifier), `checkAttached()` with
  `devicePredicate`/`fstypePredicate`/`bindMountPredicate`,
  `createAttachPoint()`/`removeAttachPoint()`. `Device::_ephemeral` = forget
  the device after its last attachment is released.

### 15.4 The tools (`zyppng/tools/zypp-media-*`, installed to `ZYPP_WORKER_PATH`)

| Binary | Class | Type / flags | Schemes (incl. aliases) | What it does |
|---|---|---|---|---|
| `zypp-media-http` | `NetworkProvider` (`ProvideWorker`) | Downloading, Pipeline | http, https, ftp, tftp | `NetworkRequestDispatcher` downloads into staging → cache, zchunk via `ZckLoader` + range requests when a `delta_file` is given, auth through `requireAuthorization`. Requires `AttachPoint` |
| `zypp-media-chksum` | `ChksumProvider` (`main.cc`) | CPUBound, Pipeline | chksum | `filesystem::checksum(path, chksumType)`, returned as an extra header. Only `Prov` is accepted |
| `zypp-media-copy` | `CopyProvider` (`main.cc`) | CPUBound, Pipeline, FileArtifacts | copy | `filesystem::hardlinkCopy(url path → filename hint)`. The result is an artifact the controller cleans up |
| `zypp-media-dir` | `DirProvider` (`DeviceDriver`) | SimpleMount | dir, file | no mount. A "device" is the realpath of the directory, then `isDesiredMedium` |
| `zypp-media-disk` | `DiskProvider` | SimpleMount | disk, hd | mounts `?device=` (optional `filesystem=`, `mountoptions=`), or bind-mounts an already mounted device |
| `zypp-media-iso` | `IsoProvider` | SimpleMount | iso | mounts `?iso=` with `ro,loop`. The ISO source comes from `?url=` (hd/dir/nfs/smb) via **embedded** `DiskProvider`/`NfsProvider`/`SmbProvider` driver instances with sub attach roots. Nested `iso` is rejected |
| `zypp-media-nfs` | `NfsProvider` | SimpleMount | nfs, nfs4 | `mount -t nfs|nfs4` (`?type=nfs4`, `mountoptions=`) |
| `zypp-media-smb` | `SmbProvider` | SimpleMount | smb, cifs | `mount -t cifs`. Credentials from URL/options/`requireAuthorization`, `workgroup`/`domain`. Retries `checkAttached` |
| `zypp-media-disc` | `DiscProvider` | VolatileMount | disc, cd, dvd | udev enumeration (`ID_CDROM=1`), tries `iso9660` and other filesystems, `requestMediaChange` when the wrong disc is inserted |
| `zypp-media-tvm` | `TestVMProvider` | VolatileMount | tvm | **test only**. Devices come from `<ProviderRoot>/tvm.conf` (YAML, re-read in every `detectDevices()`). "Mount" = symlink to the `insertedPath` dir. Exercises the media-change flow without root |

`zyppng/lib/shared/tvm/tvmsettings.{h,cc}`: `zypp::test::TVMSettings { vector<Device{name, insertedPath}> devices }`
with `YAML::convert<>` specialisations. These are shared by the tvm worker and
`zyppng/tests/media/Provider_test.cc` (`tvm_basic`, `tvm_medchange`, which write
`tvm.conf` with devices `/fakedev/tvm/slot1..3`).

---

## 16. Reference: smaller `zypp-logic/zypp-core/ng` building blocks

| Header | Purpose | Gotchas |
|---|---|---|
| `base/abstracteventsource.h` | base for anything driven by fds. `updateFdWatch(fd, Read|Write|…)`, `removeFdWatch`, `onFdReady(fd, events)` | needs a thread dispatcher at construction (throws). `onSignal()` is pure virtual but "not supported yet", so every subclass needs an empty override |
| `base/socketnotifier.h` | `SocketNotifier::create(fd, evTypes, enable)` → `sigActivated(notifier, evTypes)` | does not own or close the fd |
| `base/unixsignalsource.h` | `signalfd`-based signal delivery, obtained via `EventDispatcher::unixSignalSource()`. `addSignal`/`removeSignal` are ref-counted, `sigReceived(signum)` | `addSignal` blocks the signal **only in the calling thread** (`pthread_sigmask`). Other threads that leave it unblocked get process-directed signals instead. `removeSignal` **unblocks** the signal even if the application blocked it before |
| `base/autodisconnect.h` | RAII `AutoDisconnect{connection}` | move-only. Used for scoped connections, e.g. `ProvideWorker::run` |
| `base/precondition.h` | `ZYPP_PRECONDITION(expr[, "msg"])` → `INT` log + `std::terminate()` in debug **and** release | never use it for data-dependent errors (use `expected`) |
| `io/iodevice.h` | multi-channel buffered device: `read/readLine/channelReadUntil`, `canReadLine(ch)`, `sigReadyRead`, `sigChannelReadyRead(ch)`, `sigBytesWritten`, `sigAllBytesWritten`, `sigReadChannelFinished` | `waitForReadyRead` blocks ("do not use until there is a very good reason") |
| `io/asyncdatasource.h` | `IODevice` over raw fds: `openFds(readFds, writeFd)`, `closeWriteChannel()`, `flush()` (blocking), `sigReadFdClosed(ch, reason)`, `sigWriteFdClosed(reason)` | **does not close the fds** (the caller owns them). `closeWriteChannel()` **discards** unflushed data, so call `flush()` first (as `ProvideQueue::immediateShutdown` does) |
| `io/sockaddr.h` | `SockAddr` interface, `UnixSockAddr(path, abstract)` | only unix sockets implemented |
| `thread/asyncqueue.h` | `AsyncQueue<Msg>` (mutex + condvar, `push`, blocking `pop`, `tryPop`, manual `lock/pushUnlocked/notify`), `AsyncQueueWatch` → `sigMessageAvailable` in the receiver's event loop | create the watch in the consuming thread. `pushUnlocked` needs an explicit `notify()` |
| `pipelines/algorithm.h` | `firstOf(transform, default, predicate)`: first transformed element satisfying the predicate. `containsException<E>()`, `containsOneOfException<E…>()` | `firstOf` does `std::move(in)` on every element, **even for lvalue containers**. Exception tests rethrow internally, so they are slow in hot loops |
| `ui/userinterface.h` | `UserInterface::sendUserRequest(UserRequestRef)` → synchronous `sigEvent` | with no listener, the request silently gets the default answer. NG `Context` *is* a `UserInterface` |
| `core/bytearray.h` | `zyppng::ByteArray`/`UByteArray` = aliases of `zypp::ByteArray` (`vector<char>`) | it is not a separate type, so overloads on both collide |

---

## 17. Worked example: one source, sync in legacy, async in NG

The overlay headers (§2, §4.1) make the following file compile **unchanged**
in both builds. Rules: return `MaybeAwaitable<T>`, never write literal
`co_await`/`co_return` (C++17 legacy builds do not know them, and a discarded
`if constexpr (ZYPP_IS_ASYNC)` branch in a non-template still has to parse).
Keep the state captured by lambdas inside a coroutine frame.

```cpp
#include <zypp-core/ng/async/awaitable.h>     // resolves to zypp/… (sync) or zyppng/lib/… (async)
#include <zypp-core/ng/pipelines/expected.h>  // pulls async overloads under ZYPP_ENABLE_ASYNC
#include <zypp-core/ng/pipelines/operators.h> // overlay: plain | vs. coroutine |

namespace zyppng {
  // Leaf op supplied per tier: legacy returns expected<Pathname> directly
  // (e.g. via the sync Provide facade), NG returns Task<expected<Pathname>>.
  MaybeAwaitable<expected<zypp::Pathname>> fetchFile( ContextRef ctx, zypp::Url url );

  namespace {
    struct CountLinesLogic {
      ContextRef _ctx; zypp::Url _url;
      MaybeAwaitable<expected<int>> execute() {
        using namespace zyppng::operators;
        if ( !_url.isValid() )
          return makeReadyTask( expected<int>::error( ZYPP_EXCPT_PTR( zypp::Exception("bad url") ) ) );
        return fetchFile( _ctx, _url )
             | and_then( [this]( zypp::Pathname p ) { return countLines( p ); } ); // [this] → see below
      }
      expected<int> countLines( const zypp::Pathname &p );          // plain sync helper
    };
  }

  // Parameters BY VALUE (coroutine rule). In NG this function is a coroutine:
  // `impl` lives in its frame for the whole pipeline, so `[this]` stays valid.
  MaybeAwaitable<expected<int>> countRemoteLines( ContextRef ctx, zypp::Url url ) {
    CountLinesLogic impl{ std::move(ctx), std::move(url) };
    zypp_co_return zypp_co_await( impl.execute() );
  }
}
```

What the preprocessor and overlays turn this into:

| | Legacy (`zypp/` overlay) | NG (`zyppng/lib/` overlay) |
|---|---|---|
| `MaybeAwaitable<expected<int>>` | `expected<int>` | `Task<expected<int>, coro_lazy_execution>` |
| `makeReadyTask(x)` | returns `x` | `Task` that `co_return`s `x` (`task.h:560`) |
| `fetchFile(...) \| and_then(f)` | sync `operator\|` → `f(value)` at once | coroutine `operator\|`: `co_await` LHS, then `f`. Callback taken **by value** into the frame |
| `zypp_co_return zypp_co_await( e )` | `return ( e );` | `co_return co_await ( e );`. The function becomes a coroutine |
| caller | `auto r = countRemoteLines(ctx, u);` | `auto r = co_await countRemoteLines(ctx, u);` or `ZYPP_CORO_TEST_CASE` |

Why the wrapper is needed: if `countRemoteLines` returned
`impl.execute()` directly, legacy would work, but in NG the lazy `Task` would
outlive `impl` (a local), and `[this]` would dangle when the pipeline resumes.
The same applies to `[&]` captures in any non-coroutine function that returns
a pipeline (`Provide::copyFile` captures `[&]` but uses no captured state,
which is safe but fragile). Real examples of this pattern:
`zypp/zypp/ng/workflows/keyringwf.cc` (`ImportKeyFromRepoLogic`) and
`repo/workflows/repomanagerwf.cc`.

For callback-driven async primitives, NG uses the **`IOTaskAwaiter` bridge**
(see `Provide::provide`): create the awaiter in the coroutine frame, hand a
pointer to the producer (the `ProvideItem` calls `return_value()`), register a
destroy callback for cancellation, then `co_return co_await awaiter;`.

---

## 18. Toolchain details for the module build

* **Compilers.** No compiler or version check exists in CMake. The only
  compiler-specific logic is: GCC-only
  `-fconcepts-diagnostics-depth=4` (Debug), and for Clang a matching
  `ld.lld-<major>` / `ld.lld` (`zyppng/CMakeLists.txt`, and project-wide in
  the zypp-stack root `CMakeLists.txt` through `CMAKE_*_LINKER_FLAGS_INIT`).
  The libzypp root skips `-Wl,-as-needed` for Clang ("noisy warning"). The
  packaging and the developer build in this stack use **Clang 19**
  (`CMAKE_CXX_COMPILER=/usr/bin/clang++-19`,
  `CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS=/usr/bin/clang-scan-deps-19`,
  `CMAKE_GENERATOR=Ninja`). A `gcm.cache/` directory (GCC module cache, with a
  `zyppng-idstring.gcm`) in the stack root suggests someone tried GCC.
  **Whether GCC builds the full module is unverified.**
* **No hand-written `-fmodules*` flags.** Module support comes entirely from
  CMake's `FILE_SET CXX_MODULES` + dependency scanning (`clang-scan-deps` for
  Clang). Scanning must be enabled per consuming target:
  `CXX_SCAN_FOR_MODULES ON` on the tests (comment: CMake skips scanning when the
  target was created under a policy older than CMP0155). No `import std;`. The
  standard library comes in through `#include` in each global module fragment.
* **Generator.** The Ninja generator is what packaging and the stack use. Use it.
  Makefile generator support for C++ modules depends on your CMake version
  (unverified for this tree).
* **Step by step (standalone libzypp checkout):**
  1. `zypper in clang19 lld19 ninja cmake` (≥3.28) plus the usual -devel
     packages (libsolv static, glib2, gobject-introspection, sigc++2, yaml-cpp,
     libcurl, rpm, gpgme, zchunk, udev).
  2. `cmake -G Ninja -S . -B build -DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19 -DBUILD_LIBZYPPNG=ON -DENABLE_BUILD_TESTS=ON -DENABLE_DEVEL_BUILD=ON`
     (add `-DBUILD_LIBZYPP=OFF` for an NG-only build). Check that the configure
     output shows `zyppng: Clang 19 — using linker: …ld.lld-19` and the
     "ZyppNG devel build enabled" warning.
  3. `ninja -C build zyppng-objlib` first. Module ordering errors show up here
     quickly, then build everything.
  4. `ctest --test-dir build/zyppng --output-on-failure`. `Provider_test` needs
     the workers in `build/zyppng/tools/workers` (`ZYPPNG_WORKERS_DIR`) and
     nginx/vsftpd/squid for the network cases.
  In the zypp-stack checkout, first apply `libsolv.patch`, then configure the
  stack root the same way. It forces `BUILD_LIBZYPP` and `BUILD_LIBZYPPNG` on.
* **Known module build constraints documented in code:**
  `ZYPPNG_MODULE_INTERFACE_UNITS` must stay in bottom-up order
  (`zyppng/lib/zypp/CMakeLists.txt`). GMF-attached free operators of bridged
  types are not found by ADL across the module boundary: re-export
  (`edition.cppm`, `locale.cppm`, `reskind.cppm`, `solvattr.cppm` do
  `export import`) or add explicit wrappers (`base/strmatcher.cppm` "ADL
  wrappers"). Forward declarations shared between partitions must carry the
  same `export` linkage (`poolcomponents.cppm`, `poolmember.cppm` comments).
  Link order: `zyppng-curl`, `zyppng-media` before `zyppng-core`. Clang ≤19 needs the
  deduced-`auto` sync `operator|` (§5). The zyppng tests still use Boost.Test
  (`-DBOOST_TEST_DYN_LINK`), despite the "no new Boost" rule.

---

## Pitfalls & special cases

* **NG is not just experimental.** `zypp-logic/*/ng` and `zypp/zypp/ng` run in every
  `zypper` call (RepoManager, refresh, key import, checksum/signature checks,
  MediaCurl2, preloader, zypp-rpm commit, the global libsolv pool). Treat
  changes there with legacy-level caution.
* The same `#include` resolves to **different headers per tier** (sync vs async
  overlay). Test both builds when you touch `zypp-logic/*/ng` or anything that
  uses `MaybeAwaitable`/`operator|`.
* `ZYPP_ENABLE_ASYNC` blocks inside `zypp/zypp/ng/**` are dead code (never
  compiled). They can rot silently.
* `logichelpers.h` (`LogicBase`, `AsyncOp`, `SyncContext`) refers to types that
  no longer exist. Do not copy this pattern.
* The EventDispatcher is held **weakly** per thread. Timers and event sources
  throw if none exists at construction, and stop working silently if their
  dispatcher dies. Keep an `EventDispatcherRef` member declared *before* any
  async member (see `MediaNetworkRequestExecutor`).
* Objects are bound to the thread (dispatcher) they were created in. Do not use
  them from another thread. Use `AsyncQueue`/`Wakeup` to talk across threads.
* `Base::connect` locks the receiver through `shared_from_this`. A NG object
  that is used without a `shared_ptr` (stack or `unique_ptr`) hits `bad_weak_ptr`
  → `ERR` + `assert(false)` in debug builds.
* Deleting a `Signal` during emission asserts in debug builds. Old sigc++ needs
  `ENABLE_SIGC_BLOCK_WORKAROUND`.
* `Task::registerNotifyCallback` runs inside `final_suspend`. Destroying the Task
  there is a use-after-free. Post to the loop instead.
* Lazy `Task` that was already `start()`ed and is then `co_await`ed: the
  awaiter's `Running` branch returns `noop_coroutine()` **without registering a
  continuation**, so the awaiting coroutine never resumes. Either await or
  start/poll, never both.
* `co_await signal` connects only when the `co_await` expression is evaluated. A
  signal emitted synchronously before that is lost.
* The async `mtry` (`zyppng/lib/zypp-core/ng/async/pipelines/mtry.hpp`) is a
  coroutine with `F&&`/`Args&&` parameters, which breaks the by-value rule.
  Watch for dangling references.
* NG `Context::execute()` / `joinPipeline()` (`context.cppm`) still use
  `op->isReady()` / `res->get()` from the `AsyncOp` era. `Task` has no
  `operator->`, and `execute` never calls `start()` on a lazy task. They will
  not compile or will hang as soon as someone instantiates them. Fix them before
  you use them.
* `LazyMediaHandle::parent()` (`zypp-logic/zypp-media/ng/lazymediahandle.h`)
  returns `const Ref<T>&` to the temporary from `weak_ptr::lock()`. That is a
  dangling reference (UB).
* `expected<T,E>::unwrap() const` contains `std::is_same_v<E, std::exception_ptr>()`
  (an extra `()`). It is ill-formed once instantiated. Use non-const `unwrap()`.
* Legacy `Context::defaultContext()` is a function-local static.
  `FileChecker` creates fresh `Context::create()` instances instead. Both
  delegate to `getZYpp()->keyRing()` / `ZConfig::instance()`, so they still
  depend on global state. Calling the key workflows without a ZYpp instance
  acquires the ZYpp lock (chapter 08).
* Only **one** libsolv pool exists per process (`StringPool` singleton, shared by
  the legacy `PoolImpl` and the NG `sat::Pool`). `POOL_DESIGN.md` makes this
  explicit.
* `ZyppApplication` replaces the process-wide GLib default log handler. That can
  surprise host applications. Its user-data API is a stub.
* The libzypp-glib SONAME/version comes from `libzypp-glib-version.inc`, not from
  `zypp/VERSION.cmake`. Bump them independently.
* Worker binaries are only found under `${libexecdir}/zyppng/workers`. Legacy
  `libzypp` does not ship them.
* **NG sat components are never attached** (§13). `NamespaceComponent` does not
  install the libsolv `nscallback`, `ArchitectureComponent::setProvider()` does
  not invalidate, and `NamespaceProvider::notifyDirty()` drops invalidations
  with an `ERR`. Call `attach(pool)` by hand until `PoolComponentSet` does it.
* `Pool::setDirty()` inside `prepare()` (passes 2–4, e.g. from a component's
  `prepare()`) is a `ZYPP_PRECONDITION`: `std::terminate()` in release builds
  too. Only `checkDirty()` may call it.
* **Provide file cache, inverted death timer** (`ProvidePrivate::onPulseTimeout`):
  `if ( now - *deathTimer < 20s ) release`. Files are released on the first pulse
  (~5 s) after they become unreferenced. If more than 20 s pass between pulses
  (blocked loop), they are **never** released until the `Provide` dies. The
  intended check is `>=`.
* **Download scheduler, missing `break`** (`doSchedule`, the "decommission idle
  queues" branch for Downloading workers): after the first successful
  `startup()`+`enqueue()` the loop continues over the remaining mirrors. It can
  start more workers and enqueue the *same* request again. `enqueue()`
  reassigns the request id, so the first worker's answer is "unknown request
  ID" and its active item never leaves that queue. This only happens when ≥20
  download workers exist.
* **CPU worker name collision**: new CPU queues are named
  `scheme#<existingSchemeWorkers>`. After the laziest worker (of *any* CPU
  scheme) is decommissioned, the count can match a live queue name, and
  `_workerQueues[str] = q` drops (and shuts down) a busy worker. Likely bug,
  unverified at runtime.
* **Cache-miss requeue** (`ProvideQueue::processMessage`): the request is put
  back with `requestId = InvalidId` (never renumbered, because `scheduleNext()`
  does not call `enqueue()`), and `dequeueActive()` clears its `currentQueue`.
  Two concurrent cache misses on one queue share id `0xFFFFFFFF`. A cancel
  during that window searches the wrong list. Likely bug, unverified.
* `schemeConfig()` spawns a worker **synchronously** just to read its caps. The
  first request for a scheme blocks the loop. A missing worker binary fails
  every request for that scheme ("Failed to query scheme config.").
* Mirror failover does not trigger on `Timeout` (407, commented out), only on
  403/404/406/409. Mounting and CPU-bound schemes never use mirrors.
* Mounted media are detached only when a scheduler run happens. Downloading
  "attachments" stay for 1 h. There is no timer.
* `ProvideMessage` serialisation drops empty-string values and turns
  everything into strings. A required field set to `""` fails parsing on the
  other side. Never use `-` in field names (old STOMP parsers, bsc#1227793).
* `WorkerCaps::protocol_version` is never checked. Change the wire format
  only together with all workers (they ship in the same package).
* `ProvideWorker::handleSingleMessage` logs "Received Cancel for unknown
  request" for *known* requests and stays silent for unknown ones (the log line
  is in the wrong branch). Do not trust that message when you debug cancels.
* `Provide::ejectDevice()` opens the tray **in the controller process**, not in
  the disc worker, and does nothing when `queueRef` is empty.
* `UnixSignalSource::removeSignal()` unconditionally unblocks the signal in the
  calling thread, even if the application had blocked it before.
* `AsyncDataSource::closeWriteChannel()` discards pending writes. Call `flush()`
  first. It never closes the fds you passed to `openFds()`.
* `zyppng::firstOf` moves from the elements of lvalue containers.
* The doc comment in `zypp/zypp-core/ng/async/awaitable.h` shows
  `if constexpr (ZYPP_IS_ASYNC) { co_await …; }`. That does not compile in the
  C++17 legacy build outside templates. Use `zypp_co_await`/`zypp_co_return`.
* A non-coroutine function that returns `task | lambda` with `[&]`/`[this]`
  captures of locals dangles in NG (lazy Task). Wrap it as in §17.

## Where to start when debugging

* **Logs**: NG code uses the normal zypp logger (chapter 02). Useful log groups
  and markers: `zypp::repomanager` (NG RepoManager/workflows), `ZYPP_MEDIA_PROVIDE`
  (async Provide, enable with **`ZYPP_MEDIA_PROVIDER_DEBUG=1`**), `zyppng`
  (glib layer), `zypp-rpm` (commit helper output), and messages like
  "Creating the Event Dispatcher for thread", "Requested Event Dispatcher … but
  none was created", "Ignoring signal emit due to a bad_weak_ptr", "Unhandled …
  exception in coro", "Awaiter destroyed before Task became ready".
* **Env vars**: `ZYPP_CURL2=0|1` (switch HTTP backend, isolates NG network
  issues), `ZYPP_PCK_PRELOAD=0|1` (parallel preloader), `ZYPP_FORK_BACKEND=pfork|gspawn`
  (process spawning), `ZYPP_SINGLE_RPMTRANS` (see chapter 08), `ZYPP_MEDIA_PROVIDER_DEBUG`.
  For curl request traces, see chapter 07 (`mediadebug_p.h`).
* **Is it a legacy-path NG bug?** Check whether the stack contains
  `zyppng::RepoManager`, `*Workflow::`, `MediaCurl2`, `CommitPackagePreloader` or
  `MediaNetworkRequestExecutor`. If yes, fix it in `zypp/zypp/ng` or
  `zypp-logic/*/ng` and release it through libzypp.
* **Tests**:
  * Shared framework in legacy (sync) mode: `zypp/tests/zyppng/*`
    (`EventLoop_test`, `Process_test`, `AsyncDataSource_test`,
    `UnixSocket_test`, `NetworkRequestDispatcher_test`, `EvDownloader_test`,
    `StompMessageStream_test`).
  * NG module and coroutines: `zyppng/tests/async/{Task,Pipelines}_test`,
    `sat/{PoolNg,PoolComponentSet}_test`, `config/*_test`,
    `media/Provider_test` (needs the tvm worker and nginx. Disable with
    `DISABLE_MEDIABACKEND_TESTS`).
  * Write coroutine tests with `ZYPP_CORO_TEST_CASE` (`zypp-logic/tests/lib/TestTools.h`).
* **Worker problems**: run a worker by hand (`build/zyppng/tools/workers/zypp-media-http`)
  and feed it STOMP frames, or check `ProvideQueue::startup` messages ("Trying to
  start …", "Failed to find worker …", "… is not executable").
* **Provide scheduling problems**: set `ZYPP_MEDIA_PROVIDER_DEBUG=1` and follow
  `"Triggering the schedule timer (<reason>)"`, `"Start scheduling for scheme:"`,
  `"Current stats:"` (worker/connection counts), `"End of line, deferring
  request"`, `"Reached maximum nr of connections"`, `"Found idle worker:"`. For
  attach issues look for `"Found item providing the same medium"`, `"Detaching
  medium <id> for baseUrl"`, `"Registered new media attachment with ID"`. For worker
  handshakes, look for `"Received config for worker: … Worker Type: … Flags: <bits>"`.
* **Worker-side logs** arrive in the main log as raw zypp-format lines
  (`ZyppLogFormat`) with the worker name as thread name (log group
  `ProvideWorker`). `"Remembering message for later"` means the worker was
  blocked in an auth or media-change round trip.
* **Media change / CD flows without hardware**: use the tvm worker (§15.4). Write
  `tvm.conf` into the provider root and start from `Provider_test`'s
  `tvm_medchange`.
* **NG sat pool**: `"pool_createwhatprovides..."` (MIL) marks Pass 3. A
  `PRECONDITION VIOLATED` line in the `INT` channel followed by abort is a
  `ZYPP_PRECONDITION` (`precondition.h`). Check for a second `sat::Pool` or a
  `setDirty()` during `prepare()`. `"NamespaceProvider not attached"` → §13.
* **Module build errors**: first check the order in `ZYPPNG_MODULE_INTERFACE_UNITS`,
  missing `export import` of GMF-attached operators (see `edition.cppm`), and
  whether the target has `CXX_SCAN_FOR_MODULES ON`. Use Clang 19 + Ninja + CMake ≥ 3.28,
  as the spec does.
* Related chapters: [Core infrastructure](02-core-infrastructure.md),
  [Repositories](05-repositories-and-services.md),
  [GPG/KeyRing](06-security-gpg-keyring.md), [Media](07-media-and-downloads.md),
  [Commit/zypp-rpm](08-target-commit-and-rpm.md),
  [Configuration](09-configuration-and-environment.md), [Testing](11-testing.md),
  [Release & packaging](12-release-packaging-and-compat.md).
