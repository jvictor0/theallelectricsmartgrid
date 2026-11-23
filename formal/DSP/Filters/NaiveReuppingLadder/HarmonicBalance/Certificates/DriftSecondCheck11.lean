import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck11
open Enclosure DriftBlockCellData DriftSecondCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem second_00 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 110) (fun i => matrices 111 i j) (fun i => matrices 111 i k) (fun i => hessian 110 i j k) (fun i => hessian 111 i j k) = true := by decide +kernel
private theorem center_00 : matrixCheck LockedData.coefficients (centerMatrix 110) (centerMatrix 111) (centerSaturation 110) = true := by decide +kernel
private theorem second_01 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 111) (fun i => matrices 112 i j) (fun i => matrices 112 i k) (fun i => hessian 111 i j k) (fun i => hessian 112 i j k) = true := by decide +kernel
private theorem center_01 : matrixCheck LockedData.coefficients (centerMatrix 111) (centerMatrix 112) (centerSaturation 111) = true := by decide +kernel
private theorem second_02 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 112) (fun i => matrices 113 i j) (fun i => matrices 113 i k) (fun i => hessian 112 i j k) (fun i => hessian 113 i j k) = true := by decide +kernel
private theorem center_02 : matrixCheck LockedData.coefficients (centerMatrix 112) (centerMatrix 113) (centerSaturation 112) = true := by decide +kernel
private theorem second_03 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 113) (fun i => matrices 114 i j) (fun i => matrices 114 i k) (fun i => hessian 113 i j k) (fun i => hessian 114 i j k) = true := by decide +kernel
private theorem center_03 : matrixCheck LockedData.coefficients (centerMatrix 113) (centerMatrix 114) (centerSaturation 113) = true := by decide +kernel
private theorem second_04 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 114) (fun i => matrices 115 i j) (fun i => matrices 115 i k) (fun i => hessian 114 i j k) (fun i => hessian 115 i j k) = true := by decide +kernel
private theorem center_04 : matrixCheck LockedData.coefficients (centerMatrix 114) (centerMatrix 115) (centerSaturation 114) = true := by decide +kernel
private theorem second_05 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 115) (fun i => matrices 116 i j) (fun i => matrices 116 i k) (fun i => hessian 115 i j k) (fun i => hessian 116 i j k) = true := by decide +kernel
private theorem center_05 : matrixCheck LockedData.coefficients (centerMatrix 115) (centerMatrix 116) (centerSaturation 115) = true := by decide +kernel
private theorem second_06 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 116) (fun i => matrices 117 i j) (fun i => matrices 117 i k) (fun i => hessian 116 i j k) (fun i => hessian 117 i j k) = true := by decide +kernel
private theorem center_06 : matrixCheck LockedData.coefficients (centerMatrix 116) (centerMatrix 117) (centerSaturation 116) = true := by decide +kernel
private theorem second_07 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 117) (fun i => matrices 118 i j) (fun i => matrices 118 i k) (fun i => hessian 117 i j k) (fun i => hessian 118 i j k) = true := by decide +kernel
private theorem center_07 : matrixCheck LockedData.coefficients (centerMatrix 117) (centerMatrix 118) (centerSaturation 117) = true := by decide +kernel
private theorem second_08 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 118) (fun i => matrices 119 i j) (fun i => matrices 119 i k) (fun i => hessian 118 i j k) (fun i => hessian 119 i j k) = true := by decide +kernel
private theorem center_08 : matrixCheck LockedData.coefficients (centerMatrix 118) (centerMatrix 119) (centerSaturation 118) = true := by decide +kernel
private theorem second_09 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 119) (fun i => matrices 120 i j) (fun i => matrices 120 i k) (fun i => hessian 119 i j k) (fun i => hessian 120 i j k) = true := by decide +kernel
private theorem center_09 : matrixCheck LockedData.coefficients (centerMatrix 119) (centerMatrix 120) (centerSaturation 119) = true := by decide +kernel
theorem second : ∀ j : Fin 10, let n := 110+j.val; ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation n) (fun i => matrices (n+1) i j) (fun i => matrices (n+1) i k) (fun i => hessian n i j k) (fun i => hessian (n+1) i j k) = true := by
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
theorem center : ∀ j : Fin 10, let n := 110+j.val; matrixCheck LockedData.coefficients (centerMatrix n) (centerMatrix (n+1)) (centerSaturation n) = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck11
