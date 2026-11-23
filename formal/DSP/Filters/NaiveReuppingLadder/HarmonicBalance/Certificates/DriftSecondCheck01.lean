import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck01
open Enclosure DriftBlockCellData DriftSecondCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem second_00 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 10) (fun i => matrices 11 i j) (fun i => matrices 11 i k) (fun i => hessian 10 i j k) (fun i => hessian 11 i j k) = true := by decide +kernel
private theorem center_00 : matrixCheck LockedData.coefficients (centerMatrix 10) (centerMatrix 11) (centerSaturation 10) = true := by decide +kernel
private theorem second_01 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 11) (fun i => matrices 12 i j) (fun i => matrices 12 i k) (fun i => hessian 11 i j k) (fun i => hessian 12 i j k) = true := by decide +kernel
private theorem center_01 : matrixCheck LockedData.coefficients (centerMatrix 11) (centerMatrix 12) (centerSaturation 11) = true := by decide +kernel
private theorem second_02 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 12) (fun i => matrices 13 i j) (fun i => matrices 13 i k) (fun i => hessian 12 i j k) (fun i => hessian 13 i j k) = true := by decide +kernel
private theorem center_02 : matrixCheck LockedData.coefficients (centerMatrix 12) (centerMatrix 13) (centerSaturation 12) = true := by decide +kernel
private theorem second_03 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 13) (fun i => matrices 14 i j) (fun i => matrices 14 i k) (fun i => hessian 13 i j k) (fun i => hessian 14 i j k) = true := by decide +kernel
private theorem center_03 : matrixCheck LockedData.coefficients (centerMatrix 13) (centerMatrix 14) (centerSaturation 13) = true := by decide +kernel
private theorem second_04 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 14) (fun i => matrices 15 i j) (fun i => matrices 15 i k) (fun i => hessian 14 i j k) (fun i => hessian 15 i j k) = true := by decide +kernel
private theorem center_04 : matrixCheck LockedData.coefficients (centerMatrix 14) (centerMatrix 15) (centerSaturation 14) = true := by decide +kernel
private theorem second_05 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 15) (fun i => matrices 16 i j) (fun i => matrices 16 i k) (fun i => hessian 15 i j k) (fun i => hessian 16 i j k) = true := by decide +kernel
private theorem center_05 : matrixCheck LockedData.coefficients (centerMatrix 15) (centerMatrix 16) (centerSaturation 15) = true := by decide +kernel
private theorem second_06 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 16) (fun i => matrices 17 i j) (fun i => matrices 17 i k) (fun i => hessian 16 i j k) (fun i => hessian 17 i j k) = true := by decide +kernel
private theorem center_06 : matrixCheck LockedData.coefficients (centerMatrix 16) (centerMatrix 17) (centerSaturation 16) = true := by decide +kernel
private theorem second_07 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 17) (fun i => matrices 18 i j) (fun i => matrices 18 i k) (fun i => hessian 17 i j k) (fun i => hessian 18 i j k) = true := by decide +kernel
private theorem center_07 : matrixCheck LockedData.coefficients (centerMatrix 17) (centerMatrix 18) (centerSaturation 17) = true := by decide +kernel
private theorem second_08 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 18) (fun i => matrices 19 i j) (fun i => matrices 19 i k) (fun i => hessian 18 i j k) (fun i => hessian 19 i j k) = true := by decide +kernel
private theorem center_08 : matrixCheck LockedData.coefficients (centerMatrix 18) (centerMatrix 19) (centerSaturation 18) = true := by decide +kernel
private theorem second_09 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 19) (fun i => matrices 20 i j) (fun i => matrices 20 i k) (fun i => hessian 19 i j k) (fun i => hessian 20 i j k) = true := by decide +kernel
private theorem center_09 : matrixCheck LockedData.coefficients (centerMatrix 19) (centerMatrix 20) (centerSaturation 19) = true := by decide +kernel
theorem second : ∀ j : Fin 10, let n := 10+j.val; ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation n) (fun i => matrices (n+1) i j) (fun i => matrices (n+1) i k) (fun i => hessian n i j k) (fun i => hessian (n+1) i j k) = true := by
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
theorem center : ∀ j : Fin 10, let n := 10+j.val; matrixCheck LockedData.coefficients (centerMatrix n) (centerMatrix (n+1)) (centerSaturation n) = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck01
