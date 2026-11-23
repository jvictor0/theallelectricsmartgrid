import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck07
open Enclosure DriftBlockCellData DriftSecondCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem second_00 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 70) (fun i => matrices 71 i j) (fun i => matrices 71 i k) (fun i => hessian 70 i j k) (fun i => hessian 71 i j k) = true := by decide +kernel
private theorem center_00 : matrixCheck LockedData.coefficients (centerMatrix 70) (centerMatrix 71) (centerSaturation 70) = true := by decide +kernel
private theorem second_01 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 71) (fun i => matrices 72 i j) (fun i => matrices 72 i k) (fun i => hessian 71 i j k) (fun i => hessian 72 i j k) = true := by decide +kernel
private theorem center_01 : matrixCheck LockedData.coefficients (centerMatrix 71) (centerMatrix 72) (centerSaturation 71) = true := by decide +kernel
private theorem second_02 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 72) (fun i => matrices 73 i j) (fun i => matrices 73 i k) (fun i => hessian 72 i j k) (fun i => hessian 73 i j k) = true := by decide +kernel
private theorem center_02 : matrixCheck LockedData.coefficients (centerMatrix 72) (centerMatrix 73) (centerSaturation 72) = true := by decide +kernel
private theorem second_03 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 73) (fun i => matrices 74 i j) (fun i => matrices 74 i k) (fun i => hessian 73 i j k) (fun i => hessian 74 i j k) = true := by decide +kernel
private theorem center_03 : matrixCheck LockedData.coefficients (centerMatrix 73) (centerMatrix 74) (centerSaturation 73) = true := by decide +kernel
private theorem second_04 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 74) (fun i => matrices 75 i j) (fun i => matrices 75 i k) (fun i => hessian 74 i j k) (fun i => hessian 75 i j k) = true := by decide +kernel
private theorem center_04 : matrixCheck LockedData.coefficients (centerMatrix 74) (centerMatrix 75) (centerSaturation 74) = true := by decide +kernel
private theorem second_05 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 75) (fun i => matrices 76 i j) (fun i => matrices 76 i k) (fun i => hessian 75 i j k) (fun i => hessian 76 i j k) = true := by decide +kernel
private theorem center_05 : matrixCheck LockedData.coefficients (centerMatrix 75) (centerMatrix 76) (centerSaturation 75) = true := by decide +kernel
private theorem second_06 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 76) (fun i => matrices 77 i j) (fun i => matrices 77 i k) (fun i => hessian 76 i j k) (fun i => hessian 77 i j k) = true := by decide +kernel
private theorem center_06 : matrixCheck LockedData.coefficients (centerMatrix 76) (centerMatrix 77) (centerSaturation 76) = true := by decide +kernel
private theorem second_07 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 77) (fun i => matrices 78 i j) (fun i => matrices 78 i k) (fun i => hessian 77 i j k) (fun i => hessian 78 i j k) = true := by decide +kernel
private theorem center_07 : matrixCheck LockedData.coefficients (centerMatrix 77) (centerMatrix 78) (centerSaturation 77) = true := by decide +kernel
private theorem second_08 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 78) (fun i => matrices 79 i j) (fun i => matrices 79 i k) (fun i => hessian 78 i j k) (fun i => hessian 79 i j k) = true := by decide +kernel
private theorem center_08 : matrixCheck LockedData.coefficients (centerMatrix 78) (centerMatrix 79) (centerSaturation 78) = true := by decide +kernel
private theorem second_09 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 79) (fun i => matrices 80 i j) (fun i => matrices 80 i k) (fun i => hessian 79 i j k) (fun i => hessian 80 i j k) = true := by decide +kernel
private theorem center_09 : matrixCheck LockedData.coefficients (centerMatrix 79) (centerMatrix 80) (centerSaturation 79) = true := by decide +kernel
theorem second : ∀ j : Fin 10, let n := 70+j.val; ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation n) (fun i => matrices (n+1) i j) (fun i => matrices (n+1) i k) (fun i => hessian n i j k) (fun i => hessian (n+1) i j k) = true := by
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
theorem center : ∀ j : Fin 10, let n := 70+j.val; matrixCheck LockedData.coefficients (centerMatrix n) (centerMatrix (n+1)) (centerSaturation n) = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck07
