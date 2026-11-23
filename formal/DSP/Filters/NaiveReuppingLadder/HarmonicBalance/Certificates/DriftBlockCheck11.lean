import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck11
open Enclosure DriftBlockCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem step_00 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 110) (center 110) (center 111) (centerSaturation 110) (whole 111) (DriftBlockCellData.saturation 110) (matrices 110) (matrices 111) width 24 = true := by decide +kernel
private theorem step_01 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 111) (center 111) (center 112) (centerSaturation 111) (whole 112) (DriftBlockCellData.saturation 111) (matrices 111) (matrices 112) width 24 = true := by decide +kernel
private theorem step_02 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 112) (center 112) (center 113) (centerSaturation 112) (whole 113) (DriftBlockCellData.saturation 112) (matrices 112) (matrices 113) width 24 = true := by decide +kernel
private theorem step_03 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 113) (center 113) (center 114) (centerSaturation 113) (whole 114) (DriftBlockCellData.saturation 113) (matrices 113) (matrices 114) width 24 = true := by decide +kernel
private theorem step_04 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 114) (center 114) (center 115) (centerSaturation 114) (whole 115) (DriftBlockCellData.saturation 114) (matrices 114) (matrices 115) width 24 = true := by decide +kernel
private theorem step_05 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 115) (center 115) (center 116) (centerSaturation 115) (whole 116) (DriftBlockCellData.saturation 115) (matrices 115) (matrices 116) width 24 = true := by decide +kernel
private theorem step_06 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 116) (center 116) (center 117) (centerSaturation 116) (whole 117) (DriftBlockCellData.saturation 116) (matrices 116) (matrices 117) width 24 = true := by decide +kernel
private theorem step_07 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 117) (center 117) (center 118) (centerSaturation 117) (whole 118) (DriftBlockCellData.saturation 117) (matrices 117) (matrices 118) width 24 = true := by decide +kernel
private theorem step_08 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 118) (center 118) (center 119) (centerSaturation 118) (whole 119) (DriftBlockCellData.saturation 118) (matrices 118) (matrices 119) width 24 = true := by decide +kernel
private theorem step_09 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 119) (center 119) (center 120) (centerSaturation 119) (whole 120) (DriftBlockCellData.saturation 119) (matrices 119) (matrices 120) width 24 = true := by decide +kernel
theorem steps : ∀ j : Fin 10, let n := 110+j.val; centeredCheck LockedData.coefficients (DriftBlockCellData.input n) (center n) (center (n+1)) (centerSaturation n) (whole (n+1)) (DriftBlockCellData.saturation n) (matrices n) (matrices (n+1)) width 24 = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck11
