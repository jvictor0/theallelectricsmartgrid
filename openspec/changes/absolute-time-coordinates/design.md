## Context

The motivating behavior is fractional-speed PhasorPlayHead playback across ToT loop boundaries. Before this change, time was reduced to a circle at the source and each child, then reconstructed by CircleTracker, per-loop winding, monodromy recursion, and downstream distance trackers. The accepted design makes ToT coordinates a pure forward function of absolute input phase and accepted topology. Boundary events still depend on consecutive samples; parameter acceptance, LFO filters, and triggered envelopes retain their natural state.

The existing standalone test target is `smartgrid_tests`, configured from `private/test/CMakeLists.txt`. The current workspace is an already isolated Codex worktree. Implementation was authorized after the core-header review checkpoint.

## Goals / Non-Goals

**Goals:** absolute coordinates; fractional and reverse sample playback; direct signed gate-step queries; aligned reparenting; AHD timing captured at trigger; consistent terminology and one time query surface; tests at actual accepted topology transitions.

**Non-Goals:** arbitrary-precision or fixed-point time; smoothing all topology changes; changing independent-versus-modulated acceptance timing; changing PolyXFader's partial-lobe definition, mixing controls, topology weights, or slew; preserving historical child cycle counts after topology edits; changing existing persistence keys, StateSaver missing-key policy, or product clock modes.

## Decisions

### 1. Terms, ownership, and interfaces

| Term | Meaning |
| --- | --- |
| Global loop | Root loop, index 5, constant `x_globalLoop` |
| Phase | Absolute `double`, measured in cycles of the named loop |
| Unmodulated / Modulated | Before / after the existing phase-modulation offset |
| Position | Absolute signed `int64_t` ticks on the current common lattice |
| Period ticks | Positive number of lattice ticks in a loop cycle |
| Parent multiplier | Integer child cycles per parent cycle |
| Cycle ratio | Integer loop cycles per global cycle |
| Gate-step index | Signed whole-cycle coordinate, optionally relative to an ancestor reset |
| Cycle boundary | A change in the floor-divided cycle coordinate between samples |
| Wrapped phase | Local output value `phase - floor(phase)`, never canonical ToT state |

Use `PhaseDomain::Unmodulated` and `PhaseDomain::Modulated`; remove Boolean domain selectors. Use loop index first, sample coordinate second, and domain last. All sample coordinates refer to the same buffered microblock. The core surface is:

```cpp
double GetPhase(size_t loopIndex, double samplePosition, PhaseDomain domain) const;
int64_t GetPosition(size_t sampleIndex, PhaseDomain domain) const;
int64_t GetPeriodTicks(size_t loopIndex, size_t sampleIndex) const;
int64_t GetCycleRatio(size_t loopIndex, size_t sampleIndex) const;
SampleTop CrossedCycleBoundary(size_t loopIndex, size_t sampleIndex, PhaseDomain domain) const;
int64_t GetGateStepIndex(size_t loopIndex, size_t sampleIndex, int resetLoopIndex) const;
```

Crossing queries preserve main's `SampleTop` event and fractional sample offset; `AnyTick` consumes the event as a Boolean without discarding the timing data used by scope consumers.

Extract the clock core (`TimeLoop`, `TheoryOfTimeBase`, and `PhaseDomain`) into `private/src/TheoryOfTimeBase.hpp`. Keep the frequency/LFO/MIDI orchestration in `TheoryOfTime.hpp`. PolyXFader includes the core directly, removing both `GetTheoryOfTimePhasor` and `GetTheoryOfTimeTop` forward-declaration bridges. Remove master-only convenience overloads, direct/indirect synonyms, unwound accessors, and internal/external multiplier variants. Use one full loop cycle per gate step, without a factor of two. Keep the separate MultiPhasorGate note cutoff at 0.5 of a voice cycle. Name state consistently (`m_unmodulatedPhase`, `m_modulatedPhase`, `m_periodTicks`, `m_cycleRatio`, `m_unmodulatedCycleCrossed / m_modulatedCycleCrossed`, `m_globalPeriodSamples`). Keep ordinary const/non-const access where genuinely necessary; this is not a repository-wide renaming of unrelated oscillators or historical artifacts.

### 2. Pure coordinate evaluation

Advance unmodulated input phase without wrapping; modulated global phase is unmodulated phase plus the existing LFO offset, also without wrapping. Store the two global phase arrays, accepted topology snapshots, and derived absolute integer positions. Loop phases need no separate winding or fractional storage: `loopPhase = globalPhase * cycleRatio`.

Use the LCM of the global cycle ratios as the lattice period, without doubling; loop periods of one tick are valid. For global period ticks `Lg`, evaluate `P = floor(globalPhase * Lg)` once per domain/sample. All loops observe that same absolute position through their own period `L`. Their gate-step index is `FloorDiv(P, L)`. At each modulated tick, evaluate the configured rhythm at the reset-relative index modulo its size, and hold the resulting gate between ticks. Use Euclidean helpers with positive divisors; never C++ truncating division for signed coordinates. Widen lattice multiplication intermediates and arp index propagation to `int64_t`, using ordinary multiplication and `std::lcm`; input assertions enforce valid parent ordering and positive multipliers.

