---
name: add-source-machine
description: How to add a new source machine to Smart Grid One. Covers VoiceMachineEnums, creating the DSP class, integrating into SquiggleBoySource, SetEncoderParameters wiring, and parameter page/slot routing. Use when adding a new oscillator, synth engine, or audio source.
---

# Adding a New Source Machine to Smart Grid One

## Overview

A source machine generates audio before it reaches the filter. Examples: `DualWaveShapingVCO` (wavetable/phase-shaping VCO), `PhysicalModeling` (noise-excited comb filter), `Thru` (passes external audio). Adding a new source machine requires:

1. Update the enum in `VoiceMachineEnums.hpp`
2. Create a DSP class file for the new machine
3. Add the machine instance and input struct to `SquiggleBoySource`
4. Integrate into `ProcessUBlock` and dispatch in `SetEncoderParams` in `SquiggleBoySource`
5. Add the source page and typed Source-slot route
6. Add parameter metadata and page memberships; update visualizer flags if needed
7. If the source has an internal AHD envelope, connect it in `SquiggleBoyVoice`

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
    }

    return "Unknown";
}
```

If a visualizer gates on the new source machine, update the separate `VoiceMachine::SourceMachineFlags` in `VoiceMachineEnums.hpp`:

- add a bit constant for the new machine
- include it in `x_all`
- add a helper (for example `MyNewSourceOnly()`) if needed for visualizer gating

## Step 2: Create the DSP Class

Create a new header file `private/src/MyNewSource.hpp` following the pattern of `DualWaveShapingVCO.hpp` or `PhysicalModelingSource.hpp`:

```cpp
#pragma once

#include "PhaseUtils.hpp"
#include "SampleTimer.hpp"
#include "Slew.hpp"

struct MyNewSource
{
    static constexpr size_t x_oversample = 4;
    static constexpr size_t x_uBlockSize = SampleTimer::x_controlFrameRate * x_oversample;

    // Output buffers (required interface)
    //
    bool m_uBlockTop[SampleTimer::x_controlFrameRate];
    float m_uBlockOutput[x_uBlockSize];

    // Internal DSP state
    //
    float m_phase;

    // Slewed parameters (for oversampled processing)
    //
    ParamSlew m_myParamSlew;

    struct Input
    {
        float m_baseFreq;
        float m_myParam;

        Input()
            : m_baseFreq(PhaseUtils::VOctToNatural(0.0, 1.0 / 48000.0))
            , m_myParam(0.5f)
        {
        }
    };

    MyNewSource()
        : m_phase(0.0f)
        , m_myParamSlew(x_oversample)
    {
    }

    void ProcessUBlock(const Input& input)
    {
        // Update slew targets at start of block
        //
        m_myParamSlew.Update(input.m_myParam);

        for (size_t i = 0; i < x_uBlockSize; ++i)
        {
            float myParam = m_myParamSlew.Process();
            float baseFreq = input.m_baseFreq / x_oversample;

            // Your DSP here
            //
            m_phase += baseFreq;
            if (m_phase >= 1.0f)
            {
                m_phase -= 1.0f;
            }

            m_uBlockOutput[i] = /* your output */;

            // Track zero crossings for sync (every x_oversample samples)
            //
            if ((i + 1) % x_oversample == 0)
            {
                size_t baseIndex = i / x_oversample;
                m_uBlockTop[baseIndex] = /* true if top of cycle */;
            }
        }
    }
};
```

Key requirements:
- `m_uBlockOutput[x_uBlockSize]`: oversampled audio output buffer
- `m_uBlockTop[SampleTimer::x_controlFrameRate]`: cycle-top flags for filter key tracking
- Use `ParamSlew` for any parameter consumed in the oversampled loop

## Step 3: Add to SquiggleBoySource

In `private/src/SquiggleBoySource.hpp`, add the include, member, and input struct:

```cpp
#include "DualWaveShapingVCO.hpp"
#include "MyNewSource.hpp"              // ADD

// ... inside struct SquiggleBoySource ...

    // Source machines
    //
    DualWaveShapingVCO m_dualWaveShapingVCO;
    PhysicalModelingSource m_physicalModeling;
    MyNewSource m_myNewSource;          // ADD

// ... inside struct Input ...

    DualWaveShapingVCO::Input m_dualWaveShapingVCOInput;
    PhysicalModelingSource::Input m_physicalModelingInput;
    MyNewSource::Input m_myNewSourceInput;  // ADD
