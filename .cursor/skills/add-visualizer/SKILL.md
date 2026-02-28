---
name: add-visualizer
description: How to add a new Smart Grid One visualizer. Covers SmartGridOneMainVisualizerComponent, ForEachSmartGridOneVisualizer, UI state for data, and NonagonWrapper accessors. Use when adding a new scope, analyzer, meter, or other grid visualizer.
---

# Adding a Smart Grid One Visualizer

## Overview

Adding a visualizer requires: (1) creating a component that inherits `SmartGridOneMainVisualizerComponent`, (2) registering it in `ForEachSmartGridOneVisualizer.hpp`, and (3) if the visualizer needs runtime data, adding that data to the UI state and exposing it via `NonagonWrapper`.

## Step 1: Create the Visualizer Component

Create a struct that inherits `SmartGridOneMainVisualizerComponent` and implements `Draw()`. Optionally override `OnClick()`.

**Minimal example** (no data, static drawing):

```cpp
struct MyVisualizerComponent : public SmartGridOneMainVisualizerComponent
{
    MyVisualizerComponent()
        : SmartGridOneMainVisualizerComponent()
    {
    }

    void Draw(juce::Graphics& g, juce::Rectangle<int> boundsRect) override
    {
        g.fillAll(juce::Colours::black);
        auto bounds = boundsRect.toFloat();
        // ... draw using bounds.getWidth(), bounds.getHeight()
    }
};
```

**With UI state** (reads from `m_squiggleBoyUIState`):

```cpp
struct MyVisualizerComponent : public SmartGridOneMainVisualizerComponent
{
    TheNonagonSquiggleBoyInternal::UIState* m_uiState;

    MyVisualizerComponent(TheNonagonSquiggleBoyInternal::UIState* uiState)
        : SmartGridOneMainVisualizerComponent()
        , m_uiState(uiState)
    {
    }

    void Draw(juce::Graphics& g, juce::Rectangle<int> boundsRect) override
    {
        g.fillAll(juce::Colours::black);
        // Read from m_uiState->m_squiggleBoyUIState.*
    }
};
```

**Place the component** in the appropriate header:
- `ScopeComponent.hpp` — Scopes, analyzers, sound stage, melody roll
- `MeterComponent.hpp` — Meter-style visualizers
- `MasteringComponents.hpp` — Mastering chain visualizers
- Or create a new header and include it in `SmartGridOneVisualizerMain.hpp`

## Step 2: Add to ForEachSmartGridOneVisualizer.hpp

Add an `F(...)` entry:

```
F(VisualizerName, Bank, Block, std::make_unique<MyComponent>(...))
```

- **VisualizerName**: C++ identifier for the member (e.g. `MyViz` → `m_MyViz`)
- **Bank**: `SmartGridOneEncoders::Bank` enum value — `Source`, `FilterAndAmp`, `PanningAndSequencing`, `VoiceLFOs`, `Delay`, `Reverb`, `TheoryOfTime`, `Mastering`, `Inputs`, `DeepVocoder`
- **Block**: `0` = (0,0), `1` = (0,8), `2` = (8,8), `3` = (16,8). Use `-1` for full-width (24×8).
- **Constructor**: `std::make_unique<MyComponent>(args)`. Available in constructor context: `m_nonagon`, `uiState`, `m_scopeVoiceOffset`, `m_nonagon->GetAudioScopeWriter()`, etc.

Example:

```
F(
    MyViz,
    Source,
    0,
    std::make_unique<MyVisualizerComponent>(uiState))
```

## Step 3: Add Data to UI State (if needed)

If the visualizer needs data that does not yet exist in UI state:

### Where to add

- **`SquiggleBoyWithEncoderBank::UIState`** in `private/src/SquiggleBoy.hpp` — Add members to `m_squiggleBoyUIState`. This is the main UI state for scopes, meters, encoder bank, etc.
- **Nested UI state structs** — e.g. `m_delayUIState`, `m_reverbUIState`, `m_sourceMixerUIState` for effect-specific visualization data.

### Wiring

1. Add the member to the appropriate UI state struct.
2. Ensure the DSP or nonagon code **writes** to it (e.g. in `Process()`, `WriteScopes()`, or a dedicated `PopulateUIState()` path).
3. The visualizer receives `TheNonagonSquiggleBoyInternal::UIState*` via `NonagonWrapper::GetUIState()`, which contains `m_squiggleBoyUIState`.

### NonagonWrapper accessors

If the data is a `ScopeWriter`, `MeterReader`, or similar that should be accessed by name, add a getter to `NonagonWrapper` in `JUCE/SmartGridOne/Source/NonagonWrapper.hpp`:

```cpp
ScopeWriter* GetMyScopeWriter()
{
    return &m_internal.m_uiState.m_squiggleBoyUIState.m_myScopeWriter;
}
```

Then use `m_nonagon->GetMyScopeWriter()` in the ForEach constructor.

## Checklist

- [ ] Component struct inherits `SmartGridOneMainVisualizerComponent`, implements `Draw()`
- [ ] Component placed in appropriate header; header included in `SmartGridOneVisualizerMain.hpp`
- [ ] `F(name, bank, block, ctor)` added in `ForEachSmartGridOneVisualizer.hpp`
- [ ] If data needed: added to `SquiggleBoyWithEncoderBank::UIState` (or nested struct)
- [ ] If data needed: DSP/nonagon writes to it; optionally add `NonagonWrapper` getter
- [ ] Build and verify visualizer appears when the encoder bank is selected

## Reference

- [Smart Grid Visualizers](docs/smart-grid-visualizers.md) — Architecture, layout, bank mapping
- [Visualization Pipeline](docs/ui-visualization-pipeline.md) — ScopeWriter, FFT, data flow
