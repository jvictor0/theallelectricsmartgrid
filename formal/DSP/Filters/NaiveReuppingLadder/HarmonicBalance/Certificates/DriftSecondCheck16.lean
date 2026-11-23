import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck16
open Enclosure DriftBlockCellData DriftSecondCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem second_00 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 160) (fun i => matrices 161 i j) (fun i => matrices 161 i k) (fun i => hessian 160 i j k) (fun i => hessian 161 i j k) = true := by decide +kernel
private theorem center_00 : matrixCheck LockedData.coefficients (centerMatrix 160) (centerMatrix 161) (centerSaturation 160) = true := by decide +kernel
private theorem second_01 : ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation 161) (fun i => matrices 162 i j) (fun i => matrices 162 i k) (fun i => hessian 161 i j k) (fun i => hessian 162 i j k) = true := by decide +kernel
private theorem center_01 : matrixCheck LockedData.coefficients (centerMatrix 161) (centerMatrix 162) (centerSaturation 161) = true := by decide +kernel
theorem second : ∀ j : Fin 2, let n := 160+j.val; ∀ j k : Fin 4, mixedCheck LockedData.coefficients (DriftBlockCellData.saturation n) (fun i => matrices (n+1) i j) (fun i => matrices (n+1) i k) (fun i => hessian n i j k) (fun i => hessian (n+1) i j k) = true := by
  intro j
  fin_cases j
  · exact second_00
  · exact second_01
theorem center : ∀ j : Fin 2, let n := 160+j.val; matrixCheck LockedData.coefficients (centerMatrix n) (centerMatrix (n+1)) (centerSaturation n) = true := by
  intro j
  fin_cases j
  · exact center_00
  · exact center_01
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftSecondCheck16
