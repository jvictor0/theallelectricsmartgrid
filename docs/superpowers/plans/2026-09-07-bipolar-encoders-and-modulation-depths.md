# Bipolar Encoders and Modulation Depths Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Support bipolar encoders and exponential signed modulation depths while preserving crossfade modulation, with signed knob positions in unversioned patch JSON and automatic polarity conversion in DSP value getters.

**Architecture:** Every state encoder owns `m_bipolar`. Internal scene, gesture, output, and slew state remains normalized to `[0, 1]`, as in Sheaf. JSON and DSP value getters expose signed knob position for bipolar encoders; the parent modulation mixer applies the exponential depth curve and the negative-depth offset.

**Tech Stack:** C++17, header-based DSP core, arena JSON, doctest/CMake standalone tests, JUCE encoder rendering, MIDI controller output.

## Execution Notes

The implementation keeps the conversion helpers on `StateEncoderCell` and the
depth curve on `BankedEncoderCell`, rather than adding a separate mapping header.
This follows the user's instruction to keep the implementation simple. Core and
recursive patch coverage is consolidated in `private/test/unit/encoder_bipolar.cpp`;
the front-door negative-depth save/load/reset regression lives in
`private/test/system/sys_encoder_reset.cpp`. The original steps below describe the
planned work; verification outcomes are recorded at the end of this document.

- [x] Encoder polarity, normalized storage, and automatic DSP getter conversion.
- [x] Unversioned signed JSON and centered depth/gesture lifecycle.
- [x] Recursive exponential depth mixing with the negative-depth offset.
- [x] Polarity publication and center marker with normalized UI/MIDI values.
- [x] Regression coverage, macOS build, review, and baseline failure comparison.

## Global Constraints

- Store `m_bipolar` on the encoder, not a polarity choice at each getter call.
- Bipolar JSON stores knob position in `[-1, 1]`, not the exponential effective depth. Unipolar JSON remains `[0, 1]`.
- Do not version saved patches, add format-detection heuristics, or apply inverse-exponential migration to old depths.
- `GetValue` and `GetValueNoSlew` return bipolarized values when the encoder is bipolar. Internal normalized access must remain explicit.
- Match Sheaf's continuous zero-based exponential depth curve, including exact zero and signed endpoints.
- Keep modulation sources normalized; preserve the existing separate source amplitude multiplier.
- Keep all existing top-level parameter declarations unipolar in this change. Enable polarity in declarations, and make every modulation-depth cell bipolar. Gesture target cells inherit their parent's polarity.
- Preserve existing top-level defaults, DSP callers, MIDI knob coordinates, switch positions, and normalized UI value/minimum/maximum coordinates.
- Follow repository braces, `m_` member names, `x_` constants, HammerCase functions, public structs, and comment formatting. Do not modify agent infrastructure.
- This document is a plan only. Execute in the existing managed worktree; inspect its branch state before implementation or commits. Do not develop on `main`.

## Accepted Design and Domain Contracts

Let `u` be internal normalized knob position and `b` the externally visible bipolar position:

```text
GetValue / JSON write: b = 2u - 1
JSON read:            u = (b + 1) / 2
Effective depth:      D(b) = sign(b) * (9^abs(b) - 1) / 8
```

For a unipolar encoder both boundary conversions are identity. The boolean is configuration, inferred from the parameter declaration or child role; do not add it to patch JSON. Configure it before loading a cell's values.

| Old saved depth / new signed knob position | New internal position | New effective depth |
| ---: | ---: | ---: |
| 0 | 0.5 | 0 |
| 0.1 | 0.55 | approximately 0.03072 |
| 0.5 | 0.75 | 0.25 |
| 0.9 | 0.95 | approximately 0.77808 |
| 1 | 1 | 1 |
| -0.5 (newly allowed) | 0.25 | -0.25 |

This intentionally changes old sounds: positive depths become weaker away from the endpoints. Old zero remains neutral. Affine JSON decoding also preserves linear interpolation of signed knob positions across scenes and gesture targets; the new exponential curve is applied after those normalized computations. Existing nested modulation is reinterpreted under the same new rules, without promising identical old audio.

Defaults in parameter declarations and `CreateEncoder` are in the externally visible knob domain. Convert once at creation; retain `m_defaultValue` in normalized units because reset and slew initialization use internal state. A newly declared bipolar parameter can therefore specify default `0` for its midpoint.

