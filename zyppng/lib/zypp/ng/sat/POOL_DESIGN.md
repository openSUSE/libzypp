# Architectural Design: The NG SAT Pool Orchestrator

This document describes the architecture of the **Next-Generation (NG) SAT Pool**
in `libzypp`. It is the authoritative reference for all contributors working on
`zyppng::sat`.

---

## 1. Design Goals

The NG pool has three non-negotiable properties:

1. **No global state.** Every operation receives an explicit `Pool &` reference.
   This is designed to enable multiple isolated pool instances in a single
   process (e.g. two different filesystem roots). However, due to the current
   libsolv constraint that the `StringPool` is embedded by value inside `CPool`
   and cannot be shared across instances, **only one `Pool` instance may exist
   at a time in practice**. The explicit-context design is retained so that this
   restriction can be lifted without an API break if libsolv ever lifts it.
2. **Deterministic lifecycle.** Construction, mutation, preparation, and
   destruction all follow a precisely defined sequence with no hidden
   side-effects.
3. **Index integrity.** The `whatprovides` index — libsolv's central data
   structure for dependency queries — is never accessible unless it is known to
   be valid. This invariant is enforced at the type level by `PreparedPool`.

---

## 2. Core Types at a Glance

| Type | Role |
|---|---|
| `Pool` | Owns the libsolv `CPool`. Lifecycle manager and component registry. |
| `PreparedPool` | Move-only, non-owning view guaranteeing a valid `whatprovides` index. |
| `PoolMember<T>` | CRTP base giving any handle type a `pool()` accessor. |
| `IPoolComponent` | Pre-index component interface (runs before `whatprovides` is built). |
| `IPreparedPoolComponent` | Post-index component interface (runs after `whatprovides` is built). |
| `PoolComponentSet` | Registry and dispatcher for all registered components. |

---

## 3. Pool Ownership and Identity (`CPool::appdata`)

`Pool` is **non-copyable and non-movable**. The identity contract is enforced
through libsolv's `CPool::appdata` pointer — a free user-data slot that libsolv
provides on every pool instance and makes no use of itself:

- **Construction** — `Pool::Pool()` asserts `_pool->appdata == nullptr` (only one
  `Pool` instance per `CPool` is legal) and then writes `this` into `appdata`.
- **Destruction** — `Pool::~Pool()` calls `clear()` and then writes `nullptr`
  into `appdata`.

`ZYPP_PRECONDITION` (see §8) enforces both invariants unconditionally in debug
and release builds.

### How handles recover their Pool

**`Repository`** — A `CRepo` carries a direct back-pointer to its owning `CPool`
(`repo->pool`). `poolFromType<Repository>` follows that pointer and reads
`appdata` to recover the `Pool &`. This is straightforward and has no global
state.

**`Solvable`** — A solvable ID is just an integer index. Given only that integer
and no `CRepo` pointer, there is no direct path from `id` to `CPool` without
already having the pool. `poolFromType<Solvable>` therefore exploits a current
implementation constraint: `StringPool::instance().getPool()` returns the same
`CPool` that the `Pool` instance was constructed over. It then reads `appdata`
from that pointer to recover the `Pool &`.

This is an **internal implementation detail of `solvable.cc`** and is not
visible to API users. It is valid as long as there is exactly one `Pool`
instance in the process. If libsolv ever supports multiple independent string
pools, this specialisation will need to change — but the `PoolMember<T>` API
contract will remain identical.

---

## 4. Thin Handles and `PoolMember<T>`

`Repository` and `Solvable` are **thin handles**: lightweight wrappers around an
integer ID or a raw libsolv pointer. They carry no business logic and no pool
reference of their own.

Both inherit the CRTP base `PoolMember<Derived>`:

```cpp
template <typename Derived>
class PoolMember {
public:
    Pool &       pool();
    const Pool & pool() const;
};
```

`pool()` is implemented by calling `detail::poolFromType<Derived>`, whose
specialisations are described in §3.

The primary templates are intentionally left **undefined**. A missing
specialisation produces a linker error rather than a silent fallback to any
global singleton. There is no global singleton.

**Logic delegation** — Policies (e.g. "Is this solvable retracted?") are not
part of the handle. They must be queried via the appropriate `PoolComponent`:

```cpp
auto & policy = pool.component<PackagePolicyComponent>();
if ( policy.isRetracted(solvable) ) { ... }
```

---

## 5. Pool Lifecycle

### 5.1 Construction

`Pool::Pool()` acquires a `CPool *` from `StringPool::instance().getPool()`,
asserts `appdata == nullptr`, and registers itself. No components are attached
at this point.

