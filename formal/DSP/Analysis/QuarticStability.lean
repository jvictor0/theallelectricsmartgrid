import Mathlib.Analysis.Complex.Basic
import Mathlib.Tactic

/-! A quartic left-half-plane criterion, proved directly by separating the
real and imaginary parts. Used after a Cayley transform of the actual delayed
Euler ladder characteristic polynomial. No root solver is trusted. -/

noncomputable section
namespace DSP.QuarticStability

theorem re_neg_of_monic (a b c d : ℝ) (ha : 0 < a) (hb : 0 < b)
    (hc : 0 < c) (hd : 0 < d) (hdelta : 0 < a*b*c - c^2 - a^2*d)
    (z : ℂ) (hz : z^4 + (a : ℂ)*z^3 + (b : ℂ)*z^2 + (c : ℂ)*z + d = 0) :
    z.re < 0 := by
  by_contra h
  have hx : 0 ≤ z.re := le_of_not_gt h
  have hr := congrArg Complex.re hz
  have hi := congrArg Complex.im hz
  simp only [Complex.add_re, Complex.add_im, Complex.mul_re, Complex.mul_im,
    Complex.ofReal_re, Complex.ofReal_im, Complex.zero_re, Complex.zero_im,
    pow_succ, pow_zero, Complex.one_re, Complex.one_im] at hr hi
  by_cases hy : z.im = 0
  · simp only [hy, mul_zero, zero_mul, add_zero, sub_zero] at hr
    have : 0 < z.re^4 + a*z.re^3 + b*z.re^2 + c*z.re + d := by positivity
    nlinarith
  have hyrel : (4*z.re+a)*z.im^2 = 4*z.re^3+3*a*z.re^2+2*b*z.re+c := by
    have hm : z.im * ((4*z.re+a)*z.im^2 - (4*z.re^3+3*a*z.re^2+2*b*z.re+c)) = 0 := by
      linear_combination -hi
    exact sub_eq_zero.mp ((mul_eq_zero.mp hm).resolve_left hy)
  have hbd : 0 < b^2 - 4*d := by
    have hs := sq_nonneg (a*b-2*c)
    have he : a^2*(b^2-4*d) = (a*b-2*c)^2 + 4*(a*b*c-c^2-a^2*d) := by ring
    have hp : 0 < a^2*(b^2-4*d) := by rw [he]; positivity
    exact (mul_pos_iff.mp hp).resolve_right (by intro hn; nlinarith [sq_nonneg a]) |>.2
  let F := 64*z.re^6 + 96*a*z.re^5 + (48*a^2+32*b)*z.re^4 +
    (8*a^3+32*a*b)*z.re^3 + (8*a^2*b+4*a*c+4*(b^2-4*d))*z.re^2 +
    (2*a^2*c+2*a*(b^2-4*d))*z.re + (a*b*c-c^2-a^2*d)
  have hpos : 0 < F := by dsimp only [F]; positivity
  have hzero : F = 0 := by
    dsimp only [F]
    linear_combination -(4*z.re+a)^2 * hr +
      (z.im^2*(4*z.re+a)+(4*z.re^3+3*a*z.re^2+2*b*z.re+c)-
        (6*z.re^2+3*a*z.re+b)*(4*z.re+a)) * hyrel
  linarith

theorem re_neg (a4 a3 a2 a1 a0 : ℝ) (h4 : 0 < a4) (h2 : 0 < a2)
    (h1 : 0 < a1) (h0 : 0 < a0)
    (hdelta : 0 < a3*a2*a1 - a4*a1^2 - a3^2*a0)
    (z : ℂ) (hz : (a4 : ℂ)*z^4 + (a3 : ℂ)*z^3 + (a2 : ℂ)*z^2 + (a1 : ℂ)*z + a0 = 0) :
    z.re < 0 := by
  have h3 : 0 < a3 := by
    by_contra h
    have hn := mul_nonpos_of_nonpos_of_nonneg
      (mul_nonpos_of_nonpos_of_nonneg (le_of_not_gt h) h2.le) h1.le
    have hp := mul_nonneg (sq_nonneg a3) h0.le
    have hq := mul_pos h4 (sq_pos_of_pos h1)
    linarith
  have hd : 0 < (a3/a4)*(a2/a4)*(a1/a4) - (a1/a4)^2 - (a3/a4)^2*(a0/a4) := by
    have he : (a3/a4)*(a2/a4)*(a1/a4) - (a1/a4)^2 - (a3/a4)^2*(a0/a4) =
        (a3*a2*a1-a4*a1^2-a3^2*a0)/a4^3 := by field_simp
    rw [he]
    positivity
  apply re_neg_of_monic (a3/a4) (a2/a4) (a1/a4) (a0/a4)
    (div_pos h3 h4) (div_pos h2 h4) (div_pos h1 h4) (div_pos h0 h4) hd z
  have hn : (a4 : ℂ) ≠ 0 := by exact_mod_cast ne_of_gt h4
  push_cast
  field_simp
  linear_combination hz

end DSP.QuarticStability
