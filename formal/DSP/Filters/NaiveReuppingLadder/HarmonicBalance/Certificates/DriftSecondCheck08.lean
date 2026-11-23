import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck08
open Enclosure DriftBlockCellData DriftSecondCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem second_00 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 80) (fun i => matrices 81 i j) (fun i => matrices 81 i k) (fun i => hessian 80 i j k) (fun i => hessian 81 i j k) = true := by decide +kernel
private theorem center_00 : matrixCheck LockedData.coefficients (centerMatrix 80) (centerMatrix 81) (centerSaturation 80) = true := by decide +kernel
private theorem second_01 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 81) (fun i => matrices 82 i j) (fun i => matrices 82 i k) (fun i => hessian 81 i j k) (fun i => hessian 82 i j k) = true := by decide +kernel
private theorem center_01 : matrixCheck LockedData.coefficients (centerMatrix 81) (centerMatrix 82) (centerSaturation 81) = true := by decide +kernel
private theorem second_02 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 82) (fun i => matrices 83 i j) (fun i => matrices 83 i k) (fun i => hessian 82 i j k) (fun i => hessian 83 i j k) = true := by decide +kernel
private theorem center_02 : matrixCheck LockedData.coefficients (centerMatrix 82) (centerMatrix 83) (centerSaturation 82) = true := by decide +kernel
private theorem second_03 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 83) (fun i => matrices 84 i j) (fun i => matrices 84 i k) (fun i => hessian 83 i j k) (fun i => hessian 84 i j k) = true := by decide +kernel
private theorem center_03 : matrixCheck LockedData.coefficients (centerMatrix 83) (centerMatrix 84) (centerSaturation 83) = true := by decide +kernel
private theorem second_04 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 84) (fun i => matrices 85 i j) (fun i => matrices 85 i k) (fun i => hessian 84 i j k) (fun i => hessian 85 i j k) = true := by decide +kernel
private theorem center_04 : matrixCheck LockedData.coefficients (centerMatrix 84) (centerMatrix 85) (centerSaturation 84) = true := by decide +kernel
private theorem second_05 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 85) (fun i => matrices 86 i j) (fun i => matrices 86 i k) (fun i => hessian 85 i j k) (fun i => hessian 86 i j k) = true := by decide +kernel
private theorem center_05 : matrixCheck LockedData.coefficients (centerMatrix 85) (centerMatrix 86) (centerSaturation 85) = true := by decide +kernel
private theorem second_06 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 86) (fun i => matrices 87 i j) (fun i => matrices 87 i k) (fun i => hessian 86 i j k) (fun i => hessian 87 i j k) = true := by decide +kernel
private theorem center_06 : matrixCheck LockedData.coefficients (centerMatrix 86) (centerMatrix 87) (centerSaturation 86) = true := by decide +kernel
private theorem second_07 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 87) (fun i => matrices 88 i j) (fun i => matrices 88 i k) (fun i => hessian 87 i j k) (fun i => hessian 88 i j k) = true := by decide +kernel
private theorem center_07 : matrixCheck LockedData.coefficients (centerMatrix 87) (centerMatrix 88) (centerSaturation 87) = true := by decide +kernel
private theorem second_08 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 88) (fun i => matrices 89 i j) (fun i => matrices 89 i k) (fun i => hessian 88 i j k) (fun i => hessian 89 i j k) = true := by decide +kernel
private theorem center_08 : matrixCheck LockedData.coefficients (centerMatrix 88) (centerMatrix 89) (centerSaturation 88) = true := by decide +kernel
private theorem second_09 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 89) (fun i => matrices 90 i j) (fun i => matrices 90 i k) (fun i => hessian 89 i j k) (fun i => hessian 90 i j k) = true := by decide +kernel
private theorem center_09 : matrixCheck LockedData.coefficients (centerMatrix 89) (centerMatrix 90) (centerSaturation 89) = true := by decide +kernel
theorem second : ∀ j : Fin 10, let n := 80+j.val; ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation n) (fun i => matrices (n+1) i j) (fun i => matrices (n+1) i k) (fun i => hessian n i j k) (fun i => hessian (n+1) i j k) = true := by
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
theorem center : ∀ j : Fin 10, let n := 80+j.val; matrixCheck LockedData.coefficients (centerMatrix n) (centerMatrix (n+1)) (centerSaturation n) = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck08
