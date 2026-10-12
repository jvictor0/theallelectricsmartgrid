# Source Machine

The source machine is the first stage in each `SquiggleBoyVoice` and is responsible for generating (or forwarding) the raw signal before filtering and amplitude shaping.

## Source Types

Each voice currently supports four source machine modes:

1. **Dual Wave Shaping VCO**: dual wavetable oscillator source with vector phase shaping and cross-modulation. Implemented in `DualWaveShapingVCO.hpp`.
2. **Physical Modeling**: noise-excited source with a morphable SVF, AHD modulation, sample-rate reduction, and a tuned decay comb. Implemented in `PhysicalModelingSource.hpp`.
3. **Thru**: external-input passthrough source.
4. **Sample**: per-voice sample-directory source that reads a bank of WAV files, blends between adjacent files, and resynthesizes playback with the grain engine. Implemented by `SampleSource` in `DualSampleSource.hpp`.

## Dual Wave Shaping VCO Source Machine

The primary sound source is the **Dual Wave Shaping VCO** (`DualWaveShapingVCO`), which consists of two complex wavetable oscillators (`VectorPhaseShaperInternal`) that can modulate each other.

### Random Morphing Wavetables

The core of each oscillator is a randomly generated, constantly morphing additive wavetable (`MorphingWaveTable`).

- There are two wavetables active at any time (Left and Right).
- A random LFO lazily crossfades between them.
- When the crossfade reaches 100% to one side (making the other table completely inaudible), the inaudible wavetable is discarded and replaced with a newly generated random wavetable. This ensures continuous, infinite timbral evolution.
- The harmonic complexity of the generated wavetables is controlled by a **Level of Detail (LOD)** parameter (`m_morphHarmonics`), which determines how many harmonics are included.

### Vector Phase Shaping

The oscillators use **Vector Phase Shaping (VPS)** to distort the read phase of the wavetable. This is controlled by two parameters per oscillator:

- **v (Vertical)**: controls the vertical inflection point of the phase distortion.
- **d (Horizontal)**: controls the horizontal inflection point (duty cycle/symmetry) of the phase distortion.

### Dual Oscillator Interaction

The Dual Wave Shaping VCO contains two such oscillators (VCO 0 and VCO 1):

- **Pitch**: VCO 0 runs at the base frequency (modified by `detune`). VCO 1 runs at `baseFreq * offsetFreqFactor / detune`.
- **Mutual phase modulation**: each oscillator can phase-modulate the other. The amount of modulation is controlled by `m_crossModIndex`.
- **Mix**: the outputs of VCO 0 and VCO 1 are mixed together using an equal-power crossfade (`fade`).
- **Bitcrusher**: the mixed output is finally passed through a bit-rate reducer (`BitRateReducer`) for digital degradation.

## Thru Source Machine

The **Thru** source machine allows external audio to be routed through the voice's filter and amp sections. It is currently a simple pass-through for external signals, allowing the synthesizer to act as a polyphonic filter bank and envelope shaper for outside audio.

## Physical Modeling Source Machine

The **Physical Modeling** source machine (`PhysicalModelingSource`) is a noise-driven resonant source built around a damped comb. The chain is:

1. White noise excitation.
2. Morphable 2-pole SVF pre-filter (LP/BP/HP blend).
3. Inverted AHD envelope modulation.
4. Sample-rate reduction for texture control.
5. Tuned decay comb (`TunedDecayComb`) with phase-compensated two-pole low-pass damping.

The source runs at 4× the 48 kHz base rate. Its amplitude-depth control is inverted; attack ranges from 100 µs to 2.5 s and decay from 500 µs to 2.5 s. Envelope durations advance in base-rate sample units, and each microblock trigger/release is delivered once. SVF morph uses quarter-circle LP→BP→HP weights, shared with its UI response.

Factory defaults are CUT 100%, RES 0%, MRPH 0%, RATE 0%, FDBK 80%, DAMP 50%, ATK 0%, HOLD 25%, DEC 0%, and AMP 100%. RATE 0 reaches the full 192 kHz processing rate and bypasses sample holding; the exponential range retains its 48 Hz lower endpoint. A small upper-end headroom lets the parameter slew reach bypass reliably. Defaults apply to new/reset parameter state; saved parameter values remain intact.

