import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCell
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftOutputCell

/-! Whole-cell second derivatives and tight center first derivatives of the
complete coordinate block. Curvature of both the initial profile and the
final polar/Fourier chart is retained. -/

noncomputable section
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondOutput
open LeanCert.Core DSP.IntervalArithmetic PolarTube Enclosure DriftSecondInitial
set_option maxRecDepth 10000

def endFrame (s : NaiveReuppingLadder ℝ) (x : State) (j k : Fin 4) (t : ℝ) : DerivativeFrame :=
  DerivativeFrame.linear DriftChart.coordinates
    (Step.states (DerivativeFrame.advance s) DriftBlockParameters.forcing (frame x j k t) 162)

def outputFrame (s : NaiveReuppingLadder ℝ) (x : State) (j k : Fin 4) (t : ℝ) : DerivativeFrame :=
  DerivativeFrame.encode DriftChart.profile (fun θ => DriftChart.profileDerivative θ)
    (fun θ => DriftChart.profileSecondDerivative θ)
    (unitPhase ((DriftOutputCellData.anchor:ℝ):Real.Angle)) (endFrame s x j k t)

def firstBox (j : Fin 4) : StateBox := matrixVector DriftBlockGeometry.transformBox
  (fun i => DriftBlockCellData.matrices 162 i j)
def mixedBox (j k : Fin 4) : StateBox := matrixVector DriftBlockGeometry.transformBox
  (fun i => DriftSecondCellData.hessian 162 i j k)
def centerBox (j : Fin 4) : StateBox := matrixVector DriftBlockGeometry.transformBox
  (fun i => DriftSecondCellData.centerMatrix 162 i j)

def outputFirstBox (j : Fin 4) : StateBox := encodeFirstBox (DriftOutputCellData.ranges 1 1)
  (DriftOutputCellData.state 1) (firstBox j) (DriftOutputCellData.inverseSquare 1)
  (DriftOutputCellData.inverseRadius 1)
def outputMixedBox (j k : Fin 4) : StateBox := encodeMixedBox (DriftOutputCellData.ranges 1 1)
  (DriftOutputCellData.ranges 1 2) (DriftOutputCellData.state 1) (firstBox j) (firstBox k)
  (mixedBox j k) (DriftOutputCellData.inverseSquare 1) (DriftOutputCellData.inverseRadius 1)
def outputCenterBox (j : Fin 4) : StateBox := encodeFirstBox (DriftOutputCellData.ranges 0 1)
  (DriftOutputCellData.state 0) (centerBox j) (DriftOutputCellData.inverseSquare 0)
  (DriftOutputCellData.inverseRadius 0)

theorem value_eq (s : NaiveReuppingLadder ℝ) (θ : ℝ) (r : Normal) (j k : Fin 4) :
    (endFrame s (DriftBlockInitial.coordinates θ r) j k 0).value = DriftChart.chart.linear
      (DriftBlockGeometry.block s (DriftChart.chart.decode ((θ:Real.Angle),r))) := by
  simp only [endFrame,DerivativeFrame.linear,DerivativeFrame.run_value,value_zero]
  rw [DriftBlockCell.point_eq_decode]
  rfl

theorem end_differentiates (s : NaiveReuppingLadder ℝ) (h : LockedParameters.Parameters s)
    (x : State) (j k : Fin 4) (t : ℝ) : DerivativeFrame.Differentiates (endFrame s x j k) t :=
  DerivativeFrame.linear_differentiates DriftChart.coordinates _ t
    (DerivativeFrame.run_differentiates s h.damping DriftBlockParameters.forcing _ t
      (differentiates x j k t) 162)

theorem end_value_mem (s : NaiveReuppingLadder ℝ) (h : LockedParameters.Parameters s)
    (θ : ℝ) (r : Normal) (hθ : θ ∈ DriftProfileCellData.angle)
    (hr : ∀ j, |r j| ≤ (1/500000:ℝ)) (j k : Fin 4) :
    ∀ i, (endFrame s (DriftBlockInitial.coordinates θ r) j k 0).value i ∈ DriftOutputCellData.state 1 i := by
  rw [value_eq]
  exact DriftOutputCell.whole_state_mem s h θ r hθ hr

