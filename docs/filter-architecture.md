# Filter Architecture & DSP Layout

This document describes the architectural layout and design of the various filter structures within the DSP engine, illustrating how parameters, UI state, and signal processing operate independently while staying in sync.

## Overview

A core design principle of our filter classes is separating the filter's stateful signal processing from its stateless frequency response analysis. This is achieved by designing each filter with the following components:

1. **The DSP Class**: e.g., `LinearStateVariableFilter` or `LadderFilterLP`. This class contains the state for real-time sample processing, including integrators and previous sample buffers.
2. **Transfer Function**: A stateless static function `TransferFunction(args, freq)` that calculates the complex frequency response at a specific normalized frequency.
3. **Frequency Response**: A stateless static function `FrequencyResponse(args, freq)` that calls `std::abs()` on the output of the `TransferFunction` to return the magnitude response.
4. **UI State Struct**: A nested struct (`UIState`) housing atomic variables representing the filter's computed coefficients (e.g., `m_g` and `m_k` for SVF, or `m_alpha` and `m_feedback` for Ladder).

## Static Transfer and Frequency Functions

Each filter exposes static methods for evaluating its response at any normalized frequency. For example, in the `LinearStateVariableFilter`:

```cpp
static std::complex<float> LowPassTransferFunction(float g, float k, float freq)
{
    // ... complex math using bilinear transform ...
}

static float LowPassFrequencyResponse(float g, float k, float freq)
{
    return std::abs(LowPassTransferFunction(g, k, freq));
}
```

The same pattern applies to `LadderFilterLP`, where it simulates four cascaded one-pole stages with a feedback loop:

```cpp
static std::complex<float> TransferFunction(float alpha, float feedback, float freq)
{
    // ... four-stage complex response with feedback ...
}

static float FrequencyResponse(float alpha, float feedback, float freq)
{
    return std::abs(TransferFunction(alpha, feedback, freq));
}
```

Returning a `std::complex<float>` from the `TransferFunction` makes it straightforward to cascade multiple filters mathematically or compute phase responses if needed, while `FrequencyResponse` provides a direct magnitude for visualizers.

## The UIState Paradigm

The DSP code runs on the real-time audio thread, and UI rendering (like filter curve visualizers) runs on the message thread. It is unsafe for the UI thread to read directly from the DSP filter classes.

To resolve this, every filter implements a `UIState` struct. This struct holds *only* the computed coefficients necessary to perfectly reconstruct the filter's frequency response without any audio state (history buffers, integrators).

For example, `LadderFilterLP::UIState`:

```cpp
struct UIState
{
    std::atomic<float> m_alpha;
    std::atomic<float> m_feedback;

    float FrequencyResponse(float freq)
    {
        return LadderFilterLP::FrequencyResponse(m_alpha.load(), m_feedback.load(), freq);
    }
    
    // ...
};
```

### Operation Flow

1. **Coefficient Calculation**: When a user changes a parameter (like Cutoff or Resonance), the filter calculates its new internal coefficients (e.g., converting Cutoff to `m_alpha` or `m_g`).
2. **State Population**: The DSP thread periodically calls `filter.PopulateUIState(&uiState)`, which thread-safely stores these computed coefficients into the `atomic` variables within `UIState`.
3. **UI Rendering**: The visualizer holds a reference to the `UIState`, completely detached from the DSP filter. For every pixel column, it calls `uiState.FrequencyResponse(freq)`, plotting the curve using the stateless static functions.

This architecture ensures high-performance DSP while allowing pixel-perfect visual representations of complex cascading non-linear filter models.
