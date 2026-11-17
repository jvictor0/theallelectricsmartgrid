import DSP.Filters.OnePoleZOHLowPass.Transfer
import DSP.IfLess.Real
import Mathlib.Analysis.SpecialFunctions.Trigonometric.Basic

noncomputable section

namespace DSP.OnePoleZOHLowPass

/-- One sample of delay at `frequency` cycles per sample: exp(-2πif).
This is an exact mathematical definition, not an executable complex kernel. -/
def unitDelay (frequency : ℝ) : ℂ :=
  Complex.exp (((-(2 * (Real.pi * frequency)) : ℝ) : ℂ) * Complex.I)

@[simp] theorem unitDelay_re (frequency : ℝ) :
    (unitDelay frequency).re = Real.cos (2 * (Real.pi * frequency)) := by
  rw [unitDelay, Complex.exp_ofReal_mul_I_re, Real.cos_neg]

@[simp] theorem unitDelay_im (frequency : ℝ) :
    (unitDelay frequency).im = -Real.sin (2 * (Real.pi * frequency)) := by
  rw [unitDelay, Complex.exp_ofReal_mul_I_im, Real.sin_neg]

/-- The same transfer used in `realizes`, now interpreted at a complex frequency. -/
def Snapshot.response (s : Snapshot ℝ) (frequency : ℝ) : ℂ :=
  transfer (s.alpha : ℂ) (unitDelay frequency)

/-- The optimization is a real identity: expand the squared complex norm,
use sin² + cos² = 1, then rewrite cos(2θ) = 1 - 2 sin²(θ). -/
theorem denominator_normSq (alpha frequency : ℝ) :
    Complex.normSq (1 - (1 - (alpha : ℂ)) * unitDelay frequency) =
      alpha ^ 2 + 4 * (1 - alpha) * Real.sin (Real.pi * frequency) ^ 2 := by
  calc
    _ = (1 - alpha) ^ 2 * (Real.sin (2 * (Real.pi * frequency)) ^ 2 +
        Real.cos (2 * (Real.pi * frequency)) ^ 2) + 1 -
        2 * (1 - alpha) * Real.cos (2 * (Real.pi * frequency)) := by
      simp [Complex.normSq_apply, -Real.sin_sq_add_cos_sq]
      ring
    _ = _ := by
      rw [Real.sin_sq_add_cos_sq, Real.cos_two_mul_eq_one_sub]
      ring

/-- Squared magnitude is the squared norm of the numerator divided by that
of the denominator. Lean's exact division is total, including division by zero. -/
theorem Snapshot.response_normSq (s : Snapshot ℝ) (frequency : ℝ) :
    Complex.normSq (s.response frequency) =
      s.alpha ^ 2 / (s.alpha ^ 2 + 4 * (1 - s.alpha) * Real.sin (Real.pi * frequency) ^ 2) := by
  rw [Snapshot.response, transfer, Complex.normSq_div, Complex.normSq_ofReal,
    denominator_normSq]
  rw [pow_two]

/-- For the positive ZOH coefficient range the raw denominator is strictly
positive, even when the supplied sine is zero at DC. -/
theorem magnitude_denominator_pos (alpha sinHalf : ℝ) (hpos : 0 < alpha) (hle : alpha ≤ 1) :
    0 < alpha ^ 2 + 4 * (1 - alpha) * sinHalf ^ 2 := by
  have hn : 0 ≤ 4 * (1 - alpha) * sinHalf ^ 2 :=
    mul_nonneg (mul_nonneg (by norm_num) (sub_nonneg.mpr hle)) (sq_nonneg _)
  exact add_pos_of_pos_of_nonneg (sq_pos_of_pos hpos) hn

/-- The executable real-arithmetic kernel computes the exact transfer's
squared magnitude, including the zero-coefficient endpoint. -/
theorem Snapshot.magnitudeSquared_eq_normSq (s : Snapshot ℝ) (frequency : ℝ)
    (ha : 0 ≤ s.alpha) :
    s.magnitudeSquared (Real.sin (Real.pi * frequency)) = Complex.normSq (s.response frequency) := by
  rw [s.response_normSq]
  by_cases hpos : 0 < s.alpha
  · simp [Snapshot.magnitudeSquared, fma_eq, IfLess.ifLess, hpos, pow_two, add_comm]
  · have hz : s.alpha = 0 := le_antisymm (le_of_not_gt hpos) ha
    simp [Snapshot.magnitudeSquared, hz, IfLess.ifLess]

/-- The UI can take one square root to obtain magnitude. No executable complex
response is needed to evaluate it. -/
theorem Snapshot.magnitude_eq_norm (s : Snapshot ℝ) (frequency : ℝ) (ha : 0 ≤ s.alpha) :
    Real.sqrt (s.magnitudeSquared (Real.sin (Real.pi * frequency))) = ‖s.response frequency‖ := by
  rw [s.magnitudeSquared_eq_normSq frequency ha, Complex.normSq_eq_norm_sq,
    Real.sqrt_sq (norm_nonneg _)]

@[simp] theorem magnitudeSquared_zero (sinHalf : ℝ) :
    (⟨0⟩ : Snapshot ℝ).magnitudeSquared sinHalf = 0 := by
  simp [Snapshot.magnitudeSquared, IfLess.ifLess]

@[simp] theorem magnitudeSquared_one (sinHalf : ℝ) :
    (⟨1⟩ : Snapshot ℝ).magnitudeSquared sinHalf = 1 := by
  simp [Snapshot.magnitudeSquared, IfLess.ifLess, fma_eq]

theorem Snapshot.magnitudeSquared_dc (s : Snapshot ℝ) (ha : 0 < s.alpha) :
    s.magnitudeSquared 0 = 1 := by
  simp [Snapshot.magnitudeSquared, IfLess.ifLess, fma_eq, ha, ne_of_gt ha]

end DSP.OnePoleZOHLowPass