For an opted-in top-level parameter, future changes must deliberately update its DSP consumers and accept the changed interpretation of old saved values. In particular, do not mark `PanCenterX/Y` bipolar in this change: their current callers already apply `2u - 1`, and their old saved centers are `0.5`.

The DSP path is:

```text
scene/gesture storage [0,1]
    -> recursive normalized modulation mix [0,1]
    -> normalized output slew [0,1]
    -> polarity-aware DSP getter ([0,1] or [-1,1])
```

For depth evaluation, the parent uses the child's unslewed signed value and applies `D` exactly once. Do not write signed or curved values back into `m_output`, `m_bankedValue`, `m_postGestureValue`, or the slew state. UI/MIDI continue reading normalized output directly.

## Source References

- Sheaf: `/Users/joyo/Sheaf/projects/synth/include/synth/ParameterModulation.hpp`, `ModulationDepthTargetFromKnob` and the zero-based exponential helper.
- Sheaf: `/Users/joyo/Sheaf/projects/synth/src/ParameterModulation.cpp`, absolute depth normalization and `normalizationOffset` in `ComputeAtDepth`.
- This project: `private/src/Encoder.hpp`, scene storage, JSON boundaries, state propagation, and zero checks.
- This project: `private/src/EncoderBank.hpp`, recursive cell construction, mixing, reset, activity, collection, getters, UI publication, and recursive loading.
- This project: `private/src/EncoderBankBank.hpp` and `private/src/SmartGridOneEncoders.hpp`, parameter creation and public getter delegation.

## File Map

| File | Responsibility |
| --- | --- |
| Create `private/src/EncoderValueMapping.hpp` | Small pure polarity and depth mapping helpers |
| Modify `private/src/Encoder.hpp` | Polarity metadata, JSON conversion, normalized state propagation, neutral checks |
| Modify `private/src/EncoderBank.hpp` | Child initialization/inheritance, signed depth mixing, amplitude invalidation, getters, lifecycle, UI publication |
| Modify `private/src/EncoderBankBank.hpp` | Creation/default conversion and consistent getters by encoder index |
| Modify `private/src/SmartGridOneEncoders.hpp` | Thread polarity through declaration expansions; normalized switch lookup |
| Modify `private/src/ForEachSmartGridOneParam.hpp` | Add a trailing bipolar flag, initially false for every existing top-level parameter |
| Modify `private/src/EncoderUIState.hpp` | Publish polarity alongside normalized visual coordinates |
| Modify `JUCE/SmartGridOne/Source/EncoderComponent.hpp` | Render a center marker for bipolar encoders |
| Create `private/test/unit/encoder_bipolar.cpp` | Mapping, getter, serialization, mixing, and cell lifecycle regression tests |
| Modify `private/test/system/sys_encoder_reset.cpp` | Front-door centered depth reset regression |
| Modify `private/test/system/sys_patch_roundtrip.cpp` | Negative depths and nested gesture/depth patch round trips |
| Modify `docs/encoder-system.md` | Document domains, curve, crossfade inversion, and intentional old-patch depth changes |

`private/src/EncoderMidi.hpp` needs an audit and a regression check, but no value-domain conversion: its `valueF * 127` input remains normalized. Tests are discovered by the existing CMake glob; no test build-system change is needed.

## Verification Commands

Run from the repository root. Configure once before running implementation tests:

```bash
cmake -S private/test -B private/test/build
cmake --build private/test/build -j 8
private/test/build/smartgrid_tests --test-case="encoder_bipolar:*"
```

Run the relevant case after each change; the focused filter above runs the complete new unit coverage. Final core regression gate:

```bash
private/test/build/smartgrid_tests --test-case="sys_encoder_reset:*,sys_patch_roundtrip:*,sys_gestures:*,sys_scenes:*"
ctest --test-dir private/test/build --output-on-failure
```

For each test-first step below, first establish the expected failure, then implement and require the test to pass. Do not weaken existing tests to hide a changed assumption; explain any assertion that explicitly encoded the old depth curve before updating that expectation.

## Task 1: Define Polarity Boundaries and Automatic Getter Semantics

**Files:** `EncoderValueMapping.hpp`, `Encoder.hpp`, `EncoderBank.hpp`, `EncoderBankBank.hpp`, `SmartGridOneEncoders.hpp`, `private/test/unit/encoder_bipolar.cpp`.

