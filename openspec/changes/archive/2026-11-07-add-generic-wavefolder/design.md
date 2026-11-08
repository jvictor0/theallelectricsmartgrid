## Context

The sub currently multiplies its sine by the smoothed amp envelope, evaluates normalized tanh, and applies volume. DSP runs at the engine rate using lane-generic vectors. Encoder cells already provide modulation, scenes, and persistence.

## Goals / Non-Goals

**Goals:** Encapsulate all seven shapes, morphing, safe level compensation, and DC state in a reusable processor. Keep the envelope ahead of shaping. Provide useful timbral changes with 0.125×–8× drive.

**Non-Goals:** Main voice or master DC filtering, new oversampling stages, dynamic RMS normalization, and deployment.

## Decisions

- `WavefolderGeneric<Size>` follows the existing float-vector VCO/Lissajous pattern, with mono/trio aliases, `SetGain` in physical units, and `Process` accepting audio and normalized Shape. Its constructor configures a 5 Hz `OPHighPassFilterGeneric` at the engine sample rate.
- The seven equally spaced landmarks are `t`, `t-t*t*t`, `t-t*t*s`, `t-t*s*s`, `t-s*s*s`, `s-s*s*s`, and `s`, where `t` is the existing rational tanh and `s=sin(gain*input)`. The order groups similar curves by replacing one factor at a time through the cubic folds. All landmarks are odd and retain the low-drive fundamental. Use the sine table directly with no zero-input override.
- Each audio sample computes one vector tanh and one vector sine. Polynomial arithmetic derives the rest. Adjacent landmarks use a linear crossfade, followed by one shared DC blocker per lane; changing controls never resets its state.
- Normalization coefficients update only when gain changes. Tanh and sine use their maximum magnitude over the full-scale input range. Pure cubic folds use the maximum of `u-u^3` over the reachable interval, attained at `min(peak,1/sqrt(3))`. Mixed folds use their endpoint, first reachable local maximum, or global maximum. For the existing rational tanh, their first peak inputs are approximately 0.6203883040, 0.5975659569, and 0.5782429879, with magnitudes 0.3768772696, 0.3697895228, and 0.3634337041. Global peaks are 2 at 3π/2, 1 at π, and 2 at 3π/2. Independent dense sweeps verify these bounds. This prevents division by a folded endpoint that may be zero, retains the tanh endpoint's previous normalization, and leaves envelope dynamics intact. This is peak-based scaling, not equal-loudness normalization. DC subtraction can change peak magnitude.
- Sub Drive maps `0.125*64^x`, retaining the persisted `SubTanhGain` identifier. Add `SubShape` at the end of the parameter catalogue to preserve IDs, label SHAP, default 0, at Sub page (1,1). Existing volume/rise/fall locations stay fixed.
- Register the existing audio `ScopeComponent` against `VoiceScopes::Sub` in Sub slot 0, and move `SubAnalyzer` to slot 1 with span 2. Both reuse the published sub audio and shared voice selector, with no additional DSP tap or buffering.
- The sine VCO exposes per-lane fractional `SampleTop` events using the same wrap interpolation as VPS. After writing the shaped sub audio, the track records each sub scope start at its own oscillator's top, independently of main-source and envelope triggers.

## Risks / Trade-offs

- Folding creates upper harmonics → retain engine-rate processing and existing output decimation; test representative spectra without claiming universal alias freedom.
- High-drive folding can still suppress the fundamental → guarantee its retention only in the low-drive regime and test every landmark there.
- Existing tests use constant input to stand in for sub audio → preserve their envelope assertions, but measure oscillating audio for sound/tail assertions because a DC blocker intentionally removes constant input.
- Saved normalized Drive settings produce less drive → document this intentional range change. Missing Shape roots retain normal patch-loader semantics; a fresh/default instance starts at tanh.
- A 5 Hz blocker introduces a settling tail → test decay and DC rejection without resetting on a note or knob change.

## Migration Plan

Append the new encoder, retain existing serialized roots, update documentation and catalogue fixtures, run core tests and an app build in canonical worktree build directories. Rollback is a code revert; old software ignores the new Shape root.
