import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedData
/-! Generated kernel checks; no native_decide or imported numerical assumptions. -/
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck14
open Enclosure LockedData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem wholeValues_00 : valuesCheck coefficients (LockedData.input 140) (wholeHistory 140) (wholeHistory 141) (wholeSaturation 140) 24 = true := by decide +kernel
private theorem wholeValues_01 : valuesCheck coefficients (LockedData.input 141) (wholeHistory 141) (wholeHistory 142) (wholeSaturation 141) 24 = true := by decide +kernel
private theorem wholeValues_02 : valuesCheck coefficients (LockedData.input 142) (wholeHistory 142) (wholeHistory 143) (wholeSaturation 142) 24 = true := by decide +kernel
private theorem wholeValues_03 : valuesCheck coefficients (LockedData.input 143) (wholeHistory 143) (wholeHistory 144) (wholeSaturation 143) 24 = true := by decide +kernel
private theorem wholeValues_04 : valuesCheck coefficients (LockedData.input 144) (wholeHistory 144) (wholeHistory 145) (wholeSaturation 144) 24 = true := by decide +kernel
private theorem wholeValues_05 : valuesCheck coefficients (LockedData.input 145) (wholeHistory 145) (wholeHistory 146) (wholeSaturation 145) 24 = true := by decide +kernel
private theorem wholeValues_06 : valuesCheck coefficients (LockedData.input 146) (wholeHistory 146) (wholeHistory 147) (wholeSaturation 146) 24 = true := by decide +kernel
private theorem wholeValues_07 : valuesCheck coefficients (LockedData.input 147) (wholeHistory 147) (wholeHistory 148) (wholeSaturation 147) 24 = true := by decide +kernel
private theorem wholeValues_08 : valuesCheck coefficients (LockedData.input 148) (wholeHistory 148) (wholeHistory 149) (wholeSaturation 148) 24 = true := by decide +kernel
private theorem wholeValues_09 : valuesCheck coefficients (LockedData.input 149) (wholeHistory 149) (wholeHistory 150) (wholeSaturation 149) 24 = true := by decide +kernel
theorem wholeValues : ∀ j : Fin 10,
    let n := 140+j.val; valuesCheck coefficients (LockedData.input n) (wholeHistory n) (wholeHistory (n+1)) (wholeSaturation n) 24 = true := by
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
private theorem pointValues_00 : valuesCheck coefficients (LockedData.input 140) (pointHistory 140) (pointHistory 141) (pointSaturation 140) 24 = true := by decide +kernel
private theorem pointValues_01 : valuesCheck coefficients (LockedData.input 141) (pointHistory 141) (pointHistory 142) (pointSaturation 141) 24 = true := by decide +kernel
private theorem pointValues_02 : valuesCheck coefficients (LockedData.input 142) (pointHistory 142) (pointHistory 143) (pointSaturation 142) 24 = true := by decide +kernel
private theorem pointValues_03 : valuesCheck coefficients (LockedData.input 143) (pointHistory 143) (pointHistory 144) (pointSaturation 143) 24 = true := by decide +kernel
private theorem pointValues_04 : valuesCheck coefficients (LockedData.input 144) (pointHistory 144) (pointHistory 145) (pointSaturation 144) 24 = true := by decide +kernel
private theorem pointValues_05 : valuesCheck coefficients (LockedData.input 145) (pointHistory 145) (pointHistory 146) (pointSaturation 145) 24 = true := by decide +kernel
private theorem pointValues_06 : valuesCheck coefficients (LockedData.input 146) (pointHistory 146) (pointHistory 147) (pointSaturation 146) 24 = true := by decide +kernel
private theorem pointValues_07 : valuesCheck coefficients (LockedData.input 147) (pointHistory 147) (pointHistory 148) (pointSaturation 147) 24 = true := by decide +kernel
private theorem pointValues_08 : valuesCheck coefficients (LockedData.input 148) (pointHistory 148) (pointHistory 149) (pointSaturation 148) 24 = true := by decide +kernel
private theorem pointValues_09 : valuesCheck coefficients (LockedData.input 149) (pointHistory 149) (pointHistory 150) (pointSaturation 149) 24 = true := by decide +kernel
theorem pointValues : ∀ j : Fin 10,
    let n := 140+j.val; valuesCheck coefficients (LockedData.input n) (pointHistory n) (pointHistory (n+1)) (pointSaturation n) 24 = true := by
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
private theorem smallValues_00 : valuesCheck coefficients (LockedData.input 140) (smallHistory 140) (smallHistory 141) (smallSaturation 140) 24 = true := by decide +kernel
private theorem smallValues_01 : valuesCheck coefficients (LockedData.input 141) (smallHistory 141) (smallHistory 142) (smallSaturation 141) 24 = true := by decide +kernel
private theorem smallValues_02 : valuesCheck coefficients (LockedData.input 142) (smallHistory 142) (smallHistory 143) (smallSaturation 142) 24 = true := by decide +kernel
private theorem smallValues_03 : valuesCheck coefficients (LockedData.input 143) (smallHistory 143) (smallHistory 144) (smallSaturation 143) 24 = true := by decide +kernel
private theorem smallValues_04 : valuesCheck coefficients (LockedData.input 144) (smallHistory 144) (smallHistory 145) (smallSaturation 144) 24 = true := by decide +kernel
private theorem smallValues_05 : valuesCheck coefficients (LockedData.input 145) (smallHistory 145) (smallHistory 146) (smallSaturation 145) 24 = true := by decide +kernel
private theorem smallValues_06 : valuesCheck coefficients (LockedData.input 146) (smallHistory 146) (smallHistory 147) (smallSaturation 146) 24 = true := by decide +kernel
private theorem smallValues_07 : valuesCheck coefficients (LockedData.input 147) (smallHistory 147) (smallHistory 148) (smallSaturation 147) 24 = true := by decide +kernel
private theorem smallValues_08 : valuesCheck coefficients (LockedData.input 148) (smallHistory 148) (smallHistory 149) (smallSaturation 148) 24 = true := by decide +kernel
private theorem smallValues_09 : valuesCheck coefficients (LockedData.input 149) (smallHistory 149) (smallHistory 150) (smallSaturation 149) 24 = true := by decide +kernel
theorem smallValues : ∀ j : Fin 10,
    let n := 140+j.val; valuesCheck coefficients (LockedData.input n) (smallHistory n) (smallHistory (n+1)) (smallSaturation n) 24 = true := by
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
private theorem wholeMatrices_00 : matrixCheck coefficients (matrices 140) (matrices 141) (wholeSaturation 140) = true := by decide +kernel
private theorem wholeMatrices_01 : matrixCheck coefficients (matrices 141) (matrices 142) (wholeSaturation 141) = true := by decide +kernel
private theorem wholeMatrices_02 : matrixCheck coefficients (matrices 142) (matrices 143) (wholeSaturation 142) = true := by decide +kernel
private theorem wholeMatrices_03 : matrixCheck coefficients (matrices 143) (matrices 144) (wholeSaturation 143) = true := by decide +kernel
private theorem wholeMatrices_04 : matrixCheck coefficients (matrices 144) (matrices 145) (wholeSaturation 144) = true := by decide +kernel
private theorem wholeMatrices_05 : matrixCheck coefficients (matrices 145) (matrices 146) (wholeSaturation 145) = true := by decide +kernel
private theorem wholeMatrices_06 : matrixCheck coefficients (matrices 146) (matrices 147) (wholeSaturation 146) = true := by decide +kernel
private theorem wholeMatrices_07 : matrixCheck coefficients (matrices 147) (matrices 148) (wholeSaturation 147) = true := by decide +kernel
private theorem wholeMatrices_08 : matrixCheck coefficients (matrices 148) (matrices 149) (wholeSaturation 148) = true := by decide +kernel
private theorem wholeMatrices_09 : matrixCheck coefficients (matrices 149) (matrices 150) (wholeSaturation 149) = true := by decide +kernel
theorem wholeMatrices : ∀ j : Fin 10,
    let n := 140+j.val; matrixCheck coefficients (matrices n) (matrices (n+1)) (wholeSaturation n) = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck14
