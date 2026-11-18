import DSP.Filters.NaiveReuppingLadder.Contracts

/-! Proof-only floor-free controller, as requested for the convergence study.
The production step and generated code retain their existing power floor.
Results connecting this ratio to physical powers require positive input power;
Lean's total division at zero is not a silence-handling policy. -/

noncomputable section
namespace DSP.NaiveReuppingLadder.FloorFree
open Filter
open scoped Topology

/-- K is base feedback, d is dampingScale, R is referencePowerGain, and x is
feedback detector power divided by input detector power. -/
def feedback (K d R x : ℝ) : ℝ := K / (1 + d * max 0 (x - R))

def fromPowers (K d R inputPower feedbackPower : ℝ) : ℝ :=
  feedback K d R (feedbackPower / inputPower)

theorem fromPowers_eq (K d R inputPower feedbackPower : ℝ) (hp : 0 < inputPower) :
    fromPowers K d R inputPower feedbackPower =
      K / (1 + d * (max 0 (feedbackPower - R * inputPower) / inputPower)) := by
  unfold fromPowers feedback
  have he : feedbackPower / inputPower - R = (feedbackPower - R * inputPower) / inputPower := by
    field_simp
  rw [he, ← max_div_div_right hp.le]
  simp

/-- Amplitude scaling cancels exactly from the power ratio. This establishes
scale invariance of the controller, not superposition or closed-loop settling. -/
theorem fromPowers_scaled (K d R inputPower feedbackPower amplitude : ℝ) (ha : amplitude ≠ 0) :
    fromPowers K d R (amplitude ^ 2 * inputPower) (amplitude ^ 2 * feedbackPower) =
      fromPowers K d R inputPower feedbackPower := by
  unfold fromPowers
  rw [mul_div_mul_left _ _ (pow_ne_zero 2 ha)]

theorem denominator_ge_one (d R x : ℝ) (hd : 0 ≤ d) : 1 ≤ 1 + d * max 0 (x - R) := by
  have h := mul_nonneg hd (le_max_left 0 (x - R))
  linarith

theorem feedback_bounds (K d R x : ℝ) (hK : 0 ≤ K) (hd : 0 ≤ d) :
    0 ≤ feedback K d R x ∧ feedback K d R x ≤ K := by
  have hden := denominator_ge_one d R x hd
  constructor
  · exact div_nonneg hK (by linarith)
  · exact div_le_self hK hden

/-- Equality with critical loop gain is the boundary, not strict stability.
This scalar condition still does not prove stability of time-varying feedback. -/
theorem below_critical_iff (K d R x slope critical : ℝ) (hd : 0 ≤ d) :
    feedback K d R x * slope ^ 3 < critical ↔
      K * slope ^ 3 < critical * (1 + d * max 0 (x - R)) := by
  unfold feedback
  rw [div_mul_eq_mul_div, div_lt_iff₀ (by have := denominator_ge_one d R x hd; linarith)]

/-- Converting a power-ratio error into a coefficient error. A further
closed-loop estimate must account for amplification at generated frequencies. -/
theorem feedback_lipschitz (K d R x y : ℝ) (hK : 0 ≤ K) (hd : 0 ≤ d) :
    |feedback K d R x - feedback K d R y| ≤ K * d * |x - y| := by
  have hx := denominator_ge_one d R x hd
  have hy := denominator_ge_one d R y hd
  have hx0 : 0 < 1 + d * max 0 (x - R) := by linarith
  have hy0 : 0 < 1 + d * max 0 (y - R) := by linarith
  have he : feedback K d R x - feedback K d R y =
      K * d * (max 0 (y - R) - max 0 (x - R)) /
        ((1 + d * max 0 (x - R)) * (1 + d * max 0 (y - R))) := by
    unfold feedback
    field_simp
    ring
  have hm : |max 0 (y - R) - max 0 (x - R)| ≤ |x - y| := by
    rw [max_comm 0 (y - R), max_comm 0 (x - R)]
    simpa only [sub_sub_sub_cancel_right, abs_sub_comm] using
      abs_max_sub_max_le_abs (y - R) (x - R) 0
  rw [he, abs_div, abs_of_pos (mul_pos hx0 hy0), abs_mul,
    abs_of_nonneg (mul_nonneg hK hd)]
  calc
    _ ≤ K * d * |max 0 (y - R) - max 0 (x - R)| :=
      div_le_self (mul_nonneg (mul_nonneg hK hd) (abs_nonneg _)) (by nlinarith)
    _ ≤ _ := mul_le_mul_of_nonneg_left hm (mul_nonneg hK hd)

