import Encoder.Evaluation
import Mathlib.Analysis.SpecialFunctions.Pow.Real
import Mathlib.Tactic.Linarith
import Mathlib.Tactic.NormNum
import Mathlib.Tactic.Ring

namespace Encoder.Curve

/-! Two deliberately distinct models: the ideal exponential law over reals, and
an exact-arithmetic reading of the actual binary32 polynomial constants. Neither
asserts that floating-point Horner evaluation is exact or globally monotone.
-/

-- @cpp curve: BankedEncoderCell::ModulationDepthFromValue.
-- @head curve: the same signed, clamped transform at the pinned revision.
noncomputable def idealMagnitude (u : ℝ) : ℝ := ((9 : ℝ)^u - 1)/8
noncomputable def ideal (v : ℝ) : ℝ :=
  let m := idealMagnitude (min 1 |v|)
  if v < 0 then -m else m

theorem idealMagnitude_zero : idealMagnitude 0 = 0 := by norm_num [idealMagnitude]
theorem idealMagnitude_one : idealMagnitude 1 = 1 := by norm_num [idealMagnitude]

theorem ideal_half_depth : ideal (1/2) = 1/4 := by
  have hp := Real.rpow_mul_natCast (show (0 : ℝ) ≤ 9 by norm_num) (1/2) 2
  have hn := Real.rpow_nonneg (show (0 : ℝ) ≤ 9 by norm_num) (1/2)
  norm_num at hp
  have hx : (9 : ℝ)^(1/2 : ℝ) = 3 := by nlinarith
  norm_num [ideal, idealMagnitude, hx]

theorem idealMagnitude_monotone : Monotone idealMagnitude := by
  intro a b h
  have hp := Real.rpow_le_rpow_of_exponent_le (show (1 : ℝ) ≤ 9 by norm_num) h
  dsimp [idealMagnitude]
  linarith

theorem idealMagnitude_bounds (u : ℝ) (h : 0 ≤ u ∧ u ≤ 1) :
    0 ≤ idealMagnitude u ∧ idealMagnitude u ≤ 1 := by
  have lo := idealMagnitude_monotone h.1
  have hi := idealMagnitude_monotone h.2
  rw [idealMagnitude_zero] at lo
  rw [idealMagnitude_one] at hi
  exact ⟨lo, hi⟩

theorem ideal_zero : ideal 0 = 0 := by simp [ideal, idealMagnitude_zero]
theorem ideal_endpoints : ideal 1 = 1 ∧ ideal (-1) = -1 := by
  norm_num [ideal, idealMagnitude]

theorem ideal_odd (v : ℝ) : ideal (-v) = -ideal v := by
  unfold ideal
  rw [abs_neg]
  by_cases h : v = 0
  · subst v; simp [idealMagnitude_zero]
  · split_ifs <;> first | (exfalso; linarith) | (exfalso; apply h; linarith) | ring

theorem ideal_bounded (v : ℝ) : -1 ≤ ideal v ∧ ideal v ≤ 1 := by
  have hm := idealMagnitude_bounds (min 1 |v|) ⟨le_min (by norm_num) (abs_nonneg v), min_le_left _ _⟩
  unfold ideal
  split_ifs <;> constructor <;> linarith

theorem ideal_saturates (v : ℝ) (h : 1 ≤ v) : ideal v = 1 ∧ ideal (-v) = -1 := by
  have hv : 0 ≤ v := by linarith
  have hp : ideal v = 1 := by
    simp [ideal, abs_of_nonneg hv, min_eq_left h, show ¬v < 0 by linarith, idealMagnitude_one]
  exact ⟨hp, by rw [ideal_odd, hp]⟩

-- @cpp exp_polynomial: MultiChannelNumber<float,4>::Exp2, nearest integer range
-- reduction and sixth-degree Horner polynomial. Constants are exact binary32
-- rationals (not their decimal source spellings).
-- @head exp_polynomial: unchanged in HEAD. Exponent-bit scaling is modeled as
-- multiplication by 2^n, under the normal-number condition stated by the C++.
def polynomial (f : Rat) : Rat :=
  let p := f * (10585225/68719476736) + 11453439/8589934592
  let p := p*f + 10327387/1073741824
  let p := p*f + 14899271/268435456
  let p := p*f + 1007583/4194304
  let p := p*f + 1453635/2097152
  p*f + 1

def log2Nine : Rat := 13295629/4194304

def exp2Approx (x : Rat) : Rat :=
  let n : Int := ⌊x+1/2⌋
  polynomial (x-n) * (2 : Rat)^n

def magnitude (v : Rat) : Rat := (exp2Approx (min 1 (absRat v) * log2Nine)-1)/8

def approximate (v : Rat) : Rat := if v < 0 then -absRat (magnitude v) else absRat (magnitude v)

theorem polynomial_at_zero : polynomial 0 = 1 := by norm_num [polynomial]
theorem approximate_zero : approximate 0 = 0 := by
  norm_num [approximate, magnitude, exp2Approx, absRat, log2Nine, polynomial]

theorem approximate_odd (v : Rat) : approximate (-v) = -approximate v := by
  have ha : absRat (-v) = absRat v := by unfold absRat; split_ifs <;> linarith
  have hm : magnitude (-v) = magnitude v := by unfold magnitude; rw [ha]
  by_cases hz : v = 0
  · subst v; simp [approximate_zero]
  · unfold approximate
    rw [hm]
    split_ifs <;> first | (exfalso; linarith) | (exfalso; apply hz; linarith) | ring

theorem approximate_saturates (v : Rat) (h : 1 ≤ v) :
    approximate v = approximate 1 ∧ approximate (-v) = approximate (-1) := by
  have hp : approximate v = approximate 1 := by
    have hv : ¬v < 0 := by linarith
    norm_num [approximate, magnitude, absRat, hv, min_eq_left h]
  exact ⟨hp, by rw [approximate_odd, approximate_odd, hp]⟩

end Encoder.Curve
