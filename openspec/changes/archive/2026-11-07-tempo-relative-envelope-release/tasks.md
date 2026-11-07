## 1. Clock and envelope timing

- [x] 1.1 Expose the basic global clock increment and integrate it during explicit AHD release using captured timing and fractional release factors.
- [x] 1.2 Update envelope documentation for tempo-relative unmodulated release.

## 2. Validation

- [x] 2.1 Add regression coverage for stopped transport, live tempo, captured ratios, modulation independence, fractional steps, and physical-model release; configure existing release fixtures with explicit clocks.
- [x] 2.2 Run the core test suite and validate the OpenSpec change.

Validation: `make core-test` passed both the main core suite and the passthrough-rate suite with RelWithDebInfo assertions enabled. `openspec validate tempo-relative-envelope-release --strict` and `git diff --check` passed.
