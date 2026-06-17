# Voice Architecture Specification

## Purpose
The DSP engine (`SquiggleBoy` in `private/src/SquiggleBoy.hpp`) hosts nine independent synthesizer voices (`SquiggleBoyVoice`), each a complete chain of source machine, filter section, amp section, pan section, and sub-oscillator. Voices are organized into three trios that act as tracks sharing base parameter values with per-voice modulation. Each voice processes its source and filter at 4x internal oversampling, downsamples, applies AHD-driven amplification (see ahd-envelopes), and pans into a quadraphonic field. Triggers and envelope timing arrive from the multi-phasor-gate; source detail lives in source-machine-oscillators and filter detail in filter-system.

## Requirements

### Requirement: Fixed Nine-Voice Polyphony
The system SHALL provide exactly nine voices (`SquiggleBoy::x_numVoices = 9`), each with its own persistent `SquiggleBoyVoice` instance and `SquiggleBoyVoice::Input` state. There is no voice stealing or dynamic voice allocation: voice index i is permanently bound to sequencer voice i, and every voice is processed every sample.

#### Scenario: All voices process every sample
- **WHEN** `SquiggleBoy::ProcessSample` runs for one audio sample
- **THEN** `m_voices[i].Processs(m_state[i])` executes for all i in 0..8 and each voice's output, sub output, and pan X/Y are written into the mixer input slots 0..8
- **AND** a voice with no active envelope contributes silence rather than being deallocated

### Requirement: Trio Track Grouping
The system SHALL group the nine voices into three tracks of three voices each (`x_numTracks = 3`, `x_voicesPerTrack = 3`; voices 0-2, 3-5, 6-8). Voices in a track share base parameter values from the encoder banks and share per-track resources (ganged random LFO gangs of size 3, shared wavetable generation per track), while each voice retains independently modulated values and independent DSP state.

#### Scenario: Track-level LFO ganging
- **WHEN** the ganged random LFOs are configured at startup
- **THEN** each `ManyGangedRandomLFO` input has `m_gangSize = 3` and `m_numGangs = 3`, matching the three-voice tracks
- **AND** wavetable generators push the same left/right wavetables to all three voices of a track

### Requirement: Per-Voice Signal Chain
Each voice SHALL process audio through the chain: source machine, then filter section, then downsample, then amp section, then pan section. `ProcessUBlock` runs the source and filter at the oversampled rate once per control frame; `Processs` then per sample feeds the downsampled filter output and the sub-oscillator sample into `AmpSection::Process` and drives `PanSection::Process`, which maps a Lissajous LFO to pan coordinates `m_outputX`/`m_outputY` in [0, 1] for the quadraphonic mixer.

#### Scenario: Signal ordering is observable on the scopes
- **WHEN** a voice plays a note
- **THEN** the `AudioScopes::PostFilter` scope channel shows the filtered pre-amp signal while `AudioScopes::PostAmp` shows the same signal multiplied by the amp envelope
- **AND** the voice's pan coordinates are published to the UI via `UIState::SetPos(i, x, y)` as atomically readable X/Y positions

### Requirement: Per-Voice Sub-Oscillator
Each voice SHALL include a per-voice sub-oscillator that produces audio one octave below the voice's base frequency, gated by the main amp envelope, and routed to the mixer's mono input lane (`m_monoIn[i]`) separately from the main quad-panned output (generation and saturation behavior is specified in source-machine-oscillators). The sub is enabled per note by the sub-trigger input, sampled by the amp section each time the amp envelope is triggered.

#### Scenario: Sub follows the main envelope
- **WHEN** a voice is triggered with `m_subTrig` true and the main AHD envelope rises and decays
- **THEN** the sub signal present on the mono lane `m_monoIn[i]` follows the amp envelope, reaching zero when the envelope returns to zero
- **AND** a subsequent trigger with `m_subTrig` false disables the sub for that note

