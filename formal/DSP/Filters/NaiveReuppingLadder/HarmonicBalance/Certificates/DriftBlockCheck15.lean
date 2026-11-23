import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck15
open Enclosure DriftBlockCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem step_00 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 150) (center 150) (center 151) (centerSaturation 150) (whole 151) (DriftBlockCellData.saturation 150) (matrices 150) (matrices 151) width 24 = true := by decide +kernel
private theorem step_01 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 151) (center 151) (center 152) (centerSaturation 151) (whole 152) (DriftBlockCellData.saturation 151) (matrices 151) (matrices 152) width 24 = true := by decide +kernel
private theorem step_02 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 152) (center 152) (center 153) (centerSaturation 152) (whole 153) (DriftBlockCellData.saturation 152) (matrices 152) (matrices 153) width 24 = true := by decide +kernel
private theorem step_03 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 153) (center 153) (center 154) (centerSaturation 153) (whole 154) (DriftBlockCellData.saturation 153) (matrices 153) (matrices 154) width 24 = true := by decide +kernel
private theorem step_04 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 154) (center 154) (center 155) (centerSaturation 154) (whole 155) (DriftBlockCellData.saturation 154) (matrices 154) (matrices 155) width 24 = true := by decide +kernel
private theorem step_05 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 155) (center 155) (center 156) (centerSaturation 155) (whole 156) (DriftBlockCellData.saturation 155) (matrices 155) (matrices 156) width 24 = true := by decide +kernel
private theorem step_06 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 156) (center 156) (center 157) (centerSaturation 156) (whole 157) (DriftBlockCellData.saturation 156) (matrices 156) (matrices 157) width 24 = true := by decide +kernel
private theorem step_07 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 157) (center 157) (center 158) (centerSaturation 157) (whole 158) (DriftBlockCellData.saturation 157) (matrices 157) (matrices 158) width 24 = true := by decide +kernel
private theorem step_08 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 158) (center 158) (center 159) (centerSaturation 158) (whole 159) (DriftBlockCellData.saturation 158) (matrices 158) (matrices 159) width 24 = true := by decide +kernel
private theorem step_09 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 159) (center 159) (center 160) (centerSaturation 159) (whole 160) (DriftBlockCellData.saturation 159) (matrices 159) (matrices 160) width 24 = true := by decide +kernel
theorem steps : ∀ j : Fin 10, let n := 150+j.val; centeredCheck LockedData.coefficients (DriftBlockCellData.input n) (center n) (center (n+1)) (centerSaturation n) (whole (n+1)) (DriftBlockCellData.saturation n) (matrices n) (matrices (n+1)) width 24 = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck15
