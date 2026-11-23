import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck06
open Enclosure DriftBlockCellData DriftSecondCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem second_00 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 60) (fun i => matrices 61 i j) (fun i => matrices 61 i k) (fun i => hessian 60 i j k) (fun i => hessian 61 i j k) = true := by decide +kernel
private theorem center_00 : matrixCheck LockedData.coefficients (centerMatrix 60) (centerMatrix 61) (centerSaturation 60) = true := by decide +kernel
private theorem second_01 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 61) (fun i => matrices 62 i j) (fun i => matrices 62 i k) (fun i => hessian 61 i j k) (fun i => hessian 62 i j k) = true := by decide +kernel
private theorem center_01 : matrixCheck LockedData.coefficients (centerMatrix 61) (centerMatrix 62) (centerSaturation 61) = true := by decide +kernel
private theorem second_02 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 62) (fun i => matrices 63 i j) (fun i => matrices 63 i k) (fun i => hessian 62 i j k) (fun i => hessian 63 i j k) = true := by decide +kernel
private theorem center_02 : matrixCheck LockedData.coefficients (centerMatrix 62) (centerMatrix 63) (centerSaturation 62) = true := by decide +kernel
private theorem second_03 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 63) (fun i => matrices 64 i j) (fun i => matrices 64 i k) (fun i => hessian 63 i j k) (fun i => hessian 64 i j k) = true := by decide +kernel
private theorem center_03 : matrixCheck LockedData.coefficients (centerMatrix 63) (centerMatrix 64) (centerSaturation 63) = true := by decide +kernel
private theorem second_04 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 64) (fun i => matrices 65 i j) (fun i => matrices 65 i k) (fun i => hessian 64 i j k) (fun i => hessian 65 i j k) = true := by decide +kernel
private theorem center_04 : matrixCheck LockedData.coefficients (centerMatrix 64) (centerMatrix 65) (centerSaturation 64) = true := by decide +kernel
private theorem second_05 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 65) (fun i => matrices 66 i j) (fun i => matrices 66 i k) (fun i => hessian 65 i j k) (fun i => hessian 66 i j k) = true := by decide +kernel
private theorem center_05 : matrixCheck LockedData.coefficients (centerMatrix 65) (centerMatrix 66) (centerSaturation 65) = true := by decide +kernel
private theorem second_06 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 66) (fun i => matrices 67 i j) (fun i => matrices 67 i k) (fun i => hessian 66 i j k) (fun i => hessian 67 i j k) = true := by decide +kernel
private theorem center_06 : matrixCheck LockedData.coefficients (centerMatrix 66) (centerMatrix 67) (centerSaturation 66) = true := by decide +kernel
private theorem second_07 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 67) (fun i => matrices 68 i j) (fun i => matrices 68 i k) (fun i => hessian 67 i j k) (fun i => hessian 68 i j k) = true := by decide +kernel
private theorem center_07 : matrixCheck LockedData.coefficients (centerMatrix 67) (centerMatrix 68) (centerSaturation 67) = true := by decide +kernel
private theorem second_08 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 68) (fun i => matrices 69 i j) (fun i => matrices 69 i k) (fun i => hessian 68 i j k) (fun i => hessian 69 i j k) = true := by decide +kernel
private theorem center_08 : matrixCheck LockedData.coefficients (centerMatrix 68) (centerMatrix 69) (centerSaturation 68) = true := by decide +kernel
private theorem second_09 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 69) (fun i => matrices 70 i j) (fun i => matrices 70 i k) (fun i => hessian 69 i j k) (fun i => hessian 70 i j k) = true := by decide +kernel
private theorem center_09 : matrixCheck LockedData.coefficients (centerMatrix 69) (centerMatrix 70) (centerSaturation 69) = true := by decide +kernel
theorem second : ∀ j : Fin 10, let n := 60+j.val; ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation n) (fun i => matrices (n+1) i j) (fun i => matrices (n+1) i k) (fun i => hessian n i j k) (fun i => hessian (n+1) i j k) = true := by
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
theorem center : ∀ j : Fin 10, let n := 60+j.val; matrixCheck LockedData.coefficients (centerMatrix n) (centerMatrix (n+1)) (centerSaturation n) = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck06
