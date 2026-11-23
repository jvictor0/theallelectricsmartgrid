import Mathlib.Analysis.SpecialFunctions.Trigonometric.Angle
import Mathlib.Analysis.SpecialFunctions.Trigonometric.Deriv

/-! Exact finite real Fourier proposals for profiles and correction
potentials. Coefficients are rational; differentiation stays in the same
representation. Being a finite polynomial makes no invariance claim. -/

namespace DSP

structure TrigPolynomial (count : ℕ) where
  cosine : Fin count → ℚ
  sine : Fin count → ℚ

namespace TrigPolynomial
open Finset
variable {count : ℕ}

noncomputable def eval (p : TrigPolynomial count) (θ : Real.Angle) : ℝ :=
  ∑ i, ((p.cosine i:ℝ)*(i.val • θ).cos+(p.sine i:ℝ)*(i.val • θ).sin)

def derivative (p : TrigPolynomial count) : TrigPolynomial count :=
  ⟨fun i => i.val*p.sine i, fun i => -i.val*p.cosine i⟩

def absoluteBound (p : TrigPolynomial count) : ℚ := ∑ i, (|p.cosine i|+|p.sine i|)

def lowerBound (p : TrigPolynomial count) : ℚ :=
  ∑ i, if i.val = 0 then p.cosine i else -(|p.cosine i|+|p.sine i|)

theorem continuous_eval (p : TrigPolynomial count) : Continuous p.eval := by
  apply continuous_finsetSum
  intro i _
  exact ((Real.Angle.continuous_cos.comp (continuous_id.nsmul i.val)).const_mul _).add
    ((Real.Angle.continuous_sin.comp (continuous_id.nsmul i.val)).const_mul _)

theorem eval_coe (p : TrigPolynomial count) (θ : ℝ) :
    p.eval (θ:Real.Angle) =
      ∑ i, ((p.cosine i:ℝ)*Real.cos (i.val*θ)+(p.sine i:ℝ)*Real.sin (i.val*θ)) := by
  simp only [eval, ← Real.Angle.natCast_mul_eq_nsmul, Real.Angle.cos_coe, Real.Angle.sin_coe]

theorem hasDerivAt_eval (p : TrigPolynomial count) (θ : ℝ) :
    HasDerivAt (fun t : ℝ => p.eval (t:Real.Angle)) (p.derivative.eval (θ:Real.Angle)) θ := by
  simp_rw [eval_coe]
  apply HasDerivAt.fun_sum
  intro i _
  have h := (hasDerivAt_id θ).const_mul (i.val:ℝ)
  apply ((h.cos.const_mul (p.cosine i:ℝ)).add (h.sin.const_mul (p.sine i:ℝ))).congr_deriv
  simp only [derivative, Rat.cast_mul, Rat.cast_neg, Rat.cast_natCast, id_eq]
  ring

private theorem abs_cos (θ : Real.Angle) : |θ.cos| ≤ 1 := by
  induction θ using Real.Angle.induction_on with
  | _ θ => exact Real.abs_cos_le_one θ

private theorem abs_sin (θ : Real.Angle) : |θ.sin| ≤ 1 := by
  induction θ using Real.Angle.induction_on with
  | _ θ => exact Real.abs_sin_le_one θ

theorem term_bound (a b : ℝ) (θ : Real.Angle) :
    |a*θ.cos+b*θ.sin| ≤ |a|+|b| := by
  calc
    _ ≤ |a*θ.cos|+|b*θ.sin| := abs_add_le _ _
    _ = |a| * |θ.cos| + |b| * |θ.sin| := by rw [abs_mul, abs_mul]
    _ ≤ |a| * 1 + |b| * 1 := add_le_add
      (mul_le_mul_of_nonneg_left (abs_cos θ) (abs_nonneg a))
      (mul_le_mul_of_nonneg_left (abs_sin θ) (abs_nonneg b))
    _ = _ := by ring

theorem eval_absoluteBound (p : TrigPolynomial count) (θ : Real.Angle) :
    |p.eval θ| ≤ (p.absoluteBound:ℝ) := by
  unfold eval absoluteBound
  push_cast
  exact (abs_sum_le_sum_abs _ _).trans (sum_le_sum (fun i _ => term_bound _ _ _))

theorem lowerBound_le (p : TrigPolynomial count) (θ : Real.Angle) :
    (p.lowerBound:ℝ) ≤ p.eval θ := by
  unfold eval lowerBound
  push_cast
  apply sum_le_sum
  intro i _
  split_ifs with hi
  · simp [hi]
  · push_cast
    have h := term_bound (p.cosine i:ℝ) (p.sine i:ℝ) (i.val • θ)
    have hneg := neg_abs_le ((p.cosine i:ℝ)*(i.val • θ).cos+(p.sine i:ℝ)*(i.val • θ).sin)
    linarith

end TrigPolynomial
end DSP
