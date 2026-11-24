import Mathlib.Algebra.Order.Archimedean.Real.Basic
import Mathlib.Algebra.Order.GroupWithZero.Basic
import Mathlib.Tactic.NormNum
import Mathlib.Tactic.Positivity
import Mathlib.Tactic.Linarith
import Mathlib.Tactic.Ring

namespace PhaseTime.InverseResolution

/-- The finite binary64 values, interpreted as exact reals. The trailing
    zeroes of the significand need not be normalized. -/
def Binary64 (x : ℝ) : Prop :=
  ∃ m e : ℤ, |m| < 2 ^ 53 ∧ -1074 ≤ e ∧ e ≤ 971 ∧ x = (m : ℝ) * 2 ^ e

theorem binary64_signedPower (e : ℤ) (hl : -1074 ≤ e) (hr : e ≤ 971) :
    Binary64 ((2 : ℝ) ^ e) ∧ Binary64 (-((2 : ℝ) ^ e)) := by
  constructor
  · refine ⟨1, e, by norm_num, hl, hr, by simp⟩
  · refine ⟨-1, e, by norm_num, hl, hr, by simp⟩

private theorem scaleRepresentable {x : ℝ} (hx : Binary64 x) (k : ℕ)
    (range : |x * (2 : ℝ) ^ k| < (2 : ℝ) ^ (53 : ℤ)) : Binary64 (x * (2 : ℝ) ^ k) := by
  obtain ⟨m, e, hm, he, _, rfl⟩ := hx
  by_cases zero : m = 0
  · subst m
    exact ⟨0, 0, by norm_num, by norm_num, by norm_num, by simp⟩
  have product : (m : ℝ) * 2 ^ e * (2 : ℝ) ^ k = (m : ℝ) * 2 ^ (e + (k : ℤ)) := by
    rw [zpow_add₀ (by norm_num : (2 : ℝ) ≠ 0), zpow_natCast, mul_assoc]
  have mAbs : 1 ≤ |(m : ℝ)| := by exact_mod_cast (Int.add_one_le_iff.mpr (abs_pos.mpr zero))
  have powerPos : 0 < (2 : ℝ) ^ (e + (k : ℤ)) := zpow_pos (by norm_num) _
  have exponent : e + (k : ℤ) < 53 := by
    rw [product, abs_mul, abs_of_pos powerPos] at range
    have below : (2 : ℝ) ^ (e + (k : ℤ)) < (2 : ℝ) ^ (53 : ℤ) :=
      lt_of_le_of_lt (le_mul_of_one_le_left powerPos.le mAbs) range
    exact (zpow_lt_zpow_iff_right₀ (by norm_num : 1 < (2 : ℝ))).mp below
  exact ⟨m, e + (k : ℤ), hm, by omega, by omega, product⟩

/-- The native phase limit is inside the wider power-of-two boundaries used to
    infer that the exact product is representable. Rounding only needs to
    preserve order and fix representable results. -/
theorem gridScaleExact (round : ℝ → ℝ) (ordered : Monotone round)
    (exact : ∀ x, Binary64 x → round x = x) {x : ℝ} (hx : Binary64 x) (k : ℕ)
    (bound : |round (x * (2 : ℝ) ^ k)| ≤ (2 : ℝ) ^ (52 : ℤ)) :
    round (x * (2 : ℝ) ^ k) = x * (2 : ℝ) ^ k := by
  have ⟨positive, negative⟩ := binary64_signedPower 53 (by decide) (by decide)
  have headroom : (2 : ℝ) ^ (52 : ℤ) < (2 : ℝ) ^ (53 : ℤ) := by norm_num
  apply exact _ (scaleRepresentable hx k _)
  rw [abs_lt]
  constructor
  · by_contra! low
    have le := ordered low
    rw [exact _ negative] at le
    linarith [(abs_le.mp bound).1]
  · by_contra! high
    have le := ordered high
    rw [exact _ positive] at le
    linarith [(abs_le.mp bound).2]

private theorem multipleOfMin {x : ℝ} (hx : Binary64 x) :
    ∃ units : ℤ, x = (units : ℝ) * (2 : ℝ) ^ (-1074 : ℤ) := by
  obtain ⟨m, e, _, low, _, rfl⟩ := hx
  let shift := (e + 1074).toNat
  have he : e = (shift : ℤ) + (-1074) := by dsimp [shift]; omega
  refine ⟨m * 2 ^ shift, ?_⟩
  conv => lhs; rw [he, zpow_add₀ (by norm_num : (2 : ℝ) ≠ 0), zpow_natCast]
  push_cast
  ring

/-- Model the native multiplication and subtraction through their rounding
    map. With a nonnegative binary shift and two bounded scaled phases, the
    writer's forward branch gets a positive interpolation denominator. -/
theorem roundedWidthPositive (round : ℝ → ℝ) (ordered : Monotone round)
    (exact : ∀ x, Binary64 x → round x = x) {left right : ℝ}
    (hl : Binary64 left) (hr : Binary64 right) (k : ℕ) (forward : left < right)
    (leftBound : |round (left * (2 : ℝ) ^ k)| ≤ (2 : ℝ) ^ (52 : ℤ))
    (rightBound : |round (right * (2 : ℝ) ^ k)| ≤ (2 : ℝ) ^ (52 : ℤ)) :
    0 < round (round (right * (2 : ℝ) ^ k) - round (left * (2 : ℝ) ^ k)) := by
  have leftExact := gridScaleExact round ordered exact hl k leftBound
  have rightExact := gridScaleExact round ordered exact hr k rightBound
  have within (x : ℝ) (h : |x| ≤ (2 : ℝ) ^ (52 : ℤ)) : |x| < (2 : ℝ) ^ (53 : ℤ) :=
    h.trans_lt (by norm_num)
  obtain ⟨i, hi⟩ := multipleOfMin (scaleRepresentable hl k (within _ (leftExact ▸ leftBound)))
  obtain ⟨j, hj⟩ := multipleOfMin (scaleRepresentable hr k (within _ (rightExact ▸ rightBound)))
  have strict := mul_lt_mul_of_pos_right forward (by positivity : 0 < (2 : ℝ) ^ k)
  rw [hi, hj] at strict
  have minPositive : 0 < (2 : ℝ) ^ (-1074 : ℤ) := zpow_pos (by norm_num) _
  have indexOrder : i < j := by exact_mod_cast lt_of_mul_lt_mul_right strict minPositive.le
  have atLeastOne : (1 : ℝ) ≤ (j : ℝ) - (i : ℝ) := by exact_mod_cast (show 1 ≤ j - i by omega)
  have step : (2 : ℝ) ^ (-1074 : ℤ) ≤
      (j : ℝ) * 2 ^ (-1074 : ℤ) - (i : ℝ) * 2 ^ (-1074 : ℤ) := by
    rw [← sub_mul]
    simpa only [one_mul] using mul_le_mul_of_nonneg_right atLeastOne minPositive.le
  have width := ordered step
  rw [exact _ (binary64_signedPower (-1074) (by decide) (by decide)).1] at width
  rw [leftExact, rightExact, hi, hj]
  exact minPositive.trans_le width

end PhaseTime.InverseResolution
