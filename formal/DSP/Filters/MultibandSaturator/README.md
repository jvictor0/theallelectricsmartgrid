# Multiband saturator: core and small-signal response

[MultibandSaturator.lean](../MultibandSaturator.lean) composes the existing
phase-compensated [LR splitter](../LinkwitzRileySplitter/README.md) with band
gains, unnormalized saturation, summation, master gain and final saturation.
The splitter calls the shared TPTSVF recurrence. There is no second crossover
implementation and no call into generated C++.

The pure processor, time-first small-signal proofs and optimized power-response
kernel are implemented and emitted through the shared C++ generator. The app's
four-band stereo and quad mastering chains use the generated core through
`private/src/MultibandSaturator.hpp`. The old crossover had fourth-order
Butterworth branches; this core uses the proved LR4 splitter with sibling phase
compensation. That response change is intentional. Knob mapping, cutoff clamps,
mono-bass/sub routing and meter timing retain their app contracts.

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

The observations contain the exact values computed for audio. The C++
wrapper feeds them to `MeterGeneric::RecordSaturated(before, after)` without
evaluating saturation again. RMS/peak accumulation, reduction floors, host-frame timing and atomic
publication remain outside the Lean audio state. The output record is not an
input to the next step, so changing or discarding an observation cannot feed
back into processing. The existing generator already supports `State × Output`.

The generated entry point is [Runtime.process4Band](Runtime.lean), a compact
representation of that same step. Its state stores the splitter's coefficients
and integrator memories plus gains. Child SVF taps, leaf outputs and duplicate
band observations remain local expressions, so unused taps are not computed or
saved. The original model remains the arithmetic source and differential-test
oracle; no crossover or saturation equations are duplicated.

`Runtime.simulation` and `Runtime.output_stream` prove equality of every output
sample, including all meter observations, using the shared processor simulation
theorem. [RuntimeProofs.lean](RuntimeProofs.lean) proves that coefficient
preparation, prepared installation and reset commute with the compact-state
projection. Thus the existing small-signal result applies to the generated
processor's output stream, with the same coefficient and initialization
assumptions. No change to the transfer function or controller is involved.

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
order. The mono-bass small-signal theorem includes this channel coupling
explicitly, including different input phases.

`stepWith` exposes the bass routing and saturation functions as pure parameters.
The proofs connect its actual output to a memoryless observation of the actual
LR state; they do not assume a separate tangent processor is equivalent.

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
an independent cutoff. The generated `Preparation<3>::Prepare` evaluates a packed three-cutoff
bank with Math4096. The wrapper broadcasts those records into stereo or quad
audio sections in a local bank and calls the shared by-value `InstallPrepared`.
Only the packed three-cutoff preparation is cached; there is no persistent
audio-width prepared bank duplicating the installed coefficients and gains.
Cutoff preparation is cached while the clamped cutoffs remain unchanged.
Installation and response capture run when
cutoffs or gains change, preserving histories; fixed controls do not recopy the bank.
The constant LR damping folds through the constexpr Math4096 table entry.
Those preparation lanes are distinct from the audio lanes used by mono bass.
The preparer, installer, audio processor, reset, snapshot and power response
live in `MultibandSaturator4BandCoreGeneric.hpp`, using the existing generator.

## Validation

`DSP.Tests.MultibandSaturator`, run by the existing `dsp_filter_smoke` executable,
checks native Float and Float32 processing for 1, 2, 3, 4, 5, 7 and 8 bands.
Prepared coefficients and snapshots are compared with the existing LR tuner,
including nonzero histories. Tests check reset, coefficient updates, muted
gains, observations and the final sum against independently evaluated rational
saturation. Additional tests cover independent audio lanes, stereo/quad mono
bass, and packed three-cutoff preparation.

Small-signal execution checks independently demodulate the actual nonlinear
Float output at four frequencies for flat, unequal and muted band gains, and
compare measured power with the new kernel. A stereo anti-phase check verifies
mono-bass cancellation. These numerical checks complement the real-number proofs;
they do not establish floating-point error bounds.

## Proved response

For independent channels, let `B_i(f)` be the existing splitter's exact complex
band response, `G_i` the linear band gain, and `M` the linear master gain. Then

```
H(f) = M * sum_i G_i * B_i(f).
```

Both the band and master saturations have derivative one at zero. The shared
LR phase compensation gives `B_i = A*w_i`, with `|A|=1`, `w_i >= 0`, and
`sum_i w_i=1`. Consequently

```
|H(f)|^2 = M^2 * (sum_i G_i*w_i(f))^2.
```

Each node uses `low = (g*cos(pi*f))^4`, `high = sin(pi*f)^4`, and the normalized
weights `low/(low+high)` and `high/(low+high)`. A band's weight is their product
along its tree path. [Response.lean](Response.lean) evaluates this formula using
the existing `ResponseFrequency` cache, generic arithmetic and the captured
snapshot. It requires neither new trig calls, complex arithmetic nor square roots.
The formula includes all cross terms in the coherent band sum; it is not a sum
of individual band powers. Equal band gains give a flat magnitude, although the
common all-pass phase remains.

