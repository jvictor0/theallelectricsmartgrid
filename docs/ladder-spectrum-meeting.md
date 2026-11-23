# How much noise history does the ladder spectrum remember?

The detector-on binary32 ladder can lose its earlier history bit for bit at feedback levels well above the small-gain bound. Once two copies have identical audio memories and analyzer histories, the same future input produces the same entire spectrum. The experiments use the production filter, normalized band-limited noise at 192 kHz, and the dual-rate analyzer. Whole-curve RMS integrates squared displayed dB error over log frequency, 20 Hz to 20 kHz, with the −60 dB clamp applied after interpolation.

![A 515 Hz whole-spectrum comparison and fresh-noise meeting times](../build/results/ladder-noise-grid/screen-v3/lagged-fresh-extended/whole-spectrum.png)

## A solvable version at high feedback

Start the filter and analyzer at zero on a record boundary. At every boundary independently choose a 9.5573-second noise record uniformly from a fixed bank, with replacement. This input produces infinitely many different sequences. The complete transition graph reachable from reset can sometimes be enumerated.

| Cutoff | Drive | Resonance | K s³ | Bank size | Reachable states including reset | Records sufficient for every pair and every future sequence | Frozen fit error, whole-curve RMS |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 515.43 Hz | 71.46% | 93.13% | 29.38 | 2 | 8 | 3 | 0.811 dB |
| 515.43 Hz | 71.46% | 93.13% | 29.38 | 4 | 12 | 3 | 0.788 dB |
| 358.52 Hz | 49.59% | 92.72% | 7.11 | 2 | 43 | 10 | 0.804 dB |
| 18,170.35 Hz | 91.67% | 79.17–100% | 36.99 | 4 | 5 | 1 | ≤ 0.090 dB throughout |

