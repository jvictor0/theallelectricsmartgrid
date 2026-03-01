---
name: add-source-machine
description: How to add a new source machine to Smart Grid One. Covers VoiceMachineEnums, creating the DSP class, integrating into SquiggleBoySource, SetEncoderParameters wiring, and MachineFlags updates. Use when adding a new oscillator, synth engine, or audio source.
---

# Adding a New Source Machine to Smart Grid One

## Overview

A source machine generates audio before it reaches the filter. Examples: `DualWaveShapingVCO` (wavetable/phase-shaping VCO), `PhysicalModeling` (noise-excited comb filter), `Thru` (passes external audio). Adding a new source machine requires:

1. Update the enum in `VoiceMachineEnums.hpp`
2. Create a DSP class file for the new machine
3. Add the machine instance and input struct to `SquiggleBoySource`
4. Integrate into `ProcessUBlock` and `SetEncoderParams` in `SquiggleBoySource`
5. Update `MachineFlags.hpp` if the machine count changes
6. Add machine-specific parameters to `ForEachSmartGridOneParam.hpp`
7. If the source has an internal AHD envelope, connect it in `SquiggleBoyVoice`

## Step 1: Update VoiceMachineEnums.hpp

Add your new machine to the `SourceMachine` enum and update `NumSourceMachines`:

```cpp
enum class SourceMachine : int
{
    DualWaveShapingVCO = 0,
    Thru = 1,
    PhysicalModeling = 2,
    MyNewSource = 3,           // ADD: new machine
    NumSourceMachines = 4,     // UPDATE: increment count
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
        case SourceMachine::MyNewSource:      // ADD
            return "MySource";                // ADD: short display name
    }

    return "Unknown";
}
```

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

In `SquiggleBoySource::SetEncoderParams`, add a case and helper function:

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
            SetMyNewSourceParams(encoders, input.m_myNewSourceInput, voiceIx, baseFreq);
            break;
        }

        default:
            break;
    }
}

void SetMyNewSourceParams(
    SmartGridOneEncoders& encoders,
    MyNewSource::Input& input,
    size_t voiceIx,
    float baseFreq)
{
    input.m_baseFreq = baseFreq;
    input.m_myParam = encoders.GetValueNoSlew(Param::MyNewParam, voiceIx);
    // ... wire other params ...
}
```

## Step 5: Update MachineFlags.hpp

If the total number of source machines changes, update `MachineFlags.hpp`:

```cpp
static constexpr uint8_t x_all = 0b1111;                   // 4 machines
static constexpr uint8_t x_dualWaveShapingVCOOnly = 0b0001;
static constexpr uint8_t x_physicalModelingOnly = 0b0100;
static constexpr uint8_t x_myNewSourceOnly = 0b1000;       // ADD: for machine-specific params
static constexpr size_t x_numSourceMachines = 4;           // UPDATE to match enum count
```

## Step 6: Add Machine-Specific Parameters

In `ForEachSmartGridOneParam.hpp`, add parameters for your machine using the machine-specific flag:

```cpp
F(MyNewParam, MNP, Source, 0, 0, 0.5, "My New Param", SmartGrid::Color::Cyan, 
  MachineFlags::x_myNewSourceOnly,  // Only show for MyNewSource
  MachineFlags::x_all)              // All filter machines
```

Parameters at the same (x, y) position can coexist if they have different machine flags - only the matching machine's params will appear.

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
| `private/src/MachineFlags.hpp` | Update `x_numSourceMachines`, update `x_all`, add bitmask constant |
| `private/src/ForEachSmartGridOneParam.hpp` | Add machine-specific parameters |
| `private/src/SquiggleBoy.hpp` | (Optional) Connect internal AHD in SetGates and Processs |

## Checklist

- [ ] Enum value added to `SourceMachine` in VoiceMachineEnums.hpp
- [ ] `NumSourceMachines` incremented
- [ ] ToString case added
- [ ] DSP class created with `m_uBlockOutput` and `m_uBlockTop`
- [ ] Member and input struct added to SquiggleBoySource
- [ ] ProcessUBlock case added in SquiggleBoySource
- [ ] SetEncoderParams case and helper function added in SquiggleBoySource
- [ ] MachineFlags constants updated (`x_all`, `x_numSourceMachines`, new bitmask)
- [ ] Machine-specific parameters added to ForEachSmartGridOneParam.hpp
- [ ] (If applicable) Internal AHD connected in SquiggleBoyVoice::SetGates and Processs
