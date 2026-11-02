---
name: add-source-machine
description: How to add a new source machine to Smart Grid One. Covers VoiceMachineEnums, creating the DSP class, integrating into SquiggleBoySource, SetEncoderParameters wiring, and parameter page/slot routing. Use when adding a new oscillator, synth engine, or audio source.
---

# Adding a New Source Machine to Smart Grid One

## Overview

A source machine generates audio before it reaches the filter. Examples: `DualWaveShapingVCO` (wavetable/phase-shaping VCO), `PhysicalModelingSource` (noise-excited tuned comb), `SampleSource` (grain-resynthesized sample playback, in `DualSampleSource.hpp`), and `Thru` (passes external audio from `SourceMixer`).

Each of the three `SquiggleBoyTrack`s (`SquiggleBoyTrack.hpp`) processes a trio of voices. The track's `SquiggleBoySourceGeneric<3>` owns one instance of every machine, each generic over the channel count (`<Size>`) and holding one channel per voice in a `MultiChannelNumber<float, Size>` (`TrioFloat` for a trio). Every `Process` call renders one engine sample for every channel at `SampleTimer::x_sampleRate`; there are no blocks or internal resamplers. Adding a new source machine requires:

1. Update the enum in `VoiceMachineEnums.hpp`
2. Create a DSP class file for the new machine
3. Add the machine instance and input struct to `SquiggleBoySource`
4. Integrate into `Process` and dispatch in `SetEncoderParams` in `SquiggleBoySource`
5. Add the source page and typed Source-slot route
6. Add parameter metadata and page memberships; update visualizer flags if needed
7. If the source has an internal AHD envelope, connect it in `SquiggleBoyTrack`

## Step 1: Update VoiceMachineEnums.hpp

Add your new machine to the `SourceMachine` enum and update `NumSourceMachines`:

```cpp
enum class SourceMachine : int
{
    DualWaveShapingVCO = 0,
    Thru = 1,
    PhysicalModeling = 2,
    Sample = 3,
    MyNewSource = 4,
    NumSourceMachines = 5,
};
```

Add a case to the `ToString` function:

```cpp
inline const char* ToString(SourceMachine machine)
{
    switch (machine)
    {
        case SourceMachine::DualWaveShapingVCO:
            return "Dual VCO";
        case SourceMachine::Thru:
            return "Thru";
        case SourceMachine::PhysicalModeling:
            return "PhysMod";
        case SourceMachine::Sample:
            return "Sample";
        case SourceMachine::MyNewSource:
            return "MySource";
        default:
            return "Unknown";
    }
}
```

If the machine is pitched, add it to the synth-like cases of `IsSynthLike`; the amp section applies its frequency-dependent gain only to synth-like sources.

If a visualizer gates on the new source machine, update the separate `VoiceMachine::SourceMachineFlags` in `VoiceMachineEnums.hpp`:

- add a bit constant for the new machine
- include it in `x_all`
- add a helper (for example `MyNewSourceOnly()`) if needed for visualizer gating

## Step 2: Create the DSP Class

Create a new header file `private/src/MyNewSource.hpp` following the pattern of `DualWaveShapingVCO.hpp` or `PhysicalModelingSource.hpp`:

```cpp
#pragma once

#include "MultiChannelNumber.hpp"
#include "PhaseUtils.hpp"
#include "SampleTimer.hpp"
#include "SampleTop.hpp"
#include "SmartGridOneEncoders.hpp"

// One sample per call for every channel. Channel i is voice track * Size + i.
//
template <size_t Size>
struct MyNewSourceGeneric
{
    using Vector = MultiChannelNumber<float, Size>;

    Vector m_phase;
    Vector m_output;
    SampleTop m_top[Size];

    // Encoder mappings
    //
    PhaseUtils::ExpParamGeneric<float, Size> m_myParamMapping;

    // Frequencies are in cycles per engine sample.
    //
    struct Input
    {
        Vector m_baseFreq;
        Vector m_myParam;

        Input()
            : m_baseFreq(Vector::Splat(0))
            , m_myParam(Vector::Splat(1))
        {
        }
    };

    MyNewSourceGeneric()
        : m_phase(Vector::Splat(0))
        , m_output(Vector::Splat(0))
        , m_myParamMapping(0.25f, 4.0f)
    {
        for (size_t i = 0; i < Size; ++i)
        {
            m_top[i] = false;
        }
    }

    Vector Process(const Input& input)
    {
        m_phase += input.m_baseFreq;
        Vector wrapped = m_phase - m_phase.Floor();
        for (size_t i = 0; i < Size; ++i)
        {
            m_top[i] = false;
            if (1 <= m_phase[i])
            {
                m_top[i] = SampleTop::FromWrap(wrapped[i], input.m_baseFreq[i]);
            }
        }

        m_phase = Vector::IfLess(m_phase, Vector::Splat(1), m_phase, wrapped);

        // Your DSP here, using m_phase and input.m_myParam
        //
        m_output = /* your output */;
        return m_output;
    }

    void SetEncoderParams(
        SmartGridOneEncoders& encoders,
        Input& input,
        size_t track,
        const Vector& baseFreq)
    {
        using Param = SmartGridOneEncoders::Param;

        input.m_baseFreq = baseFreq;
        input.m_myParam = m_myParamMapping.Update(encoders.GetValue<Size>(Param::MyNewParam, track));
    }
};

using MyNewSource = MyNewSourceGeneric<1>;
using TrioMyNewSource = MyNewSourceGeneric<3>;
```

