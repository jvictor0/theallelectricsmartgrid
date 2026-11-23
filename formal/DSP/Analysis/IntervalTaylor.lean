import DSP.Analysis.CenteredTaylor
import DSP.Analysis.IntervalArithmetic

/-! A rational checker for a centered Taylor enclosure. It consumes boxes for
the value and gradient at the center, plus a Hessian box valid along every
segment in the cell. The theorem proves the entire-cell implication; callers
must still prove the derivative identities and uniform Hessian membership. -/

namespace DSP.IntervalTaylor
open LeanCert.Core DSP.IntervalArithmetic Finset Set

def radiusCheck (center outer : IntervalRat) (deviation : ℚ) : Bool :=
  decide (outer.lo ≤ center.lo-deviation ∧ center.hi+deviation ≤ outer.hi)

theorem radiusCheck_sound (center outer : IntervalRat) (deviation : ℚ)
    (hc : radiusCheck center outer deviation = true) (x y : ℝ)
    (hx : x ∈ center) (hxy : |y-x| ≤ (deviation:ℝ)) : y ∈ outer := by
  simp only [radiusCheck, decide_eq_true_eq] at hc
  have hlo : (outer.lo:ℝ) ≤ (center.lo:ℝ)-(deviation:ℝ) := by exact_mod_cast hc.1
  have hhi : (center.hi:ℝ)+(deviation:ℝ) ≤ (outer.hi:ℝ) := by exact_mod_cast hc.2
  have hd := abs_le.mp hxy
  exact ⟨by linarith [hx.1], by linarith [hx.2]⟩

def firstOrderRadius {n : ℕ} (gradient : Fin n → IntervalRat) (width : Fin n → ℚ) : ℚ :=
  ∑ i, absoluteBound (gradient i)*width i

/-- This version bounds the gradient uniformly throughout the cell. It also
encloses the Jacobian from center Jacobian entries and Hessian rows. -/
theorem firstOrder_sound {n : ℕ} (center : IntervalRat) (gradient : Fin n → IntervalRat)
    (width : Fin n → ℚ) (outer : IntervalRat)
    (hc : radiusCheck center outer (firstOrderRadius gradient width) = true)
    (f : ℝ → ℝ) (g : ℝ → Fin n → ℝ) (offset : Fin n → ℝ)
    (hcenter : f 0 ∈ center) (hg : ∀ t ∈ Icc (0:ℝ) 1, ∀ i, g t i ∈ gradient i)
    (hw : ∀ i, |offset i| ≤ (width i:ℝ))
    (hfirst : ∀ t ∈ Icc (0:ℝ) 1, HasDerivAt f (∑ i, g t i*offset i) t) : f 1 ∈ outer := by
  apply radiusCheck_sound center outer (firstOrderRadius gradient width) hc (f 0) (f 1) hcenter
  apply norm_image_sub_le_of_norm_deriv_le_segment_01'
    (fun t ht => (hfirst t ht).hasDerivWithinAt)
  intro t ht
  have h := CenteredTaylor.linear_bound (g t) (fun i => (absoluteBound (gradient i):ℝ))
    offset (fun i => (width i:ℝ))
    (fun i => norm_le_absoluteBound (hg t (Ico_subset_Icc_self ht) i)) hw
  simpa [firstOrderRadius] using h

def radius {n : ℕ} (gradient : Fin n → IntervalRat)
    (hessian : Fin n → Fin n → IntervalRat) (width : Fin n → ℚ) : ℚ :=
  (∑ i, absoluteBound (gradient i)*width i)+
    (∑ i, ∑ j, absoluteBound (hessian i j)*width i*width j)/2

def check {n : ℕ} (center : IntervalRat) (gradient : Fin n → IntervalRat)
    (hessian : Fin n → Fin n → IntervalRat) (width : Fin n → ℚ) (outer : IntervalRat) : Bool :=
  radiusCheck center outer (radius gradient hessian width)

theorem check_sound {n : ℕ} (center : IntervalRat) (gradient : Fin n → IntervalRat)
    (hessian : Fin n → Fin n → IntervalRat) (width : Fin n → ℚ) (outer : IntervalRat)
    (hc : check center gradient hessian width outer = true)
    (f f' : ℝ → ℝ) (g : Fin n → ℝ) (h : ℝ → Fin n → Fin n → ℝ) (offset : Fin n → ℝ)
    (hcenter : f 0 ∈ center) (hg : ∀ i, g i ∈ gradient i)
    (hh : ∀ t ∈ Icc (0:ℝ) 1, ∀ i j, h t i j ∈ hessian i j)
    (hw : ∀ i, |offset i| ≤ (width i:ℝ))
    (hfirst : ∀ t ∈ Icc (0:ℝ) 1, HasDerivAt f (f' t) t)
    (hsecond : ∀ t ∈ Icc (0:ℝ) 1,
      HasDerivAt f' (∑ i, ∑ j, h t i j*offset i*offset j) t)
    (hinitial : f' 0 = ∑ i, g i*offset i) : f 1 ∈ outer := by
  have hrem := CenteredTaylor.cell_remainder f f' h
    (fun i j => (absoluteBound (hessian i j):ℝ)) offset (fun i => (width i:ℝ))
    hfirst hsecond (fun t ht i j => norm_le_absoluteBound (hh t ht i j)) hw
  have hlin : |f' 0| ≤ ∑ i, (absoluteBound (gradient i):ℝ)*(width i:ℝ) := by
    rw [hinitial]
    exact CenteredTaylor.linear_bound g (fun i => (absoluteBound (gradient i):ℝ))
      offset (fun i => (width i:ℝ)) (fun i => norm_le_absoluteBound (hg i)) hw
  have hrad : (radius gradient hessian width:ℝ) =
      (∑ i, (absoluteBound (gradient i):ℝ)*(width i:ℝ))+
      CenteredTaylor.quadraticBound (fun i j => (absoluteBound (hessian i j):ℝ))
        (fun i => (width i:ℝ))/2 := by
    simp [radius, CenteredTaylor.quadraticBound]
  have hd : |f 1-f 0| ≤ (radius gradient hessian width:ℝ) := by
    have ht := abs_add_le (f 1-f 0-f' 0) (f' 0)
    rw [sub_add_cancel] at ht
    rw [hrad]
    linarith
  exact radiusCheck_sound center outer (radius gradient hessian width) hc (f 0) (f 1) hcenter hd

end DSP.IntervalTaylor
