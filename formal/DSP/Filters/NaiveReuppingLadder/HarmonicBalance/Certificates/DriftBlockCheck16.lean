import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCellData
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck16
open Enclosure DriftBlockCellData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem step_00 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 160) (center 160) (center 161) (centerSaturation 160) (whole 161) (DriftBlockCellData.saturation 160) (matrices 160) (matrices 161) width 24 = true := by decide +kernel
private theorem step_01 : centeredCheck LockedData.coefficients (DriftBlockCellData.input 161) (center 161) (center 162) (centerSaturation 161) (whole 162) (DriftBlockCellData.saturation 161) (matrices 161) (matrices 162) width 24 = true := by decide +kernel
theorem steps : ∀ j : Fin 2, let n := 160+j.val; centeredCheck LockedData.coefficients (DriftBlockCellData.input n) (center n) (center (n+1)) (centerSaturation n) (whole (n+1)) (DriftBlockCellData.saturation n) (matrices n) (matrices (n+1)) width 24 = true := by
  intro j
  fin_cases j
  · exact step_00
  · exact step_01
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck16
