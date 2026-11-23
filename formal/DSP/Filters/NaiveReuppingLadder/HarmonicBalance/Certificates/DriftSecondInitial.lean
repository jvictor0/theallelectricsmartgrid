import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCellData
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockGeometry
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.CoordinateVariation

/-! Exact curved initial frames for first and mixed second variations, with
whole-cell Hessian bounds and a tighter center-only Jacobian enclosure. -/

noncomputable section
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondInitial
open LeanCert.Core DSP.IntervalArithmetic PolarTube Enclosure
open DriftBlockInitial DriftSecondCellData
open scoped Matrix
set_option maxRecDepth 100000
set_option maxHeartbeats 0

def frame (x : State) (j k : Fin 4) (t : ℝ) : DerivativeFrame :=
  DerivativeFrame.linear DriftChart.coordinates.symm
    (DerivativeFrame.decode DriftChart.profile (fun θ => DriftChart.profileDerivative θ)
      (fun θ => DriftChart.profileSecondDerivative θ)
      (DerivativeFrame.affine x (Pi.single j 1) (Pi.single k 1) t))

theorem differentiates (x : State) (j k : Fin 4) (t : ℝ) :
    DerivativeFrame.Differentiates (frame x j k) t :=
  DerivativeFrame.linear_differentiates DriftChart.coordinates.symm _ t
    (DerivativeFrame.decode_differentiates DriftChart.profile _ _ _ t
      (DerivativeFrame.affine_differentiates x _ _ t)
      (fun θ => hasDerivAt_pi.mp (DriftChart.profile_hasDerivAt θ))
      (fun θ => hasDerivAt_pi.mp (DriftChart.profileDerivative_hasDerivAt θ)))

theorem value_zero (θ : ℝ) (r : Normal) (j k : Fin 4) :
    (frame (coordinates θ r) j k 0).value = point θ r := by
  simp [frame,DerivativeFrame.linear,DerivativeFrame.decode,DerivativeFrame.affine,point,
    DriftChart.coordinates]

theorem left_zero (x : State) (j k : Fin 4) :
    (frame x j k 0).left = fun i =>
      (DriftChart.inverse*decodeJacobian DriftChart.profile (fun θ => DriftChart.profileDerivative θ) x) i j := by
  simp only [frame,DerivativeFrame.linear,DerivativeFrame.decode,DerivativeFrame.affine,zero_smul,add_zero]
  change DriftChart.inverse *ᵥ decodeVariation _ _ x (Pi.single j 1) = _
  rw [← decodeJacobian_mul,Matrix.mulVec_mulVec,Matrix.mulVec_single_one]
  rfl

theorem right_zero (x : State) (j k : Fin 4) :
    (frame x j k 0).right = (frame x k j 0).left := by
  simp [frame,DerivativeFrame.linear,DerivativeFrame.decode,DerivativeFrame.affine]

theorem left_mem (θ : ℝ) (r : Normal) (hθ : θ ∈ DriftProfileCellData.angle)
    (hr : ∀ j, |r j| ≤ (1/500000:ℝ)) (j k : Fin 4) :
    ∀ i, (frame (coordinates θ r) j k 0).left i ∈ DriftBlockCellData.matrices 0 i j := by
  rw [left_zero]
  intro i
  simpa [path,displacement] using jacobian_mem θ r hθ hr 1 (by norm_num) i j

def rawHessianBox : HessianBox := decodeHessianBox (DriftProfileCellData.ranges 0)
  (DriftProfileCellData.ranges 1) (DriftProfileCellData.ranges 2) radialBox cosineBox sineBox

theorem mixed_mem (θ : ℝ) (r : Normal) (hθ : θ ∈ DriftProfileCellData.angle)
    (hr : ∀ j, |r j| ≤ (1/500000:ℝ)) (j k : Fin 4) :
    ∀ i, (frame (coordinates θ r) j k 0).mixed i ∈ hessian 0 i j k := by
  have hc : ∀ i j k, contains (hessian 0 i j k)
      (matrixVector DriftBlockCellData.inverseBox (fun l => rawHessianBox l j k) i) = true := by decide +kernel
  have hb := DriftBlockGeometry.carriers_mem θ hθ
  have hraw := decodeHessianBox_mem DriftChart.profile (fun θ => DriftChart.profileDerivative θ)
    (fun θ => DriftChart.profileSecondDerivative θ) (coordinates θ r)
    (DriftProfileCellData.ranges 0) (DriftProfileCellData.ranges 1) (DriftProfileCellData.ranges 2)
    radialBox cosineBox sineBox (DriftProfileCell.profile_mem θ hθ)
    (DriftProfileCell.profileDerivative_mem θ hθ) (DriftProfileCell.profileSecondDerivative_mem θ hθ)
    (by norm_num [coordinates,radialBox,IntervalRat.mem_def]; exact abs_le.mp (hr 0)) hb.1 hb.2
  intro i
  have h := contains_sound (hc i j k) (mem_matrixVector _ _ _ _ inverse_mem (fun l => hraw l j k) i)
  simp only [frame,DerivativeFrame.linear,DerivativeFrame.decode,DerivativeFrame.affine,zero_smul,add_zero]
  exact h

theorem center_left_mem (j k : Fin 4) : ∀ i, (frame seed j k 0).left i ∈ centerMatrix 0 i j := by
  rw [left_zero]
  have hc : ∀ i j, contains (centerMatrix 0 i j) (matrixProduct DriftBlockCellData.inverseBox
      (decodeJacobianBox (DriftProfileCellData.pointRanges 0) (DriftProfileCellData.pointRanges 1)
        (IntervalRat.singleton 0) (DriftProfileCellData.base 0) (DriftProfileCellData.base 1)) i j) = true := by decide +kernel
  have hz : seed 1 ∈ IntervalRat.singleton 0 := by simpa [seed] using IntervalRat.mem_singleton 0
  have hraw := decodeJacobianBox_mem DriftChart.profile (fun θ => DriftChart.profileDerivative θ) seed
    (DriftProfileCellData.pointRanges 0) (DriftProfileCellData.pointRanges 1) (IntervalRat.singleton 0)
    (DriftProfileCellData.base 0) (DriftProfileCellData.base 1)
    (DriftProfileCell.point_ranges_mem 0) (DriftProfileCell.point_ranges_mem 1) hz
    DriftProfileCell.base_mem.1 DriftProfileCell.base_mem.2
  exact fun i => contains_sound (hc i j) (mem_matrixProduct _ _ _ _ inverse_mem hraw i j)

end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondInitial
