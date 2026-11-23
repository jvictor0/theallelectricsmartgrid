import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck10
open Enclosure DriftBlockCellData DriftSecondCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem second_00 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 100) (fun i => matrices 101 i j) (fun i => matrices 101 i k) (fun i => hessian 100 i j k) (fun i => hessian 101 i j k) = true := by decide +kernel
private theorem center_00 : matrixCheck LockedData.coefficients (centerMatrix 100) (centerMatrix 101) (centerSaturation 100) = true := by decide +kernel
private theorem second_01 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 101) (fun i => matrices 102 i j) (fun i => matrices 102 i k) (fun i => hessian 101 i j k) (fun i => hessian 102 i j k) = true := by decide +kernel
private theorem center_01 : matrixCheck LockedData.coefficients (centerMatrix 101) (centerMatrix 102) (centerSaturation 101) = true := by decide +kernel
private theorem second_02 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 102) (fun i => matrices 103 i j) (fun i => matrices 103 i k) (fun i => hessian 102 i j k) (fun i => hessian 103 i j k) = true := by decide +kernel
private theorem center_02 : matrixCheck LockedData.coefficients (centerMatrix 102) (centerMatrix 103) (centerSaturation 102) = true := by decide +kernel
private theorem second_03 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 103) (fun i => matrices 104 i j) (fun i => matrices 104 i k) (fun i => hessian 103 i j k) (fun i => hessian 104 i j k) = true := by decide +kernel
private theorem center_03 : matrixCheck LockedData.coefficients (centerMatrix 103) (centerMatrix 104) (centerSaturation 103) = true := by decide +kernel
private theorem second_04 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 104) (fun i => matrices 105 i j) (fun i => matrices 105 i k) (fun i => hessian 104 i j k) (fun i => hessian 105 i j k) = true := by decide +kernel
private theorem center_04 : matrixCheck LockedData.coefficients (centerMatrix 104) (centerMatrix 105) (centerSaturation 104) = true := by decide +kernel
private theorem second_05 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 105) (fun i => matrices 106 i j) (fun i => matrices 106 i k) (fun i => hessian 105 i j k) (fun i => hessian 106 i j k) = true := by decide +kernel
private theorem center_05 : matrixCheck LockedData.coefficients (centerMatrix 105) (centerMatrix 106) (centerSaturation 105) = true := by decide +kernel
private theorem second_06 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 106) (fun i => matrices 107 i j) (fun i => matrices 107 i k) (fun i => hessian 106 i j k) (fun i => hessian 107 i j k) = true := by decide +kernel
private theorem center_06 : matrixCheck LockedData.coefficients (centerMatrix 106) (centerMatrix 107) (centerSaturation 106) = true := by decide +kernel
private theorem second_07 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 107) (fun i => matrices 108 i j) (fun i => matrices 108 i k) (fun i => hessian 107 i j k) (fun i => hessian 108 i j k) = true := by decide +kernel
private theorem center_07 : matrixCheck LockedData.coefficients (centerMatrix 107) (centerMatrix 108) (centerSaturation 107) = true := by decide +kernel
private theorem second_08 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 108) (fun i => matrices 109 i j) (fun i => matrices 109 i k) (fun i => hessian 108 i j k) (fun i => hessian 109 i j k) = true := by decide +kernel
private theorem center_08 : matrixCheck LockedData.coefficients (centerMatrix 108) (centerMatrix 109) (centerSaturation 108) = true := by decide +kernel
private theorem second_09 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 109) (fun i => matrices 110 i j) (fun i => matrices 110 i k) (fun i => hessian 109 i j k) (fun i => hessian 110 i j k) = true := by decide +kernel
private theorem center_09 : matrixCheck LockedData.coefficients (centerMatrix 109) (centerMatrix 110) (centerSaturation 109) = true := by decide +kernel
theorem second : ∀ j : Fin 10, let n := 100+j.val; ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation n) (fun i => matrices (n+1) i j) (fun i => matrices (n+1) i k) (fun i => hessian n i j k) (fun i => hessian (n+1) i j k) = true := by
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
theorem center : ∀ j : Fin 10, let n := 100+j.val; matrixCheck LockedData.coefficients (centerMatrix n) (centerMatrix (n+1)) (centerSaturation n) = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck10
