import DSP.Transcendental
import Mathlib.Analysis.SpecialFunctions.Trigonometric.Basic
import Mathlib.Analysis.SpecialFunctions.Exp

noncomputable section
namespace DSP

instance : Trig ℝ := ⟨fun x => Real.sin (2 * Real.pi * x),
  fun x => Real.cos (2 * Real.pi * x), fun x => Real.tan (Real.pi * x)⟩
instance : ExpDecay ℝ := ⟨fun x => Real.exp (-(2 * Real.pi * x))⟩

@[simp] theorem sin2pi_real (x : ℝ) : Trig.sin2pi x = Real.sin (2 * Real.pi * x) := rfl
@[simp] theorem cos2pi_real (x : ℝ) : Trig.cos2pi x = Real.cos (2 * Real.pi * x) := rfl
@[simp] theorem tanPi_real (x : ℝ) : Trig.tanPi x = Real.tan (Real.pi * x) := rfl
@[simp] theorem expDecay_real (x : ℝ) : ExpDecay.expNegTwoPi x = Real.exp (-(2 * Real.pi * x)) := rfl

end DSP
