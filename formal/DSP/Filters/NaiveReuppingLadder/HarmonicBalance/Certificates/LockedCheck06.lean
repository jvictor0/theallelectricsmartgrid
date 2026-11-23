import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedData
/-! Generated kernel checks; no native_decide or imported numerical assumptions. -/
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck06
open Enclosure LockedData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem wholeValues_00 : valuesCheck coefficients (LockedData.input 60) (wholeHistory 60) (wholeHistory 61) (wholeSaturation 60) 24 = true := by decide +kernel
private theorem wholeValues_01 : valuesCheck coefficients (LockedData.input 61) (wholeHistory 61) (wholeHistory 62) (wholeSaturation 61) 24 = true := by decide +kernel
private theorem wholeValues_02 : valuesCheck coefficients (LockedData.input 62) (wholeHistory 62) (wholeHistory 63) (wholeSaturation 62) 24 = true := by decide +kernel
private theorem wholeValues_03 : valuesCheck coefficients (LockedData.input 63) (wholeHistory 63) (wholeHistory 64) (wholeSaturation 63) 24 = true := by decide +kernel
private theorem wholeValues_04 : valuesCheck coefficients (LockedData.input 64) (wholeHistory 64) (wholeHistory 65) (wholeSaturation 64) 24 = true := by decide +kernel
private theorem wholeValues_05 : valuesCheck coefficients (LockedData.input 65) (wholeHistory 65) (wholeHistory 66) (wholeSaturation 65) 24 = true := by decide +kernel
private theorem wholeValues_06 : valuesCheck coefficients (LockedData.input 66) (wholeHistory 66) (wholeHistory 67) (wholeSaturation 66) 24 = true := by decide +kernel
private theorem wholeValues_07 : valuesCheck coefficients (LockedData.input 67) (wholeHistory 67) (wholeHistory 68) (wholeSaturation 67) 24 = true := by decide +kernel
private theorem wholeValues_08 : valuesCheck coefficients (LockedData.input 68) (wholeHistory 68) (wholeHistory 69) (wholeSaturation 68) 24 = true := by decide +kernel
private theorem wholeValues_09 : valuesCheck coefficients (LockedData.input 69) (wholeHistory 69) (wholeHistory 70) (wholeSaturation 69) 24 = true := by decide +kernel
theorem wholeValues : ∀ j : Fin 10,
    let n := 60+j.val; valuesCheck coefficients (LockedData.input n) (wholeHistory n) (wholeHistory (n+1)) (wholeSaturation n) 24 = true := by
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
private theorem pointValues_00 : valuesCheck coefficients (LockedData.input 60) (pointHistory 60) (pointHistory 61) (pointSaturation 60) 24 = true := by decide +kernel
private theorem pointValues_01 : valuesCheck coefficients (LockedData.input 61) (pointHistory 61) (pointHistory 62) (pointSaturation 61) 24 = true := by decide +kernel
private theorem pointValues_02 : valuesCheck coefficients (LockedData.input 62) (pointHistory 62) (pointHistory 63) (pointSaturation 62) 24 = true := by decide +kernel
private theorem pointValues_03 : valuesCheck coefficients (LockedData.input 63) (pointHistory 63) (pointHistory 64) (pointSaturation 63) 24 = true := by decide +kernel
private theorem pointValues_04 : valuesCheck coefficients (LockedData.input 64) (pointHistory 64) (pointHistory 65) (pointSaturation 64) 24 = true := by decide +kernel
private theorem pointValues_05 : valuesCheck coefficients (LockedData.input 65) (pointHistory 65) (pointHistory 66) (pointSaturation 65) 24 = true := by decide +kernel
private theorem pointValues_06 : valuesCheck coefficients (LockedData.input 66) (pointHistory 66) (pointHistory 67) (pointSaturation 66) 24 = true := by decide +kernel
private theorem pointValues_07 : valuesCheck coefficients (LockedData.input 67) (pointHistory 67) (pointHistory 68) (pointSaturation 67) 24 = true := by decide +kernel
private theorem pointValues_08 : valuesCheck coefficients (LockedData.input 68) (pointHistory 68) (pointHistory 69) (pointSaturation 68) 24 = true := by decide +kernel
private theorem pointValues_09 : valuesCheck coefficients (LockedData.input 69) (pointHistory 69) (pointHistory 70) (pointSaturation 69) 24 = true := by decide +kernel
theorem pointValues : ∀ j : Fin 10,
    let n := 60+j.val; valuesCheck coefficients (LockedData.input n) (pointHistory n) (pointHistory (n+1)) (pointSaturation n) 24 = true := by
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
private theorem smallValues_00 : valuesCheck coefficients (LockedData.input 60) (smallHistory 60) (smallHistory 61) (smallSaturation 60) 24 = true := by decide +kernel
private theorem smallValues_01 : valuesCheck coefficients (LockedData.input 61) (smallHistory 61) (smallHistory 62) (smallSaturation 61) 24 = true := by decide +kernel
private theorem smallValues_02 : valuesCheck coefficients (LockedData.input 62) (smallHistory 62) (smallHistory 63) (smallSaturation 62) 24 = true := by decide +kernel
private theorem smallValues_03 : valuesCheck coefficients (LockedData.input 63) (smallHistory 63) (smallHistory 64) (smallSaturation 63) 24 = true := by decide +kernel
private theorem smallValues_04 : valuesCheck coefficients (LockedData.input 64) (smallHistory 64) (smallHistory 65) (smallSaturation 64) 24 = true := by decide +kernel
private theorem smallValues_05 : valuesCheck coefficients (LockedData.input 65) (smallHistory 65) (smallHistory 66) (smallSaturation 65) 24 = true := by decide +kernel
private theorem smallValues_06 : valuesCheck coefficients (LockedData.input 66) (smallHistory 66) (smallHistory 67) (smallSaturation 66) 24 = true := by decide +kernel
private theorem smallValues_07 : valuesCheck coefficients (LockedData.input 67) (smallHistory 67) (smallHistory 68) (smallSaturation 67) 24 = true := by decide +kernel
private theorem smallValues_08 : valuesCheck coefficients (LockedData.input 68) (smallHistory 68) (smallHistory 69) (smallSaturation 68) 24 = true := by decide +kernel
private theorem smallValues_09 : valuesCheck coefficients (LockedData.input 69) (smallHistory 69) (smallHistory 70) (smallSaturation 69) 24 = true := by decide +kernel
theorem smallValues : ∀ j : Fin 10,
    let n := 60+j.val; valuesCheck coefficients (LockedData.input n) (smallHistory n) (smallHistory (n+1)) (smallSaturation n) 24 = true := by
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
private theorem wholeMatrices_00 : matrixCheck coefficients (matrices 60) (matrices 61) (wholeSaturation 60) = true := by decide +kernel
private theorem wholeMatrices_01 : matrixCheck coefficients (matrices 61) (matrices 62) (wholeSaturation 61) = true := by decide +kernel
private theorem wholeMatrices_02 : matrixCheck coefficients (matrices 62) (matrices 63) (wholeSaturation 62) = true := by decide +kernel
private theorem wholeMatrices_03 : matrixCheck coefficients (matrices 63) (matrices 64) (wholeSaturation 63) = true := by decide +kernel
private theorem wholeMatrices_04 : matrixCheck coefficients (matrices 64) (matrices 65) (wholeSaturation 64) = true := by decide +kernel
private theorem wholeMatrices_05 : matrixCheck coefficients (matrices 65) (matrices 66) (wholeSaturation 65) = true := by decide +kernel
private theorem wholeMatrices_06 : matrixCheck coefficients (matrices 66) (matrices 67) (wholeSaturation 66) = true := by decide +kernel
private theorem wholeMatrices_07 : matrixCheck coefficients (matrices 67) (matrices 68) (wholeSaturation 67) = true := by decide +kernel
private theorem wholeMatrices_08 : matrixCheck coefficients (matrices 68) (matrices 69) (wholeSaturation 68) = true := by decide +kernel
private theorem wholeMatrices_09 : matrixCheck coefficients (matrices 69) (matrices 70) (wholeSaturation 69) = true := by decide +kernel
theorem wholeMatrices : ∀ j : Fin 10,
    let n := 60+j.val; matrixCheck coefficients (matrices n) (matrices (n+1)) (wholeSaturation n) = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck06
