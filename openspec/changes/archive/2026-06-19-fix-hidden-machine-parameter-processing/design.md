## Context

The encoder parameter system has two responsibilities that are currently entangled:

- Parameter model: the flat `EncoderBankBank::m_encoders[]` array of named `BankedEncoderCell` instances stores patch values, gesture targets, modulation depth cells, and computed DSP outputs.
- Bank view: each `EncoderBankInternal` owns a 4x4 topology of placed cells for controller/UI presentation.

Machine-dependent voice parameters expose the bug. Selecting a trio calls `SquiggleBoyWithEncoderBank::SetTrack()`, which updates the voice bank topology from that trio's source/filter machine. If the selected trio uses a machine that does not expose a parameter, `UpdateEncodersForMachine()` removes that parameter from every voice bank view. The parameter remains in `m_encoders[]`, and DSP for another trio may still read its output by parameter index, but the removed parameter no longer receives the bank-local compute and bulk-operation passes.

The first repro is `OscillatorDetune`: Water uses the VCO machine, Earth uses Sample, and gesture 2 controls Spread modulation depth on Detune. Changing gesture 2 while Earth is selected hides Detune from the Source bank; Water's Detune misses the gesture-change edge and stays stale when Water is selected again.

## Goals / Non-Goals

**Goals:**

- Make encoder cells continuously wired to their bank-mode shared state independent of current 4x4 placement.
- Process all named parameter cells in the flat owner array on control frames, while preserving change-driven recompute inside each cell.
- Ensure hidden machine-dependent parameters participate in mode-level operations that affect stored or computed parameter state: gesture clearing, full patch default reset, and scene copy.
- Preserve machine-dependent UI layout semantics: incompatible parameters remain hidden/disconnected in the selected bank view.
- Add regression tests for the discovered processing anomaly and for each related hidden-parameter bulk-operation anomaly before fixing behavior.

**Non-Goals:**

- Do not change patch JSON schema, parameter names, controller routing, or `EncoderBankUIState` shape.
- Do not remove machine flags or make incompatible parameters visible for the current machine.
- Do not redesign gesture math, modulation mixing math, or scene blend semantics.
- Do not broaden the change into source-machine DSP behavior outside encoder parameter ownership and processing.

## Decisions

### Move shared state to bank mode ownership first

`EncoderBankBank::BankMode` will own the `BankedEncoderCell::SharedEncoderState` for that mode, including `m_numTracks`, `m_numVoices`, and `m_modulatorValues`. `CreateEncoder()` will wire every named encoder to its mode's shared state at creation time. `EncoderBankInternal` will no longer be the only place where a parameter receives `m_sharedEncoderState`; bank placement will only set view-specific ownership needed for UI/interaction.

Alternative considered: force-update cells when they are placed back into a bank. That would fix the immediate missed-edge repro, but it keeps hidden parameters semantically asleep while hidden and leaves the same class of bug in clear/default/copy paths.

### Process the flat parameter array

`EncoderBankBank::Process()` will continue computing changed modulator/gesture masks per mode, then loop `m_encoders[]` and call `Compute()` for each connected named parameter. Bank topology processing will remain responsible for view metadata such as `m_activeEncoderPrefix` and selected visible state, not for core parameter output.

Alternative considered: have each bank process its hidden historical cells too. That duplicates work across banks and preserves the wrong ownership boundary: banks are views, not the canonical parameter set.

### Convert bulk operations from bank-local to mode/owner-aware where needed

Operations that mutate semantic parameter state must operate over the flat parameter set when the user action is global or mode-wide, not only currently placed cells. This includes at least:

- `ClearGesture(gesture)` across all named parameter cells.
- Full patch `RevertToDefault(...)` for all named parameter cells.
- `CopyToScene(scene)` for hidden parameters in the affected scope.
- Affecting-bit recomputation for hidden cells after JSON load, gesture changes, resets, and scene changes.

Visible-grid operations that are inherently UI interactions stay bank-local. Shift-resetting a bank selector resets only the cells currently placed in that bank's visible 4x4 projection.

Alternative considered: keep all bulk operations bank-local and only fix processing. That leaves global patch operations inconsistent: a hidden parameter could still retain a gesture after ClearGesture, skip a full patch reset, or fail to copy scene values.

### Preserve UI topology as a projection

Machine filtering will still call `NullBank()` and `PlaceEncoder()` for the current selected machine. The difference is that placement no longer determines whether the parameter exists, is wired, or is processed. Disconnected grid positions should continue to publish zero/disconnected UIState for the selected topology.

## Risks / Trade-offs

- Processing cost increases from visible placed cells to all named parameters on control frames. Mitigation: the parameter count is currently 163 and each cell already short-circuits unless force-update or affected source bits changed.
- Hidden parameter affecting-bit caches may expose assumptions that only visible cells need summaries. Mitigation: add focused tests for hidden processing, clear, default, and copy operations before changing behavior.
- Moving shared state could disturb modulator/gesture child cells created before or after placement. Mitigation: preserve parent-derived shared-state wiring for nested cells and test gesture-controlled spread depth through the normal front-door flow.
- Bank-local UI summaries may diverge from mode-level semantic summaries. Mitigation: keep UIState publication based on selected visible cells, and only move semantic/bulk processing to the owner array.

## Migration Plan

1. Temporarily disable the existing hidden Detune failing test while moving shared state to mode level, so the intermediate wiring refactor can be verified without requiring the behavior fix in the same step.
2. Move shared state ownership and wire all named encoders at creation time.
3. Add failing tests for hidden processing, hidden gesture clearing, visible-bank reset scope, and hidden scene copy anomalies.
4. Change processing and bulk operations to operate on the full parameter owner array or the appropriate mode-level subset.
5. Re-enable the hidden Detune test and verify the focused suite, then run the full `smartgrid_tests` binary.
