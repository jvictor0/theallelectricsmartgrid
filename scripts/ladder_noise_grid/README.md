# Detector-on noise spectrum survey

The response is the ratio of the expected output power in a frequency band to the expected dry input power in that band, under the protocol below. Compute the ratio after averaging linear powers. The experiment measures finite-time responses from reset.

## Overnight run on Studio

The run directory is `/Users/joyo/benchmarks/ladder-noise-grid-v1/results/dense-73x73x361` on `studio.local`. Read `progress.json` there for live status. The runner writes, syncs and renames each completed tile before updating progress. `run.lock` prevents competing writers, and restarting the same command skips completed tiles. Keep the source and native libraries recorded in the manifest to resume.

The grid contains 73 linearly spaced drive and resonance knob positions apiece and 361 logarithmically spaced cutoffs from 20 Hz to 20 kHz, including endpoints. Probe every configuration with 16 noise realizations, all ladder stages and both envelope detectors starting at zero. Each realization has independently seeded Gaussian 48 kHz host samples. Remove DC and Nyquist, Fourier-interpolate to the 192 kHz engine and normalize the record to RMS 0.176777. The same tile's points use the same noises. Tiles receive independent seeds and points are shuffled within nested lattices. The dense run and pilot use disjoint seed ranges.

Discard 4.096 seconds, then measure for 5.461333 seconds. The primary analysis decimates to 48 kHz with a 257-tap FIR and takes 4,096-sample periodic-Hann FFTs. Each of eight measurement blocks contains 15 overlapping frames, for 120 host-rate frames per realization. For bass, decimate again to 6 kHz with a 129-tap FIR and take one 4,096-sample frame per block. Both filters retain overlap at block boundaries, and the dry reference takes the identical path. Bin spacing is 11.71875 Hz and 1.46484375 Hz, respectively. The display-band diagnostics integrate fractional-bin overlaps in 1/12-octave bands (final band clipped at 20 kHz), using the bass spectrum below 640 Hz. Narrow features below the FFT resolution are window-broadened.

Each `tile-*.npz` retains configuration indices, two independent eight-seed means of the output PSD in linear units per Hz, the corresponding dry spectra, per-band standard deviation across individual trial means, and per-band means from the first and second halves of the measurement interval. The ordering of bins, units, accumulation and seed rules are in `manifest.json`. These data permit new display-band choices and checks on fitting error without retaining every audio sample. Standard deviations describe variation across noise realizations after reset.

## Pilot and interpolation

The longer 10×10×10 pilot lives in the sibling `pilot-long` directory. It retains band powers per seed, configuration and chronological block for 16.384 seconds. The current pilot, warm-up and holdout diagnostics score both curves at the drawing floor of −60 dB. With `check_startup.py --production`, the identical suffix from reset and after a 6.827-second head start differed by 0.0015 dB at the 95th percentile, 0.127 dB at the 99th percentile and 0.982 dB maximum across all 120,000 configuration/bands. These measurements use the overnight 4.096-second warm-up. Earlier archived pilot JSON used a −40 dB eligibility threshold. The recomputed results accompany the fitted model. [Visible-response analysis, proofs and model](../../docs/ladder-noise-response.md)

`holdout.py` retains every other knot along each axis on a nested lattice and predicts every omitted point using trilinear interpolation in dB, with cutoff measured in octaves. It reports independent seed-half disagreement alongside interpolation error. The 10-to-19 result lives in `holdout-10-to-19.json`. After acquisition finishes, `holdout-full.json` compares a 37×37×181 grid with withheld points from the 73×73×361 measurements. The complete data also support other fits.

## Reproduction

Build the Metal bridge and stock coefficient/reference library in the worktree. NumPy is the only Python dependency. Validation compares GPU waveforms against the actual float C++ ladder and both spectral paths against CPU FIRs and FFTs across consecutive blocks.

```sh
make -f scripts/ladder_noise_grid/Makefile noise-grid-build
python3 scripts/ladder_noise_grid/survey.py --tools build/tools/ladder-noise-grid --validate
```

Studio uses `src/` for the research source and `tools/` for the two libraries. From `/Users/joyo/benchmarks/ladder-noise-grid-v1`, the acquisition command is

```sh
caffeinate -i /Users/joyo/benchmarks/ladder-energy-grid/build/tools/ladder-sql-env/bin/python -u src/survey.py --grid 73 --cutoffs 361 --trials 16 --warm 6 --measure 8 --tile 256 --seed 20263122 --out results/dense-73x73x361
```

## Cutoff refinement on Studio

The compact run `results/cutoff-bands-73x73x2881` fills the seven interior points of each of the 360 original log-cutoff intervals. The 73 drive and 73 resonance positions stay on the original grid. The old archive supplies every eighth cutoff. Refinement visits the midpoint of every interval first, then the two quarter positions and finally the four eighth positions. This gives complete 2×, 4× and 8× grids in sequence. Progress counts the 13,429,080 new configurations.

Each interior point reuses the 16 noise records from the tile that measured its left-hand cutoff. This pairs measurements across nearby cutoffs and preserves the two independent eight-trial estimates within each setting. Knots sharing records have correlated sampling errors. The seven output arrays include the indices, wet and dry powers for each seed half integrated into 120 bands, and all the original per-band dispersion and temporal diagnostics. Band gains use the ratio of summed wet and dry powers. Every launch recomputes the first original tile and verifies the serialized arrays against the original FFTs and diagnostics, byte for byte.

From the Studio noise project, use the survey Python environment and the installed source snapshot.

```sh
caffeinate -i "$PYTHON" -u src/refinement-v1/refine_survey.py \
    --tools tools --parent results/dense-73x73x361 --out results/cutoff-bands-73x73x2881
nice -n 10 "$PYTHON" -u src/refinement-v1/refined_certificate.py \
    --grid results/cutoff-bands-73x73x2881 --base-fit fits/whole-curve-v1 \
    --model fits/screen-v3/display-model.npz --factor 1 2 4 8 --wait
```

The checker reproduces the old integer certificate at 1×. At each finer stage it verifies complete, unique vertex coverage, computes raw band dB, and checks the frozen fit against the interpolated measured curves with the existing −60 dB screen clamp. The JSON reports whole-curve RMS bounds by cutoff interval as well as the global, worst-curve and pointwise bounds. A geometric cutoff interval is subdivided in log frequency. The model's cutoff coefficients use the same coordinates on each grid. The checker stores only the small reports on disk and assembles its temporary quantized table in memory.
