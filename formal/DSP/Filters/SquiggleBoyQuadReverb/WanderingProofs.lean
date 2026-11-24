import DSP.Filters.SchroederAllPass.WanderingProofs
import DSP.Filters.SquiggleBoyQuadReverb.Loop

namespace DSP.SquiggleBoyQuadReverb

open SmoothRandomLFO WanderingSchroederAllPassGeneric Set

/-- The tight, uniform excursion limit over the eight intro stages and all
lanes of both feedback stages. The shortest intro tap sets the limit. -/
theorem cubicDepth_fits_iff (depth : Nat) :
    ((∀ stage : Fin 8,
        DelayBuffer.CubicOffsetFits (squiggleBoyReverbIntroAPFCapacity stage)
          (squiggleBoyReverbIntroAPFsDelayTime stage) depth) ∧
      (∀ (stage : Fin 2) (channel : Fin 4),
        DelayBuffer.CubicOffsetFits (squiggleBoyReverbLoopAPFCapacity stage)
          (squiggleBoyReverbLoopAPFsDelayTime stage channel) depth)) ↔ depth ≤ 290 := by
  constructor
  · intro h
    have nearest : depth + 3 ≤ 293 := (h.1 0).1
    omega
  · intro h
    have introFits : ∀ stage : Fin 8,
        DelayBuffer.CubicOffsetFits (squiggleBoyReverbIntroAPFCapacity stage)
          (squiggleBoyReverbIntroAPFsDelayTime stage) 290 := by decide
    have loopFits : ∀ (stage : Fin 2) (channel : Fin 4),
        DelayBuffer.CubicOffsetFits (squiggleBoyReverbLoopAPFCapacity stage)
          (squiggleBoyReverbLoopAPFsDelayTime stage channel) 290 := by decide
    have smaller {capacity tap : Nat} (fit : DelayBuffer.CubicOffsetFits capacity tap 290) :
        DelayBuffer.CubicOffsetFits capacity tap depth := by
      obtain ⟨lo, hi⟩ := fit
      constructor <;> omega
    exact ⟨fun stage => smaller (introFits stage),
      fun stage channel => smaller (loopFits stage channel)⟩

/-- The speed update preserves any nonnegative upper bound shared by the
current speed and the next draw. -/
theorem smoothVelocity_mem_Icc {alpha speed draw limit : ℝ} (ha : alpha ∈ Icc 0 1)
    (hs : speed ∈ Icc 0 limit) (hd : draw ∈ Icc 0 limit) :
    smoothVelocity alpha speed draw ∈ Icc 0 limit := by
  rw [smoothVelocity, fma_eq]
  have complement : 0 ≤ 1 - alpha := sub_nonneg.mpr ha.2
  refine ⟨add_nonneg (mul_nonneg ha.1 hs.1) (mul_nonneg complement hd.1), ?_⟩
  calc
    _ ≤ alpha * limit + (1 - alpha) * limit :=
      add_le_add (mul_le_mul_of_nonneg_left hs.2 ha.1) (mul_le_mul_of_nonneg_left hd.2 complement)
    _ = limit := by ring

/-- Ideal tap pitch during a 32-sample frame with clock speed at most
1/50000 knots per sample. The cubic has unit-width knots in phase space. -/
theorem idealTapPitchCents_le_nine {s : Segment ℝ} {point a b : ℝ}
    (bounds : SegmentBounded s 1) (hpoint : |point| ≤ 1) (hab : a < b)
    (clock : b - a ≤ 32 / 50000) (within : Icc a b ⊆ Icc (-s.width) s.next) :
    let f := joinValue s point 1
    let depth : ℝ := squiggleBoyReverbWanderSpec.depth
    let drift := (offsetSamples depth (f b) - offsetSamples depth (f a)) / 32
    |pitchCents (idealTapPitchRatio 0 drift)| ≤ 9 := by
  dsimp only
  let f := joinValue s point 1
  let depth : ℝ := squiggleBoyReverbWanderSpec.depth
  have depthEq : depth = 128 := by norm_num [depth, squiggleBoyReverbWanderSpec]
  have secant : |frameSlope f a b| ≤ 2 := by
    simpa [f] using abs_join_frameSlope_le (by norm_num : (0 : ℝ) < 1)
      bounds hpoint le_rfl hab within
  rw [frameSlope, abs_div, abs_of_pos (sub_pos.mpr hab)] at secant
  have movement : |f b - f a| ≤ 2 * (32 / 50000) :=
    ((div_le_iff₀ (sub_pos.mpr hab)).mp secant).trans (mul_le_mul_of_nonneg_left clock (by norm_num))
  have changeBound : |offsetSamples depth (f b) - offsetSamples depth (f a)| ≤
      128 * (2 * (32 / 50000)) := by
    rw [depthEq]
    exact (abs_offsetSamples_sub_le (by norm_num) _ _).trans
      (mul_le_mul_of_nonneg_left movement (by norm_num))
  have drift : |idealTapPitchRatio 0 ((offsetSamples depth (f b) - offsetSamples depth (f a)) / 32)
      - 1| ≤ 0.00512 := by
    rw [idealTapPitchRatio, sub_zero, sub_sub_cancel_left, abs_neg, abs_div,
      abs_of_pos (by norm_num : (0 : ℝ) < 32)]
    apply (div_le_iff₀ (by norm_num : (0 : ℝ) < 32)).mpr
    exact changeBound.trans (by norm_num)
  refine (abs_pitchCents_le (by norm_num) drift).trans ?_
  rw [div_mul_eq_mul_div, div_le_iff₀ (Real.log_pos (by norm_num))]
  norm_num
  linarith [Real.log_two_gt_d9]

end DSP.SquiggleBoyQuadReverb