Key requirements:
- `Vector Process(const Input& input)`: renders one engine sample for every channel and returns it
- `m_top[Size]` (`SampleTop`): optional cycle-top events with a fractional sample offset; the track starts post-filter scope cycles on them. `SampleTop::FromWrap` (phase wrap) and `SampleTop::FromPhases` (integer crossing between two phases) build them
- Frequencies and times are per engine sample; convert constants with the `SampleTimer` helpers (`HzToCycPerSamp`, `SecondsToSamples`, `SecondsToCycPerSamp`, and the `Host` variants for values defined per host sample)
- `SetEncoderParams` runs every engine sample; read each value with `encoders.GetValue<Size>(Param::X, track)` and map it through `PhaseUtils::ExpParamGeneric` / `ZeroedExpParamGeneric` `Update`, which only recompute when the value changes. Encoder values arrive smoothed at their sources, so no per-parameter slew is needed

## Step 3: Add to SquiggleBoySource

In `private/src/SquiggleBoySource.hpp`, add the include, a type alias, the member, and the input struct:

```cpp
#include "DualWaveShapingVCO.hpp"
#include "MyNewSource.hpp"

// ... inside struct SquiggleBoySourceGeneric ...

    using MyNewSourceType = MyNewSourceGeneric<Size>;

    // Source machines
    //
    DualWaveShapingVCOType m_dualWaveShapingVCO;
    PhysicalModelingType m_physicalModeling;
    SampleSourceType m_sampleSource;
    MyNewSourceType m_myNewSource;

// ... inside struct Input ...

    typename DualWaveShapingVCOType::Input m_dualWaveShapingVCOInput;
    typename PhysicalModelingType::Input m_physicalModelingInput;
    typename SampleSourceType::Input m_sampleSourceInput;
    typename MyNewSourceType::Input m_myNewSourceInput;
```

## Step 4: Integrate into Process and SetEncoderParams

`SquiggleBoySource::Process` clears `m_top`, dispatches to the selected machine, and returns `m_output`. Add a case for your machine, copying its tops if it produces them:

```cpp
Vector Process(Input& input)
{
    ClearTops();
    switch (input.m_sourceMachine)
    {
        case SourceMachine::DualWaveShapingVCO:
        {
            m_output = m_dualWaveShapingVCO.Process(input.m_dualWaveShapingVCOInput);
            for (size_t i = 0; i < Size; ++i)
            {
                m_top[i] = m_dualWaveShapingVCO.m_vco[0].m_top[i];
            }

            break;
        }

        // ... other cases ...

        case SourceMachine::MyNewSource:
        {
            m_output = m_myNewSource.Process(input.m_myNewSourceInput);
            for (size_t i = 0; i < Size; ++i)
            {
                m_top[i] = m_myNewSource.m_top[i];
            }

            break;
        }

        default:
            break;
    }

    return m_output;
}
```

In `SquiggleBoySource::SetEncoderParams`, add a case that dispatches to the machine's own setter:

```cpp
void SetEncoderParams(
    SmartGridOneEncoders& encoders,
    Input& input,
    size_t track,
    const Vector& baseFreq,
    const Vector& wtBlend0,
    const Vector& wtBlend1)
{
    switch (input.m_sourceMachine)
    {
        // ... existing cases ...

        case SourceMachine::MyNewSource:
        {
            m_myNewSource.SetEncoderParams(encoders, input.m_myNewSourceInput, track, baseFreq);
            break;
        }

        default:
            break;
    }
}
```

Recommended pattern: keep machine-specific parameter mapping inside the machine class (`MyNewSource::SetEncoderParams`) to keep `SquiggleBoySource` as a thin dispatcher. If the machine writes its own audio scopes, give it a `SetScopeWriters(scopeWriter, track)` like `DualWaveShapingVCO` and call it from `SquiggleBoySource::SetScopeWriters`; call `ScopeWriterHolder::Write(value)` every sample.

## Step 5: Add the Source Page and Route

Give the new source its own `SmartGridOneLayout::Page` value in `SmartGridOneLayout.hpp`. Add a typed route in `ForEachSmartGridOneSlot.hpp` using its existing five-field signature:

```cpp
F(Source, VoiceMachine::SourceMachine, MyNewSource, MyNewSourcePage, SmartGrid::Color::Cyan)
```

