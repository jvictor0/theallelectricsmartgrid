# Filter System Specification

## Purpose
Defines the per-voice filter section of the Smart Grid One DSP engine: the two selectable filter machines (Ladder and SVF), their pitch-tracking cutoff law, post-filter sample-rate reduction, and the UIState paradigm that separates stateful audio-thread DSP from the stateless transfer functions used by the JUCE filter-response visualizers. The filter section sits between the source machine output (source-machine-oscillators) and the amp section (voice-architecture, ahd-envelopes), and runs in the voice's 4x-oversampled domain.

## Requirements

### Requirement: Per-Voice Filter Machine Selection
Each voice's filter section SHALL process the source machine's oversampled output with exactly one of two filter machines selected by `VoiceConfig::FilterMachine`: Ladder4Pole (the Ladder Filter Machine) or SVF2Pole (the SVF Filter Machine).
Before either machine, the input passes a shared DC-blocking one-pole high-pass fixed at 5 Hz at the oversampled rate (5 / 192000). Both machines consume the same parameter set (cutoff factors, resonances, saturation gain), and the selection is evaluated per oversampled sample from the live voice config.

#### Scenario: Filter machine switch redirects processing
- **WHEN** a voice's `VoiceConfig::FilterMachine` changes from `Ladder4Pole` to `SVF2Pole`
- **THEN** subsequent samples are processed by the 2-pole SVF low-pass/high-pass pair instead of the ladder/4-pole-HP pair
- **AND** the per-voice `VoiceFilterUIState::m_filterMachine` atomic reflects `SVF2Pole` after the next UI state population

#### Scenario: DC offset is blocked ahead of the filter machines
- **WHEN** the source machine output carries a DC offset
- **THEN** the 5 Hz one-pole high-pass removes it before the selected filter machine processes the sample

### Requirement: Ladder Filter Machine
The Ladder Filter Machine SHALL apply a 4-pole transistor-ladder low-pass (`LadderFilterLP`) followed by a 4-pole SVF high-pass (`LinearSVF4PoleHighPass`, two cascaded 2-pole SVF high-pass stages).
The ladder is four cascaded one-pole low-pass stages with a `TanhSaturator` after each stage and resonance feedback from the fourth stage's output. `SetCutoff` solves the per-stage alpha so the 4-stage cascade is -3 dB at the requested cutoff; `SetResonance` maps resonance to `feedback = resonance * 4`. Each sample, the effective feedback `m_kEff` is clamped to `0.9 / Cpi`, where `Cpi` is the fourth power of the per-stage Nyquist gain including the saturator's small-signal gain, guaranteeing stability; the output is scaled by `1 + m_kEff` to hold approximately unity DC gain as resonance rises. The saturation gain parameter sets the ladder saturators' input gain.

#### Scenario: Resonance feedback is clamped for stability
- **WHEN** the resonance parameter requests `feedback = resonance * 4` exceeding `0.9 / Cpi` for the current alpha and saturation gain
- **THEN** the effective feedback `m_kEff` is limited to `0.9 / Cpi` and the published UIState feedback coefficient equals the clamped value

#### Scenario: DC gain stays near unity as resonance increases
- **WHEN** resonance is raised while a low-frequency signal passes the ladder
- **THEN** the `1 + m_kEff` output compensation counters the resonance-induced passband drop, keeping DC gain approximately 1

### Requirement: SVF Filter Machine
The SVF Filter Machine SHALL apply a 2-pole SVF low-pass followed by a 2-pole SVF high-pass (both `LinearStateVariableFilter`, a TPT/trapezoidal structure) with three tanh saturation stages: before the low-pass, between the two filters, and after the high-pass.
All three stages use one `TanhSaturator` whose input gain is the slewed saturation gain parameter (exponential, 0.25 to 5). The SVF computes `g = tan(pi * cutoff)` and `k = 1 / Q` with Q mapped exponentially from the resonance knob.

#### Scenario: Three saturation stages shape the signal path
- **WHEN** a sample is processed in SVF2Pole mode
- **THEN** the signal path is saturate -> SVF low-pass -> saturate -> SVF high-pass -> saturate, with every saturator at the current saturation gain

#### Scenario: Saturation gain drives drive intensity
- **WHEN** the saturation gain parameter is at its maximum (5)
- **THEN** all three tanh stages run at input gain 5, producing markedly more harmonic saturation than the minimum gain of 0.25

### Requirement: Pitch-Tracking Cutoff Frequencies
The filter section SHALL derive both cutoff frequencies from the voice base frequency: `hpFreq = min(0.5 / 4, vcoBaseFreq * hpCutoffFactor / 4)` and `lpFreq = min(0.5 / 4, vcoBaseFreq * hpCutoffFactor * lpCutoffFactor / 4)`, where division by 4 normalizes to the oversampled rate.
`hpCutoffFactor` is an exponential parameter spanning 0.25 to 256 and `lpCutoffFactor` spans 0.25 to 1024; because the low-pass cutoff multiplies the high-pass cutoff by its own factor, the low-pass is anchored relative to the high-pass and both track played pitch. When the voice's source machine is Thru, the tracking base frequency is fixed at 80 Hz (80 / 48000) instead of the voice pitch. Base frequency, cutoff factors, resonances, and saturation gain are all slewed per oversampled sample.

#### Scenario: Cutoffs follow the played note
- **WHEN** a voice's base frequency doubles (up one octave) with cutoff factors unchanged
- **THEN** both `hpFreq` and `lpFreq` double, up to the oversampled-domain clamp of 0.5 / 4