**Interfaces produced:** `SmartGrid::EncoderValueMapping::{ToValue, ToNormalized, ModulationDepthFromValue}`, `StateEncoderCell::m_bipolar`, `StateEncoderCell::{ToValue, ToNormalized, GetNeutralNormalizedValue}`, `BankedEncoderCell::GetValueNoSlew(size_t channel)`, and `EncoderBankBank::GetNormalizedValueNoSlewByEncoderIndex(size_t encoderIndex, size_t channel)`.

- [ ] Add the mapping helper test below, build, and establish failure because the helper does not exist.

```cpp
#include "doctest.h"
#include "EncoderValueMapping.hpp"

DOCTEST_TEST_CASE("encoder_bipolar: signed knob and exponential depth mapping")
{
    using Mapping = SmartGrid::EncoderValueMapping;
    const float positions[] = {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f};
    const float depths[] = {-1.0f, -0.25f, 0.0f, 0.25f, 1.0f};
    for (size_t i = 0; i < 5; ++i)
    {
        const float normalized = Mapping::ToNormalized(positions[i], true);
        DOCTEST_CHECK(Mapping::ToValue(normalized, true) == doctest::Approx(positions[i]));
        DOCTEST_CHECK(Mapping::ModulationDepthFromValue(positions[i]) == doctest::Approx(depths[i]));
    }

    DOCTEST_CHECK(Mapping::ToValue(0.25f, false) == 0.25f);
    DOCTEST_CHECK(Mapping::ToNormalized(0.25f, false) == 0.25f);
}
```

- [ ] Implement the pure helpers in namespace `SmartGrid`, with `<algorithm>` and `<cmath>` included explicitly:

```cpp
struct EncoderValueMapping
{
    static float ToValue(float normalized, bool bipolar)
    {
        return bipolar ? 2.0f * normalized - 1.0f : normalized;
    }

    static float ToNormalized(float value, bool bipolar)
    {
        return bipolar ? (value + 1.0f) * 0.5f : value;
    }

    static float ModulationDepthFromValue(float value)
    {
        const float bipolar = std::clamp(value, -1.0f, 1.0f);
        if (bipolar == 0.0f)
        {
            return 0.0f;
        }

        const float magnitude = (std::pow(9.0f, std::fabs(bipolar)) - 1.0f) / 8.0f;
        return std::copysign(magnitude, bipolar);
    }
};
```

- [ ] Add `bool m_bipolar` to `StateEncoderCell`, initialized false in both constructors, plus one-argument `ToValue`/`ToNormalized` forwarding members and `GetNeutralNormalizedValue()` returning `m_bipolar ? 0.5f : 0.0f`.
- [ ] Change `StateEncoderCell::GetValue(track)` to map its normalized scene value, but change `SetStateForTrack(track)` to read `GetNormalizedValueForTrack(track)` directly. Add this regression using a default-constructed `BankedEncoderCell`, local `SceneManager`, and a single connected state pointer:

```cpp
SmartGrid::SceneManager scenes;
SmartGrid::BankedEncoderCell cell;
float state = 0.0f;
cell.m_sceneManager = &scenes;
cell.m_numTracks = 1;
cell.m_bipolar = true;
cell.SetStatePtr(&state, 0);
cell.m_values[0][0] = 0.25f;
cell.SetStateForTrack(0);
DOCTEST_CHECK(state == 0.25f);
DOCTEST_CHECK(cell.GetValue(0) == -0.5f);
cell.m_output[0] = 0.25f;
cell.InitSlewState(0.25f);
DOCTEST_CHECK(cell.GetValueNoSlew(0) == -0.5f);
DOCTEST_CHECK(cell.GetSlewedValue(0) == -0.5f);
DOCTEST_CHECK(cell.m_slew[0].m_output == 0.25f);
```

- [ ] Implement `GetSlewedValue(channel)` as `ToValue(m_slew[channel].Process(m_output[channel]))` and `GetValueNoSlew(channel)` as `ToValue(m_output[channel])`. Make the index-based and bank-coordinate unslewed getters delegate to the latter; slewed getters already delegate to `GetSlewedValue`.
- [ ] Add the explicitly normalized unslewed index getter, returning `cell->m_output[channel]` or zero for an absent cell. Change `SmartGridOneEncoders::GetSwitchVal` to use it before rounding switch position. Preserve the existing null-cell zero return at all public getter boundaries.
- [ ] Run focused tests. Commit this independently testable boundary change after verification. Existing production cells are still unipolar at this task's end.

