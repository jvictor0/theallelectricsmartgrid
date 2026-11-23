import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedData
/-! Generated kernel checks; no native_decide or imported numerical assumptions. -/
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck10
open Enclosure LockedData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem wholeValues_00 : valuesCheck coefficients (LockedData.input 100) (wholeHistory 100) (wholeHistory 101) (wholeSaturation 100) 24 = true := by decide +kernel
private theorem wholeValues_01 : valuesCheck coefficients (LockedData.input 101) (wholeHistory 101) (wholeHistory 102) (wholeSaturation 101) 24 = true := by decide +kernel
private theorem wholeValues_02 : valuesCheck coefficients (LockedData.input 102) (wholeHistory 102) (wholeHistory 103) (wholeSaturation 102) 24 = true := by decide +kernel
private theorem wholeValues_03 : valuesCheck coefficients (LockedData.input 103) (wholeHistory 103) (wholeHistory 104) (wholeSaturation 103) 24 = true := by decide +kernel
private theorem wholeValues_04 : valuesCheck coefficients (LockedData.input 104) (wholeHistory 104) (wholeHistory 105) (wholeSaturation 104) 24 = true := by decide +kernel
private theorem wholeValues_05 : valuesCheck coefficients (LockedData.input 105) (wholeHistory 105) (wholeHistory 106) (wholeSaturation 105) 24 = true := by decide +kernel
private theorem wholeValues_06 : valuesCheck coefficients (LockedData.input 106) (wholeHistory 106) (wholeHistory 107) (wholeSaturation 106) 24 = true := by decide +kernel
private theorem wholeValues_07 : valuesCheck coefficients (LockedData.input 107) (wholeHistory 107) (wholeHistory 108) (wholeSaturation 107) 24 = true := by decide +kernel
private theorem wholeValues_08 : valuesCheck coefficients (LockedData.input 108) (wholeHistory 108) (wholeHistory 109) (wholeSaturation 108) 24 = true := by decide +kernel
private theorem wholeValues_09 : valuesCheck coefficients (LockedData.input 109) (wholeHistory 109) (wholeHistory 110) (wholeSaturation 109) 24 = true := by decide +kernel
theorem wholeValues : ∀ j : Fin 10,
    let n := 100+j.val; valuesCheck coefficients (LockedData.input n) (wholeHistory n) (wholeHistory (n+1)) (wholeSaturation n) 24 = true := by
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
private theorem pointValues_00 : valuesCheck coefficients (LockedData.input 100) (pointHistory 100) (pointHistory 101) (pointSaturation 100) 24 = true := by decide +kernel
private theorem pointValues_01 : valuesCheck coefficients (LockedData.input 101) (pointHistory 101) (pointHistory 102) (pointSaturation 101) 24 = true := by decide +kernel
private theorem pointValues_02 : valuesCheck coefficients (LockedData.input 102) (pointHistory 102) (pointHistory 103) (pointSaturation 102) 24 = true := by decide +kernel
private theorem pointValues_03 : valuesCheck coefficients (LockedData.input 103) (pointHistory 103) (pointHistory 104) (pointSaturation 103) 24 = true := by decide +kernel
private theorem pointValues_04 : valuesCheck coefficients (LockedData.input 104) (pointHistory 104) (pointHistory 105) (pointSaturation 104) 24 = true := by decide +kernel
private theorem pointValues_05 : valuesCheck coefficients (LockedData.input 105) (pointHistory 105) (pointHistory 106) (pointSaturation 105) 24 = true := by decide +kernel
private theorem pointValues_06 : valuesCheck coefficients (LockedData.input 106) (pointHistory 106) (pointHistory 107) (pointSaturation 106) 24 = true := by decide +kernel
private theorem pointValues_07 : valuesCheck coefficients (LockedData.input 107) (pointHistory 107) (pointHistory 108) (pointSaturation 107) 24 = true := by decide +kernel
private theorem pointValues_08 : valuesCheck coefficients (LockedData.input 108) (pointHistory 108) (pointHistory 109) (pointSaturation 108) 24 = true := by decide +kernel
private theorem pointValues_09 : valuesCheck coefficients (LockedData.input 109) (pointHistory 109) (pointHistory 110) (pointSaturation 109) 24 = true := by decide +kernel
theorem pointValues : ∀ j : Fin 10,
    let n := 100+j.val; valuesCheck coefficients (LockedData.input n) (pointHistory n) (pointHistory (n+1)) (pointSaturation n) 24 = true := by
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
private theorem smallValues_00 : valuesCheck coefficients (LockedData.input 100) (smallHistory 100) (smallHistory 101) (smallSaturation 100) 24 = true := by decide +kernel
private theorem smallValues_01 : valuesCheck coefficients (LockedData.input 101) (smallHistory 101) (smallHistory 102) (smallSaturation 101) 24 = true := by decide +kernel
private theorem smallValues_02 : valuesCheck coefficients (LockedData.input 102) (smallHistory 102) (smallHistory 103) (smallSaturation 102) 24 = true := by decide +kernel
private theorem smallValues_03 : valuesCheck coefficients (LockedData.input 103) (smallHistory 103) (smallHistory 104) (smallSaturation 103) 24 = true := by decide +kernel
private theorem smallValues_04 : valuesCheck coefficients (LockedData.input 104) (smallHistory 104) (smallHistory 105) (smallSaturation 104) 24 = true := by decide +kernel
private theorem smallValues_05 : valuesCheck coefficients (LockedData.input 105) (smallHistory 105) (smallHistory 106) (smallSaturation 105) 24 = true := by decide +kernel
private theorem smallValues_06 : valuesCheck coefficients (LockedData.input 106) (smallHistory 106) (smallHistory 107) (smallSaturation 106) 24 = true := by decide +kernel
private theorem smallValues_07 : valuesCheck coefficients (LockedData.input 107) (smallHistory 107) (smallHistory 108) (smallSaturation 107) 24 = true := by decide +kernel
private theorem smallValues_08 : valuesCheck coefficients (LockedData.input 108) (smallHistory 108) (smallHistory 109) (smallSaturation 108) 24 = true := by decide +kernel
private theorem smallValues_09 : valuesCheck coefficients (LockedData.input 109) (smallHistory 109) (smallHistory 110) (smallSaturation 109) 24 = true := by decide +kernel
theorem smallValues : ∀ j : Fin 10,
    let n := 100+j.val; valuesCheck coefficients (LockedData.input n) (smallHistory n) (smallHistory (n+1)) (smallSaturation n) 24 = true := by
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
private theorem wholeMatrices_00 : matrixCheck coefficients (matrices 100) (matrices 101) (wholeSaturation 100) = true := by decide +kernel
private theorem wholeMatrices_01 : matrixCheck coefficients (matrices 101) (matrices 102) (wholeSaturation 101) = true := by decide +kernel
private theorem wholeMatrices_02 : matrixCheck coefficients (matrices 102) (matrices 103) (wholeSaturation 102) = true := by decide +kernel
private theorem wholeMatrices_03 : matrixCheck coefficients (matrices 103) (matrices 104) (wholeSaturation 103) = true := by decide +kernel
private theorem wholeMatrices_04 : matrixCheck coefficients (matrices 104) (matrices 105) (wholeSaturation 104) = true := by decide +kernel
private theorem wholeMatrices_05 : matrixCheck coefficients (matrices 105) (matrices 106) (wholeSaturation 105) = true := by decide +kernel
private theorem wholeMatrices_06 : matrixCheck coefficients (matrices 106) (matrices 107) (wholeSaturation 106) = true := by decide +kernel
private theorem wholeMatrices_07 : matrixCheck coefficients (matrices 107) (matrices 108) (wholeSaturation 107) = true := by decide +kernel
private theorem wholeMatrices_08 : matrixCheck coefficients (matrices 108) (matrices 109) (wholeSaturation 108) = true := by decide +kernel
private theorem wholeMatrices_09 : matrixCheck coefficients (matrices 109) (matrices 110) (wholeSaturation 109) = true := by decide +kernel
theorem wholeMatrices : ∀ j : Fin 10,
    let n := 100+j.val; matrixCheck coefficients (matrices n) (matrices (n+1)) (wholeSaturation n) = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck10
