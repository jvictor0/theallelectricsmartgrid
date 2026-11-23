import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck09
open Enclosure DriftBlockCellData DriftSecondCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem second_00 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 90) (fun i => matrices 91 i j) (fun i => matrices 91 i k) (fun i => hessian 90 i j k) (fun i => hessian 91 i j k) = true := by decide +kernel
private theorem center_00 : matrixCheck LockedData.coefficients (centerMatrix 90) (centerMatrix 91) (centerSaturation 90) = true := by decide +kernel
private theorem second_01 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 91) (fun i => matrices 92 i j) (fun i => matrices 92 i k) (fun i => hessian 91 i j k) (fun i => hessian 92 i j k) = true := by decide +kernel
private theorem center_01 : matrixCheck LockedData.coefficients (centerMatrix 91) (centerMatrix 92) (centerSaturation 91) = true := by decide +kernel
private theorem second_02 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 92) (fun i => matrices 93 i j) (fun i => matrices 93 i k) (fun i => hessian 92 i j k) (fun i => hessian 93 i j k) = true := by decide +kernel
private theorem center_02 : matrixCheck LockedData.coefficients (centerMatrix 92) (centerMatrix 93) (centerSaturation 92) = true := by decide +kernel
private theorem second_03 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 93) (fun i => matrices 94 i j) (fun i => matrices 94 i k) (fun i => hessian 93 i j k) (fun i => hessian 94 i j k) = true := by decide +kernel
private theorem center_03 : matrixCheck LockedData.coefficients (centerMatrix 93) (centerMatrix 94) (centerSaturation 93) = true := by decide +kernel
private theorem second_04 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 94) (fun i => matrices 95 i j) (fun i => matrices 95 i k) (fun i => hessian 94 i j k) (fun i => hessian 95 i j k) = true := by decide +kernel
private theorem center_04 : matrixCheck LockedData.coefficients (centerMatrix 94) (centerMatrix 95) (centerSaturation 94) = true := by decide +kernel
private theorem second_05 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 95) (fun i => matrices 96 i j) (fun i => matrices 96 i k) (fun i => hessian 95 i j k) (fun i => hessian 96 i j k) = true := by decide +kernel
private theorem center_05 : matrixCheck LockedData.coefficients (centerMatrix 95) (centerMatrix 96) (centerSaturation 95) = true := by decide +kernel
private theorem second_06 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 96) (fun i => matrices 97 i j) (fun i => matrices 97 i k) (fun i => hessian 96 i j k) (fun i => hessian 97 i j k) = true := by decide +kernel
private theorem center_06 : matrixCheck LockedData.coefficients (centerMatrix 96) (centerMatrix 97) (centerSaturation 96) = true := by decide +kernel
private theorem second_07 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 97) (fun i => matrices 98 i j) (fun i => matrices 98 i k) (fun i => hessian 97 i j k) (fun i => hessian 98 i j k) = true := by decide +kernel
private theorem center_07 : matrixCheck LockedData.coefficients (centerMatrix 97) (centerMatrix 98) (centerSaturation 97) = true := by decide +kernel
private theorem second_08 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 98) (fun i => matrices 99 i j) (fun i => matrices 99 i k) (fun i => hessian 98 i j k) (fun i => hessian 99 i j k) = true := by decide +kernel
private theorem center_08 : matrixCheck LockedData.coefficients (centerMatrix 98) (centerMatrix 99) (centerSaturation 98) = true := by decide +kernel
private theorem second_09 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 99) (fun i => matrices 100 i j) (fun i => matrices 100 i k) (fun i => hessian 99 i j k) (fun i => hessian 100 i j k) = true := by decide +kernel
private theorem center_09 : matrixCheck LockedData.coefficients (centerMatrix 99) (centerMatrix 100) (centerSaturation 99) = true := by decide +kernel
theorem second : ∀ j : Fin 10, let n := 90+j.val; ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation n) (fun i => matrices (n+1) i j) (fun i => matrices (n+1) i k) (fun i => hessian n i j k) (fun i => hessian (n+1) i j k) = true := by
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
theorem center : ∀ j : Fin 10, let n := 90+j.val; matrixCheck LockedData.coefficients (centerMatrix n) (centerMatrix (n+1)) (centerSaturation n) = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck09
