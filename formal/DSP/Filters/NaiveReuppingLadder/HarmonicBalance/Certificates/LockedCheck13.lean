import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedData
/-! Generated kernel checks; no native_decide or imported numerical assumptions. -/
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck13
open Enclosure LockedData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem wholeValues_00 : valuesCheck coefficients (LockedData.input 130) (wholeHistory 130) (wholeHistory 131) (wholeSaturation 130) 24 = true := by decide +kernel
private theorem wholeValues_01 : valuesCheck coefficients (LockedData.input 131) (wholeHistory 131) (wholeHistory 132) (wholeSaturation 131) 24 = true := by decide +kernel
private theorem wholeValues_02 : valuesCheck coefficients (LockedData.input 132) (wholeHistory 132) (wholeHistory 133) (wholeSaturation 132) 24 = true := by decide +kernel
private theorem wholeValues_03 : valuesCheck coefficients (LockedData.input 133) (wholeHistory 133) (wholeHistory 134) (wholeSaturation 133) 24 = true := by decide +kernel
private theorem wholeValues_04 : valuesCheck coefficients (LockedData.input 134) (wholeHistory 134) (wholeHistory 135) (wholeSaturation 134) 24 = true := by decide +kernel
private theorem wholeValues_05 : valuesCheck coefficients (LockedData.input 135) (wholeHistory 135) (wholeHistory 136) (wholeSaturation 135) 24 = true := by decide +kernel
private theorem wholeValues_06 : valuesCheck coefficients (LockedData.input 136) (wholeHistory 136) (wholeHistory 137) (wholeSaturation 136) 24 = true := by decide +kernel
private theorem wholeValues_07 : valuesCheck coefficients (LockedData.input 137) (wholeHistory 137) (wholeHistory 138) (wholeSaturation 137) 24 = true := by decide +kernel
private theorem wholeValues_08 : valuesCheck coefficients (LockedData.input 138) (wholeHistory 138) (wholeHistory 139) (wholeSaturation 138) 24 = true := by decide +kernel
private theorem wholeValues_09 : valuesCheck coefficients (LockedData.input 139) (wholeHistory 139) (wholeHistory 140) (wholeSaturation 139) 24 = true := by decide +kernel
theorem wholeValues : ∀ j : Fin 10,
    let n := 130+j.val; valuesCheck coefficients (LockedData.input n) (wholeHistory n) (wholeHistory (n+1)) (wholeSaturation n) 24 = true := by
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
private theorem pointValues_00 : valuesCheck coefficients (LockedData.input 130) (pointHistory 130) (pointHistory 131) (pointSaturation 130) 24 = true := by decide +kernel
private theorem pointValues_01 : valuesCheck coefficients (LockedData.input 131) (pointHistory 131) (pointHistory 132) (pointSaturation 131) 24 = true := by decide +kernel
private theorem pointValues_02 : valuesCheck coefficients (LockedData.input 132) (pointHistory 132) (pointHistory 133) (pointSaturation 132) 24 = true := by decide +kernel
private theorem pointValues_03 : valuesCheck coefficients (LockedData.input 133) (pointHistory 133) (pointHistory 134) (pointSaturation 133) 24 = true := by decide +kernel
private theorem pointValues_04 : valuesCheck coefficients (LockedData.input 134) (pointHistory 134) (pointHistory 135) (pointSaturation 134) 24 = true := by decide +kernel
private theorem pointValues_05 : valuesCheck coefficients (LockedData.input 135) (pointHistory 135) (pointHistory 136) (pointSaturation 135) 24 = true := by decide +kernel
private theorem pointValues_06 : valuesCheck coefficients (LockedData.input 136) (pointHistory 136) (pointHistory 137) (pointSaturation 136) 24 = true := by decide +kernel
private theorem pointValues_07 : valuesCheck coefficients (LockedData.input 137) (pointHistory 137) (pointHistory 138) (pointSaturation 137) 24 = true := by decide +kernel
private theorem pointValues_08 : valuesCheck coefficients (LockedData.input 138) (pointHistory 138) (pointHistory 139) (pointSaturation 138) 24 = true := by decide +kernel
private theorem pointValues_09 : valuesCheck coefficients (LockedData.input 139) (pointHistory 139) (pointHistory 140) (pointSaturation 139) 24 = true := by decide +kernel
theorem pointValues : ∀ j : Fin 10,
    let n := 130+j.val; valuesCheck coefficients (LockedData.input n) (pointHistory n) (pointHistory (n+1)) (pointSaturation n) 24 = true := by
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
private theorem smallValues_00 : valuesCheck coefficients (LockedData.input 130) (smallHistory 130) (smallHistory 131) (smallSaturation 130) 24 = true := by decide +kernel
private theorem smallValues_01 : valuesCheck coefficients (LockedData.input 131) (smallHistory 131) (smallHistory 132) (smallSaturation 131) 24 = true := by decide +kernel
private theorem smallValues_02 : valuesCheck coefficients (LockedData.input 132) (smallHistory 132) (smallHistory 133) (smallSaturation 132) 24 = true := by decide +kernel
private theorem smallValues_03 : valuesCheck coefficients (LockedData.input 133) (smallHistory 133) (smallHistory 134) (smallSaturation 133) 24 = true := by decide +kernel
private theorem smallValues_04 : valuesCheck coefficients (LockedData.input 134) (smallHistory 134) (smallHistory 135) (smallSaturation 134) 24 = true := by decide +kernel
private theorem smallValues_05 : valuesCheck coefficients (LockedData.input 135) (smallHistory 135) (smallHistory 136) (smallSaturation 135) 24 = true := by decide +kernel
private theorem smallValues_06 : valuesCheck coefficients (LockedData.input 136) (smallHistory 136) (smallHistory 137) (smallSaturation 136) 24 = true := by decide +kernel
private theorem smallValues_07 : valuesCheck coefficients (LockedData.input 137) (smallHistory 137) (smallHistory 138) (smallSaturation 137) 24 = true := by decide +kernel
private theorem smallValues_08 : valuesCheck coefficients (LockedData.input 138) (smallHistory 138) (smallHistory 139) (smallSaturation 138) 24 = true := by decide +kernel
private theorem smallValues_09 : valuesCheck coefficients (LockedData.input 139) (smallHistory 139) (smallHistory 140) (smallSaturation 139) 24 = true := by decide +kernel
theorem smallValues : ∀ j : Fin 10,
    let n := 130+j.val; valuesCheck coefficients (LockedData.input n) (smallHistory n) (smallHistory (n+1)) (smallSaturation n) 24 = true := by
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
private theorem wholeMatrices_00 : matrixCheck coefficients (matrices 130) (matrices 131) (wholeSaturation 130) = true := by decide +kernel
private theorem wholeMatrices_01 : matrixCheck coefficients (matrices 131) (matrices 132) (wholeSaturation 131) = true := by decide +kernel
private theorem wholeMatrices_02 : matrixCheck coefficients (matrices 132) (matrices 133) (wholeSaturation 132) = true := by decide +kernel
private theorem wholeMatrices_03 : matrixCheck coefficients (matrices 133) (matrices 134) (wholeSaturation 133) = true := by decide +kernel
private theorem wholeMatrices_04 : matrixCheck coefficients (matrices 134) (matrices 135) (wholeSaturation 134) = true := by decide +kernel
private theorem wholeMatrices_05 : matrixCheck coefficients (matrices 135) (matrices 136) (wholeSaturation 135) = true := by decide +kernel
private theorem wholeMatrices_06 : matrixCheck coefficients (matrices 136) (matrices 137) (wholeSaturation 136) = true := by decide +kernel
private theorem wholeMatrices_07 : matrixCheck coefficients (matrices 137) (matrices 138) (wholeSaturation 137) = true := by decide +kernel
private theorem wholeMatrices_08 : matrixCheck coefficients (matrices 138) (matrices 139) (wholeSaturation 138) = true := by decide +kernel
private theorem wholeMatrices_09 : matrixCheck coefficients (matrices 139) (matrices 140) (wholeSaturation 139) = true := by decide +kernel
theorem wholeMatrices : ∀ j : Fin 10,
    let n := 130+j.val; matrixCheck coefficients (matrices n) (matrices (n+1)) (wholeSaturation n) = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck13
