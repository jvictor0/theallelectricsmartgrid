# PVDR phase reacquisition in a neutral feedback delay

The minimal −60/−72 dB Schmitt reset closely matched conventional delay in all
156 main configurations, with aligned or randomized initial phases. Maximum
relative waveform error was 0.266%. The current implementation tracked
conventional delay when it stayed phase-aligned, but lost alignment across
long silence gaps and retained randomized initial offsets.

The opt-in reproducible harness is in
[`private/test/experiments/pvdr_phase`](../../private/test/experiments/pvdr_phase/README.md).
Production `Resynthesis.hpp` was unchanged. The source baseline was
`c9f5e9ab2cd587535b0d8191690f8cb2fe4ced17`.

## Results

Error is `RMS(output - conventional) / RMS(conventional)` over the complete wet
output, with no phase, lag, or gain fitting. Percentages are amplitude ratios.

| Version | Initial phase | Median error | Worst error | Cases above 1% |
| --- | --- | ---: | ---: | ---: |
| Copy analysis phase every frame | aligned | 0.00161% | 0.0111% | 0/156 |
| Current | aligned | 0.00180% | 170.86% | 18/156 |
| Current | random | 149.63% | 216.85% | 156/156 |
| Schmitt snap | aligned | 0.08783% | 0.26596% | 0/156 |
| Schmitt snap | random | 0.08782% | 0.26596% | 0/156 |
| −60 dB cutoff without snapping | aligned | 0.18246% | 156.15% | 36/156 |
| −60 dB cutoff without snapping | random | 150.04% | 448.56% | 156/156 |

Two further seeds on nine representative/worst configurations added 18 cases.
Schmitt stayed below 1% in both initial conditions throughout these checks;
the worst additional-seed error was 0.26429%.

All 18 aligned-current failures above 1% were 640 ms notes with delay near
960 ms and nonzero feedback. These leave silence gaps longer than the 85.3 ms
analysis window. Quiet-bin randomization breaks the phase invariant, and the
next echo inherits the resulting offset. Schmitt triggering reacquires phase.
For isolated echoes the magnitude spectrogram can remain essentially unchanged
while the waveform is substantially out of phase; waveform and phase plots
are necessary to see this failure.

For overlapping/repeated notes, randomized initialization also changes the
feedback spectrum. In the 997 Hz repeated-note example with a 250.25 ms delay
and gain 1.2, the late conventional and Schmitt spectra have dominant peaks
near 995 and 999 Hz. The current random-initial version instead concentrates
near 998 Hz. These are finite-window feedback-spectrum estimates, consistent
with changed feedback phase altering the comb resonances; the source oscillator
is still 997 Hz.

Schmitt is not exact reconstruction: the raised floor randomizes more weak
bins. Its residual error is greater than that of the already-aligned current
version in continuous regions. Merely raising the cutoff does not solve phase
reacquisition, as the threshold-only control demonstrates.

## Coverage and validation

- 750 Hz bin-centered sine, 997 Hz off-bin sine, and 220 Hz eight-harmonic note.
- 160/640 ms note lengths; 20/80 ms cosine attacks and releases.
- Nominal delay/note ratios 0.75/1/1.5, with constructive/destructive carrier
  phase and feedback 0/0.5/0.9/1.2.
- Repeated and alternating-polarity note sequences, plus both initial-phase
  conditions and additional random seeds.
- Neutral 48 kHz linear loops using actual C++ spectral code, N=4096/H=1024,
  through the existing passthrough-rate test configuration. Each version feeds
  back its own output. No saturation, clipping, filtering, slew, detune, shift,
  or warp. Latency is matched to the conventional reference.
- `make core-test CMAKE_ARGS=-DSMARTGRID_PVDR_EXPERIMENT=ON`: all five CTest
  targets passed, including 824 core cases, 32 passthrough-rate cases, and the
  three experiment contract checks. Four pre-existing core cases were skipped.

This experiment does not establish behavior under warp, production 4x rate
conversion, nonlinear processing, or a persistently High bin disturbed after
its trigger. It does not resolve every persistent-background counterexample.

The full local report, plots, listening files, raw selected renders, exact
configuration manifest, and machine-readable metrics are preserved in
[`build/results/pvdr-phase-20261107`](../../build/results/pvdr-phase-20261107/index.html).
These generated results are ignored by Git; the harness and this summary are
tracked.
