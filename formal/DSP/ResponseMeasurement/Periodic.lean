import DSP.ResponseMeasurement
import Mathlib.Data.Nat.Periodic

/-! Exact averaging of periodic responses. The convergence hypothesis concerns
the waveform (or the modulated product), not the coefficient alone. -/

noncomputable section
namespace DSP.ResponseMeasurement
open Filter Finset
open scoped Topology

theorem average_sub (signal reference : ℕ → ℂ) (count : ℕ) :
    average (fun n => signal n - reference n) count = average signal count - average reference count := by
  simp only [average, Finset.sum_sub_distrib, smul_sub]

theorem average_const (value : ℂ) (count : ℕ) (hc : count ≠ 0) :
    average (fun _ => value) count = value := by
  have hn : (count : ℂ) ≠ 0 := by exact_mod_cast hc
  simp only [average, Finset.sum_const, card_range, nsmul_eq_mul, Complex.real_smul,
    Complex.ofReal_inv, Complex.ofReal_natCast]
  field_simp

theorem periodic_zero_sum_remainder (signal : ℕ → ℂ) (period : ℕ)
    (hp : Function.Periodic signal period) (hs : ∑ n ∈ range period, signal n = 0) (count : ℕ) :
    ∑ n ∈ range count, signal n = ∑ n ∈ range (count % period), signal n := by
  have hshift (q n : ℕ) : signal (q * period + n) = signal n := by
    simpa [Nat.add_comm] using hp.nat_mul q n
  have hblocks (q : ℕ) : ∑ n ∈ range (q * period), signal n = 0 := by
    induction q with
    | zero => simp
    | succ q ih =>
      rw [Nat.succ_mul, Finset.sum_range_add, ih]
      simp only [hshift, hs, zero_add]
  conv_lhs => rw [← Nat.div_add_mod' count period]
  rw [Finset.sum_range_add, hblocks]
  simp only [hshift, zero_add]

/-- A bounded remainder of less than one period disappears in the average. -/
theorem average_zero_of_periodic (signal : ℕ → ℂ) (period : ℕ) (hp0 : 0 < period)
    (hp : Function.Periodic signal period) (hs : ∑ n ∈ range period, signal n = 0) :
    Tendsto (average signal) atTop (𝓝 0) := by
  let bound := ∑ n ∈ range period, ‖signal n‖
  have hb (count : ℕ) : ‖∑ n ∈ range count, signal n‖ ≤ bound := by
    rw [periodic_zero_sum_remainder signal period hp hs]
    apply (norm_sum_le _ _).trans
    apply Finset.sum_le_sum_of_subset_of_nonneg (Finset.range_mono (Nat.mod_lt count hp0).le)
    intro n _ _
    exact norm_nonneg _
  have ht := (tendsto_inv_atTop_nhds_zero_nat (𝕜 := ℝ)).mul_const bound
  simp only [zero_mul] at ht
  apply tendsto_zero_iff_norm_tendsto_zero.mpr
  apply squeeze_zero (fun _ => norm_nonneg _) _ ht
  intro count
  simp only [average, norm_smul, Real.norm_of_nonneg (inv_nonneg.mpr (Nat.cast_nonneg count))]
  exact mul_le_mul_of_nonneg_left (hb count) (inv_nonneg.mpr (Nat.cast_nonneg count))

/-- The all-window limit, not merely the subsequence of complete periods. -/
theorem average_of_periodic (signal : ℕ → ℂ) (period : ℕ) (hp0 : 0 < period)
    (hp : Function.Periodic signal period) :
    Tendsto (average signal) atTop (𝓝 (average signal period)) := by
  let center := average signal period
  have hc : Function.Periodic (fun n => signal n - center) period := by
    intro n
    change signal (n + period) - center = signal n - center
    rw [hp n]
  have hs : ∑ n ∈ range period, (signal n - center) = 0 := by
    have hn : (period : ℂ) ≠ 0 := by exact_mod_cast ne_of_gt hp0
    simp only [center, average, Finset.sum_sub_distrib, Finset.sum_const, card_range,
      nsmul_eq_mul, Complex.real_smul, Complex.ofReal_inv, Complex.ofReal_natCast]
    field_simp
    ring
  have h := (average_zero_of_periodic _ period hp0 hc hs).add_const center
  simp only [zero_add] at h
  apply h.congr'
  filter_upwards [eventually_ge_atTop 1] with count hn
  rw [average_sub, average_const center count (by omega), sub_add_cancel]

/-- A periodic real waveform has a coherent response given by one period,
provided the carrier also repeats on that period. -/
theorem coherent_of_periodic (z : ℂ) (signal : ℕ → ℝ) (period : ℕ) (hp0 : 0 < period)
    (hz : z ^ period = 1) (hp : Function.Periodic signal period) :
    HasCoherentResponse z signal (coherent z signal period) := by
  have hz' : star z ^ period = 1 := by rw [← star_pow, hz, star_one]
  have hperiod : Function.Periodic (fun n => (signal n : ℂ) * star z ^ n) period := by
    intro n
    simp only [hp n, pow_add, hz', mul_one]
  exact (average_of_periodic _ period hp0 hperiod).const_mul 2

/-- Integrating a settling periodic waveform is justified without pretending
that its coefficient is constant or that its harmonics vanish pointwise. -/
theorem coherent_of_periodic_tracking (z : ℂ) (hz : ‖z‖ = 1) (signal reference : ℕ → ℝ)
    (period : ℕ) (hp0 : 0 < period) (hzp : z ^ period = 1)
    (hp : Function.Periodic reference period)
    (ht : Tendsto (fun n => signal n - reference n) atTop (𝓝 0)) :
    HasCoherentResponse z signal (coherent z reference period) :=
  (coherent_of_periodic z reference period hp0 hzp hp).of_tracking hz ht

/-- The companion energy measurement retains all harmonics in the period. -/
theorem meanSquare_of_periodic (signal : ℕ → ℝ) (period : ℕ) (hp0 : 0 < period)
    (hp : Function.Periodic signal period) :
    HasMeanSquare signal (meanSquare signal period) := by
  have h := average_of_periodic (fun n => ((signal n ^ 2 : ℝ) : ℂ)) period hp0
    (by intro n; simp only [hp n])
  have hr := (Complex.continuous_re.tendsto _).comp h
  change Tendsto (fun count => meanSquare signal count) atTop (𝓝 (meanSquare signal period))
  simpa only [Function.comp_def, meanSquare, average, Complex.real_smul, Complex.mul_re,
    Complex.ofReal_re, Complex.ofReal_im, zero_mul, sub_zero, Complex.re_sum] using hr

theorem meanSquare_of_periodic_tracking (signal reference : ℕ → ℝ)
    (period : ℕ) (hp0 : 0 < period) (hp : Function.Periodic reference period)
    (ht : Tendsto (fun n => signal n - reference n) atTop (𝓝 0)) :
    HasMeanSquare signal (meanSquare reference period) := by
  have hb (n : ℕ) : |reference n| ≤ ∑ i ∈ range period, |reference i| := by
    rw [← hp.map_mod_nat n]
    exact Finset.single_le_sum (fun i _ => abs_nonneg (reference i))
      (Finset.mem_range.mpr (Nat.mod_lt n hp0))
  have he := (Asymptotic.square_tracking signal reference _ hb ht).cesaro_smul
  have hd : Tendsto (fun count => meanSquare signal count - meanSquare reference count)
      atTop (𝓝 0) := by
    simpa [meanSquare, Finset.sum_sub_distrib, smul_eq_mul, mul_sub] using he
  have h := hd.add (meanSquare_of_periodic reference period hp0 hp)
  change Tendsto (fun count => meanSquare signal count) atTop (𝓝 (meanSquare reference period))
  simpa only [sub_add_cancel, zero_add] using h

theorem rms_of_periodic_tracking (signal reference : ℕ → ℝ)
    (period : ℕ) (hp0 : 0 < period) (hp : Function.Periodic reference period)
    (ht : Tendsto (fun n => signal n - reference n) atTop (𝓝 0)) :
    Tendsto (rms signal) atTop (𝓝 (rms reference period)) :=
  (Real.continuous_sqrt.tendsto _).comp
    (meanSquare_of_periodic_tracking signal reference period hp0 hp ht)

end DSP.ResponseMeasurement
