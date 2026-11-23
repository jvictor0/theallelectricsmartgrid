import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedData
/-! Generated kernel checks; no native_decide or imported numerical assumptions. -/
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck07
open Enclosure LockedData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem wholeValues_00 : valuesCheck coefficients (LockedData.input 70) (wholeHistory 70) (wholeHistory 71) (wholeSaturation 70) 24 = true := by decide +kernel
private theorem wholeValues_01 : valuesCheck coefficients (LockedData.input 71) (wholeHistory 71) (wholeHistory 72) (wholeSaturation 71) 24 = true := by decide +kernel
private theorem wholeValues_02 : valuesCheck coefficients (LockedData.input 72) (wholeHistory 72) (wholeHistory 73) (wholeSaturation 72) 24 = true := by decide +kernel
private theorem wholeValues_03 : valuesCheck coefficients (LockedData.input 73) (wholeHistory 73) (wholeHistory 74) (wholeSaturation 73) 24 = true := by decide +kernel
private theorem wholeValues_04 : valuesCheck coefficients (LockedData.input 74) (wholeHistory 74) (wholeHistory 75) (wholeSaturation 74) 24 = true := by decide +kernel
private theorem wholeValues_05 : valuesCheck coefficients (LockedData.input 75) (wholeHistory 75) (wholeHistory 76) (wholeSaturation 75) 24 = true := by decide +kernel
private theorem wholeValues_06 : valuesCheck coefficients (LockedData.input 76) (wholeHistory 76) (wholeHistory 77) (wholeSaturation 76) 24 = true := by decide +kernel
private theorem wholeValues_07 : valuesCheck coefficients (LockedData.input 77) (wholeHistory 77) (wholeHistory 78) (wholeSaturation 77) 24 = true := by decide +kernel
private theorem wholeValues_08 : valuesCheck coefficients (LockedData.input 78) (wholeHistory 78) (wholeHistory 79) (wholeSaturation 78) 24 = true := by decide +kernel
private theorem wholeValues_09 : valuesCheck coefficients (LockedData.input 79) (wholeHistory 79) (wholeHistory 80) (wholeSaturation 79) 24 = true := by decide +kernel
theorem wholeValues : ∀ j : Fin 10,
    let n := 70+j.val; valuesCheck coefficients (LockedData.input n) (wholeHistory n) (wholeHistory (n+1)) (wholeSaturation n) 24 = true := by
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
private theorem pointValues_00 : valuesCheck coefficients (LockedData.input 70) (pointHistory 70) (pointHistory 71) (pointSaturation 70) 24 = true := by decide +kernel
private theorem pointValues_01 : valuesCheck coefficients (LockedData.input 71) (pointHistory 71) (pointHistory 72) (pointSaturation 71) 24 = true := by decide +kernel
private theorem pointValues_02 : valuesCheck coefficients (LockedData.input 72) (pointHistory 72) (pointHistory 73) (pointSaturation 72) 24 = true := by decide +kernel
private theorem pointValues_03 : valuesCheck coefficients (LockedData.input 73) (pointHistory 73) (pointHistory 74) (pointSaturation 73) 24 = true := by decide +kernel
private theorem pointValues_04 : valuesCheck coefficients (LockedData.input 74) (pointHistory 74) (pointHistory 75) (pointSaturation 74) 24 = true := by decide +kernel
private theorem pointValues_05 : valuesCheck coefficients (LockedData.input 75) (pointHistory 75) (pointHistory 76) (pointSaturation 75) 24 = true := by decide +kernel
private theorem pointValues_06 : valuesCheck coefficients (LockedData.input 76) (pointHistory 76) (pointHistory 77) (pointSaturation 76) 24 = true := by decide +kernel
private theorem pointValues_07 : valuesCheck coefficients (LockedData.input 77) (pointHistory 77) (pointHistory 78) (pointSaturation 77) 24 = true := by decide +kernel
private theorem pointValues_08 : valuesCheck coefficients (LockedData.input 78) (pointHistory 78) (pointHistory 79) (pointSaturation 78) 24 = true := by decide +kernel
private theorem pointValues_09 : valuesCheck coefficients (LockedData.input 79) (pointHistory 79) (pointHistory 80) (pointSaturation 79) 24 = true := by decide +kernel
theorem pointValues : ∀ j : Fin 10,
    let n := 70+j.val; valuesCheck coefficients (LockedData.input n) (pointHistory n) (pointHistory (n+1)) (pointSaturation n) 24 = true := by
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
private theorem smallValues_00 : valuesCheck coefficients (LockedData.input 70) (smallHistory 70) (smallHistory 71) (smallSaturation 70) 24 = true := by decide +kernel
private theorem smallValues_01 : valuesCheck coefficients (LockedData.input 71) (smallHistory 71) (smallHistory 72) (smallSaturation 71) 24 = true := by decide +kernel
private theorem smallValues_02 : valuesCheck coefficients (LockedData.input 72) (smallHistory 72) (smallHistory 73) (smallSaturation 72) 24 = true := by decide +kernel
private theorem smallValues_03 : valuesCheck coefficients (LockedData.input 73) (smallHistory 73) (smallHistory 74) (smallSaturation 73) 24 = true := by decide +kernel
private theorem smallValues_04 : valuesCheck coefficients (LockedData.input 74) (smallHistory 74) (smallHistory 75) (smallSaturation 74) 24 = true := by decide +kernel
private theorem smallValues_05 : valuesCheck coefficients (LockedData.input 75) (smallHistory 75) (smallHistory 76) (smallSaturation 75) 24 = true := by decide +kernel
private theorem smallValues_06 : valuesCheck coefficients (LockedData.input 76) (smallHistory 76) (smallHistory 77) (smallSaturation 76) 24 = true := by decide +kernel
private theorem smallValues_07 : valuesCheck coefficients (LockedData.input 77) (smallHistory 77) (smallHistory 78) (smallSaturation 77) 24 = true := by decide +kernel
private theorem smallValues_08 : valuesCheck coefficients (LockedData.input 78) (smallHistory 78) (smallHistory 79) (smallSaturation 78) 24 = true := by decide +kernel
private theorem smallValues_09 : valuesCheck coefficients (LockedData.input 79) (smallHistory 79) (smallHistory 80) (smallSaturation 79) 24 = true := by decide +kernel
theorem smallValues : ∀ j : Fin 10,
    let n := 70+j.val; valuesCheck coefficients (LockedData.input n) (smallHistory n) (smallHistory (n+1)) (smallSaturation n) 24 = true := by
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
private theorem wholeMatrices_00 : matrixCheck coefficients (matrices 70) (matrices 71) (wholeSaturation 70) = true := by decide +kernel
private theorem wholeMatrices_01 : matrixCheck coefficients (matrices 71) (matrices 72) (wholeSaturation 71) = true := by decide +kernel
private theorem wholeMatrices_02 : matrixCheck coefficients (matrices 72) (matrices 73) (wholeSaturation 72) = true := by decide +kernel
private theorem wholeMatrices_03 : matrixCheck coefficients (matrices 73) (matrices 74) (wholeSaturation 73) = true := by decide +kernel
private theorem wholeMatrices_04 : matrixCheck coefficients (matrices 74) (matrices 75) (wholeSaturation 74) = true := by decide +kernel
private theorem wholeMatrices_05 : matrixCheck coefficients (matrices 75) (matrices 76) (wholeSaturation 75) = true := by decide +kernel
private theorem wholeMatrices_06 : matrixCheck coefficients (matrices 76) (matrices 77) (wholeSaturation 76) = true := by decide +kernel
private theorem wholeMatrices_07 : matrixCheck coefficients (matrices 77) (matrices 78) (wholeSaturation 77) = true := by decide +kernel
private theorem wholeMatrices_08 : matrixCheck coefficients (matrices 78) (matrices 79) (wholeSaturation 78) = true := by decide +kernel
private theorem wholeMatrices_09 : matrixCheck coefficients (matrices 79) (matrices 80) (wholeSaturation 79) = true := by decide +kernel
theorem wholeMatrices : ∀ j : Fin 10,
    let n := 70+j.val; matrixCheck coefficients (matrices n) (matrices (n+1)) (wholeSaturation n) = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck07
