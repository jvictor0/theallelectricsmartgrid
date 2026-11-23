import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedData
/-! Generated kernel checks; no native_decide or imported numerical assumptions. -/
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck03
open Enclosure LockedData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem wholeValues_00 : valuesCheck coefficients (LockedData.input 30) (wholeHistory 30) (wholeHistory 31) (wholeSaturation 30) 24 = true := by decide +kernel
private theorem wholeValues_01 : valuesCheck coefficients (LockedData.input 31) (wholeHistory 31) (wholeHistory 32) (wholeSaturation 31) 24 = true := by decide +kernel
private theorem wholeValues_02 : valuesCheck coefficients (LockedData.input 32) (wholeHistory 32) (wholeHistory 33) (wholeSaturation 32) 24 = true := by decide +kernel
private theorem wholeValues_03 : valuesCheck coefficients (LockedData.input 33) (wholeHistory 33) (wholeHistory 34) (wholeSaturation 33) 24 = true := by decide +kernel
private theorem wholeValues_04 : valuesCheck coefficients (LockedData.input 34) (wholeHistory 34) (wholeHistory 35) (wholeSaturation 34) 24 = true := by decide +kernel
private theorem wholeValues_05 : valuesCheck coefficients (LockedData.input 35) (wholeHistory 35) (wholeHistory 36) (wholeSaturation 35) 24 = true := by decide +kernel
private theorem wholeValues_06 : valuesCheck coefficients (LockedData.input 36) (wholeHistory 36) (wholeHistory 37) (wholeSaturation 36) 24 = true := by decide +kernel
private theorem wholeValues_07 : valuesCheck coefficients (LockedData.input 37) (wholeHistory 37) (wholeHistory 38) (wholeSaturation 37) 24 = true := by decide +kernel
private theorem wholeValues_08 : valuesCheck coefficients (LockedData.input 38) (wholeHistory 38) (wholeHistory 39) (wholeSaturation 38) 24 = true := by decide +kernel
private theorem wholeValues_09 : valuesCheck coefficients (LockedData.input 39) (wholeHistory 39) (wholeHistory 40) (wholeSaturation 39) 24 = true := by decide +kernel
theorem wholeValues : ∀ j : Fin 10,
    let n := 30+j.val; valuesCheck coefficients (LockedData.input n) (wholeHistory n) (wholeHistory (n+1)) (wholeSaturation n) 24 = true := by
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
private theorem pointValues_00 : valuesCheck coefficients (LockedData.input 30) (pointHistory 30) (pointHistory 31) (pointSaturation 30) 24 = true := by decide +kernel
private theorem pointValues_01 : valuesCheck coefficients (LockedData.input 31) (pointHistory 31) (pointHistory 32) (pointSaturation 31) 24 = true := by decide +kernel
private theorem pointValues_02 : valuesCheck coefficients (LockedData.input 32) (pointHistory 32) (pointHistory 33) (pointSaturation 32) 24 = true := by decide +kernel
private theorem pointValues_03 : valuesCheck coefficients (LockedData.input 33) (pointHistory 33) (pointHistory 34) (pointSaturation 33) 24 = true := by decide +kernel
private theorem pointValues_04 : valuesCheck coefficients (LockedData.input 34) (pointHistory 34) (pointHistory 35) (pointSaturation 34) 24 = true := by decide +kernel
private theorem pointValues_05 : valuesCheck coefficients (LockedData.input 35) (pointHistory 35) (pointHistory 36) (pointSaturation 35) 24 = true := by decide +kernel
private theorem pointValues_06 : valuesCheck coefficients (LockedData.input 36) (pointHistory 36) (pointHistory 37) (pointSaturation 36) 24 = true := by decide +kernel
private theorem pointValues_07 : valuesCheck coefficients (LockedData.input 37) (pointHistory 37) (pointHistory 38) (pointSaturation 37) 24 = true := by decide +kernel
private theorem pointValues_08 : valuesCheck coefficients (LockedData.input 38) (pointHistory 38) (pointHistory 39) (pointSaturation 38) 24 = true := by decide +kernel
private theorem pointValues_09 : valuesCheck coefficients (LockedData.input 39) (pointHistory 39) (pointHistory 40) (pointSaturation 39) 24 = true := by decide +kernel
theorem pointValues : ∀ j : Fin 10,
    let n := 30+j.val; valuesCheck coefficients (LockedData.input n) (pointHistory n) (pointHistory (n+1)) (pointSaturation n) 24 = true := by
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
private theorem smallValues_00 : valuesCheck coefficients (LockedData.input 30) (smallHistory 30) (smallHistory 31) (smallSaturation 30) 24 = true := by decide +kernel
private theorem smallValues_01 : valuesCheck coefficients (LockedData.input 31) (smallHistory 31) (smallHistory 32) (smallSaturation 31) 24 = true := by decide +kernel
private theorem smallValues_02 : valuesCheck coefficients (LockedData.input 32) (smallHistory 32) (smallHistory 33) (smallSaturation 32) 24 = true := by decide +kernel
private theorem smallValues_03 : valuesCheck coefficients (LockedData.input 33) (smallHistory 33) (smallHistory 34) (smallSaturation 33) 24 = true := by decide +kernel
private theorem smallValues_04 : valuesCheck coefficients (LockedData.input 34) (smallHistory 34) (smallHistory 35) (smallSaturation 34) 24 = true := by decide +kernel
private theorem smallValues_05 : valuesCheck coefficients (LockedData.input 35) (smallHistory 35) (smallHistory 36) (smallSaturation 35) 24 = true := by decide +kernel
private theorem smallValues_06 : valuesCheck coefficients (LockedData.input 36) (smallHistory 36) (smallHistory 37) (smallSaturation 36) 24 = true := by decide +kernel
private theorem smallValues_07 : valuesCheck coefficients (LockedData.input 37) (smallHistory 37) (smallHistory 38) (smallSaturation 37) 24 = true := by decide +kernel
private theorem smallValues_08 : valuesCheck coefficients (LockedData.input 38) (smallHistory 38) (smallHistory 39) (smallSaturation 38) 24 = true := by decide +kernel
private theorem smallValues_09 : valuesCheck coefficients (LockedData.input 39) (smallHistory 39) (smallHistory 40) (smallSaturation 39) 24 = true := by decide +kernel
theorem smallValues : ∀ j : Fin 10,
    let n := 30+j.val; valuesCheck coefficients (LockedData.input n) (smallHistory n) (smallHistory (n+1)) (smallSaturation n) 24 = true := by
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
private theorem wholeMatrices_00 : matrixCheck coefficients (matrices 30) (matrices 31) (wholeSaturation 30) = true := by decide +kernel
private theorem wholeMatrices_01 : matrixCheck coefficients (matrices 31) (matrices 32) (wholeSaturation 31) = true := by decide +kernel
private theorem wholeMatrices_02 : matrixCheck coefficients (matrices 32) (matrices 33) (wholeSaturation 32) = true := by decide +kernel
private theorem wholeMatrices_03 : matrixCheck coefficients (matrices 33) (matrices 34) (wholeSaturation 33) = true := by decide +kernel
private theorem wholeMatrices_04 : matrixCheck coefficients (matrices 34) (matrices 35) (wholeSaturation 34) = true := by decide +kernel
private theorem wholeMatrices_05 : matrixCheck coefficients (matrices 35) (matrices 36) (wholeSaturation 35) = true := by decide +kernel
private theorem wholeMatrices_06 : matrixCheck coefficients (matrices 36) (matrices 37) (wholeSaturation 36) = true := by decide +kernel
private theorem wholeMatrices_07 : matrixCheck coefficients (matrices 37) (matrices 38) (wholeSaturation 37) = true := by decide +kernel
private theorem wholeMatrices_08 : matrixCheck coefficients (matrices 38) (matrices 39) (wholeSaturation 38) = true := by decide +kernel
private theorem wholeMatrices_09 : matrixCheck coefficients (matrices 39) (matrices 40) (wholeSaturation 39) = true := by decide +kernel
theorem wholeMatrices : ∀ j : Fin 10,
    let n := 30+j.val; matrixCheck coefficients (matrices n) (matrices (n+1)) (wholeSaturation n) = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck03
