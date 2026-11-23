import DSP.Analysis.PolarTube
import Mathlib.Analysis.SpecialFunctions.Complex.LogDeriv

/-! Smooth local real lifts of the global circle-valued phase. Rotating the
branch cut away from a cell avoids differentiating discontinuous atan2 values.
The derivative does not depend on the chosen anchor. -/

noncomputable section
namespace DSP.PolarTube

def localPhase (anchor z : ℂ) : ℝ := (z/anchor).arg+anchor.arg

theorem localPhase_angle (anchor z : ℂ) (ha : anchor ≠ 0) (hz : z ≠ 0) :
    (localPhase anchor z:Real.Angle) = z.arg := by
  rw [localPhase, Real.Angle.coe_add, Complex.arg_div_coe_angle hz ha, sub_add_cancel]

theorem localPhase_at_anchor (anchor : ℂ) (ha : anchor ≠ 0) :
    localPhase anchor anchor = anchor.arg := by simp [localPhase, ha]

theorem hasDerivAt_localPhase (anchor : ℂ) (ha : anchor ≠ 0)
    (z : ℝ → ℂ) (t : ℝ) (v : ℂ) (hz : HasDerivAt z v t)
    (hcut : z t/anchor ∈ Complex.slitPlane) :
    HasDerivAt (fun r => localPhase anchor (z r)) (v/z t).im t := by
  have h := (Complex.imCLM.hasFDerivAt.comp_hasDerivAt t
    ((hz.div_const anchor).clog_real hcut)).add_const anchor.arg
  simpa only [Function.comp_def, Complex.imCLM_apply, Complex.log_im, localPhase,
    div_div_div_cancel_right₀ ha] using h

/-- The phase derivative in the real coordinates used by the interval
program. This is a statement about a local lift of the actual global phase. -/
theorem localPhase_derivative_formula (z v : ℂ) :
    (v/z).im = (-z.im*v.re+z.re*v.im)/(z.re^2+z.im^2) := by
  rw [Complex.div_im, Complex.normSq_apply]
  ring

/-- Differentiating the phase derivative in a second direction gives the
mixed phase curvature, including any mixed derivative of the input chart. -/
theorem hasDerivAt_phaseVariation (z v : ℝ → ℂ) (t : ℝ) (left mixed : ℂ)
    (hz : HasDerivAt z left t) (hv : HasDerivAt v mixed t) (hne : z t ≠ 0) :
    HasDerivAt (fun r => (v r/z r).im)
      (mixed/z t-v t*left/(z t)^2).im t := by
  have h : HasDerivAt (fun r => (v r/z r).im) ((mixed*z t-v t*left)/(z t)^2).im t := by
    simpa only [Function.comp_def, Complex.imCLM_apply, Pi.div_apply] using!
      Complex.imCLM.hasFDerivAt.comp_hasDerivAt t (hv.div hz hne)
  apply h.congr_deriv
  apply congrArg Complex.im
  field_simp [hne]

def phaseHessian (z left right : ℂ) : ℝ :=
  (2*z.re*z.im*left.re*right.re+
    (z.im^2-z.re^2)*(left.re*right.im+left.im*right.re)-
    2*z.re*z.im*left.im*right.im)/(z.re^2+z.im^2)^2

/-- The bilinear Hessian agrees with the real 2-by-2 matrix used by the
interval proposal, rather than merely matching a finite-difference test. -/
theorem phaseHessian_formula (z left right mixed : ℂ) :
    (mixed/z-right*left/z^2).im = (mixed/z).im+phaseHessian z left right := by
  rw [Complex.sub_im]
  simp only [Complex.div_im, Complex.mul_re, Complex.mul_im,
    Complex.normSq_apply, phaseHessian, pow_two]
  ring

/-- At every nonzero center an anchor with a safe branch cut is available.
An interval cell still has to verify that it stays in this open set. -/
theorem anchor_safe (z : ℂ) (hz : z ≠ 0) : z/z ∈ Complex.slitPlane := by
  simp [hz]

end DSP.PolarTube
