import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftOutputBoundsData

/-! Exact replay of the final rounded output jets and centered inequalities.
The derivative-to-cell implication is supplied separately, so these checks
alone are not an invariant-curve certificate. -/

namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftOutputBounds
open LeanCert.Core DSP.IntervalArithmetic PolarTube DriftOutputBoundsData
set_option maxRecDepth 100000
set_option maxHeartbeats 0

theorem gradient_checked : ∀ i j,
    contains (gradient i j) (DriftSecondOutput.outputCenterBox j i) = true := by decide +kernel

theorem hessian_checked : ∀ i j k,
    contains (hessian i j k) (DriftSecondOutput.outputMixedBox j k i) = true := by decide +kernel

theorem wholeGradient_checked : ∀ i j, IntervalTaylor.radiusCheck (gradient i j)
    (wholeGradient i j) (IntervalTaylor.firstOrderRadius (hessian i j) width) = true := by decide +kernel

theorem image_checked : ∀ i : Fin 3, IntervalTaylor.firstOrderRadius
    (wholeGradient i.succ) width + (1/100000000:ℚ) ≤ 7/5000000 := by decide +kernel

theorem center_mem (s : NaiveReuppingLadder ℝ) (h : LockedParameters.Parameters s)
    (i j k : Fin 4) :
    (DriftSecondOutput.outputFrame s DriftBlockInitial.seed j k 0).left i ∈ gradient i j :=
  contains_sound (gradient_checked i j) (DriftSecondOutput.center_first_mem s h j k i)

theorem hessian_mem (s : NaiveReuppingLadder ℝ) (h : LockedParameters.Parameters s)
    (θ : ℝ) (r : Normal) (hθ : θ ∈ DriftProfileCellData.angle)
    (hr : ∀ j, |r j| ≤ (1/500000:ℝ)) (i j k : Fin 4) :
    (DriftSecondOutput.outputFrame s (DriftBlockInitial.coordinates θ r) j k 0).mixed i ∈ hessian i j k :=
  contains_sound (hessian_checked i j k) (DriftSecondOutput.output_mixed_mem s h θ r hθ hr j k i)

end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftOutputBounds
