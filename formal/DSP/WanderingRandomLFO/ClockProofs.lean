import DSP.WanderingRandomLFO.PhaseLaw
import Mathlib.Algebra.BigOperators.Fin

namespace DSP.WanderingRandomLFO

open SmoothRandomLFO
open scoped BigOperators

private theorem exactSamplePhase_succ (lambda alpha : ℝ) (draw : ℕ → ℝ) (n : ℕ) (i : ℝ) :
    exactSamplePhase lambda alpha draw (n+1) i =
      exactSamplePhase lambda alpha draw n ((frameSamples : ℝ) + i * alpha) +
        (i * (1-alpha)) * min 1 (draw (n+1)) := by
  simp only [exactSamplePhase, exactFramePhase, phaseAfter]
  rw [show exactSpeed lambda alpha draw (n+1+1) =
    smoothVelocity alpha (exactSpeed lambda alpha draw (n+1)) (min 1 (draw (n+1))) from rfl]
  simp only [smoothVelocity, fma_eq]
  ring

private theorem clockConstant_succ (lambda alpha : ℝ) (ha : alpha ≠ 1) (n : ℕ) (i : ℝ) :
    clockConstant lambda alpha (n+1) i =
      clockConstant lambda alpha n ((frameSamples : ℝ) + i * alpha) := by
  simp only [clockConstant, pow_succ]
  apply congrArg (fun x : ℝ => x / max 1 lambda)
  have h : 1 - alpha ≠ 0 := sub_ne_zero.mpr (Ne.symm ha)
  field_simp [h]
  ring

private theorem clockCoefficient_castSucc (alpha : ℝ) (n : ℕ) (i : ℝ) (j : Fin (n+1)) :
    clockCoefficient alpha (n+1) i j.castSucc =
      clockCoefficient alpha n ((frameSamples : ℝ) + i * alpha) j := by
  unfold clockCoefficient
  simp only [Fin.val_castSucc]
  rw [show n+1-j.val = (n-j.val)+1 by omega, pow_succ]
  ring

/-- The exact sample phase is a deterministic offset plus independent draws
whose coefficients approach one frame's length with the age of the draw. -/
theorem exactSamplePhase_eq (lambda alpha : ℝ) (ha : alpha ≠ 1) (draw : ℕ → ℝ) (n : ℕ) (i : ℝ) :
    exactSamplePhase lambda alpha draw n i =
      clockConstant lambda alpha n i + ∑ j : Fin (n+1),
        clockCoefficient alpha n i j * min 1 (draw j) := by
  induction n generalizing i with
  | zero =>
    rw [Fin.sum_univ_eq_sum_range (fun j => clockCoefficient alpha 0 i j * min 1 (draw j)),
      Finset.sum_range_one]
    simp only [exactSamplePhase, phaseAfter, exactFramePhase, exactSpeed, smoothVelocity, fma_eq,
      clockConstant, clockCoefficient, Nat.sub_zero, pow_zero, mul_one, zero_add]
    ring
  | succ n ih =>
    rw [exactSamplePhase_succ, ih, clockConstant_succ lambda alpha ha]
    conv_rhs =>
      arg 2
      rw [Fin.sum_univ_castSucc]
    simp only [Fin.val_castSucc]
    rw [show clockCoefficient alpha (n+1) i (Fin.last (n+1)) = i * (1-alpha) by
      simp only [clockCoefficient, Fin.val_last, Nat.sub_self, pow_zero, mul_one]; ring,
      Fin.val_last]
    have hs : (∑ j : Fin (n+1),
        clockCoefficient alpha n ((frameSamples : ℝ) + i * alpha) j * min 1 (draw j)) =
        ∑ j : Fin (n+1), clockCoefficient alpha (n+1) i j * min 1 (draw j) := by
      apply Finset.sum_congr rfl
      intro j _
      exact congrArg (fun c : ℝ => c * min 1 (draw j)) (clockCoefficient_castSucc alpha n i j).symm
    rw [hs]
    ring

end DSP.WanderingRandomLFO
