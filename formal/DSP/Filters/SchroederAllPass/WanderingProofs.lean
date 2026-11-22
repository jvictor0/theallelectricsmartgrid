import DSP.Analysis.ModulatedMode
import DSP.Filters.SchroederAllPass.Wandering
import DSP.IfLess.Real
import DSP.SmoothRandomLFO.FramedProofs
import Mathlib.Algebra.Order.Floor.Semiring

namespace DSP.WanderingSchroederAllPassGeneric

open SmoothRandomLFO Set

noncomputable section

theorem offsetSamples_real (depth spline : ℝ) :
    offsetSamples depth spline = depth * (1 + max (-1) (min 1 spline)) := by
  have hmin : minValue (1 : ℝ) spline = min 1 spline := by
    change (if spline < 1 then spline else 1) = min 1 spline
    split
    · next h => exact (min_eq_right h.le).symm
    · next h => exact (min_eq_left (le_of_not_gt h)).symm
  have hmax : maxValue (-1 : ℝ) (min 1 spline) = max (-1) (min 1 spline) := by
    change (if -1 < min 1 spline then min 1 spline else -1) = _
    split
    · next h => exact (max_eq_right h.le).symm
    · next h => exact (max_eq_left (le_of_not_gt h)).symm
  rw [offsetSamples, hmin, hmax]

/-- Bounds follow from the runtime clamp, independently of the spline and rate. -/
theorem offsetSamples_bounds {depth : ℝ} (hd : 0 ≤ depth) (spline : ℝ) :
    0 ≤ offsetSamples depth spline ∧ offsetSamples depth spline ≤ 2 * depth := by
  have lo := le_max_left (-1 : ℝ) (min 1 spline)
  have hi : max (-1 : ℝ) (min 1 spline) ≤ 1 := max_le (by norm_num) (min_le_left _ _)
  rw [offsetSamples_real]
  constructor
  · exact mul_nonneg hd (by linarith)
  · simpa [mul_comm depth] using mul_le_mul_of_nonneg_left
      (show 1 + max (-1 : ℝ) (min 1 spline) ≤ 2 by linarith) hd

/-- The oldest and newest logical ages are inside the history even before
the ring's index reduction. This covers all four reads of the cubic stencil. -/
theorem cubicOffset_stencil {capacity tap depth whole : Nat}
    (fits : DelayBuffer.CubicOffsetFits capacity tap depth) (hw : whole ≤ 2 * depth) :
    let center := tap - depth + whole - 1
    2 ≤ center ∧ center + 1 < capacity := by
  obtain ⟨low, high⟩ := fits
  dsimp only
  omega

theorem cubicOffset_stencil_of_spline {capacity tap depth : Nat}
    (fits : DelayBuffer.CubicOffsetFits capacity tap depth) (spline : ℝ) :
    let center := tap - depth + ⌈offsetSamples (depth : ℝ) spline⌉₊ - 1
    2 ≤ center ∧ center + 1 < capacity := by
  apply cubicOffset_stencil fits
  exact Nat.ceil_le.mpr (by push_cast; exact (offsetSamples_bounds (by positivity) spline).2)

theorem abs_offsetSamples_sub_le {depth : ℝ} (hd : 0 ≤ depth) (x y : ℝ) :
    |offsetSamples depth x - offsetSamples depth y| ≤ depth * |x - y| := by
  have clamp : LipschitzWith 1 (fun u : ℝ => max (-1) (min 1 u)) :=
    (LipschitzWith.id.const_min _).const_max _
  have diff := lipschitzWith_iff_dist_le_mul.mp clamp x y
  simp only [Real.dist_eq, NNReal.coe_one, one_mul] at diff
  simp only [offsetSamples_real]
  rw [← mul_sub, add_sub_add_left_eq_sub, abs_mul, abs_of_nonneg hd]
  exact mul_le_mul_of_nonneg_left diff hd

/-- Phase in turns of an ideal fractional read of a pure tone, in sample units. -/
def idealTapPhase (frequency time delay : ℝ) : ℝ := frequency * (time - delay)

/-- The phase increment in one sample, normalized by the input tone frequency. -/
def idealTapPitchRatio (delayNow delayNext : ℝ) : ℝ := 1 - (delayNext - delayNow)

theorem idealTapPhase_increment (frequency time delayNow delayNext : ℝ) :
    idealTapPhase frequency (time + 1) delayNext - idealTapPhase frequency time delayNow =
      frequency * idealTapPitchRatio delayNow delayNext := by
  unfold idealTapPhase idealTapPitchRatio
  ring

/-- Two adjacent samples of a frame. The constant part of the tap cancels. -/
theorem idealTapPitchRatio_bound {s : Segment ℝ} {point span L a b time depth base : ℝ}
    (hL : 0 < L) (bounds : SegmentBounded s L) (hpoint : |point| ≤ 1) (hspan : L ≤ span)
    (hd : 0 ≤ depth) (hab : a < b) (within : Icc a b ⊆ Icc (-s.width) s.next) :
    let f := frameValue (joinValue s point span) a b
    let delay := fun t => base + offsetSamples depth (f t)
    |idealTapPitchRatio (delay time) (delay (time + 1)) - 1| ≤ 2 * depth / L := by
  dsimp only
  have difference : frameValue (joinValue s point span) a b (time + 1) -
      frameValue (joinValue s point span) a b time = frameSlope (joinValue s point span) a b := by
    unfold frameValue
    ring
  rw [idealTapPitchRatio, sub_sub_cancel_left, abs_neg, add_sub_add_left_eq_sub]
  calc
    _ ≤ depth * |frameValue (joinValue s point span) a b (time + 1) -
        frameValue (joinValue s point span) a b time| := abs_offsetSamples_sub_le hd _ _
    _ ≤ depth * (2 / L) := by
      rw [difference]
      exact mul_le_mul_of_nonneg_left (abs_join_frameSlope_le hL bounds hpoint hspan hab within) hd
    _ = _ := by ring

