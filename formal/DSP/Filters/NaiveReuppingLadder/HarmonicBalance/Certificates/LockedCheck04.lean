import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedData
/-! Generated kernel checks; no native_decide or imported numerical assumptions. -/
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck04
open Enclosure LockedData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem wholeValues_00 : valuesCheck coefficients (LockedData.input 40) (wholeHistory 40) (wholeHistory 41) (wholeSaturation 40) 24 = true := by decide +kernel
private theorem wholeValues_01 : valuesCheck coefficients (LockedData.input 41) (wholeHistory 41) (wholeHistory 42) (wholeSaturation 41) 24 = true := by decide +kernel
private theorem wholeValues_02 : valuesCheck coefficients (LockedData.input 42) (wholeHistory 42) (wholeHistory 43) (wholeSaturation 42) 24 = true := by decide +kernel
private theorem wholeValues_03 : valuesCheck coefficients (LockedData.input 43) (wholeHistory 43) (wholeHistory 44) (wholeSaturation 43) 24 = true := by decide +kernel
private theorem wholeValues_04 : valuesCheck coefficients (LockedData.input 44) (wholeHistory 44) (wholeHistory 45) (wholeSaturation 44) 24 = true := by decide +kernel
private theorem wholeValues_05 : valuesCheck coefficients (LockedData.input 45) (wholeHistory 45) (wholeHistory 46) (wholeSaturation 45) 24 = true := by decide +kernel
private theorem wholeValues_06 : valuesCheck coefficients (LockedData.input 46) (wholeHistory 46) (wholeHistory 47) (wholeSaturation 46) 24 = true := by decide +kernel
private theorem wholeValues_07 : valuesCheck coefficients (LockedData.input 47) (wholeHistory 47) (wholeHistory 48) (wholeSaturation 47) 24 = true := by decide +kernel
private theorem wholeValues_08 : valuesCheck coefficients (LockedData.input 48) (wholeHistory 48) (wholeHistory 49) (wholeSaturation 48) 24 = true := by decide +kernel
private theorem wholeValues_09 : valuesCheck coefficients (LockedData.input 49) (wholeHistory 49) (wholeHistory 50) (wholeSaturation 49) 24 = true := by decide +kernel
theorem wholeValues : ∀ j : Fin 10,
    let n := 40+j.val; valuesCheck coefficients (LockedData.input n) (wholeHistory n) (wholeHistory (n+1)) (wholeSaturation n) 24 = true := by
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
private theorem pointValues_00 : valuesCheck coefficients (LockedData.input 40) (pointHistory 40) (pointHistory 41) (pointSaturation 40) 24 = true := by decide +kernel
private theorem pointValues_01 : valuesCheck coefficients (LockedData.input 41) (pointHistory 41) (pointHistory 42) (pointSaturation 41) 24 = true := by decide +kernel
private theorem pointValues_02 : valuesCheck coefficients (LockedData.input 42) (pointHistory 42) (pointHistory 43) (pointSaturation 42) 24 = true := by decide +kernel
private theorem pointValues_03 : valuesCheck coefficients (LockedData.input 43) (pointHistory 43) (pointHistory 44) (pointSaturation 43) 24 = true := by decide +kernel
private theorem pointValues_04 : valuesCheck coefficients (LockedData.input 44) (pointHistory 44) (pointHistory 45) (pointSaturation 44) 24 = true := by decide +kernel
private theorem pointValues_05 : valuesCheck coefficients (LockedData.input 45) (pointHistory 45) (pointHistory 46) (pointSaturation 45) 24 = true := by decide +kernel
private theorem pointValues_06 : valuesCheck coefficients (LockedData.input 46) (pointHistory 46) (pointHistory 47) (pointSaturation 46) 24 = true := by decide +kernel
private theorem pointValues_07 : valuesCheck coefficients (LockedData.input 47) (pointHistory 47) (pointHistory 48) (pointSaturation 47) 24 = true := by decide +kernel
private theorem pointValues_08 : valuesCheck coefficients (LockedData.input 48) (pointHistory 48) (pointHistory 49) (pointSaturation 48) 24 = true := by decide +kernel
private theorem pointValues_09 : valuesCheck coefficients (LockedData.input 49) (pointHistory 49) (pointHistory 50) (pointSaturation 49) 24 = true := by decide +kernel
theorem pointValues : ∀ j : Fin 10,
    let n := 40+j.val; valuesCheck coefficients (LockedData.input n) (pointHistory n) (pointHistory (n+1)) (pointSaturation n) 24 = true := by
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
private theorem smallValues_00 : valuesCheck coefficients (LockedData.input 40) (smallHistory 40) (smallHistory 41) (smallSaturation 40) 24 = true := by decide +kernel
private theorem smallValues_01 : valuesCheck coefficients (LockedData.input 41) (smallHistory 41) (smallHistory 42) (smallSaturation 41) 24 = true := by decide +kernel
private theorem smallValues_02 : valuesCheck coefficients (LockedData.input 42) (smallHistory 42) (smallHistory 43) (smallSaturation 42) 24 = true := by decide +kernel
private theorem smallValues_03 : valuesCheck coefficients (LockedData.input 43) (smallHistory 43) (smallHistory 44) (smallSaturation 43) 24 = true := by decide +kernel
private theorem smallValues_04 : valuesCheck coefficients (LockedData.input 44) (smallHistory 44) (smallHistory 45) (smallSaturation 44) 24 = true := by decide +kernel
private theorem smallValues_05 : valuesCheck coefficients (LockedData.input 45) (smallHistory 45) (smallHistory 46) (smallSaturation 45) 24 = true := by decide +kernel
private theorem smallValues_06 : valuesCheck coefficients (LockedData.input 46) (smallHistory 46) (smallHistory 47) (smallSaturation 46) 24 = true := by decide +kernel
private theorem smallValues_07 : valuesCheck coefficients (LockedData.input 47) (smallHistory 47) (smallHistory 48) (smallSaturation 47) 24 = true := by decide +kernel
private theorem smallValues_08 : valuesCheck coefficients (LockedData.input 48) (smallHistory 48) (smallHistory 49) (smallSaturation 48) 24 = true := by decide +kernel
private theorem smallValues_09 : valuesCheck coefficients (LockedData.input 49) (smallHistory 49) (smallHistory 50) (smallSaturation 49) 24 = true := by decide +kernel
theorem smallValues : ∀ j : Fin 10,
    let n := 40+j.val; valuesCheck coefficients (LockedData.input n) (smallHistory n) (smallHistory (n+1)) (smallSaturation n) 24 = true := by
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
private theorem wholeMatrices_00 : matrixCheck coefficients (matrices 40) (matrices 41) (wholeSaturation 40) = true := by decide +kernel
private theorem wholeMatrices_01 : matrixCheck coefficients (matrices 41) (matrices 42) (wholeSaturation 41) = true := by decide +kernel
private theorem wholeMatrices_02 : matrixCheck coefficients (matrices 42) (matrices 43) (wholeSaturation 42) = true := by decide +kernel
private theorem wholeMatrices_03 : matrixCheck coefficients (matrices 43) (matrices 44) (wholeSaturation 43) = true := by decide +kernel
private theorem wholeMatrices_04 : matrixCheck coefficients (matrices 44) (matrices 45) (wholeSaturation 44) = true := by decide +kernel
private theorem wholeMatrices_05 : matrixCheck coefficients (matrices 45) (matrices 46) (wholeSaturation 45) = true := by decide +kernel
private theorem wholeMatrices_06 : matrixCheck coefficients (matrices 46) (matrices 47) (wholeSaturation 46) = true := by decide +kernel
private theorem wholeMatrices_07 : matrixCheck coefficients (matrices 47) (matrices 48) (wholeSaturation 47) = true := by decide +kernel
private theorem wholeMatrices_08 : matrixCheck coefficients (matrices 48) (matrices 49) (wholeSaturation 48) = true := by decide +kernel
private theorem wholeMatrices_09 : matrixCheck coefficients (matrices 49) (matrices 50) (wholeSaturation 49) = true := by decide +kernel
theorem wholeMatrices : ∀ j : Fin 10,
    let n := 40+j.val; matrixCheck coefficients (matrices n) (matrices (n+1)) (wholeSaturation n) = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck04
