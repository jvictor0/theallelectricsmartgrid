## Why

Machine-dependent voice parameters currently stop participating in change-driven recompute when the selected trio uses a machine that does not expose those parameters in the 4x4 encoder bank. This lets hidden parameters miss one-frame gesture/modulator change edges, leaving DSP output stale even though the stored parameter still exists and may be read by another trio's voices.

## What Changes

- Move encoder shared processing state from transient bank placement to mode-level ownership so every parameter cell is wired continuously, whether or not it is visible in the current machine topology.
- Separate parameter processing from 4x4 bank topology so processing covers the full parameter array, while bank placement remains a UI/view concern.
- Add regression coverage for hidden machine-dependent parameters across processing, gesture clearing, visible bank-reset scope, and scene-copy behavior.
- Update encoder parameter system requirements so hidden machine-dependent parameters must stay semantically live and receive bulk operations that affect their mode.
- Preserve visible UI behavior for disconnected grid positions: a hidden parameter should not appear in a bank position for an incompatible machine.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `encoder-parameter-system`: machine-dependent parameters that are hidden from the current bank topology must remain wired, processable, and included in relevant mode-level bulk operations.

## Impact

- Affected implementation: `private/src/EncoderBank.hpp`, `private/src/EncoderBankBank.hpp`, `private/src/SmartGridOneEncoders.hpp`, and tests under `private/test/system/`.
- No intended change to public patch JSON shape, DSP parameter names, controller routes, or UIState field layout.
- Runtime cost increases from processing currently placed bank cells to evaluating the full named parameter array on control frames. The current parameter count is small enough for this to be acceptable, and the existing change-driven recompute checks remain in each cell.
