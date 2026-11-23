import DSP.Analysis.IntervalTaylor

/-! Coordinatewise mean-value bounds on a rectangular cell. These consume
actual partial derivatives; sampled Jacobian entries are not sufficient.
The same lemma bounds a function from its gradient, or a gradient entry
from a row of its Hessian. -/

noncomputable section
namespace DSP.CoordinateBounds
open Set Finset
variable {n : ℕ}

def InBox (center width x : Fin n → ℝ) : Prop :=
  ∀ i, |x i-center i| ≤ width i

theorem update_mem (center width x : Fin n → ℝ) (hx : InBox center width x)
    (j : Fin n) (t : ℝ) (ht : t ∈ Icc (center j-width j) (center j+width j)) :
    InBox center width (Function.update x j t) := by
  intro i
  by_cases h : i = j
  · subst i
    simp only [Function.update_self]
    exact abs_le.mpr ⟨by linarith [ht.1],by linarith [ht.2]⟩
  · simpa [Function.update_of_ne h] using hx i

theorem axis_bound (center width : Fin n → ℝ) (f : (Fin n → ℝ) → ℝ)
    (df : (Fin n → ℝ) → Fin n → ℝ) (G : Fin n → ℝ)
    (hd : ∀ x, InBox center width x → ∀ j,
      HasDerivAt (fun t => f (Function.update x j t)) (df x j) (x j))
    (hG : ∀ x, InBox center width x → ∀ j, |df x j| ≤ G j)
    (x : Fin n → ℝ) (hx : InBox center width x) (j : Fin n) (y : ℝ)
    (hy : |y-center j| ≤ width j) :
    |f (Function.update x j y)-f x| ≤ G j*|y-x j| := by
  let s := Icc (center j-width j) (center j+width j)
  have hmem t (ht : t ∈ s) := update_mem center width x hx j t ht
  have hderiv t (ht : t ∈ s) : HasDerivAt (fun t => f (Function.update x j t))
      (df (Function.update x j t) j) t := by
    simpa only [Function.update_self,Function.update_idem] using
      hd (Function.update x j t) (hmem t ht) j
  have hx' : x j ∈ s := by
    have h := abs_le.mp (hx j)
    exact ⟨by linarith [h.1],by linarith [h.2]⟩
  have hy' : y ∈ s := by
    have h := abs_le.mp hy
    exact ⟨by linarith [h.1],by linarith [h.2]⟩
  simpa only [Real.norm_eq_abs,Function.update_eq_self] using
    Convex.norm_image_sub_le_of_norm_hasDerivWithin_le
      (fun t ht => (hderiv t ht).hasDerivWithinAt)
      (fun t ht => hG _ (hmem t ht) j) (convex_Icc _ _) hx' hy'

/-- Replace a finite set of coordinates, one at a time. -/
theorem patch_bound (center width : Fin n → ℝ) (f : (Fin n → ℝ) → ℝ)
    (df : (Fin n → ℝ) → Fin n → ℝ) (G : Fin n → ℝ)
    (hd : ∀ x, InBox center width x → ∀ j,
      HasDerivAt (fun t => f (Function.update x j t)) (df x j) (x j))
    (hG : ∀ x, InBox center width x → ∀ j, |df x j| ≤ G j)
    (x y : Fin n → ℝ) (hx : InBox center width x) (hy : InBox center width y)
    (s : Finset (Fin n)) :
    |f (fun j => if j ∈ s then y j else x j)-f x| ≤ ∑ j ∈ s, G j*|y j-x j| := by
  induction s using Finset.induction_on with
  | empty => simp
  | @insert j s hj ih =>
    let z : Fin n → ℝ := fun k => if k ∈ s then y k else x k
    have hz : InBox center width z := by
      intro k
      by_cases hk : k ∈ s
      · simpa [z,hk] using hy k
      · simpa [z,hk] using hx k
    have he : (fun k => if k ∈ insert j s then y k else x k) = Function.update z j (y j) := by
      funext k
      by_cases hk : k = j
      · subst k; simp
      · simp [z,hk]
    rw [he,Finset.sum_insert hj]
    have ha := axis_bound center width f df G hd hG z hz j (y j) (hy j)
    have hzj : z j = x j := by simp [z,hj]
    rw [hzj] at ha
    have ht := abs_add_le (f (Function.update z j (y j))-f z) (f z-f x)
    rw [sub_add_sub_cancel] at ht
    exact ht.trans (add_le_add ha ih)

theorem box_bound (center width : Fin n → ℝ) (f : (Fin n → ℝ) → ℝ)
    (df : (Fin n → ℝ) → Fin n → ℝ) (G : Fin n → ℝ)
    (hd : ∀ x, InBox center width x → ∀ j,
      HasDerivAt (fun t => f (Function.update x j t)) (df x j) (x j))
    (hG : ∀ x, InBox center width x → ∀ j, |df x j| ≤ G j)
    (x y : Fin n → ℝ) (hx : InBox center width x) (hy : InBox center width y) :
    |f y-f x| ≤ ∑ j, G j*|y j-x j| := by
  simpa using patch_bound center width f df G hd hG x y hx hy univ

end DSP.CoordinateBounds
