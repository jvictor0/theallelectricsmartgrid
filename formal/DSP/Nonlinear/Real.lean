import DSP.Nonlinear
import DSP.FMA.Algebra
import DSP.IfLess.Real
import Mathlib.Analysis.Complex.Trigonometric
import Mathlib.Analysis.SpecialFunctions.Sqrt

namespace DSP

noncomputable instance : Tanh ℝ := ⟨Real.tanh⟩
noncomputable instance : SquareRoot ℝ := ⟨Real.sqrt⟩

@[simp] theorem tanh_real (x : ℝ) : Tanh.tanh x = Real.tanh x := rfl
@[simp] theorem sqrt_real (x : ℝ) : SquareRoot.sqrt x = Real.sqrt x := rfl

end DSP
