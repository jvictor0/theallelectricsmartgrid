# Drawing Filter Response

## Overview

Filter, delay, reverb, and Partial Machine UI-state types share a common frequency-domain interface:

- `TransferFunction::FrequencyResponse(freq)`
- `TransferFunction::TransferFunctionValue(freq)`

The interface (`private/src/TransferFunction.hpp`) is implemented only by UI-state types, never by DSP classes. This lets analyzer components draw responses through a single rendering path.

## TransferFunction Provider

`private/src/TransferFunction.hpp` defines the abstraction used by UI-state providers.

**Design rule:** Only UI states implement `TransferFunction`. The underlying DSP classes (e.g. `FIR`, `TunedDecayComb`, `DampingFilter`) must never inherit from `TransferFunction`. DSP runs in the audio thread; visualization reads from UI-state snapshots populated on the control thread. Keeping transfer-function logic in UI state keeps the boundary clear and avoids coupling DSP to the visualization API.

Quad delay and quad reverb each contain four `DampingFilter::UIState` instances; the analyzer passes the specific damping filter for each speaker when drawing. The Partial Machine exposes four per-speaker transfer functions derived from its frequency-dependent reduction and panning model.

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
- `QuadAnalyserComponent` uses `DrawFrequencyResponse` for delay/reverb/partial-machine overlays, passing the per-speaker transfer-function provider
- `PhysicalModelingFrequencyResponseComponent` uses `DrawFrequencyResponse` for source-machine-specific response drawing

This keeps rendering logic centralized and makes new transfer-function overlays simpler to add.

## Tuned decay comb

`TunedDecayComb::UIState` snapshots each live branch's integer delay, allpass coefficient, low-pass coefficients `g`, `k`, and `gain`, per-sample radial factor, integer-delay gain, bypass flag, and signed feedback, plus the transition mix. For low-pass numerators `P0/P1` and complete signed return loops `R0/R1`, its complex response is `H=((1-m)*P0+m*P1)/(1-((1-m)*R0+m*R1))`. Both numerator and loop use the transformed low-pass; the loop also includes the actual integer delay and transformed allpass, rather than an ideal fractional delay. `P` is identity only at exact DECAY center. DAMP opens the actual prototype −3 dB cutoff exponentially from 2× fundamental at 0 to 128× at 1 (16× at the unchanged default midpoint), clamped to 0.49 of the processing rate; neither endpoint bypasses the filter. The output retains the host base-rate Nyquist limit. The variable-Q prototype has a unity peak at the fundamental, lower sub-fundamental gain, and tends to Butterworth as it opens. For `q=r*exp(-j*omega)`, each numerator is `gain*g*g*(1+q)^2/((1+k*g+g*g)+2*(g*g-1)*q+(1-k*g+g*g)*q*q)`, using its branch snapshots. The low-pass is algebraically the same response as the audio thread's double-precision TPT output. All radial memory terms are included. Fields are atomic; snapshots are suitable for visualization, not a synchronized transaction across all coefficients. During modulation this shows the frozen-coefficient response; the full trajectory is time-varying. Rendered complex responses cover settled and frozen-transition audio at both 48 and 192 kHz.