## Task 2: Enable Bipolar Configuration, Serialization, and Depth Lifecycle Together

**Files:** `Encoder.hpp`, `EncoderBank.hpp`, `EncoderBankBank.hpp`, `SmartGridOneEncoders.hpp`, `ForEachSmartGridOneParam.hpp`, `encoder_bipolar.cpp`.

**Consumes:** Task 1 conversion members. **Produces:** required trailing `bool bipolar` on `CreateEncoder`; trailing `bipolar` argument on every parameter macro expansion; `AllNeutral()`, `IsNeutralCurrentScene()`, `IsNeutralCurrentSceneForTrack(size_t)`, `NeutralizeCurrentScene()` replacing the current zero-named state helpers.

- [ ] Add a JSON test that exercises `StateEncoderCell`'s existing nested `values` layout and validates boundary conversion rather than merely serializing a helper result:

```cpp
SmartGrid::SceneManager scenes;
SmartGrid::BankedEncoderCell cell;
cell.m_sceneManager = &scenes;
cell.m_numTracks = 1;
cell.m_bipolar = true;
for (size_t channel = 0; channel < 16; ++channel)
{
    cell.SetStatePtr(&cell.m_bankedValue[channel], channel);
}

cell.SetValueAllScenesAllTracks(0.25f);
JsonArena arena(1024 * 1024);
JSON saved = cell.StateEncoderCell::ToJSON(arena);
DOCTEST_CHECK(saved.Get("values").GetAt(0).GetAt(0).NumberValue() == -0.5);
cell.SetValueAllScenesAllTracks(1.0f);
cell.StateEncoderCell::FromJSON(saved);
DOCTEST_CHECK(cell.m_values[0][0] == 0.25f);
DOCTEST_CHECK(cell.m_bankedValue[0] == 0.25f);
```

- [ ] Add a literal old-format JSON fixture, using the same 8 scenes and one track, to prove that a saved depth of zero decodes to internal `0.5`, saved `0.5` decodes to `0.75`, and saved `1` decodes to `1`. Repeat with `m_bipolar = false` and require identity decoding. Include negative saved values `-1` and `-0.5` for the new path. The fixture should use `arena.Array()`, `AppendNew(arena.Real(value))`, and `root.SetNew("values", values)`; do not add metadata or version fields.
- [ ] In `StateEncoderCell::ToJSON`, emit `a.Real(ToValue(m_values[j][i]))`. In `FromJSON`, decode each finite stored knob position with `ToNormalized(static_cast<float>(...NumberValue()))` before `SetState()`. Preserve current JSON object/array structure and arena error propagation. Keep malformed-input behavior within the existing loader contract.
- [ ] Append `bipolar` to the `F(...)` signature in all six expansions in `SmartGridOneEncoders.hpp`, add `false` to every current declaration, and pass the argument to `CreateEncoder`. At creation configure the boolean before default conversion:

```cpp
cell->m_bipolar = bipolar;
cell->m_defaultValue = cell->ToNormalized(defaultValue);
cell->SetValueAllScenesAllTracks(cell->m_defaultValue);
cell->InitSlewState(cell->m_defaultValue);
```

- [ ] In the child constructor, set `m_bipolar = true` for `ModulatorAmount`; otherwise inherit `parent->m_bipolar` for `GestureParam`. Establish polarity and `m_numTracks` before child JSON loading. Initialize every depth scene/track slot, banked value, output, min/max, and slew state to `0.5`, and `m_defaultValue` to `0.5`. Initialize bipolar gesture cells to their neutral normalized value; activation still copies the parent's normalized per-scene value as it does today. Do not call `SetValueAllScenesAllTracks` before all state pointers are attached.
- [ ] Rename the zero-named state helpers and compare/write `GetNeutralNormalizedValue()`. Update activity and collection callers. In `ZeroModulators`, reset each depth to its neutral value rather than literal zero, recurse, and retain gesture deactivation semantics. `RevertToDefault` continues using the normalized `m_defaultValue`.
- [ ] Test a newly materialized route at `0.5`, a route at the full-negative endpoint `0`, and a centered route with active descendants. Only the neutral leaf is collectible. Test active and inactive scenes/tracks separately; do not infer neutrality from the currently blended output because opposite scene depths can cancel at the midpoint.
- [ ] Verify child allocation/loading does not depend on a UI visit, gesture targets inherit polarity before decoding, missing routes remain absent, and reset forces corrective recomputation after the affecting bitmask becomes empty.
- [ ] Run the focused tests and the existing reset/patch/gesture cases. Commit only after Task 3's mixer is also in place if enabling bipolar children makes old mixing tests fail; do not leave an enabled bipolar-depth feature with the positive-only mixer.

