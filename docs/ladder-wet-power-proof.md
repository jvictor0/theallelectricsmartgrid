# A physical white-noise error bound for the Naive Reupping Ladder

The frozen 9,996,763-byte model has at most **4.41 dB whole-curve RMS error** throughout the knob box below. Above the band center at **1,243.561 Hz** the physical response draws at the −60 dB floor, and the fitted curve differs by at most **0.113 dB**. The pointwise upper bound over the entire spectrum is **14.78 dB**. These are conservative upper bounds. This calculation uses the recurrence, the noise law, and the frozen coefficients. It uses no wet observations from the survey.

| Coordinate | Range |
| --- | --- |
| Stage cutoff | 92.83178–100.23745 Hz, exactly coordinates 80 through 84 of the 360 log-cutoff intervals |
| Drive knob | 0 through 1/72, or 0–1.38889% |
| Resonance knob | 0 through 1/14,400, or 0–0.0069445% |
| Engine rate | 192 kHz |

The target is the ratio of expected long-run powers from zero reset. Measure the binary32 stock audio waveform using the mathematical dual-rate FIR, Hann FFT and band integration with the stored survey coefficients. Interpolate raw band-center dB linearly in log Hz, then clamp at −60 dB. The fit is evaluated as the real-valued function of the packed coefficients. Floating-point FFT implementation error and GUI rasterization are separate. The result holds for the two noise laws specified below. A finite sample mean over sixteen pseudorandom records has additional sampling error. [Numerical enclosure](../build/results/ladder-noise-grid/screen-v3/physical-stress/wet-power-certificate.json) · [Arithmetic checker](../scripts/ladder_noise_grid/response_fit/wet_power_certificate.py)

## Compare the driven ladder with a linear reference

Write a for the stored stage coefficient, K for the base feedback cap, g for drive, T for the stored normalization and s = g/T. Feed the same noise into four cascaded one-poles with gain s after each stage and zero feedback. Write z₁ through z₄ for the unsaturated linear stage outputs. The reference waveform is s z₄. With S the real clamped rational saturator divided by T, its Lipschitz constant is at most s and

```
|S(v) − s v| ≤ (8/27) g² s |v|³.
```

Inside the clip this follows by subtracting x(27+x²)/(27+9x²) from x. Beyond the upper clip, x−1 ≤ 8x³/27, and the lower clip follows by odd symmetry. The rational itself has slope between zero and one. For the real recurrence, a one-pole starting at zero has finite-record energy gain at most one by summing the convexity inequality for its squared next state. Delay also has gain at most one. If Rᵢ is the energy norm of S(zᵢ)−s zᵢ on the reference and U is that of z₄, the fourth-stage difference is at most

```
(s³ K U + s² R₁ + s R₂ + R₃) / (1 − K s³)
```

whenever K s³ < 1. The output difference is at most (1+K) times the sum of s times this bound and R₄, plus K s U. The detector enters solely through 0 ≤ kₜ ≤ K. Its dry power remains nonnegative under the EMA, and the positive floor keeps the denominator positive. Lean checks this pathwise result on the rational interpretation of the ladder's own step, for every input and record length. The population calculation below uses the same triangle inequalities in ensemble RMS, keeping separate allowances for each binary32 operation. [Lean proof](../formal/DSP/Filters/NaiveReuppingLadder/NoiseResponse/WetPower.lean)

## Input law and reference moments

Take M = 458,752 ideal independent Gaussian host samples. Project out DC and Nyquist, interpolate by four via the Fourier series and normalize the native record to squared RMS 1/32. The native length N is 1,835,008. One input law repeats a random record. The other concatenates fresh independent records. Choose the initial position uniformly within a record for stationarity. Round the resulting input samples once to binary32 before feeding the filter. The proof covers ideal normalized draws rounded to float. Identifying the finite NumPy generator with those draws would require a separate distributional argument.

The projected direction is uniform on the sphere of dimension M−2 in the sense of M−2 real coordinates. Its allowed complex Fourier coefficients have variance (1/32)/(M−2), equal for all nonzero signed host modes other than Nyquist, with diagonal Hermitian covariance. The sixth moment of a real linear form is at most 15 times its variance cubed. This follows from the spherical marginal moments, with the sixth Gaussian moment multiplied by (M−2)²/(M(M+2)). Independent record pieces retain the bound by expanding the sixth power, using symmetry, the same fourth-moment bound of three times variance squared, and independence. These estimates hold conditional on the starting phase.

