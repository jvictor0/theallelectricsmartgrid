# iPad SynthRig regression investigation, November 18–19, 2026

The largest late regression was the float SIMD `Exp2` fallback in `347601f`.
Cross-swapping that implementation reproduced the cost in both directions.
The follow-up that restored the bounded fast path landed as `757c146`.
Earlier regressions were associated with randomized LFO startup, independent
random pairs, and audio-tap routing. The encoder bins tested afterward showed
no clear full-synth gain against `757c146`.

[Per-render measurements and diagnostic counts](ipad-synthrig-2026-11-19.csv)

## Workloads and units

The iPad Air M3 (iPad15,5, iPadOS 27.2.1) ran a headless Release SynthRig at
48 kHz host rate. The historical bisect includes 48 kHz and 192 kHz engine
builds. The later comparisons all use 192 kHz. Each full run starts the
sequencer, warms up for 0.5 seconds, and measures two seconds of rendering in
1 ms slices. The swept case drives a one-second triangle scene blend. The
fixed cases hold blend at zero. Times are median calling-thread CPU
milliseconds per second of audio. Bracketed values give the observed minimum
and maximum. This is offline throughput with the regular app stopped.

- The [shared modulation-stress patch](../fixtures/modulation-stress.json)
  has SHA-256 `e7fa12f46b6105ebb0d158488169d299ba0fb0bb56ca7df60c7f9628a7594731`.
  Historical group layouts can bind its serialized slots to different sources.
- Init resets to the native defaults and runs the transport. The default patch
  renders silence. The 4:28 jam case uses a reconstructed patch, loaded at
  blend zero and played from the sequencer start. The reconstruction has
  SHA-256 `101bc1a8272fd557a4268b37f5198ad1280a89b3e2ba94b00b2add0840f7dd49`.
- The focused sweep adds one shared gesture and two opposite-endpoint
  full-depth routes to each of 96 stress roots. Its fixture has SHA-256
  `e01936fddfd9f494372e6db3469ba4603289aadad8b850f376a0fd10ec795f14`.

The CSV groups results by experiment, variant, fixture, and workload.
`launch` is a sequence number within each experiment. `git_dirty` records
the app's embedded build flag. A dirty historical revision identifies a base
for a diagnostic overlay. The original clean bisect uses two-second renders
at nominal thermal state, with Low Power Mode off and the benchmark in the
foreground. The later experiments use the same thermal and power checks.
The `counter` rows are from instrumented builds. Divide their operation totals
by `host_samples` for per-frame values. The isolated depth refresh is
normalized to 48,000 updates of the prepared route array. A blank
`playing_verified` field means the older harness did not emit that field.

## SIMD Exp2 regression

| 192 kHz build | Fixed | Sweep |
| --- | ---: | ---: |
| `d70d0d6`, parent of the filter landing | 862.6 [858.5–881.4] | 921.7 [918.0–925.2] |
| `d70d0d6` with the `347601f` Exp2 | 1025.4 [978.8–1038.4] | 1179.0 [1173.4–1195.8] |
| `347601f`, reusable delay filters | 1043.2 [1020.9–1057.1] | 1192.3 [1177.3–1202.0] |
| `347601f` with the `d70d0d6` Exp2 | 879.4 [869.9–917.2] | 953.2 [921.5–960.0] |
| Debug assertion and bounded fast path follow-up | 848.9 [823.6–869.4] | 912.9 [903.1–955.0] |

The stock filter commit added 20.9% fixed and 29.4% swept over its parent.
The source swaps change only the SIMD Exp2 implementation. The follow-up
retains the range assertion in debug builds and moves the extreme comb tests
to double precision. The checked-in generated delay filters ship with that
follow-up.

## Earlier fixed-scene increase

The initial bisect measured 690.9 fixed and 809.2 swept at `f333621`, and
776.7 fixed and 857.4 swept at `e7afa97`, all at 192 kHz. Repeating the steps
located two increases. Engine-rate scope history `5425d06` moved fixed/swept
medians from 691.9/813.2 to 711.5/832.4. Around the randomized LFO startup
landing `c2feefe`, fixed rose from 735.0 to 778.2, while swept moved from
849.0 to 847.6.

The immediate pre-LFO snapshot and `c2feefe` both visited 333 roots per host
frame. Fixed prepared updates rose from 131.83 to 183.12 per frame, about
2.46 million additional updates per second. Both sweep variants already
evaluated all 333 roots and 658 routes per frame. The benchmark covers
short-window startup activity.

## Modulator manager, random pairs, and audio taps

| Revision | Stage | Fixed | Sweep | Running vector downsamplers |
| --- | --- | ---: | ---: | ---: |
| `e7afa97` | Pre-stack | 763.1 | 847.9 | 75 |
| `7c74890` diagnostic | Central manager, five groups | 773.4 | 853.5 | 50 |
| `0822364` | Independent random pairs, 20 groups | 804.2 | 899.4 | 14 |
| `c7eaf6b` | Raw audio taps and mixer RMS | 873.1 | 930.5 | 33 |
| `9dfaa4e` | Move physical envelope scope | 853.3 | 930.2 | 33 |
| `2bdba76` | Vectorize random interpolation | 851.2 | 910.8 | 33 |
| `17a29b8` | Share gesture state | 832.4 | 892.7 | 33 |
| `c6c9574` | Deduplicate change checks and fuse clamps | 843.1 | 906.1 | 33 |
| `d0c9461` | End of landing | 826.1 | 894.1 | 33 |

The `7c74890` diagnostic substitutes `DelayPhaseModulation` for the stale
`DelayWiden` group binding. The whole landing raised the medians by 8.3%
fixed and 5.5% swept. It reduced active downsampling from 75 to 33 vectors.
From `0822364` onward, modulation noise is generated at host rate and skips
decimation. The seven mixer RMS sources also publish at host rate. The 19
filters added by `c7eaf6b` serve 12 voice vectors and seven raw mixer taps.

In the audio-tap ablation on `c7eaf6b`, disabling RMS alone produced no
repeatable gain. With RMS off, disconnecting tap bindings while leaving all
33 filters running lowered the original stress medians from 862.1/923.3 to
815.7/907.2 fixed/swept. This changes the gather, dirty-bit propagation,
and routed signal together. In the later counter run, the bindings took
fixed prepared updates from 144.07 to 203.52 per host frame. Both sweep
builds still evaluated all 333 roots and 656 routes per frame. Bypassing the
19 new filters on the tap-neutral patch saved about 20 fixed CPU ms per
audio second, with overlapping sweep ranges.

Disabling 22 engine-rate page random generators on the random-neutral
fixture recovered 49.1 fixed and 21.3 swept CPU ms per audio second. Applying
the later gesture, random-interpolation and deduplication work on both sides
still left a 69.6 fixed and 35.1 swept gap across the tap landing. The
tap-free root-clamp bypass showed overlapping ranges and no lower median.

## Encoder experiments

The bounded flat evaluator was compared with `757c146`. Its focused sweep
improved 3.7% (972.8 to 937.0) with the original curve and layout. The four
standard cases ranged from 0.7% faster to 1.7% slower at the median.

The polynomial and contiguous route arrays were measured separately and
together over that flat evaluator. The polynomial approximates
`(9^u - 1) / 8` on `[0,1]` with exact endpoints. The array layout stores
source indices, left positions, right positions, and curved depths in four
parallel arrays. The isolated refresh updated all 656 prepared routes on
each iteration.

| Workload | Flat baseline | Polynomial | Contiguous arrays | Both |
| --- | ---: | ---: | ---: | ---: |
| Isolated depth refresh | 31.2 | 19.7 | 23.5 | 13.3 |
| Full mod-stress sweep | 951.6 | 935.8 | 943.1 | 921.4 |
| Full focused sweep | 941.0 | 936.1 | 934.8 | 921.8 |

The combined prototype cut isolated refresh CPU by 57.4%, full stress-sweep
CPU by 3.2%, and focused-sweep CPU by 2.0%. The fixed stress, init and jam
medians moved by less than 1%.

The final binning experiment specialized direct routes by zero or one gesture
and zero, one, two or many modulators across the scene pair. It used the
original depth curve and route layout. The iPad reported 328 stress roots
using the one-route kernel at the held endpoint and the two-route kernel
while sweeping. Its direct comparison with clean `757c146` used four
measurements per build and workload, split across both build orders.

| Workload | `757c146` | Binned evaluator | Change |
| --- | ---: | ---: | ---: |
| Stress fixed | 872.8 [848.1–904.9] | 885.9 [869.1–908.5] | +1.5% |
| Stress sweep | 930.2 [921.4–939.5] | 934.9 [924.9–959.5] | +0.5% |
| Init playing | 648.1 [640.4–648.9] | 640.6 [638.4–646.8] | −1.2% |
| 4:28 jam playing | 668.2 [656.9–669.8] | 657.8 [657.1–670.8] | −1.6% |

The ranges overlap in all four workloads and the sweep changes direction
between build orders. The binned evaluator remained experimental.