/-- Relative ripple in both powers gives a ratio bound independent of their
common amplitude or bandpass attenuation. The strict rho < 1 condition keeps
the input power away from zero; the phases of the two ripples may differ. -/
theorem ratio_ripple_bound (inputMean feedbackMean inputPower feedbackPower rho : ℝ)
    (hi : 0 < inputMean) (hf : 0 ≤ feedbackMean) (hr : 0 ≤ rho) (hu : rho < 1)
    (hip : |inputPower - inputMean| ≤ inputMean * rho)
    (hfp : |feedbackPower - feedbackMean| ≤ feedbackMean * rho) :
    |feedbackPower / inputPower - feedbackMean / inputMean| ≤
      (feedbackMean / inputMean) * (2 * rho / (1 - rho)) := by
  have hlo : inputMean * (1 - rho) ≤ inputPower := by
    have := (abs_le.mp hip).1
    nlinarith
  have hp : 0 < inputPower := lt_of_lt_of_le (mul_pos hi (by linarith)) hlo
  have hden : 0 < inputMean * inputMean * (1 - rho) := by positivity
  have hnum : 0 ≤ inputMean * (feedbackMean * rho) + feedbackMean * (inputMean * rho) := by positivity
  have he : feedbackPower / inputPower - feedbackMean / inputMean =
      (inputMean * (feedbackPower - feedbackMean) - feedbackMean * (inputPower - inputMean)) /
        (inputPower * inputMean) := by field_simp; ring
  rw [he, abs_div, abs_of_pos (mul_pos hp hi)]
  calc
    _ ≤ (inputMean * (feedbackMean * rho) + feedbackMean * (inputMean * rho)) /
        (inputPower * inputMean) := by
      apply div_le_div_of_nonneg_right _ (mul_pos hp hi).le
      calc
        _ ≤ |inputMean * (feedbackPower - feedbackMean)| +
            |feedbackMean * (inputPower - inputMean)| := by
          simpa only [sub_zero, zero_sub, abs_neg] using
            abs_sub_le (inputMean * (feedbackPower - feedbackMean)) 0
              (feedbackMean * (inputPower - inputMean))
        _ = inputMean * |feedbackPower - feedbackMean| +
            feedbackMean * |inputPower - inputMean| := by
          rw [abs_mul, abs_mul, abs_of_pos hi, abs_of_nonneg hf]
        _ ≤ _ := add_le_add (mul_le_mul_of_nonneg_left hfp hi.le)
          (mul_le_mul_of_nonneg_left hip hf)
    _ ≤ (inputMean * (feedbackMean * rho) + feedbackMean * (inputMean * rho)) /
        (inputMean * inputMean * (1 - rho)) := by
      apply div_le_div_of_nonneg_left hnum hden
      nlinarith [mul_le_mul_of_nonneg_right hlo hi.le]
    _ = _ := by field_simp; ring

/-- This bounds the controller ripple, not the output sidebands of the closed
loop. It does not replace the time-varying controller by its mean. -/
theorem fromPowers_ripple_bound (K d R inputMean feedbackMean inputPower feedbackPower rho : ℝ)
    (hK : 0 ≤ K) (hd : 0 ≤ d)
    (hi : 0 < inputMean) (hf : 0 ≤ feedbackMean) (hr : 0 ≤ rho) (hu : rho < 1)
    (hip : |inputPower - inputMean| ≤ inputMean * rho)
    (hfp : |feedbackPower - feedbackMean| ≤ feedbackMean * rho) :
    |fromPowers K d R inputPower feedbackPower - feedback K d R (feedbackMean / inputMean)| ≤
      K * d * ((feedbackMean / inputMean) * (2 * rho / (1 - rho))) := by
  exact (feedback_lipschitz K d R _ _ hK hd).trans
    (mul_le_mul_of_nonneg_left (ratio_ripple_bound _ _ _ _ _ hi hf hr hu hip hfp)
      (mul_nonneg hK hd))

/-- If the ratio actually settles, the controller settles. The ratio of two
rippling envelopes need not converge, so this is an explicit hypothesis. -/
theorem feedback_tendsto (K d R : ℝ) (hd : 0 ≤ d) (ratio : ℕ → ℝ) (limit : ℝ)
    (h : Tendsto ratio atTop (𝓝 limit)) :
    Tendsto (fun n => feedback K d R (ratio n)) atTop (𝓝 (feedback K d R limit)) := by
  have hx : 1 + d * max 0 (limit - R) ≠ 0 := by
    have := denominator_ge_one d R limit hd
    linarith
  exact tendsto_const_nhds.div
    (tendsto_const_nhds.add (tendsto_const_nhds.mul
      (tendsto_const_nhds.max (h.sub_const R)))) hx

end DSP.NaiveReuppingLadder.FloorFree