For the cold linear reference the impulse response before stage m saturation is s^(m−1) times the m-fold convolution of the nonnegative one-pole impulse. Its squared gain integral, omitting s, is

```
Iₘ(a) = a^(2m) Σⱼ binom(m−1,j)² (1−a)^(2j) / (1−(1−a)²)^(2m−1),    0 ≤ j < m.
```

This follows by summing binom(t+m−1,m−1)² xᵗ, or multiplying the resulting series by (1−x)^(2m−1) for m = 1,2,3,4. The gain at every frequency is nondecreasing in a. For repeated records, fold the truncated nonnegative impulse into one period and dominate it entrywise by the folded full impulse. The average of its squared gain on the native DFT lattice exceeds the corresponding integral by at most 2/N, by monotone rectangle sums on the two half circles. For fresh records, split the impulse at record boundaries. Orthogonal projection bounds each contribution by its ordinary squared coefficient norm, and independence lets the variances add. Thus the variance of each zₘ at every sample is at most N/(32(M−2)) times s^(2m−2)(Iₘ(a)+2/N). This supplies the second and sixth moments even during startup.

## Audio arithmetic and the two live detectors

The coefficient audit visits every binary32 native cutoff in the padded 92–101 Hz interval and every binary32 gain in [0.25,0.261]. The box uses 0.0029 ≤ a ≤ 0.0034, 1.017 ≤ s ≤ 1.022, g ≤ 0.27 and T > 0.24. The upper knob request is 3.9(49^(1/14400)−1)/48 < 0.000022, below the proof cap of 0.000025 including rounding. The cutoff and gain margins cover conversion of the knob endpoints. [Coefficient audit](../scripts/ladder_noise_grid/response_fit/WetPowerBox.cpp) · [Audit counts](../build/results/ladder-noise-grid/screen-v3/physical-stress/wet-power-box.json)

The arithmetic model is ordinary binary32 round-to-nearest with fused multiply-add at the stock call sites. Write u = 2⁻²⁴. The checker allows an absolute slack τ = 2⁻⁸⁰ in addition to relative errors, including subnormals. Input magnitude is below 241 by the record energy bound. The audio states remain within 500 for the first stage and 5 for the other stages. At these endpoints the exact convex update points inward by more than the floating error. The saturation output has magnitude at most 1/T up to division rounding.

For the bandpass detectors the homogeneous two-state matrix is A = [[2a₁−1,−2a₂],[2a₂,1−2a₃]]. The audit tests positive definiteness of 0.999² P−Aᵀ P A with P = [[1,½],[½,1]], using positive margins exceeding 1e−10 on the determinant and 1e−8 on each diagonal. The matrix entries have magnitude below two. Binary64 error in these degree-two expressions and the determinant is below 1e−13. The input column has P-norm below 0.01. The operations computing diff, BP, LP and the two new memories introduce at most 50u times the sum of the old P-norm and the absolute input, plus τ, in P-norm. This bound follows by propagating absolute errors through the four multiplies/FMAs and two final doubled updates, using |a₁|<1, |a₂|<0.002, |a₃|<0.00001 and each coordinate ≤ √2 times its P-norm. Consequently the first SVF stays below 10⁵ in this norm, its BP output stays below 2×10⁵, and the second SVF stays below 10⁸. The squared outputs and both EMAs remain finite. Nonnegative EMA powers stay nonnegative by monotonicity of binary32 rounding. The largest possible excess divided by the floor is still finite. The detector therefore supplies nonnegative feedback bounded by the base cap throughout the record.

For the audio-stage update, forming 1−a, multiplying the old state, and the final FMA add local error bounded by 3.01u times the old magnitude plus 1.01u times a times the input magnitude, plus 6τ. For the saturator, the rounded result differs from the real S(v) using the stored g and T by at most 12u s |v| + 128τ. To see this, first charge rounding g v using the unnormalized clip's Lipschitz bound. At fixed rounded argument the numerator has at most three relative roundings, the positive denominator has at most two, and the two divisions add one each. Clamping cannot enlarge absolute discrepancies. With normal intermediates the accumulated ratios are bounded by (1+u)⁵/(1−u)² and its lower counterpart. If |gv| ≤ 1, absolute underflow errors propagate through a denominator ≥27, a numerator multiplier ≤28 and T>0.24; the 128τ allowance exceeds those contributions. For |gv|>1 the rational's intermediates are normal. Input quantization, feedback subtraction, and output compensation have their own allowances in the checker.

