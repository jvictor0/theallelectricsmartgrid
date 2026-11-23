import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck10
open Enclosure DriftBlockCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem step_00 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 100) (center 100) (center 101) (centerSaturation 100) (whole 101) (DriftBlockCellData.saturation 100) (matrices 100) (matrices 101) width 24 = true := by decide +kernel
private theorem step_01 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 101) (center 101) (center 102) (centerSaturation 101) (whole 102) (DriftBlockCellData.saturation 101) (matrices 101) (matrices 102) width 24 = true := by decide +kernel
private theorem step_02 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 102) (center 102) (center 103) (centerSaturation 102) (whole 103) (DriftBlockCellData.saturation 102) (matrices 102) (matrices 103) width 24 = true := by decide +kernel
private theorem step_03 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 103) (center 103) (center 104) (centerSaturation 103) (whole 104) (DriftBlockCellData.saturation 103) (matrices 103) (matrices 104) width 24 = true := by decide +kernel
private theorem step_04 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 104) (center 104) (center 105) (centerSaturation 104) (whole 105) (DriftBlockCellData.saturation 104) (matrices 104) (matrices 105) width 24 = true := by decide +kernel
private theorem step_05 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 105) (center 105) (center 106) (centerSaturation 105) (whole 106) (DriftBlockCellData.saturation 105) (matrices 105) (matrices 106) width 24 = true := by decide +kernel
private theorem step_06 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 106) (center 106) (center 107) (centerSaturation 106) (whole 107) (DriftBlockCellData.saturation 106) (matrices 106) (matrices 107) width 24 = true := by decide +kernel
private theorem step_07 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 107) (center 107) (center 108) (centerSaturation 107) (whole 108) (DriftBlockCellData.saturation 107) (matrices 107) (matrices 108) width 24 = true := by decide +kernel
private theorem step_08 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 108) (center 108) (center 109) (centerSaturation 108) (whole 109) (DriftBlockCellData.saturation 108) (matrices 108) (matrices 109) width 24 = true := by decide +kernel
private theorem step_09 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 109) (center 109) (center 110) (centerSaturation 109) (whole 110) (DriftBlockCellData.saturation 109) (matrices 109) (matrices 110) width 24 = true := by decide +kernel
theorem steps : ∀ j : Fin 10, let n := 100+j.val; centeredCheck LockedData.coefficients (DriftBlockCellData.input n) (center n) (center (n+1)) (centerSaturation n) (whole (n+1)) (DriftBlockCellData.saturation n) (matrices n) (matrices (n+1)) width 24 = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck10