Each `SourceMachine` value needs exactly one Source-slot route. `SmartGridOneLayout::BuildPageAndSlotTables` uses the enum value as a direct route index and checks coverage at compile time. It derives the new page's trio instances and keeps the existing live Source slot for each trio; this does not create another owner cell for a shared parameter.

## Step 6: Add Parameter Metadata and Page Memberships

Add any new parameters once to `ForEachSmartGridOneParam.hpp` using its eight-field metadata signature. Place each one in `ForEachSmartGridOnePage.hpp` using the four-field page signature:

```cpp
// ForEachSmartGridOneParam.hpp
//
F(MyNewParam, MNP, Voice, 0.5, "My New Param", SmartGrid::Color::Cyan, 0, false)

// ForEachSmartGridOnePage.hpp
//
F(MyNewSourcePage, MyNewParam, 0, 0)
F(MyNewSourcePage, FilterDrive, 2, 2)
```

A shared parameter may appear on several source pages at the same or different coordinates. Each page coordinate must be unique within that page. The layout helper checks that every parameter belongs to one logical slot, even if it appears on several alternatives. Existing parameter names, defaults, and order are persisted behavior; preserve them when only adding a machine page.

`MachineFlags.hpp` no longer exists and does not control parameter visibility. If source-specific visualizers need to gate on the new machine, add its bit and an optional helper to `VoiceMachine::SourceMachineFlags` in `VoiceMachineEnums.hpp`. Its `x_all` must cover the new bit.

## Step 7: Connect Internal AHD (if applicable)

If your source has an internal AHD envelope (like `PhysicalModelingSource`), you need to:

1. Give the machine an `AHDGeneric<Size>` envelope, an `Envelope::Input m_ahdInput` in its `Input`, and an `Envelope::InputSetter` driven from its `SetEncoderParams`.

2. In `SquiggleBoyTrack::Input::SetGates()`, set the AHD input from each voice's `AHDControl`:

```cpp
void SetGates()
{
    for (size_t i = 0; i < Size; ++i)
    {
        m_ampInput.m_ahdInput.Set(i, m_ahdControl[i]);
        m_ampInput.m_modulationAHDInput.Set(i, m_ahdControl[i]);
        m_sourceInput.m_physicalModelingInput.m_ahdInput.Set(i, m_ahdControl[i]);
        m_sourceInput.m_myNewSourceInput.m_ahdInput.Set(i, m_ahdControl[i]);
        // ...
    }
}
```

3. Run the envelope once per engine sample inside the machine, as `PhysicalModelingSource::ProcessWithAHD` does, and call that from the machine's case in `SquiggleBoySource::Process`:

```cpp
Vector ProcessWithAHD(Input& input)
{
    input.m_envelope = m_ahd.Process(input.m_ahdInput);
    return Process(input);
}
```

`AHD::Process` consumes the trigger and release events, so each one is applied once.

## Files Modified

| File | Changes |
|------|---------|
| `private/src/VoiceMachineEnums.hpp` | Add enum value, update count, add ToString case, update `IsSynthLike` if pitched |
| `private/src/MyNewSource.hpp` | **Create new file** with DSP implementation |
| `private/src/SquiggleBoySource.hpp` | Add include, type alias, member, input struct, Process case, SetEncoderParams dispatch |
| `private/src/SmartGridOneLayout.hpp` | Add the source page identifier |
| `private/src/ForEachSmartGridOneSlot.hpp` | Add a typed Source-slot route for the new machine |
| `private/src/ForEachSmartGridOnePage.hpp` | Place the new and any shared parameters on the source page |
| `private/src/VoiceMachineEnums.hpp` | Update `SourceMachineFlags` only if visualizers need source gating |
| `private/src/ForEachSmartGridOneParam.hpp` | Add metadata rows for new parameters only |
| `private/src/SquiggleBoyTrack.hpp` | (Optional) Connect internal AHD in `Input::SetGates` |

## Checklist

- [ ] Enum value added to `SourceMachine` in VoiceMachineEnums.hpp
- [ ] `NumSourceMachines` incremented
- [ ] ToString case added (and `IsSynthLike` case if pitched)
- [ ] DSP class created, generic over `Size`, with a per-sample `Vector Process(const Input&)` and optional `m_top`
- [ ] Member and input struct added to SquiggleBoySource
- [ ] Process case added in SquiggleBoySource, copying tops if the machine produces them
- [ ] SetEncoderParams dispatch added in SquiggleBoySource (machine owns its own parameter setter)
- [ ] New page identifier and typed Source-slot route added
- [ ] New parameter metadata and source-page memberships added as needed
- [ ] VoiceMachine::SourceMachineFlags updated if source-specific visualizers need the new bit
- [ ] (If applicable) Internal AHD connected in `SquiggleBoyTrack::Input::SetGates` and run in the machine's Process
