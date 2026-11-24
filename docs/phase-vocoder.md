# Phase Vocoder

The phase-vocoder path is implemented by `ResynthesizerGeneric` (`private/src/Resynthesis.hpp`) and driven by `GrainManagerGeneric` (`private/src/DelayLine.hpp`). Quad Delay uses one packed four-channel instance; sample playback uses one packed three-channel instance per track. Channels share the grain launch schedule, transforms and overlap-add storage while retaining independent spectra, read positions and phase propagation.

## Why it is needed

The Quad Delay uses a moveable read head in warped time. Without correction, variable read speed causes strong pitch shifts.  
The phase vocoder decouples time-stretch from perceived pitch so delay-time warping can happen while keeping spectral pitch stable.

## Processing flow (code-level)

`GrainManager::Process()` runs every engine sample and launches a new grain every `Resynthesizer::GetSynthesisHopSamples()` engine samples: the duration of the analysis hop `H = N / 4` (`x_hopDenom = 4`, `GetGrainLaunchSamples()` host samples). Analysis reads host-rate audio; the synthesized frame is `N * x_oversample` engine samples long.

For each launched grain:

1. **Read analysis buffers**
   - Current frame: from warped-time read position (`startTime`).
   - Previous frame: from `startTime - H`.
   - Both are Hann-windowed in `GrainManager::StartGrain(...)`.
2. **Prime previous analysis**
   - The generated frame calls the packed FFT for the previous buffer and
     `FrameStorage.primeCoefficients` stores its bin phases and magnitudes.
3. **Analyze current frame**
   - The generated frame calls the same FFT for the current buffer.
   - `FrameStorage.analyzeCoefficients` updates:
     - `AnalysisMagnitudes()[bin]`
     - `m_analysisPhase[bin]`
     - instantaneous frequency `m_omegaInstantaneous[bin]` via phase-deviation formula:
       - subtract expected `omega_bin * H`
       - wrap with principal argument
       - add correction back to `omega_bin`
4. **PVDR phase-relationship build**
   - `PVDR.Storage.analyze` builds leader and follower relationships.
5. **Synthesis phase update**
   - `SpectrumStorage.fixup` advances synthesis phase in PVDR visit order.
6. **Synthesize shifted/unison spectrum**
   - `SpectrumStorage.synthesize` accumulates complex bins with shift and unison gains.
7. **Inverse transform**
   - The packed IFFT interpolates the 4096-point spectrum into an `N * x_oversample`-sample engine-rate frame. `FrameStorage.applyWindow` multiplies it by the synthesis Hann window.
   - `GrainManager` overlap-adds the frame into an `OLAGeneric` ring and reads one sample per engine sample, divided by 1.5 (four quarter-window shifts of Hann squared sum to 1.5).
   - Quad Delay selects a 6H ring with H nominal output latency, where H is the engine-rate synthesis hop. It reads each channel at `clock - H + PM` using Catmull-Rom cubic interpolation, with PM bounded to `[-H + 2, H - 2]` engine samples. A fixed cleanup position retires old samples independently of the moving read head. Sample playback retains its 4H ring and existing timing.

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
- `PVDR.Storage.attach` inherits the parent's root and accumulates the relative phase, so each result carries a direct link to its root.

Each channel keeps a three-state Schmitt gate per bin, using the largest
magnitude across its current and reference analysis frames:

- **Low → Triggered** when magnitude rises above −60 dB of that maximum.
- **Triggered → High** on the next grain, unless the bin falls Low.
- **High → Low** below −72 dB; exact silence is always Low.

The hysteresis band preserves the previous Low or High state. Low bins receive
random phase increments and are excluded from shifted synthesis. There is no
separate −160 dB cutoff or amplitude-growth retrigger. Per-grain graph clearing
preserves gate history; channel and full resynthesizer resets rearm the gates.

During synthesis:

- `SpectrumStorage.fixup` walks PVDR results and updates `m_synthesisPhase[bin]`.
- A Triggered leader sets each oscillator's synthesis phase exactly to its current analysis phase.
- On subsequent High grains, leaders resume phase advance with the oscillator's detune factor.
- Followers inherit parent phase plus stored delta, including when their own gates trigger.
- This keeps partial groups phase-coherent under warp and pitch transformations.

## Synthesis implementation

Each grain is synthesized by **3 oscillators** (`x_numOscillators = 3`), each maintaining its own `m_synthesisPhase[]`.

- Oscillator 0: center voice
- Oscillator 1: detuned up
- Oscillator 2: detuned down

For each oscillator, `SpectrumStorage.synthesize` mixes two shift layers:

- shift layer A: unshifted (`Q(1,1)`)
- shift layer B: selected musical ratio (`input.m_shift[0]`)
- Crossfade is controlled by `m_fade[0]` with curve `fade^4`:
  - `gainA = 1 - fade^4`
  - `gainB = fade^4`

Per-bin magnitude comes from `SynthesisMagnitudes()[bin][channel]`.
[FrameStorage](../formal/DSP/Resynthesis/FrameStorage.lean) owns the analysis,
magnitude slew, transform order, and window application. Its generated frame
calls the generated [PVDR traversal](../formal/DSP/Resynthesis/PVDRStorage.lean)
and [spectrum synthesis](../formal/DSP/Resynthesis/SpectrumStorage.lean).