## Task 3: Apply Exponential Signed Depths With Crossfade Normalization

**Files:** `EncoderBank.hpp`, `encoder_bipolar.cpp`.

**Consumes:** Task 1 signed unslewed getter and curve; Task 2 neutral bipolar depth initialization. **Produces:** signed-depth behavior in the existing recursive `Modulators::Compute` and amplitude-aware source invalidation.

- [ ] Add a lightweight fixture in the test file using a local `SceneManager`, default-constructed base cell, `SharedEncoderState` and `ModulatorValues`. Wire `m_numTracks = 1`, `m_numVoices = 1`, `m_currentTrack = 0`, the base cell's shared/scene pointers, and all 16 banked state pointers. Mark the base connected. Register source colors with `SetModulatorColor` before `FillModulators` so children are connected. Use `GlobalEnv::Init()` before timing-sensitive code; use the existing production allocator rather than replacing it.
- [ ] Set route storage through `SetValueAllScenesAllTracks(route.ToNormalized(signedPosition))`; call `SetStateRecursive`, `SetModulatorsAffecting`, and `Compute` on the base to establish state. Add distinct assertions for the following cases, using an unipolar base of `0.3` and source `0.8`:

| Signed knob position | Effective depth | Expected normalized output |
| ---: | ---: | ---: |
| 0 | 0 | 0.3 |
| +1 | +1 | 0.8 |
| -1 | -1 | 0.2 |
| +0.5 | +0.25 | 0.425 |
| -0.5 | -0.25 | 0.275 |

- [ ] Extend those tests to two routes: signed positions `+1/-1`, source values `0.8/0.2`, expected output `0.8`, and theoretical bounds `[0,1]`. This distinguishes absolute-weight normalization from signed cancellation. For a single signed position `-0.5`, assert bounds `[0.225,0.475]`.
- [ ] Replace the current route accumulation with the following operations inside the existing per-voice loop, adding a zero-initialized `modOffset[16]` array next to `modValue` and `modWeight`:

```cpp
const float signedPosition = cell->GetValueNoSlew(ix);
const float depth = EncoderValueMapping::ModulationDepthFromValue(signedPosition) * amp;
modValue[ix] += depth * modulatorValues->m_value[m_activeModulators[i]][ix];
modWeight[ix] += std::fabs(depth);
modOffset[ix] += std::max(0.0f, -depth);
```

- [ ] Replace both current output branches with this equivalent crossfade form. The offset is accumulated before normalization here, so divide it along with the signal contribution:

```cpp
const float totalWeight = modWeight[ix];
const float denominator = std::max(1.0f, totalWeight);
const float baseWeight = std::max(0.0f, 1.0f - totalWeight);
m_owner->m_output[ix] = value * baseWeight + (modValue[ix] + modOffset[ix]) / denominator;
m_owner->m_minValue[ix] = value * baseWeight;
m_owner->m_maxValue[ix] = value * baseWeight + std::min(1.0f, totalWeight);
```

- [ ] Keep the existing normalized final-output slew. Use absolute total weight for the existing base-contribution brightness calculation. Leave source-driven badge brightness semantics alone. Do not change source waveform generation or apply an additional amplitude conversion there.
- [ ] Add a nested-depth test: base `0.3`; route 0 initially neutral; its nested route 1 at full positive; source 1 at normalized `0.25`. The child output becomes `0.25`, its signed position becomes `-0.5`, and effective parent depth becomes `-0.25`. With source 0 at `0.8`, expect parent output `0.275`. This verifies normalized recursion and one curve application per parent-child edge.
- [ ] Add an amplitude-only update test: hold source at `0.8`, route at full positive, and base at `0.3`; changing amplitude from `1` to `0` must update the output from `0.8` to `0.3` without a source-value change or force-update. Store and initialize `m_amplitudePrev[x_numModulators][16]`, compare it alongside `m_valuePrev` in `ComputeChanged`, and update both previous arrays when marking a source changed:

