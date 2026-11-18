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
The Ladder Filter Machine SHALL apply a 4-pole transistor-ladder low-pass (`LadderFilterLP`), then the sample-rate reducer, then a 4-pole SVF high-pass (`TPTSVF4PoleHighPassFilterGeneric`, two cascaded 2-pole SVF high-pass stages).
The ladder is four cascaded one-pole low-pass stages with normalized `TanhSaturator` after each stage. Feedback SHALL use the previous sample's **unsaturated** fourth-stage state. `SetCutoffAndResonance` SHALL use the individual-stage half-power corner, giving -12.0412 dB for the unit-gain, zero-feedback cascade, and map resonance to `feedback = resonance * 3.9`. The ladder cutoff SHALL be capped at engine Nyquist (0.5 cycles per engine sample), allowing it above host Nyquist. Alpha, detector center frequency and critical feedback SHALL be derived from shared cutoff intermediates, without recovering detector tuning from the rounded alpha.

Parameter setters SHALL cache `kSafe = 0.9 / (Cpi + 1e-8)` and `baseFeedback = min(feedback, kSafe)`, where `Cpi` is the fourth power of the per-stage Nyquist gain including the saturator's small-signal gain. This preserves the existing Nyquist heuristic, not a general nonlinear stability guarantee. The detector centers SHALL follow the nominal oscillation frequency of the loop including its one-sample delay, capped at 0.499 by the bandpass SVFs. Envelope smoothing SHALL remain tied to the cascade half-power frequency for equivalent damping timing. Each sample SHALL reduce base feedback using the previous input and feedback detector powers, then advance the ladder and detectors in that order. The output SHALL be scaled by `1 + m_kEff` to counter resonance-induced passband drop. The normalized saturator SHALL retain the clamped rational `Tanh` implementation. Setters SHALL preserve stage and detector histories, and saturation-gain changes SHALL refresh the parameter-only feedback terms.

#### Scenario: Feedback respects the cached guard and adaptive damping
- **WHEN** resonance requests feedback above `kSafe`, or the feedback detector exceeds its input-referenced power
- **THEN** `m_kEff` is no greater than `min(resonance * 3.9, kSafe)` and the published UIState uses that sample's effective feedback coefficient

#### Scenario: Coefficient updates preserve the running filter
- **WHEN** cutoff, resonance or saturation gain changes, in either setter order
- **THEN** all dependent feedback coefficients are refreshed without clearing the four stage states or either detector history

#### Scenario: DC gain stays near unity as resonance increases
- **WHEN** resonance is raised while a low-frequency signal passes the ladder
- **THEN** the `1 + m_kEff` output compensation counters the resonance-induced passband drop, keeping DC gain approximately 1

### Requirement: SVF Filter Machine
The SVF Filter Machine SHALL apply a 2-pole SVF low-pass, then the sample-rate reducer, then a 2-pole SVF high-pass (generated `TPTSVFLowPassFilterGeneric` and `TPTSVFHighPassFilterGeneric`, with a TPT/trapezoidal structure) with three tanh saturation stages: before the low-pass, between the low-pass and the reducer, and after the high-pass.
All three stages use one normalized `TanhSaturator`, `Tanh(gain * x) / Tanh(gain)`, whose input gain is the saturation gain parameter (exponential, 0.25 to 5). The SVF computes `g = tan(pi * cutoff)` and `k = 1 / Q` with Q mapped exponentially from the resonance knob.

#### Scenario: Three saturation stages shape the signal path
- **WHEN** a sample is processed in SVF2Pole mode
- **THEN** the signal path is saturate -> SVF low-pass -> saturate -> sample-rate reducer -> SVF high-pass -> saturate, with every saturator at the current saturation gain

#### Scenario: Saturation gain drives drive intensity
- **WHEN** the saturation gain parameter is at its maximum (5)
- **THEN** all three tanh stages run at input gain 5, producing markedly more harmonic saturation than the minimum gain of 0.25

### Requirement: Pitch-Tracking Cutoff Frequencies
The filter section SHALL derive both cutoff frequencies from the voice base frequency in cycles per engine sample: `hpFreq = min(maxCutoff, vcoBaseFreq * hpCutoffFactor)` and `lpFreq = min(lpMaxCutoff, pivot * (vcoBaseFreq / pivot)^0.707 * hpCutoffFactor * lpCutoffFactor)`, where `pivot = HzToCycPerSamp(261.6256)`, `maxCutoff = 0.5 / SampleTimer::x_oversample`, and `lpMaxCutoff` is 0.5 for the ladder and `maxCutoff` for the SVF. The ladder LP may open above host Nyquist; the high-pass and SVF low-pass retain their host-Nyquist caps.
`hpCutoffFactor` is an exponential parameter spanning 0.25 to 256. The SVF `lpCutoffFactor` spans 0.25 to 1024. The ladder SHALL retain that 12-octave span with both endpoints multiplied by `1 / sqrt(2^(1/4)-1)`, approximately 0.574740 to 2354.134, to preserve low-frequency tuning approximately when changing from cascade half-power to stage-corner cutoff. The low-pass tracks 0.707 octaves per VCO octave around middle C (261.6256 Hz), while the high-pass cutoff factor multiplies both cutoffs linearly. The 0.707-octave LP pitch law and the common HP-factor multiplier SHALL be preserved; the LP cutoff is not redefined as a multiple of the clamped HP cutoff. When the voice's source machine is Thru, the tracking base frequency is fixed at 80 Hz (`HzToCycPerSamp(80)`) instead of the voice pitch. Cutoff factors, resonances, and saturation gain are read from the encoders every engine sample without per-parameter slew; the base frequency arrives through the track's portamento one-pole.

