import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck13
open Enclosure DriftBlockCellData DriftSecondCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem second_00 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 130) (fun i => matrices 131 i j) (fun i => matrices 131 i k) (fun i => hessian 130 i j k) (fun i => hessian 131 i j k) = true := by decide +kernel
private theorem center_00 : matrixCheck LockedData.coefficients (centerMatrix 130) (centerMatrix 131) (centerSaturation 130) = true := by decide +kernel
private theorem second_01 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 131) (fun i => matrices 132 i j) (fun i => matrices 132 i k) (fun i => hessian 131 i j k) (fun i => hessian 132 i j k) = true := by decide +kernel
private theorem center_01 : matrixCheck LockedData.coefficients (centerMatrix 131) (centerMatrix 132) (centerSaturation 131) = true := by decide +kernel
private theorem second_02 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 132) (fun i => matrices 133 i j) (fun i => matrices 133 i k) (fun i => hessian 132 i j k) (fun i => hessian 133 i j k) = true := by decide +kernel
private theorem center_02 : matrixCheck LockedData.coefficients (centerMatrix 132) (centerMatrix 133) (centerSaturation 132) = true := by decide +kernel
private theorem second_03 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 133) (fun i => matrices 134 i j) (fun i => matrices 134 i k) (fun i => hessian 133 i j k) (fun i => hessian 134 i j k) = true := by decide +kernel
private theorem center_03 : matrixCheck LockedData.coefficients (centerMatrix 133) (centerMatrix 134) (centerSaturation 133) = true := by decide +kernel
private theorem second_04 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 134) (fun i => matrices 135 i j) (fun i => matrices 135 i k) (fun i => hessian 134 i j k) (fun i => hessian 135 i j k) = true := by decide +kernel
private theorem center_04 : matrixCheck LockedData.coefficients (centerMatrix 134) (centerMatrix 135) (centerSaturation 134) = true := by decide +kernel
private theorem second_05 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 135) (fun i => matrices 136 i j) (fun i => matrices 136 i k) (fun i => hessian 135 i j k) (fun i => hessian 136 i j k) = true := by decide +kernel
private theorem center_05 : matrixCheck LockedData.coefficients (centerMatrix 135) (centerMatrix 136) (centerSaturation 135) = true := by decide +kernel
private theorem second_06 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 136) (fun i => matrices 137 i j) (fun i => matrices 137 i k) (fun i => hessian 136 i j k) (fun i => hessian 137 i j k) = true := by decide +kernel
private theorem center_06 : matrixCheck LockedData.coefficients (centerMatrix 136) (centerMatrix 137) (centerSaturation 136) = true := by decide +kernel
private theorem second_07 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 137) (fun i => matrices 138 i j) (fun i => matrices 138 i k) (fun i => hessian 137 i j k) (fun i => hessian 138 i j k) = true := by decide +kernel
private theorem center_07 : matrixCheck LockedData.coefficients (centerMatrix 137) (centerMatrix 138) (centerSaturation 137) = true := by decide +kernel
private theorem second_08 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 138) (fun i => matrices 139 i j) (fun i => matrices 139 i k) (fun i => hessian 138 i j k) (fun i => hessian 139 i j k) = true := by decide +kernel
private theorem center_08 : matrixCheck LockedData.coefficients (centerMatrix 138) (centerMatrix 139) (centerSaturation 138) = true := by decide +kernel
private theorem second_09 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 139) (fun i => matrices 140 i j) (fun i => matrices 140 i k) (fun i => hessian 139 i j k) (fun i => hessian 140 i j k) = true := by decide +kernel
private theorem center_09 : matrixCheck LockedData.coefficients (centerMatrix 139) (centerMatrix 140) (centerSaturation 139) = true := by decide +kernel
theorem second : ∀ j : Fin 10, let n := 130+j.val; ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation n) (fun i => matrices (n+1) i j) (fun i => matrices (n+1) i k) (fun i => hessian n i j k) (fun i => hessian (n+1) i j k) = true := by
  intro j
  fin_cases j
  · exact second_00
  · exact second_01
  · exact second_02
  · exact second_03
  · exact second_04
  · exact second_05
  · exact second_06
  · exact second_07
  · exact second_08
  · exact second_09
theorem center : ∀ j : Fin 10, let n := 130+j.val; matrixCheck LockedData.coefficients (centerMatrix n) (centerMatrix (n+1)) (centerSaturation n) = true := by
  intro j
  fin_cases j
  · exact center_00
  · exact center_01
  · exact center_02
  · exact center_03
  · exact center_04
  · exact center_05
  · exact center_06
  · exact center_07
  · exact center_08
  · exact center_09
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck13