DECAY retains its signed knob mapping: negative feedback below 0.5, positive above 0.5, and dry at the exact center, including with DAMP enabled. With `u = abs(2*knob-1)`, `T60 = 0.001 * 5000^u` seconds sets the exact stationary fundamental lifetime and a ceiling on all other modal lifetimes, without an additional feedback blend. This holds for every DAMP setting and either sign, including clamped pitch boundaries. The 5 ms parameter transition keeps changes through the dry center smooth; moving coefficients do not have stationary decay guarantees.

DAMP controls a two-pole low-pass whose prototype has a unity magnitude peak exactly at the clamped fundamental; no frequency exceeds unity. For clamped `d = DAMP`, the actual prototype −3 dB cutoff is `fc/Fs = min(0.49, (f0/Fs)*exp2(1+6*d))`. Increasing DAMP opens that cutoff exponentially: 0, 0.25, 0.5, 0.75, and 1 select 2×, 4√2×, 16×, 32√2×, and 128× the fundamental before the processing-rate Nyquist clamp. Equal knob distances give equal octave intervals. Q varies with both prewarped frequencies and approaches Butterworth (`1/sqrt(2)`) as the cutoff opens. At minimum DAMP and low normalized pitch, Q is about 0.855124 and DC gain is about 0.948683298 (−0.457575 dB), below the fundamental peak. Neither endpoint bypasses damping; exact DECAY center is the only bypass. Both excitation and returned delay pass through the low-pass, and output is taken after it. There is no dry blend or uniform gain boost of the old Butterworth response. Saved parameter ID `PMCombDampenCutoff`, default 0.5, and the DAMP label remain unchanged; the default midpoint cutoff intentionally changes from 32× to 16×, giving existing patches the new sound.

The stationary prototype and radial decay transform are:

```text
omega = 2*pi*f0/Fs, t = tan(pi*f0/Fs), v = tan(pi*fc/Fs)/t
shape = hypot(1, v*v-1), u2 = 1/shape
g = t*sqrt(shape), k = sqrt(2*(1-u2)), gain = sqrt((1-u2)*(1+u2))
L(z) = gain*g^2*(1+z^-1)^2 /
       ((1+k*g+g^2)+2*(g^2-1)*z^-1+(1-k*g+g^2)*z^-2)
theta = (sign>0 ? 2*pi : pi)
D = (theta + arg(L(exp(j*omega)))) / omega
N = max(1, floor(D)-1), delta = D-N
A(z) = (a+z^-1)/(1+a*z^-1)
a = (tan(omega/2)-tan(omega*delta/2)) / (tan(omega/2)+tan(omega*delta/2))
H0(z) = L(z) / (1-sign*z^-N*A(z)*L(z))
r = exp(-ln(1000)/(Fs*T60)), H(z) = H0(z/r)
```

At exact DECAY center, `L` is identity. Integer delay and the normalized allpass lattice compensate the low-pass phase at the played fundamental. The integer read uses `r^N`; the allpass state and both double-precision TPT low-pass integrator states receive `r` each sample. Every prototype pole moves radially inward, and low-pass loss cannot extend a stationary mode beyond the selected T60. Bypassed integrators are cleared so dormant energy cannot return when damping is re-enabled.

The unity peak and fundamental phase compensation put the prototype's fundamental poles on the unit circle. The radial transform therefore gives the fundamental exactly the selected T60. Upper partials still disperse and decay sooner: at 220 Hz, 192 kHz, DAMP=1/3 (8× cutoff), and a 5 s selection, independently solved negative/positive third modes are 658.545544/659.270770 Hz (−3.819368/−1.913887 cents), with T60 1.427294/2.210499 s. Full-open remains near harmonic with near-5 s third-mode decay. At the closed endpoint, independent negative/positive third-mode predictions are 636.849346/654.515578 Hz and just 0.013876/0.028245 s T60. Those modes die before the usual late projection windows; the surviving fundamental can dominate such measurements. Emitted-audio tests instead verify upper-partial suppression and closed prototype attenuation at three times the fundamental. Positive feedback also retains second and nonoscillatory modes, so total residual energy after removing the fundamental cannot measure the third mode's millisecond lifetime. Phase compensation targets the fundamental, not every partial.

