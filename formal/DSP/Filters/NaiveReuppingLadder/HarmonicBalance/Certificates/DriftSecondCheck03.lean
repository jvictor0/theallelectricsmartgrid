import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck03
open Enclosure DriftBlockCellData DriftSecondCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem second_00 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 30) (fun i => matrices 31 i j) (fun i => matrices 31 i k) (fun i => hessian 30 i j k) (fun i => hessian 31 i j k) = true := by decide +kernel
private theorem center_00 : matrixCheck LockedData.coefficients (centerMatrix 30) (centerMatrix 31) (centerSaturation 30) = true := by decide +kernel
private theorem second_01 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 31) (fun i => matrices 32 i j) (fun i => matrices 32 i k) (fun i => hessian 31 i j k) (fun i => hessian 32 i j k) = true := by decide +kernel
private theorem center_01 : matrixCheck LockedData.coefficients (centerMatrix 31) (centerMatrix 32) (centerSaturation 31) = true := by decide +kernel
private theorem second_02 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 32) (fun i => matrices 33 i j) (fun i => matrices 33 i k) (fun i => hessian 32 i j k) (fun i => hessian 33 i j k) = true := by decide +kernel
private theorem center_02 : matrixCheck LockedData.coefficients (centerMatrix 32) (centerMatrix 33) (centerSaturation 32) = true := by decide +kernel
private theorem second_03 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 33) (fun i => matrices 34 i j) (fun i => matrices 34 i k) (fun i => hessian 33 i j k) (fun i => hessian 34 i j k) = true := by decide +kernel
private theorem center_03 : matrixCheck LockedData.coefficients (centerMatrix 33) (centerMatrix 34) (centerSaturation 33) = true := by decide +kernel
private theorem second_04 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 34) (fun i => matrices 35 i j) (fun i => matrices 35 i k) (fun i => hessian 34 i j k) (fun i => hessian 35 i j k) = true := by decide +kernel
private theorem center_04 : matrixCheck LockedData.coefficients (centerMatrix 34) (centerMatrix 35) (centerSaturation 34) = true := by decide +kernel
private theorem second_05 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 35) (fun i => matrices 36 i j) (fun i => matrices 36 i k) (fun i => hessian 35 i j k) (fun i => hessian 36 i j k) = true := by decide +kernel
private theorem center_05 : matrixCheck LockedData.coefficients (centerMatrix 35) (centerMatrix 36) (centerSaturation 35) = true := by decide +kernel
private theorem second_06 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 36) (fun i => matrices 37 i j) (fun i => matrices 37 i k) (fun i => hessian 36 i j k) (fun i => hessian 37 i j k) = true := by decide +kernel
private theorem center_06 : matrixCheck LockedData.coefficients (centerMatrix 36) (centerMatrix 37) (centerSaturation 36) = true := by decide +kernel
private theorem second_07 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 37) (fun i => matrices 38 i j) (fun i => matrices 38 i k) (fun i => hessian 37 i j k) (fun i => hessian 38 i j k) = true := by decide +kernel
private theorem center_07 : matrixCheck LockedData.coefficients (centerMatrix 37) (centerMatrix 38) (centerSaturation 37) = true := by decide +kernel
private theorem second_08 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 38) (fun i => matrices 39 i j) (fun i => matrices 39 i k) (fun i => hessian 38 i j k) (fun i => hessian 39 i j k) = true := by decide +kernel
private theorem center_08 : matrixCheck LockedData.coefficients (centerMatrix 38) (centerMatrix 39) (centerSaturation 38) = true := by decide +kernel
private theorem second_09 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 39) (fun i => matrices 40 i j) (fun i => matrices 40 i k) (fun i => hessian 39 i j k) (fun i => hessian 40 i j k) = true := by decide +kernel
private theorem center_09 : matrixCheck LockedData.coefficients (centerMatrix 39) (centerMatrix 40) (centerSaturation 39) = true := by decide +kernel
theorem second : ∀ j : Fin 10, let n := 30+j.val; ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation n) (fun i => matrices (n+1) i j) (fun i => matrices (n+1) i k) (fun i => hessian n i j k) (fun i => hessian (n+1) i j k) = true := by
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
theorem center : ∀ j : Fin 10, let n := 30+j.val; matrixCheck LockedData.coefficients (centerMatrix n) (centerMatrix (n+1)) (centerSaturation n) = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck03
