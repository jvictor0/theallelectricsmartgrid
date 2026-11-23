import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck00
open Enclosure DriftBlockCellData DriftSecondCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem second_00 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 0) (fun i => matrices 1 i j) (fun i => matrices 1 i k) (fun i => hessian 0 i j k) (fun i => hessian 1 i j k) = true := by decide +kernel
private theorem center_00 : matrixCheck LockedData.coefficients (centerMatrix 0) (centerMatrix 1) (centerSaturation 0) = true := by decide +kernel
private theorem second_01 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 1) (fun i => matrices 2 i j) (fun i => matrices 2 i k) (fun i => hessian 1 i j k) (fun i => hessian 2 i j k) = true := by decide +kernel
private theorem center_01 : matrixCheck LockedData.coefficients (centerMatrix 1) (centerMatrix 2) (centerSaturation 1) = true := by decide +kernel
private theorem second_02 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 2) (fun i => matrices 3 i j) (fun i => matrices 3 i k) (fun i => hessian 2 i j k) (fun i => hessian 3 i j k) = true := by decide +kernel
private theorem center_02 : matrixCheck LockedData.coefficients (centerMatrix 2) (centerMatrix 3) (centerSaturation 2) = true := by decide +kernel
private theorem second_03 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 3) (fun i => matrices 4 i j) (fun i => matrices 4 i k) (fun i => hessian 3 i j k) (fun i => hessian 4 i j k) = true := by decide +kernel
private theorem center_03 : matrixCheck LockedData.coefficients (centerMatrix 3) (centerMatrix 4) (centerSaturation 3) = true := by decide +kernel
private theorem second_04 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 4) (fun i => matrices 5 i j) (fun i => matrices 5 i k) (fun i => hessian 4 i j k) (fun i => hessian 5 i j k) = true := by decide +kernel
private theorem center_04 : matrixCheck LockedData.coefficients (centerMatrix 4) (centerMatrix 5) (centerSaturation 4) = true := by decide +kernel
private theorem second_05 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 5) (fun i => matrices 6 i j) (fun i => matrices 6 i k) (fun i => hessian 5 i j k) (fun i => hessian 6 i j k) = true := by decide +kernel
private theorem center_05 : matrixCheck LockedData.coefficients (centerMatrix 5) (centerMatrix 6) (centerSaturation 5) = true := by decide +kernel
private theorem second_06 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 6) (fun i => matrices 7 i j) (fun i => matrices 7 i k) (fun i => hessian 6 i j k) (fun i => hessian 7 i j k) = true := by decide +kernel
private theorem center_06 : matrixCheck LockedData.coefficients (centerMatrix 6) (centerMatrix 7) (centerSaturation 6) = true := by decide +kernel
private theorem second_07 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 7) (fun i => matrices 8 i j) (fun i => matrices 8 i k) (fun i => hessian 7 i j k) (fun i => hessian 8 i j k) = true := by decide +kernel
private theorem center_07 : matrixCheck LockedData.coefficients (centerMatrix 7) (centerMatrix 8) (centerSaturation 7) = true := by decide +kernel
private theorem second_08 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 8) (fun i => matrices 9 i j) (fun i => matrices 9 i k) (fun i => hessian 8 i j k) (fun i => hessian 9 i j k) = true := by decide +kernel
private theorem center_08 : matrixCheck LockedData.coefficients (centerMatrix 8) (centerMatrix 9) (centerSaturation 8) = true := by decide +kernel
private theorem second_09 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 9) (fun i => matrices 10 i j) (fun i => matrices 10 i k) (fun i => hessian 9 i j k) (fun i => hessian 10 i j k) = true := by decide +kernel
private theorem center_09 : matrixCheck LockedData.coefficients (centerMatrix 9) (centerMatrix 10) (centerSaturation 9) = true := by decide +kernel
theorem second : ∀ j : Fin 10, let n := 0+j.val; ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation n) (fun i => matrices (n+1) i j) (fun i => matrices (n+1) i k) (fun i => hessian n i j k) (fun i => hessian (n+1) i j k) = true := by
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
theorem center : ∀ j : Fin 10, let n := 0+j.val; matrixCheck LockedData.coefficients (centerMatrix n) (centerMatrix (n+1)) (centerSaturation n) = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck00