```cpp
if (memcmp(m_value[i], m_valuePrev[i], 16 * sizeof(float)) != 0 ||
    memcmp(m_amplitude[i], m_amplitudePrev[i], 16 * sizeof(float)) != 0)
{
    m_changedModulators.Set(i, true);
    memcpy(m_valuePrev[i], m_value[i], 16 * sizeof(float));
    memcpy(m_amplitudePrev[i], m_amplitude[i], 16 * sizeof(float));
}
```

- [ ] Add two-voice coverage with different amplitudes and source values to ensure offset and absolute-weight accumulators are per voice. Include a small negative depth and verify continuity at zero with an absolute tolerance of `1e-6`.
- [ ] Run focused and existing modulation/gesture/reset tests. Commit the enabled feature from Tasks 2–3 together if necessary to keep commits passing.

## Task 4: Expose Polarity to the UI Without Changing Knob Transport

**Files:** `EncoderUIState.hpp`, `EncoderBank.hpp`, `EncoderComponent.hpp`, `encoder_bipolar.cpp`; audit `EncoderMidi.hpp`.

**Consumes:** `StateEncoderCell::m_bipolar`. **Produces:** `EncoderUIState::m_bipolar` atomic bool and `EncoderBankUIState::{SetBipolar(size_t,size_t,bool), GetBipolar(size_t,size_t)}`.

- [ ] Add a UI publication test using `EncoderBankInternal::Init`, `PlaceEncoder`, and `PopulateUIState` with a local base cell. For a bipolar cell with `m_output[0] = 0.25`, require `GetBipolar(0,0) == true` and `GetValue(0,0,0) == 0.25`. Clear the cell, republish, and require the bipolar flag false. Repeat with a unipolar cell.
- [ ] Initialize the new atomic bool false. Publish the flag from visible connected cells and clear it in the disconnected branch, alongside the existing switch metadata. Leave all UI values and bounds normalized.
- [ ] Add a center marker in the existing main arc rendering loop. Use `ValueToArcAngle(0.5f)` and the existing arc geometry, or draw a small dot at the equivalent normalized center point. One concrete rendering form, using the radius and center already present in that loop, is:

```cpp
if (m_ui.m_uiState->GetBipolar(m_x, m_y))
{
    const float angle = ValueToArcAngle(0.5f);
    const float markerX = centerX + radius * std::sin(angle);
    const float markerY = centerY - radius * std::cos(angle);
    g.fillEllipse(markerX - 2.0f, markerY - 2.0f, 4.0f, 4.0f);
}
```

- [ ] Confirm `ValueToArcAngle` uses the same JUCE angular convention before using this marker form. Keep the existing min/max arcs and value dots in normalized coordinates. Do not add a center detent or change encoder acceleration in this scope.
- [ ] Check MIDI output for normalized positions `0`, `0.5`, and `1`; the existing conversion must remain `0`, `63`, and `127` (current truncation). Confirm an absolute encoder-set message of `0.5` selects neutral depth, and `0` selects full negative depth. No negative floats should reach MIDI packing.
- [ ] Run the focused tests and existing `EncoderSet*` cases. Build the JUCE target using the repository's `JUCE/SmartGridOne` make entry point and visually inspect midpoint/negative/positive depth indicators when a local app build is available. Report an unavailable platform/device check explicitly; the standalone core tests do not compile the JUCE component.
- [ ] Commit after verification.

## Task 5: Lock Down Unversioned Patch Behavior and End-to-End Reset

**Files:** `encoder_bipolar.cpp`, `sys_patch_roundtrip.cpp`, `sys_encoder_reset.cpp`, `docs/encoder-system.md`.

**Consumes:** completed behavior from Tasks 1–4. **Produces:** regression coverage of the accepted saved-patch contract and updated user/developer documentation.

