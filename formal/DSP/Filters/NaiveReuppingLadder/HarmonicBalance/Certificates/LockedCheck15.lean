import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedData
/-! Generated kernel checks; no native_decide or imported numerical assumptions. -/
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck15
open Enclosure LockedData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem wholeValues_00 : valuesCheck coefficients (LockedData.input 150) (wholeHistory 150) (wholeHistory 151) (wholeSaturation 150) 24 = true := by decide +kernel
private theorem wholeValues_01 : valuesCheck coefficients (LockedData.input 151) (wholeHistory 151) (wholeHistory 152) (wholeSaturation 151) 24 = true := by decide +kernel
private theorem wholeValues_02 : valuesCheck coefficients (LockedData.input 152) (wholeHistory 152) (wholeHistory 153) (wholeSaturation 152) 24 = true := by decide +kernel
private theorem wholeValues_03 : valuesCheck coefficients (LockedData.input 153) (wholeHistory 153) (wholeHistory 154) (wholeSaturation 153) 24 = true := by decide +kernel
private theorem wholeValues_04 : valuesCheck coefficients (LockedData.input 154) (wholeHistory 154) (wholeHistory 155) (wholeSaturation 154) 24 = true := by decide +kernel
private theorem wholeValues_05 : valuesCheck coefficients (LockedData.input 155) (wholeHistory 155) (wholeHistory 156) (wholeSaturation 155) 24 = true := by decide +kernel
private theorem wholeValues_06 : valuesCheck coefficients (LockedData.input 156) (wholeHistory 156) (wholeHistory 157) (wholeSaturation 156) 24 = true := by decide +kernel
private theorem wholeValues_07 : valuesCheck coefficients (LockedData.input 157) (wholeHistory 157) (wholeHistory 158) (wholeSaturation 157) 24 = true := by decide +kernel
private theorem wholeValues_08 : valuesCheck coefficients (LockedData.input 158) (wholeHistory 158) (wholeHistory 159) (wholeSaturation 158) 24 = true := by decide +kernel
private theorem wholeValues_09 : valuesCheck coefficients (LockedData.input 159) (wholeHistory 159) (wholeHistory 160) (wholeSaturation 159) 24 = true := by decide +kernel
theorem wholeValues : ∀ j : Fin 10,
    let n := 150+j.val; valuesCheck coefficients (LockedData.input n) (wholeHistory n) (wholeHistory (n+1)) (wholeSaturation n) 24 = true := by
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
private theorem pointValues_00 : valuesCheck coefficients (LockedData.input 150) (pointHistory 150) (pointHistory 151) (pointSaturation 150) 24 = true := by decide +kernel
private theorem pointValues_01 : valuesCheck coefficients (LockedData.input 151) (pointHistory 151) (pointHistory 152) (pointSaturation 151) 24 = true := by decide +kernel
private theorem pointValues_02 : valuesCheck coefficients (LockedData.input 152) (pointHistory 152) (pointHistory 153) (pointSaturation 152) 24 = true := by decide +kernel
private theorem pointValues_03 : valuesCheck coefficients (LockedData.input 153) (pointHistory 153) (pointHistory 154) (pointSaturation 153) 24 = true := by decide +kernel
private theorem pointValues_04 : valuesCheck coefficients (LockedData.input 154) (pointHistory 154) (pointHistory 155) (pointSaturation 154) 24 = true := by decide +kernel
private theorem pointValues_05 : valuesCheck coefficients (LockedData.input 155) (pointHistory 155) (pointHistory 156) (pointSaturation 155) 24 = true := by decide +kernel
private theorem pointValues_06 : valuesCheck coefficients (LockedData.input 156) (pointHistory 156) (pointHistory 157) (pointSaturation 156) 24 = true := by decide +kernel
private theorem pointValues_07 : valuesCheck coefficients (LockedData.input 157) (pointHistory 157) (pointHistory 158) (pointSaturation 157) 24 = true := by decide +kernel
private theorem pointValues_08 : valuesCheck coefficients (LockedData.input 158) (pointHistory 158) (pointHistory 159) (pointSaturation 158) 24 = true := by decide +kernel
private theorem pointValues_09 : valuesCheck coefficients (LockedData.input 159) (pointHistory 159) (pointHistory 160) (pointSaturation 159) 24 = true := by decide +kernel
theorem pointValues : ∀ j : Fin 10,
    let n := 150+j.val; valuesCheck coefficients (LockedData.input n) (pointHistory n) (pointHistory (n+1)) (pointSaturation n) 24 = true := by
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
private theorem smallValues_00 : valuesCheck coefficients (LockedData.input 150) (smallHistory 150) (smallHistory 151) (smallSaturation 150) 24 = true := by decide +kernel
private theorem smallValues_01 : valuesCheck coefficients (LockedData.input 151) (smallHistory 151) (smallHistory 152) (smallSaturation 151) 24 = true := by decide +kernel
private theorem smallValues_02 : valuesCheck coefficients (LockedData.input 152) (smallHistory 152) (smallHistory 153) (smallSaturation 152) 24 = true := by decide +kernel
private theorem smallValues_03 : valuesCheck coefficients (LockedData.input 153) (smallHistory 153) (smallHistory 154) (smallSaturation 153) 24 = true := by decide +kernel
private theorem smallValues_04 : valuesCheck coefficients (LockedData.input 154) (smallHistory 154) (smallHistory 155) (smallSaturation 154) 24 = true := by decide +kernel
private theorem smallValues_05 : valuesCheck coefficients (LockedData.input 155) (smallHistory 155) (smallHistory 156) (smallSaturation 155) 24 = true := by decide +kernel
private theorem smallValues_06 : valuesCheck coefficients (LockedData.input 156) (smallHistory 156) (smallHistory 157) (smallSaturation 156) 24 = true := by decide +kernel
private theorem smallValues_07 : valuesCheck coefficients (LockedData.input 157) (smallHistory 157) (smallHistory 158) (smallSaturation 157) 24 = true := by decide +kernel
private theorem smallValues_08 : valuesCheck coefficients (LockedData.input 158) (smallHistory 158) (smallHistory 159) (smallSaturation 158) 24 = true := by decide +kernel
private theorem smallValues_09 : valuesCheck coefficients (LockedData.input 159) (smallHistory 159) (smallHistory 160) (smallSaturation 159) 24 = true := by decide +kernel
theorem smallValues : ∀ j : Fin 10,
    let n := 150+j.val; valuesCheck coefficients (LockedData.input n) (smallHistory n) (smallHistory (n+1)) (smallSaturation n) 24 = true := by
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
private theorem wholeMatrices_00 : matrixCheck coefficients (matrices 150) (matrices 151) (wholeSaturation 150) = true := by decide +kernel
private theorem wholeMatrices_01 : matrixCheck coefficients (matrices 151) (matrices 152) (wholeSaturation 151) = true := by decide +kernel
private theorem wholeMatrices_02 : matrixCheck coefficients (matrices 152) (matrices 153) (wholeSaturation 152) = true := by decide +kernel
private theorem wholeMatrices_03 : matrixCheck coefficients (matrices 153) (matrices 154) (wholeSaturation 153) = true := by decide +kernel
private theorem wholeMatrices_04 : matrixCheck coefficients (matrices 154) (matrices 155) (wholeSaturation 154) = true := by decide +kernel
private theorem wholeMatrices_05 : matrixCheck coefficients (matrices 155) (matrices 156) (wholeSaturation 155) = true := by decide +kernel
private theorem wholeMatrices_06 : matrixCheck coefficients (matrices 156) (matrices 157) (wholeSaturation 156) = true := by decide +kernel
private theorem wholeMatrices_07 : matrixCheck coefficients (matrices 157) (matrices 158) (wholeSaturation 157) = true := by decide +kernel
private theorem wholeMatrices_08 : matrixCheck coefficients (matrices 158) (matrices 159) (wholeSaturation 158) = true := by decide +kernel
private theorem wholeMatrices_09 : matrixCheck coefficients (matrices 159) (matrices 160) (wholeSaturation 159) = true := by decide +kernel
theorem wholeMatrices : ∀ j : Fin 10,
    let n := 150+j.val; matrixCheck coefficients (matrices n) (matrices (n+1)) (wholeSaturation n) = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck15
