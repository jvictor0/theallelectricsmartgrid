import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck04
open Enclosure DriftBlockCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem step_00 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 40) (center 40) (center 41) (centerSaturation 40) (whole 41) (DriftBlockCellData.saturation 40) (matrices 40) (matrices 41) width 24 = true := by decide +kernel
private theorem step_01 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 41) (center 41) (center 42) (centerSaturation 41) (whole 42) (DriftBlockCellData.saturation 41) (matrices 41) (matrices 42) width 24 = true := by decide +kernel
private theorem step_02 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 42) (center 42) (center 43) (centerSaturation 42) (whole 43) (DriftBlockCellData.saturation 42) (matrices 42) (matrices 43) width 24 = true := by decide +kernel
private theorem step_03 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 43) (center 43) (center 44) (centerSaturation 43) (whole 44) (DriftBlockCellData.saturation 43) (matrices 43) (matrices 44) width 24 = true := by decide +kernel
private theorem step_04 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 44) (center 44) (center 45) (centerSaturation 44) (whole 45) (DriftBlockCellData.saturation 44) (matrices 44) (matrices 45) width 24 = true := by decide +kernel
private theorem step_05 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 45) (center 45) (center 46) (centerSaturation 45) (whole 46) (DriftBlockCellData.saturation 45) (matrices 45) (matrices 46) width 24 = true := by decide +kernel
private theorem step_06 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 46) (center 46) (center 47) (centerSaturation 46) (whole 47) (DriftBlockCellData.saturation 46) (matrices 46) (matrices 47) width 24 = true := by decide +kernel
private theorem step_07 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 47) (center 47) (center 48) (centerSaturation 47) (whole 48) (DriftBlockCellData.saturation 47) (matrices 47) (matrices 48) width 24 = true := by decide +kernel
private theorem step_08 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 48) (center 48) (center 49) (centerSaturation 48) (whole 49) (DriftBlockCellData.saturation 48) (matrices 48) (matrices 49) width 24 = true := by decide +kernel
private theorem step_09 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 49) (center 49) (center 50) (centerSaturation 49) (whole 50) (DriftBlockCellData.saturation 49) (matrices 49) (matrices 50) width 24 = true := by decide +kernel
theorem steps : ∀ j : Fin 10, let n := 40+j.val; centeredCheck LockedData.coefficients (DriftBlockCellData.input n) (center n) (center (n+1)) (centerSaturation n) (whole (n+1)) (DriftBlockCellData.saturation n) (matrices n) (matrices (n+1)) width 24 = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck04
