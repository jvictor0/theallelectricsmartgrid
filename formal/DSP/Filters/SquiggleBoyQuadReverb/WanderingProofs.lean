import DSP.Filters.SchroederAllPass.WanderingProofs
import DSP.Filters.SquiggleBoyQuadReverb.Loop

namespace DSP.SquiggleBoyQuadReverb

open SmoothRandomLFO WanderingSchroederAllPassGeneric Set

/-- The tight, uniform excursion limit over the eight intro stages and all
lanes of both feedback stages. The shortest intro tap sets the limit. -/
theorem cubicDepth_fits_iff (depth : Nat) :
    ((∀ (branch : Fin 2) (stage : Fin 4),
        DelayBuffer.CubicOffsetFits (squiggleBoyReverbIntroAPFCapacity branch stage)
          (squiggleBoyReverbIntroAPFsDelayTime branch stage) depth) ∧
      (∀ (stage : Fin 2) (channel : Fin 4),
        DelayBuffer.CubicOffsetFits (squiggleBoyReverbLoopAPFCapacity stage)
          (squiggleBoyReverbLoopAPFsDelayTime stage channel) depth)) ↔ depth ≤ 290 := by
  constructor
  · intro h
    have nearest : depth + 3 ≤ 293 := (h.1 0 0).1
    omega
  · intro h
    have introFits : ∀ (branch : Fin 2) (stage : Fin 4),
        DelayBuffer.CubicOffsetFits (squiggleBoyReverbIntroAPFCapacity branch stage)
          (squiggleBoyReverbIntroAPFsDelayTime branch stage) 290 := by decide
    have loopFits : ∀ (stage : Fin 2) (channel : Fin 4),
        DelayBuffer.CubicOffsetFits (squiggleBoyReverbLoopAPFCapacity stage)
          (squiggleBoyReverbLoopAPFsDelayTime stage channel) 290 := by decide
    have smaller {capacity tap : Nat} (fit : DelayBuffer.CubicOffsetFits capacity tap 290) :
        DelayBuffer.CubicOffsetFits capacity tap depth := by
      obtain ⟨lo, hi⟩ := fit
      constructor <;> omega
    exact ⟨fun branch stage => smaller (introFits branch stage),
      fun stage channel => smaller (loopFits stage channel)⟩

theorem openGrid_span_ge (bits : Nat) :
    50000 ≤ idealSpan (squiggleBoyReverbLambdaSamples : ℝ) (((bits : ℝ) + 1 / 2) / 2 ^ 52) := by
  simpa [squiggleBoyReverbLambdaSamples] using reverb_open52_span_ge bits

/-- Worst-case ideal delay-tap pitch bound for the reverb. The minimum span
is guaranteed for exact-arithmetic draws from the generator's open grid. -/
theorem idealTapPitchCents_le_nine {s : Segment ℝ} {point span a b time base : ℝ}
    (bounds : SegmentBounded s 50000) (hpoint : |point| ≤ 1) (hspan : 50000 ≤ span)
    (hab : a < b) (within : Icc a b ⊆ Icc (-s.width) s.next) :
    let f := frameValue (joinValue s point span) a b
    let depth : ℝ := (squiggleBoyReverbWanderSpec true).depth
    let delay := fun t => base + offsetSamples depth (f t)
    |pitchCents (idealTapPitchRatio (delay time) (delay (time + 1)))| ≤ 9 := by
  have bound := idealTapPitchRatio_bound (depth := (squiggleBoyReverbWanderSpec true).depth)
    (time := time) (base := base) (by norm_num) bounds hpoint hspan (by positivity) hab within
  have ε : 2 * ((squiggleBoyReverbWanderSpec true).depth : ℝ) / 50000 = 0.00512 := by
    norm_num [squiggleBoyReverbWanderSpec]
  rw [ε] at bound
  refine (abs_pitchCents_le (by norm_num) bound).trans ?_
  rw [div_mul_eq_mul_div, div_le_iff₀ (Real.log_pos (by norm_num))]
  norm_num
  linarith [Real.log_two_gt_d9]

end DSP.SquiggleBoyQuadReverb
