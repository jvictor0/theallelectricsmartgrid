import DSP.Analysis.CenteredTaylor
import Mathlib.Analysis.SpecialFunctions.Trigonometric.Bounds

noncomputable section
namespace DSP.FirstOrderTaylor

theorem remainder (f f' f'' : ℝ → ℝ) (M : ℝ)
    (h1 : ∀ x, HasDerivAt f (f' x) x) (h2 : ∀ x, HasDerivAt f' (f'' x) x)
    (hb : ∀ x, |f'' x| ≤ M) (x delta : ℝ) :
    |f (x+delta)-f x-f' x*delta| ≤ M*delta^2/2 := by
  have hpath (t : ℝ) : HasDerivAt (fun t => x+t*delta) delta t := by
    simpa using ((hasDerivAt_id t).mul_const delta).const_add x
  have hfirst (t : ℝ) : HasDerivAt (fun t => f (x+t*delta)) (f' (x+t*delta)*delta) t :=
    (h1 _).comp t (hpath t)
  have hsecond (t : ℝ) : HasDerivAt (fun t => f' (x+t*delta)*delta)
      (f'' (x+t*delta)*delta^2) t :=
    (((h2 _).comp t (hpath t)).mul_const delta).congr_deriv (by ring)
  have ht := CenteredTaylor.remainder_one (fun t => f (x+t*delta))
    (fun t => f' (x+t*delta)*delta) (fun t => f'' (x+t*delta)*delta^2) (M*delta^2)
    (fun t _ => hfirst t) (fun t _ => hsecond t) (fun t _ => by
      rw [Real.norm_eq_abs,abs_mul,abs_of_nonneg (sq_nonneg delta)]
      exact mul_le_mul_of_nonneg_right (hb _) (sq_nonneg _))
  simpa only [zero_mul,one_mul,add_zero,Real.norm_eq_abs] using ht

theorem difference (f f' : ℝ → ℝ) (M : ℝ)
    (h : ∀ x, HasDerivAt f (f' x) x) (hb : ∀ x, |f' x| ≤ M) (x y : ℝ) :
    |f x-f y| ≤ M*|x-y| :=
  Convex.norm_image_sub_le_of_norm_hasDerivWithin_le
    (fun z (_ : z ∈ Set.univ) => (h z).hasDerivWithinAt)
    (fun z _ => hb z) convex_univ (Set.mem_univ y) (Set.mem_univ x)

/-- Frequency pulling changes BOTH the base orbit's delayed phase and the
delayed first-order correction. The latter contributes the |nu|*Mv term. -/
theorem delayed_remainder (f f' f'' v v' : ℝ → ℝ) (Mf Mv : ℝ)
    (h1 : ∀ x, HasDerivAt f (f' x) x) (h2 : ∀ x, HasDerivAt f' (f'' x) x)
    (hv : ∀ x, HasDerivAt v (v' x) x)
    (hfBound : ∀ x, |f'' x| ≤ Mf) (hvBound : ∀ x, |v' x| ≤ Mv)
    (x A nu : ℝ) :
    |f (x-A*nu)+A*v (x-A*nu)-(f x+A*(v x-nu*f' x))| ≤
      A^2*(Mf*nu^2/2+Mv*|nu|) := by
  have hf := remainder f f' f'' Mf h1 h2 hfBound x (-A*nu)
  have hdir := difference v v' Mv hv hvBound (x-A*nu) x
  have he : f (x-A*nu)+A*v (x-A*nu)-(f x+A*(v x-nu*f' x)) =
      (f (x+(-A*nu))-f x-f' x*(-A*nu))+A*(v (x-A*nu)-v x) := by
    rw [show x+(-A*nu) = x-A*nu by ring]
    ring
  rw [he]
  apply (abs_add_le _ _).trans
  calc
    _ ≤ Mf*(-A*nu)^2/2+|A| * (Mv*|(x-A*nu)-x|) := by
      rw [abs_mul]
      exact add_le_add hf (mul_le_mul_of_nonneg_left hdir (abs_nonneg A))
    _ = _ := by
      rw [show (x-A*nu)-x = -(A*nu) by ring,abs_neg,abs_mul]
      rw [show |A| * (Mv*(|A| * |nu|)) = Mv*|nu| * |A|^2 by ring,sq_abs]
      ring

theorem modulation_difference (cosine sine phase previousPhase : ℝ) :
    |cosine*Real.cos phase+sine*Real.sin phase-
      (cosine*Real.cos previousPhase+sine*Real.sin previousPhase)| ≤
      (|cosine|+|sine|)*|phase-previousPhase| := by
  rw [show cosine*Real.cos phase+sine*Real.sin phase-
      (cosine*Real.cos previousPhase+sine*Real.sin previousPhase) =
      cosine*(Real.cos phase-Real.cos previousPhase)+sine*(Real.sin phase-Real.sin previousPhase) by ring]
  apply (abs_add_le _ _).trans
  rw [abs_mul,abs_mul]
  exact (add_le_add
    (mul_le_mul_of_nonneg_left (Real.abs_cos_sub_cos_le _ _) (abs_nonneg cosine))
    (mul_le_mul_of_nonneg_left (Real.abs_sin_sub_sin_le _ _) (abs_nonneg sine))).trans_eq (by ring)

/-- In a drifting ansatz the previous sample also has a different mismatch
phase. This extra term must be retained instead of using a frozen-phase delay. -/
theorem moving_delayed_remainder (f f' f'' v v' : ℝ → ℝ) (Mf Mv : ℝ)
    (h1 : ∀ x, HasDerivAt f (f' x) x) (h2 : ∀ x, HasDerivAt f' (f'' x) x)
    (hv : ∀ x, HasDerivAt v (v' x) x)
    (hfBound : ∀ x, |f'' x| ≤ Mf) (hvBound : ∀ x, |v' x| ≤ Mv)
    (x A nu previousDirection modulationError : ℝ)
    (hm : |previousDirection-v (x-A*nu)| ≤ modulationError) :
    |f (x-A*nu)+A*previousDirection-(f x+A*(v x-nu*f' x))| ≤
      A^2*(Mf*nu^2/2+Mv*|nu|)+|A| * modulationError := by
  have he : f (x-A*nu)+A*previousDirection-(f x+A*(v x-nu*f' x)) =
      (f (x-A*nu)+A*v (x-A*nu)-(f x+A*(v x-nu*f' x)))+
        A*(previousDirection-v (x-A*nu)) := by ring
  rw [he]
  apply (abs_add_le _ _).trans
  apply add_le_add (delayed_remainder f f' f'' v v' Mf Mv h1 h2 hv hfBound hvBound x A nu)
  rw [abs_mul]
  exact mul_le_mul_of_nonneg_left hm (abs_nonneg A)

end DSP.FirstOrderTaylor
