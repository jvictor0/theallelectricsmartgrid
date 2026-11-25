import Mathlib.Data.ENNReal.Inv
import Mathlib.Tactic.FieldSimp
import Mathlib.Tactic.Positivity
import Mathlib.Tactic.Ring

open scoped ENNReal NNReal

namespace DSP.Survey.Certificate

/-- Check a candidate lower level `a / b` with error budget `d / e`,
given `h` hits in `n` trials. All products use exact natural arithmetic. -/
def accepts (a b d e n h : Nat) : Bool :=
  decide (0 < a ∧ a < b ∧ 0 < d ∧ d < e ∧ h ≤ n ∧
    e * 2 ^ n * a ^ h ≤ d * (a + b) ^ h)

theorem accepts_iff (a b d e n h : Nat) :
    accepts a b d e n h = true ↔
      0 < a ∧ a < b ∧ 0 < d ∧ d < e ∧ h ≤ n ∧
      ((d : ℝ≥0∞) / e)⁻¹ ≤
        ((1 + (a : ℝ≥0∞) / b) / (2 * ((a : ℝ≥0∞) / b))) ^ h *
          (1 / 2 : ℝ≥0∞) ^ (n - h) := by
  simp only [accepts, decide_eq_true_eq]
  refine and_congr_right fun ha => and_congr_right fun hab =>
    and_congr_right fun hd => and_congr_right fun hde => and_congr_right fun hn => ?_
  have ha' : (0 : ℝ≥0) < a := by exact_mod_cast ha
  have hb' : (0 : ℝ≥0) < b := by exact_mod_cast ha.trans hab
  have hd' : (0 : ℝ≥0) < d := by exact_mod_cast hd
  have he' : (0 : ℝ≥0) < e := by exact_mod_cast hd.trans hde
  have hq : (0 : ℝ≥0) < (a : ℝ≥0) / b := div_pos ha' hb'
  have hδ : (0 : ℝ≥0) < (d : ℝ≥0) / e := div_pos hd' he'
  have hitDenom : (2 * ((a : ℝ≥0) / b)) ≠ 0 := mul_ne_zero two_ne_zero hq.ne'
  have hit :
      (1 + (a : ℝ≥0) / b) / (2 * ((a : ℝ≥0) / b)) =
        ((a : ℝ≥0) + b) / (2 * a) := by
    apply NNReal.eq
    push_cast
    have haR : (0 : ℝ) < a := by exact_mod_cast ha
    have hbR : (0 : ℝ) < b := by exact_mod_cast ha.trans hab
    field_simp
    ring
  have denominator :
      ((2 : ℝ≥0) * a) ^ h * 2 ^ (n - h) = 2 ^ n * (a : ℝ≥0) ^ h := by
    rw [mul_pow, mul_right_comm, ← pow_add, Nat.add_sub_of_le hn]
  rw [← ENNReal.coe_natCast a, ← ENNReal.coe_natCast b,
    ← ENNReal.coe_natCast d, ← ENNReal.coe_natCast e,
    ← ENNReal.coe_div hb'.ne', ← ENNReal.coe_div he'.ne',
    ← ENNReal.coe_inv hδ.ne', ← ENNReal.coe_one, ← ENNReal.coe_two,
    ← ENNReal.coe_add, ← ENNReal.coe_mul,
    ← ENNReal.coe_div hitDenom, ← ENNReal.coe_div two_ne_zero,
    ← ENNReal.coe_pow, ← ENNReal.coe_pow, ← ENNReal.coe_mul,
    ENNReal.coe_le_coe, hit, inv_div, div_pow, div_pow, div_mul_div_comm,
    one_pow, mul_one, denominator]
  have hDenom : 0 < (2 : ℝ≥0) ^ n * (a : ℝ≥0) ^ h := by positivity
  rw [div_le_div_iff₀ hd' hDenom]
  norm_cast
  rw [Nat.mul_assoc, Nat.mul_comm d]

end DSP.Survey.Certificate
