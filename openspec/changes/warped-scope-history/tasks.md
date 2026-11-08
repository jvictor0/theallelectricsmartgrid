## 1. Shared history infrastructure

- [x] 1.1 Add atomic inverse storage and a bounded capacity-independent UI read view, with cutoff and wrap tests.
- [x] 1.2 Implement generic warped start/end undo and coherent marker publication; make level-zero scope storage atomic.
- [x] 1.3 Compose ScopeReader axis/value queries through the inverse while preserving unbound readers and existing LOD behavior.

## 2. Consumer integration

- [x] 2.1 Wire voice/quad scopes and modulation overlays to the shared coordinate, preserving transport and backward-trigger rules.
- [x] 2.2 Replace delay positional/min-max envelope storage with a rectified write scope and common dB visualizer.

## 3. Validation and documentation

- [x] 3.1 Add regression coverage for reversals, paired ends, retriggers, fractional markers, restart, overlays, and delay magnitude display.
- [x] 3.2 Run core tests, JUCE visualizer tests, and delay performance smoke checks; fix regressions without weakening assertions.
- [x] 3.3 Update current documentation/specs and validate the OpenSpec change.

## Validation

- `make core-test`: 851 main cases and 36 alternate-rate cases passed; four existing main cases skipped.
- Final test-only addition: rebuilt and ran 38 focused warped-scope, scope, overlay, and delay-head cases; all passed.
- `make visualizers-test`: passed, including magnitude-to-dB rendering.
- Delay performance smoke: all four cases completed with finite output; this is a smoke check, not a timing comparison.
- `openspec validate warped-scope-history --type change --strict` and `git diff --check`: passed.
- Logs retained under `build/results/time-warp-review/warped-scope-*`.
