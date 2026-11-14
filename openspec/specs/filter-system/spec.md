# Filter System Specification

## Purpose
Defines the per-voice filter section of the Smart Grid One DSP engine: the two selectable filter machines (Ladder and SVF), their pitch-tracking cutoff law, sample-rate reduction between the low-pass and high-pass stages, and the UIState paradigm that separates stateful audio-thread DSP from the stateless transfer functions used by the JUCE filter-response visualizers. The filter section (`SquiggleBoyShape`) sits between the source machine output (source-machine-oscillators) and the amp section (voice-architecture, ahd-envelopes), and processes one engine sample (`SampleTimer::x_sampleRate`) per call for each track's three voices.

## Requirements

### Requirement: Per-Voice Filter Machine Selection
Each voice's filter section SHALL process the source machine's output with exactly one of two filter machines selected by `VoiceConfig::FilterMachine`: Ladder4Pole (the Ladder Filter Machine, `Ladder4PoleShape`) or SVF2Pole (the SVF Filter Machine, `SVF2PoleShape`).
Before either machine, the input passes a shared DC-blocking one-pole high-pass fixed at 5 Hz at the engine rate (`SampleTimer::HzToCycPerSamp(5)`). Both machines consume the same parameter set (cutoff factors, resonances, saturation gain, sample-rate-reducer factor), and the selection is evaluated every engine sample from the track's live voice config (see voice-architecture).

#### Scenario: Filter machine switch redirects processing
- **WHEN** a voice's `VoiceConfig::FilterMachine` changes from `Ladder4Pole` to `SVF2Pole`
- **THEN** subsequent samples are processed by the 2-pole SVF low-pass/high-pass pair instead of the ladder/4-pole-HP pair
- **AND** the per-voice sampled shape history contains `SVF2Pole` snapshots after the next eligible host sample

#### Scenario: DC offset is blocked ahead of the filter machines
- **WHEN** the source machine output carries a DC offset
- **THEN** the 5 Hz one-pole high-pass removes it before the selected filter machine processes the sample

### Requirement: Ladder Filter Machine
The Ladder Filter Machine SHALL apply a 4-pole transistor-ladder low-pass (`LadderFilterLP`), then the sample-rate reducer, then a 4-pole SVF high-pass (`LinearSVF4PoleHighPass`, two cascaded 2-pole SVF high-pass stages).
The ladder is four cascaded one-pole low-pass stages with a `TanhSaturator` after each stage and resonance feedback from the fourth stage's output. `SetCutoff` solves the per-stage alpha so the 4-stage cascade is -3 dB at the requested cutoff; `SetResonance` maps resonance to `feedback = resonance * 4`. Each sample, the effective feedback `m_kEff` is clamped to `0.9 / Cpi`, where `Cpi` is the fourth power of the per-stage Nyquist gain including the saturator's small-signal gain, guaranteeing stability; the output is scaled by `1 + m_kEff` to hold approximately unity DC gain as resonance rises. The saturation gain parameter sets the ladder saturators' input gain.

#### Scenario: Resonance feedback is clamped for stability
- **WHEN** the resonance parameter requests `feedback = resonance * 4` exceeding `0.9 / Cpi` for the current alpha and saturation gain
- **THEN** the effective feedback `m_kEff` is limited to `0.9 / Cpi` and the published UIState feedback coefficient equals the clamped value

#### Scenario: DC gain stays near unity as resonance increases
- **WHEN** resonance is raised while a low-frequency signal passes the ladder
- **THEN** the `1 + m_kEff` output compensation counters the resonance-induced passband drop, keeping DC gain approximately 1

### Requirement: SVF Filter Machine
The SVF Filter Machine SHALL apply a 2-pole SVF low-pass, then the sample-rate reducer, then a 2-pole SVF high-pass (both `LinearStateVariableFilter`, a TPT/trapezoidal structure) with three tanh saturation stages: before the low-pass, between the low-pass and the reducer, and after the high-pass.
All three stages use one normalized `TanhSaturator`, `Tanh(gain * x) / Tanh(gain)`, whose input gain is the saturation gain parameter (exponential, 0.25 to 5). The SVF computes `g = tan(pi * cutoff)` and `k = 1 / Q` with Q mapped exponentially from the resonance knob.

#### Scenario: Three saturation stages shape the signal path
- **WHEN** a sample is processed in SVF2Pole mode
- **THEN** the signal path is saturate -> SVF low-pass -> saturate -> sample-rate reducer -> SVF high-pass -> saturate, with every saturator at the current saturation gain

#### Scenario: Saturation gain drives drive intensity
- **WHEN** the saturation gain parameter is at its maximum (5)
- **THEN** all three tanh stages run at input gain 5, producing markedly more harmonic saturation than the minimum gain of 0.25

