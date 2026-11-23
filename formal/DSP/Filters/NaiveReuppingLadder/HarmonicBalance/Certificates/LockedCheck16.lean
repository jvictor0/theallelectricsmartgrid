import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedData
/-! Generated kernel checks; no native_decide or imported numerical assumptions. -/
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck16
open Enclosure LockedData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem wholeValues_00 : valuesCheck coefficients (LockedData.input 160) (wholeHistory 160) (wholeHistory 161) (wholeSaturation 160) 24 = true := by decide +kernel
private theorem wholeValues_01 : valuesCheck coefficients (LockedData.input 161) (wholeHistory 161) (wholeHistory 162) (wholeSaturation 161) 24 = true := by decide +kernel
private theorem wholeValues_02 : valuesCheck coefficients (LockedData.input 162) (wholeHistory 162) (wholeHistory 163) (wholeSaturation 162) 24 = true := by decide +kernel
private theorem wholeValues_03 : valuesCheck coefficients (LockedData.input 163) (wholeHistory 163) (wholeHistory 164) (wholeSaturation 163) 24 = true := by decide +kernel
private theorem wholeValues_04 : valuesCheck coefficients (LockedData.input 164) (wholeHistory 164) (wholeHistory 165) (wholeSaturation 164) 24 = true := by decide +kernel
private theorem wholeValues_05 : valuesCheck coefficients (LockedData.input 165) (wholeHistory 165) (wholeHistory 166) (wholeSaturation 165) 24 = true := by decide +kernel
private theorem wholeValues_06 : valuesCheck coefficients (LockedData.input 166) (wholeHistory 166) (wholeHistory 167) (wholeSaturation 166) 24 = true := by decide +kernel
private theorem wholeValues_07 : valuesCheck coefficients (LockedData.input 167) (wholeHistory 167) (wholeHistory 168) (wholeSaturation 167) 24 = true := by decide +kernel
private theorem wholeValues_08 : valuesCheck coefficients (LockedData.input 168) (wholeHistory 168) (wholeHistory 169) (wholeSaturation 168) 24 = true := by decide +kernel
private theorem wholeValues_09 : valuesCheck coefficients (LockedData.input 169) (wholeHistory 169) (wholeHistory 170) (wholeSaturation 169) 24 = true := by decide +kernel
theorem wholeValues : ∀ j : Fin 10,
    let n := 160+j.val; valuesCheck coefficients (LockedData.input n) (wholeHistory n) (wholeHistory (n+1)) (wholeSaturation n) 24 = true := by
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
private theorem pointValues_00 : valuesCheck coefficients (LockedData.input 160) (pointHistory 160) (pointHistory 161) (pointSaturation 160) 24 = true := by decide +kernel
private theorem pointValues_01 : valuesCheck coefficients (LockedData.input 161) (pointHistory 161) (pointHistory 162) (pointSaturation 161) 24 = true := by decide +kernel
private theorem pointValues_02 : valuesCheck coefficients (LockedData.input 162) (pointHistory 162) (pointHistory 163) (pointSaturation 162) 24 = true := by decide +kernel
private theorem pointValues_03 : valuesCheck coefficients (LockedData.input 163) (pointHistory 163) (pointHistory 164) (pointSaturation 163) 24 = true := by decide +kernel
private theorem pointValues_04 : valuesCheck coefficients (LockedData.input 164) (pointHistory 164) (pointHistory 165) (pointSaturation 164) 24 = true := by decide +kernel
private theorem pointValues_05 : valuesCheck coefficients (LockedData.input 165) (pointHistory 165) (pointHistory 166) (pointSaturation 165) 24 = true := by decide +kernel
private theorem pointValues_06 : valuesCheck coefficients (LockedData.input 166) (pointHistory 166) (pointHistory 167) (pointSaturation 166) 24 = true := by decide +kernel
private theorem pointValues_07 : valuesCheck coefficients (LockedData.input 167) (pointHistory 167) (pointHistory 168) (pointSaturation 167) 24 = true := by decide +kernel
private theorem pointValues_08 : valuesCheck coefficients (LockedData.input 168) (pointHistory 168) (pointHistory 169) (pointSaturation 168) 24 = true := by decide +kernel
private theorem pointValues_09 : valuesCheck coefficients (LockedData.input 169) (pointHistory 169) (pointHistory 170) (pointSaturation 169) 24 = true := by decide +kernel
theorem pointValues : ∀ j : Fin 10,
    let n := 160+j.val; valuesCheck coefficients (LockedData.input n) (pointHistory n) (pointHistory (n+1)) (pointSaturation n) 24 = true := by
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
private theorem smallValues_00 : valuesCheck coefficients (LockedData.input 160) (smallHistory 160) (smallHistory 161) (smallSaturation 160) 24 = true := by decide +kernel
private theorem smallValues_01 : valuesCheck coefficients (LockedData.input 161) (smallHistory 161) (smallHistory 162) (smallSaturation 161) 24 = true := by decide +kernel
private theorem smallValues_02 : valuesCheck coefficients (LockedData.input 162) (smallHistory 162) (smallHistory 163) (smallSaturation 162) 24 = true := by decide +kernel
private theorem smallValues_03 : valuesCheck coefficients (LockedData.input 163) (smallHistory 163) (smallHistory 164) (smallSaturation 163) 24 = true := by decide +kernel
private theorem smallValues_04 : valuesCheck coefficients (LockedData.input 164) (smallHistory 164) (smallHistory 165) (smallSaturation 164) 24 = true := by decide +kernel
private theorem smallValues_05 : valuesCheck coefficients (LockedData.input 165) (smallHistory 165) (smallHistory 166) (smallSaturation 165) 24 = true := by decide +kernel
private theorem smallValues_06 : valuesCheck coefficients (LockedData.input 166) (smallHistory 166) (smallHistory 167) (smallSaturation 166) 24 = true := by decide +kernel
private theorem smallValues_07 : valuesCheck coefficients (LockedData.input 167) (smallHistory 167) (smallHistory 168) (smallSaturation 167) 24 = true := by decide +kernel
private theorem smallValues_08 : valuesCheck coefficients (LockedData.input 168) (smallHistory 168) (smallHistory 169) (smallSaturation 168) 24 = true := by decide +kernel
private theorem smallValues_09 : valuesCheck coefficients (LockedData.input 169) (smallHistory 169) (smallHistory 170) (smallSaturation 169) 24 = true := by decide +kernel
theorem smallValues : ∀ j : Fin 10,
    let n := 160+j.val; valuesCheck coefficients (LockedData.input n) (smallHistory n) (smallHistory (n+1)) (smallSaturation n) 24 = true := by
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
private theorem wholeMatrices_00 : matrixCheck coefficients (matrices 160) (matrices 161) (wholeSaturation 160) = true := by decide +kernel
private theorem wholeMatrices_01 : matrixCheck coefficients (matrices 161) (matrices 162) (wholeSaturation 161) = true := by decide +kernel
private theorem wholeMatrices_02 : matrixCheck coefficients (matrices 162) (matrices 163) (wholeSaturation 162) = true := by decide +kernel
private theorem wholeMatrices_03 : matrixCheck coefficients (matrices 163) (matrices 164) (wholeSaturation 163) = true := by decide +kernel
private theorem wholeMatrices_04 : matrixCheck coefficients (matrices 164) (matrices 165) (wholeSaturation 164) = true := by decide +kernel
private theorem wholeMatrices_05 : matrixCheck coefficients (matrices 165) (matrices 166) (wholeSaturation 165) = true := by decide +kernel
private theorem wholeMatrices_06 : matrixCheck coefficients (matrices 166) (matrices 167) (wholeSaturation 166) = true := by decide +kernel
private theorem wholeMatrices_07 : matrixCheck coefficients (matrices 167) (matrices 168) (wholeSaturation 167) = true := by decide +kernel
private theorem wholeMatrices_08 : matrixCheck coefficients (matrices 168) (matrices 169) (wholeSaturation 168) = true := by decide +kernel
private theorem wholeMatrices_09 : matrixCheck coefficients (matrices 169) (matrices 170) (wholeSaturation 169) = true := by decide +kernel
theorem wholeMatrices : ∀ j : Fin 10,
    let n := 160+j.val; matrixCheck coefficients (matrices n) (matrices (n+1)) (wholeSaturation n) = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck16
