## Why

Explicit AHD release currently uses audio sample time, so stopping or muting a note loses the tempo-relative timing of its normal envelope. Release should continue integrating the basic clock after Stop, without continuing phase modulation.

## What Changes

- Expose the unmodulated global clock increment independently of transport phase.
- Scale explicit release by the current basic clock rate and the envelope's trigger-captured ratio and reference period.
- Keep release independent of phase modulation and transport resets; retain fractional per-call sample factors and double-precision accumulation.
- Cover tempo changes, stopped transport, modulation independence, and physical-model envelopes in tests and documentation.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `ahd-envelopes`: Explicit release integrates tempo-relative basic clock time, including after Stop.

## Impact

`TheoryOfTimeBase`, `AHD`, envelope tests, and `docs/ahd-envelopes.md`. Amp, modulation, and physical-model envelopes share the change. No new dependencies or changes to release triggers, normal AHD progression, or transport phase behavior.