Changing the lattice changes tick units. Derive both previous and current comparison positions from their absolute global phases in the same lattice; never compare differently scaled stored ticks or rescale accumulated child history. Capture old-topology boundary events before accepting edits. After acceptance, recompute coordinates under the new topology without interpreting the coordinate remap as elapsed travel.

An index/cycle crossing flag reports whether at least one corresponding boundary was crossed between samples, even if the final gate Boolean matches the previous gate after a multi-cycle jump. Emit at most one consumer event per sample; do not synthesize intermediate samples or rewrite the actual previous coordinate to an adjacent tick. Transport startup has an explicit first-sample event rather than pretending a positive wrapped end-of-loop coordinate is the previous absolute time. Stopped state remains deterministic with topology-derived periods available before first run.

### 3. Topology acceptance and interpolation

For a running loop, evaluate the old parent and requested parent against the same pre-edit topology snapshot and sample interval. Accept the requested parent/multiplier pair only when both parents cross their modulated cycle boundary on that sample. For a multiplier-only edit, they are the same parent. Defer the entire pair otherwise. Compute eligibility for all requested edits before mutating any loop so iteration order cannot change another edit's eligibility. Keep the existing valid tree ordering and positive-multiplier constraints. Stopped/startup topology acceptance does not wait for motion.

The interpretation of the user's simultaneous-top rule is old-parent AND requested-parent top, not merely a top of the old parent or an OR of both. The existing independent/modulated mismatch remains explicitly accepted scope.

`GetPhase` first interpolates the global phase and then applies the accepted cycle ratio for that sample interval. At fractional position between integer samples j and j+1, use j's topology; at exactly j+1 use its topology. This is right-continuous parameter application and avoids interpolating an integer topology-induced jump in a child absolute coordinate. Permit access to lookahead slot 8 and interpolate positions in [7,8]; copy slot 8 to slot 0 on rollover.

### 4. Gate-step indices replace monodromy reconstruction

Without reset, return `FloorDiv(P, Lclock)`. For the selected clock itself or an ancestor reset:

```text
stepsPerReset = Lreset / Lclock
index = FloorMod(FloorDiv(P, Lclock), stepsPerReset)
```

A non-ancestor selection retains existing behavior: ignore it and return the absolute index. An ancestry walk is allowed for that validation; no recursive numerical reconstruction remains. A self reset returns zero because `Lreset / Lclock` is one. Full-cycle coordinates are the gate-step coordinates. An absolute index is a location, not a lifetime count of events; reverse motion decreases it.

IndexArp uses floor division/modulo to split a signed index into motive and rhythm coordinates, keeps absolute/motive intermediates in 64-bit or double until the bounded output mapping, and never uses a negative rhythm array subscript. Retain inversion, retro, folding, and zone mapping semantics.

### 5. Consumer behavior

PhasorPlayHead evaluates `WrapMod(0, length, absoluteLoopPhase * speed * length) + start`, then wraps into the sample's normalized output range. A half-speed head completes one cycle over two source cycles. Preserve start/window, zero-speed, and all 17 rate settings. Delay read/write coordinates remain absolute through projection; only final circular-buffer addresses and display values wrap. Preserve WriteTapeHead's tempo-change glue and stopped write behavior: those represent buffer continuity rather than winding reconstruction.

PolyXFader keeps `u = frac(absoluteLoopPhase + shift + 0.75)` inside waveform evaluation, followed by its current multiplier/partial-lobe shaping. Compute the reduction in double before float conversion. At an aligned topology edit, integer changes in loop phase leave the waveform unchanged. Existing external weights, center/slope controls, quantization, sample-and-hold, and slew remain intact. There is no new promise of constant blend output when the user or existing automatic weights change.

AHD captures `m_startGlobalPhase`, `m_phaseRatio`, and `m_envelopePeriodSamples` on trigger. The ratio is the selected source phase's cycle ratio relative to the modulated global phase, not a division of instantaneous phase values. During Running:

```text
elapsedCycles = abs(globalModulatedPhase - startGlobalPhase) * capturedPhaseRatio
elapsedSamples = elapsedCycles * capturedEnvelopePeriodSamples
```

No loop index, topology callback, winding tracker, or accumulated progress is retained by AHD. The trigger producer supplies the ratio. Preserve existing source selection during migration (production AHD inputs currently default to loop 0); do not silently substitute the voice gate denominator, which is a different quantity. Tests can supply a ratio from any selected loop. Capture the envelope period too, so later topology-derived denominator changes cannot rescale elapsed progress or hold length. Existing attack/decay/hold knob behavior remains live, with hold evaluated against that captured period. Retriggers capture fresh timing and start from current amplitude; release retains its existing per-sample decay behavior. Global phase modulation and reversal continue to affect active envelopes.

