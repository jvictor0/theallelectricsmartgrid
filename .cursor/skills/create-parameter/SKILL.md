---
name: create-parameter
description: How to add a new parameter to Smart Grid One. Covers ForEachSmartGridOneParam, SetEncoderParameters wiring, PhaseUtils (ExpParam, ZeroedExpParam), and ParamSlew for oversampled blocks. Use when adding a new knob/parameter, creating a control, or wiring encoders to DSP.
---

# Creating a Parameter in Smart Grid One

## Overview

Adding a parameter requires: (1) declaring it in the parameter list, (2) wiring it in `SetEncoderParameters`, and optionally (3) using exponential mapping or per-sample slew for oversampled blocks.

## Step 1: Add to ForEachSmartGridOneParam.hpp

Add an `F(...)` line in `private/src/ForEachSmartGridOneParam.hpp`:

```
F(name, shortName, bank, x, y, default, "Description", color, sourceMachines, filterMachines)
```

- **name**: C++ enum identifier (e.g. `MyParam`)
- **shortName**: 3–4 char label for encoder display (e.g. `MYP`)
- **bank**: `Source`, `FilterAndAmp`, `PanningAndSequencing`, `VoiceLFOs`, etc.
- **x, y**: Grid position 0–3 within the bank
- **default**: Normalized default in [0, 1]
- **sourceMachines** / **filterMachines**: `MachineFlags::x_all` for all machines, or `MachineFlags::x_dualWaveShapingVCOOnly` etc. for machine-specific params

Example:

```
F(MyCutoff, CUT, FilterAndAmp, 2, 2, 0.5, "My Cutoff", SmartGrid::Color::Green, MachineFlags::x_all, MachineFlags::x_all)
```

## Step 2: Wire in SetEncoderParameters (SquiggleBoy.hpp)

In `SetEncoderParameters(Input& input)`, add the assignment in the appropriate loop:

- **Voice params**: Inside the `for (size_t i = 0; i < x_numVoices; ++i)` loop. Use `m_encoders.GetValue(Param::MyParam, i)` or `GetValueNoSlew(Param::MyParam, i)`.
- **Quad params** (e.g. delay): Inside the `for (int i = 0; i < 4; ++i)` loop for delay/reverb.
- **Global params**: Outside loops, no voice index: `m_encoders.GetValue(Param::MyParam)`.

Assign to the correct input struct member (`m_state[i].m_vcoInput`, `m_filterInput`, `m_ampInput`, etc.).

## GetValue vs GetValueNoSlew

- **GetValue**: Encoder applies its own slew. Use when the value goes directly to DSP without further processing (e.g. pan radius, LFO shape, AHD params).
- **GetValueNoSlew**: Raw value. Use when:
  - Feeding into `ExpParam.Update()` or `ZeroedExpParam.Update()` (they handle their own response)
  - The destination has its own slew (e.g. ParamSlew in oversampled blocks)
  - The parameter is consumed at control rate and you want no encoder slew

## Step 3: Exponential Parameters (PhaseUtils)

For parameters that should feel exponential (e.g. cutoff, gain, pitch):

### ExpParam

Maps [0, 1] to [min, max] exponentially.

- Add member to input struct: `PhaseUtils::ExpParam m_myParam;`
- Constructor: `ExpParam(base)` for single-base, or `ExpParam(min, max)` for explicit range
- In SetEncoderParameters: `m_state[i].m_xxxInput.m_myParam.Update(m_encoders.GetValueNoSlew(Param::MyParam, i));`
- Use `m_myParam.m_expParam` in DSP

Example (PitchOffset): `ExpParam(4)` → value 0 → 1×, value 1 → 4×.

### ZeroedExpParam

Maps [0, 1] to [0, max] with 0 at knob 0. Use for modulation depth, crossfade, etc.

- Add member: `PhaseUtils::ZeroedExpParam m_myParam;`
- Constructor: `ZeroedExpParam(base)` (default base 20)
- Same Update pattern as ExpParam
- Optional: `SetMax(max)` or `SetBaseByCenter(center)` for curve shaping

## Step 4: Parameters in Oversampled Blocks

If the parameter is consumed inside an oversampled block (e.g. `DualWaveShapingVCO::ProcessUBlock`), it must be slewed at the oversample rate to avoid zipper noise.

### Option A: Raw float → ParamSlew

For simple linear params (e.g. `m_v`, `m_d`, `m_fade`):

1. Input struct: `float m_myParam;`
2. Consuming block: add `ParamSlew m_myParamSlew;` with `ParamSlew(x_oversample)` in constructor
3. In ProcessUBlock: `m_myParamSlew.Update(input.m_myParam);` at start of block
4. In inner loop: `float val = m_myParamSlew.Process();` each sample

### Option B: ExpParam → ParamSlew over m_expParam

For exponential params used per-sample:

1. Input struct: `PhaseUtils::ExpParam m_myParam;` (or ZeroedExpParam)
2. SetEncoderParameters: `m_xxxInput.m_myParam.Update(m_encoders.GetValueNoSlew(Param::MyParam, i));`
3. Consuming block: add `ParamSlew m_myParamSlew;` with `ParamSlew(x_oversample)`
4. In ProcessUBlock: `m_myParamSlew.Update(input.m_myParam.m_expParam);` at start
5. In inner loop: `float val = m_myParamSlew.Process();`

See `DualWaveShapingVCO.hpp` for `m_offsetFreqFactorSlew`, `m_detuneSlew`, `m_crossModIndexSlew`.

## Additional context

For encoder architecture, tracks/voices, modulation, and machine-specific parameters, see [encoder-system.md](docs/encoder-system.md).

For the full system map (DSP, Theory of Time, LameJuis, etc.), see [docs index](docs/index/README.md).

## Checklist

- [ ] F(...) added in ForEachSmartGridOneParam.hpp with bank, grid slot, machine flags
- [ ] Assignment added in SetEncoderParameters in correct loop (voice/quad/global)
- [ ] GetValue vs GetValueNoSlew chosen correctly
- [ ] If exponential: ExpParam or ZeroedExpParam in input struct, `.Update()` in SetEncoderParameters
- [ ] If oversampled: ParamSlew in consuming block, Update at block start, Process() per sample
