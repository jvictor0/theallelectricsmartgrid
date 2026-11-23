import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck07
open Enclosure DriftBlockCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem step_00 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 70) (center 70) (center 71) (centerSaturation 70) (whole 71) (DriftBlockCellData.saturation 70) (matrices 70) (matrices 71) width 24 = true := by decide +kernel
private theorem step_01 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 71) (center 71) (center 72) (centerSaturation 71) (whole 72) (DriftBlockCellData.saturation 71) (matrices 71) (matrices 72) width 24 = true := by decide +kernel
private theorem step_02 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 72) (center 72) (center 73) (centerSaturation 72) (whole 73) (DriftBlockCellData.saturation 72) (matrices 72) (matrices 73) width 24 = true := by decide +kernel
private theorem step_03 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 73) (center 73) (center 74) (centerSaturation 73) (whole 74) (DriftBlockCellData.saturation 73) (matrices 73) (matrices 74) width 24 = true := by decide +kernel
private theorem step_04 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 74) (center 74) (center 75) (centerSaturation 74) (whole 75) (DriftBlockCellData.saturation 74) (matrices 74) (matrices 75) width 24 = true := by decide +kernel
private theorem step_05 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 75) (center 75) (center 76) (centerSaturation 75) (whole 76) (DriftBlockCellData.saturation 75) (matrices 75) (matrices 76) width 24 = true := by decide +kernel
private theorem step_06 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 76) (center 76) (center 77) (centerSaturation 76) (whole 77) (DriftBlockCellData.saturation 76) (matrices 76) (matrices 77) width 24 = true := by decide +kernel
private theorem step_07 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 77) (center 77) (center 78) (centerSaturation 77) (whole 78) (DriftBlockCellData.saturation 77) (matrices 77) (matrices 78) width 24 = true := by decide +kernel
private theorem step_08 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 78) (center 78) (center 79) (centerSaturation 78) (whole 79) (DriftBlockCellData.saturation 78) (matrices 78) (matrices 79) width 24 = true := by decide +kernel
private theorem step_09 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 79) (center 79) (center 80) (centerSaturation 79) (whole 80) (DriftBlockCellData.saturation 79) (matrices 79) (matrices 80) width 24 = true := by decide +kernel
theorem steps : ∀ j : Fin 10, let n := 70+j.val; centeredCheck LockedData.coefficients (DriftBlockCellData.input n) (center n) (center (n+1)) (centerSaturation n) (whole (n+1)) (DriftBlockCellData.saturation n) (matrices n) (matrices (n+1)) width 24 = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck07
