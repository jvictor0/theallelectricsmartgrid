import DSP.Filters.NormalizedTanh
import DSP.Filters.Tuning.History
import DSP.Nonlinear.Real
import Mathlib.Analysis.SpecialFunctions.Trigonometric.DerivHyp

noncomputable section
namespace DSP
open Channels
namespace NormalizedTanh
deriving instance Functor for Coefficients
instance logicalSpecFunctor : Functor LogicalSpec where
  map f s := ⟨f s.gain⟩
theorem apply_real (gain x : ℝ) :
    apply (prepare ⟨gain⟩) x = Real.tanh (x * gain) / Real.tanh gain := rfl

theorem apply_zero (c : Coefficients ℝ) : apply c 0 = 0 := by
  simp [apply]

theorem apply_one (gain : ℝ) (h : Real.tanh gain ≠ 0) :
    apply (prepare ⟨gain⟩) 1 = 1 := by
  simp [apply, prepare, h]

theorem hasDerivAt_zero (c : Coefficients ℝ) : HasDerivAt (apply c) (slopeZero c) 0 := by
  change HasDerivAt (fun x : ℝ => Real.tanh (x * c.gain) / c.tanhGain) (c.gain / c.tanhGain) 0
  have ht : HasDerivAt Real.tanh 1 0 := by
    have h := (Real.hasDerivAt_sinh 0).div (Real.hasDerivAt_cosh 0) (by simp)
    have he : Real.tanh = Real.sinh / Real.cosh := funext Real.tanh_eq_sinh_div_cosh
    rw [he]
    simpa using h
  have hm : HasDerivAt (fun x : ℝ => x * c.gain) c.gain 0 := by
    simpa using (hasDerivAt_id (0 : ℝ)).mul_const c.gain
  have h := (show HasDerivAt Real.tanh 1 ((0 : ℝ) * c.gain) by simpa using ht).comp 0 hm
  simpa using h.div_const c.tanhGain

end NormalizedTanh
end DSP
