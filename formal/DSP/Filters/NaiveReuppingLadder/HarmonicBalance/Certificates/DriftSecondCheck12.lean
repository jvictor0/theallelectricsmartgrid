import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck12
open Enclosure DriftBlockCellData DriftSecondCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem second_00 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 120) (fun i => matrices 121 i j) (fun i => matrices 121 i k) (fun i => hessian 120 i j k) (fun i => hessian 121 i j k) = true := by decide +kernel
private theorem center_00 : matrixCheck LockedData.coefficients (centerMatrix 120) (centerMatrix 121) (centerSaturation 120) = true := by decide +kernel
private theorem second_01 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 121) (fun i => matrices 122 i j) (fun i => matrices 122 i k) (fun i => hessian 121 i j k) (fun i => hessian 122 i j k) = true := by decide +kernel
private theorem center_01 : matrixCheck LockedData.coefficients (centerMatrix 121) (centerMatrix 122) (centerSaturation 121) = true := by decide +kernel
private theorem second_02 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 122) (fun i => matrices 123 i j) (fun i => matrices 123 i k) (fun i => hessian 122 i j k) (fun i => hessian 123 i j k) = true := by decide +kernel
private theorem center_02 : matrixCheck LockedData.coefficients (centerMatrix 122) (centerMatrix 123) (centerSaturation 122) = true := by decide +kernel
private theorem second_03 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 123) (fun i => matrices 124 i j) (fun i => matrices 124 i k) (fun i => hessian 123 i j k) (fun i => hessian 124 i j k) = true := by decide +kernel
private theorem center_03 : matrixCheck LockedData.coefficients (centerMatrix 123) (centerMatrix 124) (centerSaturation 123) = true := by decide +kernel
private theorem second_04 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 124) (fun i => matrices 125 i j) (fun i => matrices 125 i k) (fun i => hessian 124 i j k) (fun i => hessian 125 i j k) = true := by decide +kernel
private theorem center_04 : matrixCheck LockedData.coefficients (centerMatrix 124) (centerMatrix 125) (centerSaturation 124) = true := by decide +kernel
private theorem second_05 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 125) (fun i => matrices 126 i j) (fun i => matrices 126 i k) (fun i => hessian 125 i j k) (fun i => hessian 126 i j k) = true := by decide +kernel
private theorem center_05 : matrixCheck LockedData.coefficients (centerMatrix 125) (centerMatrix 126) (centerSaturation 125) = true := by decide +kernel
private theorem second_06 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 126) (fun i => matrices 127 i j) (fun i => matrices 127 i k) (fun i => hessian 126 i j k) (fun i => hessian 127 i j k) = true := by decide +kernel
private theorem center_06 : matrixCheck LockedData.coefficients (centerMatrix 126) (centerMatrix 127) (centerSaturation 126) = true := by decide +kernel
private theorem second_07 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 127) (fun i => matrices 128 i j) (fun i => matrices 128 i k) (fun i => hessian 127 i j k) (fun i => hessian 128 i j k) = true := by decide +kernel
private theorem center_07 : matrixCheck LockedData.coefficients (centerMatrix 127) (centerMatrix 128) (centerSaturation 127) = true := by decide +kernel
private theorem second_08 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 128) (fun i => matrices 129 i j) (fun i => matrices 129 i k) (fun i => hessian 128 i j k) (fun i => hessian 129 i j k) = true := by decide +kernel
private theorem center_08 : matrixCheck LockedData.coefficients (centerMatrix 128) (centerMatrix 129) (centerSaturation 128) = true := by decide +kernel
private theorem second_09 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 129) (fun i => matrices 130 i j) (fun i => matrices 130 i k) (fun i => hessian 129 i j k) (fun i => hessian 130 i j k) = true := by decide +kernel
private theorem center_09 : matrixCheck LockedData.coefficients (centerMatrix 129) (centerMatrix 130) (centerSaturation 129) = true := by decide +kernel
theorem second : ∀ j : Fin 10, let n := 120+j.val; ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation n) (fun i => matrices (n+1) i j) (fun i => matrices (n+1) i k) (fun i => hessian n i j k) (fun i => hessian (n+1) i j k) = true := by
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
theorem center : ∀ j : Fin 10, let n := 120+j.val; matrixCheck LockedData.coefficients (centerMatrix n) (centerMatrix (n+1)) (centerSaturation n) = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck12
