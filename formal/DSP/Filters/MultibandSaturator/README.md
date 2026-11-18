# Multiband saturator: executable Lean core

[MultibandSaturator.lean](../MultibandSaturator.lean) composes the existing
phase-compensated [LR splitter](../LinkwitzRileySplitter/README.md) with band
gains, unnormalized saturation, summation, master gain and final saturation.
The splitter calls the shared TPTSVF recurrence. There is no second crossover
implementation and no call into generated C++.

This first step implements and tests the pure processor. Small-signal transfer
proofs, an optimized power-response kernel, C++ emission and app integration
are subsequent work. The current app continues to use its handwritten
multiband processor. Its legacy crossover has fourth-order Butterworth
branches; the Lean core uses the proved LR4 splitter with sibling phase
compensation. The two are intentionally different filters.

## Interface and signal order

- `LogicalSpec numBands R`: splitter cutoff tree, one linear gain per band,
  and a linear master gain. Cutoffs are cycles per processing sample, strictly
  between zero and one half. App knob mapping and clamps belong to the caller.
- `State numBands R`: the actual LR state plus band and master gains.
- `step : State → Sample R → State × Output`: advance the splitter, multiply
  each band by its gain, saturate each band, sum in ascending band order,
  multiply by master gain and saturate the sum.
- `Output`: a `before`/`after` saturation observation for each band and for
  the master, plus the pre-master mono-bass `sub` value. The final audio is
  `output.master.after`. Independent mode reports zero for `sub`.
- `reset`: clear splitter histories and stored band outputs, retaining tuning
  and gains. `zero` is untuned and must receive coefficients before processing.

Processing requires a positive band count through `NeZero numBands`. The
single-band case is useful: the splitter is the identity, followed by band and
master saturation. Zero band gains mute individual bands; zero master gain
mutes the final output without stopping the filter states.

`Tanh.tanh` provides the existing numeric interpretations: exact transcendental
tanh over the reals when `DSP.Nonlinear.Real` is imported, and the clamped
cubic rational approximation with explicit FMA for native `Float`/`Float32`.
There is no division by `tanh(gain)`: this is the app's unnormalized mastering
saturation, not the normalized ladder saturator.

The observations contain the exact values computed for audio. A future C++
adapter can feed them to the existing meters without evaluating saturation
again. RMS/peak accumulation, reduction floors, host-frame timing and atomic
publication remain outside the Lean audio state. The output record is not an
input to the next step, so changing or discarding an observation cannot feed
back into processing. The existing generator already supports `State × Output`.

## Channels and mono bass

Pointwise processing uses `R = Fin channels → Number`, with independent
coefficients and histories in each channel. Arithmetic operations and numeric
literals require no ring laws from floating-point types.

`stepMonoBass` averages the raw lowest band across the audio channels, broadcasts
that average, applies band gain and saturation, and adds the result after the
higher bands have been accumulated. The app uses shared gains across channels;
under that contract the bass observation and `sub` agree in every lane. Master
gain and saturation follow the addition, and `sub` retains its separate value
before those master operations. This preserves the app's mono-bass routing
order. The later transfer proof must include this channel coupling explicitly.

`stepWith` exposes the bass routing and saturation functions as pure parameters.
The eventual tangent program can reuse the same processing order.

## Preparing coefficients separately from audio

[LinkwitzRileySplitter/Prepared.lean](../LinkwitzRileySplitter/Prepared.lean)
adds a prepared coefficient bank without changing the existing splitter tuner:

1. `prepareSection` calls the shared TPTSVF preparation to obtain `a1`, `a2`,
   `a3` and the `(g,k)` response snapshot for one cutoff.
2. `prepare` computes one such record per crossover node. The four-band case
   prepares three records, reused by all eight split/compensation sections.
3. `installPrepared` copies the solved coefficients into the relevant sections,
   preserving histories and performing no trig or coefficient arithmetic.
4. `prepareCoefficients` returns the updated multiband state and its snapshot
   from the same prepared bank. The snapshot also retains band/master gains.

`prepareSection` can itself use function-backed channels: each lane then holds
an independent cutoff. The future backend can evaluate a packed three-cutoff
bank with Math4096 and broadcast its records into stereo or quad audio sections.
Those preparation lanes are distinct from the audio lanes used by mono bass.
This step does not introduce a new code generator or export standalone wrappers.

## Validation

`DSP.Tests.MultibandSaturator`, run by the existing `dsp_filter_smoke` executable,
checks native Float and Float32 processing for 1, 2, 3, 4, 5, 7 and 8 bands.
Prepared coefficients and snapshots are compared with the existing LR tuner,
including nonzero histories. Tests check reset, coefficient updates, muted
gains, observations and the final sum against independently evaluated rational
saturation. Additional tests cover independent audio lanes, stereo/quad mono
bass, and packed three-cutoff preparation.

These are execution checks. No multiband small-signal theorem is claimed yet.
