import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck15
open Enclosure DriftBlockCellData DriftSecondCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem second_00 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 150) (fun i => matrices 151 i j) (fun i => matrices 151 i k) (fun i => hessian 150 i j k) (fun i => hessian 151 i j k) = true := by decide +kernel
private theorem center_00 : matrixCheck LockedData.coefficients (centerMatrix 150) (centerMatrix 151) (centerSaturation 150) = true := by decide +kernel
private theorem second_01 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 151) (fun i => matrices 152 i j) (fun i => matrices 152 i k) (fun i => hessian 151 i j k) (fun i => hessian 152 i j k) = true := by decide +kernel
private theorem center_01 : matrixCheck LockedData.coefficients (centerMatrix 151) (centerMatrix 152) (centerSaturation 151) = true := by decide +kernel
private theorem second_02 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 152) (fun i => matrices 153 i j) (fun i => matrices 153 i k) (fun i => hessian 152 i j k) (fun i => hessian 153 i j k) = true := by decide +kernel
private theorem center_02 : matrixCheck LockedData.coefficients (centerMatrix 152) (centerMatrix 153) (centerSaturation 152) = true := by decide +kernel
private theorem second_03 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 153) (fun i => matrices 154 i j) (fun i => matrices 154 i k) (fun i => hessian 153 i j k) (fun i => hessian 154 i j k) = true := by decide +kernel
private theorem center_03 : matrixCheck LockedData.coefficients (centerMatrix 153) (centerMatrix 154) (centerSaturation 153) = true := by decide +kernel
private theorem second_04 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 154) (fun i => matrices 155 i j) (fun i => matrices 155 i k) (fun i => hessian 154 i j k) (fun i => hessian 155 i j k) = true := by decide +kernel
private theorem center_04 : matrixCheck LockedData.coefficients (centerMatrix 154) (centerMatrix 155) (centerSaturation 154) = true := by decide +kernel
private theorem second_05 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 155) (fun i => matrices 156 i j) (fun i => matrices 156 i k) (fun i => hessian 155 i j k) (fun i => hessian 156 i j k) = true := by decide +kernel
private theorem center_05 : matrixCheck LockedData.coefficients (centerMatrix 155) (centerMatrix 156) (centerSaturation 155) = true := by decide +kernel
private theorem second_06 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 156) (fun i => matrices 157 i j) (fun i => matrices 157 i k) (fun i => hessian 156 i j k) (fun i => hessian 157 i j k) = true := by decide +kernel
private theorem center_06 : matrixCheck LockedData.coefficients (centerMatrix 156) (centerMatrix 157) (centerSaturation 156) = true := by decide +kernel
private theorem second_07 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 157) (fun i => matrices 158 i j) (fun i => matrices 158 i k) (fun i => hessian 157 i j k) (fun i => hessian 158 i j k) = true := by decide +kernel
private theorem center_07 : matrixCheck LockedData.coefficients (centerMatrix 157) (centerMatrix 158) (centerSaturation 157) = true := by decide +kernel
private theorem second_08 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 158) (fun i => matrices 159 i j) (fun i => matrices 159 i k) (fun i => hessian 158 i j k) (fun i => hessian 159 i j k) = true := by decide +kernel
private theorem center_08 : matrixCheck LockedData.coefficients (centerMatrix 158) (centerMatrix 159) (centerSaturation 158) = true := by decide +kernel
private theorem second_09 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 159) (fun i => matrices 160 i j) (fun i => matrices 160 i k) (fun i => hessian 159 i j k) (fun i => hessian 160 i j k) = true := by decide +kernel
private theorem center_09 : matrixCheck LockedData.coefficients (centerMatrix 159) (centerMatrix 160) (centerSaturation 159) = true := by decide +kernel
theorem second : ∀ j : Fin 10, let n := 150+j.val; ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation n) (fun i => matrices (n+1) i j) (fun i => matrices (n+1) i k) (fun i => hessian n i j k) (fun i => hessian (n+1) i j k) = true := by
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
theorem center : ∀ j : Fin 10, let n := 150+j.val; matrixCheck LockedData.coefficients (centerMatrix n) (centerMatrix (n+1)) (centerSaturation n) = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck15
