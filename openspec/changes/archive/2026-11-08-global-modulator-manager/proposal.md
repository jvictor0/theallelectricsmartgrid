## Why

Modulator identity is currently tied to a bank-mode slot. A global source manager and fixed modulation groups allow instrument definitions to reuse sources across parameters while preserving direct slot reads in the encoder matrix.

## What Changes

- Introduce globally indexed QuadFloat source values, amplitudes, and per-source filtering in a modulator manager.
- Define fixed Water, Fire, Earth, Quad, and Global groups with 15 slots using ForEach metadata, and static parameter-to-group assignments inherited by child encoders.
- Gather source snapshots into group ModulatorValues on matrix samples; update UI source lookup through group assignments.
- Preserve DSP ownership and processing order, patch representation, loading, and existing default mappings.

## Capabilities

### New Capabilities
- `modulator-manager`: Global published source state and fixed instrument-defined groups.

### Modified Capabilities
- `encoder-parameter-system`: Source filtering belongs to the manager; encoder roots bind to groups independently of their bank modes.
- `polyxfader-lfos`: Existing LFO producers publish to global manager source entries.

## Impact

Encoder source plumbing, modulator catalogue metadata, SquiggleBoy source publication, UI source lookup, and corresponding tests. No patch format or DSP generator changes.
