import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck05
open Enclosure DriftBlockCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem step_00 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 50) (center 50) (center 51) (centerSaturation 50) (whole 51) (DriftBlockCellData.saturation 50) (matrices 50) (matrices 51) width 24 = true := by decide +kernel
private theorem step_01 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 51) (center 51) (center 52) (centerSaturation 51) (whole 52) (DriftBlockCellData.saturation 51) (matrices 51) (matrices 52) width 24 = true := by decide +kernel
private theorem step_02 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 52) (center 52) (center 53) (centerSaturation 52) (whole 53) (DriftBlockCellData.saturation 52) (matrices 52) (matrices 53) width 24 = true := by decide +kernel
private theorem step_03 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 53) (center 53) (center 54) (centerSaturation 53) (whole 54) (DriftBlockCellData.saturation 53) (matrices 53) (matrices 54) width 24 = true := by decide +kernel
private theorem step_04 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 54) (center 54) (center 55) (centerSaturation 54) (whole 55) (DriftBlockCellData.saturation 54) (matrices 54) (matrices 55) width 24 = true := by decide +kernel
private theorem step_05 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 55) (center 55) (center 56) (centerSaturation 55) (whole 56) (DriftBlockCellData.saturation 55) (matrices 55) (matrices 56) width 24 = true := by decide +kernel
private theorem step_06 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 56) (center 56) (center 57) (centerSaturation 56) (whole 57) (DriftBlockCellData.saturation 56) (matrices 56) (matrices 57) width 24 = true := by decide +kernel
private theorem step_07 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 57) (center 57) (center 58) (centerSaturation 57) (whole 58) (DriftBlockCellData.saturation 57) (matrices 57) (matrices 58) width 24 = true := by decide +kernel
private theorem step_08 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 58) (center 58) (center 59) (centerSaturation 58) (whole 59) (DriftBlockCellData.saturation 58) (matrices 58) (matrices 59) width 24 = true := by decide +kernel
private theorem step_09 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 59) (center 59) (center 60) (centerSaturation 59) (whole 60) (DriftBlockCellData.saturation 59) (matrices 59) (matrices 60) width 24 = true := by decide +kernel
theorem steps : ∀ j : Fin 10, let n := 50+j.val; centeredCheck LockedData.coefficients (DriftBlockCellData.input n) (center n) (center (n+1)) (centerSaturation n) (whole (n+1)) (DriftBlockCellData.saturation n) (matrices n) (matrices (n+1)) width 24 = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck05
