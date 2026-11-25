import Mathlib.Data.Real.Basic
import Mathlib.Data.Fin.VecNotation
import Mathlib.Algebra.BigOperators.Fin
import Mathlib.Tactic.NormNum
import Mathlib.Tactic.Ring

namespace DSP

def givensPair {R : Type} [Add R] [Sub R] [Mul R] (c s x y : R) : R × R :=
  (c * x - s * y, s * x + c * y)

/-- Rotations in planes 01, 23, 02 and 13. The unit complex coefficients are
the ratios of Gaussian primes to their conjugates, with distinct norms 29,
53, 73 and 97. Each layer connects the channel pairs across the previous layer. -/
def squiggleBoyReverbMix {R : Type} [Add R] [Sub R] [Neg R] [Mul R] [Div R] [OfScientific R]
    (input : Fin 4 → R) : Fin 4 → R :=
  let a := givensPair (21.0 / 29.0) (20.0 / 29.0) (input 0) (input 1)
  let b := givensPair (45.0 / 53.0) (28.0 / 53.0) (input 2) (input 3)
  let c := givensPair (55.0 / 73.0) (-(48.0 / 73.0)) a.1 b.1
  let d := givensPair (65.0 / 97.0) (72.0 / 97.0) a.2 b.2
  ![c.1, d.1, c.2, d.2]

theorem squiggleBoyReverbMix_energy (input : Fin 4 → ℝ) :
    (∑ ch : Fin 4, (squiggleBoyReverbMix input ch) ^ 2) = ∑ ch : Fin 4, input ch ^ 2 := by
  have sumFour (v : Fin 4 → ℝ) : (∑ ch : Fin 4, v ch ^ 2) =
      v 0 ^ 2 + (v 1 ^ 2 + (v 2 ^ 2 + v 3 ^ 2)) := by
    simp [Fin.sum_univ_succ]
  rw [sumFour, sumFour]
  norm_num [squiggleBoyReverbMix, givensPair, Matrix.cons_val_two, Matrix.cons_val_three,
    Matrix.vecHead, Matrix.vecTail, Function.comp_apply]
  ring

theorem squiggleBoyReverbMix_not_involution :
    ∃ input : Fin 4 → ℝ, squiggleBoyReverbMix (squiggleBoyReverbMix input) ≠ input := by
  refine ⟨![1, 0, 0, 0], ?_⟩
  intro h
  have h₀ := congrFun h (0 : Fin 4)
  norm_num [squiggleBoyReverbMix, givensPair, Matrix.cons_val_two, Matrix.cons_val_three,
    Matrix.vecHead, Matrix.vecTail, Function.comp_apply] at h₀

end DSP
