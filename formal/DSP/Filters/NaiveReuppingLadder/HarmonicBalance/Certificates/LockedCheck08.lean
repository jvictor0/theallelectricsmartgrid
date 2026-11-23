import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedData
/-! Generated kernel checks; no native_decide or imported numerical assumptions. -/
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck08
open Enclosure LockedData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem wholeValues_00 : valuesCheck coefficients (LockedData.input 80) (wholeHistory 80) (wholeHistory 81) (wholeSaturation 80) 24 = true := by decide +kernel
private theorem wholeValues_01 : valuesCheck coefficients (LockedData.input 81) (wholeHistory 81) (wholeHistory 82) (wholeSaturation 81) 24 = true := by decide +kernel
private theorem wholeValues_02 : valuesCheck coefficients (LockedData.input 82) (wholeHistory 82) (wholeHistory 83) (wholeSaturation 82) 24 = true := by decide +kernel
private theorem wholeValues_03 : valuesCheck coefficients (LockedData.input 83) (wholeHistory 83) (wholeHistory 84) (wholeSaturation 83) 24 = true := by decide +kernel
private theorem wholeValues_04 : valuesCheck coefficients (LockedData.input 84) (wholeHistory 84) (wholeHistory 85) (wholeSaturation 84) 24 = true := by decide +kernel
private theorem wholeValues_05 : valuesCheck coefficients (LockedData.input 85) (wholeHistory 85) (wholeHistory 86) (wholeSaturation 85) 24 = true := by decide +kernel
private theorem wholeValues_06 : valuesCheck coefficients (LockedData.input 86) (wholeHistory 86) (wholeHistory 87) (wholeSaturation 86) 24 = true := by decide +kernel
private theorem wholeValues_07 : valuesCheck coefficients (LockedData.input 87) (wholeHistory 87) (wholeHistory 88) (wholeSaturation 87) 24 = true := by decide +kernel
private theorem wholeValues_08 : valuesCheck coefficients (LockedData.input 88) (wholeHistory 88) (wholeHistory 89) (wholeSaturation 88) 24 = true := by decide +kernel
private theorem wholeValues_09 : valuesCheck coefficients (LockedData.input 89) (wholeHistory 89) (wholeHistory 90) (wholeSaturation 89) 24 = true := by decide +kernel
theorem wholeValues : ∀ j : Fin 10,
    let n := 80+j.val; valuesCheck coefficients (LockedData.input n) (wholeHistory n) (wholeHistory (n+1)) (wholeSaturation n) 24 = true := by
  intro j
  fin_cases j
  · exact wholeValues_00
  · exact wholeValues_01
  · exact wholeValues_02
  · exact wholeValues_03
  · exact wholeValues_04
  · exact wholeValues_05
  · exact wholeValues_06
  · exact wholeValues_07
  · exact wholeValues_08
  · exact wholeValues_09
private theorem pointValues_00 : valuesCheck coefficients (LockedData.input 80) (pointHistory 80) (pointHistory 81) (pointSaturation 80) 24 = true := by decide +kernel
private theorem pointValues_01 : valuesCheck coefficients (LockedData.input 81) (pointHistory 81) (pointHistory 82) (pointSaturation 81) 24 = true := by decide +kernel
private theorem pointValues_02 : valuesCheck coefficients (LockedData.input 82) (pointHistory 82) (pointHistory 83) (pointSaturation 82) 24 = true := by decide +kernel
private theorem pointValues_03 : valuesCheck coefficients (LockedData.input 83) (pointHistory 83) (pointHistory 84) (pointSaturation 83) 24 = true := by decide +kernel
private theorem pointValues_04 : valuesCheck coefficients (LockedData.input 84) (pointHistory 84) (pointHistory 85) (pointSaturation 84) 24 = true := by decide +kernel
private theorem pointValues_05 : valuesCheck coefficients (LockedData.input 85) (pointHistory 85) (pointHistory 86) (pointSaturation 85) 24 = true := by decide +kernel
private theorem pointValues_06 : valuesCheck coefficients (LockedData.input 86) (pointHistory 86) (pointHistory 87) (pointSaturation 86) 24 = true := by decide +kernel
private theorem pointValues_07 : valuesCheck coefficients (LockedData.input 87) (pointHistory 87) (pointHistory 88) (pointSaturation 87) 24 = true := by decide +kernel
private theorem pointValues_08 : valuesCheck coefficients (LockedData.input 88) (pointHistory 88) (pointHistory 89) (pointSaturation 88) 24 = true := by decide +kernel
private theorem pointValues_09 : valuesCheck coefficients (LockedData.input 89) (pointHistory 89) (pointHistory 90) (pointSaturation 89) 24 = true := by decide +kernel
theorem pointValues : ∀ j : Fin 10,
    let n := 80+j.val; valuesCheck coefficients (LockedData.input n) (pointHistory n) (pointHistory (n+1)) (pointSaturation n) 24 = true := by
  intro j
  fin_cases j
  · exact pointValues_00
  · exact pointValues_01
  · exact pointValues_02
  · exact pointValues_03
  · exact pointValues_04
  · exact pointValues_05
  · exact pointValues_06
  · exact pointValues_07
  · exact pointValues_08
  · exact pointValues_09
