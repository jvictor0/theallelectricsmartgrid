import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck14
open Enclosure DriftBlockCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem step_00 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 140) (center 140) (center 141) (centerSaturation 140) (whole 141) (DriftBlockCellData.saturation 140) (matrices 140) (matrices 141) width 24 = true := by decide +kernel
private theorem step_01 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 141) (center 141) (center 142) (centerSaturation 141) (whole 142) (DriftBlockCellData.saturation 141) (matrices 141) (matrices 142) width 24 = true := by decide +kernel
private theorem step_02 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 142) (center 142) (center 143) (centerSaturation 142) (whole 143) (DriftBlockCellData.saturation 142) (matrices 142) (matrices 143) width 24 = true := by decide +kernel
private theorem step_03 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 143) (center 143) (center 144) (centerSaturation 143) (whole 144) (DriftBlockCellData.saturation 143) (matrices 143) (matrices 144) width 24 = true := by decide +kernel
private theorem step_04 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 144) (center 144) (center 145) (centerSaturation 144) (whole 145) (DriftBlockCellData.saturation 144) (matrices 144) (matrices 145) width 24 = true := by decide +kernel
private theorem step_05 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 145) (center 145) (center 146) (centerSaturation 145) (whole 146) (DriftBlockCellData.saturation 145) (matrices 145) (matrices 146) width 24 = true := by decide +kernel
private theorem step_06 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 146) (center 146) (center 147) (centerSaturation 146) (whole 147) (DriftBlockCellData.saturation 146) (matrices 146) (matrices 147) width 24 = true := by decide +kernel
private theorem step_07 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 147) (center 147) (center 148) (centerSaturation 147) (whole 148) (DriftBlockCellData.saturation 147) (matrices 147) (matrices 148) width 24 = true := by decide +kernel
private theorem step_08 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 148) (center 148) (center 149) (centerSaturation 148) (whole 149) (DriftBlockCellData.saturation 148) (matrices 148) (matrices 149) width 24 = true := by decide +kernel
private theorem step_09 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 149) (center 149) (center 150) (centerSaturation 149) (whole 150) (DriftBlockCellData.saturation 149) (matrices 149) (matrices 150) width 24 = true := by decide +kernel
theorem steps : ∀ j : Fin 10, let n := 140+j.val; centeredCheck LockedData.coefficients (DriftBlockCellData.input n) (center n) (center (n+1)) (centerSaturation n) (whole (n+1)) (DriftBlockCellData.saturation n) (matrices n) (matrices (n+1)) width 24 = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck14
