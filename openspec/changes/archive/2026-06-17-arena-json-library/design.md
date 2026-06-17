## Context

Smart Grid One serializes its entire patch to JSON, and that serialization runs **on the audio thread**. `StateInterchange` (`private/src/StateInterchange.hpp`) is a lock-free handshake: the message thread sets `m_saveRequested`, and the audio thread, inside `ProcessFrame` → `HandleStateInterchange` (`TheNonagonSquiggleBoy.hpp:243`), calls `ToJSON()` and acks the result back. The message thread then `Dumps()` the tree to a string and writes it to disk (`MainComponent::HandleStateInterchange`).

Today the `JSON` type has two interchangeable backends behind one API:
- `private/src/JanssonAdapter.hpp` — `json_t*`, used under `EMBEDDED_BUILD` (tests).
- `private/src/JuceSon.hpp` (non-`EMBEDDED_BUILD` branch) — `juce::var`/`DynamicObject`, used by the plugin.

Both allocate on the heap during `ToJSON` (jansson `malloc`; JUCE `new DynamicObject`). On the audio thread this is a real-time hazard (unbounded latency, lock contention in the allocator). The arena removes heap allocation from the audio-thread build path and, in doing so, lets us collapse the two backends into one self-contained library and drop the `external/jansson` submodule.

Constraints that shape the design:
- The audio thread may **bump a pointer** but may not call `malloc`/`free`/`new`.
- The serialized tree is **pointers into the arena**, so the arena must outlive the message-thread `Dumps`.
- The new serializer/parser must round-trip existing on-disk patches (`patches/WRLD.BLDR.json`, ~358 KB).
- The `JSON` API surface is used at ~30–40 sites and should keep the same method names.

## Goals / Non-Goals

**Goals:**
- Zero system-allocator calls on the audio thread during `ToJSON`.
- A single, header-only JSON library depending on neither jansson nor JUCE.
- Same `JSON` API surface (names/semantics) so call-site churn is mechanical, not structural.
- Bidirectional: arena-backed build (`ToJSON`) **and** parse (`Loads`/`FromJSON`).
- Round-trip compatibility with existing patch files.
- Failure handled by a flag + null propagation + message-thread doubling retry, not by crashing.

**Non-Goals:**
- Changing the `StateInterchange` request/ack protocol shape (we extend ownership, not rewrite the handshake).
- A general-purpose, standards-perfect JSON implementation — only the subset the patch format uses (objects, arrays, strings, integers, reals, booleans, null).
- Reworking `VoiceAllocator`/`FixedAllocator`/`SegmentedAllocator` — they stay as-is; the arena is a new, separate allocator.
- Streaming/incremental parsing; we parse whole strings the message thread already holds.

## Decisions

### Decision: A new bump allocator, not a reuse of existing allocators
None of `FixedAllocator<T,N>` (typed, fixed-count, free-list), `SegmentedAllocator` (typed, growable, freeable), or `VoiceAllocator` (voice stealing) is a heterogeneous never-free bump allocator. The arena is ~a few dozen lines: `{ char* base; size_t cap; size_t off; bool failed; }` with `void* Alloc(size_t n, size_t align)` that aligns `off`, checks `off + n <= cap`, and either returns `base + off` (advancing `off`) or sets `failed` and returns `nullptr`.
- **Alternative — extend `SegmentedAllocator`**: rejected; it is typed and per-object freeable, the opposite of what we want, and would drag in `std::vector`/`new` on the audio path.

### Decision: The arena is the factory object
Allocation happens at the **leaves** (`arena.Integer(5)` deep inside `StateSaver::ToJSON`), so the arena must reach them. We make the arena the factory: `arena.Object()`, `arena.Array()`, `arena.String(s)`, `arena.Integer(n)`, etc. `ToJSON(arena, ...)` threads the arena down explicitly.
- **Alternative — explicit `JSON::Integer(arena, 5)` everywhere**: same reach, noisier call sites.
- **Alternative — thread-local/ambient arena**: minimal churn but hidden coupling and fragile across the message/audio boundary; rejected for a real-time codebase where the arena's thread affinity must be obvious.

### Decision: Copy-on-grow contiguous container blocks
Objects and arrays hold a contiguous block of entries for O(1) indexed access. On overflow, allocate a new block of ≥2× capacity from the arena, `memcpy` existing entries, and abandon the old block (never reclaimed until arena reset).
- **Rationale**: O(1) `GetAt`/`Size`; simple; the wasted abandoned blocks are bounded (geometric series ≤ ~2× the final block) and vanish when the arena resets.
- **Alternative — linked-list nodes (no copy, no waste)**: avoids abandoned blocks but makes indexed reads O(n) and fragments cache locality. Given patches are written-once/read-once but we still index arrays during load, the contiguous block was chosen for predictable access.
- **Object key lookup** is a linear scan over the contiguous entry block (objects are small and keys are short); no hashtable, so no separate hash storage to allocate.

