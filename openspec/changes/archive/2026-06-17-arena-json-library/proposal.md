## Why

Patch serialization (`ToJSON`) runs on the **audio thread** — it is polled out of `StateInterchange` inside `ProcessFrame()` (`TheNonagonSquiggleBoy.hpp`, `HandleStateInterchange`). Both current JSON backends heap-allocate there: the `EMBEDDED_BUILD` path calls jansson (`json_object`/`malloc`), and the plugin path calls JUCE (`new juce::DynamicObject`). Allocating on the audio thread is a real-time-safety violation. The two-backend split (jansson vs `juce::var` behind one `JSON` API) also means we carry the `external/jansson` submodule and a `#ifdef EMBEDDED_BUILD` fork purely to paper over that an audio-thread-safe path never existed.

## What Changes

- Introduce a single self-contained, header-only JSON library backed by a **bump (arena) allocator**: every node, key, and string is allocated by moving a pointer forward in a pre-sized buffer; nothing is freed individually — the whole arena is released when the document goes out of scope.
- The arena **is** the factory: call sites use `arena.Object()`, `arena.Array()`, `arena.Integer(5)`, `arena.String(s)`. Every `ToJSON`/`FromJSON` function takes the arena as its first argument and threads it down to the leaves.
- Containers (objects/arrays) use **growable copy-on-grow blocks** inside the arena: on overflow, bump-allocate a new doubled block, `memcpy`, and abandon the old block (acceptable waste — the arena is transient). Indexed access stays O(1).
- **Allocation failure is a flag, not a crash**: when the arena is exhausted, `Alloc` sets a `failed` flag and returns null. Null propagates automatically because `SetNew`/`Append` on a null node are no-ops. Callers check **once** at the top-level root.
- **Retry lives on the message thread**: if the audio thread returns a null/failed root, the message thread frees the arena, doubles its capacity (`malloc` is legal there), and re-requests the save via `StateInterchange`. The audio thread only ever bumps a pointer — never allocates. Default capacity is sized from real patches (~358 KB serialized → multi-MB in-tree), so retry is a pathological-case safety net, not a normal occurrence.
- Scope covers **both directions**: build (`ToJSON`) and parse (`Loads`/`FromJSON`) allocate from the same arena; `Dumps` serializes the arena tree to a string on the message thread.
- The library presents the **same `JSON` API surface** already used across the codebase (`Object`/`Array`/`SetNew`/`Append`/`Get`/`GetAt`/`Size`/`StringValue`/`IntegerValue`/`RealValue`/`BooleanValue`/`IsNull`/`Loads`/`Dumps`). `Incref`/`Decref` become no-ops (lifetime = arena lifetime).
- **BREAKING (internal)**: `ToJSON`/`FromJSON`/`ConfigToJSON` signatures gain a leading arena parameter (~30–40 leaf call sites). `StateInterchange` owns the arena across the save round-trip so the tree (pointers into the arena) outlives the message-thread `Dumps`/`AckSaveCompleted`.
- **Removals**: delete the `external/jansson` submodule dependency and the `#ifdef EMBEDDED_BUILD` JSON fork in `JuceSon.hpp`. `VoiceAllocator`/`FixedAllocator`/`SegmentedAllocator` are untouched — none is a bump allocator, so the arena is a genuinely new allocator type.

## Capabilities

### New Capabilities
- `arena-json`: A self-contained JSON value library backed by a bump allocator. Covers the arena (bump-to-failure, copy-on-grow containers, failure flag, capacity doubling), the arena-as-factory build API, the parse/serialize API, and the audio-thread-safe / message-thread-retry usage contract.

### Modified Capabilities
- `scene-state-management`: The "Patch Persistence with Separate Encoder Serialization" requirement gains a real-time-safety guarantee — audio-thread serialization performs **no heap allocation**, allocating only from a caller-owned arena, with capacity-doubling retry driven from the message thread and the arena's lifetime bracketing the save round-trip.

## Impact

- **New code**: `private/src/` — the arena allocator + JSON value type + parser/serializer (replaces `JanssonAdapter.hpp` and the JUCE branch of `JuceSon.hpp`).
- **Signature ripple**: every `ToJSON`/`FromJSON`/`ConfigToJSON` in `StateSaver.hpp`, `TheNonagon.hpp`, `TheNonagonSquiggleBoy.hpp`, `Encoder*.hpp`, `SquiggleBoy*.hpp`, and `JUCE/SmartGridOne/Source/{NonagonWrapper,MidiHandlers,IOUtils,MainComponent}` gains a leading arena argument.
- **Threading/ownership**: `StateInterchange` holds the arena for save and load; `MainComponent`/`IOUtils` `Dumps`/`Loads` against arena-backed trees.
- **Build/deps**: `external/jansson` submodule and its include/link drop from `private/test/CMakeLists.txt` and the JUCE project; `EMBEDDED_BUILD` no longer forks JSON.
- **Tests**: `sys_patch_roundtrip.cpp` must still round-trip existing on-disk patch files (e.g. `patches/WRLD.BLDR.json`); new unit tests for arena exhaustion/retry, copy-on-grow, and failure propagation.
