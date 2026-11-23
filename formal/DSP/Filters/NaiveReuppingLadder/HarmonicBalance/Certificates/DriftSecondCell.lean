import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondInitial
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondChecks

/-! Checked first and mixed second variations of the full 162-sample block.
The whole-cell value boxes are inherited from the centered state replay;
the tighter center derivative uses the independently checked center trace. -/

noncomputable section
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCell
open LeanCert.Core PolarTube Enclosure DriftBlockCellData DriftSecondCellData DriftSecondInitial

def boxes (j k : Fin 4) (n : ℕ) : FrameBox :=
  ⟨fun i => matrices n i j,fun i => matrices n i k,fun i => hessian n i j k⟩

theorem frame_checked (j k : Fin 4) (n : ℕ) (hn : n < 162) :
    frameCheck LockedData.coefficients (boxes j k n) (boxes j k (n+1))
      (whole (n+1)) (DriftBlockCellData.saturation n) 24 = true := by
  have hc := DriftBlockChecks.steps n hn
  simp only [centeredCheck,Bool.and_eq_true,decide_eq_true_eq] at hc
  simp only [frameCheck,Bool.and_eq_true,decide_eq_true_eq,boxes]
  exact ⟨⟨⟨hc.2,matrixCheck_column _ _ _ _ hc.1.2 j⟩,
    matrixCheck_column _ _ _ _ hc.1.2 k⟩,DriftSecondChecks.second n hn j k⟩

theorem frames_mem (s : NaiveReuppingLadder ℝ) (h : LockedParameters.Parameters s)
    (θ : ℝ) (r : Normal) (hθ : θ ∈ DriftProfileCellData.angle)
    (hr : ∀ j, |r j| ≤ (1/500000:ℝ)) (j k : Fin 4) :
    ∀ n ≤ 162, (boxes j k n).Mem
      (Step.states (DerivativeFrame.advance s) DriftBlockParameters.forcing
        (frame (DriftBlockInitial.coordinates θ r) j k 0) n) := by
  apply frames_run s LockedData.coefficients (LockedParameters.valid s h) (boxes j k)
    whole DriftBlockCellData.saturation 24 162 (frame_checked j k) DriftBlockParameters.forcing
    (frame (DriftBlockInitial.coordinates θ r) j k 0)
  · refine ⟨left_mem θ r hθ hr j k,?_,mixed_mem θ r hθ hr j k⟩
    rw [right_zero]
    exact left_mem θ r hθ hr k j
  · intro n hn i
    rw [value_zero]
    exact (DriftBlockCell.replay s h θ r hθ hr n hn).2.1 i

theorem center_saturation_checked (n : ℕ) (hn : n < 162) : ∀ i,
    IntervalTanh.check ((center (n+1) i).mul LockedData.coefficients.gain) (centerSaturation n i) 24 = true := by
  have hc := DriftBlockChecks.steps n hn
  simp only [centeredCheck,Bool.and_eq_true,decide_eq_true_eq] at hc
  have hv := hc.1.1.1.1
  simp only [valuesCheck,Bool.and_eq_true,decide_eq_true_eq] at hv
  exact hv.1.1.1.2

theorem center_derivative_mem (s : NaiveReuppingLadder ℝ) (h : LockedParameters.Parameters s)
    (j k : Fin 4) : ∀ n ≤ 162, ∀ i,
      (Step.states (DerivativeFrame.advance s) DriftBlockParameters.forcing
        (frame DriftBlockInitial.seed j k 0) n).left i ∈ centerMatrix n i j := by
  apply first_run s LockedData.coefficients (LockedParameters.valid s h) centerMatrix center centerSaturation
    24 162 DriftSecondChecks.center center_saturation_checked DriftBlockParameters.forcing
    (frame DriftBlockInitial.seed j k 0) j (center_left_mem j k)
  have hs : (frame DriftBlockInitial.seed j k 0).value = DriftBlockInitial.centerPoint := by
    simp [frame,DerivativeFrame.linear,DerivativeFrame.decode,DerivativeFrame.affine,
      DriftBlockInitial.centerPoint,DriftChart.coordinates]
  rw [hs]
  exact fun n hn => (DriftBlockCell.replay s h (5/2) 0
    (by norm_num [DriftProfileCellData.angle,DriftProfileCellData.interval,IntervalRat.mem_def])
    (by intro i; norm_num) n hn).1

end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCell
