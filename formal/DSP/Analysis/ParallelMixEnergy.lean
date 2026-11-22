import Mathlib.Analysis.InnerProductSpace.Basic
import Mathlib.Analysis.Real.Sqrt
import Mathlib.Algebra.BigOperators.Group.Finset.Sigma
import Mathlib.Algebra.Order.BigOperators.Group.Finset
import Mathlib.Tactic.NormNum
import Mathlib.Tactic.Ring

/-! Energy of equal-gain parallel branches in a real inner-product space.
The same identity applies to finite sample vectors, square-summable impulse
responses, and spectra with an inner product weighted over a frequency band. -/

open scoped BigOperators RealInnerProductSpace

namespace DSP.ParallelMix

noncomputable section

variable {E : Type*} [NormedAddCommGroup E] [InnerProductSpace ℝ E] {n : ℕ}

/-- Equal mixing with the power-normalizing gain `1 / √n` on every branch. -/
def normalizedMix (branches : Fin n → E) : E :=
  (Real.sqrt (n : ℝ))⁻¹ • ∑ i, branches i

/-- Each ordered pair of distinct branches contributes its cross energy.
Equivalently, each unordered pair contributes twice its inner product. -/
def crossEnergy (branches : Fin n → E) : ℝ :=
  ∑ p ∈ (Finset.univ : Finset (Fin n)).offDiag, ⟪branches p.1, branches p.2⟫

/-- The normalized mix contains the average branch energy plus the cross terms. -/
theorem energy_eq (branches : Fin n → E) :
    ‖normalizedMix branches‖ ^ 2 =
      ((∑ i, ‖branches i‖ ^ 2) + crossEnergy branches) / (n : ℝ) := by
  classical
  have hsum : ‖∑ i, branches i‖ ^ 2 = (∑ i, ‖branches i‖ ^ 2) + crossEnergy branches := by
    let indices : Finset (Fin n) := Finset.univ
    calc
      ‖∑ i, branches i‖ ^ 2 = ⟪∑ i, branches i, ∑ i, branches i⟫ :=
        (real_inner_self_eq_norm_sq _).symm
      _ = ∑ p ∈ indices ×ˢ indices, ⟪branches p.1, branches p.2⟫ := by
        simp only [indices, Finset.sum_product, sum_inner, inner_sum]
        exact Finset.sum_comm
      _ = _ := by
        rw [← Finset.diag_union_offDiag, Finset.sum_union (Finset.disjoint_diag_offDiag _), Finset.sum_diag]
        simp only [indices, real_inner_self_eq_norm_sq, crossEnergy]
  rw [normalizedMix, norm_smul, Real.norm_eq_abs, mul_pow, sq_abs, inv_pow,
    Real.sq_sqrt (Nat.cast_nonneg n), hsum, div_eq_mul_inv, mul_comm]

/-- For unit-energy branches, all deviation from unity comes from cross energy. -/
theorem unit_energy_eq (hn : 0 < n) (branches : Fin n → E) (unit : ∀ i, ‖branches i‖ = 1) :
    ‖normalizedMix branches‖ ^ 2 = 1 + crossEnergy branches / (n : ℝ) := by
  have hn' : (n : ℝ) ≠ 0 := by exact_mod_cast Nat.ne_of_gt hn
  have hsum : (∑ i, ‖branches i‖ ^ 2) = (n : ℝ) := by simp [unit]
  rw [energy_eq, hsum, add_div, div_self hn']

/-- Uniformly small pair correlations give a sharp bound on the power error. -/
theorem unit_energy_error_le (hn : 0 < n) (branches : Fin n → E) (unit : ∀ i, ‖branches i‖ = 1)
    (ε : ℝ) (correlation : ∀ i j, i ≠ j → |⟪branches i, branches j⟫| ≤ ε) :
    |‖normalizedMix branches‖ ^ 2 - 1| ≤ ((n : ℝ) - 1) * ε := by
  have hn' : 0 < (n : ℝ) := by exact_mod_cast hn
  rw [unit_energy_eq hn branches unit, add_sub_cancel_left, abs_div, abs_of_pos hn']
  apply (div_le_iff₀ hn').mpr
  let pairs := (Finset.univ : Finset (Fin n)).offDiag
  have hmul : n ≤ n * n := by simpa only [Nat.mul_one] using Nat.mul_le_mul_left n hn
  have hcard : (pairs.card : ℝ) = (n : ℝ) * (n - 1) := by
    simp only [pairs, Finset.offDiag_card, Finset.card_univ, Fintype.card_fin,
      Nat.cast_sub hmul, Nat.cast_mul]
    ring
  calc
    |crossEnergy branches| ≤ ∑ p ∈ pairs, |⟪branches p.1, branches p.2⟫| :=
      Finset.abs_sum_le_sum_abs _ _
    _ ≤ ∑ _p ∈ pairs, ε :=
      Finset.sum_le_sum fun p hp => correlation p.1 p.2 (Finset.mem_offDiag.mp hp).2.2
    _ = ((n : ℝ) - 1) * ε * (n : ℝ) := by
      rw [Finset.sum_const, nsmul_eq_mul, hcard]
      ring

/-- Zero pair correlations give exactly unity energy. -/
theorem unit_energy_of_orthogonal (hn : 0 < n) (branches : Fin n → E) (unit : ∀ i, ‖branches i‖ = 1)
    (orthogonal : ∀ i j, i ≠ j → ⟪branches i, branches j⟫ = 0) : ‖normalizedMix branches‖ ^ 2 = 1 := by
  rw [unit_energy_eq hn branches unit]
  have hcross : crossEnergy branches = 0 :=
    Finset.sum_eq_zero fun p hp => orthogonal p.1 p.2 (Finset.mem_offDiag.mp hp).2.2
  rw [hcross, zero_div, add_zero]

/-- For two unit-energy branches the energy error is their zero-lag correlation. -/
theorem two_branch_energy (x y : E) (hx : ‖x‖ = 1) (hy : ‖y‖ = 1) :
    ‖(Real.sqrt 2)⁻¹ • (x + y)‖ ^ 2 = 1 + ⟪x, y⟫ := by
  rw [norm_smul, Real.norm_eq_abs, mul_pow, sq_abs, inv_pow,
    Real.sq_sqrt (by norm_num : 0 ≤ (2 : ℝ)), norm_add_sq_real, hx, hy]
  ring

end
end DSP.ParallelMix