## Pitch shifting details

Pitch shift is ratio-based (`Q` rational values), selected upstream in `QuadDelayInputSetter` from musical intervals.

`SpectrumStorage.accumulate` chooses the synthesis path:

- If shift is `1/1`, bins are synthesized directly.
- Otherwise:
  - leader target bin is the rounded and clamped bin for `omega_instantaneous * shift`
  - leader phase = `synthesisPhase * shift`
  - follower bins keep local offsets relative to parent:
    - `targetFollower = targetParent - parent + bin`
    - `phaseFollower = phaseParent + delta`
- Alias guard:
  - `acceptedBinWith` rejects out-of-range bins and bins where frequency mismatch exceeds `0.5 / H`.

So the shift is not "bin-by-bin naive remap"; it preserves PVDR structural relationships where possible.

## Unison details

For the delay's Unison knob `u` in `[0, 1]`, each channel uses detune ratio
`d = 2^(C*u/1200)`, where `C` is 40, 45, 50, or 55 cents for channels 1–4.
The three copies have frequency multipliers `1`, `d`, and `1/d`, so their pitch
offsets are zero and `+/-C*u` cents. The spread is deterministic and symmetric
in cents. At half Unison, the four channels spread by `+/-20`, `+/-22.5`,
`+/-25`, and `+/-27.5` cents respectively.

`ResynthesizerGeneric::GetUnisonBase()` supplies the four channel bases as
`2^(C/1200)` to the cached exponential mapper. Other channel counts return a
vector filled with the original `1.03` base.

- Detune factors:
  - osc0: `1.0`
  - osc1: `m_unisonDetune`
  - osc2: `1.0 / m_unisonDetune`
- Gain distribution (`GetUnisonGainForOscillator()`):
  - center: `sqrt(1 - 2 * g^2 / 3)`
  - side oscillators: `g / sqrt(3)`
  where `g = m_unisonGain = (20^min(1, 3*u) - 1) / 19`.

The first third of knob travel fades in the side copies and reduces the center
gain. At one third, all three gains are `1/sqrt(3)` and the pitch spreads are
approximately `+/-13.33`, `+/-15`, `+/-16.67`, and `+/-18.33` cents across the four channels.
Further travel increases detuning while gains stay equal.
The squared gains sum to one throughout the fade.

The generic grain manager accepts per-channel host-sample offsets before analysis capture. Quad Delay passes zero and derives channel differences from its tape heads; it has no internal LFO.

## Notes on exposed controls

From `QuadDelayInputSetter` into `Resynthesizer::Input`:

- `m_shift[0]`: musical pitch ratio
- `m_fade[0]`: blend between dry spectral map and shifted spectral map
- `m_unisonDetune`, `m_unisonGain`
- `m_slewUp`: partial rise-rate control for spectral bloom

`m_slewDown` is reserved in the input struct. `FrameStorage.analyzeCoefficients`
uses the rise cutoff for increasing magnitudes and tracks falling magnitudes immediately.

## Quad Delay integration

In the delay path:

- `QuadDelayInputSetter` computes warped read head positions.
- `QuadGrainManager` launches all four channels together at their individual mapped positions, bounded by the latest complete recorded analysis window.
- `Resynthesizer` performs phase-coherent reconstruction.

This enables:

- pitch-stable warped delays
- unison detune/spread
- interval pitch-shift choices (`Q` ratios)
- per-partial slew-up behavior for gradual spectral bloom
- per-channel output PM through the linear bipolar PM knob on the delay page

## Exact steady-sinusoid result

The [Lean theorem](../formal/DSP/Resynthesis/HannToneVocoder.lean) follows a
steady sinusoid through the exact one-channel `process` model and the normalized
four-grain overlap-add sum. The source and shifted frequencies sit on interior
FFT bins, with both neighboring bins present. With full shift fade, unison off,
and immediate magnitude tracking, four overlapping grains preserve the input
amplitude and multiply the pitch by the selected ratio.

Each pair of analysis windows samples the ideal sinusoid at host-rate spacing,
one analysis hop apart. Their start may move arbitrarily between grains. The
first grain sets the output phase, and subsequent grains advance it at the
synthesis hop. The theorem uses exact real arithmetic and assumes the queue
implements maximum-priority pop and preserves the remaining entries.

Separate correspondence proofs connect the storage-based PVDR traversal to
the model's gates, graph, and random-source state. The spectrum proofs connect
phase updates and both coefficient accumulation paths to the model. They apply
to the real interpretation as well as the executable types.

For one 4096-point channel, the exact interpretation of the generated frame
agrees with the mathematical spectral step, finite inverse DFT, and supplied
window at every output sample. The proof also identifies the retained channel
and random-source state. The packed model's one-channel specialization uses
that spectral step followed by the inverse transform and Hann window. The
differential tests exercise the generated packed frame and its floating-point
primitives. [Frame correspondence](../formal/DSP/Resynthesis/FrameExactProof.lean)

## Related

- [Quadraphonic Delay and Phase Vocoder](quad-delay.md)
- [DSP Overview](dsp-overview.md)
