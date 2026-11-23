import DSP.Analysis.ComplexPolynomialBounds

noncomputable section
namespace DSP.FinitePrimitive
open Finset DSP.ComplexPolynomialBounds
variable {ι : Type*} [Fintype ι]

def value (a : ι → ℂ) (degree : ι → ℕ) (z : ℂ) := ∑ i, primitiveTerm (a i) (degree i) z
def slope (a : ι → ℂ) (degree : ι → ℕ) (z : ℂ) := ∑ i, a i*z^(degree i)
def bound0 (a : ι → ℂ) (degree : ι → ℕ) : ℝ := ∑ i, ‖a i‖/((degree i:ℝ)+1)
def bound1 (a : ι → ℂ) : ℝ := ∑ i, ‖a i‖
def bound2 (a : ι → ℂ) (degree : ι → ℕ) : ℝ := ∑ i, ‖a i‖*(degree i:ℝ)

theorem value_bound (a : ι → ℂ) (degree : ι → ℕ) (z : ℂ) (hz : ‖z‖ ≤ 1) :
    ‖value a degree z‖ ≤ bound0 a degree := by
  apply (norm_sum_le _ _).trans
  exact sum_le_sum (fun i _ => primitiveTerm_bound _ _ _ hz)

theorem slope_bound (a : ι → ℂ) (degree : ι → ℕ) (z : ℂ) (hz : ‖z‖ ≤ 1) :
    ‖slope a degree z‖ ≤ bound1 a := by
  apply (norm_sum_le _ _).trans
  apply sum_le_sum
  intro i _
  rw [norm_mul,norm_pow]
  exact (mul_le_mul_of_nonneg_left (pow_le_one₀ (norm_nonneg z) hz) (norm_nonneg _)).trans_eq (mul_one _)

theorem remainder (a : ι → ℂ) (degree : ι → ℕ) (z w : ℂ)
    (hz : ‖z‖ ≤ 1) (hw : ‖w‖ ≤ 1) :
    ‖value a degree w-value a degree z-slope a degree z*(w-z)‖ ≤
      bound2 a degree*‖w-z‖^2/2 := by
  simp only [value,slope,Finset.sum_mul,← sum_sub_distrib]
  apply (norm_sum_le _ _).trans
  calc
    _ ≤ ∑ i, (‖a i‖*(degree i:ℝ)/2)*‖w-z‖^2 :=
      sum_le_sum (fun i _ => primitiveTerm_remainder _ _ _ _ hz hw)
    _ = _ := by simp only [bound2,← Finset.sum_mul,← Finset.sum_div]; ring

end DSP.FinitePrimitive
