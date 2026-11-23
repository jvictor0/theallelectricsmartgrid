import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedData
/-! Generated kernel checks; no native_decide or imported numerical assumptions. -/
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck02
open Enclosure LockedData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem wholeValues_00 : valuesCheck coefficients (LockedData.input 20) (wholeHistory 20) (wholeHistory 21) (wholeSaturation 20) 24 = true := by decide +kernel
private theorem wholeValues_01 : valuesCheck coefficients (LockedData.input 21) (wholeHistory 21) (wholeHistory 22) (wholeSaturation 21) 24 = true := by decide +kernel
private theorem wholeValues_02 : valuesCheck coefficients (LockedData.input 22) (wholeHistory 22) (wholeHistory 23) (wholeSaturation 22) 24 = true := by decide +kernel
private theorem wholeValues_03 : valuesCheck coefficients (LockedData.input 23) (wholeHistory 23) (wholeHistory 24) (wholeSaturation 23) 24 = true := by decide +kernel
private theorem wholeValues_04 : valuesCheck coefficients (LockedData.input 24) (wholeHistory 24) (wholeHistory 25) (wholeSaturation 24) 24 = true := by decide +kernel
private theorem wholeValues_05 : valuesCheck coefficients (LockedData.input 25) (wholeHistory 25) (wholeHistory 26) (wholeSaturation 25) 24 = true := by decide +kernel
private theorem wholeValues_06 : valuesCheck coefficients (LockedData.input 26) (wholeHistory 26) (wholeHistory 27) (wholeSaturation 26) 24 = true := by decide +kernel
private theorem wholeValues_07 : valuesCheck coefficients (LockedData.input 27) (wholeHistory 27) (wholeHistory 28) (wholeSaturation 27) 24 = true := by decide +kernel
private theorem wholeValues_08 : valuesCheck coefficients (LockedData.input 28) (wholeHistory 28) (wholeHistory 29) (wholeSaturation 28) 24 = true := by decide +kernel
private theorem wholeValues_09 : valuesCheck coefficients (LockedData.input 29) (wholeHistory 29) (wholeHistory 30) (wholeSaturation 29) 24 = true := by decide +kernel
theorem wholeValues : ∀ j : Fin 10,
    let n := 20+j.val; valuesCheck coefficients (LockedData.input n) (wholeHistory n) (wholeHistory (n+1)) (wholeSaturation n) 24 = true := by
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
private theorem pointValues_00 : valuesCheck coefficients (LockedData.input 20) (pointHistory 20) (pointHistory 21) (pointSaturation 20) 24 = true := by decide +kernel
private theorem pointValues_01 : valuesCheck coefficients (LockedData.input 21) (pointHistory 21) (pointHistory 22) (pointSaturation 21) 24 = true := by decide +kernel
private theorem pointValues_02 : valuesCheck coefficients (LockedData.input 22) (pointHistory 22) (pointHistory 23) (pointSaturation 22) 24 = true := by decide +kernel
private theorem pointValues_03 : valuesCheck coefficients (LockedData.input 23) (pointHistory 23) (pointHistory 24) (pointSaturation 23) 24 = true := by decide +kernel
private theorem pointValues_04 : valuesCheck coefficients (LockedData.input 24) (pointHistory 24) (pointHistory 25) (pointSaturation 24) 24 = true := by decide +kernel
private theorem pointValues_05 : valuesCheck coefficients (LockedData.input 25) (pointHistory 25) (pointHistory 26) (pointSaturation 25) 24 = true := by decide +kernel
private theorem pointValues_06 : valuesCheck coefficients (LockedData.input 26) (pointHistory 26) (pointHistory 27) (pointSaturation 26) 24 = true := by decide +kernel
private theorem pointValues_07 : valuesCheck coefficients (LockedData.input 27) (pointHistory 27) (pointHistory 28) (pointSaturation 27) 24 = true := by decide +kernel
private theorem pointValues_08 : valuesCheck coefficients (LockedData.input 28) (pointHistory 28) (pointHistory 29) (pointSaturation 28) 24 = true := by decide +kernel
private theorem pointValues_09 : valuesCheck coefficients (LockedData.input 29) (pointHistory 29) (pointHistory 30) (pointSaturation 29) 24 = true := by decide +kernel
theorem pointValues : ∀ j : Fin 10,
    let n := 20+j.val; valuesCheck coefficients (LockedData.input n) (pointHistory n) (pointHistory (n+1)) (pointSaturation n) 24 = true := by
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
private theorem smallValues_00 : valuesCheck coefficients (LockedData.input 20) (smallHistory 20) (smallHistory 21) (smallSaturation 20) 24 = true := by decide +kernel
private theorem smallValues_01 : valuesCheck coefficients (LockedData.input 21) (smallHistory 21) (smallHistory 22) (smallSaturation 21) 24 = true := by decide +kernel
private theorem smallValues_02 : valuesCheck coefficients (LockedData.input 22) (smallHistory 22) (smallHistory 23) (smallSaturation 22) 24 = true := by decide +kernel
private theorem smallValues_03 : valuesCheck coefficients (LockedData.input 23) (smallHistory 23) (smallHistory 24) (smallSaturation 23) 24 = true := by decide +kernel
private theorem smallValues_04 : valuesCheck coefficients (LockedData.input 24) (smallHistory 24) (smallHistory 25) (smallSaturation 24) 24 = true := by decide +kernel
private theorem smallValues_05 : valuesCheck coefficients (LockedData.input 25) (smallHistory 25) (smallHistory 26) (smallSaturation 25) 24 = true := by decide +kernel
private theorem smallValues_06 : valuesCheck coefficients (LockedData.input 26) (smallHistory 26) (smallHistory 27) (smallSaturation 26) 24 = true := by decide +kernel
private theorem smallValues_07 : valuesCheck coefficients (LockedData.input 27) (smallHistory 27) (smallHistory 28) (smallSaturation 27) 24 = true := by decide +kernel
private theorem smallValues_08 : valuesCheck coefficients (LockedData.input 28) (smallHistory 28) (smallHistory 29) (smallSaturation 28) 24 = true := by decide +kernel
private theorem smallValues_09 : valuesCheck coefficients (LockedData.input 29) (smallHistory 29) (smallHistory 30) (smallSaturation 29) 24 = true := by decide +kernel
theorem smallValues : ∀ j : Fin 10,
    let n := 20+j.val; valuesCheck coefficients (LockedData.input n) (smallHistory n) (smallHistory (n+1)) (smallSaturation n) 24 = true := by
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
private theorem wholeMatrices_00 : matrixCheck coefficients (matrices 20) (matrices 21) (wholeSaturation 20) = true := by decide +kernel
private theorem wholeMatrices_01 : matrixCheck coefficients (matrices 21) (matrices 22) (wholeSaturation 21) = true := by decide +kernel
private theorem wholeMatrices_02 : matrixCheck coefficients (matrices 22) (matrices 23) (wholeSaturation 22) = true := by decide +kernel
private theorem wholeMatrices_03 : matrixCheck coefficients (matrices 23) (matrices 24) (wholeSaturation 23) = true := by decide +kernel
private theorem wholeMatrices_04 : matrixCheck coefficients (matrices 24) (matrices 25) (wholeSaturation 24) = true := by decide +kernel
private theorem wholeMatrices_05 : matrixCheck coefficients (matrices 25) (matrices 26) (wholeSaturation 25) = true := by decide +kernel
private theorem wholeMatrices_06 : matrixCheck coefficients (matrices 26) (matrices 27) (wholeSaturation 26) = true := by decide +kernel
private theorem wholeMatrices_07 : matrixCheck coefficients (matrices 27) (matrices 28) (wholeSaturation 27) = true := by decide +kernel
private theorem wholeMatrices_08 : matrixCheck coefficients (matrices 28) (matrices 29) (wholeSaturation 28) = true := by decide +kernel
private theorem wholeMatrices_09 : matrixCheck coefficients (matrices 29) (matrices 30) (wholeSaturation 29) = true := by decide +kernel
theorem wholeMatrices : ∀ j : Fin 10,
    let n := 20+j.val; matrixCheck coefficients (matrices n) (matrices (n+1)) (wholeSaturation n) = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck02
