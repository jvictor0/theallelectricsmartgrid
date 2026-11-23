import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockDerivatives
import DSP.Analysis.PolarTube.Lift

/-! Domain safety at the output of the checked curved cell. The entire
output head stays positively aligned with the input phase, so the phase lift
has a safe branch. This is one cell, not a uniform whole-circle certificate. -/

noncomputable section
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockGeometry
open LeanCert.Core DSP.IntervalArithmetic PolarTube Enclosure DriftBlockCellData DriftBlockInitial
set_option maxRecDepth 100000
set_option maxHeartbeats 0

def transformBox : MatrixBox := fun i j => IntervalRat.singleton (DriftChartData.transformRat i j)
def endBox : Box := matrixVector transformBox (whole 162)
def alignmentBox : IntervalRat := ((endBox 0).mul cosineBox).add ((endBox 1).mul sineBox)
def block (s : NaiveReuppingLadder ℝ) (x : State) : State :=
  Step.states (audioStep s) DriftBlockParameters.forcing x 162

theorem transform_mem : ∀ i j, DriftChart.transform i j ∈ transformBox i j :=
  fun i j => IntervalRat.mem_singleton (DriftChartData.transformRat i j)

theorem end_mem (s : NaiveReuppingLadder ℝ) (h : LockedParameters.Parameters s)
    (θ : ℝ) (r : Normal) (hθ : θ ∈ DriftProfileCellData.angle)
    (hr : ∀ j, |r j| ≤ (1/500000:ℝ)) :
    ∀ i, DriftChart.chart.linear (block s (DriftChart.chart.decode ((θ:Real.Angle),r))) i ∈ endBox i :=
  mem_matrixVector _ _ _ _ transform_mem (DriftBlockCell.states_mem s h θ r hθ hr 162 le_rfl)

theorem carriers_mem (θ : ℝ) (hθ : θ ∈ DriftProfileCellData.angle) :
    Real.cos θ ∈ cosineBox ∧ Real.sin θ ∈ sineBox := by
  have hmid : (DriftProfileCellData.angle.midpoint:ℝ) = 5/2 := by
    norm_num [DriftProfileCellData.angle,DriftProfileCellData.interval,IntervalRat.midpoint]
  have hd := IntervalTrig.midpoint_distance DriftProfileCellData.angle θ hθ
  rw [hmid] at hd
  exact ⟨IntervalTrig.widen_mem _ _ _ _ DriftProfileCell.base_mem.1 ((Real.abs_cos_sub_cos_le _ _).trans hd),
    IntervalTrig.widen_mem _ _ _ _ DriftProfileCell.base_mem.2 ((Real.abs_sin_sub_sin_le _ _).trans hd)⟩

theorem alignment_checked : (1:ℚ) < alignmentBox.lo := by decide +kernel

theorem alignment (s : NaiveReuppingLadder ℝ) (h : LockedParameters.Parameters s)
    (θ : ℝ) (r : Normal) (hθ : θ ∈ DriftProfileCellData.angle)
    (hr : ∀ j, |r j| ≤ (1/500000:ℝ)) :
    1 < (DriftChart.chart.nextHead (block s) (θ:Real.Angle) r).re*Real.cos θ+
      (DriftChart.chart.nextHead (block s) (θ:Real.Angle) r).im*Real.sin θ := by
  have hv := end_mem s h θ r hθ hr
  have ht := carriers_mem θ hθ
  have he := IntervalRat.mem_add (IntervalRat.mem_mul (hv 0) ht.1) (IntervalRat.mem_mul (hv 1) ht.2)
  have hl : (1:ℝ) < (alignmentBox.lo:ℝ) := by exact_mod_cast alignment_checked
  exact hl.trans_le he.1

theorem branch_safe (s : NaiveReuppingLadder ℝ) (h : LockedParameters.Parameters s)
    (θ : ℝ) (r : Normal) (hθ : θ ∈ DriftProfileCellData.angle)
    (hr : ∀ j, |r j| ≤ (1/500000:ℝ)) :
    DriftChart.chart.nextHead (block s) (θ:Real.Angle) r / unitPhase (θ:Real.Angle) ∈ Complex.slitPlane :=
  relative_safe _ _ (lt_trans (by norm_num) (alignment s h θ r hθ hr))

theorem head_ne_zero (s : NaiveReuppingLadder ℝ) (h : LockedParameters.Parameters s)
    (θ : ℝ) (r : Normal) (hθ : θ ∈ DriftProfileCellData.angle)
    (hr : ∀ j, |r j| ≤ (1/500000:ℝ)) :
    DriftChart.chart.nextHead (block s) (θ:Real.Angle) r ≠ 0 := by
  intro he
  have hb := alignment s h θ r hθ hr
  norm_num [he] at hb

end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockGeometry
