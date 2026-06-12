# Quad Panning Specification

## Purpose
The quad panning system (`SquiggleBoyVoice::PanSection` in `private/src/SquiggleBoy.hpp` and `LissajousLFOInternal` in `private/src/Lissajous.hpp`) positions each of the nine voices (see voice-architecture) in a two-dimensional quadraphonic field. A single global phase oscillator drives a per-voice Lissajous LFO; static per-voice phase offsets keep the nine voices spatially distributed, and per-voice multiplier, center, radius, and phase-shift parameters shape each voice's spatial trajectory. The resulting normalized (X, Y) coordinates feed the quadraphonic mixer's pan law, which distributes each voice's signal across the four output channels (the mixer's bus and mastering behavior is specified in mixdown-mastering).

## Requirements

### Requirement: Shared Global Pan Phase
The system SHALL drive panning for all nine voices from a single global phase oscillator (`SquiggleBoy::m_panPhase`, a `PhaseUtils::SimpleOsc`). The oscillator advances once per sample by a fixed phase delta of 0.025/44100 (one full pan cycle approximately every 37 seconds at 48 kHz; `SetPhaseDelta` is never called on it), and every voice's pan input receives the identical phase value each sample. No voice has an independent pan clock.

#### Scenario: All voices share one phase clock
- **WHEN** `SquiggleBoy::ProcessSample` runs for one audio sample
- **THEN** `m_panPhase.Process()` advances the global phase exactly once
- **AND** `m_state[i].m_panInput.m_input` is set to the same `m_panPhase.m_phase` value for all voices i in 0..8

#### Scenario: Phase wraps on the unit circle
- **WHEN** the global pan phase exceeds 1.0
- **THEN** it wraps by subtracting 1.0, so the phase delivered to every voice always lies in [0, 1)

### Requirement: Static Per-Voice Phase Offsets
The system SHALL add a static phase offset to each voice's Lissajous phase, computed from the voice index as `staticOffset = (i mod x_numTracks) / x_numTracks + floor(i / x_numTracks) / (x_numTracks * x_numTracks)` with `x_numTracks = 3`. This yields nine distinct offsets, one at each multiple of 1/9 (voice order 0, 3, 6, 1, 4, 7, 2, 5, 8 ninths), so the voices are spread evenly around the shared pan cycle even with all user parameters at zero. The modulatable `PanPhaseShift` encoder value is added on top of the static offset.

#### Scenario: Nine distinct offsets at neutral settings
- **WHEN** `PanPhaseShift` is 0 for all nine voices
- **THEN** each voice's effective phase shift equals its static offset, and the nine offsets are the nine distinct values k/9 for k in 0..8
- **AND** no two voices receive the same Lissajous input phase in the same sample

#### Scenario: User phase shift rotates a voice's figure
- **WHEN** `PanPhaseShift` for one voice changes while `PanMultX`, `PanMultY`, `PanCenterX`, `PanCenterY`, and `PanRadius` stay constant
- **THEN** that voice's position advances along the same Lissajous trajectory (a pure phase rotation) without changing the figure's shape, center, or size

### Requirement: Per-Voice Lissajous Figure Parameters
The system SHALL compute each voice's raw pan coordinates with a per-voice `LissajousLFOInternal`, whose parameters are mapped from the encoder banks per voice: `m_multX` and `m_multY` map from [0, 1] to [1, 5] (`value * 4 + 1`), `m_centerX` and `m_centerY` map from [0, 1] to [-1, 1] (`value * 2 - 1`), and `m_radius` is used directly in [0, 1]. The X axis is evaluated a quarter cycle ahead of the Y axis (`tx` uses `t + 0.25`), so equal multipliers of 1 trace a circle (cosine against sine) rather than a diagonal line.

#### Scenario: Unit multipliers trace a circle
- **WHEN** a voice has `m_multX = 1`, `m_multY = 1`, `m_phaseShift = 0`, and `m_radius = 1`
- **THEN** its raw coordinates are (cos(2πt), sin(2πt)) as the input phase t sweeps 0 to 1, tracing one full circle per global pan cycle

#### Scenario: Different multipliers trace different figures
- **WHEN** voice A has multipliers X:Y = 1:3 and voice B has multipliers 2:1
- **THEN** voice A's Y axis completes three sine cycles per X cycle while voice B's X axis completes two cycles per Y cycle, producing distinct Lissajous figures

### Requirement: Fractional Multiplier Continuity
The Lissajous computation SHALL handle non-integer multipliers by amplitude-scaling the trailing partial cycle: when the scaled phase on an axis enters the final fractional cycle (`floor(mult) < t * mult`), the amplitude for that axis is reduced to the fractional part `mult - floor(mult)` and the phase is renormalized to a full sine cycle within that fraction, so the output remains continuous across the cycle boundary instead of jumping mid-sine.

#### Scenario: Non-integer multiplier stays continuous
- **WHEN** a voice has `m_multX = 2.5` and the input phase sweeps through one full cycle
- **THEN** the X output completes two full-amplitude sine cycles followed by one half-amplitude full sine cycle
- **AND** the X output has no discontinuity at the boundary between the integer and fractional cycles

### Requirement: Radius Scaling and Center Mixing
The system SHALL blend each axis between its sine term and its center point by radius: `output = radius * amp * sin(2π t) + (1 - radius) * center`. At radius 0 the coordinates equal the center point exactly (a static pan position); at radius 1 the figure spans full sine amplitude and the center contribution vanishes.

#### Scenario: Radius zero collapses to the center point
- **WHEN** a voice has `m_radius = 0` and `m_centerX = 0.5`, `m_centerY = -0.5`
- **THEN** its raw coordinates are exactly (0.5, -0.5) on every sample regardless of the input phase

#### Scenario: Radius sweep morphs from point to full figure
- **WHEN** `PanRadius` sweeps from 0 to 1 on a voice
- **THEN** the voice's trajectory grows from the fixed center point to the full-amplitude Lissajous figure, with intermediate radii producing a proportionally scaled figure pulled toward the center

### Requirement: Normalized Coordinates and Quadraphonic Pan Law
The system SHALL map the raw Lissajous coordinates from [-1, 1] into [0, 1] (`PanSection` computes `output * 0.5 + 0.5`) before handing them to the quadraphonic mixer, which applies the quarter-sine pan law `QuadFloat::Pan(x, y, sample)`: channel gains sin(2π(1-x)y/4), sin(2πxy/4), sin(2πx(1-y)/4), and sin(2π(1-x)(1-y)/4), placing the four channels at the corners (0,1), (1,1), (1,0), and (0,0) of the unit pan square. Per-voice positions are published to the UI via `UIState::SetPos(i, x, y)`.

#### Scenario: Corner position isolates one channel
- **WHEN** a voice's normalized pan position is (0, 1)
- **THEN** the mixer pan gains are 1 for channel 0 and 0 for channels 1, 2, and 3, so the voice's signal appears only in channel 0

#### Scenario: Center position feeds all channels equally
- **WHEN** a voice's normalized pan position is (0.5, 0.5)
- **THEN** all four channel gains equal sin(π/8) (approximately 0.383), distributing the signal equally across the quad field

#### Scenario: Pan positions are observable in the UI state
- **WHEN** the engine processes audio with the panning visual display active
- **THEN** each voice's current (X, Y) position in [0, 1] x [0, 1] is readable from the UI state via `SetPos`, matching the coordinates handed to the mixer
