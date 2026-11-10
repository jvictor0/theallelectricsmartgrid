## Why

Give each parameter family its own random modulation pair while sharing the other existing sources. Avoid filtering sources that already suit host-rate sampling and separate wavetable morph randomness from parameter modulation.

## What Changes

- Define five three-lane groups per track (Source, Shape, PanMelody, LFO, Sub), four quad groups (Delay, Reverb, PartialMachine, LFO), and the existing global group.
- Assign parameters by page, with voice mixer gain and sends in Shape.
- Provide ten track and eight quad ganged random sources. Each group uses an independent Standard(12, 0.1)/Standard(24, 0.3) pair in slots 2/3; slots 0/1 are empty.
- Give DualWaveShapingVCO two internal wavetable morph random generators and remove external random arguments from source parameter wiring.
- Declare filtering requirements in ForEach source metadata. Sample ganged random, sheaf, spread, and noise sources directly on host frames; generate modulation noise only on host frames.
- Existing patch storage/loading stays unchanged; local random-slot meanings intentionally follow the new instrument mappings.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `modulator-manager`: Page-based groups and per-source downsampling policy.
- `ganged-random-lfo`: Independent per-group pairs and internal VCO morph sources.
- `polyxfader-lfos`: Shared quad LFO outputs feed all four quad groups.

## Impact

Modulator metadata, source manager, DSP publication, VCO wiring, source history/visualizers, and focused integration tests. No new dependencies or changes to main.
