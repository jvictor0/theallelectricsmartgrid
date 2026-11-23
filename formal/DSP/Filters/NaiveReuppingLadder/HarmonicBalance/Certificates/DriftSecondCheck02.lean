import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck02
open Enclosure DriftBlockCellData DriftSecondCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem second_00 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 20) (fun i => matrices 21 i j) (fun i => matrices 21 i k) (fun i => hessian 20 i j k) (fun i => hessian 21 i j k) = true := by decide +kernel
private theorem center_00 : matrixCheck LockedData.coefficients (centerMatrix 20) (centerMatrix 21) (centerSaturation 20) = true := by decide +kernel
private theorem second_01 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 21) (fun i => matrices 22 i j) (fun i => matrices 22 i k) (fun i => hessian 21 i j k) (fun i => hessian 22 i j k) = true := by decide +kernel
private theorem center_01 : matrixCheck LockedData.coefficients (centerMatrix 21) (centerMatrix 22) (centerSaturation 21) = true := by decide +kernel
private theorem second_02 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 22) (fun i => matrices 23 i j) (fun i => matrices 23 i k) (fun i => hessian 22 i j k) (fun i => hessian 23 i j k) = true := by decide +kernel
private theorem center_02 : matrixCheck LockedData.coefficients (centerMatrix 22) (centerMatrix 23) (centerSaturation 22) = true := by decide +kernel
private theorem second_03 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 23) (fun i => matrices 24 i j) (fun i => matrices 24 i k) (fun i => hessian 23 i j k) (fun i => hessian 24 i j k) = true := by decide +kernel
private theorem center_03 : matrixCheck LockedData.coefficients (centerMatrix 23) (centerMatrix 24) (centerSaturation 23) = true := by decide +kernel
private theorem second_04 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 24) (fun i => matrices 25 i j) (fun i => matrices 25 i k) (fun i => hessian 24 i j k) (fun i => hessian 25 i j k) = true := by decide +kernel
private theorem center_04 : matrixCheck LockedData.coefficients (centerMatrix 24) (centerMatrix 25) (centerSaturation 24) = true := by decide +kernel
private theorem second_05 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 25) (fun i => matrices 26 i j) (fun i => matrices 26 i k) (fun i => hessian 25 i j k) (fun i => hessian 26 i j k) = true := by decide +kernel
private theorem center_05 : matrixCheck LockedData.coefficients (centerMatrix 25) (centerMatrix 26) (centerSaturation 25) = true := by decide +kernel
private theorem second_06 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 26) (fun i => matrices 27 i j) (fun i => matrices 27 i k) (fun i => hessian 26 i j k) (fun i => hessian 27 i j k) = true := by decide +kernel
private theorem center_06 : matrixCheck LockedData.coefficients (centerMatrix 26) (centerMatrix 27) (centerSaturation 26) = true := by decide +kernel
private theorem second_07 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 27) (fun i => matrices 28 i j) (fun i => matrices 28 i k) (fun i => hessian 27 i j k) (fun i => hessian 28 i j k) = true := by decide +kernel
private theorem center_07 : matrixCheck LockedData.coefficients (centerMatrix 27) (centerMatrix 28) (centerSaturation 27) = true := by decide +kernel
private theorem second_08 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 28) (fun i => matrices 29 i j) (fun i => matrices 29 i k) (fun i => hessian 28 i j k) (fun i => hessian 29 i j k) = true := by decide +kernel
private theorem center_08 : matrixCheck LockedData.coefficients (centerMatrix 28) (centerMatrix 29) (centerSaturation 28) = true := by decide +kernel
private theorem second_09 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 29) (fun i => matrices 30 i j) (fun i => matrices 30 i k) (fun i => hessian 29 i j k) (fun i => hessian 30 i j k) = true := by decide +kernel
private theorem center_09 : matrixCheck LockedData.coefficients (centerMatrix 29) (centerMatrix 30) (centerSaturation 29) = true := by decide +kernel
theorem second : ∀ j : Fin 10, let n := 20+j.val; ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation n) (fun i => matrices (n+1) i j) (fun i => matrices (n+1) i k) (fun i => hessian n i j k) (fun i => hessian (n+1) i j k) = true := by
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
theorem center : ∀ j : Fin 10, let n := 20+j.val; matrixCheck LockedData.coefficients (centerMatrix n) (centerMatrix (n+1)) (centerSaturation n) = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck02
