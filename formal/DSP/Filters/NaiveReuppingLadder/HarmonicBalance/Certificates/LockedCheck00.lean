import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedData
/-! Generated kernel checks; no native_decide or imported numerical assumptions. -/
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck00
open Enclosure LockedData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem wholeValues_00 : valuesCheck coefficients (LockedData.input 0) (wholeHistory 0) (wholeHistory 1) (wholeSaturation 0) 24 = true := by decide +kernel
private theorem wholeValues_01 : valuesCheck coefficients (LockedData.input 1) (wholeHistory 1) (wholeHistory 2) (wholeSaturation 1) 24 = true := by decide +kernel
private theorem wholeValues_02 : valuesCheck coefficients (LockedData.input 2) (wholeHistory 2) (wholeHistory 3) (wholeSaturation 2) 24 = true := by decide +kernel
private theorem wholeValues_03 : valuesCheck coefficients (LockedData.input 3) (wholeHistory 3) (wholeHistory 4) (wholeSaturation 3) 24 = true := by decide +kernel
private theorem wholeValues_04 : valuesCheck coefficients (LockedData.input 4) (wholeHistory 4) (wholeHistory 5) (wholeSaturation 4) 24 = true := by decide +kernel
private theorem wholeValues_05 : valuesCheck coefficients (LockedData.input 5) (wholeHistory 5) (wholeHistory 6) (wholeSaturation 5) 24 = true := by decide +kernel
private theorem wholeValues_06 : valuesCheck coefficients (LockedData.input 6) (wholeHistory 6) (wholeHistory 7) (wholeSaturation 6) 24 = true := by decide +kernel
private theorem wholeValues_07 : valuesCheck coefficients (LockedData.input 7) (wholeHistory 7) (wholeHistory 8) (wholeSaturation 7) 24 = true := by decide +kernel
private theorem wholeValues_08 : valuesCheck coefficients (LockedData.input 8) (wholeHistory 8) (wholeHistory 9) (wholeSaturation 8) 24 = true := by decide +kernel
private theorem wholeValues_09 : valuesCheck coefficients (LockedData.input 9) (wholeHistory 9) (wholeHistory 10) (wholeSaturation 9) 24 = true := by decide +kernel
theorem wholeValues : ∀ j : Fin 10,
    let n := 0+j.val; valuesCheck coefficients (LockedData.input n) (wholeHistory n) (wholeHistory (n+1)) (wholeSaturation n) 24 = true := by
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
private theorem pointValues_00 : valuesCheck coefficients (LockedData.input 0) (pointHistory 0) (pointHistory 1) (pointSaturation 0) 24 = true := by decide +kernel
private theorem pointValues_01 : valuesCheck coefficients (LockedData.input 1) (pointHistory 1) (pointHistory 2) (pointSaturation 1) 24 = true := by decide +kernel
private theorem pointValues_02 : valuesCheck coefficients (LockedData.input 2) (pointHistory 2) (pointHistory 3) (pointSaturation 2) 24 = true := by decide +kernel
private theorem pointValues_03 : valuesCheck coefficients (LockedData.input 3) (pointHistory 3) (pointHistory 4) (pointSaturation 3) 24 = true := by decide +kernel
private theorem pointValues_04 : valuesCheck coefficients (LockedData.input 4) (pointHistory 4) (pointHistory 5) (pointSaturation 4) 24 = true := by decide +kernel
private theorem pointValues_05 : valuesCheck coefficients (LockedData.input 5) (pointHistory 5) (pointHistory 6) (pointSaturation 5) 24 = true := by decide +kernel
private theorem pointValues_06 : valuesCheck coefficients (LockedData.input 6) (pointHistory 6) (pointHistory 7) (pointSaturation 6) 24 = true := by decide +kernel
private theorem pointValues_07 : valuesCheck coefficients (LockedData.input 7) (pointHistory 7) (pointHistory 8) (pointSaturation 7) 24 = true := by decide +kernel
private theorem pointValues_08 : valuesCheck coefficients (LockedData.input 8) (pointHistory 8) (pointHistory 9) (pointSaturation 8) 24 = true := by decide +kernel
private theorem pointValues_09 : valuesCheck coefficients (LockedData.input 9) (pointHistory 9) (pointHistory 10) (pointSaturation 9) 24 = true := by decide +kernel
theorem pointValues : ∀ j : Fin 10,
    let n := 0+j.val; valuesCheck coefficients (LockedData.input n) (pointHistory n) (pointHistory (n+1)) (pointSaturation n) 24 = true := by
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
private theorem smallValues_00 : valuesCheck coefficients (LockedData.input 0) (smallHistory 0) (smallHistory 1) (smallSaturation 0) 24 = true := by decide +kernel
private theorem smallValues_01 : valuesCheck coefficients (LockedData.input 1) (smallHistory 1) (smallHistory 2) (smallSaturation 1) 24 = true := by decide +kernel
private theorem smallValues_02 : valuesCheck coefficients (LockedData.input 2) (smallHistory 2) (smallHistory 3) (smallSaturation 2) 24 = true := by decide +kernel
private theorem smallValues_03 : valuesCheck coefficients (LockedData.input 3) (smallHistory 3) (smallHistory 4) (smallSaturation 3) 24 = true := by decide +kernel
private theorem smallValues_04 : valuesCheck coefficients (LockedData.input 4) (smallHistory 4) (smallHistory 5) (smallSaturation 4) 24 = true := by decide +kernel
private theorem smallValues_05 : valuesCheck coefficients (LockedData.input 5) (smallHistory 5) (smallHistory 6) (smallSaturation 5) 24 = true := by decide +kernel
private theorem smallValues_06 : valuesCheck coefficients (LockedData.input 6) (smallHistory 6) (smallHistory 7) (smallSaturation 6) 24 = true := by decide +kernel
private theorem smallValues_07 : valuesCheck coefficients (LockedData.input 7) (smallHistory 7) (smallHistory 8) (smallSaturation 7) 24 = true := by decide +kernel
private theorem smallValues_08 : valuesCheck coefficients (LockedData.input 8) (smallHistory 8) (smallHistory 9) (smallSaturation 8) 24 = true := by decide +kernel
private theorem smallValues_09 : valuesCheck coefficients (LockedData.input 9) (smallHistory 9) (smallHistory 10) (smallSaturation 9) 24 = true := by decide +kernel
theorem smallValues : ∀ j : Fin 10,
    let n := 0+j.val; valuesCheck coefficients (LockedData.input n) (smallHistory n) (smallHistory (n+1)) (smallSaturation n) 24 = true := by
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
private theorem wholeMatrices_00 : matrixCheck coefficients (matrices 0) (matrices 1) (wholeSaturation 0) = true := by decide +kernel
private theorem wholeMatrices_01 : matrixCheck coefficients (matrices 1) (matrices 2) (wholeSaturation 1) = true := by decide +kernel
private theorem wholeMatrices_02 : matrixCheck coefficients (matrices 2) (matrices 3) (wholeSaturation 2) = true := by decide +kernel
private theorem wholeMatrices_03 : matrixCheck coefficients (matrices 3) (matrices 4) (wholeSaturation 3) = true := by decide +kernel
private theorem wholeMatrices_04 : matrixCheck coefficients (matrices 4) (matrices 5) (wholeSaturation 4) = true := by decide +kernel
private theorem wholeMatrices_05 : matrixCheck coefficients (matrices 5) (matrices 6) (wholeSaturation 5) = true := by decide +kernel
private theorem wholeMatrices_06 : matrixCheck coefficients (matrices 6) (matrices 7) (wholeSaturation 6) = true := by decide +kernel
private theorem wholeMatrices_07 : matrixCheck coefficients (matrices 7) (matrices 8) (wholeSaturation 7) = true := by decide +kernel
private theorem wholeMatrices_08 : matrixCheck coefficients (matrices 8) (matrices 9) (wholeSaturation 8) = true := by decide +kernel
private theorem wholeMatrices_09 : matrixCheck coefficients (matrices 9) (matrices 10) (wholeSaturation 9) = true := by decide +kernel
theorem wholeMatrices : ∀ j : Fin 10,
    let n := 0+j.val; matrixCheck coefficients (matrices n) (matrices (n+1)) (wholeSaturation n) = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck00