> **Note on `StringPool`:** libsolv embeds the string pool by value inside
> `CPool`, making it impossible to share a single `CPool` across multiple
> `Pool` instances without a libsolv-level change. `StringPool` therefore
> remains a process-wide singleton. This is a known architectural constraint,
> not a design choice.

### 5.2 Mutation — `setDirty()`

Any operation that changes pool state calls `setDirty()` with one of two
invalidation modes:

| Mode | Meaning |
|---|---|
| `PoolInvalidation::Data` | Structural change — repo content, architecture, rootfs. Pool data and the index are both stale. |
| `PoolInvalidation::Dependency` | Only external context changed (e.g. locale set). Pool data is still valid but the `whatprovides` index must be rebuilt. |

In both modes `setDirty()`:
1. Dispatches `onInvalidate(pool, invalidation)` to all registered components,
   passing the mode so components can react proportionally.
2. Calls `::pool_freewhatprovides` — marks the index as stale.

Only `Data` additionally bumps `_serial`. A `Dependency` invalidation therefore
does not trigger Pass 2 (`notifyPrepare`) in the next `prepare()` call — only
Pass 3 (index rebuild) and Pass 4 (`notifyPrepareWithIndex`).

`setDirty()` is **illegal inside `prepare()`** (asserted in debug builds via
`_preparing`). The only legal point to call it during the prepare sequence is
inside a component's `checkDirty()` callback (Pass 1).

### 5.3 Preparation — `Pool::prepare()`

`prepare()` is the **Stability Loop**. It synchronises all components with the
current pool state and rebuilds the `whatprovides` index if necessary. It
returns a `PreparedPool` (§6).

The four passes run in order:

| Pass | Name | Condition | What happens |
|---|---|---|---|
| 1 | `notifyCheckDirty` | Always | Components probe external state; calling `setDirty()` is legal here. |
| 2 | `notifyPrepare` | Serial changed | Pre-index component work (stage/priority order). |
| 3 | Index rebuild | `whatprovides == nullptr` | `pool_addfileprovides` + `pool_createwhatprovides`. |
| 4 | `notifyPrepareWithIndex` | Serial changed **or** index rebuilt | Post-index component work (stage/priority order). |

An early return after Pass 3 skips Pass 4 when neither the serial nor the index
changed (nothing to do).

`_preparing` is set via `zypp::DtorReset` for RAII exception safety — it is
cleared automatically even if a component throws.

### 5.4 Reset — `Pool::clear()`

`clear()` performs a **hard reset**: all repositories, solvables, and the
`whatprovides` index are destroyed.

```
notifyReset(pool)                    // components drop all pool-derived caches
pool_freewhatprovides                // destroy the index
pool_freeallrepos(reuseIds=true)     // destroy all repos and solvables
_serialIDs.setDirty()               // bump ID serial (IDs were reused)
_serial.setDirty()                  // bump content serial
```

`clear()` deliberately does **not** call `setDirty()` and does **not** fire
`onInvalidate`. `onReset` is categorically stronger: it means "all your
pointers are dead, drop everything unconditionally". Firing `onInvalidate`
after `onReset` would be redundant and confusing.

Components survive `clear()` — their registrations remain intact. If a truly
clean slate is required, destroy the `Pool` and create a new one.

### 5.5 Destruction — `Pool::~Pool()`

```cpp
Pool::~Pool() {
    clear();                       // fires onReset, destroys all repos/solvables
    _pool->appdata = nullptr;      // revoke ownership
}
```

After `~Pool()` returns, any `Repository` or `Solvable` handle that still holds
an ID derived from this pool is a dangling handle. Using it is undefined
behaviour.

---

## 6. `PreparedPool` — Index Integrity at the Type Level

`PreparedPool` is a **move-only, non-owning view** of a `Pool` that carries the
static guarantee that the `whatprovides` index was valid at the moment
`Pool::prepare()` returned it.

```cpp
// Only obtainable as the return value of Pool::prepare():
PreparedPool pp = pool.prepare();

// All whatprovides queries live here, not on Pool:
Queue q = pp.whatMatchesDep( attr, cap );
```

Key properties:

- **Non-constructible** outside of `Pool::prepare()` (constructor is `private`,
  `Pool` is a `friend`).
- **Non-copyable** — there is exactly one owner of a `PreparedPool` at a time.
- **Non-owning** — destroying a `PreparedPool` does nothing to the libsolv index.
  The index lives inside `CPool` and is managed by the owning `Pool`.
- **Debug staleness detection** — in debug builds, `PreparedPool` captures the
  pool's `SerialNumber` at construction. Every query asserts the serial has not
  changed, catching use-after-invalidate bugs at the call site.

> **Thread safety:** Concurrent reads through a single `PreparedPool` are safe as
> long as the owning `Pool` is not mutated concurrently. `PreparedPool` provides
> no synchronisation of its own.

---

## 7. PoolComponents — Modular Logic

All domain-specific pool logic is encapsulated in **PoolComponents**. Adding new
logic to `Pool` directly is prohibited; use a component instead:

```cpp
auto & policy = pool.component<PackagePolicyComponent>();
if ( policy.isRetracted(solvable) ) { ... }
```

### 7.1 Component Interfaces

Two interfaces split work across the prepare sequence:

**`IPoolComponent`** (pre-index, `InitStage`):
- `checkDirty(Pool &)` — probe external state; may call `pool.setDirty()`.
- `prepare(Pool &)` — pre-index work (runs when serial changed).
- `onInvalidate(Pool &, PoolInvalidation)` — clear caches on `setDirty()`.
- `onRepoAdded / onRepoRemoved` — incremental repo-delta events.
- `onReset(Pool &)` — hard reset; drop all pool-derived data unconditionally.

**`IPreparedPoolComponent`** (post-index, `PreparedStage`):
- Same lifecycle callbacks as above.
- `prepare(PreparedPool &)` — post-index work (runs when serial changed or index was rebuilt).

A component **must not** implement both interfaces. Split into two cooperating
components instead.

### 7.2 Execution Stages

Pre-index stages (`InitStage`), executed in order:

| Stage | Purpose |
|---|---|
| `Environment` | Architecture, locales, namespace callbacks — the foundation. |

Post-index stages (`PreparedStage`), executed in order:

| Stage | Purpose |
|---|---|
| `Policy` | Blacklists, reboot specs, storage policy. |
| `Metadata` | ID-indexed metadata caches. |
| `Logging` | Diagnostic components. |

Within each stage, components are sorted by `priority()` (lower runs first).
`stable_sort` preserves registration order for equal priorities. Sorting is
deferred and performed once per registration change (dirty-bucket flag).

### 7.3 The `onReset` Event

`onReset` is **categorically stronger** than `onInvalidate`:

| Event | Meaning | Required action |
|---|---|---|
| `onInvalidate(Data)` | Pool data changed; recompute lazily. | Clear all caches; recompute on next access. |
| `onInvalidate(Dependency)` | Only `whatprovides` index stale; pool data unchanged. | Clear only index-derived caches; keep data-derived state. |
| `onReset` | All pool memory freed; all handles are dead. | Drop everything unconditionally, now. |

After `onReset` returns, the component must not hold any `Repository`,
`Solvable`, or libsolv ID that was derived from the now-destroyed pool state.
User-configured data (e.g. requested locales, solver policy specs) must be
**preserved** across `onReset` — only pool-derived caches are dropped.

### 7.4 Component Registration

Components are registered lazily on first access via `pool.component<T>()`.
`PoolComponentSet::assertComponent<T>()` inserts the component exactly once and
wires it into all applicable dispatch lists.

---

## 8. `ZYPP_PRECONDITION` — Always-On Invariant Enforcement

`ZYPP_PRECONDITION(expr)` and `ZYPP_PRECONDITION(expr, "message")` are defined
in `zypp-core/ng/base/precondition.h`.

Unlike `assert()`, which is stripped by `NDEBUG`, `ZYPP_PRECONDITION` fires in
**both debug and release builds**. A violated precondition logs to the `INT`
(internal error) channel and calls `std::terminate()`. It is not recoverable and
must never be caught.

Use it for invariants that, if violated, indicate a programming error with no
safe recovery path — for example, calling `pool()` on a null handle, or
constructing two `Pool` instances over the same `CPool`.

```cpp
ZYPP_PRECONDITION( _pool->appdata == nullptr,
    "Only one zyppng::sat::Pool instance per CPool is permitted" );
ZYPP_PRECONDITION( _id != detail::noSolvableId,
    "pool() called on a null Solvable handle" );
```

In C++20 and later the violation branch is annotated `[[unlikely]]` as a free
optimiser hint.

---

## 9. Serial Numbers and Cache Invalidation

`Pool` maintains two serial numbers:

| Serial | Meaning |
|---|---|
| `_serial` | Bumped on any data change (`setDirty(Data)`) and on `clear()`. Components use this to detect whether pre- or post-index work is needed during `prepare()`. |
| `_serialIDs` | Bumped when libsolv reuses solvable IDs (on `clear()` and on `_deleteRepo` when the pool becomes empty). Consumers that store solvable IDs (e.g. `ResPool::PoolItem`) must invalidate their caches when this serial changes. |

`SerialNumberWatcher::remember(serial)` returns `true` the first time it sees a
given serial value, and `false` on all subsequent calls with the same value.
`Pool::prepare()` uses this to gate Pass 2 and Pass 4 on whether the serial
actually changed.