def pitchCents (ratio : ℝ) : ℝ := 1200 * Real.log ratio / Real.log 2

/-- A conservative cents bound for a relative frequency error of at most ε. -/
theorem abs_pitchCents_le {ratio ε : ℝ} (he : ε < 1) (bound : |ratio - 1| ≤ ε) :
    |pitchCents ratio| ≤ 1200 / Real.log 2 * (ε / (1 - ε)) := by
  have hlog2 := Real.log_pos (by norm_num : (1 : ℝ) < 2)
  obtain ⟨lo, hi⟩ := abs_le.mp bound
  have hp : 0 < ratio := by linarith
  have he0 := (abs_nonneg _).trans bound
  have hden := sub_pos.mpr he
  have scaled : ε ≤ ε / (1 - ε) := by
    apply (le_div_iff₀ hden).mpr
    nlinarith [sq_nonneg ε]
  have hlog : |Real.log ratio| ≤ ε / (1 - ε) := by
    apply abs_le.mpr
    constructor
    · have hlo := Real.one_sub_inv_le_log_of_pos hp
      have inv : ratio⁻¹ - 1 ≤ ε / (1 - ε) := by
        rw [inv_eq_one_div, sub_le_iff_le_add]
        calc
          _ ≤ 1 / (1 - ε) := div_le_div_of_nonneg_left (by norm_num) hden (by linarith)
          _ = ε / (1 - ε) + 1 := by field_simp; ring
      linarith
    · exact (Real.log_le_sub_one_of_pos hp).trans (hi.trans scaled)
  rw [pitchCents, abs_div, abs_mul, abs_of_nonneg (by norm_num : (0 : ℝ) ≤ 1200),
    abs_of_pos hlog2, mul_div_right_comm]
  exact mul_le_mul_of_nonneg_left hlog (by positivity)

/-- Select a normalized speed cutoff B. Spans of at least λ/B give a relative
pitch bound ε = 2 depth B / λ. `shortSpan_probability_le` bounds exceptions
among a finite number of ideal draws by n exp(-B). -/
theorem idealTapPitchCents_le {s : Segment ℝ} {point span lambdaSamples B a b time depth base : ℝ}
    (hRate : 0 < lambdaSamples) (hB : 0 < B) (bounds : SegmentBounded s (lambdaSamples / B))
    (hpoint : |point| ≤ 1) (hspan : lambdaSamples / B ≤ span)
    (hd : 0 ≤ depth) (hab : a < b) (within : Icc a b ⊆ Icc (-s.width) s.next)
    (slow : 2 * depth * B / lambdaSamples < 1) :
    let f := frameValue (joinValue s point span) a b
    let delay := fun t => base + offsetSamples depth (f t)
    let ε := 2 * depth * B / lambdaSamples
    |pitchCents (idealTapPitchRatio (delay time) (delay (time + 1)))| ≤
      1200 / Real.log 2 * (ε / (1 - ε)) := by
  apply abs_pitchCents_le slow
  have bound := idealTapPitchRatio_bound (time := time) (base := base)
    (div_pos hRate hB) bounds hpoint hspan hd hab within
  simpa [div_div_eq_mul_div] using bound

/-- One sample of motion can detune an ideal frozen APF by at most this
chord length. The bound combines the rate, depth, frequency and APF gain. -/
theorem frozenAllPass_motion_le {s : Segment ℝ}
    {point span lambdaSamples B a b time depth base gain frequency : ℝ}
    (hRate : 0 < lambdaSamples) (hB : 0 < B) (bounds : SegmentBounded s (lambdaSamples / B))
    (hpoint : |point| ≤ 1) (hspan : lambdaSamples / B ≤ span)
    (hd : 0 ≤ depth) (hab : a < b) (within : Icc a b ⊆ Icc (-s.width) s.next)
    (hg : |gain| < 1) :
    let f := frameValue (joinValue s point span) a b
    let delay := fun t => base + offsetSamples depth (f t)
    ‖ModulatedMode.frozenAllPass gain frequency (delay (time + 1)) -
      ModulatedMode.frozenAllPass gain frequency (delay time)‖ ≤
      (1 + |gain|) / (1 - |gain|) * (2 * Real.pi * |frequency| * (2 * depth * B / lambdaSamples)) := by
  dsimp only
  refine (ModulatedMode.norm_frozenAllPass_sub_le hg).trans ?_
  have drift := idealTapPitchRatio_bound (time := time) (base := base)
    (div_pos hRate hB) bounds hpoint hspan hd hab within
  simp only [idealTapPitchRatio, sub_sub_cancel_left, abs_neg, div_div_eq_mul_div] at drift
  exact mul_le_mul_of_nonneg_left (mul_le_mul_of_nonneg_left drift (by positivity))
    (by positivity)

end
end DSP.WanderingSchroederAllPassGeneric
