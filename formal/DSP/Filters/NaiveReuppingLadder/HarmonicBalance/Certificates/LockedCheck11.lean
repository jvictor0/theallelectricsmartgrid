import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedData
/-! Generated kernel checks; no native_decide or imported numerical assumptions. -/
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck11
open Enclosure LockedData
set_option maxRecDepth 100000
set_option maxHeartbeats 0
private theorem wholeValues_00 : valuesCheck coefficients (LockedData.input 110) (wholeHistory 110) (wholeHistory 111) (wholeSaturation 110) 24 = true := by decide +kernel
private theorem wholeValues_01 : valuesCheck coefficients (LockedData.input 111) (wholeHistory 111) (wholeHistory 112) (wholeSaturation 111) 24 = true := by decide +kernel
private theorem wholeValues_02 : valuesCheck coefficients (LockedData.input 112) (wholeHistory 112) (wholeHistory 113) (wholeSaturation 112) 24 = true := by decide +kernel
private theorem wholeValues_03 : valuesCheck coefficients (LockedData.input 113) (wholeHistory 113) (wholeHistory 114) (wholeSaturation 113) 24 = true := by decide +kernel
private theorem wholeValues_04 : valuesCheck coefficients (LockedData.input 114) (wholeHistory 114) (wholeHistory 115) (wholeSaturation 114) 24 = true := by decide +kernel
private theorem wholeValues_05 : valuesCheck coefficients (LockedData.input 115) (wholeHistory 115) (wholeHistory 116) (wholeSaturation 115) 24 = true := by decide +kernel
private theorem wholeValues_06 : valuesCheck coefficients (LockedData.input 116) (wholeHistory 116) (wholeHistory 117) (wholeSaturation 116) 24 = true := by decide +kernel
private theorem wholeValues_07 : valuesCheck coefficients (LockedData.input 117) (wholeHistory 117) (wholeHistory 118) (wholeSaturation 117) 24 = true := by decide +kernel
private theorem wholeValues_08 : valuesCheck coefficients (LockedData.input 118) (wholeHistory 118) (wholeHistory 119) (wholeSaturation 118) 24 = true := by decide +kernel
private theorem wholeValues_09 : valuesCheck coefficients (LockedData.input 119) (wholeHistory 119) (wholeHistory 120) (wholeSaturation 119) 24 = true := by decide +kernel
theorem wholeValues : ∀ j : Fin 10,
    let n := 110+j.val; valuesCheck coefficients (LockedData.input n) (wholeHistory n) (wholeHistory (n+1)) (wholeSaturation n) 24 = true := by
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
private theorem pointValues_00 : valuesCheck coefficients (LockedData.input 110) (pointHistory 110) (pointHistory 111) (pointSaturation 110) 24 = true := by decide +kernel
private theorem pointValues_01 : valuesCheck coefficients (LockedData.input 111) (pointHistory 111) (pointHistory 112) (pointSaturation 111) 24 = true := by decide +kernel
private theorem pointValues_02 : valuesCheck coefficients (LockedData.input 112) (pointHistory 112) (pointHistory 113) (pointSaturation 112) 24 = true := by decide +kernel
private theorem pointValues_03 : valuesCheck coefficients (LockedData.input 113) (pointHistory 113) (pointHistory 114) (pointSaturation 113) 24 = true := by decide +kernel
private theorem pointValues_04 : valuesCheck coefficients (LockedData.input 114) (pointHistory 114) (pointHistory 115) (pointSaturation 114) 24 = true := by decide +kernel
private theorem pointValues_05 : valuesCheck coefficients (LockedData.input 115) (pointHistory 115) (pointHistory 116) (pointSaturation 115) 24 = true := by decide +kernel
private theorem pointValues_06 : valuesCheck coefficients (LockedData.input 116) (pointHistory 116) (pointHistory 117) (pointSaturation 116) 24 = true := by decide +kernel
private theorem pointValues_07 : valuesCheck coefficients (LockedData.input 117) (pointHistory 117) (pointHistory 118) (pointSaturation 117) 24 = true := by decide +kernel
private theorem pointValues_08 : valuesCheck coefficients (LockedData.input 118) (pointHistory 118) (pointHistory 119) (pointSaturation 118) 24 = true := by decide +kernel
private theorem pointValues_09 : valuesCheck coefficients (LockedData.input 119) (pointHistory 119) (pointHistory 120) (pointSaturation 119) 24 = true := by decide +kernel
theorem pointValues : ∀ j : Fin 10,
    let n := 110+j.val; valuesCheck coefficients (LockedData.input n) (pointHistory n) (pointHistory (n+1)) (pointSaturation n) 24 = true := by
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
private theorem smallValues_00 : valuesCheck coefficients (LockedData.input 110) (smallHistory 110) (smallHistory 111) (smallSaturation 110) 24 = true := by decide +kernel
private theorem smallValues_01 : valuesCheck coefficients (LockedData.input 111) (smallHistory 111) (smallHistory 112) (smallSaturation 111) 24 = true := by decide +kernel
private theorem smallValues_02 : valuesCheck coefficients (LockedData.input 112) (smallHistory 112) (smallHistory 113) (smallSaturation 112) 24 = true := by decide +kernel
private theorem smallValues_03 : valuesCheck coefficients (LockedData.input 113) (smallHistory 113) (smallHistory 114) (smallSaturation 113) 24 = true := by decide +kernel
private theorem smallValues_04 : valuesCheck coefficients (LockedData.input 114) (smallHistory 114) (smallHistory 115) (smallSaturation 114) 24 = true := by decide +kernel
private theorem smallValues_05 : valuesCheck coefficients (LockedData.input 115) (smallHistory 115) (smallHistory 116) (smallSaturation 115) 24 = true := by decide +kernel
private theorem smallValues_06 : valuesCheck coefficients (LockedData.input 116) (smallHistory 116) (smallHistory 117) (smallSaturation 116) 24 = true := by decide +kernel
private theorem smallValues_07 : valuesCheck coefficients (LockedData.input 117) (smallHistory 117) (smallHistory 118) (smallSaturation 117) 24 = true := by decide +kernel
private theorem smallValues_08 : valuesCheck coefficients (LockedData.input 118) (smallHistory 118) (smallHistory 119) (smallSaturation 118) 24 = true := by decide +kernel
private theorem smallValues_09 : valuesCheck coefficients (LockedData.input 119) (smallHistory 119) (smallHistory 120) (smallSaturation 119) 24 = true := by decide +kernel
theorem smallValues : ∀ j : Fin 10,
    let n := 110+j.val; valuesCheck coefficients (LockedData.input n) (smallHistory n) (smallHistory (n+1)) (smallSaturation n) 24 = true := by
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
private theorem wholeMatrices_00 : matrixCheck coefficients (matrices 110) (matrices 111) (wholeSaturation 110) = true := by decide +kernel
private theorem wholeMatrices_01 : matrixCheck coefficients (matrices 111) (matrices 112) (wholeSaturation 111) = true := by decide +kernel
private theorem wholeMatrices_02 : matrixCheck coefficients (matrices 112) (matrices 113) (wholeSaturation 112) = true := by decide +kernel
private theorem wholeMatrices_03 : matrixCheck coefficients (matrices 113) (matrices 114) (wholeSaturation 113) = true := by decide +kernel
private theorem wholeMatrices_04 : matrixCheck coefficients (matrices 114) (matrices 115) (wholeSaturation 114) = true := by decide +kernel
private theorem wholeMatrices_05 : matrixCheck coefficients (matrices 115) (matrices 116) (wholeSaturation 115) = true := by decide +kernel
private theorem wholeMatrices_06 : matrixCheck coefficients (matrices 116) (matrices 117) (wholeSaturation 116) = true := by decide +kernel
private theorem wholeMatrices_07 : matrixCheck coefficients (matrices 117) (matrices 118) (wholeSaturation 117) = true := by decide +kernel
private theorem wholeMatrices_08 : matrixCheck coefficients (matrices 118) (matrices 119) (wholeSaturation 118) = true := by decide +kernel
private theorem wholeMatrices_09 : matrixCheck coefficients (matrices 119) (matrices 120) (wholeSaturation 119) = true := by decide +kernel
theorem wholeMatrices : ∀ j : Fin 10,
    let n := 110+j.val; matrixCheck coefficients (matrices n) (matrices (n+1)) (wholeSaturation n) = true := by
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
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck11
