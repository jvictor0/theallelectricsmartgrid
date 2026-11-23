import DSP.Analysis.PolarTube.Phase
import Mathlib.Analysis.SpecialFunctions.Sqrt

/-! Real first and mixed second derivatives of polar radius. Together with
the phase derivatives these justify the nonlinear output coordinate map. -/

noncomputable section
namespace DSP.PolarTube

def dot (z v : ℂ) : ℝ := z.re*v.re+z.im*v.im

def radiusVariation (z v : ℂ) : ℝ := dot z v/‖z‖

theorem hasDerivAt_re (z : ℝ → ℂ) (t : ℝ) (v : ℂ) (h : HasDerivAt z v t) :
    HasDerivAt (fun r => (z r).re) v.re t := by
  simpa only [Function.comp_def, Complex.reCLM_apply] using!
    Complex.reCLM.hasFDerivAt.comp_hasDerivAt t h

theorem hasDerivAt_im (z : ℝ → ℂ) (t : ℝ) (v : ℂ) (h : HasDerivAt z v t) :
    HasDerivAt (fun r => (z r).im) v.im t := by
  simpa only [Function.comp_def, Complex.imCLM_apply] using!
    Complex.imCLM.hasFDerivAt.comp_hasDerivAt t h

theorem norm_eq_radius (z : ℂ) : ‖z‖ = Real.sqrt (z.re^2+z.im^2) := by
  simp only [Complex.norm_def, Complex.normSq_apply, pow_two]

theorem hasDerivAt_radius (z : ℝ → ℂ) (t : ℝ) (v : ℂ)
    (hz : HasDerivAt z v t) (hne : z t ≠ 0) :
    HasDerivAt (fun r => ‖z r‖) (radiusVariation (z t) v) t := by
  have hnorm : ‖z t‖ ≠ 0 := norm_ne_zero_iff.mpr hne
  have hn : (z t).re^2+(z t).im^2 ≠ 0 := by
    simpa only [Complex.normSq_apply, pow_two] using (Complex.normSq_eq_zero.not.mpr hne)
  have h := (((hasDerivAt_re z t v hz).pow 2).add ((hasDerivAt_im z t v hz).pow 2)).sqrt hn
  simp only [Pi.add_apply, Pi.pow_apply] at h
  simp_rw [← norm_eq_radius] at h
  apply h.congr_deriv
  dsimp [radiusVariation, dot]
  field_simp [hnorm]

def radiusHessian (z left right : ℂ) : ℝ :=
  dot left right/‖z‖-dot z left*dot z right/‖z‖^3

theorem hasDerivAt_radiusVariation (z v : ℝ → ℂ) (t : ℝ) (left mixed : ℂ)
    (hz : HasDerivAt z left t) (hv : HasDerivAt v mixed t) (hne : z t ≠ 0) :
    HasDerivAt (fun r => radiusVariation (z r) (v r))
      (radiusVariation (z t) mixed+radiusHessian (z t) left (v t)) t := by
  have hdot := ((hasDerivAt_re z t left hz).mul (hasDerivAt_re v t mixed hv)).add
    ((hasDerivAt_im z t left hz).mul (hasDerivAt_im v t mixed hv))
  have hnorm : ‖z t‖ ≠ 0 := norm_ne_zero_iff.mpr hne
  have h := hdot.div (hasDerivAt_radius z t left hz hne) hnorm
  apply h.congr_deriv
  dsimp [radiusVariation, radiusHessian, dot]
  field_simp [hnorm]
  ring

/-- The bilinear radius curvature is the same 2-by-2 matrix used by the
interval proposal. Nonzero radius is an explicit domain obligation. -/
theorem radiusHessian_formula (z left right : ℂ) (hne : z ≠ 0) :
    radiusHessian z left right =
      (z.im^2*left.re*right.re-z.re*z.im*(left.re*right.im+left.im*right.re)+
        z.re^2*left.im*right.im)/‖z‖^3 := by
  have hn : ‖z‖ ≠ 0 := norm_ne_zero_iff.mpr hne
  have he : ‖z‖^2 = z.re^2+z.im^2 := by
    simpa only [Complex.normSq_apply, pow_two] using Complex.sq_norm z
  dsimp [radiusHessian, dot]
  field_simp [hn]
  rw [he]
  ring

end DSP.PolarTube
