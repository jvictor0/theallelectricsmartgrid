import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck13
open Enclosure DriftBlockCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem step_00 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 130) (center 130) (center 131) (centerSaturation 130) (whole 131) (DriftBlockCellData.saturation 130) (matrices 130) (matrices 131) width 24 = true := by decide +kernel
private theorem step_01 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 131) (center 131) (center 132) (centerSaturation 131) (whole 132) (DriftBlockCellData.saturation 131) (matrices 131) (matrices 132) width 24 = true := by decide +kernel
private theorem step_02 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 132) (center 132) (center 133) (centerSaturation 132) (whole 133) (DriftBlockCellData.saturation 132) (matrices 132) (matrices 133) width 24 = true := by decide +kernel
private theorem step_03 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 133) (center 133) (center 134) (centerSaturation 133) (whole 134) (DriftBlockCellData.saturation 133) (matrices 133) (matrices 134) width 24 = true := by decide +kernel
private theorem step_04 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 134) (center 134) (center 135) (centerSaturation 134) (whole 135) (DriftBlockCellData.saturation 134) (matrices 134) (matrices 135) width 24 = true := by decide +kernel
private theorem step_05 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 135) (center 135) (center 136) (centerSaturation 135) (whole 136) (DriftBlockCellData.saturation 135) (matrices 135) (matrices 136) width 24 = true := by decide +kernel
private theorem step_06 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 136) (center 136) (center 137) (centerSaturation 136) (whole 137) (DriftBlockCellData.saturation 136) (matrices 136) (matrices 137) width 24 = true := by decide +kernel
private theorem step_07 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 137) (center 137) (center 138) (centerSaturation 137) (whole 138) (DriftBlockCellData.saturation 137) (matrices 137) (matrices 138) width 24 = true := by decide +kernel
private theorem step_08 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 138) (center 138) (center 139) (centerSaturation 138) (whole 139) (DriftBlockCellData.saturation 138) (matrices 138) (matrices 139) width 24 = true := by decide +kernel
private theorem step_09 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 139) (center 139) (center 140) (centerSaturation 139) (whole 140) (DriftBlockCellData.saturation 139) (matrices 139) (matrices 140) width 24 = true := by decide +kernel
theorem steps : ∀ j : Fin 10, let n := 130+j.val; centeredCheck LockedData.coefficients (DriftBlockCellData.input n) (center n) (center (n+1)) (centerSaturation n) (whole (n+1)) (DriftBlockCellData.saturation n) (matrices n) (matrices (n+1)) width 24 = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck13
