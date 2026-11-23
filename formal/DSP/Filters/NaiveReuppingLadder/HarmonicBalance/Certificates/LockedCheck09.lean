import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedData
/-! Generated kernel checks; no native_decide or imported numerical assumptions. -/
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck09
open Enclosure LockedData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem wholeValues_00 : valuesCheck coefficients (LockedData.input 90) (wholeHistory 90) (wholeHistory 91) (wholeSaturation 90) 24 = true := by decide +kernel
private theorem wholeValues_01 : valuesCheck coefficients (LockedData.input 91) (wholeHistory 91) (wholeHistory 92) (wholeSaturation 91) 24 = true := by decide +kernel
private theorem wholeValues_02 : valuesCheck coefficients (LockedData.input 92) (wholeHistory 92) (wholeHistory 93) (wholeSaturation 92) 24 = true := by decide +kernel
private theorem wholeValues_03 : valuesCheck coefficients (LockedData.input 93) (wholeHistory 93) (wholeHistory 94) (wholeSaturation 93) 24 = true := by decide +kernel
private theorem wholeValues_04 : valuesCheck coefficients (LockedData.input 94) (wholeHistory 94) (wholeHistory 95) (wholeSaturation 94) 24 = true := by decide +kernel
private theorem wholeValues_05 : valuesCheck coefficients (LockedData.input 95) (wholeHistory 95) (wholeHistory 96) (wholeSaturation 95) 24 = true := by decide +kernel
private theorem wholeValues_06 : valuesCheck coefficients (LockedData.input 96) (wholeHistory 96) (wholeHistory 97) (wholeSaturation 96) 24 = true := by decide +kernel
private theorem wholeValues_07 : valuesCheck coefficients (LockedData.input 97) (wholeHistory 97) (wholeHistory 98) (wholeSaturation 97) 24 = true := by decide +kernel
private theorem wholeValues_08 : valuesCheck coefficients (LockedData.input 98) (wholeHistory 98) (wholeHistory 99) (wholeSaturation 98) 24 = true := by decide +kernel
private theorem wholeValues_09 : valuesCheck coefficients (LockedData.input 99) (wholeHistory 99) (wholeHistory 100) (wholeSaturation 99) 24 = true := by decide +kernel
theorem wholeValues : ∀ j : Fin 10,
    let n := 90+j.val; valuesCheck coefficients (LockedData.input n) (wholeHistory n) (wholeHistory (n+1)) (wholeSaturation n) 24 = true := by
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
private theorem pointValues_00 : valuesCheck coefficients (LockedData.input 90) (pointHistory 90) (pointHistory 91) (pointSaturation 90) 24 = true := by decide +kernel
private theorem pointValues_01 : valuesCheck coefficients (LockedData.input 91) (pointHistory 91) (pointHistory 92) (pointSaturation 91) 24 = true := by decide +kernel
private theorem pointValues_02 : valuesCheck coefficients (LockedData.input 92) (pointHistory 92) (pointHistory 93) (pointSaturation 92) 24 = true := by decide +kernel
private theorem pointValues_03 : valuesCheck coefficients (LockedData.input 93) (pointHistory 93) (pointHistory 94) (pointSaturation 93) 24 = true := by decide +kernel
private theorem pointValues_04 : valuesCheck coefficients (LockedData.input 94) (pointHistory 94) (pointHistory 95) (pointSaturation 94) 24 = true := by decide +kernel
private theorem pointValues_05 : valuesCheck coefficients (LockedData.input 95) (pointHistory 95) (pointHistory 96) (pointSaturation 95) 24 = true := by decide +kernel
private theorem pointValues_06 : valuesCheck coefficients (LockedData.input 96) (pointHistory 96) (pointHistory 97) (pointSaturation 96) 24 = true := by decide +kernel
private theorem pointValues_07 : valuesCheck coefficients (LockedData.input 97) (pointHistory 97) (pointHistory 98) (pointSaturation 97) 24 = true := by decide +kernel
private theorem pointValues_08 : valuesCheck coefficients (LockedData.input 98) (pointHistory 98) (pointHistory 99) (pointSaturation 98) 24 = true := by decide +kernel
private theorem pointValues_09 : valuesCheck coefficients (LockedData.input 99) (pointHistory 99) (pointHistory 100) (pointSaturation 99) 24 = true := by decide +kernel
theorem pointValues : ∀ j : Fin 10,
    let n := 90+j.val; valuesCheck coefficients (LockedData.input n) (pointHistory n) (pointHistory (n+1)) (pointSaturation n) 24 = true := by
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
private theorem smallValues_00 : valuesCheck coefficients (LockedData.input 90) (smallHistory 90) (smallHistory 91) (smallSaturation 90) 24 = true := by decide +kernel
private theorem smallValues_01 : valuesCheck coefficients (LockedData.input 91) (smallHistory 91) (smallHistory 92) (smallSaturation 91) 24 = true := by decide +kernel
private theorem smallValues_02 : valuesCheck coefficients (LockedData.input 92) (smallHistory 92) (smallHistory 93) (smallSaturation 92) 24 = true := by decide +kernel
private theorem smallValues_03 : valuesCheck coefficients (LockedData.input 93) (smallHistory 93) (smallHistory 94) (smallSaturation 93) 24 = true := by decide +kernel
private theorem smallValues_04 : valuesCheck coefficients (LockedData.input 94) (smallHistory 94) (smallHistory 95) (smallSaturation 94) 24 = true := by decide +kernel
private theorem smallValues_05 : valuesCheck coefficients (LockedData.input 95) (smallHistory 95) (smallHistory 96) (smallSaturation 95) 24 = true := by decide +kernel
private theorem smallValues_06 : valuesCheck coefficients (LockedData.input 96) (smallHistory 96) (smallHistory 97) (smallSaturation 96) 24 = true := by decide +kernel
private theorem smallValues_07 : valuesCheck coefficients (LockedData.input 97) (smallHistory 97) (smallHistory 98) (smallSaturation 97) 24 = true := by decide +kernel
private theorem smallValues_08 : valuesCheck coefficients (LockedData.input 98) (smallHistory 98) (smallHistory 99) (smallSaturation 98) 24 = true := by decide +kernel
private theorem smallValues_09 : valuesCheck coefficients (LockedData.input 99) (smallHistory 99) (smallHistory 100) (smallSaturation 99) 24 = true := by decide +kernel
theorem smallValues : ∀ j : Fin 10,
    let n := 90+j.val; valuesCheck coefficients (LockedData.input n) (smallHistory n) (smallHistory (n+1)) (smallSaturation n) 24 = true := by
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
private theorem wholeMatrices_00 : matrixCheck coefficients (matrices 90) (matrices 91) (wholeSaturation 90) = true := by decide +kernel
private theorem wholeMatrices_01 : matrixCheck coefficients (matrices 91) (matrices 92) (wholeSaturation 91) = true := by decide +kernel
private theorem wholeMatrices_02 : matrixCheck coefficients (matrices 92) (matrices 93) (wholeSaturation 92) = true := by decide +kernel
private theorem wholeMatrices_03 : matrixCheck coefficients (matrices 93) (matrices 94) (wholeSaturation 93) = true := by decide +kernel
private theorem wholeMatrices_04 : matrixCheck coefficients (matrices 94) (matrices 95) (wholeSaturation 94) = true := by decide +kernel
private theorem wholeMatrices_05 : matrixCheck coefficients (matrices 95) (matrices 96) (wholeSaturation 95) = true := by decide +kernel
private theorem wholeMatrices_06 : matrixCheck coefficients (matrices 96) (matrices 97) (wholeSaturation 96) = true := by decide +kernel
private theorem wholeMatrices_07 : matrixCheck coefficients (matrices 97) (matrices 98) (wholeSaturation 97) = true := by decide +kernel
private theorem wholeMatrices_08 : matrixCheck coefficients (matrices 98) (matrices 99) (wholeSaturation 98) = true := by decide +kernel
private theorem wholeMatrices_09 : matrixCheck coefficients (matrices 99) (matrices 100) (wholeSaturation 99) = true := by decide +kernel
theorem wholeMatrices : ∀ j : Fin 10,
    let n := 90+j.val; matrixCheck coefficients (matrices n) (matrices (n+1)) (wholeSaturation n) = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck09
