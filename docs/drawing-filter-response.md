# Drawing Filter Response

## Overview

Filter, delay, and reverb UI-state types now share a common frequency-domain interface:

- `TransferFunction::FrequencyResponse(freq)`
- `TransferFunction::TransferFunctionValue(freq)`

This allows analyzer components to draw responses through a single rendering path.

## TransferFunction Provider

`private/src/TransferFunction.hpp` defines the abstraction used by UI-state providers.

**Design rule:** Only UI states implement `TransferFunction`. The underlying DSP classes (e.g. `FIR`, `CombFilterWithOnePole`, `DampingFilter`) must never inherit from `TransferFunction`. DSP runs in the audio thread; visualization reads from UI-state snapshots populated on the control thread. Keeping transfer-function logic in UI state keeps the boundary clear and avoids coupling DSP to the visualization API.

Quad delay and quad reverb each contain four `DampingFilter::UIState` instances; the analyzer passes the specific damping filter for each speaker when drawing.

## DampingFilter

`private/src/Filter.hpp` defines `DampingFilter`, a HP+LP filter with a `UIState` that implements `TransferFunction`. Each damping filter has its own `m_hpAlpha` and `m_lpAlpha`. Quad delay and quad reverb each contain four damping filters.

## Path Rendering API

`JUCE/SmartGridOne/Source/PathDrawer.hpp` adds:

- `DrawFrequencyResponse(g, colour, transferFunction, freqScale)`

Behavior:

- samples the provider on the current x-axis mode (`m_logX` or linear)
- converts linear amplitude to normalized dB with `AmpToDbNormalized`
- applies the expected display scaling (`/ 2`)
- draws the final response curve using `DrawPath`

## Integration Points

- `AnalyserComponent` uses `DrawFrequencyResponse` for voice filter overlays
- `QuadAnalyserComponent` uses `DrawFrequencyResponse` for delay/reverb filter overlays, passing the per-speaker damping filter
- `PhysicalModelingFrequencyResponseComponent` uses `DrawFrequencyResponse` for source-machine-specific response drawing

This keeps rendering logic centralized and makes new transfer-function overlays simpler to add.