Normalized pitch is clamped to `[cycles/16383, 0.25]`, where cycles is 1 for positive/center and 0.5 for negative feedback. Integer reads stay inside the 16384-sample memory. Usually `delta` lies in `[1,2)`; at the highest negative-feedback pitches, `D` can be slightly below 2, so `N=1` and `0<delta<1` avoid unsigned underflow or a zero-delay read. At PM's 192 kHz processing rate the low pitch bounds are approximately 11.72 Hz positive and 5.86 Hz negative; the upper bound is 48 kHz. The low-pass cutoff clamps to 0.49 of the processing rate, while audible output retains the host's base-rate Nyquist limit.

Coefficient calculations happen once per changed microblock target. Two small filter/read branches share the delay buffer and crossfade their filtered outputs over 5 ms; the mixed output is written into the shared delay. Changes during a fade coalesce to the latest target without restarting it. No delay-buffer copies, allocations, or locks occur in processing. Modal guarantees apply after settling: moving parameters change the trajectory, and the plotted frozen-coefficient response is not a time-varying-system transfer function.

This source owns a `UIState` that implements `TransferFunction`, so the visualizer can draw the combined pre-comb SVF and comb response in the Source bank.

## Sample Source Machine

The **Sample** source machine (`SampleSource`) reads from one `AudioBufferBank` per voice. The bank is assigned in the Voice Config grid and points at a directory under the Smart Grid One `samples/` root.

- Each bank loads all `.wav` files in the selected directory.
- `SampleBankPosition` scans across the loaded WAV files. Even segments select a single file; odd segments linearly blend adjacent files.
- Playback position comes from `PhasorPlayHead`, which reads absolute modulated phase through `GetPhase(loop, sample, PhaseDomain::Modulated)` and applies read speed before wrapping.
- `SampleStart` and `SampleLength` define a wrapped window inside the normalized sample domain.
- `SampleReadSpeed` selects quantized reverse, stopped, and forward rates from `-4x` through `4x`.
- Audio is rendered through `GrainManager<AudioBufferBank>`, reusing the grain/resynthesis path used by the phase-vocoder components.

Directory navigation and loading are asynchronous:

- UI commands are sent through `IoTaskThread`.
- `DirectoryExplorer` handles folder traversal and selection.
- Confirming a selection (`Yes`) builds a new `AudioBufferBank` and swaps it into the selected voice during acknowledgment.
- Restoring saved state queues `LoadAudioBufferBankFromDirectory` tasks for each saved relative sample directory.

The JUCE voice config page now displays:

- active trio name and source/filter mode,
- one per-voice sample directory label for the active trio,
- a directory explorer view while browsing.

The Source visualizer bank includes `SampleTrioWaveformVisualizerComponent` for Sample voices. It draws min/max waveform buckets for the active trio's three sample banks, highlights the active start/length window in each voice color, and overlays the current read-head position.

## Parameter Visibility

Parameters are tagged with which source and filter machines they apply to (see [Encoder System](encoder-system.md#machine-specific-parameters)). When a source machine is selected, only matching source-specific controls are connected (e.g. VCO harmonic/phase controls for Dual VCO, comb/damping controls for Physical Modeling). This keeps the interface focused on controls relevant to the active machine.

Sample-specific Source bank controls are only visible for Sample voices:

- `SampleLoopIndex`: chooses which of the six Theory of Time loops drives the read head.
- `SampleStart`: normalized start position of the playback window.
- `SampleReadSpeed`: quantized read speed, including reverse and stopped playback.
- `SampleLength`: normalized wrapped length of the playback window.
- `SampleBankPosition`: position across the WAV files loaded in the selected bank.

## Related

- [DSP Overview](dsp-overview.md)
- [Ladder Filter Machine](filter-ladder.md)
- [SVF Filter Machine](filter-svf.md)
