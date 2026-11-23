import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck08
open Enclosure DriftBlockCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem step_00 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 80) (center 80) (center 81) (centerSaturation 80) (whole 81) (DriftBlockCellData.saturation 80) (matrices 80) (matrices 81) width 24 = true := by decide +kernel
private theorem step_01 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 81) (center 81) (center 82) (centerSaturation 81) (whole 82) (DriftBlockCellData.saturation 81) (matrices 81) (matrices 82) width 24 = true := by decide +kernel
private theorem step_02 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 82) (center 82) (center 83) (centerSaturation 82) (whole 83) (DriftBlockCellData.saturation 82) (matrices 82) (matrices 83) width 24 = true := by decide +kernel
private theorem step_03 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 83) (center 83) (center 84) (centerSaturation 83) (whole 84) (DriftBlockCellData.saturation 83) (matrices 83) (matrices 84) width 24 = true := by decide +kernel
private theorem step_04 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 84) (center 84) (center 85) (centerSaturation 84) (whole 85) (DriftBlockCellData.saturation 84) (matrices 84) (matrices 85) width 24 = true := by decide +kernel
private theorem step_05 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 85) (center 85) (center 86) (centerSaturation 85) (whole 86) (DriftBlockCellData.saturation 85) (matrices 85) (matrices 86) width 24 = true := by decide +kernel
private theorem step_06 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 86) (center 86) (center 87) (centerSaturation 86) (whole 87) (DriftBlockCellData.saturation 86) (matrices 86) (matrices 87) width 24 = true := by decide +kernel
private theorem step_07 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 87) (center 87) (center 88) (centerSaturation 87) (whole 88) (DriftBlockCellData.saturation 87) (matrices 87) (matrices 88) width 24 = true := by decide +kernel
private theorem step_08 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 88) (center 88) (center 89) (centerSaturation 88) (whole 89) (DriftBlockCellData.saturation 88) (matrices 88) (matrices 89) width 24 = true := by decide +kernel
private theorem step_09 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 89) (center 89) (center 90) (centerSaturation 89) (whole 90) (DriftBlockCellData.saturation 89) (matrices 89) (matrices 90) width 24 = true := by decide +kernel
theorem steps : ∀ j : Fin 10, let n := 80+j.val; centeredCheck LockedData.coefficients (DriftBlockCellData.input n) (center n) (center (n+1)) (centerSaturation n) (whole (n+1)) (DriftBlockCellData.saturation n) (matrices n) (matrices (n+1)) width 24 = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck08
