## 1. Implementation

- [x] 1.1 In `private/src/SquiggleBoy.hpp`, set the Partial Machine synthesis azimuth offset from `m_panPhase.m_phase` after the pan phase advances for the sample.
- [x] 1.2 Preserve all existing Partial Machine lane parameter wiring and avoid adding preset or encoder state.

## 2. Verification

- [x] 2.1 Add or update focused coverage showing the Partial Machine input/UI state receives the same phase value used by voice Lissajous panning.
- [x] 2.2 Run the relevant unit test target for Partial Machine or SquiggleBoy wiring.
