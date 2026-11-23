import Mathlib.Analysis.Calculus.MeanValue
import Mathlib.Analysis.Calculus.Deriv.Pow

/-! Centered whole-cell bounds from derivatives on straight segments. The
quadratic remainder retains the factor 1/2 and each coordinate's own width;
using a single box norm here would lose the thin tube's useful anisotropy.
No sampled Hessian is substituted for a uniform bound on the segment. -/

noncomputable section
namespace DSP.CenteredTaylor
open Set Finset
variable {E : Type*} [NormedAddCommGroup E] [NormedSpace ℝ E]

theorem remainder (f f' f'' : ℝ → E) (M : ℝ)
    (hfirst : ∀ t ∈ Icc (0:ℝ) 1, HasDerivAt f (f' t) t)
    (hsecond : ∀ t ∈ Icc (0:ℝ) 1, HasDerivAt f' (f'' t) t)
    (hbound : ∀ t ∈ Icc (0:ℝ) 1, ‖f'' t‖ ≤ M)
    (t : ℝ) (ht : t ∈ Icc (0:ℝ) 1) :
    ‖f t-f 0-t • f' 0‖ ≤ M*t^2/2 := by
  have hd : ∀ t ∈ Icc (0:ℝ) 1, ‖f' t-f' 0‖ ≤ M*t := by
    simpa only [sub_zero] using norm_image_sub_le_of_norm_deriv_le_segment'
      (fun t ht => (hsecond t ht).hasDerivWithinAt)
      (fun t ht => hbound t (Ico_subset_Icc_self ht))
  have he : ∀ t ∈ Icc (0:ℝ) 1,
      HasDerivAt (fun t => f t-f 0-t • f' 0) (f' t-f' 0) t := by
    intro t ht
    simpa only [one_smul, Pi.sub_apply, id_eq] using! ((hfirst t ht).sub_const (f 0)).sub
      ((hasDerivAt_id t).smul_const (f' 0))
  have hB : ∀ t : ℝ, HasDerivAt (fun t => M*t^2/2) (M*t) t := by
    intro t
    convert! (((hasDerivAt_id t).pow 2).const_mul M).div_const 2 using 1
    dsimp only [id_eq]
    ring
  exact image_norm_le_of_norm_deriv_right_le_deriv_boundary
    (fun t ht => (he t ht).continuousAt.continuousWithinAt)
    (fun t ht => (he t (Ico_subset_Icc_self ht)).hasDerivWithinAt)
    (by simp) hB (fun t ht => hd t (Ico_subset_Icc_self ht)) ht

theorem remainder_one (f f' f'' : ℝ → E) (M : ℝ)
    (hfirst : ∀ t ∈ Icc (0:ℝ) 1, HasDerivAt f (f' t) t)
    (hsecond : ∀ t ∈ Icc (0:ℝ) 1, HasDerivAt f' (f'' t) t)
    (hbound : ∀ t ∈ Icc (0:ℝ) 1, ‖f'' t‖ ≤ M) :
    ‖f 1-f 0-f' 0‖ ≤ M/2 := by
  simpa only [one_smul, one_pow, mul_one] using
    remainder f f' f'' M hfirst hsecond hbound 1 (by simp)

def quadraticBound {n : ℕ} (H : Fin n → Fin n → ℝ) (width : Fin n → ℝ) : ℝ :=
  ∑ i, ∑ j, H i j*width i*width j

theorem linear_bound {n : ℕ} (g G v width : Fin n → ℝ)
    (hg : ∀ i, |g i| ≤ G i) (hv : ∀ i, |v i| ≤ width i) :
    |∑ i, g i*v i| ≤ ∑ i, G i*width i := by
  apply (abs_sum_le_sum_abs _ _).trans
  apply sum_le_sum
  intro i _
  rw [abs_mul]
  exact mul_le_mul (hg i) (hv i) (abs_nonneg _) ((abs_nonneg _).trans (hg i))

theorem quadratic_bound {n : ℕ} (h H : Fin n → Fin n → ℝ) (v width : Fin n → ℝ)
    (hh : ∀ i j, |h i j| ≤ H i j) (hv : ∀ i, |v i| ≤ width i) :
    |∑ i, ∑ j, h i j*v i*v j| ≤ quadraticBound H width := by
  apply (abs_sum_le_sum_abs _ _).trans
  apply sum_le_sum
  intro i _
  apply (abs_sum_le_sum_abs _ _).trans
  apply sum_le_sum
  intro j _
  rw [abs_mul, abs_mul]
  exact mul_le_mul
    (mul_le_mul (hh i j) (hv i) (abs_nonneg _) ((abs_nonneg _).trans (hh i j)))
    (hv j) (abs_nonneg _) (mul_nonneg ((abs_nonneg _).trans (hh i j))
      ((abs_nonneg _).trans (hv i)))

/-- Apply the quadratic bound to a path from a cell center to any point in
the cell. The supplied derivatives include all chart and dynamics terms. -/
theorem cell_remainder {n : ℕ} (f f' : ℝ → ℝ)
    (hessian : ℝ → Fin n → Fin n → ℝ) (H : Fin n → Fin n → ℝ)
    (offset width : Fin n → ℝ)
    (hfirst : ∀ t ∈ Icc (0:ℝ) 1, HasDerivAt f (f' t) t)
    (hsecond : ∀ t ∈ Icc (0:ℝ) 1,
      HasDerivAt f' (∑ i, ∑ j, hessian t i j*offset i*offset j) t)
    (hh : ∀ t ∈ Icc (0:ℝ) 1, ∀ i j, |hessian t i j| ≤ H i j)
    (hw : ∀ i, |offset i| ≤ width i) :
    |f 1-f 0-f' 0| ≤ quadraticBound H width/2 := by
  exact remainder_one f f' (fun t => ∑ i, ∑ j, hessian t i j*offset i*offset j)
    (quadraticBound H width) hfirst hsecond
    (fun t ht => quadratic_bound (hessian t) H offset width (hh t ht) hw)

end DSP.CenteredTaylor
