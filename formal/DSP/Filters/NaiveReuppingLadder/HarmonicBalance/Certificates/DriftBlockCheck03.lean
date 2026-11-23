import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck03
open Enclosure DriftBlockCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem step_00 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 30) (center 30) (center 31) (centerSaturation 30) (whole 31) (DriftBlockCellData.saturation 30) (matrices 30) (matrices 31) width 24 = true := by decide +kernel
private theorem step_01 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 31) (center 31) (center 32) (centerSaturation 31) (whole 32) (DriftBlockCellData.saturation 31) (matrices 31) (matrices 32) width 24 = true := by decide +kernel
private theorem step_02 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 32) (center 32) (center 33) (centerSaturation 32) (whole 33) (DriftBlockCellData.saturation 32) (matrices 32) (matrices 33) width 24 = true := by decide +kernel
private theorem step_03 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 33) (center 33) (center 34) (centerSaturation 33) (whole 34) (DriftBlockCellData.saturation 33) (matrices 33) (matrices 34) width 24 = true := by decide +kernel
private theorem step_04 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 34) (center 34) (center 35) (centerSaturation 34) (whole 35) (DriftBlockCellData.saturation 34) (matrices 34) (matrices 35) width 24 = true := by decide +kernel
private theorem step_05 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 35) (center 35) (center 36) (centerSaturation 35) (whole 36) (DriftBlockCellData.saturation 35) (matrices 35) (matrices 36) width 24 = true := by decide +kernel
private theorem step_06 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 36) (center 36) (center 37) (centerSaturation 36) (whole 37) (DriftBlockCellData.saturation 36) (matrices 36) (matrices 37) width 24 = true := by decide +kernel
private theorem step_07 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 37) (center 37) (center 38) (centerSaturation 37) (whole 38) (DriftBlockCellData.saturation 37) (matrices 37) (matrices 38) width 24 = true := by decide +kernel
private theorem step_08 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 38) (center 38) (center 39) (centerSaturation 38) (whole 39) (DriftBlockCellData.saturation 38) (matrices 38) (matrices 39) width 24 = true := by decide +kernel
private theorem step_09 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 39) (center 39) (center 40) (centerSaturation 39) (whole 40) (DriftBlockCellData.saturation 39) (matrices 39) (matrices 40) width 24 = true := by decide +kernel
theorem steps : ∀ j : Fin 10, let n := 30+j.val; centeredCheck LockedData.coefficients (DriftBlockCellData.input n) (center n) (center (n+1)) (centerSaturation n) (whole (n+1)) (DriftBlockCellData.saturation n) (matrices n) (matrices (n+1)) width 24 = true := by
  intro j
  fin_cases j
  · exact step_00
  · exact step_01
  · exact step_02
  · exact step_03
  · exact step_04
  · exact step_05
  · exact step_06
  · exact step_07
  · exact step_08
  · exact step_09
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck03
