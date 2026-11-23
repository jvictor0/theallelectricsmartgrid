import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck12
open Enclosure DriftBlockCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem step_00 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 120) (center 120) (center 121) (centerSaturation 120) (whole 121) (DriftBlockCellData.saturation 120) (matrices 120) (matrices 121) width 24 = true := by decide +kernel
private theorem step_01 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 121) (center 121) (center 122) (centerSaturation 121) (whole 122) (DriftBlockCellData.saturation 121) (matrices 121) (matrices 122) width 24 = true := by decide +kernel
private theorem step_02 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 122) (center 122) (center 123) (centerSaturation 122) (whole 123) (DriftBlockCellData.saturation 122) (matrices 122) (matrices 123) width 24 = true := by decide +kernel
private theorem step_03 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 123) (center 123) (center 124) (centerSaturation 123) (whole 124) (DriftBlockCellData.saturation 123) (matrices 123) (matrices 124) width 24 = true := by decide +kernel
private theorem step_04 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 124) (center 124) (center 125) (centerSaturation 124) (whole 125) (DriftBlockCellData.saturation 124) (matrices 124) (matrices 125) width 24 = true := by decide +kernel
private theorem step_05 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 125) (center 125) (center 126) (centerSaturation 125) (whole 126) (DriftBlockCellData.saturation 125) (matrices 125) (matrices 126) width 24 = true := by decide +kernel
private theorem step_06 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 126) (center 126) (center 127) (centerSaturation 126) (whole 127) (DriftBlockCellData.saturation 126) (matrices 126) (matrices 127) width 24 = true := by decide +kernel
private theorem step_07 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 127) (center 127) (center 128) (centerSaturation 127) (whole 128) (DriftBlockCellData.saturation 127) (matrices 127) (matrices 128) width 24 = true := by decide +kernel
private theorem step_08 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 128) (center 128) (center 129) (centerSaturation 128) (whole 129) (DriftBlockCellData.saturation 128) (matrices 128) (matrices 129) width 24 = true := by decide +kernel
private theorem step_09 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 129) (center 129) (center 130) (centerSaturation 129) (whole 130) (DriftBlockCellData.saturation 129) (matrices 129) (matrices 130) width 24 = true := by decide +kernel
theorem steps : ∀ j : Fin 10, let n := 120+j.val; centeredCheck LockedData.coefficients (DriftBlockCellData.input n) (center n) (center (n+1)) (centerSaturation n) (whole (n+1)) (DriftBlockCellData.saturation n) (matrices n) (matrices (n+1)) width 24 = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck12