```

## Step 4: Integrate into ProcessUBlock and SetEncoderParams

In `SquiggleBoySource::ProcessUBlock`, add a case for your machine:

```cpp
void ProcessUBlock(Input& input)
{
    switch (input.m_sourceMachine)
    {
        case SourceMachine::DualWaveShapingVCO:
        {
            m_dualWaveShapingVCO.ProcessUBlock(input.m_dualWaveShapingVCOInput);
            m_uBlockOutput = m_dualWaveShapingVCO.m_uBlockOutput;
            m_uBlockTop = m_dualWaveShapingVCO.m_uBlockTop;
            break;
        }

        // ... other cases ...

        case SourceMachine::MyNewSource:  // ADD
        {
            m_myNewSource.ProcessUBlock(input.m_myNewSourceInput);
            m_uBlockOutput = m_myNewSource.m_uBlockOutput;
            m_uBlockTop = m_myNewSource.m_uBlockTop;
            break;
        }

        default:
            break;
    }
}
```

In `SquiggleBoySource::SetEncoderParams`, add a case that dispatches to the machine's own setter:

```cpp
void SetEncoderParams(
    SmartGridOneEncoders& encoders,
    Input& input,
    size_t voiceIx,
    float baseFreq,
    float wtBlend0,
    float wtBlend1)
{
    switch (input.m_sourceMachine)
    {
        // ... existing cases ...

        case SourceMachine::MyNewSource:  // ADD
        {
            m_myNewSource.SetEncoderParams(encoders, input.m_myNewSourceInput, voiceIx, baseFreq);
            break;
        }

        default:
            break;
    }
}

```

Recommended pattern: keep machine-specific parameter mapping inside the machine class (`MyNewSource::SetEncoderParams`) to keep `SquiggleBoySource` as a thin dispatcher.

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

If your source has an internal AHD envelope (like PhysicalModeling), you need to:

1. In `SquiggleBoyVoice::Input::SetGates()`, set the AHD input from the voice's AHDControl:

```cpp
void SetGates()
{            
    m_filterInput.m_ahdInput.Set(m_ahdControl);
    m_ampInput.m_ahdInput.Set(m_ahdControl);
    m_sourceInput.m_myNewSourceInput.m_ahdInput.Set(m_ahdControl);  // ADD
    // ...
}
```

2. In `SquiggleBoyVoice::Processs()`, process the AHD at control rate:

```cpp
float Processs(Input& input)
{
    // ...
    
    // Process MyNewSource AHD if that source is active
    //
    if (input.m_voiceConfig.m_sourceMachine == VoiceConfig::SourceMachine::MyNewSource)
    {
        m_source.m_myNewSource.m_ahd.Process(input.m_sourceInput.m_myNewSourceInput.m_ahdInput);
    }
    
    // ...
}
```

## Files Modified

| File | Changes |
|------|---------|
| `private/src/VoiceMachineEnums.hpp` | Add enum value, update count, add ToString case |
| `private/src/MyNewSource.hpp` | **Create new file** with DSP implementation |
| `private/src/SquiggleBoySource.hpp` | Add include, member, input struct, ProcessUBlock case, SetEncoderParams dispatch |
| `private/src/SmartGridOneLayout.hpp` | Add the source page identifier |
| `private/src/ForEachSmartGridOneSlot.hpp` | Add a typed Source-slot route for the new machine |
| `private/src/ForEachSmartGridOnePage.hpp` | Place the new and any shared parameters on the source page |
| `private/src/VoiceMachineEnums.hpp` | Update `SourceMachineFlags` only if visualizers need source gating |
| `private/src/ForEachSmartGridOneParam.hpp` | Add metadata rows for new parameters only |
| `private/src/SquiggleBoy.hpp` | (Optional) Connect internal AHD in SetGates and Processs |

## Checklist

- [ ] Enum value added to `SourceMachine` in VoiceMachineEnums.hpp
- [ ] `NumSourceMachines` incremented
- [ ] ToString case added
- [ ] DSP class created with `m_uBlockOutput` and `m_uBlockTop`
- [ ] Member and input struct added to SquiggleBoySource
- [ ] ProcessUBlock case added in SquiggleBoySource
- [ ] SetEncoderParams dispatch added in SquiggleBoySource (machine owns its own parameter setter)
- [ ] New page identifier and typed Source-slot route added
- [ ] New parameter metadata and source-page memberships added as needed
- [ ] VoiceMachine::SourceMachineFlags updated if source-specific visualizers need the new bit
- [ ] (If applicable) Internal AHD connected in SquiggleBoyVoice::SetGates and Processs
