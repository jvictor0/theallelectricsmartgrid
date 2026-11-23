import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck04
open Enclosure DriftBlockCellData DriftSecondCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem second_00 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 40) (fun i => matrices 41 i j) (fun i => matrices 41 i k) (fun i => hessian 40 i j k) (fun i => hessian 41 i j k) = true := by decide +kernel
private theorem center_00 : matrixCheck LockedData.coefficients (centerMatrix 40) (centerMatrix 41) (centerSaturation 40) = true := by decide +kernel
private theorem second_01 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 41) (fun i => matrices 42 i j) (fun i => matrices 42 i k) (fun i => hessian 41 i j k) (fun i => hessian 42 i j k) = true := by decide +kernel
private theorem center_01 : matrixCheck LockedData.coefficients (centerMatrix 41) (centerMatrix 42) (centerSaturation 41) = true := by decide +kernel
private theorem second_02 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 42) (fun i => matrices 43 i j) (fun i => matrices 43 i k) (fun i => hessian 42 i j k) (fun i => hessian 43 i j k) = true := by decide +kernel
private theorem center_02 : matrixCheck LockedData.coefficients (centerMatrix 42) (centerMatrix 43) (centerSaturation 42) = true := by decide +kernel
private theorem second_03 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 43) (fun i => matrices 44 i j) (fun i => matrices 44 i k) (fun i => hessian 43 i j k) (fun i => hessian 44 i j k) = true := by decide +kernel
private theorem center_03 : matrixCheck LockedData.coefficients (centerMatrix 43) (centerMatrix 44) (centerSaturation 43) = true := by decide +kernel
private theorem second_04 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 44) (fun i => matrices 45 i j) (fun i => matrices 45 i k) (fun i => hessian 44 i j k) (fun i => hessian 45 i j k) = true := by decide +kernel
private theorem center_04 : matrixCheck LockedData.coefficients (centerMatrix 44) (centerMatrix 45) (centerSaturation 44) = true := by decide +kernel
private theorem second_05 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 45) (fun i => matrices 46 i j) (fun i => matrices 46 i k) (fun i => hessian 45 i j k) (fun i => hessian 46 i j k) = true := by decide +kernel
private theorem center_05 : matrixCheck LockedData.coefficients (centerMatrix 45) (centerMatrix 46) (centerSaturation 45) = true := by decide +kernel
private theorem second_06 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 46) (fun i => matrices 47 i j) (fun i => matrices 47 i k) (fun i => hessian 46 i j k) (fun i => hessian 47 i j k) = true := by decide +kernel
private theorem center_06 : matrixCheck LockedData.coefficients (centerMatrix 46) (centerMatrix 47) (centerSaturation 46) = true := by decide +kernel
private theorem second_07 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 47) (fun i => matrices 48 i j) (fun i => matrices 48 i k) (fun i => hessian 47 i j k) (fun i => hessian 48 i j k) = true := by decide +kernel
private theorem center_07 : matrixCheck LockedData.coefficients (centerMatrix 47) (centerMatrix 48) (centerSaturation 47) = true := by decide +kernel
private theorem second_08 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 48) (fun i => matrices 49 i j) (fun i => matrices 49 i k) (fun i => hessian 48 i j k) (fun i => hessian 49 i j k) = true := by decide +kernel
private theorem center_08 : matrixCheck LockedData.coefficients (centerMatrix 48) (centerMatrix 49) (centerSaturation 48) = true := by decide +kernel
private theorem second_09 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 49) (fun i => matrices 50 i j) (fun i => matrices 50 i k) (fun i => hessian 49 i j k) (fun i => hessian 50 i j k) = true := by decide +kernel
private theorem center_09 : matrixCheck LockedData.coefficients (centerMatrix 49) (centerMatrix 50) (centerSaturation 49) = true := by decide +kernel
theorem second : ∀ j : Fin 10, let n := 40+j.val; ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation n) (fun i => matrices (n+1) i j) (fun i => matrices (n+1) i k) (fun i => hessian n i j k) (fun i => hessian (n+1) i j k) = true := by
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
theorem center : ∀ j : Fin 10, let n := 40+j.val; matrixCheck LockedData.coefficients (centerMatrix n) (centerMatrix (n+1)) (centerSaturation n) = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck04