### Requirement: Pitch-Tracking Cutoff Frequencies
The filter section SHALL derive both cutoff frequencies from the voice base frequency in cycles per engine sample: `hpFreq = min(maxCutoff, vcoBaseFreq * hpCutoffFactor)` and `lpFreq = min(maxCutoff, pivot * (vcoBaseFreq / pivot)^0.707 * hpCutoffFactor * lpCutoffFactor)`, where `pivot = HzToCycPerSamp(261.6256)` and `maxCutoff = 0.5 / SampleTimer::x_oversample` keeps both cutoffs below the host Nyquist.
`hpCutoffFactor` is an exponential parameter spanning 0.25 to 256 and `lpCutoffFactor` spans 0.25 to 1024. The low-pass tracks 0.707 octaves per VCO octave around middle C (261.6256 Hz), while the high-pass cutoff factor multiplies both cutoffs linearly. The original LP/HP relationship is preserved at a middle-C VCO frequency for every high-pass cutoff factor. When the voice's source machine is Thru, the tracking base frequency is fixed at 80 Hz (`HzToCycPerSamp(80)`) instead of the voice pitch. Cutoff factors, resonances, and saturation gain are read from the encoders every engine sample without per-parameter slew; the base frequency arrives through the track's portamento one-pole.

#### Scenario: Cutoffs follow the played note
- **WHEN** a voice's base frequency doubles (up one octave) with cutoff factors unchanged
- **THEN** `hpFreq` doubles and `lpFreq` increases by a factor of `2^0.707`, each up to the clamp of `0.5 / x_oversample`

#### Scenario: The low-pass retains its original ratio at the pivot
- **WHEN** the voice's base frequency is middle C (261.6256 Hz)
- **THEN** the low-pass cutoff before clamping is `261.6256 Hz * hpCutoffFactor * lpCutoffFactor`

#### Scenario: High-pass cutoff factor moves both cutoffs together
- **WHEN** the high-pass cutoff factor doubles with voice base frequency and low-pass cutoff factor unchanged
- **THEN** both cutoffs double, each up to the clamp of `0.5 / x_oversample`

#### Scenario: Thru mode anchors the filter at 80 Hz
- **WHEN** the voice's source machine is Thru
- **THEN** the filter tracking frequency is 80 Hz in cycles per engine sample regardless of the voice's pitch input, so external audio is filtered against a fixed 80 Hz anchor

### Requirement: Sample-Rate Reduction Before the High-Pass
Each filter machine SHALL apply a `SampleRateReducer` to its low-pass output and feed the reduced signal to its high-pass, with reduction frequency `vcoBaseFreq * sampleRateReducerFactor` in cycles per engine sample, where the factor is an exponential parameter spanning 1 to 2048. Channels whose reduction frequency reaches one cycle per engine sample bypass the reducer.
Because the reduction frequency is proportional to the voice base frequency, the digital-degradation character tracks pitch; at the maximum factor the reduction frequency is far above the audible band and the stage is effectively transparent.

#### Scenario: Sample-rate reduction tracks pitch
- **WHEN** the reduction factor is low and a voice plays a note
- **THEN** the low-pass output is resampled at a rate proportional to the note's base frequency, producing pitch-tracked aliasing texture

#### Scenario: The high-pass removes bass created by the reducer
- **WHEN** the high-pass is engaged and the reducer folds a tone to aliases below the high-pass cutoff, at any reducer setting
- **THEN** the high-pass attenuates those aliases, so raising the reduction does not bypass the chosen low-frequency attenuation

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

### Requirement: Sampled Filter Coefficient Publication
The one-pole LP/HP, base-width, ladder, SVF, shape, crossover, FIR, physical-model/comb, source-input, and mastering response providers SHALL use `TransferFunctionGeneric<Snapshot>` with coefficient-only snapshots retaining whole DSP channel vectors. `SampledSnapshotWriter` SHALL make its sampling decision on the audio thread at host-sample boundaries after coefficient updates. It SHALL publish an entire composite response in one capture, including both base-width stages and all four quad damping lanes. The history capacity, number of recent samples, percentile bounds, and sampling divisor SHALL be configurable. Standalone internal smoothing filters SHALL not allocate response histories unless an owner creates a UI state for them.

#### Scenario: Composite samples preserve correlated modulation
- **WHEN** both stages of a base-width or shape filter are modulated
- **THEN** one sampling decision captures both stages' coefficients from the same audio instant
- **AND** the UI composes each captured response before computing order statistics

#### Scenario: Frozen response math is preserved
- **WHEN** a captured filter state is evaluated
- **THEN** its complex response follows the existing frozen linear model, including the ladder's effective feedback
- **AND** virtual magnitude response defaults to complex magnitude, with equivalent direct magnitude overrides where cheaper
- **AND** the display does not measure saturation or substitute a different linearization

### Requirement: Per-Voice Filter Visualization State
The UI SHALL hold one three-lane sampled shape response per track. Each snapshot SHALL identify the active filter machine and capture the entire trio's LP/HP coefficient vectors. Per-voice queries SHALL select a lane from the shared evaluated batch. A machine change SHALL invalidate the previous machine's history and seed the replacement at the next eligible host sample. The physical-model source SHALL likewise discard stale history when its source selection changes.

#### Scenario: Visualizer follows a machine switch
- **WHEN** a voice changes from SVF2Pole to Ladder4Pole
- **THEN** its next sampled response contains the ladder/4-pole-HP pair
- **AND** subsequent percentile calculations exclude the preceding machine's samples
