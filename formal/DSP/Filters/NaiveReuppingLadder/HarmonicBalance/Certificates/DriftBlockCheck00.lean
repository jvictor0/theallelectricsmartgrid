import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck00
open Enclosure DriftBlockCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem step_00 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 0) (center 0) (center 1) (centerSaturation 0) (whole 1) (DriftBlockCellData.saturation 0) (matrices 0) (matrices 1) width 24 = true := by decide +kernel
private theorem step_01 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 1) (center 1) (center 2) (centerSaturation 1) (whole 2) (DriftBlockCellData.saturation 1) (matrices 1) (matrices 2) width 24 = true := by decide +kernel
private theorem step_02 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 2) (center 2) (center 3) (centerSaturation 2) (whole 3) (DriftBlockCellData.saturation 2) (matrices 2) (matrices 3) width 24 = true := by decide +kernel
private theorem step_03 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 3) (center 3) (center 4) (centerSaturation 3) (whole 4) (DriftBlockCellData.saturation 3) (matrices 3) (matrices 4) width 24 = true := by decide +kernel
private theorem step_04 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 4) (center 4) (center 5) (centerSaturation 4) (whole 5) (DriftBlockCellData.saturation 4) (matrices 4) (matrices 5) width 24 = true := by decide +kernel
private theorem step_05 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 5) (center 5) (center 6) (centerSaturation 5) (whole 6) (DriftBlockCellData.saturation 5) (matrices 5) (matrices 6) width 24 = true := by decide +kernel
private theorem step_06 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 6) (center 6) (center 7) (centerSaturation 6) (whole 7) (DriftBlockCellData.saturation 6) (matrices 6) (matrices 7) width 24 = true := by decide +kernel
private theorem step_07 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 7) (center 7) (center 8) (centerSaturation 7) (whole 8) (DriftBlockCellData.saturation 7) (matrices 7) (matrices 8) width 24 = true := by decide +kernel
private theorem step_08 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 8) (center 8) (center 9) (centerSaturation 8) (whole 9) (DriftBlockCellData.saturation 8) (matrices 8) (matrices 9) width 24 = true := by decide +kernel
private theorem step_09 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 9) (center 9) (center 10) (centerSaturation 9) (whole 10) (DriftBlockCellData.saturation 9) (matrices 9) (matrices 10) width 24 = true := by decide +kernel
theorem steps : ∀ j : Fin 10, let n := 0+j.val; centeredCheck LockedData.coefficients (DriftBlockCellData.input n) (center n) (center (n+1)) (centerSaturation n) (whole (n+1)) (DriftBlockCellData.saturation n) (matrices n) (matrices (n+1)) width 24 = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck00
