import DSP.Filters.OnePoleZOHLowPass
import Mathlib.Analysis.SpecialFunctions.Trigonometric.Basic
import Mathlib.Analysis.SpecificLimits.Basic
import Mathlib.Tactic.FieldSimp
import Mathlib.Tactic.Linarith
import Mathlib.Tactic.LinearCombination
import Mathlib.Tactic.Ring

/-! Exact real-arithmetic response of the shared difference-form smoother to
squared sinusoids. Frequencies here are radians per sample. The observation
`power n` is AFTER processing sinusoid sample n; its initial value may include
arbitrary earlier history. No averaging approximation or floating-point claim
is used, and the persistent ripple is not claimed to converge to zero. -/

noncomputable section

namespace DSP.OnePoleZOHLowPass.Ripple

open Filter
open scoped Topology

def denominator (alpha theta : ℝ) : ℝ :=
  (1 - (1 - alpha) * Real.cos theta) ^ 2 +
    ((1 - alpha) * Real.sin theta) ^ 2

theorem denominator_eq (alpha theta : ℝ) :
    denominator alpha theta = alpha ^ 2 + 2 * (1 - alpha) * (1 - Real.cos theta) := by
  have h := Real.sin_sq_add_cos_sq theta
  unfold denominator
  nlinarith [sq_nonneg (1 - alpha)]

theorem denominator_pos (alpha theta : ℝ) (ha : 0 < alpha) (hupper : alpha ≤ 1) :
    0 < denominator alpha theta := by
  rw [denominator_eq]
  have h := mul_nonneg (sub_nonneg.mpr hupper) (sub_nonneg.mpr (Real.cos_le_one theta))
  nlinarith [sq_pos_of_pos ha]

def cosineCoefficient (alpha theta : ℝ) : ℝ :=
  alpha * (1 - (1 - alpha) * Real.cos theta) / denominator alpha theta

def sineCoefficient (alpha theta : ℝ) : ℝ :=
  alpha * (1 - alpha) * Real.sin theta / denominator alpha theta

/-- The post-update response to cos(t), expressed without an inverse trig. -/
def harmonic (alpha theta t : ℝ) : ℝ :=
  cosineCoefficient alpha theta * Real.cos t + sineCoefficient alpha theta * Real.sin t

theorem harmonic_step (alpha theta t : ℝ) (hd : denominator alpha theta ≠ 0) :
    relax alpha (Real.cos (t + theta)) (harmonic alpha theta t) =
      harmonic alpha theta (t + theta) := by
  have h := Real.sin_sq_add_cos_sq theta
  have hc : (1 - alpha) * cosineCoefficient alpha theta + alpha * Real.cos theta =
      cosineCoefficient alpha theta * Real.cos theta + sineCoefficient alpha theta * Real.sin theta := by
    unfold cosineCoefficient sineCoefficient
    field_simp
    unfold denominator
    linear_combination (alpha * Real.cos theta * (1 - alpha) ^ 2 - alpha * (1 - alpha)) * h
  have hs : (1 - alpha) * sineCoefficient alpha theta - alpha * Real.sin theta =
      -cosineCoefficient alpha theta * Real.sin theta + sineCoefficient alpha theta * Real.cos theta := by
    unfold cosineCoefficient sineCoefficient
    field_simp
    unfold denominator
    linear_combination -alpha * Real.sin theta * (1 - alpha) ^ 2 * h
  simp only [relax, harmonic, Real.cos_add, Real.sin_add]
  linear_combination Real.cos t * hc + Real.sin t * hs

theorem harmonic_sq_le (alpha theta t : ℝ) (hd : denominator alpha theta ≠ 0) :
    harmonic alpha theta t ^ 2 ≤ alpha ^ 2 / denominator alpha theta := by
  have hnorm : cosineCoefficient alpha theta ^ 2 + sineCoefficient alpha theta ^ 2 =
      alpha ^ 2 / denominator alpha theta := by
    unfold cosineCoefficient sineCoefficient
    field_simp
    unfold denominator
    ring
  rw [← hnorm]
  have h := Real.sin_sq_add_cos_sq t
  have hid : harmonic alpha theta t ^ 2 +
      (cosineCoefficient alpha theta * Real.sin t - sineCoefficient alpha theta * Real.cos t) ^ 2 =
      cosineCoefficient alpha theta ^ 2 + sineCoefficient alpha theta ^ 2 := by
    unfold harmonic
    linear_combination (cosineCoefficient alpha theta ^ 2 + sineCoefficient alpha theta ^ 2) * h
  nlinarith [sq_nonneg (cosineCoefficient alpha theta * Real.sin t -
    sineCoefficient alpha theta * Real.cos t)]

/-- Ripple amplitude divided by mean power, at TWICE the input frequency. -/
def ratio (alpha omega : ℝ) : ℝ :=
  Real.sqrt (alpha ^ 2 / (alpha ^ 2 + 4 * (1 - alpha) * Real.sin omega ^ 2))

