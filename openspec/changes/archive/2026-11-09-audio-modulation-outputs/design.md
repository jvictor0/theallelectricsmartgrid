## Context

The benchmark established a small cost for clamping final root outputs. Existing sources publish the previous engine sample before manager filtering and host-frame encoder evaluation, so audio feedback routes can reuse that ordering without a new feedback solver.

## Goals / Non-Goals

Expose two source-machine signals and sub/voice stage outputs in all fifteen voice groups; preserve unclamped modulation until final parameter publication. Keep Butterworth order two, patch representation, existing random identities, and DSP ownership unchanged. Do not promote FIRs, deploy, or land.

## Decisions

- Promote the clamp at EncoderMatrix::Publish. Child modulation depths remain untouched; normalized root output is clamped before interpolation and bipolar conversion. The manager retains raw filtered excursions.
- Add `Vector m_modOut[2]` to the generic source wrapper (two floats per voice). Pass the pair to source processing overloads; keep existing audio-only overloads for standalone callers. Each source writes both outputs every sample, including zero placeholders for Sample/Thru, preventing stale values after a machine change.
- Use modulation-domain outputs: VPS samples and filtered pre-envelope noise map as `0.5 + 0.5*x`; the physical source's internal AHD output stays unipolar. Sub taps the enveloped and shaped sub before any volume gains; Voice uses the amp stage's pre-track-gain `m_voiceModOut`, with the same bipolar mapping. No mapping clamps excursions.
- Append source identities rather than reuse the existing random identities. Bind SourceMod1/2 at local slots 0/1, Sub/Voice at 12/13, in the ForEach group table. All four declare downsampling; voice bindings remain unchanged by subsequent mixer additions.
- Preserve the existing previous-engine-sample publication order. Source and output feedback therefore remain causal.
- Ship a single production path: fixed Butterworth order two and unconditional final-parameter clamps. Remove the completed experiment's compile switches, harness, and variant-only tests; retain its measurements under ignored build results and keep the production regression tests.

- Keep colors in the modulator catalogue, using the pre-group slot palette: source slots 0/1 retain red/mauve; every zero-based even/odd ganged random uses slot 2/3 yellow/fuchsia and RND1/RND2. This applies to voice and Quad groups; global names/colors already match.
- Reuse VCO1/VCO2, Sub, and PostAmp scopes for modulation. Move the existing Sub scope before frequency compensation/sub gain/track gain to its modulation tap; move Voice modulation before track gain to the existing PostAmp tap. Store audio once in bipolar form and normalize on UI reads. Keep only a separate physical source envelope history; use the existing noise visualization for physical noise. Remove duplicate writers and unused scope slots. VPS/sub cycles and AHD boundaries retain their time axes.
- Declare source-machine-to-visualizer assignments separately: Dual uses the existing LFO/time-axis scope component for both taps; Physical Modeling uses Noise and the envelope/time-axis scope component. Sample/Thru have no source view. Resolve against each source's owning trio on the UI thread. Sub/Voice always use time-domain scopes.

## Risks / Trade-offs

- Audio signals can exceed nominal amplitude → preserve the signal and clamp final parameters, with regression coverage for headroom and saturation.
- Old depths in newly populated slots become audible → this is the requested instrument metadata change, with no patch migration or persistence changes.
- Source changes can leave stale taps → explicitly clear placeholder outputs each call and test switching.
- Existing exact-arithmetic publication proofs model the interpolation stage → document and test the new clamp boundary without claiming old normalized-input proofs cover arbitrary audio excursions.

## Mixer follower extension

- Add `EnvelopeFollowerGeneric<Size>` with independent lanes, raw audio, a mean-square state, and two optional scope holders. Use `alpha = -expm1(-engineSamplePeriod / seconds)` with a 10 ms default; process every engine sample. Hold the square root of that state as the RMS envelope, updating only on host frames. Preserve excursions above one.
- Own seven quad followers in the mixer: three full send buses after feedback mixing, three effect returns before return gain/saturation, and the mastered output before recording headroom gain, output decimation, and monitor volume. Existing meters and audio processing remain independent.
- Declare tap identities and paired scope/modulator names through one ForEach list. Append fourteen unique Quad source indices, with only the seven audio sources downsampled by the manager. The seven RMS envelopes bypass additional downsampling because their power state is already low-pass filtered. Keep previous-engine-sample publication, with raw envelope and bipolar-to-unipolar audio mapping.
- Populate the three effect groups at 0/1 (send envelope/audio) and 4/5 (wet envelope/audio). All four Quad groups share final envelope/audio at 12/13; QuadLFO retains empty 0/1/4/5. Interpret “every page” as the four Quad groups, preserving the required three-lane voice and one-lane Global configurations.
- Add fourteen quad histories using the existing publication/LOD bank, increasing storage from 176 to 232 float streams per history sample across all banks. These are distinct taps, so they do not replace existing scope or meter histories.
- Add a configurable recent-scope time-axis view (100 ms by default) that does not need markers. Envelope views read the latest accepted RMS scope sample for four VU needles; share radial drawing math and convert with 20*log10, without using existing meter values or peak needles.
