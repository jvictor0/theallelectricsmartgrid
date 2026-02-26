# Phase Vocoder

The phase-vocoder path is implemented by `Resynthesizer` (`private/src/Resynthesis.hpp`) and driven by `GrainManager` (`private/src/DelayLine.hpp`) inside the Quad Delay.

## Why it is needed

The Quad Delay uses a moveable read head in warped time. Without correction, variable read speed causes strong pitch shifts.  
The phase vocoder decouples time-stretch from perceived pitch so delay-time warping can happen while keeping spectral pitch stable.

## Processing flow (code-level)

`GrainManager::Process()` launches a new grain every `Resynthesizer::GetGrainLaunchSamples()` samples, i.e. every hop `H = N / 4` (`x_hopDenom = 4`).

For each launched grain:

1. **Read analysis buffers**
   - Current frame: from warped-time read position (`startTime`).
   - Previous frame: from `startTime - H`.
   - Both are Hann-windowed in `Grain::Start(...)`.
2. **Prime previous analysis**
   - `Resynthesizer::PrimeAnalysis(previousWaveTable)` stores previous bin phases/magnitudes.
3. **Analyze current frame**
   - `DFT::Transform(buffer)`
   - `ProcessPhases()` computes:
     - `m_analysisMagnitudes[bin]`
     - `m_analysisPhase[bin]`
     - instantaneous frequency `m_omegaInstantaneous[bin]` via phase-deviation formula:
       - subtract expected `omega_bin * H`
       - wrap with principal argument
       - add correction back to `omega_bin`
4. **PVDR phase-relationship build**
   - `PVDR::Analyze()` builds leader/follower bin relationships.
5. **Synthesis phase update**
   - `Oscillator::FixupPhases(detune, pvdr)` advances synthesis phase per PVDR result.
6. **Synthesize shifted/unison spectrum**
   - `Oscillator::Synthesize(...)` writes complex bins with shift/unison gains.
7. **Inverse transform**
   - `InverseTransform(...)` creates the grain waveform and `grain->Start()` schedules playback.

This is the "phase vocoder done right" principle in practice: phase propagation is controlled by measured inter-frame phase advance, not by naive phase reuse.

## How PVDR is used

`PVDR` is the core phase-relationship tracker that prevents incoherent bin drift.

- It prioritizes bins by magnitude (`PriorityQueue<Entry,...>` over current and previous analysis magnitudes).
- For each bin it emits a `PVDR::Result`:
  - `m_bin`: current bin
  - `m_parent`: leader bin this bin follows
  - `m_delta`: phase increment to apply from parent
  - `m_small`: low-energy/randomized handling flag
- Strong bins become leaders (`bin == parent`), usually using `omega_instantaneous * H`.
- Neighbor bins can attach to leaders and inherit phase structure through relative phase differences.
- `PushResult()` recursively flattens ancestry so each result references a stable root parent.

During synthesis:

- `FixupPhases()` walks PVDR results and updates `m_synthesisPhase[bin]`.
- Followers inherit parent phase plus stored delta; leaders get explicit phase advance.
- This keeps partial groups phase-coherent under warp and pitch transformations.

## Synthesis implementation

Each grain is synthesized by **3 oscillators** (`x_numOscillators = 3`), each maintaining its own `m_synthesisPhase[]`.

- Oscillator 0: center voice
- Oscillator 1: detuned up
- Oscillator 2: detuned down

For each oscillator, `Synthesize()` mixes two shift layers (`Oscillator::x_numShifts = 2`):

- shift layer A: unshifted (`Q(1,1)`)
- shift layer B: selected musical ratio (`input.m_shift[0]`)
- Crossfade is controlled by `m_fade[0]` with curve `fade^4`:
  - `gainA = 1 - fade^4`
  - `gainB = fade^4`

Per-bin magnitude source is `m_synthesisMagnitudes[bin].m_output`, which is slewed to reduce abrupt spectral jumps.

## Pitch shifting details

Pitch shift is ratio-based (`Q` rational values), selected upstream in `QuadDelayInputSetter` from musical intervals.

`Oscillator::SynthesizeSingleShift()`:

- If shift is `1/1`, bins are synthesized directly.
- Otherwise:
  - leader target bin = `BinOmega(omega_instantaneous * shift)`
  - leader phase = `synthesisPhase * shift`
  - follower bins keep local offsets relative to parent:
    - `targetFollower = targetParent - parent + bin`
    - `phaseFollower = phaseParent + delta`
- Alias guard:
  - `WillAlias(...)` rejects bins where frequency mismatch exceeds threshold (`0.5 / H` criterion).

So the shift is not "bin-by-bin naive remap"; it preserves PVDR structural relationships where possible.

## Unison details

Unison is implemented as 3-oscillator energy-distributed layering:

- Detune factors:
  - osc0: `1.0`
  - osc1: `m_unisonDetune`
  - osc2: `1.0 / m_unisonDetune`
- Gain distribution (`GetUnisonGainForOscillator()`):
  - center: `sqrt(1 - 2 * u^2 / 3)`
  - side oscillators: `u / sqrt(3)`
  where `u = m_unisonGain`.

This keeps output level controlled while adding symmetric spread.

In addition, delay modulation can inject per-channel sample offsets (`sampleOffset`) before analysis grain capture, which contributes to stereo/quad width and motion.

## Notes on exposed controls

From `QuadDelayInputSetter` into `Resynthesizer::Input`:

- `m_shift[0]`: musical pitch ratio
- `m_fade[0]`: blend between dry spectral map and shifted spectral map
- `m_unisonDetune`, `m_unisonGain`
- `m_slewUp`: partial rise-rate control for spectral bloom

`m_slewDown` exists in the input struct but the active code path currently applies `SetSlewUp(...)` during grain start.

## Quad Delay integration

In the delay path:

- `QuadDelayInputSetter` computes warped read head positions.
- `QuadGrainManager` launches grains at those positions.
- `Resynthesizer` performs phase-coherent reconstruction.

This enables:

- pitch-stable warped delays
- unison detune/spread
- interval pitch-shift choices (`Q` ratios)
- per-partial slew-up behavior for gradual spectral bloom

## Related

- [Quadraphonic Delay and Phase Vocoder](quad-delay.md)
- [DSP Overview](dsp-overview.md)
