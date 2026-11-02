---
name: create-parameter
description: How to add a new parameter to Smart Grid One. Covers ForEachSmartGridOneParam, SetEncoderParameters wiring, PhaseUtils (ExpParam, ZeroedExpParam), and per-sample consumption. Use when adding a new knob/parameter, creating a control, or wiring encoders to DSP.
---

# Creating a Parameter in Smart Grid One

## Overview

Adding a parameter requires: (1) declaring it in the parameter list, (2) wiring it in `SetEncoderParameters` or a track/machine `SetEncoderParams`, and optionally (3) using exponential mapping. Parameters are read every engine sample.

## Step 1: Define the parameter and its page placement

Add one metadata row to `private/src/ForEachSmartGridOneParam.hpp`:

```cpp
F(name, shortName, mode, defaultValue, description, color, switchValues, bipolar)
```

- **name**: `SmartGridOneLayout::Param` identifier (for example `MyCutoff`)
- **shortName**: 3–4 character encoder label
- **mode**: `Voice`, `Quad`, or `Global`; Voice creates one owned cell per trio
- **defaultValue**: Default in the exposed knob domain
- **description**, **color**: Long name and encoder color
- **switchValues**: Number of discrete values, or `0` for a continuous control
- **bipolar**: Whether the exposed knob domain is signed

For example, a Voice parameter on the Thru source page can use an unoccupied coordinate:

```cpp
// ForEachSmartGridOneParam.hpp
//
F(MyCutoff, CUT, Voice, 0.5, "My Cutoff", SmartGrid::Color::Green, 0, false)

// ForEachSmartGridOnePage.hpp
//
F(ThruSource, MyCutoff, 0, 0)
```

The page row uses `F(page, param, x, y)` with zero-based coordinates from 0 through 3. `ForEachSmartGridOneSlot.hpp` already routes `VoiceMachine::SourceMachine::Thru` from the Source slot to `ThruSource`, so no new route is needed. For a machine-dependent control, place the same parameter on the relevant source pages; one parameter can occur on multiple alternative pages while retaining a single owned cell per trio. Add or adjust a route row only when adding a page alternative or a new slot. The route signature is `F(slot, enumtype, enumval, page, color)`. `SmartGridOneLayout::BuildPageAndSlotTables` validates references, coordinates, route coverage, and logical ownership at compile time; the route's machine enum selects the page directly.

## Step 2: Wire in SetEncoderParameters

`SquiggleBoyWithEncoderBank::ProcessSample` runs `m_encoders.Process()` and then `SetEncoderParameters(input)` (`SquiggleBoy.hpp`) every engine sample. Add the assignment where the parameter's consumer lives:

- **Voice params**: In `SquiggleBoyTrack::SetEncoderParams` (`SquiggleBoyTrack.hpp`), or in the setter of the machine that consumes it (`SquiggleBoySource` / `SquiggleBoyShape` dispatch to `DualWaveShapingVCO`, `PhysicalModelingSource`, `SampleSource`, `Ladder4PoleShape`, `SVF2PoleShape`). Read a whole track with `encoders.GetValue<Size>(Param::MyParam, track)`, which returns one value per voice (`TrioFloat` for a trio). Per-voice mixer values use the `for (size_t i = 0; i < x_numVoices; ++i)` loop in `SetEncoderParameters` with `m_encoders.GetValue(Param::MyParam, i)`.
- **Quad params** (e.g. delay): Inside the `for (int i = 0; i < 4; ++i)` loop for delay/reverb.
- **Global params**: Outside loops, no voice index: `m_encoders.GetValue(Param::MyParam)`.

