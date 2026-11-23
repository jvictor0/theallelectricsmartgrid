import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck01
open Enclosure DriftBlockCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem step_00 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 10) (center 10) (center 11) (centerSaturation 10) (whole 11) (DriftBlockCellData.saturation 10) (matrices 10) (matrices 11) width 24 = true := by decide +kernel
private theorem step_01 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 11) (center 11) (center 12) (centerSaturation 11) (whole 12) (DriftBlockCellData.saturation 11) (matrices 11) (matrices 12) width 24 = true := by decide +kernel
private theorem step_02 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 12) (center 12) (center 13) (centerSaturation 12) (whole 13) (DriftBlockCellData.saturation 12) (matrices 12) (matrices 13) width 24 = true := by decide +kernel
private theorem step_03 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 13) (center 13) (center 14) (centerSaturation 13) (whole 14) (DriftBlockCellData.saturation 13) (matrices 13) (matrices 14) width 24 = true := by decide +kernel
private theorem step_04 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 14) (center 14) (center 15) (centerSaturation 14) (whole 15) (DriftBlockCellData.saturation 14) (matrices 14) (matrices 15) width 24 = true := by decide +kernel
private theorem step_05 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 15) (center 15) (center 16) (centerSaturation 15) (whole 16) (DriftBlockCellData.saturation 15) (matrices 15) (matrices 16) width 24 = true := by decide +kernel
private theorem step_06 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 16) (center 16) (center 17) (centerSaturation 16) (whole 17) (DriftBlockCellData.saturation 16) (matrices 16) (matrices 17) width 24 = true := by decide +kernel
private theorem step_07 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 17) (center 17) (center 18) (centerSaturation 17) (whole 18) (DriftBlockCellData.saturation 17) (matrices 17) (matrices 18) width 24 = true := by decide +kernel
private theorem step_08 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 18) (center 18) (center 19) (centerSaturation 18) (whole 19) (DriftBlockCellData.saturation 18) (matrices 18) (matrices 19) width 24 = true := by decide +kernel
private theorem step_09 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 19) (center 19) (center 20) (centerSaturation 19) (whole 20) (DriftBlockCellData.saturation 19) (matrices 19) (matrices 20) width 24 = true := by decide +kernel
theorem steps : ∀ j : Fin 10, let n := 10+j.val; centeredCheck LockedData.coefficients (DriftBlockCellData.input n) (center n) (center (n+1)) (centerSaturation n) (whole (n+1)) (DriftBlockCellData.saturation n) (matrices n) (matrices (n+1)) width 24 = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck01