- [ ] Add a production `BankedEncoderCell::ToJSON/FromJSON` test with nested depths and an active gesture target on a depth cell. Use signed positions `-0.5`, `0`, and `0.5` in different scenes. Verify the serialized numeric values through the existing `values -> values -> scene -> track` path and recursive `modulators`/`gestures` arrays. Verify effective outputs after load, not only the raw JSON.
- [ ] Add a literal legacy root/route fixture to that test. Old route `0` must remain neutral, `0.5` must produce depth `0.25`, and `1` must produce full depth. An old gesture target `0.5` on that route must load into normalized `0.75`, with inherited bipolar polarity. Assert that neither a `version` nor a `bipolar` member was added to cell JSON.
- [ ] Test scene morphing across signed `-1/+1`: at blend `0.5`, normalized position must be `0.5` and effective depth exactly zero. At blend `0.75`, normalized position must be `0.75` and effective depth `0.25`. This must be independent of the base parameter's polarity.
- [ ] In `sys_patch_roundtrip.cpp`, use the existing `SynthRig` front door to enter modulation view, set a depth knob to normalized `0.25`, save, reset, load, and re-enter the view. Require normalized knob `0.25` within encoder transport tolerance, negative signed saved position, and the route still affecting its parent. Extend this with a nested depth and gesture target, then verify repeated save/load does not reapply the curve or progressively shrink depths. Use `1e-6` tolerance for internal float conversions and the existing 14-bit transport tolerance for front-door checks.
- [ ] In `sys_encoder_reset.cpp`, enter modulation view with `PressEncoder`, set a depth to normalized `0`, and clear modulation through the existing shift/reset path. Re-enter modulation view and require every newly materialized depth at `0.5`; require the parent's output to return to its base value and no active modulation badge to remain. Exercise resetting one active scene and all scenes/tracks with negative and positive depth endpoints.
- [ ] Add public getter assertions through `SmartGridOneEncoders` and both bank access paths using a controlled test encoder marked bipolar, with normalized output/slew state `0.25`. Require signed output `-0.5` from all DSP getters and normalized UI output `0.25`. Restore any temporarily changed singleton metadata and values before returning from the test. Check a bipolar configured switch through the normalized switch path, so future opt-in cannot shift switch indices.
- [ ] Update `docs/encoder-system.md` with the domain table, signed JSON convention, exact curve, normalized crossfade equation, neutral lifecycle, automatic getters, and accepted weaker old depths. State that existing top-level parameters remain unipolar until deliberately opted in.
- [ ] Run the focused suite, relevant existing system suites, and the complete CTest gate using the commands above. Run `git diff --check`. Review the changed parameter declaration list to ensure every existing bipolar flag is false and no default values or names changed.
- [ ] Commit the final coverage and documentation after the checks pass. Report the core test results and JUCE/manual verification status separately.

## Plan Self-Review

- The accepted `m_bipolar`, unversioned signed JSON, automatic getter, and internal Sheaf-domain requirements are covered by Tasks 1–2.
- The exponential curve and absolute-weight/negative-offset crossfade are covered by Task 3, including nested depths and amplitude-only changes.
- Neutral initialization/reset/activity/collection and inherited gesture polarity are covered by Task 2 and end-to-end tests in Task 5.
- Normalized UI/MIDI/switch coordinates are explicit and covered by Tasks 1, 4, and 5.
- Old sounds are intentionally changed by the curve; old zero is never interpreted as full negative depth. There is no version field or legacy-evaluation branch.
- Top-level opt-in is supported but existing parameter semantics are not silently changed.
- Task 2 and Task 3 may be committed together to avoid an intermediate enabled feature with invalid mixing semantics.

## Implementation Verification

- Polarity, unversioned signed JSON, automatic getters, centered depth lifecycle,
  recursive exponential crossfades, amplitude invalidation, and the UI center
  marker are implemented. Existing top-level parameter flags are all false.
- The initial six regression tests failed against the original implementation,
  then passed after the encoder changes. Expanded focused/system coverage passed:
  49 test cases and 520 assertions, including negative-depth save/load/reset.
- The macOS Debug app built successfully with Xcode and code signing disabled.
  The center marker has compile coverage; interactive visual/device checks were
  not performed.
- Independent code review reported no actionable issues.
- The unfiltered CTest run aborts in `PartialMachine: espace etale patch remains
  finite after load`, at the `phi_vps < 1` assertion in
  `private/src/VectorPhaseShaper.hpp:340`. A separately built archive of unchanged
  HEAD `e85d903` reproduces that exact failure. No assertion, test exclusion, or
  DSP workaround was committed to hide it.
- Running the suite with only that crashing case excluded completes: 289 passed,
  two failed, one excluded. The two failures are `startup: pre-start run is finite,
  bounded, and quiescent` and `startup: reproduce pre-first-start frozen-clock
  observable`. Both fail identically on unchanged HEAD, with a pre-start output
  peak of `0.000688948` instead of silence. These pre-existing failures remain
  untouched. The complete suite is therefore not green.
- All 11 added test cases pass. `git diff --check` reports no whitespace errors.
- Changes remain in the managed worktree for review; no branch integration or
  publishing was performed.