MultiPhasorGate already captures a voice denominator at trigger; retain that policy using absolute global distance and consistent captured-period publication. Compute voice cycle ratio as the undoubled LCM of the selected clock and all read loop ratios. Neither contribution gets a factor of two. Distinguish that voice ratio from AHD source cycle ratio, and retain the separate half-voice-cycle note-gate cutoff. Remove the now-redundant AHDControl/Input elapsed-sample relay and its filter only after checking all consumers (including DeepVocoder); global phase is the sole progress source for AHD.

RecordingBuffer and ExternalClockSync read unmodulated global phase directly, without combining modulated winding with an unmodulated fraction. MIDI ticks derive from division crossings of unmodulated absolute phase. Note scopes and normalized displays receive wrapped values at their final adapters; recording durations remain absolute differences. Use explicit sample indices throughout, including replacing direct reads of a rolling global tracker in TapeHead/QuadDelay.

### 6. Whole-cycle rhythm and controller contract

Each loop has 16 engine gate slots, an active size, and a reset selection. Defaults are size 2, slot zero true, all others false, reset -1. A tick means a modulated full-cycle crossing, independent of whether the gate value changes or self reset keeps the index at zero. `AnyTick` aggregates these events for Nonagon's selected clock and read dimensions. Gate/size/reset edits are consumed only at the edited loop's next modulated tick, including when faster loops tick first; edits alone do not raise any-change.

Per-sample order is positions, crossings, accepted topology remap preserving flags, then gate evaluation. Startup forces all crossing flags and evaluates every rhythm; stop forces gates false. Invalid ancestry never deletes a stored reset: core queries ignore it and UI hides it until ancestry returns.

Wrld.Bldr aux pad (1,1) selects TheoryOfTimeRhythm mode, pairing rhythm on the left with ancestor resets on the right. Press toggles a gate; Shift-press sets length to row+1. Only strict ancestors are selectable; repeat press clears reset. StateSaver persists slots 0-7 through TheoryOfTimeRhythm and each loop's TheoryOfTimeRhythmSize and TheoryOfTimeRhythmReset. The engine capacity is 16, while current UI size is 1-8 and slots 8-15 have no persistence entries. Adding this mode shifts later runtime ordinals but requires no patch migration.

### 7. LameJuis acceptance and section identity

Pass each input gate and its modulated tick separately into LameJuis. Co-mutes and individual matrix elements accept only on their own input tick, including equal-gate steps. A row accepts RHS and target edits when any ticked input is non-muted in the accepted or requested matrix. Accumulator intervals retain process-time acceptance.

The row owns active-input counts, masks, RHS, and target; sections own per-accumulator counts of active rows. When the last mute is accepted, make the row false and exclude it from every accumulator in the same frame's sheaf rebuild. An empty row remains inactive until a requested unmute's input ticks; that tick also accepts current RHS and target requests. Initialize each lane's grid converter from the initial co-mute lens rather than waiting for the first co-mute edit.

Section identity includes all high counts and total counts, with evaluated pitch compared by the selected-result wrapper. Denominator-only changes may request a same-pitch note on an existing read or arp trigger. They do not create new read times. Nonagon captures the new section's timbre coefficients when a note actually starts.

### 8. Alternatives considered

- Keeping wrapped state with wider winding counters preserves the unwanted playback restart and reconstruction complexity; rejected.
- Reanchoring each active AHD on topology edits would require following edits; rejected in favor of the user's trigger-captured ratio.
- Moving PolyXFader's multiplier before wrapping changes its established partial-lobe waveform; preserve it while changing PhasorPlayHead as requested.
- A global fixed lattice or new fixed-point representation adds machinery not needed when positions derive afresh from double global phase and current topology; retain the existing LCM resolution policy.

## Risks / Trade-offs

- Existing continuity tests request edits without proving acceptance, and some only warn on a jump → new tests must observe accepted parameters and fail at actual transitions.
- Integer-coordinate remaps can masquerade as large travel during interpolation/event detection → interpolate global phase, snapshot topology, and compare in consistent lattice units.
- Float casts in consumers can discard the benefit of double phase → reduce to bounded output or subtract the captured origin before narrowing.
- Active AHD notes now keep their trigger-time ratio/period → explicit test that edits affect the next trigger, not the current note.
- Independent/modulated boundary mismatch can still move the phase-modulation LFO when topology changes → retain existing behavior and document the limitation; no added smoothing subsystem.

## Migration Plan

Implement the arithmetic and core contracts, migrate time consumers and index propagation, fix acceptance/interpolation, replace AHD tracking, and verify focused plus full tests. Keep the repository buildable at each completed task; temporary compatibility shims may exist within a task but none remain in the final change. Preserve persisted key strings. Update current docs and delta specs, leaving archived history untouched. New rhythm fields follow ordinary StateSaver loading: missing keys preserve current values, with constructor defaults only on a fresh instance. The controller mode ordinals are runtime-only. Remove the obsolete LaTeX source and PDF; the active time description is Markdown.

## Open Questions

None blocking planning. The naming table, simultaneous-parent interpretation, and captured AHD period are explicit design choices for review.
