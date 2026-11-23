import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedData
/-! Generated kernel checks; no native_decide or imported numerical assumptions. -/
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck05
open Enclosure LockedData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem wholeValues_00 : valuesCheck coefficients (LockedData.input 50) (wholeHistory 50) (wholeHistory 51) (wholeSaturation 50) 24 = true := by decide +kernel
private theorem wholeValues_01 : valuesCheck coefficients (LockedData.input 51) (wholeHistory 51) (wholeHistory 52) (wholeSaturation 51) 24 = true := by decide +kernel
private theorem wholeValues_02 : valuesCheck coefficients (LockedData.input 52) (wholeHistory 52) (wholeHistory 53) (wholeSaturation 52) 24 = true := by decide +kernel
private theorem wholeValues_03 : valuesCheck coefficients (LockedData.input 53) (wholeHistory 53) (wholeHistory 54) (wholeSaturation 53) 24 = true := by decide +kernel
private theorem wholeValues_04 : valuesCheck coefficients (LockedData.input 54) (wholeHistory 54) (wholeHistory 55) (wholeSaturation 54) 24 = true := by decide +kernel
private theorem wholeValues_05 : valuesCheck coefficients (LockedData.input 55) (wholeHistory 55) (wholeHistory 56) (wholeSaturation 55) 24 = true := by decide +kernel
private theorem wholeValues_06 : valuesCheck coefficients (LockedData.input 56) (wholeHistory 56) (wholeHistory 57) (wholeSaturation 56) 24 = true := by decide +kernel
private theorem wholeValues_07 : valuesCheck coefficients (LockedData.input 57) (wholeHistory 57) (wholeHistory 58) (wholeSaturation 57) 24 = true := by decide +kernel
private theorem wholeValues_08 : valuesCheck coefficients (LockedData.input 58) (wholeHistory 58) (wholeHistory 59) (wholeSaturation 58) 24 = true := by decide +kernel
private theorem wholeValues_09 : valuesCheck coefficients (LockedData.input 59) (wholeHistory 59) (wholeHistory 60) (wholeSaturation 59) 24 = true := by decide +kernel
theorem wholeValues : ∀ j : Fin 10,
    let n := 50+j.val; valuesCheck coefficients (LockedData.input n) (wholeHistory n) (wholeHistory (n+1)) (wholeSaturation n) 24 = true := by
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
private theorem pointValues_00 : valuesCheck coefficients (LockedData.input 50) (pointHistory 50) (pointHistory 51) (pointSaturation 50) 24 = true := by decide +kernel
private theorem pointValues_01 : valuesCheck coefficients (LockedData.input 51) (pointHistory 51) (pointHistory 52) (pointSaturation 51) 24 = true := by decide +kernel
private theorem pointValues_02 : valuesCheck coefficients (LockedData.input 52) (pointHistory 52) (pointHistory 53) (pointSaturation 52) 24 = true := by decide +kernel
private theorem pointValues_03 : valuesCheck coefficients (LockedData.input 53) (pointHistory 53) (pointHistory 54) (pointSaturation 53) 24 = true := by decide +kernel
private theorem pointValues_04 : valuesCheck coefficients (LockedData.input 54) (pointHistory 54) (pointHistory 55) (pointSaturation 54) 24 = true := by decide +kernel
private theorem pointValues_05 : valuesCheck coefficients (LockedData.input 55) (pointHistory 55) (pointHistory 56) (pointSaturation 55) 24 = true := by decide +kernel
private theorem pointValues_06 : valuesCheck coefficients (LockedData.input 56) (pointHistory 56) (pointHistory 57) (pointSaturation 56) 24 = true := by decide +kernel
private theorem pointValues_07 : valuesCheck coefficients (LockedData.input 57) (pointHistory 57) (pointHistory 58) (pointSaturation 57) 24 = true := by decide +kernel
private theorem pointValues_08 : valuesCheck coefficients (LockedData.input 58) (pointHistory 58) (pointHistory 59) (pointSaturation 58) 24 = true := by decide +kernel
private theorem pointValues_09 : valuesCheck coefficients (LockedData.input 59) (pointHistory 59) (pointHistory 60) (pointSaturation 59) 24 = true := by decide +kernel
theorem pointValues : ∀ j : Fin 10,
    let n := 50+j.val; valuesCheck coefficients (LockedData.input n) (pointHistory n) (pointHistory (n+1)) (pointSaturation n) 24 = true := by
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
private theorem smallValues_00 : valuesCheck coefficients (LockedData.input 50) (smallHistory 50) (smallHistory 51) (smallSaturation 50) 24 = true := by decide +kernel
private theorem smallValues_01 : valuesCheck coefficients (LockedData.input 51) (smallHistory 51) (smallHistory 52) (smallSaturation 51) 24 = true := by decide +kernel
private theorem smallValues_02 : valuesCheck coefficients (LockedData.input 52) (smallHistory 52) (smallHistory 53) (smallSaturation 52) 24 = true := by decide +kernel
private theorem smallValues_03 : valuesCheck coefficients (LockedData.input 53) (smallHistory 53) (smallHistory 54) (smallSaturation 53) 24 = true := by decide +kernel
private theorem smallValues_04 : valuesCheck coefficients (LockedData.input 54) (smallHistory 54) (smallHistory 55) (smallSaturation 54) 24 = true := by decide +kernel
private theorem smallValues_05 : valuesCheck coefficients (LockedData.input 55) (smallHistory 55) (smallHistory 56) (smallSaturation 55) 24 = true := by decide +kernel
private theorem smallValues_06 : valuesCheck coefficients (LockedData.input 56) (smallHistory 56) (smallHistory 57) (smallSaturation 56) 24 = true := by decide +kernel
private theorem smallValues_07 : valuesCheck coefficients (LockedData.input 57) (smallHistory 57) (smallHistory 58) (smallSaturation 57) 24 = true := by decide +kernel
private theorem smallValues_08 : valuesCheck coefficients (LockedData.input 58) (smallHistory 58) (smallHistory 59) (smallSaturation 58) 24 = true := by decide +kernel
private theorem smallValues_09 : valuesCheck coefficients (LockedData.input 59) (smallHistory 59) (smallHistory 60) (smallSaturation 59) 24 = true := by decide +kernel
theorem smallValues : ∀ j : Fin 10,
    let n := 50+j.val; valuesCheck coefficients (LockedData.input n) (smallHistory n) (smallHistory (n+1)) (smallSaturation n) 24 = true := by
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
private theorem wholeMatrices_00 : matrixCheck coefficients (matrices 50) (matrices 51) (wholeSaturation 50) = true := by decide +kernel
private theorem wholeMatrices_01 : matrixCheck coefficients (matrices 51) (matrices 52) (wholeSaturation 51) = true := by decide +kernel
private theorem wholeMatrices_02 : matrixCheck coefficients (matrices 52) (matrices 53) (wholeSaturation 52) = true := by decide +kernel
private theorem wholeMatrices_03 : matrixCheck coefficients (matrices 53) (matrices 54) (wholeSaturation 53) = true := by decide +kernel
private theorem wholeMatrices_04 : matrixCheck coefficients (matrices 54) (matrices 55) (wholeSaturation 54) = true := by decide +kernel
private theorem wholeMatrices_05 : matrixCheck coefficients (matrices 55) (matrices 56) (wholeSaturation 55) = true := by decide +kernel
private theorem wholeMatrices_06 : matrixCheck coefficients (matrices 56) (matrices 57) (wholeSaturation 56) = true := by decide +kernel
private theorem wholeMatrices_07 : matrixCheck coefficients (matrices 57) (matrices 58) (wholeSaturation 57) = true := by decide +kernel
private theorem wholeMatrices_08 : matrixCheck coefficients (matrices 58) (matrices 59) (wholeSaturation 58) = true := by decide +kernel
private theorem wholeMatrices_09 : matrixCheck coefficients (matrices 59) (matrices 60) (wholeSaturation 59) = true := by decide +kernel
theorem wholeMatrices : ∀ j : Fin 10,
    let n := 50+j.val; matrixCheck coefficients (matrices n) (matrices (n+1)) (wholeSaturation n) = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck05