theorem denominator_double (alpha omega : ℝ) :
    denominator alpha (2 * omega) = alpha ^ 2 + 4 * (1 - alpha) * Real.sin omega ^ 2 := by
  rw [denominator_eq, Real.cos_two_mul_eq_one_sub]
  ring

theorem harmonic_abs_le (alpha omega t : ℝ) (hd : denominator alpha (2 * omega) ≠ 0) :
    |harmonic alpha (2 * omega) t| ≤ ratio alpha omega := by
  unfold ratio
  rw [← denominator_double]
  exact Real.abs_le_sqrt (harmonic_sq_le alpha (2 * omega) t hd)

theorem ratio_le_one (alpha omega : ℝ) (ha : 0 < alpha) (hupper : alpha ≤ 1) :
    ratio alpha omega ≤ 1 := by
  have hd := denominator_pos alpha (2 * omega) ha hupper
  have hb : alpha ^ 2 ≤ denominator alpha (2 * omega) := by
    rw [denominator_double]
    have h := mul_nonneg (sub_nonneg.mpr hupper) (sq_nonneg (Real.sin omega))
    linarith
  unfold ratio
  rw [← denominator_double]
  exact (Real.sqrt_le_one).mpr ((div_le_one hd).mpr hb)

def sinusoid (amplitude omega phase : ℝ) (n : ℕ) : ℝ :=
  amplitude * Real.cos (omega * n + phase)

def steadyPower (alpha amplitude omega phase : ℝ) (n : ℕ) : ℝ :=
  amplitude ^ 2 / 2 * (1 + harmonic alpha (2 * omega) (2 * (omega * n + phase)))

theorem sinusoid_sq (amplitude omega phase : ℝ) (n : ℕ) :
    sinusoid amplitude omega phase n ^ 2 =
      amplitude ^ 2 / 2 * (1 + Real.cos (2 * (omega * n + phase))) := by
  simp only [sinusoid, Real.cos_two_mul]
  ring

theorem steadyPower_step (alpha amplitude omega phase : ℝ)
    (hd : denominator alpha (2 * omega) ≠ 0) (n : ℕ) :
    relax alpha (sinusoid amplitude omega phase (n + 1) ^ 2)
      (steadyPower alpha amplitude omega phase n) =
      steadyPower alpha amplitude omega phase (n + 1) := by
  have ht : 2 * (omega * ((n + 1 : ℕ) : ℝ) + phase) =
      2 * (omega * n + phase) + 2 * omega := by push_cast; ring
  rw [sinusoid_sq]
  simp only [steadyPower, ht]
  have h := harmonic_step alpha (2 * omega) (2 * (omega * n + phase)) hd
  unfold relax at *
  linear_combination amplitude ^ 2 / 2 * h

/-- Exact decomposition for any initial smoother power, including zero. -/
theorem error_eq (alpha amplitude omega phase : ℝ)
    (hd : denominator alpha (2 * omega) ≠ 0) (power : ℕ → ℝ)
    (hstep : ∀ n, power (n + 1) =
      relax alpha (sinusoid amplitude omega phase (n + 1) ^ 2) (power n)) (n : ℕ) :
    power n - steadyPower alpha amplitude omega phase n =
      (1 - alpha) ^ n * (power 0 - steadyPower alpha amplitude omega phase 0) := by
  induction n with
  | zero => simp
  | succ n ih =>
    rw [hstep, ← steadyPower_step alpha amplitude omega phase hd n, pow_succ]
    unfold relax
    linear_combination (1 - alpha) * ih

/-- Convergence is to the rippling response, not to its constant center. -/
theorem tendsto_error_zero (alpha amplitude omega phase : ℝ)
    (ha : 0 < alpha) (hupper : alpha ≤ 1) (power : ℕ → ℝ)
    (hstep : ∀ n, power (n + 1) =
      relax alpha (sinusoid amplitude omega phase (n + 1) ^ 2) (power n)) :
    Tendsto (fun n => power n - steadyPower alpha amplitude omega phase n) atTop (𝓝 0) := by
  have hp := tendsto_pow_atTop_nhds_zero_of_lt_one (sub_nonneg.mpr hupper)
    (show 1 - alpha < 1 by linarith)
  have he := error_eq alpha amplitude omega phase
    (ne_of_gt (denominator_pos alpha (2 * omega) ha hupper)) power hstep
  have ht := hp.mul_const (power 0 - steadyPower alpha amplitude omega phase 0)
  simp only [zero_mul] at ht
  exact ht.congr (fun n => (he n).symm)

