import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck14
open Enclosure DriftBlockCellData DriftSecondCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem second_00 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 140) (fun i => matrices 141 i j) (fun i => matrices 141 i k) (fun i => hessian 140 i j k) (fun i => hessian 141 i j k) = true := by decide +kernel
private theorem center_00 : matrixCheck LockedData.coefficients (centerMatrix 140) (centerMatrix 141) (centerSaturation 140) = true := by decide +kernel
private theorem second_01 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 141) (fun i => matrices 142 i j) (fun i => matrices 142 i k) (fun i => hessian 141 i j k) (fun i => hessian 142 i j k) = true := by decide +kernel
private theorem center_01 : matrixCheck LockedData.coefficients (centerMatrix 141) (centerMatrix 142) (centerSaturation 141) = true := by decide +kernel
private theorem second_02 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 142) (fun i => matrices 143 i j) (fun i => matrices 143 i k) (fun i => hessian 142 i j k) (fun i => hessian 143 i j k) = true := by decide +kernel
private theorem center_02 : matrixCheck LockedData.coefficients (centerMatrix 142) (centerMatrix 143) (centerSaturation 142) = true := by decide +kernel
private theorem second_03 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 143) (fun i => matrices 144 i j) (fun i => matrices 144 i k) (fun i => hessian 143 i j k) (fun i => hessian 144 i j k) = true := by decide +kernel
private theorem center_03 : matrixCheck LockedData.coefficients (centerMatrix 143) (centerMatrix 144) (centerSaturation 143) = true := by decide +kernel
private theorem second_04 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 144) (fun i => matrices 145 i j) (fun i => matrices 145 i k) (fun i => hessian 144 i j k) (fun i => hessian 145 i j k) = true := by decide +kernel
private theorem center_04 : matrixCheck LockedData.coefficients (centerMatrix 144) (centerMatrix 145) (centerSaturation 144) = true := by decide +kernel
private theorem second_05 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 145) (fun i => matrices 146 i j) (fun i => matrices 146 i k) (fun i => hessian 145 i j k) (fun i => hessian 146 i j k) = true := by decide +kernel
private theorem center_05 : matrixCheck LockedData.coefficients (centerMatrix 145) (centerMatrix 146) (centerSaturation 145) = true := by decide +kernel
private theorem second_06 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 146) (fun i => matrices 147 i j) (fun i => matrices 147 i k) (fun i => hessian 146 i j k) (fun i => hessian 147 i j k) = true := by decide +kernel
private theorem center_06 : matrixCheck LockedData.coefficients (centerMatrix 146) (centerMatrix 147) (centerSaturation 146) = true := by decide +kernel
private theorem second_07 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 147) (fun i => matrices 148 i j) (fun i => matrices 148 i k) (fun i => hessian 147 i j k) (fun i => hessian 148 i j k) = true := by decide +kernel
private theorem center_07 : matrixCheck LockedData.coefficients (centerMatrix 147) (centerMatrix 148) (centerSaturation 147) = true := by decide +kernel
private theorem second_08 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 148) (fun i => matrices 149 i j) (fun i => matrices 149 i k) (fun i => hessian 148 i j k) (fun i => hessian 149 i j k) = true := by decide +kernel
private theorem center_08 : matrixCheck LockedData.coefficients (centerMatrix 148) (centerMatrix 149) (centerSaturation 148) = true := by decide +kernel
private theorem second_09 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 149) (fun i => matrices 150 i j) (fun i => matrices 150 i k) (fun i => hessian 149 i j k) (fun i => hessian 150 i j k) = true := by decide +kernel
private theorem center_09 : matrixCheck LockedData.coefficients (centerMatrix 149) (centerMatrix 150) (centerSaturation 149) = true := by decide +kernel
theorem second : ∀ j : Fin 10, let n := 140+j.val; ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation n) (fun i => matrices (n+1) i j) (fun i => matrices (n+1) i k) (fun i => hessian n i j k) (fun i => hessian (n+1) i j k) = true := by
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
theorem center : ∀ j : Fin 10, let n := 140+j.val; matrixCheck LockedData.coefficients (centerMatrix n) (centerMatrix (n+1)) (centerSaturation n) = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck14
