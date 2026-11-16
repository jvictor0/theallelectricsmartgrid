import DSP.Transfer.Basic
import Mathlib.FieldTheory.RatFunc.Basic
import Mathlib.LinearAlgebra.Matrix.Charpoly.Coeff
import Mathlib.Tactic.FieldSimp

namespace DSP.StateSpace

open Matrix Polynomial

variable {n : ℕ} {K : Type} [Field K]

noncomputable def transferDenominator (s : StateSpace n K) : K[X] := (Matrix.of s.A).charpoly

noncomputable def transferNumerator (s : StateSpace n K) : K[X] :=
  Polynomial.C s.D * s.transferDenominator +
    (fun i => Polynomial.C (s.C i)) ⬝ᵥ ((Matrix.of s.A).charmatrix.adjugate *ᵥ (fun i => Polynomial.C (s.B i)))

noncomputable def rationalTransfer (s : StateSpace n K) : RatFunc K :=
  algebraMap K[X] (RatFunc K) s.transferNumerator / algebraMap K[X] (RatFunc K) s.transferDenominator

theorem transferDenominator_ne_zero (s : StateSpace n K) : s.transferDenominator ≠ 0 :=
  (Matrix.charpoly_monic (Matrix.of s.A)).ne_zero

theorem eval_charmatrix (s : StateSpace n K) (z : K) :
    (Matrix.of s.A).charmatrix.map (Polynomial.evalRingHom z) = s.resolvent z := by
  ext i j
  by_cases h : i = j
  · subst j; simp [resolvent]
  · simp [resolvent, h]

theorem transferDenominator_eval (s : StateSpace n K) (z : K) :
    s.transferDenominator.eval z = (s.resolvent z).det := by
  change (Polynomial.evalRingHom z) (Matrix.of s.A).charmatrix.det = _
  rw [RingHom.map_det, RingHom.mapMatrix_apply, s.eval_charmatrix]

theorem transferNumerator_eval (s : StateSpace n K) (z : K) :
    s.transferNumerator.eval z =
      s.D * (s.resolvent z).det + s.C ⬝ᵥ ((s.resolvent z).adjugate *ᵥ s.B) := by
  let f := Polynomial.evalRingHom z
  have hAdj : (Matrix.of s.A).charmatrix.adjugate.map f = (s.resolvent z).adjugate := by
    change f.mapMatrix (Matrix.of s.A).charmatrix.adjugate = _
    rw [RingHom.map_adjugate, RingHom.mapMatrix_apply, s.eval_charmatrix]
  change f (Polynomial.C s.D * s.transferDenominator + _) = _
  rw [map_add, map_mul]
  have hf : f (Polynomial.C s.D) = s.D := by simp [f]
  rw [hf]
  change s.D * s.transferDenominator.eval z + f _ = _
  rw [transferDenominator_eval]
  congr 1
  rw [RingHom.map_dotProduct]
  have hB : f ∘ (fun i => Polynomial.C (s.B i)) = s.B := by funext i; simp [f]
  have hC : f ∘ (fun i => Polynomial.C (s.C i)) = s.C := by funext i; simp [f]
  rw [hC]
  congr 1
  funext i
  rw [Function.comp_apply, RingHom.map_mulVec, hAdj, hB]

theorem transfer_eq_rationalEvaluation (s : StateSpace n K) (z : K) (hz : s.RegularAt z) :
    s.transfer z = s.transferNumerator.eval z / s.transferDenominator.eval z := by
  rw [transferNumerator_eval, transferDenominator_eval]
  simp only [transfer, modeState, Matrix.inv_def, Ring.inverse_eq_inv, smul_mulVec,
    dotProduct_smul, smul_eq_mul]
  change (s.resolvent z).det ≠ 0 at hz
  field_simp [hz]
  ring

end DSP.StateSpace