### Requirement: Internal 4x Oversampling
Each voice SHALL run its source machine and filter section at 4x the base sample rate (`x_oversample = 4`, e.g. 192 kHz for a 48 kHz session), processing micro-blocks of `x_uBlockSize = SampleTimer::x_controlFrameRate * 4` oversampled samples per control frame, and SHALL downsample the filter output back to the base rate (`Downsampler m_downsampler`) before the amp section. Filter cutoffs are scaled by `1/x_oversample` for the oversampled rate, and the filter's DC blocker is tuned at 5 Hz relative to 192 kHz.

#### Scenario: Oversampled micro-block per control frame
- **WHEN** `SampleTimer::IsControlFrame()` is true for a sample
- **THEN** the voice processes one micro-block of `x_uBlockSize` oversampled samples through source and filter and stores `SampleTimer::x_controlFrameRate` downsampled samples in `m_uBlockFilterOut`
- **AND** the amp section consumes one downsampled sample per base-rate sample via `SampleTimer::GetUBlockIndex()`

### Requirement: Parameter Slewing Per Control Frame
The system SHALL update voice parameter targets once per control frame and slew them per oversampled sample to avoid zipper noise. The filter section maintains `ParamSlew` instances (constructed with the oversample factor) for VCO base frequency, low-pass and high-pass cutoff and resonance, saturation gain, and sample-rate-reducer frequency; `Update` sets the target at micro-block start and `Process` interpolates each oversampled sample.

#### Scenario: Cutoff change is interpolated
- **WHEN** the low-pass cutoff factor target changes between two control frames
- **THEN** the effective cutoff applied to the filter moves smoothly across the `x_uBlockSize` oversampled samples of the micro-block rather than stepping once
- **AND** the same slewing applies to resonance and saturation gain changes

### Requirement: Voice Configuration
Each voice SHALL be configured by a `SquiggleBoyVoiceConfig` carrying: the source machine selection (`DualWaveShapingVCO`, `PhysicalModeling`, `Thru`, or `Sample`), the filter machine selection (`Ladder4Pole` or `SVF2Pole`), a source assignment, and an `AudioBufferBank` pointer used by the Sample source machine. The source assignment SHALL be able to represent Silent, Source Left for a source index, and Source Right for a source index. Defaults are `DualWaveShapingVCO`, `Ladder4Pole`, source 0 Left for compatibility, and a null buffer bank. The filter and amp sections read the active configuration through a shared pointer in the voice input, selecting the DSP path per micro-block.

#### Scenario: Filter machine selection switches the DSP path
- **WHEN** a voice's config has `m_filterMachine = Ladder4Pole`
- **THEN** the filter section processes through the 4-pole ladder low-pass followed by the 4-pole SVF high-pass
- **AND** with `m_filterMachine = SVF2Pole` it instead processes through the saturated 2-pole SVF low-pass/high-pass chain

#### Scenario: Thru source pins the filter base frequency
- **WHEN** a voice's config has `m_sourceMachine = Thru`
- **THEN** the filter section overrides the VCO base frequency with 80 Hz normalized to the sample rate so external audio is filtered against a fixed reference

#### Scenario: Silent source assignment is representable
- **WHEN** a trio source selection leaves a voice unassigned
- **THEN** that voice's config can store a Silent source assignment for the Thru source machine to render as silence

### Requirement: Trio Source Channel Assignment
For each trio, the system SHALL derive voice source assignments from selected source channels without infill. A selected Mono source SHALL contribute one Source Left assignment. A selected Stereo source SHALL contribute two assignments, Source Left followed by Source Right. The first assignment SHALL be applied to the first voice in the trio, the second assignment to the second voice, and the third assignment to the third voice. If fewer than three assignments exist, all remaining voices in the trio SHALL receive Silent source assignments.

The selected source-channel count for a trio SHALL NOT exceed three. When selecting a source or changing a selected source from Mono to Stereo would exceed three selected channels, the system SHALL unselect sources or revert the width change deterministically so the selected source-channel count remains at most three, then rerun propagation for affected trios.

#### Scenario: Stereo source consumes two trio voices
- **WHEN** only source 1 is selected for a trio and source 1 is configured as Stereo
- **THEN** the trio's first voice is assigned source 1 Left
- **AND** the trio's second voice is assigned source 1 Right
- **AND** the trio's third voice is assigned Silent