At 249.70 Hz the two-record search discovered 256 states before it finished examining the 176th. The state budget ran out. A separate construction for the full fresh-noise law now proves that every reset-reachable history has the same long-run spectrum at each fixed analyzer alignment there. [Three-record construction](ladder-response-accuracy.md#a-synchronizing-word-at-24970-hz)

A graph node records the bits of the four Euler stages, the four SVF integrators and one power memory in each detector, the wet FIR history, and the previous record identity for the dry FIR history. Node equality compares the bytes. Every outgoing edge runs the next record through the stock C++ step. The checker starts with every unequal pair of reachable states, advances both sides with every possible common record, and discards pairs that agree. At 515 Hz with four records the successive counts of still-possible unequal pairs are 66, 18, 1, 0. At 359 Hz with two records they are 903, 445, 237, 128, 71, 35, 17, 6, 3, 1, 0. These memory horizons also hold for unequal probabilities of choosing the records.

For the uniform law, count the sequences leading to each state at the certified depth. One more transition leaves the distribution unchanged. These integer counts supply exact weights for the entire stationary power vector. A second calculation propagates outward-rounded integer intervals for forward expected powers. The checker verifies by rational arithmetic that all 240 weighted sums, treating the stored binary64 wet and dry FFT rewards as data, lie in those intervals. The resulting band-ratio brackets have width below 4 × 10⁻¹³ dB in these examples. That width measures the stationary averaging calculation. The FFTs and conversion to displayed dB use floating point. [Graph builder](../scripts/ladder_noise_grid/response_fit/record_bank.py) · [Exact word-count checker](../scripts/ladder_noise_grid/response_fit/record_bank_check.py) · [Witnesses](../build/results/ladder-noise-grid/screen-v3/record-banks)

The high-cutoff interval uses the previously checked saturation of the prepared feedback cap. At fixed cutoff and drive all its resonance settings produce the identical waveform. Every packed fit knot there is above the floor. Between adjacent resonance knots the signed error is affine, so its RMS norm is bounded by the larger endpoint value. The largest endpoint error is 0.08948 dB rounded upward. [Cap analysis](ladder-noise-response.md#an-exact-limitation-of-the-frozen-model)

## Fresh noise

Start both copies from reset at the same uniformly sampled phase of a new noise record. Run both to its end. Give one copy one extra independent full record, then give both copies the same newly generated records. The offset between the FFT epoch grid and the noise boundary stays fixed for the pair. At a boundary compare every feedback memory, the pending analyzer epoch prefix and the FIR tail. Once these match, every subsequent readout from the two copies agrees exactly.

| Cutoff | Independent pairs | Last meeting, shared records | Measured correction to the first record, whole-curve RMS | Frozen fit error after correction |
| ---: | ---: | ---: | ---: | ---: |
| 249.70 Hz | 24 | 17 | 0.097 dB | 0.838 dB |
| 515.43 Hz | 24 | 2 | 0.035 dB | 0.787 dB |
| 358.52 Hz | 24 | 6 | 0.035 dB | 0.789 dB |

The first pass stopped at 12 records, with two pairs unresolved at 249.70 Hz. Both met by record 17. The estimates differ from the independent 64-record late-window measurements by 0.104, 0.054 and 0.048 dB respectively. The two halves of the 24-trial estimate differ by 0.205, 0.132 and 0.092 dB. The 64-record comparison used longer uninterrupted normalized records, so that comparison also includes differences in the acquisition protocol. [Measurements](../build/results/ladder-noise-grid/screen-v3/lagged-fresh-extended/summary.json)

A separate experiment drew 96 independent combinations of knobs and histories. Drive and resonance were uniform over 49–72% and 92–98%, and cutoff was uniform in log frequency from 240 to 530 Hz. Sampled K s³ ran from 6.97 to 33.08. All pairs met by record 14. Under independent random draws the one-sided exact 95% binomial upper limit for a pair drawn from this joint distribution to remain apart at the prespecified 24-record endpoint is 3.073%. A small region could behave differently. These experiments use pseudorandom draws. [Range experiment](../build/results/ladder-noise-grid/screen-v3/lagged-range/summary.json)

## What the proof needs from fresh noise

Existence of the realized long-run power average is now proved for the full fresh-record law at every surveyed setting. Coalescence can still give a quantitative history or warm-up error bound. [Single-run convergence and limiting variance](ladder-response-accuracy.md#which-curve-exists)

Let a(j) be the vector of expected wet and dry band powers in a record at age j, measured from the first fresh boundary after reset. In the paired experiment the older and younger copies have the marginals for a(j+1) and a(j). Subtract their powers on the shared input. The expectation of that difference is exactly a(j+1) − a(j). Adding those differences to the first readout through age m−1 therefore estimates a(m). When two copies meet, every later correction for that pair is zero. Individual corrections can be negative. Pool powers before taking ratios, logs or applying the floor.

For the finite digital filter including analyzer memory and record phase, consider the Markov chain of reachable pairs whose memories still differ. Remove probability mass when the two sides agree. Suppose from **each reachable unequal pair** there is **some positive-probability future continuation** that makes it agree. Each pair may need a different continuation. Finiteness gives a common block length L and a q < 1 such that survival after k blocks has probability at most qᵏ. Monotonicity and the Markov property give the bound. A closed recurrent class of reachable unequal pairs would defeat this particular argument.

If D bounds the diameter of a complete power readout in the chosen norm, the discrepancy in its expectation is at most D times the probability that the pair still differs. Summing over future blocks gives a whole-vector warm-up allowance of L D qᵏ/(1−q) after kL records. Summability also makes the expected infinite correction equal the limiting power vector. Bounds on the signed power differences can be tighter. The same reset occupation weights apply to every band. Lean also factors all quadratic waveform analyzers through one uncentered second-moment matrix. [Lean theorems](../formal/DSP/Filters/NaiveReuppingLadder/NoiseResponse/JointSpectrum.lean)

The missing dynamics argument for the full noise law is reachability of agreement from all relevant pairs, or a summable bound directly on their power differences. The measurements leave this open for untested pairs. A useful physical error in dB also needs quantitative tail and sampling bounds and control of variation between knob samples. The exact finite-bank calculation fixes the input law and initial record alignment.

## Reproduction

Run from the worktree root. The Python experiments use NumPy and SciPy. Drawing uses Matplotlib. The graph files retain every edge, wet and dry reward, feedback state, FIR tail and integer-propagation power bracket. The fresh trial files retain the vector corrections and meeting times.

```sh
scripts/lean.sh build DSP.Filters.NaiveReuppingLadder.NoiseResponse
mkdir -p build/tools/ladder-noise-grid
clang++ -std=c++17 -O3 -DNDEBUG -fno-fast-math -ffp-contract=off -fPIC -shared \
    -I scripts/ladder_noise_grid -I private/src -I formal/DSP/generated -I build/core/RelWithDebInfo/generated \
    scripts/ladder_noise_grid/response_fit/Cycle.cpp -o build/tools/ladder-noise-grid/cycle.so
root=build/results/ladder-noise-grid
out="$root/screen-v3"
src=scripts/ladder_noise_grid/response_fit
python3 "$src/record_bank.py" "$root" "$out/record-banks/two"
python3 "$src/record_bank.py" "$root" "$out/record-banks/four" --settings 1 4 --bank 4
python3 "$src/record_bank_check.py" "$out"/record-banks/*/*.npz
python3 "$src/lagged_records.py" "$root" "$out/lagged-fresh-extended" --depth 48
python3 "$src/lagged_range.py" "$out/lagged-range"
python3 "$src/draw_lagged.py" "$out"
```
