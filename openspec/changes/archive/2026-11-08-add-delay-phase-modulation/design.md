## Context

OLA currently uses one power-of-two frame as its accumulation ring and clears each sample when read. Quad Delay shares its grain manager implementation with sample playback. The delay page has one vacant cell, `(0, 3)`.

## Goals / Non-Goals

**Goals:** Audio-rate per-channel PM, cubic interpolation, 6H storage with H nominal latency, safe history retention, and a linear bipolar PM knob with existing modulation routing.

**Non-Goals:** Changes to FFT/window size, grain scheduling, sample playback or Partial Machine latency, and modulation-depth curves.

## Decisions

- Add a compile-time OLA latency parameter, defaulting to zero for existing clients. Storage is frame size plus twice the latency. QuadGrainManager selects one synthesis hop; frames remain 4H and its ring becomes 6H. This keeps storage and timing explicit without a second OLA algorithm or a separate delay buffer.
- Keep the clock index as the next frame start. For latency H, read at `index - H + pm`, then clear `index - 2H` and advance once. Reads never retire samples themselves. Writes add at the clock index. ClearChannel clears both retained history and queued output.
- Use the existing Catmull-Rom cubic interpolation convention with four samples and per-lane fractional positions. Bound PM to `+/- (H - 2)` engine samples. The two-sample epsilon keeps the interpolation support within retained, finalized output, including around launches and wraparound. Zero PM is exactly H extra samples of latency. Assert the input contract in the DSP reader.
- Add DelayPhaseModulation as a bipolar Quad parameter, default zero, short label OFFS to match Reverb, at `(0, 3)`. Encoder values are already bipolar; multiply directly by `H - 2` in the input setter without another depth curve or slew. Live modulation bypasses the shared manual encoder smoothing and receives no additional filter. Pass this per-sample control separately from grain-rate spectral inputs.
- Keep grain launch ordering and normalization unchanged. Add no realtime allocations.
- Initialize configured encoder outputs and their caches from the normalized default so the new bipolar knob reads zero even before its first matrix update. Exercise knob wiring through actual encoder processing; direct output-field edits bypass the DSP cache.
- Remove DelayWiden from the parameter catalogue, page, input wiring, and DSP. ReadTapeHead receives the selected buffer fraction directly. The old `(0, 2)` cell becomes empty. Saved legacy DelayWiden fields are ignored by the existing name-based loader; other controls retain their positions and names.
- Add `ResynthesizerGeneric::GetUnisonBase()`: the generic result is a vector filled with 1.03, while the four-channel specialization selects `2^(cents/1200)` for 40, 45, 50, and 55 cents in channel order. Add a vector-base constructor to the cached exponential mapper and initialize Quad Delay's detune mapper from this helper. Each channel uses the three frequency ratios `1`, `base^u`, and `base^-u`. Side-copy gain uses `(20^min(1, 3*u) - 1) / 19` and reaches its full value at `u = 1/3`.

## Risks / Trade-offs

- Additional delay latency changes timing and feedback phase even at neutral PM: document the one-hop increase (21.33 ms).
- Cubic interpolation does not eliminate aliasing from fast, deep time modulation: retain existing engine oversampling; avoid promising bandlimited modulation.
- Retirement or interpolation off-by-one errors can corrupt output near ring wraps: compare against independently accumulated unbounded reference output under endpoint jumps, fractional offsets, both launch orderings, and multiple wraps.
- Catalogue changes must preserve saved patches: serialization uses parameter names; test round trips and the neutral default for patches without the new parameter.
