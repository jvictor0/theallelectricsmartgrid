import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck05
open Enclosure DriftBlockCellData DriftSecondCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem second_00 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 50) (fun i => matrices 51 i j) (fun i => matrices 51 i k) (fun i => hessian 50 i j k) (fun i => hessian 51 i j k) = true := by decide +kernel
private theorem center_00 : matrixCheck LockedData.coefficients (centerMatrix 50) (centerMatrix 51) (centerSaturation 50) = true := by decide +kernel
private theorem second_01 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 51) (fun i => matrices 52 i j) (fun i => matrices 52 i k) (fun i => hessian 51 i j k) (fun i => hessian 52 i j k) = true := by decide +kernel
private theorem center_01 : matrixCheck LockedData.coefficients (centerMatrix 51) (centerMatrix 52) (centerSaturation 51) = true := by decide +kernel
private theorem second_02 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 52) (fun i => matrices 53 i j) (fun i => matrices 53 i k) (fun i => hessian 52 i j k) (fun i => hessian 53 i j k) = true := by decide +kernel
private theorem center_02 : matrixCheck LockedData.coefficients (centerMatrix 52) (centerMatrix 53) (centerSaturation 52) = true := by decide +kernel
private theorem second_03 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 53) (fun i => matrices 54 i j) (fun i => matrices 54 i k) (fun i => hessian 53 i j k) (fun i => hessian 54 i j k) = true := by decide +kernel
private theorem center_03 : matrixCheck LockedData.coefficients (centerMatrix 53) (centerMatrix 54) (centerSaturation 53) = true := by decide +kernel
private theorem second_04 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 54) (fun i => matrices 55 i j) (fun i => matrices 55 i k) (fun i => hessian 54 i j k) (fun i => hessian 55 i j k) = true := by decide +kernel
private theorem center_04 : matrixCheck LockedData.coefficients (centerMatrix 54) (centerMatrix 55) (centerSaturation 54) = true := by decide +kernel
private theorem second_05 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 55) (fun i => matrices 56 i j) (fun i => matrices 56 i k) (fun i => hessian 55 i j k) (fun i => hessian 56 i j k) = true := by decide +kernel
private theorem center_05 : matrixCheck LockedData.coefficients (centerMatrix 55) (centerMatrix 56) (centerSaturation 55) = true := by decide +kernel
private theorem second_06 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 56) (fun i => matrices 57 i j) (fun i => matrices 57 i k) (fun i => hessian 56 i j k) (fun i => hessian 57 i j k) = true := by decide +kernel
private theorem center_06 : matrixCheck LockedData.coefficients (centerMatrix 56) (centerMatrix 57) (centerSaturation 56) = true := by decide +kernel
private theorem second_07 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 57) (fun i => matrices 58 i j) (fun i => matrices 58 i k) (fun i => hessian 57 i j k) (fun i => hessian 58 i j k) = true := by decide +kernel
private theorem center_07 : matrixCheck LockedData.coefficients (centerMatrix 57) (centerMatrix 58) (centerSaturation 57) = true := by decide +kernel
private theorem second_08 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 58) (fun i => matrices 59 i j) (fun i => matrices 59 i k) (fun i => hessian 58 i j k) (fun i => hessian 59 i j k) = true := by decide +kernel
private theorem center_08 : matrixCheck LockedData.coefficients (centerMatrix 58) (centerMatrix 59) (centerSaturation 58) = true := by decide +kernel
private theorem second_09 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 59) (fun i => matrices 60 i j) (fun i => matrices 60 i k) (fun i => hessian 59 i j k) (fun i => hessian 60 i j k) = true := by decide +kernel
private theorem center_09 : matrixCheck LockedData.coefficients (centerMatrix 59) (centerMatrix 60) (centerSaturation 59) = true := by decide +kernel
theorem second : ∀ j : Fin 10, let n := 50+j.val; ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation n) (fun i => matrices (n+1) i j) (fun i => matrices (n+1) i k) (fun i => hessian n i j k) (fun i => hessian (n+1) i j k) = true := by
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
theorem center : ∀ j : Fin 10, let n := 50+j.val; matrixCheck LockedData.coefficients (centerMatrix n) (centerMatrix (n+1)) (centerSaturation n) = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck05
