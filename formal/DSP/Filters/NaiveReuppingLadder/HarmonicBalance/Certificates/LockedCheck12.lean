import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedData
/-! Generated kernel checks; no native_decide or imported numerical assumptions. -/
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck12
open Enclosure LockedData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem wholeValues_00 : valuesCheck coefficients (LockedData.input 120) (wholeHistory 120) (wholeHistory 121) (wholeSaturation 120) 24 = true := by decide +kernel
private theorem wholeValues_01 : valuesCheck coefficients (LockedData.input 121) (wholeHistory 121) (wholeHistory 122) (wholeSaturation 121) 24 = true := by decide +kernel
private theorem wholeValues_02 : valuesCheck coefficients (LockedData.input 122) (wholeHistory 122) (wholeHistory 123) (wholeSaturation 122) 24 = true := by decide +kernel
private theorem wholeValues_03 : valuesCheck coefficients (LockedData.input 123) (wholeHistory 123) (wholeHistory 124) (wholeSaturation 123) 24 = true := by decide +kernel
private theorem wholeValues_04 : valuesCheck coefficients (LockedData.input 124) (wholeHistory 124) (wholeHistory 125) (wholeSaturation 124) 24 = true := by decide +kernel
private theorem wholeValues_05 : valuesCheck coefficients (LockedData.input 125) (wholeHistory 125) (wholeHistory 126) (wholeSaturation 125) 24 = true := by decide +kernel
private theorem wholeValues_06 : valuesCheck coefficients (LockedData.input 126) (wholeHistory 126) (wholeHistory 127) (wholeSaturation 126) 24 = true := by decide +kernel
private theorem wholeValues_07 : valuesCheck coefficients (LockedData.input 127) (wholeHistory 127) (wholeHistory 128) (wholeSaturation 127) 24 = true := by decide +kernel
private theorem wholeValues_08 : valuesCheck coefficients (LockedData.input 128) (wholeHistory 128) (wholeHistory 129) (wholeSaturation 128) 24 = true := by decide +kernel
private theorem wholeValues_09 : valuesCheck coefficients (LockedData.input 129) (wholeHistory 129) (wholeHistory 130) (wholeSaturation 129) 24 = true := by decide +kernel
theorem wholeValues : ∀ j : Fin 10,
    let n := 120+j.val; valuesCheck coefficients (LockedData.input n) (wholeHistory n) (wholeHistory (n+1)) (wholeSaturation n) 24 = true := by
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
private theorem pointValues_00 : valuesCheck coefficients (LockedData.input 120) (pointHistory 120) (pointHistory 121) (pointSaturation 120) 24 = true := by decide +kernel
private theorem pointValues_01 : valuesCheck coefficients (LockedData.input 121) (pointHistory 121) (pointHistory 122) (pointSaturation 121) 24 = true := by decide +kernel
private theorem pointValues_02 : valuesCheck coefficients (LockedData.input 122) (pointHistory 122) (pointHistory 123) (pointSaturation 122) 24 = true := by decide +kernel
private theorem pointValues_03 : valuesCheck coefficients (LockedData.input 123) (pointHistory 123) (pointHistory 124) (pointSaturation 123) 24 = true := by decide +kernel
private theorem pointValues_04 : valuesCheck coefficients (LockedData.input 124) (pointHistory 124) (pointHistory 125) (pointSaturation 124) 24 = true := by decide +kernel
private theorem pointValues_05 : valuesCheck coefficients (LockedData.input 125) (pointHistory 125) (pointHistory 126) (pointSaturation 125) 24 = true := by decide +kernel
private theorem pointValues_06 : valuesCheck coefficients (LockedData.input 126) (pointHistory 126) (pointHistory 127) (pointSaturation 126) 24 = true := by decide +kernel
private theorem pointValues_07 : valuesCheck coefficients (LockedData.input 127) (pointHistory 127) (pointHistory 128) (pointSaturation 127) 24 = true := by decide +kernel
private theorem pointValues_08 : valuesCheck coefficients (LockedData.input 128) (pointHistory 128) (pointHistory 129) (pointSaturation 128) 24 = true := by decide +kernel
private theorem pointValues_09 : valuesCheck coefficients (LockedData.input 129) (pointHistory 129) (pointHistory 130) (pointSaturation 129) 24 = true := by decide +kernel
theorem pointValues : ∀ j : Fin 10,
    let n := 120+j.val; valuesCheck coefficients (LockedData.input n) (pointHistory n) (pointHistory (n+1)) (pointSaturation n) 24 = true := by
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
private theorem smallValues_00 : valuesCheck coefficients (LockedData.input 120) (smallHistory 120) (smallHistory 121) (smallSaturation 120) 24 = true := by decide +kernel
private theorem smallValues_01 : valuesCheck coefficients (LockedData.input 121) (smallHistory 121) (smallHistory 122) (smallSaturation 121) 24 = true := by decide +kernel
private theorem smallValues_02 : valuesCheck coefficients (LockedData.input 122) (smallHistory 122) (smallHistory 123) (smallSaturation 122) 24 = true := by decide +kernel
private theorem smallValues_03 : valuesCheck coefficients (LockedData.input 123) (smallHistory 123) (smallHistory 124) (smallSaturation 123) 24 = true := by decide +kernel
private theorem smallValues_04 : valuesCheck coefficients (LockedData.input 124) (smallHistory 124) (smallHistory 125) (smallSaturation 124) 24 = true := by decide +kernel
private theorem smallValues_05 : valuesCheck coefficients (LockedData.input 125) (smallHistory 125) (smallHistory 126) (smallSaturation 125) 24 = true := by decide +kernel
private theorem smallValues_06 : valuesCheck coefficients (LockedData.input 126) (smallHistory 126) (smallHistory 127) (smallSaturation 126) 24 = true := by decide +kernel
private theorem smallValues_07 : valuesCheck coefficients (LockedData.input 127) (smallHistory 127) (smallHistory 128) (smallSaturation 127) 24 = true := by decide +kernel
private theorem smallValues_08 : valuesCheck coefficients (LockedData.input 128) (smallHistory 128) (smallHistory 129) (smallSaturation 128) 24 = true := by decide +kernel
private theorem smallValues_09 : valuesCheck coefficients (LockedData.input 129) (smallHistory 129) (smallHistory 130) (smallSaturation 129) 24 = true := by decide +kernel
theorem smallValues : ∀ j : Fin 10,
    let n := 120+j.val; valuesCheck coefficients (LockedData.input n) (smallHistory n) (smallHistory (n+1)) (smallSaturation n) 24 = true := by
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
private theorem wholeMatrices_00 : matrixCheck coefficients (matrices 120) (matrices 121) (wholeSaturation 120) = true := by decide +kernel
private theorem wholeMatrices_01 : matrixCheck coefficients (matrices 121) (matrices 122) (wholeSaturation 121) = true := by decide +kernel
private theorem wholeMatrices_02 : matrixCheck coefficients (matrices 122) (matrices 123) (wholeSaturation 122) = true := by decide +kernel
private theorem wholeMatrices_03 : matrixCheck coefficients (matrices 123) (matrices 124) (wholeSaturation 123) = true := by decide +kernel
private theorem wholeMatrices_04 : matrixCheck coefficients (matrices 124) (matrices 125) (wholeSaturation 124) = true := by decide +kernel
private theorem wholeMatrices_05 : matrixCheck coefficients (matrices 125) (matrices 126) (wholeSaturation 125) = true := by decide +kernel
private theorem wholeMatrices_06 : matrixCheck coefficients (matrices 126) (matrices 127) (wholeSaturation 126) = true := by decide +kernel
private theorem wholeMatrices_07 : matrixCheck coefficients (matrices 127) (matrices 128) (wholeSaturation 127) = true := by decide +kernel
private theorem wholeMatrices_08 : matrixCheck coefficients (matrices 128) (matrices 129) (wholeSaturation 128) = true := by decide +kernel
private theorem wholeMatrices_09 : matrixCheck coefficients (matrices 129) (matrices 130) (wholeSaturation 129) = true := by decide +kernel
theorem wholeMatrices : ∀ j : Fin 10,
    let n := 120+j.val; matrixCheck coefficients (matrices n) (matrices (n+1)) (wholeSaturation n) = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck12