The four checked RMS radii form an invariant box. If the previous stage error is ≤d, the new input error is ≤e and the local arithmetic error is ≤v, the next error is ≤(1−a)d + ae + v. Thus e+v/a ≤ d is sufficient. Each next stage consumes the current updated bound. Cubic residuals of the linear reference use √15 times the cube of the stage RMS upper bound. The checker verifies the four inequalities over the entire coefficient box with rational arithmetic. The resulting per-sample RMS output discrepancy from the cold reference is at most 3.9089×10⁻⁶ for binary32. The stable linear reference approaches its infinite-history version in RMS, so the same bound applies in the limit to each analysis window. Real arithmetic gives 9.77×10⁻⁷.

## Band powers and the drawing

For a periodic record, sum the positive squared gains of the linear reference, the FIR stages and the Hann window over the signed input Fourier lines. Decimation aliases add in power in this ensemble. For fresh records at uniform random phase, the covariance of the ideal concatenated input at lag h equals that of the circular record multiplied by max(0,1−|h|/N). The expected spectrum is therefore the line spectrum spread by the Fejér kernel of length N. The checker encloses both laws in one calculation. At displacement η from a native lattice index j, |η| ≤ ½, the relative density is a sum of squared interpolation weights over allowed input modes. The sum over all native modes is exactly one by DFT orthogonality. At integer circular distance r from j each weight is at most 1/[4(r−½)²], by |sin πt| ≥ 2|t| for |t| ≤ ½. At most two modes have each distance. Summing the missing or excess tail from distance d by an integral gives at most 1/[2(d−1.5)] when d≥2. This gives explicit upper and lower density in every native frequency cell, including DC and both edges of the host bandwidth.

Integer cosine enclosures, exact FIR coefficient quantization errors, and a first-moment bound on the change across half a cell enclose the analyzer gain. Away from the Hann main lobe, two summations by parts give |W(ω)| ≤ ‖Δ²w‖₁/|1−exp(iω)|². The spectrum checker sums nonnegative terms and allows 10⁻⁸ relative error on the binary64 reductions. This exceeds the sequential summation error even at 458,752 terms. The linear-reference cutoff gain uses a²/(a²+4(1−a)sin²(ω/2)) per pole in power.

The triangle inequality in the space of random FFT coefficients bounds the difference of band RMS amplitudes. The amplifier from a uniform per-sample RMS error is at most the products of the relevant FIR ℓ¹ norms and √(Σw²/1536). This follows from the one-sided Parseval identity since band overlaps do not exceed bin width. Apply this separately to wet and dry, square the nonnegative amplitude endpoints, then divide by the opposite positive dry endpoint. A cold reference loses its initialization transient under the stable linear recurrence. The resulting power bounds apply to the limit of expected averages, or to every accumulation point. Digital long-run existence for the fresh finite digital-record law is covered by the earlier finite-state argument.

The checker encloses the packed fit over every subcell in the knob box using its multiaffine vertices. It interpolates the raw physical and fit dB envelopes and splits each frequency interval at their floor crossings. If a physical lower power endpoint is zero, −60 is a valid displayed lower bound throughout its adjacent segment. The square of the maximum possible absolute error is convex on each remaining piece, so a rational trapezoid bounds its integral. End segments extend flat. The certified denominator for the log-frequency length is the same one used for the dense archive.

The paper argument connects the noise law, stock rounding and the analyzer. Lean checks the pathwise real-arithmetic energy theorem, the one-stage RMS invariant and the band-amplitude-to-power inequality. The matrix enumeration, spectral sums and reported decimal error bounds are external numerical certificates. A tight whole-curve upper bound at high resonance remains open.

## Reproduction

From the worktree root, run the following commands after the normal core and noise-grid build.

```sh
scripts/lean.sh build DSP.Filters.NaiveReuppingLadder.NoiseResponse
clang++ -std=c++20 -O2 -fno-fast-math -ffp-contract=off -I scripts/ladder_noise_grid \
  -I private/src -I formal/DSP/generated -I build/core/RelWithDebInfo/generated \
  scripts/ladder_noise_grid/response_fit/WetPowerBox.cpp -o build/tools/ladder-noise-grid/wet-power-box
build/tools/ladder-noise-grid/wet-power-box
grid=build/results/ladder-noise-grid
OPENBLAS_NUM_THREADS=1 VECLIB_MAXIMUM_THREADS=1 python3 scripts/ladder_noise_grid/response_fit/wet_power_certificate.py \
  "$grid/physics-v1/analysis-coefficients.npz" "$grid/screen-v3/display-model.npz" \
  "$grid/screen-v3/physical-stress/wet-power-certificate.json"
```
