import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck02
open Enclosure DriftBlockCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem step_00 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 20) (center 20) (center 21) (centerSaturation 20) (whole 21) (DriftBlockCellData.saturation 20) (matrices 20) (matrices 21) width 24 = true := by decide +kernel
private theorem step_01 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 21) (center 21) (center 22) (centerSaturation 21) (whole 22) (DriftBlockCellData.saturation 21) (matrices 21) (matrices 22) width 24 = true := by decide +kernel
private theorem step_02 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 22) (center 22) (center 23) (centerSaturation 22) (whole 23) (DriftBlockCellData.saturation 22) (matrices 22) (matrices 23) width 24 = true := by decide +kernel
private theorem step_03 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 23) (center 23) (center 24) (centerSaturation 23) (whole 24) (DriftBlockCellData.saturation 23) (matrices 23) (matrices 24) width 24 = true := by decide +kernel
private theorem step_04 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 24) (center 24) (center 25) (centerSaturation 24) (whole 25) (DriftBlockCellData.saturation 24) (matrices 24) (matrices 25) width 24 = true := by decide +kernel
private theorem step_05 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 25) (center 25) (center 26) (centerSaturation 25) (whole 26) (DriftBlockCellData.saturation 25) (matrices 25) (matrices 26) width 24 = true := by decide +kernel
private theorem step_06 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 26) (center 26) (center 27) (centerSaturation 26) (whole 27) (DriftBlockCellData.saturation 26) (matrices 26) (matrices 27) width 24 = true := by decide +kernel
private theorem step_07 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 27) (center 27) (center 28) (centerSaturation 27) (whole 28) (DriftBlockCellData.saturation 27) (matrices 27) (matrices 28) width 24 = true := by decide +kernel
private theorem step_08 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 28) (center 28) (center 29) (centerSaturation 28) (whole 29) (DriftBlockCellData.saturation 28) (matrices 28) (matrices 29) width 24 = true := by decide +kernel
private theorem step_09 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 29) (center 29) (center 30) (centerSaturation 29) (whole 30) (DriftBlockCellData.saturation 29) (matrices 29) (matrices 30) width 24 = true := by decide +kernel
theorem steps : ∀ j : Fin 10, let n := 20+j.val; centeredCheck LockedData.coefficients (DriftBlockCellData.input n) (center n) (center (n+1)) (centerSaturation n) (whole (n+1)) (DriftBlockCellData.saturation n) (matrices n) (matrices (n+1)) width 24 = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck02