The convergence statement is about the **actual nonlinear processor** at fixed
valid coefficients, with reset histories. It first takes the long-time coherent
average of the output at the input frequency, then divides by input amplitude,
then takes that amplitude to zero from above. The inner time limit exists: the
stable LR tree approaches its sinusoidal mode, and the saturations approach a
continuous function of the same circle phase. Circle averages cover both rational
and irrational digital frequencies. This measures the complex fundamental,
not RMS energy of all harmonics, and does not assert linearity at finite drive.

Cutoffs must lie strictly between zero and half the processing sample rate at
every tree node. Band and master gains may be any finite real values, including
zero. The coherent theorem excludes DC and Nyquist because its factor-of-two
normalization is specific to nondegenerate real sinusoids. The power identity
itself is valid at those endpoints too. Coefficients are held fixed throughout;
parameter modulation and floating-point/Math4096 approximation errors are not
part of this theorem. The proof interprets saturation as exact real tanh; the
native smoke tests use the existing rational approximation.

### Mono bass

For input channel amplitudes `a_j` (complex, retaining relative phase), the
response at output channel `l` is

```
M_l * (G_0l * sum_j B_0j*a_j / channels
       + sum_(i > 0) G_il * B_il*a_l).
```

This is a matrix response across channels. There is no single input-independent
scalar curve for arbitrary stereo input in this mode. With identical channel
tuning, a common input mode gives the scalar response above; any zero-mean input
mode, such as opposite-phase stereo, removes the bass contribution. The theorem
also permits lane-specific tuning and gains. The scalar magnitude kernel is not
claimed to describe every mono-bass input mode.

## Theorem inventory and shared proof structure

| Result | What it establishes |
| --- | --- |
| `prepared_smallSignal` | Actual scalar processing has the stated time-first response from the prepared snapshot. |
| `prepared_monoSmallSignal` | Actual mono-bass processing has the coupled response for any vector of complex channel amplitudes. |
| `prepared_magnitudeSquared` | The prepared power kernel equals the squared magnitude of that same scalar response. |
| `Snapshot.equal_gain_flat` | Equal band gains produce constant power `(M*G)^2`. |
| `Snapshot.monoTransfer_common`, `monoTransfer_zeroMean` | Common and differential channel modes behave as described above. |
| `output_eq`, `monoBass_output_eq` | The existing processor output is exactly the analyzed observation of the LR bands. |
| LR `install_prepare`, `prepare_snapshot` | The prepared coefficient path and snapshot equal the existing proved tuner. |
| LR `bandTransfer_factor`, `bandWeight_nonneg`, `sum_bandWeight` | Common all-pass factor and nonnegative partition of unity. |
| LR `computeCoefficients_settles_band` | Each actual band settles to its complex mode, even with a vanishing input transient. |
| SVF `spectrum_lt_one`, `settles_readout` | Actual history recurrence is strictly stable at positive tuning/damping and settles to its low-pass/all-pass response. |

The reusable pieces are deliberately separate from the multiband filter:

- [LinearTracking](../../Analysis/LinearTracking.lean) proves stable linear
  recurrences reject vanishing input errors. [Tracking](../../ResponseMeasurement/Tracking.lean)
  packages that contract so projections, cascades and differences compose.
  The LR proof follows the existing processing tree, including sibling phase compensation.
- [SaturatingSum](../../Nonlinear/SaturatingSum.lean) proves the derivative and
  Lipschitz bound of band gains, saturations, sum and master saturation.
- [StaticObservation](../../ResponseMeasurement/StaticObservation.lean) turns
  a settled linear bank and differentiable memoryless observation into the
  time-first small-signal theorem. Mono bass inserts a linear routing map before
  this observation; it does not require a second nonlinear convergence argument.
- The tanh calculus facts, phase rotation and digital-frequency/Fourier identity
  are shared with the conservative ladder and normalized saturator. Their old
  private copies were removed; the ladder's feedback convergence proof remains.

`DSP.Audit` checks these public results and all DSP dependencies for admissions
or additional axioms. The generated entry point is connected to the proved independent and mono-bass
steps by `step4Band_independent` and `step4Band_mono`; `install4Band_prepare`
connects prepared installation to the proved coefficient/snapshot pair.

## C++ integration and validation

`Core::Process` takes an input vector plus a scalar mono-bass flag and writes
`m_output`, containing four before/after observations, the master observation,
and the separate saturated bass. A shared `ChannelMean` operation interprets
mean-and-broadcast as the original finite sum in Lean and `MeanBroadcast` in C++.
The C++ implementation uses a fixed index pack, retains the zero-seeded left-to-right
sum, and ignores the padding lane at width three. Processing has no lane loops.

The app wrapper owns the meters, readers and sampled writer. It records every
observation once per engine sample; `RecordSaturated` preserves the old floor,
RMS/peak accumulation, reduction and host-frame smoothing. Existing
`ProcessAndSaturate` callers delegate their bookkeeping to this same function.
The single response curve captures the shared coefficients and uses the generated
power kernel for common-channel input; meters retain their individual audio lanes.

The differential codegen test checks every state and output field plus the power
response against native Lean at widths 1–4, in float and double, with distinct channel coefficients,
nonzero histories, muted bands and changes to the mono-bass flag. Core tests
check packed tuning/routing against scalar LR processing, external metering
against the existing API, and the UI power against measured small common-channel
sinusoids. These tests validate the compiler boundary; the compiler is not itself
formally verified.