#### Scenario: Cutoffs follow the played note
- **WHEN** a voice's base frequency doubles (up one octave) with cutoff factors unchanged
- **THEN** `hpFreq` doubles and `lpFreq` increases by a factor of `2^0.707`, each up to its respective cutoff clamp

#### Scenario: Low-pass pitch tracking is anchored at the pivot
- **WHEN** the voice's base frequency is middle C (261.6256 Hz)
- **THEN** the low-pass cutoff before clamping is `261.6256 Hz * hpCutoffFactor * lpCutoffFactor`

#### Scenario: High-pass cutoff factor moves both cutoffs together
- **WHEN** the high-pass cutoff factor doubles with voice base frequency and low-pass cutoff factor unchanged
- **THEN** both cutoffs double, each up to its respective cutoff clamp

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

### Requirement: Stateless Power Responses
Generated filters SHALL expose static magnitude-squared functions taking coefficient snapshots and prepared frequency vectors. The SVF app wrappers SHALL retain natural cutoff in cycles per engine sample, clamped at 0.499, and `Q = 0.5 * 11^resonance`. The four-pole high-pass SHALL apply that cutoff and Q independently to both sections. The ladder's band-pass envelope followers SHALL use generated SVF sections at Q = 1.
Composite UI snapshots SHALL multiply section power responses. Physical-modeling snapshots SHALL use the generated morph response, including its cross terms, then multiply by the comb power response. Saturation, envelope modulation and sample-rate reduction SHALL remain outside these frozen linear response plots. No executable complex response is required from generated filters.
The handwritten ladder low-pass SHALL retain its frozen response model and effective feedback coefficient.

#### Scenario: SVF cutoff convention is preserved
- **WHEN** a generated LP or HP section is evaluated at its natural cutoff
- **THEN** its power equals Q squared in the mathematical model
- **AND** at zero resonance Q is 0.5, giving power 0.25 (-6.02 dB)
- **AND** the four-pole HP cascade has power Q to the fourth (-12.04 dB at zero resonance)

#### Scenario: Morph retains its existing control curve
- **WHEN** the physical-modeling morph moves from 0 to 1
- **THEN** the generated filter uses positive quarter-circle weights from LP to BP to HP, with BP at 0.5 and the blend clamped to [0, 1]
- **AND** its snapshot captures the same weights used by the audio step

### Requirement: Sampled Filter Coefficient Publication
The one-pole LP/HP, base-width, ladder, SVF, shape, crossover, FIR, physical-model/comb, source-input, and mastering response providers SHALL use `TransferFunctionGeneric<Snapshot>` with coefficient-only snapshots retaining whole DSP channel vectors. `SampledSnapshotWriter` SHALL make its sampling decision on the audio thread at host-sample boundaries after coefficient updates. It SHALL publish an entire composite response in one capture, including both base-width stages and all four quad damping lanes. The history capacity, number of recent samples, percentile bounds, and sampling divisor SHALL be configurable. Standalone internal smoothing filters SHALL not allocate response histories unless an owner creates a UI state for them.

#### Scenario: Composite samples preserve correlated modulation
- **WHEN** both stages of a base-width or shape filter are modulated
- **THEN** one sampling decision captures both stages' coefficients from the same audio instant
- **AND** the UI composes each captured response before computing order statistics

#### Scenario: Frozen response math is preserved
- **WHEN** a captured filter state is evaluated
- **THEN** its power response follows the existing frozen linear model, including the ladder's effective feedback
- **AND** generated children use their emitted magnitude-squared kernels; handwritten children may retain equivalent complex-derived power responses
- **AND** the display does not measure saturation or substitute a different linearization

### Requirement: Per-Voice Filter Visualization State
The UI SHALL hold one three-lane sampled shape response per track. Each snapshot SHALL identify the active filter machine and capture the entire trio's LP/HP coefficient vectors. Per-voice queries SHALL select a lane from the shared evaluated batch. A machine change SHALL invalidate the previous machine's history and seed the replacement at the next eligible host sample. The physical-model source SHALL likewise discard stale history when its source selection changes.

#### Scenario: Visualizer follows a machine switch
- **WHEN** a voice changes from SVF2Pole to Ladder4Pole
- **THEN** its next sampled response contains the ladder/4-pole-HP pair
- **AND** subsequent percentile calculations exclude the preceding machine's samples