private theorem smallValues_00 : valuesCheck coefficients (LockedData.input 80) (smallHistory 80) (smallHistory 81) (smallSaturation 80) 24 = true := by decide +kernel
private theorem smallValues_01 : valuesCheck coefficients (LockedData.input 81) (smallHistory 81) (smallHistory 82) (smallSaturation 81) 24 = true := by decide +kernel
private theorem smallValues_02 : valuesCheck coefficients (LockedData.input 82) (smallHistory 82) (smallHistory 83) (smallSaturation 82) 24 = true := by decide +kernel
private theorem smallValues_03 : valuesCheck coefficients (LockedData.input 83) (smallHistory 83) (smallHistory 84) (smallSaturation 83) 24 = true := by decide +kernel
private theorem smallValues_04 : valuesCheck coefficients (LockedData.input 84) (smallHistory 84) (smallHistory 85) (smallSaturation 84) 24 = true := by decide +kernel
private theorem smallValues_05 : valuesCheck coefficients (LockedData.input 85) (smallHistory 85) (smallHistory 86) (smallSaturation 85) 24 = true := by decide +kernel
private theorem smallValues_06 : valuesCheck coefficients (LockedData.input 86) (smallHistory 86) (smallHistory 87) (smallSaturation 86) 24 = true := by decide +kernel
private theorem smallValues_07 : valuesCheck coefficients (LockedData.input 87) (smallHistory 87) (smallHistory 88) (smallSaturation 87) 24 = true := by decide +kernel
private theorem smallValues_08 : valuesCheck coefficients (LockedData.input 88) (smallHistory 88) (smallHistory 89) (smallSaturation 88) 24 = true := by decide +kernel
private theorem smallValues_09 : valuesCheck coefficients (LockedData.input 89) (smallHistory 89) (smallHistory 90) (smallSaturation 89) 24 = true := by decide +kernel
theorem smallValues : ∀ j : Fin 10,
    let n := 80+j.val; valuesCheck coefficients (LockedData.input n) (smallHistory n) (smallHistory (n+1)) (smallSaturation n) 24 = true := by
  intro j
  fin_cases j
  · exact smallValues_00
  · exact smallValues_01
  · exact smallValues_02
  · exact smallValues_03
  · exact smallValues_04
  · exact smallValues_05
  · exact smallValues_06
  · exact smallValues_07
  · exact smallValues_08
  · exact smallValues_09
private theorem wholeMatrices_00 : matrixCheck coefficients (matrices 80) (matrices 81) (wholeSaturation 80) = true := by decide +kernel
private theorem wholeMatrices_01 : matrixCheck coefficients (matrices 81) (matrices 82) (wholeSaturation 81) = true := by decide +kernel
private theorem wholeMatrices_02 : matrixCheck coefficients (matrices 82) (matrices 83) (wholeSaturation 82) = true := by decide +kernel
private theorem wholeMatrices_03 : matrixCheck coefficients (matrices 83) (matrices 84) (wholeSaturation 83) = true := by decide +kernel
private theorem wholeMatrices_04 : matrixCheck coefficients (matrices 84) (matrices 85) (wholeSaturation 84) = true := by decide +kernel
private theorem wholeMatrices_05 : matrixCheck coefficients (matrices 85) (matrices 86) (wholeSaturation 85) = true := by decide +kernel
private theorem wholeMatrices_06 : matrixCheck coefficients (matrices 86) (matrices 87) (wholeSaturation 86) = true := by decide +kernel
private theorem wholeMatrices_07 : matrixCheck coefficients (matrices 87) (matrices 88) (wholeSaturation 87) = true := by decide +kernel
private theorem wholeMatrices_08 : matrixCheck coefficients (matrices 88) (matrices 89) (wholeSaturation 88) = true := by decide +kernel
private theorem wholeMatrices_09 : matrixCheck coefficients (matrices 89) (matrices 90) (wholeSaturation 89) = true := by decide +kernel
theorem wholeMatrices : ∀ j : Fin 10,
    let n := 80+j.val; matrixCheck coefficients (matrices n) (matrices (n+1)) (wholeSaturation n) = true := by
  intro j
  fin_cases j
  · exact wholeMatrices_00
  · exact wholeMatrices_01
  · exact wholeMatrices_02
  · exact wholeMatrices_03
  · exact wholeMatrices_04
  · exact wholeMatrices_05
  · exact wholeMatrices_06
  · exact wholeMatrices_07
  · exact wholeMatrices_08
  · exact wholeMatrices_09
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck08
