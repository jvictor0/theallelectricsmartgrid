## 1. Arena allocator

- [x] 1.1 Add a `JsonArena` struct (`{ char* base; size_t cap; size_t off; bool failed; }`) with owner-side buffer allocation, `Alloc(size, align)` that aligns/bumps or sets `failed` and returns null, plus `Reset()` and `Failed()`.
- [x] 1.2 Unit tests: sequential allocations advance the offset with correct alignment; exhaustion sets `failed` and returns null without calling the system allocator; failure is sticky until `Reset()`; reset reclaims everything.

## 2. JSON value type on the arena

- [x] 2.1 Define the arena-backed `JSON` value (tagged union over object/array/string/integer/real/boolean/null; storage is a pointer into the arena).
- [x] 2.2 Implement copy-on-grow contiguous container blocks for objects/arrays (≥2× growth, memcpy, abandon old; O(1) indexed access; linear key scan for objects).
- [x] 2.3 Make all build/read ops null-tolerant (`SetNew`/`Append`/`Get`/`GetAt`/`Size`/value getters on null are no-ops / null / zero / empty).
- [x] 2.4 Unit tests: copy-on-grow preserves entries and keeps indexed access O(1); writes onto null nodes are no-ops; mid-build exhaustion collapses to a null root checkable at the top level.

## 3. Arena-as-factory build API

- [x] 3.1 Add arena factory methods: `arena.Object()`, `arena.Array()`, `arena.String(s)`, `arena.Integer(n)`, `arena.Real(d)`, `arena.Boolean(b)`, `arena.Null()`.
- [x] 3.2 Copy strings and object keys into the arena at construction; cover with a test that overwrites the source buffer after construction.
- [x] 3.3 Preserve current numeric boundary behavior (int64 storage, `IntegerValue()` narrowing to match the jansson adapter; audit `StateSaver` int8-as-Integer packing).

## 4. Parse and serialize

- [x] 4.1 Implement `Loads(arena, text)` parsing into arena-backed nodes under the same failure semantics.
- [x] 4.2 Implement `Dumps(tree)` serializing to a JSON string (define `Real` precision/rounding for byte-stable round-trips); make `Incref`/`Decref` no-ops.
- [x] 4.3 Unit test: build → serialize → parse round-trips structurally equal.

## 5. Replace the dual backend

- [x] 5.1 Switch `JuceSon.hpp` to include the new library unconditionally; remove the `#ifdef EMBEDDED_BUILD` JSON fork.
- [x] 5.2 Delete `JanssonAdapter.hpp`; remove `external/jansson` from `private/test/CMakeLists.txt` and the JUCE project. (Submodule kept: the separate VCV-Rack `TheNonagon.xcodeproj` still links jansson, so `.gitmodules`/`external/jansson` must stay until that project is migrated too.)

## 6. Thread the arena through ToJSON / FromJSON

- [x] 6.1 Add the arena as the required first argument to `ToJSON`/`FromJSON`/`ConfigToJSON` in `StateSaver.hpp`, `TheNonagon.hpp`, `TheNonagonSquiggleBoy.hpp`, `Encoder*.hpp`, `SquiggleBoy*.hpp`.
- [x] 6.2 Update the JUCE-side builders in `NonagonWrapper.hpp`, `MidiHandlers.hpp`, `IOUtils.{hpp,cpp}`, `MainComponent.{h,cpp}`; let the compiler enumerate any missed leaf sites. (JUCE target not compilable in this env — edited by analogy; needs a Mac/Xcode build to confirm.)

## 7. StateInterchange ownership and retry

- [x] 7.1 Give `StateInterchange` ownership of the save (and load) arena across the round-trip; build on the audio thread, keep the arena alive through the message-thread `Dumps`/`AckSaveCompleted`.
- [x] 7.2 Implement message-thread doubling retry: on a null/failed root, free the arena, reallocate at ≥2× capacity, and re-request the save; the audio thread only ever bumps.
- [x] 7.3 Measure the in-arena footprint of a representative patch and set the default capacity (8 MB) so retry effectively never fires. (Measured ~419 KB in-arena for a populated patch — ~19× headroom; test `sys_save_arena` reports it.)

## 8. Verification

- [x] 8.1 Add real on-disk fixture round-trips (`sys_json_fixture.cpp`): the 358 KB `patches/WRLD.BLDR.json` parse→dump→parse→dump is byte-stable, and a nonagon patch survives a disk write/read cycle NaN-clean.
- [x] 8.2 Add a test (`sys_save_arena.cpp`) asserting the audio-thread build performs zero system heap allocations (global `operator new` counter) with an adequately sized arena.
- [x] 8.3 Add a test forcing the save arena to 4 KB and driving the real `SavePatch` retry path; the doubled-capacity retry yields a complete four-section patch.
- [x] 8.4 Full EMBEDDED test suite green (185 cases, 1.49M assertions, 0 failed). JUCE/macOS plugin builds clean via `make build` (universal arm64+x86_64, BUILD SUCCEEDED, no warnings in converted files).
