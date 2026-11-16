import DSP.Transfer.Analog
import DSP.Transfer.Digital
import DSP.Transfer.IIR
import Mathlib.Algebra.BigOperators.Field
import Mathlib.Tactic.LinearCombination

namespace DSP

/-- A proper rational presentation with a monic denominator of degree n+1.
The numerator's leading coefficient is the direct feedthrough. -/
structure RationalCoefficients (n : ℕ) (K : Type) where
  a : Fin (n + 1) → K
  b : Fin (n + 1) → K
  d : K

namespace RationalCoefficients

open Matrix Polynomial

variable {n : ℕ} {K : Type} [Field K]

def system (c : RationalCoefficients n K) : StateSpace (n + 1) K where
  A i j := Fin.lastCases (-c.a j) (fun k => if j = k.succ then 1 else 0) i
  B := Fin.lastCases 1 (fun _ => 0)
  C i := c.b i - c.d * c.a i
  D := c.d

noncomputable def denominator (c : RationalCoefficients n K) : K[X] :=
  X ^ (n + 1) + ∑ i, Polynomial.C (c.a i) * X ^ i.val

noncomputable def numerator (c : RationalCoefficients n K) : K[X] :=
  Polynomial.C c.d * X ^ (n + 1) + ∑ i, Polynomial.C (c.b i) * X ^ i.val

def evalDenominator (c : RationalCoefficients n K) (z : K) : K :=
  z ^ (n + 1) + ∑ i, c.a i * z ^ i.val

def evalNumerator (c : RationalCoefficients n K) (z : K) : K :=
  c.d * z ^ (n + 1) + ∑ i, c.b i * z ^ i.val

theorem denominator_eval (c : RationalCoefficients n K) (z : K) :
    (c.denominator).eval z = c.evalDenominator z := by simp [denominator, evalDenominator, Polynomial.eval_finsetSum]

theorem numerator_eval (c : RationalCoefficients n K) (z : K) :
    (c.numerator).eval z = c.evalNumerator z := by simp [numerator, evalNumerator, Polynomial.eval_finsetSum]

theorem stateEquation_iff (c : RationalCoefficients n K) (z : K) (v : Fin (n + 1) → K) :
    z • v = c.system.A *ᵥ v + c.system.B ↔
      (∀ i : Fin n, z * v i.castSucc = v i.succ) ∧
        z * v (Fin.last n) = -(∑ i, c.a i * v i) + 1 := by
  constructor
  · intro h
    constructor
    · intro i
      simpa [system, mulVec, dotProduct] using congrFun h i.castSucc
    · simpa [system, mulVec, dotProduct, Finset.sum_neg_distrib] using congrFun h (Fin.last n)
  · rintro ⟨h, hl⟩
    funext i
    refine Fin.lastCases ?_ (fun j => ?_) i
    · simpa [system, mulVec, dotProduct, Finset.sum_neg_distrib] using hl
    · simpa [system, mulVec, dotProduct] using h j

theorem chain_eq (z : K) (v : Fin (n + 1) → K)
    (h : ∀ i : Fin n, z * v i.castSucc = v i.succ) (i : Fin (n + 1)) :
    v i = z ^ i.val * v 0 := by
  induction i using Fin.induction with
  | zero => simp
  | succ i ih => rw [← h i, ih]; simp [pow_succ, mul_assoc, mul_left_comm]

theorem observation_eq (c : RationalCoefficients n K) (z w : K)
    (h : c.evalDenominator z * w = 1) :
    c.system.C ⬝ᵥ (fun i => z ^ i.val * w) + c.system.D = c.evalNumerator z * w := by
  have hs : c.system.C ⬝ᵥ (fun i => z ^ i.val * w) =
      ((∑ i, c.b i * z ^ i.val) - c.d * ∑ i, c.a i * z ^ i.val) * w := by
    simp only [system, dotProduct, sub_mul, ← mul_assoc, Finset.sum_sub_distrib,
      ← Finset.sum_mul]
    simp only [mul_assoc, ← Finset.mul_sum]
  rw [hs]
  dsimp [system, evalNumerator, evalDenominator] at *
  linear_combination -c.d * h

theorem modeEquations_iff (c : RationalCoefficients n K) (z gain : K)
    (hz : c.evalDenominator z ≠ 0) :
    c.system.ModeEquations z gain ↔ gain = c.evalNumerator z / c.evalDenominator z := by
  constructor
  · rintro ⟨v, hv, hg⟩
    obtain ⟨hc, hl⟩ := (c.stateEquation_iff z v).mp hv
    obtain ⟨w, hw⟩ : ∃ w, v = fun i => z ^ i.val * w :=
      ⟨v 0, funext (chain_eq z v hc)⟩
    rw [hw] at hl hg
    have hden : c.evalDenominator z * w = 1 := by
      dsimp only at hl
      simp only [evalDenominator, add_mul, Finset.sum_mul]
      simp only [Fin.val_last] at hl
      simp only [← mul_assoc] at hl
      linear_combination hl
    have hw0 : w = (c.evalDenominator z)⁻¹ := by
      apply (mul_left_cancel₀ hz)
      rw [hden, mul_inv_cancel₀ hz]
    rw [hg, c.observation_eq z w hden, hw0, div_eq_mul_inv]
  · intro hg
    let w := (c.evalDenominator z)⁻¹
    have hden : c.evalDenominator z * w = 1 := mul_inv_cancel₀ hz
    refine ⟨fun i => z ^ i.val * w, (c.stateEquation_iff z _).mpr ⟨?_, ?_⟩, ?_⟩
    · intro i
      simp only [Fin.val_castSucc, Fin.val_succ, pow_succ]
      ring
    · change z * (z ^ n * w) = _
      dsimp [evalDenominator] at hden
      simp only [add_mul, Finset.sum_mul] at hden
      simp only [← mul_assoc]
      linear_combination hden
    · rw [c.observation_eq z w hden, hg]
      exact div_eq_mul_inv _ _

theorem realizeAnalog_transfer (c : RationalCoefficients n ℂ) (s gain : ℂ)
    (hs : c.evalDenominator s ≠ 0) :
    c.system.HasAnalogMode s gain ↔ gain = c.evalNumerator s / c.evalDenominator s :=
  (c.system.analogMode_iff_equations s gain).trans (c.modeEquations_iff s gain hs)

theorem realizeDigital_transfer (c : RationalCoefficients n K) (z gain : K)
    (hz : c.evalDenominator z ≠ 0) :
    c.system.HasDigitalMode z gain ↔ gain = c.evalNumerator z / c.evalDenominator z :=
  (c.system.digitalMode_iff_equations z gain).trans (c.modeEquations_iff z gain hz)

end RationalCoefficients
end DSP
