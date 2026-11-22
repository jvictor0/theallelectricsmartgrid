import DSP.Nonlinear.Analysis
import DSP.Filters.SquiggleBoyQuadReverb.Loop

namespace DSP.SquiggleBoyQuadReverb

/-- The slope of the SAT stage around silence. -/
theorem saturate_hasDerivAt_zero (gain : ℝ) :
    HasDerivAt (saturate gain) gain 0 := by
  change HasDerivAt (fun sample : ℝ => Real.tanh (sample * gain)) gain 0
  have scaled := (hasDerivAt_id (0 : ℝ)).mul_const gain
  have localSlope : HasDerivAt Real.tanh 1 ((id (0 : ℝ)) * gain) := by
    simpa using Tanh.hasDerivAt_zero
  simpa [Function.comp_def] using localSlope.comp 0 scaled

/-- One scalar return from the wet tap to the SAT output, with the intervening
linear path represented by `pathGain` and the diffused input held at zero. -/
theorem saturate_compensatedFeedback_hasDerivAt_zero (gain feedback pathGain : ℝ) (hg : gain ≠ 0) :
    HasDerivAt (fun wet => saturate gain (sumFeedback 0 wet (feedback / gain) * pathGain))
      (feedback * pathGain) 0 := by
  have linear := ((hasDerivAt_id (0 : ℝ)).mul_const (feedback / gain)).mul_const pathGain
  have localSlope : HasDerivAt (saturate gain) gain ((id (0 : ℝ)) * (feedback / gain) * pathGain) := by
    simpa using saturate_hasDerivAt_zero gain
  have slope : gain * (1 * (feedback / gain) * pathGain) = feedback * pathGain := by
    rw [one_mul, ← mul_assoc, mul_comm gain, div_mul_cancel₀ _ hg]
  have composed := localSlope.comp 0 linear
  rw [slope] at composed
  simpa [sumFeedback, Function.comp_def] using composed

end DSP.SquiggleBoyQuadReverb
