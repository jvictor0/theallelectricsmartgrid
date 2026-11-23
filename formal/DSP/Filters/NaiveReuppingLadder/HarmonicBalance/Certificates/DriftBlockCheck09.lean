import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck09
open Enclosure DriftBlockCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem step_00 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 90) (center 90) (center 91) (centerSaturation 90) (whole 91) (DriftBlockCellData.saturation 90) (matrices 90) (matrices 91) width 24 = true := by decide +kernel
private theorem step_01 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 91) (center 91) (center 92) (centerSaturation 91) (whole 92) (DriftBlockCellData.saturation 91) (matrices 91) (matrices 92) width 24 = true := by decide +kernel
private theorem step_02 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 92) (center 92) (center 93) (centerSaturation 92) (whole 93) (DriftBlockCellData.saturation 92) (matrices 92) (matrices 93) width 24 = true := by decide +kernel
private theorem step_03 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 93) (center 93) (center 94) (centerSaturation 93) (whole 94) (DriftBlockCellData.saturation 93) (matrices 93) (matrices 94) width 24 = true := by decide +kernel
private theorem step_04 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 94) (center 94) (center 95) (centerSaturation 94) (whole 95) (DriftBlockCellData.saturation 94) (matrices 94) (matrices 95) width 24 = true := by decide +kernel
private theorem step_05 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 95) (center 95) (center 96) (centerSaturation 95) (whole 96) (DriftBlockCellData.saturation 95) (matrices 95) (matrices 96) width 24 = true := by decide +kernel
private theorem step_06 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 96) (center 96) (center 97) (centerSaturation 96) (whole 97) (DriftBlockCellData.saturation 96) (matrices 96) (matrices 97) width 24 = true := by decide +kernel
private theorem step_07 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 97) (center 97) (center 98) (centerSaturation 97) (whole 98) (DriftBlockCellData.saturation 97) (matrices 97) (matrices 98) width 24 = true := by decide +kernel
private theorem step_08 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 98) (center 98) (center 99) (centerSaturation 98) (whole 99) (DriftBlockCellData.saturation 98) (matrices 98) (matrices 99) width 24 = true := by decide +kernel
private theorem step_09 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 99) (center 99) (center 100) (centerSaturation 99) (whole 100) (DriftBlockCellData.saturation 99) (matrices 99) (matrices 100) width 24 = true := by decide +kernel
theorem steps : ∀ j : Fin 10, let n := 90+j.val; centeredCheck LockedData.coefficients (DriftBlockCellData.input n) (center n) (center (n+1)) (centerSaturation n) (whole (n+1)) (DriftBlockCellData.saturation n) (matrices n) (matrices (n+1)) width 24 = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck09