#### Scenario: Mono source leaves the remaining voices silent
- **WHEN** only source 2 is selected for a trio and source 2 is configured as Mono
- **THEN** the trio's first voice is assigned source 2 Left
- **AND** the trio's second and third voices are assigned Silent

#### Scenario: Multiple selections fill voices without repetition
- **WHEN** source 0 is selected as Mono and source 1 is selected as Stereo for a trio
- **THEN** the trio's voices are assigned source 0 Left, source 1 Left, and source 1 Right in source-selection order
- **AND** no selected source is repeated to fill additional voices

#### Scenario: Width change enforces the three-channel limit
- **WHEN** three Mono sources are selected for a trio and one selected source is changed to Stereo
- **THEN** source selection is adjusted or the width change is reverted so no more than three source channels remain selected
- **AND** the trio's voice assignments are propagated from the adjusted selected source channels

### Requirement: Dual Envelopes in the Amp Section
Each voice's amp section SHALL host two phase-driven AHD envelopes sharing the same `AHDControl` trigger timing: the main amp envelope (`m_ahd`, amplitude polarity false) whose output is shaped by an exponential curve and multiplied into the filter output, and a modulation envelope (`m_modulationAHD`, amplitude polarity true) exposed as a modulation source with independent attack, hold, decay, and amplitude parameters (see ahd-envelopes). On trigger, the amp section also computes a frequency-dependent gain target of `1/sqrt(freqHz/100)` for synth-like sources above 100 Hz (1.0 otherwise), slewed at 250 Hz.

#### Scenario: Both envelopes start on one trigger
- **WHEN** `AHDControl.m_trig` is true for a voice
- **THEN** `m_ahd` and `m_modulationAHD` both enter their attack stage on the same control frame
- **AND** different attack/hold/decay settings on each envelope produce independently shaped outputs from the shared timing

### Requirement: Voice UI State Exposure
The system SHALL expose per-voice UI state through `SquiggleBoyWithEncoderBank::UIState` without the UI touching live DSP objects: nine `VoiceFilterUIState` entries (atomic filter coefficients for ladder and SVF stages plus the active filter machine, providing `FrequencyResponse` and `TransferFunctionValue`) populated by `PopulateVoiceFilterUIState`, and nine `VoiceSourceUIState` entries (atomic source machine, physical-modeling, sample-source, buffer-bank, and sample-directory snapshot states) populated by `PopulateVoiceSourceUIState`.

#### Scenario: Filter response drawn from UI state
- **WHEN** the UI draws voice i's filter curve while `m_voiceFilterUIState[i].m_filterMachine` is `Ladder4Pole`
- **THEN** `FrequencyResponse(freq)` returns the product of the ladder low-pass and SVF4 high-pass responses computed from atomically loaded coefficients
- **AND** switching the voice to `SVF2Pole` changes the response to the SVF low-pass/high-pass product after the next populate

#### Scenario: Sample directory path snapshot
- **WHEN** voice i uses the Sample source machine with a loaded `AudioBufferBank`
- **THEN** `m_voiceSourceUIState[i].m_sampleDirectoryUIState.GetPath()` returns the bank's directory name, capped at 511 characters plus terminator

### Requirement: Per-Voice Scope Observability
The system SHALL write per-voice signals to dedicated scope channels via `ScopeWriterHolder`: audio scopes `VCO1`, `VCO2`, `PostFilter`, and `PostAmp` per voice, and control scopes `LFO1`, `LFO2`, `ModulationEnvelope`, and `AmpEnvelope` per voice. Audio scopes record at the base sample rate from within the oversampled loop; envelope scopes record once per control frame, with record-start marks on envelope trigger and record-end marks when the envelope returns to Idle.

#### Scenario: Amp envelope visible on its scope channel
- **WHEN** voice i plays a note
- **THEN** the `ControlScopes::AmpEnvelope` channel for voice i shows the amp AHD output starting at the trigger frame and ending when the envelope state returns to Idle
- **AND** `AudioScopes::PostAmp` for the same voice shows the corresponding audio amplitude contour
