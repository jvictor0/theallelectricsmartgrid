import DSP.IfLess
import Mathlib.Data.Real.Basic

namespace DSP

/-- Exact ordered selection, shared by real-number response and filter proofs. -/
noncomputable instance : IfLess ℝ := ⟨fun a b x y => if a < b then x else y⟩

noncomputable instance : LawfulIfLess ℝ where
  less a b := decide (a < b)
  ifLess_eq a b x y := by simp [IfLess.ifLess]

end DSP