theorem output_differentiates (s : NaiveReuppingLadder ℝ) (h : LockedParameters.Parameters s)
    (θ : ℝ) (r : Normal) (hθ : θ ∈ DriftProfileCellData.angle)
    (hr : ∀ j, |r j| ≤ (1/500000:ℝ)) (j k : Fin 4) :
    DerivativeFrame.Differentiates (outputFrame s (DriftBlockInitial.coordinates θ r) j k) 0 :=
  DerivativeFrame.encode_differentiates DriftChart.profile _ _ _ (unitPhase_ne_zero _) _ 0
    (end_differentiates s h _ j k 0)
    (DriftOutputCell.branch_safe 1 _ (end_value_mem s h θ r hθ hr j k))
    (fun θ => hasDerivAt_pi.mp (DriftChart.profile_hasDerivAt θ))
    (fun θ => hasDerivAt_pi.mp (DriftChart.profileDerivative_hasDerivAt θ))

theorem output_mixed_mem (s : NaiveReuppingLadder ℝ) (h : LockedParameters.Parameters s)
    (θ : ℝ) (r : Normal) (hθ : θ ∈ DriftProfileCellData.angle)
    (hr : ∀ j, |r j| ≤ (1/500000:ℝ)) (j k : Fin 4) :
    ∀ i, (outputFrame s (DriftBlockInitial.coordinates θ r) j k 0).mixed i ∈ outputMixedBox j k i := by
  have hf := DriftSecondCell.frames_mem s h θ r hθ hr j k 162 le_rfl
  have hv := end_value_mem s h θ r hθ hr j k
  have hl := mem_matrixVector _ _ _ _ DriftBlockGeometry.transform_mem hf.1
  have hr' := mem_matrixVector _ _ _ _ DriftBlockGeometry.transform_mem hf.2.1
  have hm := mem_matrixVector _ _ _ _ DriftBlockGeometry.transform_mem hf.2.2
  have hi := DriftOutputCell.radius_mem 1 _ hv
  exact encodeMixedBox_mem (fun θ => DriftChart.profileDerivative θ)
    (fun θ => DriftChart.profileSecondDerivative θ) _ _ _ _ _
    (DriftOutputCellData.ranges 1 1) (DriftOutputCellData.ranges 1 2)
    (DriftOutputCellData.state 1) (firstBox j) (firstBox k) (mixedBox j k)
    (DriftOutputCellData.inverseSquare 1) (DriftOutputCellData.inverseRadius 1) hv hl hr' hm
    (DriftOutputCell.local_derivative_mem 1 _ hv) (DriftOutputCell.local_secondDerivative_mem 1 _ hv)
    (DriftOutputCell.head_ne_zero 1 _ hv) hi.2.1 hi.2.2

theorem center_first_mem (s : NaiveReuppingLadder ℝ) (h : LockedParameters.Parameters s)
    (j k : Fin 4) : ∀ i,
    (outputFrame s DriftBlockInitial.seed j k 0).left i ∈ outputCenterBox j i := by
  have hv : ∀ i, (endFrame s DriftBlockInitial.seed j k 0).value i ∈ DriftOutputCellData.state 0 i := by
    have he : (frame DriftBlockInitial.seed j k 0).value = DriftBlockInitial.centerPoint := by
      simp [frame,DerivativeFrame.linear,DerivativeFrame.decode,DerivativeFrame.affine,
        DriftBlockInitial.centerPoint,DriftChart.coordinates]
    simpa only [endFrame,DerivativeFrame.linear,DerivativeFrame.run_value,he,
      DriftBlockGeometry.block,DriftChart.chart] using DriftOutputCell.center_state_mem s h
  have hl := mem_matrixVector _ _ _ _ DriftBlockGeometry.transform_mem
    (DriftSecondCell.center_derivative_mem s h j k 162 le_rfl)
  have hi := DriftOutputCell.radius_mem 0 _ hv
  exact encodeFirstBox_mem (fun θ => DriftChart.profileDerivative θ) _ _ _
    (DriftOutputCellData.ranges 0 1) (DriftOutputCellData.state 0) (centerBox j)
    (DriftOutputCellData.inverseSquare 0) (DriftOutputCellData.inverseRadius 0) hv hl
    (DriftOutputCell.local_derivative_mem 0 _ hv) hi.2.1 hi.2.2

end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondOutput
