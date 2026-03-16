# Architectural Concept: The Modular SAT Pool Orchestrator

This document defines the architectural foundations of the **Next-Generation (NG) SAT Pool** orchestrator in `libzypp`. It describes the service-oriented system designed to manage solver state and its associated modular logic.

---

## 1. The Orchestration Model
In `zyppng`, the **Pool** is an **Active Orchestrator** and a **Service Registry**. Its primary responsibility is to coordinate the lifecycle of modular logic units called **PoolComponents**.

### Key Principle: Explicit Context
Every operation requires a reference to a specific `Pool` instance. By eliminating global singletons, we enable:
*   **Multi-Root Support**: The ability to manage different filesystem roots (e.g., `/` and `/mnt`) simultaneously within a single process.
*   **Instance Isolation**: Ensuring that operations on one solver state do not leak into another.
*   **Future-Proofing**: While `libsolv` currently couples its internal `StringPool` to a single solver instance, the orchestrator is designed to support a shared `StringPool` across multiple solver instances if the underlying engine ever allows it.

---

## 2. The Core: `sat::Pool`
The `sat::Pool` class manages the raw `libsolv` state and coordinates high-level C++ services through the **Stability Loop** (`prepare()`):
1.  **Stage Order**: Components are executed in a deterministic sequence (Environment → Policy → Metadata → Logging).
2.  **Convergence**: If a component modifies the pool (e.g., a Namespace provider triggers a repository load), the Pool's `SerialNumber` increments, and the loop repeats.
3.  **Finality**: The loop completes when all components reach a stable state, ensuring data consistency across the entire stack.

---

## 3. PoolComponents: Modular Logic
All domain-specific pool logic (architectures, namespace callbacks, metadata caches) is encapsulated in **PoolComponents**.

### Lifecycle
*   **Attach**: Initial registration and dependency wiring.
*   **Prepare**: Synchronizes internal logic with the latest solver data during the Stability Loop.
*   **Invalidate**: Triggered when the Pool is marked "dirty." Components must clear their internal caches.
*   **Delta Events**: `onRepoAdded` and `onRepoRemoved` allow for optimized incremental updates.

### Component Stages
To prevent race conditions between services, components are grouped into deterministic execution stages:
1.  **Environment**: The foundation (Systems Arches, Locales, Namespace Providers).
2.  **Policy**: High-level solver rules and policy hints.
3.  **Metadata**: Caches and data structures built on raw solver IDs.
4.  **Logging**: Diagnostic systems that report on the final stable state.

---

## 4. Thin Handles: `Solvable` and `Repository`
In `zyppng`, handles represent entities within the pool but remain logically "thin."
*   **Pure Indices**: A handle is essentially a lightweight wrapper around an integer index or a `libsolv` pointer. It contains no business logic.
*   **Context-Aware**: All handles derive from **`PoolMember`**, providing a guaranteed path back to their parent `sat::Pool` instance. This ensures that indices are never used with the wrong pool instance.
*   **Logic Delegation**: Policies (e.g., "Is this solvable blacklisted?") are not part of the handle; they must be queried via the appropriate `PoolComponent`.

---

## 5. Programming Patterns for Developers

### Accessing Services
Never add hardcoded methods to the `Pool` class for new logic. Instead, retrieve the specialized component:
```cpp
auto &policy = pool.component<PackagePolicyComponent>();
if (policy.isRetracted(solvable)) { ... }
```
