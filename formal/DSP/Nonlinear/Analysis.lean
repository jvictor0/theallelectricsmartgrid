import DSP.Nonlinear.Real
import Mathlib.Analysis.SpecialFunctions.Trigonometric.DerivHyp
import Mathlib.Analysis.Calculus.ContDiff.RCLike
import Mathlib.Analysis.Calculus.MeanValue

noncomputable section
namespace DSP.Tanh

@[fun_prop] theorem contDiff_real : ContDiff ℝ 1 Real.tanh := by
  rw [show Real.tanh = Real.sinh / Real.cosh from funext Real.tanh_eq_sinh_div_cosh]
  exact Real.contDiff_sinh.div Real.contDiff_cosh (fun x => ne_of_gt (Real.cosh_pos x))

theorem hasDerivAt_real (x : ℝ) : HasDerivAt Real.tanh (1/Real.cosh x^2) x := by
  have h := (Real.hasDerivAt_sinh x).div (Real.hasDerivAt_cosh x) (ne_of_gt (Real.cosh_pos x))
  rw [show Real.tanh = Real.sinh / Real.cosh from funext Real.tanh_eq_sinh_div_cosh]
  convert! h using 1
  congr 1
  nlinarith [Real.cosh_sq_sub_sinh_sq x]

theorem hasDerivAt_zero : HasDerivAt Real.tanh 1 0 := by simpa using hasDerivAt_real 0

theorem lipschitz_real : LipschitzWith 1 Real.tanh := by
  apply lipschitzWith_of_nnnorm_deriv_le (fun x => (hasDerivAt_real x).differentiableAt)
  intro x
  rw [(hasDerivAt_real x).deriv]
  apply NNReal.coe_le_coe.mp
  change |1/Real.cosh x^2| ≤ 1
  rw [abs_of_pos (by positivity)]
  apply (div_le_one (by positivity)).mpr
  nlinarith [Real.one_le_cosh x]

end DSP.Tanh
