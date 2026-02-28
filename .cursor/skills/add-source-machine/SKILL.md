---
name: add-source-machine
description: How to add a new source machine to Smart Grid One. Covers VoiceMachineEnums, creating the DSP class, integrating into SquiggleBoy, SetEncoderParameters wiring, and MachineFlags updates. Use when adding a new oscillator, synth engine, or audio source.
---

# Adding a New Source Machine to Smart Grid One

## Overview

A source machine generates audio before it reaches the filter. Examples: `DualWaveShapingVCO` (wavetable/phase-shaping VCO), `Thru` (passes external audio). Adding a new source machine requires:

1. Update the enum in `VoiceMachineEnums.hpp`
2. Create a DSP class file for the new machine
3. Add the machine instance and input struct to `SquiggleBoyVoice`
4. Integrate into `ProcessUBlock` in `SquiggleBoyVoice`
5. Wire encoder parameters in `SetEncoderParameters`
6. Update `MachineFlags.hpp` if the machine count changes
7. Add machine-specific parameters using the create-parameter skill

## Step 1: Update VoiceMachineEnums.hpp

Add your new machine to the `SourceMachine` enum and update `NumSourceMachines`:

```cpp
enum class SourceMachine : int
{
    DualWaveShapingVCO = 0,
    Thru = 1,
    MyNewSource = 2,           // ADD: new machine
    NumSourceMachines = 3,     // UPDATE: increment count
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
        case SourceMachine::MyNewSource:      // ADD
            return "MySource";                // ADD: short display name
    }

    return "Unknown";
}
```

## Step 2: Create the DSP Class

Create a new header file `private/src/MyNewSource.hpp` following the pattern of `DualWaveShapingVCO.hpp`:

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

## Step 3: Add to SquiggleBoyVoice

In `private/src/SquiggleBoy.hpp`, add the include and member:

```cpp
#include "DualWaveShapingVCO.hpp"
#include "MyNewSource.hpp"              // ADD

// ... inside struct SquiggleBoyVoice ...

    DualWaveShapingVCO m_vco;
    MyNewSource m_myNewSource;          // ADD

// ... inside struct Input ...

    DualWaveShapingVCO::Input m_vcoInput;
    MyNewSource::Input m_myNewSourceInput;  // ADD
```

## Step 4: Integrate into ProcessUBlock

In `SquiggleBoyVoice::ProcessUBlock`, add a branch for your machine:

```cpp
void ProcessUBlock(Input& input)
{
    float* audioInput;
    float upsampledSource[x_uBlockSize];

    if (input.m_voiceConfig.m_sourceMachine == VoiceConfig::SourceMachine::DualWaveShapingVCO)
    {
        m_vco.ProcessUBlock(input.m_vcoInput);
        audioInput = m_vco.m_uBlockOutput;
    }
    else if (input.m_voiceConfig.m_sourceMachine == VoiceConfig::SourceMachine::MyNewSource)  // ADD
    {
        m_myNewSource.ProcessUBlock(input.m_myNewSourceInput);
        audioInput = m_myNewSource.m_uBlockOutput;
    }
    else  // Thru
    {
        const float* sourceBlock = input.m_sourceMixer->m_sources[input.m_voiceConfig.m_sourceIndex].m_uBlockOutput;
        m_upsampler.Process(sourceBlock, upsampledSource);
        audioInput = upsampledSource;
    }

    // Pass m_uBlockTop from your source to filter if needed for key tracking
    //
    m_filter.ProcessUBlock(input.m_filterInput, audioInput, m_vco.m_uBlockTop);
    // ...
}
```

If your source provides `m_uBlockTop` for filter key tracking, pass it instead of `m_vco.m_uBlockTop`:

```cpp
bool* topFlags = (input.m_voiceConfig.m_sourceMachine == VoiceConfig::SourceMachine::MyNewSource)
    ? m_myNewSource.m_uBlockTop
    : m_vco.m_uBlockTop;
m_filter.ProcessUBlock(input.m_filterInput, audioInput, topFlags);
```

## Step 5: Wire Encoder Parameters

In `SquiggleBoyWithEncoderBank::SetEncoderParameters`, add assignments for your machine's input struct:

```cpp
void SetEncoderParameters(Input& input)
{
    for (size_t i = 0; i < x_numVoices; ++i)
    {
        // Existing VCO params
        //
        m_state[i].m_vcoInput.m_baseFreq = m_voices[i].m_baseFreqSlew.Process(input.m_baseFreq[i]);
        // ...

        // MyNewSource params (ADD)
        //
        m_state[i].m_myNewSourceInput.m_baseFreq = m_voices[i].m_baseFreqSlew.Process(input.m_baseFreq[i]);
        m_state[i].m_myNewSourceInput.m_myParam = m_encoders.GetValueNoSlew(Param::MyNewParam, i);
        // ...
    }
}
```

## Step 6: Update MachineFlags.hpp

If the total number of source machines changes, update `MachineFlags.hpp`:

```cpp
static constexpr size_t x_numSourceMachines = 3;  // UPDATE to match enum count
```

Also update the bitmask constants if needed:

```cpp
static constexpr uint8_t x_all = 0b111;                    // 3 machines
static constexpr uint8_t x_dualWaveShapingVCOOnly = 0b001;
static constexpr uint8_t x_myNewSourceOnly = 0b100;        // ADD: for machine-specific params
```

## Step 7: Add Machine-Specific Parameters

Use the **create-parameter** skill to add parameters specific to your source machine.

In `ForEachSmartGridOneParam.hpp`, use the `sourceMachines` flag to restrict params to your machine:

```cpp
F(MyNewParam, MNP, Source, 3, 0, 0.5, "My New Param", SmartGrid::Color::Cyan, 
  MachineFlags::x_myNewSourceOnly,  // Only show for MyNewSource
  MachineFlags::x_all)              // All filter machines
```

See `create-parameter/SKILL.md` for full details on parameter creation.

## Files Modified

| File | Changes |
|------|---------|
| `private/src/VoiceMachineEnums.hpp` | Add enum value, update count, add ToString case |
| `private/src/MyNewSource.hpp` | **Create new file** with DSP implementation |
| `private/src/SquiggleBoy.hpp` | Add include, member, input struct, ProcessUBlock branch, SetEncoderParameters wiring |
| `private/src/MachineFlags.hpp` | Update `x_numSourceMachines`, add bitmask constant |
| `private/src/ForEachSmartGridOneParam.hpp` | Add machine-specific parameters |

## Checklist

- [ ] Enum value added to `SourceMachine` in VoiceMachineEnums.hpp
- [ ] `NumSourceMachines` incremented
- [ ] ToString case added
- [ ] DSP class created with `m_uBlockOutput` and `m_uBlockTop`
- [ ] Member and input struct added to SquiggleBoyVoice
- [ ] ProcessUBlock branch added for the new machine
- [ ] Parameters wired in SetEncoderParameters
- [ ] MachineFlags constants updated
- [ ] Machine-specific parameters added via create-parameter skill