#### Scenario: Thru mode anchors the filter at 80 Hz
- **WHEN** the voice's source machine is Thru
- **THEN** the filter tracking frequency is 80 / 48000 regardless of the voice's pitch input, so external audio is filtered against a fixed 80 Hz anchor

### Requirement: Post-Filter Sample-Rate Reduction
The filter section SHALL apply a `SampleRateReducer` to the selected filter machine's output, with reduction frequency `vcoBaseFreq * sampleRateReducerFactor / 4`, where the factor is an exponential parameter spanning 1 to 2048.
Because the reduction frequency is proportional to the voice base frequency, the digital-degradation character tracks pitch; at the maximum factor the reduction frequency is far above the audible band and the stage is effectively transparent.

#### Scenario: Sample-rate reduction tracks pitch
- **WHEN** the reduction factor is low and a voice plays a note
- **THEN** the filter output is resampled at a rate proportional to the note's base frequency, producing pitch-tracked aliasing texture

### Requirement: Stateless Transfer Functions
Each filter class SHALL expose static, stateless `TransferFunction` methods returning `std::complex<float>` and `FrequencyResponse` methods returning the magnitude `std::abs(TransferFunction(...))`, parameterized only by computed coefficients and a normalized frequency.
`LadderFilterLP::TransferFunction(alpha, feedback, freq)` models four cascaded one-pole stages `H(z) = alpha / (1 - (1 - alpha) z^-1)` raised to the fourth power, with one-sample-delayed feedback `H^4 / (1 + feedback * z^-1 * H^4)` and `(1 + feedback)` gain compensation. `LinearStateVariableFilter` exposes `LowPassTransferFunction`, `HighPassTransferFunction`, and `BandPassTransferFunction` in `(g, k, freq)` via the bilinear transform `s = (1 - z^-1) / ((1 + z^-1) * g)` over the denominator `s^2 + k s + 1`. Returning complex values allows cascades to be composed by multiplication and phase response to be derived.

#### Scenario: Ladder response is unity at DC
- **WHEN** `LadderFilterLP::FrequencyResponse(alpha, feedback, 0)` is evaluated for any alpha in (0, 1] and feedback >= 0
- **THEN** it returns 1.0, because the four-stage cascade is unity at DC and the `(1 + feedback)` compensation cancels the feedback denominator

#### Scenario: SVF low-pass magnitude at the cutoff equals Q
- **WHEN** `LinearStateVariableFilter::LowPassFrequencyResponse(g, k, fc)` is evaluated at the cutoff frequency `fc = atan(g) / pi`
- **THEN** the magnitude equals `1 / k` (the filter's Q)
- **AND** `HighPassFrequencyResponse(g, k, 0)` evaluates to 0

### Requirement: Filter UIState Coefficient Publication
Each stateful filter SHALL own a nested `UIState` struct holding only atomic copies of its computed coefficients, populated thread-safely by `PopulateUIState` from the DSP side: `LadderFilterLP::UIState` stores `m_alpha` (per-stage alpha) and `m_feedback` (the clamped effective feedback `m_kEff`); `LinearStateVariableFilter::UIState` and `LinearSVF4PoleHighPass::UIState` store `m_g` and `m_k`.
A `UIState` contains no audio state (no integrators or history buffers); its member `FrequencyResponse`/`TransferFunction` helpers delegate to the static stateless functions using the loaded atomics, so the UI thread reconstructs the exact response without reading DSP objects.

#### Scenario: Population copies live coefficients atomically
- **WHEN** `PopulateUIState` runs after a cutoff or resonance change has recomputed coefficients
- **THEN** the UIState atomics hold the new coefficient values, and a concurrent UI-thread `FrequencyResponse(freq)` call uses only those atomics

#### Scenario: UIState response matches the static function
- **WHEN** the UI thread calls `LadderFilterLP::UIState::FrequencyResponse(f)`
- **THEN** the result equals `LadderFilterLP::FrequencyResponse(m_alpha.load(), m_feedback.load(), f)`

### Requirement: Per-Voice Filter Visualization State
`SquiggleBoy::UIState` SHALL hold one `VoiceFilterUIState` per voice (9 total), each implementing the `TransferFunction` interface over the UIStates of all four component filters (ladder LP, 4-pole SVF HP, SVF LP, SVF HP) plus an atomic copy of the voice's filter machine.
`PopulateVoiceFilterUIState` populates all four component UIStates and the machine atomic regardless of which machine is active. `FrequencyResponse(freq)` dispatches on the stored machine: in Ladder4Pole mode it returns the ladder low-pass magnitude multiplied by the 4-pole SVF high-pass magnitude; in SVF2Pole mode it returns the SVF low-pass magnitude multiplied by the SVF high-pass magnitude. `TransferFunctionValue(freq)` returns the corresponding complex product. Filter visualizers hold references to these structs and evaluate `FrequencyResponse(freq)` per pixel column, fully decoupled from the audio thread.

#### Scenario: Composite response is the product of the active pair
- **WHEN** a voice's `VoiceFilterUIState` has `m_filterMachine == Ladder4Pole` and `FrequencyResponse(f)` is called
- **THEN** it returns `LadderFilterLP::FrequencyResponse(alpha, feedback, f) * LinearSVF4PoleHighPass::FrequencyResponse(g, k, f)` using the stored atomics

#### Scenario: Visualizer follows a machine switch
- **WHEN** the performer switches a voice from SVF2Pole to Ladder4Pole and the next `PopulateVoiceFilterUIState` runs
- **THEN** subsequent `FrequencyResponse` calls dispatch through the ladder/4-pole-HP branch and the drawn curve changes to the 4-pole low-pass times high-pass shape without the visualizer touching any DSP filter object
