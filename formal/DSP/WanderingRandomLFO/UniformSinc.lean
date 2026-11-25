import DSP.WanderingRandomLFO.ProbabilityModel
import Mathlib.Analysis.SpecialFunctions.Integrals.Basic

namespace DSP.WanderingRandomLFO

open MeasureTheory Set

/-- The characteristic function of a uniform draw rescaled to the bipolar interval. -/
theorem integral_uniform_bipolar_exp (a : ℝ) :
    (∫ u, Complex.exp ((↑(a * (2 * u - 1)) : ℂ) * Complex.I) ∂unitUniform) =
      (Real.sinc a : ℂ) := by
  have hUniform : unitUniform = volume.restrict (Ioc 0 1) := restrict_Ico_eq_restrict_Ioc
  rw [hUniform, ← intervalIntegral.integral_of_le zero_le_one]
  by_cases ha : a = 0
  · simp [ha, intervalIntegral.integral_const]
  have hScale : (2 : ℝ) * a ≠ 0 := mul_ne_zero (by norm_num) ha
  have hArgument (u : ℝ) : a * (2 * u - 1) = 2 * a * u - a := by ring
  simp_rw [hArgument]
  rw [intervalIntegral.integral_comp_mul_sub
    (f := fun t : ℝ => Complex.exp ((t : ℂ) * Complex.I)) hScale a]
  have hEnd : (2 : ℝ) * a * 1 - a = a := by ring
  rw [mul_zero, zero_sub, hEnd, integral_exp_mul_I_eq_sinc]
  have hScaleComplex : (2 : ℂ) * (a : ℂ) ≠ 0 := by exact_mod_cast hScale
  rw [Complex.real_smul, Complex.ofReal_inv, Complex.ofReal_mul, Complex.ofReal_ofNat,
    ← mul_assoc, inv_mul_cancel₀ hScaleComplex, one_mul]

end DSP.WanderingRandomLFO
