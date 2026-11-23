import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck06
open Enclosure DriftBlockCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem step_00 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 60) (center 60) (center 61) (centerSaturation 60) (whole 61) (DriftBlockCellData.saturation 60) (matrices 60) (matrices 61) width 24 = true := by decide +kernel
private theorem step_01 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 61) (center 61) (center 62) (centerSaturation 61) (whole 62) (DriftBlockCellData.saturation 61) (matrices 61) (matrices 62) width 24 = true := by decide +kernel
private theorem step_02 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 62) (center 62) (center 63) (centerSaturation 62) (whole 63) (DriftBlockCellData.saturation 62) (matrices 62) (matrices 63) width 24 = true := by decide +kernel
private theorem step_03 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 63) (center 63) (center 64) (centerSaturation 63) (whole 64) (DriftBlockCellData.saturation 63) (matrices 63) (matrices 64) width 24 = true := by decide +kernel
private theorem step_04 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 64) (center 64) (center 65) (centerSaturation 64) (whole 65) (DriftBlockCellData.saturation 64) (matrices 64) (matrices 65) width 24 = true := by decide +kernel
private theorem step_05 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 65) (center 65) (center 66) (centerSaturation 65) (whole 66) (DriftBlockCellData.saturation 65) (matrices 65) (matrices 66) width 24 = true := by decide +kernel
private theorem step_06 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 66) (center 66) (center 67) (centerSaturation 66) (whole 67) (DriftBlockCellData.saturation 66) (matrices 66) (matrices 67) width 24 = true := by decide +kernel
private theorem step_07 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 67) (center 67) (center 68) (centerSaturation 67) (whole 68) (DriftBlockCellData.saturation 67) (matrices 67) (matrices 68) width 24 = true := by decide +kernel
private theorem step_08 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 68) (center 68) (center 69) (centerSaturation 68) (whole 69) (DriftBlockCellData.saturation 68) (matrices 68) (matrices 69) width 24 = true := by decide +kernel
private theorem step_09 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 69) (center 69) (center 70) (centerSaturation 69) (whole 70) (DriftBlockCellData.saturation 69) (matrices 69) (matrices 70) width 24 = true := by decide +kernel
theorem steps : ∀ j : Fin 10, let n := 60+j.val; centeredCheck LockedData.coefficients (DriftBlockCellData.input n) (center n) (center (n+1)) (centerSaturation n) (whole (n+1)) (DriftBlockCellData.saturation n) (matrices n) (matrices (n+1)) width 24 = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck06