Assign to the correct input struct member (`input.m_sourceInput`, `input.m_shapeInput`, `input.m_ampInput`, `input.m_panInput`, or the machine's own `Input`).

## Smoothing

The encoder matrix computes on host frames, and `GetValue` interpolates linearly across each host frame from the previous output to the latest (exact at oversample 1), with no further slew. Smoothing happens at the sources: modulators are continuous, scene blend and fader inputs are slewed on arrival, and encoder ticks and `EncoderSet` messages are spread over the time since the cell's previous edit (`TickDistributor`, 2 to 50 ms). Consumers read the value every engine sample and do not add their own parameter slew.

## Step 3: Exponential Parameters (PhaseUtils)

For parameters that should feel exponential (e.g. cutoff, gain, pitch):

### ExpParam

Maps [0, 1] to [min, max] exponentially.

- Add member to input struct: `PhaseUtils::ExpParamGeneric<float, Size> m_myParam;` (`PhaseUtils::ExpParam` for one channel)
- Constructor: `ExpParam(base)` for single-base, or `ExpParam(min, max)` for explicit range
- In SetEncoderParams: `input.m_myParam.Update(encoders.GetValue<Size>(Param::MyParam, track));`
- Use `m_myParam.m_expParam` in DSP. `Update` also returns the mapped value and only recomputes when the input changes

Example (PitchOffset): `ExpParam(4)` → value 0 → 1×, value 1 → 4×.

### ZeroedExpParam

Maps [0, 1] to [0, max] with 0 at knob 0. Use for modulation depth, crossfade, etc.

- Add member: `PhaseUtils::ZeroedExpParamGeneric<float, Size> m_myParam;` (`PhaseUtils::ZeroedExpParam` for one channel)
- Constructor: `ZeroedExpParam(base)` (default base 20)
- Same Update pattern as ExpParam
- Optional: `SetMax(max)` or `SetBaseByCenter(center)` for curve shaping

## Step 4: Consuming the Parameter per Sample

The voice tracks and their source and filter machines process one engine sample per `Process` call at `SampleTimer::x_sampleRate` (`SampleTimer::x_oversample` engine samples per host frame). The setter runs before each of those samples, so the DSP sees a fresh value every sample:

### Linear params

For simple linear params (e.g. VPS `m_v`, `m_d`, `m_fade`):

1. Input struct: `Vector m_myParam;` (`MultiChannelNumber<float, Size>`)
2. SetEncoderParams: `input.m_myParam = encoders.GetValue<Size>(Param::MyParam, track);`
3. In `Process`: use `input.m_myParam` directly

### Exponential params

1. Mapping: `PhaseUtils::ExpParamGeneric<float, Size>` (or `ZeroedExpParamGeneric`), held in the input struct or the machine
2. SetEncoderParams: `input.m_myParam = m_myParamMapping.Update(encoders.GetValue<Size>(Param::MyParam, track));`, or keep the mapping in the input and read `m_expParam`
3. In `Process`: use the mapped value directly

Frequencies are in cycles per engine sample and times in engine samples; convert constants with the `SampleTimer` helpers (`HzToCycPerSamp`, `SecondsToSamples`, `SecondsToCycPerSamp`, and the `Host` variants for values defined per host sample). Expensive derived state should recompute only when its input changes, as `ExpParam::Update`, `BitRateReducer::SetAmount`, and `SVF2PoleShape::SetSaturationGain` do.

See `DualWaveShapingVCO::SetEncoderParams` (`m_offsetFreqFactorParam`, `m_detuneParam`, `m_crossModIndexParam`) and `Ladder4PoleShape::SetEncoderParams` for both patterns.

## Additional context

For encoder architecture, tracks/voices, modulation, and machine-specific parameters, see [encoder-system.md](docs/encoder-system.md).

For the full system map (DSP, Theory of Time, LameJuis, etc.), see [docs index](docs/index/README.md).

## Checklist

- [ ] Parameter metadata row added in ForEachSmartGridOneParam.hpp with the correct mode and value fields
- [ ] Page membership added in ForEachSmartGridOnePage.hpp at a free coordinate on each intended page
- [ ] Slot route added in ForEachSmartGridOneSlot.hpp only if a new page alternative or slot needs one
- [ ] Assignment added in the consumer's setter (track/machine SetEncoderParams for voice, SetEncoderParameters loop for quad/global)
- [ ] Voice params read per track with `GetValue<Size>(param, track)`
- [ ] If exponential: ExpParam or ZeroedExpParam mapping, `.Update()` in the setter
- [ ] Rate-dependent constants converted with SampleTimer helpers; no per-consumer parameter slew
