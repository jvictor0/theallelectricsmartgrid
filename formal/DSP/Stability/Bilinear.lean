import Mathlib.Analysis.Complex.Basic
import Mathlib.Tactic.Linarith
import Mathlib.Tactic.Ring
import Mathlib.Tactic.Positivity

noncomputable section
namespace DSP.Stability

/-- Every root lies strictly in the continuous left half-plane. -/
def Hurwitz (denominator : ℂ → ℂ) : Prop :=
  ∀ p, denominator p = 0 → p.re < 0

/-- Frozen digital poles obtained by the bilinear map lie inside the unit disk.
This is pole stability, not a claim about arbitrary time-varying modulation. -/
def BilinearStable (denominator : ℂ → ℂ) : Prop :=
  ∀ p, denominator p = 0 → ‖(1 + p) / (1 - p)‖ < 1

theorem cayley_stable (p : ℂ) (hp : p.re < 0) : ‖(1 + p) / (1 - p)‖ < 1 := by
  have hd : 1 - p ≠ 0 := by
    intro h
    have hr := congrArg Complex.re h
    simp at hr
    linarith
  have hdpos : 0 < Complex.normSq (1 - p) := Complex.normSq_pos.mpr hd
  have hlt : Complex.normSq (1 + p) < Complex.normSq (1 - p) := by
    simp [Complex.normSq_apply]
    nlinarith
  have hs : Complex.normSq ((1+p)/(1-p)) < 1 := by
    rw [Complex.normSq_div, div_lt_one hdpos]
    exact hlt
  rw [Complex.normSq_eq_norm_sq] at hs
  nlinarith [norm_nonneg ((1+p)/(1-p))]

theorem Hurwitz.bilinear {d : ℂ → ℂ} (h : Hurwitz d) : BilinearStable d :=
  fun p hp => cayley_stable p (h p hp)

theorem Hurwitz.cascade {d1 d2 : ℂ → ℂ} (h1 : Hurwitz d1) (h2 : Hurwitz d2) :
    Hurwitz (fun p => d1 p * d2 p) := by
  intro p hp
  rcases mul_eq_zero.mp hp with h | h
  · exact h1 p h
  · exact h2 p h

theorem onePole (g : ℝ) (hg : 0 < g) : Hurwitz (fun p => p + (g : ℂ)) := by
  intro p hp
  have hr := congrArg Complex.re hp
  simp at hr
  linarith

/-- The quadratic SVF denominator is Hurwitz for every positive g and damping k. -/
theorem svf (g k : ℝ) (hg : 0 < g) (hk : 0 < k) :
    Hurwitz (fun p => p*p + (k*g : ℝ)*p + (g*g : ℝ)) := by
  intro p hp
  have hr := congrArg Complex.re hp
  have hi := congrArg Complex.im hp
  simp at hr hi
  by_contra hn
  have hx : 0 ≤ p.re := le_of_not_gt hn
  have ha : 0 < 2*p.re + k*g := by positivity
  have hz : p.im = 0 := by
    have : p.im * (2*p.re+k*g) = 0 := by nlinarith [hi]
    exact (mul_eq_zero.mp this).resolve_right (ne_of_gt ha)
  have hxk : 0 ≤ k*g*p.re := by positivity
  nlinarith [sq_nonneg p.re, sq_pos_of_pos hg]

/-- For the equal-stage ideal ladder, raw negative feedback 0 ≤ k < 4 is stable.
The proof uses the real and imaginary parts of (p+g)^4 = -k*g^4. -/
theorem ladder (g k : ℝ) (hg : 0 < g) (hk : 0 ≤ k) (hk4 : k < 4) :
    Hurwitz (fun p => (p+(g : ℂ))^4 + (k*g^4 : ℝ)) := by
  intro p hp
  by_contra hn
  have hx : 0 ≤ p.re := le_of_not_gt hn
  let u := p.re + g
  let v := p.im
  have hu : 0 < u := by dsimp [u]; linarith
  have hug : g ≤ u := by dsimp [u]; linarith
  have hr : u^4 - 6*u^2*v^2 + v^4 + k*g^4 = 0 := by
    have h := congrArg Complex.re hp
    simp [pow_succ] at h
    dsimp [u, v]
    nlinarith [h]
  have hi : 4*u*v*(u^2-v^2) = 0 := by
    have h := congrArg Complex.im hp
    simp [pow_succ] at h
    dsimp [u, v]
    nlinarith [h]
  have hg4 : 0 < g^4 := pow_pos hg 4
  have hu4 : 0 < u^4 := pow_pos hu 4
  have hkg : 0 ≤ k*g^4 := mul_nonneg hk (le_of_lt hg4)
  have hv : v = 0 ∨ u^2 = v^2 := by
    have hu4n : 4*u ≠ 0 := by positivity
    rcases mul_eq_zero.mp hi with h | h
    · exact Or.inl ((mul_eq_zero.mp h).resolve_left hu4n)
    · exact Or.inr (sub_eq_zero.mp h)
  rcases hv with hv | hv
  · rw [hv] at hr
    norm_num at hr
    linarith
  · have huv4 : g^4 ≤ u^4 := pow_le_pow_left₀ (le_of_lt hg) hug 4
    have hv4 : v^4 = u^4 := by nlinarith [sq_nonneg (u^2-v^2)]
    have hr' : k*g^4 = 4*u^4 := by nlinarith [hr, sq_nonneg (u^2-v^2)]
    have hlt : k*g^4 < 4*g^4 := mul_lt_mul_of_pos_right hk4 hg4
    linarith

/-- At k=4 the ladder reaches the imaginary axis at its natural frequency. -/
theorem ladder_boundary (g : ℝ) :
    ((⟨0,g⟩ : ℂ)+(g : ℂ))^4 + (4*g^4 : ℝ) = 0 := by
  apply Complex.ext
  · simp [pow_succ]
    ring
  · simp [pow_succ]

end DSP.Stability
