import DSP.Filters.ConservativeReuppingLadder.Transfer
import DSP.Analysis.QuarticStability

noncomputable section
namespace DSP.ConservativeReuppingLadder

/-- Characteristic polynomial of the four actual histories. Q = k*slope^3.
The z^3 term encodes the previous-fourth-stage feedback and sequential updates. -/
def characteristic (alpha Q : ℝ) (z : ℂ) : ℂ :=
  (z-(1-(alpha : ℂ)))^4+(Q : ℂ)*(alpha : ℂ)^4*z^3

theorem stretched_roots_lt_one (t Q : ℝ) (ht : 1 < t) (hQ : 0 ≤ Q)
    (hnyquist : Q < t^4) (hcrossing : Q*(t-1)^2 < 4*t^2)
    (z : ℂ) (hz : (((t : ℂ)+1)*z-((t : ℂ)-1))^4+16*(Q : ℂ)*z^3 = 0) :
    ‖z‖ < 1 := by
  have ht0 : 0 < t := by linarith
  have hn : z+1 ≠ 0 := by
    intro h
    have he : z = -1 := by linear_combination h
    rw [he] at hz
    have hr := congrArg Complex.re hz
    simp only [Complex.add_re, Complex.add_im, Complex.sub_re, Complex.sub_im,
      Complex.mul_re, Complex.mul_im, Complex.ofReal_re, Complex.ofReal_im,
      Complex.neg_re, Complex.neg_im, Complex.one_re, Complex.one_im,
      Complex.zero_re, pow_succ, pow_zero] at hr
    norm_num at hr
    nlinarith
  let w := (z-1)/(z+1)
  have hw : ((t^4-Q : ℝ) : ℂ)*w^4 + ((4*t^3-2*Q : ℝ) : ℂ)*w^3 +
      ((6*t^2 : ℝ) : ℂ)*w^2 + ((4*t+2*Q : ℝ) : ℂ)*w + ((1+Q : ℝ) : ℂ) = 0 := by
    dsimp only [w]
    push_cast
    field_simp
    linear_combination hz
  have hd : 0 < (4*t^3-2*Q)*(6*t^2)*(4*t+2*Q) -
      (t^4-Q)*(4*t+2*Q)^2 - (4*t^3-2*Q)^2*(1+Q) := by
    have he : (4*t^3-2*Q)*(6*t^2)*(4*t+2*Q) -
        (t^4-Q)*(4*t+2*Q)^2 - (4*t^3-2*Q)^2*(1+Q) =
        4*(4*t^2-(t-1)^2*Q)*(4*t^4+(t-1)^2*Q) := by ring
    rw [he]
    have hp : 0 < 4*t^2-(t-1)^2*Q := by nlinarith
    positivity
  have hwneg := QuarticStability.re_neg (t^4-Q) (4*t^3-2*Q) (6*t^2) (4*t+2*Q) (1+Q)
    (sub_pos.mpr hnyquist) (by positivity) (by positivity) (by positivity) hd w hw
  have hre : w.re = (‖z‖^2-1)/Complex.normSq (z+1) := by
    simp only [w, Complex.div_re, Complex.sub_re, Complex.sub_im, Complex.add_re,
      Complex.add_im, Complex.one_re, Complex.one_im, sub_zero, add_zero, Complex.sq_norm,
      Complex.normSq_apply]
    ring
  by_contra h
  have hnorm : 1 ≤ ‖z‖ := le_of_not_gt h
  have hnumer : 0 ≤ ‖z‖^2-1 := by nlinarith
  rw [hre] at hwneg
  have := div_nonneg hnumer (Complex.normSq_nonneg (z+1))
  linarith

theorem roots_lt_one (alpha Q : ℝ) (ha : 0 < alpha) (hu : alpha < 1) (hQ : 0 ≤ Q)
    (hnyquist : Q < ((2-alpha)/alpha)^4)
    (hcrossing : Q*(1-alpha)^2 < (2-alpha)^2)
    (z : ℂ) (hz : characteristic alpha Q z = 0) : ‖z‖ < 1 := by
  let t := (2-alpha)/alpha
  have ht : 1 < t := by dsimp only [t]; rw [lt_div_iff₀ ha]; linarith
  have htalpha : t*alpha = 2-alpha := by dsimp only [t]; field_simp
  have hc : Q*(t-1)^2 < 4*t^2 := by
    have he : (4*t^2-Q*(t-1)^2)*alpha^2 =
        4*((2-alpha)^2-Q*(1-alpha)^2) := by
      dsimp only [t]
      field_simp
      ring
    have hp : 0 < (4*t^2-Q*(t-1)^2)*alpha^2 := by rw [he]; linarith
    have := (mul_pos_iff.mp hp).resolve_right (by intro hn; nlinarith [sq_nonneg alpha])
    linarith [this.1]
  apply stretched_roots_lt_one t Q ht hQ hnyquist hc z
  have hn : (alpha : ℂ) ≠ 0 := by exact_mod_cast ne_of_gt ha
  dsimp only [t]
  push_cast
  field_simp
  unfold characteristic at hz
  linear_combination 16*hz

theorem denominator_ne_zero_of_roots (alpha Q : ℝ)
    (hroots : ∀ z, characteristic alpha Q z = 0 → ‖z‖ < 1)
    (q : ℂ) (hq : ‖q‖ = 1) :
    (1-(1-(alpha : ℂ))*q)^4+(Q : ℂ)*(alpha : ℂ)^4*q ≠ 0 := by
  have hn : q ≠ 0 := norm_ne_zero_iff.mp (by rw [hq]; norm_num)
  have he : q^4*characteristic alpha Q q⁻¹ =
      (1-(1-(alpha : ℂ))*q)^4+(Q : ℂ)*(alpha : ℂ)^4*q := by
    unfold characteristic
    field_simp
  intro hz
  rw [hz] at he
  have hc := (mul_eq_zero.mp he).resolve_left (pow_ne_zero 4 hn)
  have hlt := hroots q⁻¹ hc
  rw [norm_inv, hq, inv_one] at hlt
  exact (lt_irrefl 1) hlt

end DSP.ConservativeReuppingLadder
