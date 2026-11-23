import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedData
/-! Generated kernel checks; no native_decide or imported numerical assumptions. -/
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck01
open Enclosure LockedData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem wholeValues_00 : valuesCheck coefficients (LockedData.input 10) (wholeHistory 10) (wholeHistory 11) (wholeSaturation 10) 24 = true := by decide +kernel
private theorem wholeValues_01 : valuesCheck coefficients (LockedData.input 11) (wholeHistory 11) (wholeHistory 12) (wholeSaturation 11) 24 = true := by decide +kernel
private theorem wholeValues_02 : valuesCheck coefficients (LockedData.input 12) (wholeHistory 12) (wholeHistory 13) (wholeSaturation 12) 24 = true := by decide +kernel
private theorem wholeValues_03 : valuesCheck coefficients (LockedData.input 13) (wholeHistory 13) (wholeHistory 14) (wholeSaturation 13) 24 = true := by decide +kernel
private theorem wholeValues_04 : valuesCheck coefficients (LockedData.input 14) (wholeHistory 14) (wholeHistory 15) (wholeSaturation 14) 24 = true := by decide +kernel
private theorem wholeValues_05 : valuesCheck coefficients (LockedData.input 15) (wholeHistory 15) (wholeHistory 16) (wholeSaturation 15) 24 = true := by decide +kernel
private theorem wholeValues_06 : valuesCheck coefficients (LockedData.input 16) (wholeHistory 16) (wholeHistory 17) (wholeSaturation 16) 24 = true := by decide +kernel
private theorem wholeValues_07 : valuesCheck coefficients (LockedData.input 17) (wholeHistory 17) (wholeHistory 18) (wholeSaturation 17) 24 = true := by decide +kernel
private theorem wholeValues_08 : valuesCheck coefficients (LockedData.input 18) (wholeHistory 18) (wholeHistory 19) (wholeSaturation 18) 24 = true := by decide +kernel
private theorem wholeValues_09 : valuesCheck coefficients (LockedData.input 19) (wholeHistory 19) (wholeHistory 20) (wholeSaturation 19) 24 = true := by decide +kernel
theorem wholeValues : ∀ j : Fin 10,
    let n := 10+j.val; valuesCheck coefficients (LockedData.input n) (wholeHistory n) (wholeHistory (n+1)) (wholeSaturation n) 24 = true := by
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
private theorem pointValues_00 : valuesCheck coefficients (LockedData.input 10) (pointHistory 10) (pointHistory 11) (pointSaturation 10) 24 = true := by decide +kernel
private theorem pointValues_01 : valuesCheck coefficients (LockedData.input 11) (pointHistory 11) (pointHistory 12) (pointSaturation 11) 24 = true := by decide +kernel
private theorem pointValues_02 : valuesCheck coefficients (LockedData.input 12) (pointHistory 12) (pointHistory 13) (pointSaturation 12) 24 = true := by decide +kernel
private theorem pointValues_03 : valuesCheck coefficients (LockedData.input 13) (pointHistory 13) (pointHistory 14) (pointSaturation 13) 24 = true := by decide +kernel
private theorem pointValues_04 : valuesCheck coefficients (LockedData.input 14) (pointHistory 14) (pointHistory 15) (pointSaturation 14) 24 = true := by decide +kernel
private theorem pointValues_05 : valuesCheck coefficients (LockedData.input 15) (pointHistory 15) (pointHistory 16) (pointSaturation 15) 24 = true := by decide +kernel
private theorem pointValues_06 : valuesCheck coefficients (LockedData.input 16) (pointHistory 16) (pointHistory 17) (pointSaturation 16) 24 = true := by decide +kernel
private theorem pointValues_07 : valuesCheck coefficients (LockedData.input 17) (pointHistory 17) (pointHistory 18) (pointSaturation 17) 24 = true := by decide +kernel
private theorem pointValues_08 : valuesCheck coefficients (LockedData.input 18) (pointHistory 18) (pointHistory 19) (pointSaturation 18) 24 = true := by decide +kernel
private theorem pointValues_09 : valuesCheck coefficients (LockedData.input 19) (pointHistory 19) (pointHistory 20) (pointSaturation 19) 24 = true := by decide +kernel
theorem pointValues : ∀ j : Fin 10,
    let n := 10+j.val; valuesCheck coefficients (LockedData.input n) (pointHistory n) (pointHistory (n+1)) (pointSaturation n) 24 = true := by
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
private theorem smallValues_00 : valuesCheck coefficients (LockedData.input 10) (smallHistory 10) (smallHistory 11) (smallSaturation 10) 24 = true := by decide +kernel
private theorem smallValues_01 : valuesCheck coefficients (LockedData.input 11) (smallHistory 11) (smallHistory 12) (smallSaturation 11) 24 = true := by decide +kernel
private theorem smallValues_02 : valuesCheck coefficients (LockedData.input 12) (smallHistory 12) (smallHistory 13) (smallSaturation 12) 24 = true := by decide +kernel
private theorem smallValues_03 : valuesCheck coefficients (LockedData.input 13) (smallHistory 13) (smallHistory 14) (smallSaturation 13) 24 = true := by decide +kernel
private theorem smallValues_04 : valuesCheck coefficients (LockedData.input 14) (smallHistory 14) (smallHistory 15) (smallSaturation 14) 24 = true := by decide +kernel
private theorem smallValues_05 : valuesCheck coefficients (LockedData.input 15) (smallHistory 15) (smallHistory 16) (smallSaturation 15) 24 = true := by decide +kernel
private theorem smallValues_06 : valuesCheck coefficients (LockedData.input 16) (smallHistory 16) (smallHistory 17) (smallSaturation 16) 24 = true := by decide +kernel
private theorem smallValues_07 : valuesCheck coefficients (LockedData.input 17) (smallHistory 17) (smallHistory 18) (smallSaturation 17) 24 = true := by decide +kernel
private theorem smallValues_08 : valuesCheck coefficients (LockedData.input 18) (smallHistory 18) (smallHistory 19) (smallSaturation 18) 24 = true := by decide +kernel
private theorem smallValues_09 : valuesCheck coefficients (LockedData.input 19) (smallHistory 19) (smallHistory 20) (smallSaturation 19) 24 = true := by decide +kernel
theorem smallValues : ∀ j : Fin 10,
    let n := 10+j.val; valuesCheck coefficients (LockedData.input n) (smallHistory n) (smallHistory (n+1)) (smallSaturation n) 24 = true := by
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
private theorem wholeMatrices_00 : matrixCheck coefficients (matrices 10) (matrices 11) (wholeSaturation 10) = true := by decide +kernel
private theorem wholeMatrices_01 : matrixCheck coefficients (matrices 11) (matrices 12) (wholeSaturation 11) = true := by decide +kernel
private theorem wholeMatrices_02 : matrixCheck coefficients (matrices 12) (matrices 13) (wholeSaturation 12) = true := by decide +kernel
private theorem wholeMatrices_03 : matrixCheck coefficients (matrices 13) (matrices 14) (wholeSaturation 13) = true := by decide +kernel
private theorem wholeMatrices_04 : matrixCheck coefficients (matrices 14) (matrices 15) (wholeSaturation 14) = true := by decide +kernel
private theorem wholeMatrices_05 : matrixCheck coefficients (matrices 15) (matrices 16) (wholeSaturation 15) = true := by decide +kernel
private theorem wholeMatrices_06 : matrixCheck coefficients (matrices 16) (matrices 17) (wholeSaturation 16) = true := by decide +kernel
private theorem wholeMatrices_07 : matrixCheck coefficients (matrices 17) (matrices 18) (wholeSaturation 17) = true := by decide +kernel
private theorem wholeMatrices_08 : matrixCheck coefficients (matrices 18) (matrices 19) (wholeSaturation 18) = true := by decide +kernel
private theorem wholeMatrices_09 : matrixCheck coefficients (matrices 19) (matrices 20) (wholeSaturation 19) = true := by decide +kernel
theorem wholeMatrices : ∀ j : Fin 10,
    let n := 10+j.val; matrixCheck coefficients (matrices n) (matrices (n+1)) (wholeSaturation n) = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck01
