## 1. Envelope and wiring

- [x] 1.1 Expose modulated ToT increments and add scalar/vector ASD with absolute-delta ramps.
- [x] 1.2 Add the amp muting ASD, copy amp timing, blend effective depth, and wire exactly ToT running and voice unmuted.
- [x] 1.3 Document ASD and the amp muting behavior.

## 2. Validation

- [x] 2.1 Add regression tests for ramps, reversal, transport resets, tempo, precision, independent lanes, depth blending, and production gate wiring; update affected audio fixtures.
- [x] 2.2 Run the core suite and strict OpenSpec validation, review the final diff, and record results.

Validation: `make core-test` passed 819 core cases (4 skipped) and all 32 passthrough-rate cases in RelWithDebInfo with assertions enabled. The new coverage includes ASD gate changes, reversal, zero/large deltas, stopped tempo changes, small-increment precision, independent lanes, amp depth preservation, stopped drone closure, and exact production gate wiring across all nine voices. `openspec validate gate-driven-amp-muting --strict` and `git diff --check` passed.
