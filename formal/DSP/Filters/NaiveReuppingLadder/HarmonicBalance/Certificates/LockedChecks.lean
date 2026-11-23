import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck00
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck01
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck02
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck03
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck04
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck05
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck06
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck07
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck08
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck09
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck10
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck11
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck12
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck13
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck14
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck15
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedCheck16
/-! Collect the checked chunks into whole-period certificates. -/
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedChecks
open Enclosure LockedData
private theorem from_chunks (P : ℕ → Prop) (h : ∀ i : Fin 17, ∀ j : Fin 10, P (10*i.val+j.val))
    (n : ℕ) (hn : n < 170) : P n := by
  have hi : n/10 < 17 := by omega
  have hj : n%10 < 10 := Nat.mod_lt _ (by norm_num)
  have he : 10*(n/10)+n%10 = n := by omega
  simpa only [he] using h ⟨n/10,hi⟩ ⟨n%10,hj⟩
theorem wholeValues : ∀ n < 170, valuesCheck coefficients (LockedData.input n) (wholeHistory n) (wholeHistory (n+1)) (wholeSaturation n) 24 = true := by
  apply from_chunks
  intro i
  fin_cases i
  · exact LockedCheck00.wholeValues
  · exact LockedCheck01.wholeValues
  · exact LockedCheck02.wholeValues
  · exact LockedCheck03.wholeValues
  · exact LockedCheck04.wholeValues
  · exact LockedCheck05.wholeValues
  · exact LockedCheck06.wholeValues
  · exact LockedCheck07.wholeValues
  · exact LockedCheck08.wholeValues
  · exact LockedCheck09.wholeValues
  · exact LockedCheck10.wholeValues
  · exact LockedCheck11.wholeValues
  · exact LockedCheck12.wholeValues
  · exact LockedCheck13.wholeValues
  · exact LockedCheck14.wholeValues
  · exact LockedCheck15.wholeValues
  · exact LockedCheck16.wholeValues
theorem pointValues : ∀ n < 170, valuesCheck coefficients (LockedData.input n) (pointHistory n) (pointHistory (n+1)) (pointSaturation n) 24 = true := by
  apply from_chunks
  intro i
  fin_cases i
  · exact LockedCheck00.pointValues
  · exact LockedCheck01.pointValues
  · exact LockedCheck02.pointValues
  · exact LockedCheck03.pointValues
  · exact LockedCheck04.pointValues
  · exact LockedCheck05.pointValues
  · exact LockedCheck06.pointValues
  · exact LockedCheck07.pointValues
  · exact LockedCheck08.pointValues
  · exact LockedCheck09.pointValues
  · exact LockedCheck10.pointValues
  · exact LockedCheck11.pointValues
  · exact LockedCheck12.pointValues
  · exact LockedCheck13.pointValues
  · exact LockedCheck14.pointValues
  · exact LockedCheck15.pointValues
  · exact LockedCheck16.pointValues
theorem smallValues : ∀ n < 170, valuesCheck coefficients (LockedData.input n) (smallHistory n) (smallHistory (n+1)) (smallSaturation n) 24 = true := by
  apply from_chunks
  intro i
  fin_cases i
  · exact LockedCheck00.smallValues
  · exact LockedCheck01.smallValues
  · exact LockedCheck02.smallValues
  · exact LockedCheck03.smallValues
  · exact LockedCheck04.smallValues
  · exact LockedCheck05.smallValues
  · exact LockedCheck06.smallValues
  · exact LockedCheck07.smallValues
  · exact LockedCheck08.smallValues
  · exact LockedCheck09.smallValues
  · exact LockedCheck10.smallValues
  · exact LockedCheck11.smallValues
  · exact LockedCheck12.smallValues
  · exact LockedCheck13.smallValues
  · exact LockedCheck14.smallValues
  · exact LockedCheck15.smallValues
  · exact LockedCheck16.smallValues
theorem wholeMatrices : ∀ n < 170, matrixCheck coefficients (matrices n) (matrices (n+1)) (wholeSaturation n) = true := by
  apply from_chunks
  intro i
  fin_cases i
  · exact LockedCheck00.wholeMatrices
  · exact LockedCheck01.wholeMatrices
  · exact LockedCheck02.wholeMatrices
  · exact LockedCheck03.wholeMatrices
  · exact LockedCheck04.wholeMatrices
  · exact LockedCheck05.wholeMatrices
  · exact LockedCheck06.wholeMatrices
  · exact LockedCheck07.wholeMatrices
  · exact LockedCheck08.wholeMatrices
  · exact LockedCheck09.wholeMatrices
  · exact LockedCheck10.wholeMatrices
  · exact LockedCheck11.wholeMatrices
  · exact LockedCheck12.wholeMatrices
  · exact LockedCheck13.wholeMatrices
  · exact LockedCheck14.wholeMatrices
  · exact LockedCheck15.wholeMatrices
  · exact LockedCheck16.wholeMatrices
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedChecks
