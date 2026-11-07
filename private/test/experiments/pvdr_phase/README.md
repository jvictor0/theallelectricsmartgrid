# Neutral feedback phase experiment

This opt-in experiment compares conventional delay, the original PVDR baseline,
and the production three-state Schmitt phase reset. It also measures a
threshold-only ablation and an always-copy-analysis-phase reconstruction control.
The historical executable and data key `current` refer to PVDR before Schmitt
snapping was implemented. The harness does not modify production source files.

## Run

From the checkout root:

```sh
make core-test CMAKE_ARGS=-DSMARTGRID_PVDR_EXPERIMENT=ON
/usr/bin/python3 -m venv build/tools/pvdr-plot
build/tools/pvdr-plot/bin/pip install numpy==2.0.2 matplotlib==3.9.4 scipy==1.13.1
MPLCONFIGDIR=build/tools/pvdr-plot/cache PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 \
  build/tools/pvdr-plot/bin/python private/test/experiments/pvdr_phase/run.py \
  --binaries build/core/RelWithDebInfo --output build/results/pvdr-phase/full
MPLCONFIGDIR=build/tools/pvdr-plot/cache PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 \
  build/tools/pvdr-plot/bin/python private/test/experiments/pvdr_phase/finish.py \
  --binaries build/core/RelWithDebInfo --output build/results/pvdr-phase/full
```

The recorded run used macOS Python 3.9 and the package versions above. `--quick`
selects five pilot cases in `run.py`; use a different output directory for them.
`finish.py --reuse` rebuilds the findings from completed extra-seed metrics and
plots without repeating those renders. Keep unique results under `build/results`
and preserve them outside the checkout before worktree cleanup.

## Method

- The actual `Resynthesizer` FFT, phase graph, phase fixup, inverse FFT, and OLA
  run at 48 kHz using the repository's existing passthrough-rate test headers.
  `N = 4096`, `H = 1024`. The driver exposes the neutral center-oscillator path
  between analysis and synthesis for initialization and instrumentation.
- Source and synthesis positions advance by one host sample per output sample.
  The reference analysis window is exactly one hop behind the current window.
  The source position compensates the OLA launch's one-sample latency.
  Every analysis sample is asserted to have already been written.
- Each version closes its own loop: `tape[n] = dry[n] + feedback * wet[n]`.
  Conventional output is `tape[n - delay]`. There is no clipping, saturation,
  filtering, shifting, detune, time warp, or magnitude slew. Above-unity feedback
  is rendered for a finite duration, not stabilized.
- The Schmitt generated header is byte-for-byte production `Resynthesis.hpp`.
  `variant.py` reconstructs the original −160 dB/no-reset and −60 dB/no-reset
  controls using checked, narrowly scoped substitutions to generated headers.
  Changes to source contracts fail
  generation rather than silently applying an approximate patch.
- Schmitt state is per bin and persists across synthesis launches. Low becomes
  Triggered above `maxMagnitude * 0.001`; Triggered becomes High next frame;
  High becomes Low below `maxMagnitude * 10^(-72/20)`. Exact silence is Low.
  The maximum includes current and reference frames, as in production. Low bins
  retain random phase increments; a triggered root is assigned its current
  analysis phase exactly; follower propagation is unchanged. There is no
  growth retrigger or transient detector. Between −60 and −72 dB, High bins
  remain tracked.
- Aligned initialization copies reference analysis phases into synthesis state
  at the first nonzero input window. Random initialization adds independent
  uniform offsets in `[-0.5, 0.5)` cycles at that same instant. Low-bin random
  increments use a separate reproducibly seeded generator.
- DC and Nyquist omission are unchanged. The always-copy-phase control measures
  the resulting reconstruction and floating-point floor independently of phase
  propagation.

## Coverage and output

The main matrix has 144 single-note cases: 750 Hz on-bin sine, 997 Hz off-bin
sine, and a 220 Hz eight-harmonic note; 160/640 ms duration with 20/80 ms cosine
attack and release; nominal delay/note ratios 0.75/1/1.5; feedback 0/0.5/0.9/1.2;
and near-constructive/destructive carrier delay phase. Delay is an integer
number of samples chosen near the requested ratio and phase. Twelve further
cases use four repeated or alternating-polarity 997 Hz notes.

Every main case runs both phase initializations for current, Schmitt, and the
threshold-only version, plus conventional and copy-phase controls. The main
seed is 104729. `finish.py` repeats the selected representative and worst cases
with seeds 17 and 65537.

`index.html` contains the report, audio controls, spectrograms, frequency detail,
phase evolution, and waveform overlays. Metrics are in CSV/JSON; exact cases
are in `cases.json`; selected raw float signals and per-grain diagnostics are
in NPZ files. Listening WAVs use a common gain per case; original amplitudes
remain in the NPZ files. The main spectrogram uses 4096-sample Hann windows;
frequency detail uses 16384 samples and consequently smears time over 341 ms.
No lag, phase, or gain is fitted when computing waveform error.

The contract checks exercise arbitrary frame phase changes and changing heap
order, as well as Schmitt hysteresis, one-frame triggering, and exact root
assignment. Core tests remain assertion-enabled RelWithDebInfo.

## Limits

This isolates neutral phase behavior. It does not test the production 4x rate
conversion, nonlinear QuadDelay processing, warp, shifts, unison, or a
continuously high bin whose phase has been disturbed after its trigger.
The renderer uses fresh instances per run. Production channel and full-reset
behavior is covered separately by `dsp_resynthesis.cpp`.