theorem steadyPower_deviation_le (alpha amplitude omega phase : ℝ)
    (hd : denominator alpha (2 * omega) ≠ 0) (n : ℕ) :
    |steadyPower alpha amplitude omega phase n - amplitude ^ 2 / 2| ≤
      amplitude ^ 2 / 2 * ratio alpha omega := by
  have he : steadyPower alpha amplitude omega phase n - amplitude ^ 2 / 2 =
      amplitude ^ 2 / 2 * harmonic alpha (2 * omega) (2 * (omega * n + phase)) := by
    unfold steadyPower
    ring
  rw [he, abs_mul, abs_of_nonneg (div_nonneg (sq_nonneg _) (by norm_num))]
  exact mul_le_mul_of_nonneg_left (harmonic_abs_le alpha omega _ hd)
    (div_nonneg (sq_nonneg _) (by norm_num))

/-- A perturbation estimate for the same smoother on arbitrary input streams. -/
theorem relax_abs_sub_le (alpha u v p q : ℝ) (ha : 0 ≤ alpha) (hupper : alpha ≤ 1) :
    |relax alpha u p - relax alpha v q| ≤
      (1 - alpha) * |p - q| + alpha * |u - v| := by
  have he : relax alpha u p - relax alpha v q =
      (1 - alpha) * (p - q) + alpha * (u - v) := by unfold relax; ring
  rw [he]
  simpa only [abs_mul, abs_of_nonneg ha, abs_of_nonneg (sub_nonneg.mpr hupper)]
    using abs_add_le ((1 - alpha) * (p - q)) (alpha * (u - v))

/-- Uniform forcing error is not amplified; initial error decays geometrically. -/
theorem tracking_bound (alpha delta : ℝ) (ha : 0 ≤ alpha) (hupper : alpha ≤ 1)
    (u v p q : ℕ → ℝ)
    (hp : ∀ n, p (n + 1) = relax alpha (u n) (p n))
    (hq : ∀ n, q (n + 1) = relax alpha (v n) (q n))
    (huv : ∀ n, |u n - v n| ≤ delta) (n : ℕ) :
    |p n - q n| ≤ (1 - alpha) ^ n * |p 0 - q 0| +
      (1 - (1 - alpha) ^ n) * delta := by
  induction n with
  | zero => simp
  | succ n ih =>
    rw [hp, hq]
    calc
      _ ≤ (1 - alpha) * |p n - q n| + alpha * |u n - v n| :=
        relax_abs_sub_le alpha _ _ _ _ ha hupper
      _ ≤ (1 - alpha) * ((1 - alpha) ^ n * |p 0 - q 0| +
          (1 - (1 - alpha) ^ n) * delta) + alpha * delta :=
        add_le_add (mul_le_mul_of_nonneg_left ih (sub_nonneg.mpr hupper))
          (mul_le_mul_of_nonneg_left (huv n) ha)
      _ = _ := by rw [pow_succ]; ring

/-- Vanishing forcing error gives vanishing state error. This admits the
bandpass stages' startup transients instead of assuming an exact sine at start. -/
theorem tendsto_tracking (alpha : ℝ) (ha : 0 < alpha) (hupper : alpha ≤ 1)
    (u v p q : ℕ → ℝ)
    (hp : ∀ n, p (n + 1) = relax alpha (u n) (p n))
    (hq : ∀ n, q (n + 1) = relax alpha (v n) (q n))
    (huv : Tendsto (fun n => u n - v n) atTop (𝓝 0)) :
    Tendsto (fun n => p n - q n) atTop (𝓝 0) := by
  rw [Metric.tendsto_atTop] at huv ⊢
  intro epsilon hepsilon
  obtain ⟨N, hN⟩ := huv (epsilon / 2) (by linarith)
  have ht := (tendsto_pow_atTop_nhds_zero_of_lt_one (sub_nonneg.mpr hupper)
    (show 1 - alpha < 1 by linarith)).mul_const |p N - q N|
  simp only [zero_mul] at ht
  obtain ⟨M, hM⟩ := Metric.tendsto_atTop.mp ht (epsilon / 2) (by linarith)
  refine ⟨N + M, ?_⟩
  intro n hn
  obtain ⟨j, rfl⟩ := Nat.exists_eq_add_of_le (show N ≤ n by omega)
  have hj : M ≤ j := by omega
  have hbound := tracking_bound alpha (epsilon / 2) ha.le hupper
    (fun i => u (N + i)) (fun i => v (N + i))
    (fun i => p (N + i)) (fun i => q (N + i))
    (fun i => by simpa [Nat.add_assoc] using hp (N + i))
    (fun i => by simpa [Nat.add_assoc] using hq (N + i))
    (fun i => by simpa only [Real.dist_eq, sub_zero] using (hN (N + i) (by omega)).le) j
  have hsmall := hM j hj
  have hnneg := mul_nonneg (pow_nonneg (sub_nonneg.mpr hupper) j) (abs_nonneg (p N - q N))
  simp only [Real.dist_eq, sub_zero, abs_of_nonneg hnneg] at hsmall ⊢
  simp only [Nat.add_zero] at hbound
  have hr := mul_nonneg (pow_nonneg (sub_nonneg.mpr hupper) j) (le_of_lt hepsilon)
  nlinarith

end DSP.OnePoleZOHLowPass.Ripple