### Decision: Failure = sticky flag + null, checked once at the root
`Alloc` failure sets `failed` and returns null. All build/read ops are null-tolerant (the existing adapters already guard `if (m_json)` / `if (isObject)`), so a mid-build exhaustion collapses the tree to a null root. Callers check `root.IsNull()` (or `arena.failed`) exactly once.
- **Rationale**: keeps the ~30–40 `ToJSON` bodies almost unchanged — no error threading through every call.
- **Alternative — `expected<JSON>`/status returns at each step**: explicit but would rewrite every builder; rejected.

### Decision: Retry and buffer ownership live on the message thread
The arena's backing buffer is `malloc`-ed on the **message thread** and handed to the audio thread, which only bumps. On a null root, the message thread frees and re-allocates at ≥2× capacity, then re-requests via `StateInterchange`. `StateInterchange` owns the arena across the save round-trip (build on audio thread → `Dumps` on message thread → `AckSaveCompleted`), guaranteeing the tree's backing memory outlives every read. `Incref`/`Decref` become no-ops (lifetime = arena lifetime).
- **Capacity sizing**: a real patch is ~358 KB serialized; the in-arena tree (node headers + key/string copies + abandoned grow blocks) is several MB. Default starts ~8 MB so retry effectively never fires; doubling is a pathological-patch backstop.

### Decision: Both backends collapse into the new library
The new header replaces `JanssonAdapter.hpp` and the JUCE branch of `JuceSon.hpp`; `#ifdef EMBEDDED_BUILD` no longer forks JSON. `external/jansson` is removed from `private/test/CMakeLists.txt` and the JUCE project. `Loads`/`Dumps` use the new parser/serializer in both builds.

## Risks / Trade-offs

- **Audio-thread `ToJSON` re-entrancy across retries** → The audio thread must not block waiting for a bigger arena; the retry is a *new request* on a later frame. Risk: a save in flight while a retry is requested. Mitigation: keep the existing single-in-flight `m_saveRequested`/`m_saveCompleted` gating; a null ack transitions back to "request" state cleanly.
- **Arena outlives its build but is reused too early** → If the message thread reuses/frees the arena before `Dumps` completes, the tree dangles. Mitigation: `StateInterchange` owns the arena and only frees/reuses after `AckSaveCompleted`.
- **Serializer/parser incompatibility with existing patches** → A subtly different number format or escaping breaks user patches. Mitigation: `sys_patch_roundtrip.cpp` must load every existing on-disk patch and re-emit equivalently; add fixtures from `patches/`.
- **Copy-on-grow wastes arena space** → Worst case ~2× the final container block per container. Mitigation: generous default capacity; waste is transient (freed on arena reset). Acceptable per the chosen trade-off.
- **`StringValue()` lifetime** → The JUCE adapter returns a pointer into a `static` temporary; the arena version returns a pointer into arena memory (valid for arena lifetime), which is stronger. Verify no call site assumed copy semantics.
- **Integer width** → `JanssonAdapter::IntegerValue` returns `int`, jansson stores `int64`. Keep `int64` storage; preserve current narrowing at the `IntegerValue()` boundary to avoid behavior change. Audit `StateSaver` byte packing (stores `int8` as Integer).
- **Wide signature ripple (~30–40 sites)** → Mechanical but broad; a missed site fails to compile (good — no silent breakage). Mitigation: change the API so the arena argument is required, letting the compiler enumerate the sites.

## Migration Plan

1. Add the arena + JSON value library as a new header; keep it self-contained (no JUCE/jansson include).
2. Land it behind the existing `JSON` API names; switch `JuceSon.hpp` to include it unconditionally (drop the `#ifdef EMBEDDED_BUILD` fork).
3. Thread the arena as the first argument through `ToJSON`/`FromJSON`/`ConfigToJSON` and all leaf factories; let the compiler surface every site.
4. Give `StateInterchange` ownership of the save/load arena and the doubling retry; size the default capacity from `patches/WRLD.BLDR.json`.
5. Update `Loads`/`Dumps` call sites in `IOUtils`/`MainComponent` to the arena-backed parser/serializer.
6. Remove `external/jansson` from `private/test/CMakeLists.txt` and the JUCE project; delete `JanssonAdapter.hpp`.
7. Extend `sys_patch_roundtrip.cpp` with real on-disk patch fixtures; add unit tests for exhaustion/retry, copy-on-grow, null propagation, and string/key copying.

**Rollback**: the change is one library swap plus a signature ripple; reverting the commit restores the dual-backend `JSON`. Keep the jansson submodule reference until round-trip tests pass on real patches.

## Open Questions

- Exact default arena capacity (measure the in-arena footprint of `patches/WRLD.BLDR.json` once the representation is fixed; ~8 MB is the starting estimate).
- Whether load (`FromJSON`) and save should share one arena instance in `StateInterchange` or use two; load currently parses on the message thread before handing the tree to the audio thread.
- Number formatting policy for `Dumps` (precision/rounding for `Real`) to guarantee byte-stable round-trips of existing patches.
