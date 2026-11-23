import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck00
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck01
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck02
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck03
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck04
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck05
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck06
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck07
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck08
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck09
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck10
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck11
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck12
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck13
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck14
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck15
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCheck16
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockChecks
open Enclosure DriftBlockCellData
theorem steps : ∀ n < 162, centeredCheck LockedData.coefficients (DriftBlockCellData.input n) (center n) (center (n+1)) (centerSaturation n) (whole (n+1)) (DriftBlockCellData.saturation n) (matrices n) (matrices (n+1)) width 24 = true := by
  intro n hn
  interval_cases n
  · exact DriftBlockCheck00.steps ⟨0,by decide⟩
  · exact DriftBlockCheck00.steps ⟨1,by decide⟩
  · exact DriftBlockCheck00.steps ⟨2,by decide⟩
  · exact DriftBlockCheck00.steps ⟨3,by decide⟩
  · exact DriftBlockCheck00.steps ⟨4,by decide⟩
  · exact DriftBlockCheck00.steps ⟨5,by decide⟩
  · exact DriftBlockCheck00.steps ⟨6,by decide⟩
  · exact DriftBlockCheck00.steps ⟨7,by decide⟩
  · exact DriftBlockCheck00.steps ⟨8,by decide⟩
  · exact DriftBlockCheck00.steps ⟨9,by decide⟩
  · exact DriftBlockCheck01.steps ⟨0,by decide⟩
  · exact DriftBlockCheck01.steps ⟨1,by decide⟩
  · exact DriftBlockCheck01.steps ⟨2,by decide⟩
  · exact DriftBlockCheck01.steps ⟨3,by decide⟩
  · exact DriftBlockCheck01.steps ⟨4,by decide⟩
  · exact DriftBlockCheck01.steps ⟨5,by decide⟩
  · exact DriftBlockCheck01.steps ⟨6,by decide⟩
  · exact DriftBlockCheck01.steps ⟨7,by decide⟩
  · exact DriftBlockCheck01.steps ⟨8,by decide⟩
  · exact DriftBlockCheck01.steps ⟨9,by decide⟩
  · exact DriftBlockCheck02.steps ⟨0,by decide⟩
  · exact DriftBlockCheck02.steps ⟨1,by decide⟩
  · exact DriftBlockCheck02.steps ⟨2,by decide⟩
  · exact DriftBlockCheck02.steps ⟨3,by decide⟩
  · exact DriftBlockCheck02.steps ⟨4,by decide⟩
  · exact DriftBlockCheck02.steps ⟨5,by decide⟩
  · exact DriftBlockCheck02.steps ⟨6,by decide⟩
  · exact DriftBlockCheck02.steps ⟨7,by decide⟩
  · exact DriftBlockCheck02.steps ⟨8,by decide⟩
  · exact DriftBlockCheck02.steps ⟨9,by decide⟩
  · exact DriftBlockCheck03.steps ⟨0,by decide⟩
  · exact DriftBlockCheck03.steps ⟨1,by decide⟩
  · exact DriftBlockCheck03.steps ⟨2,by decide⟩
  · exact DriftBlockCheck03.steps ⟨3,by decide⟩
  · exact DriftBlockCheck03.steps ⟨4,by decide⟩
  · exact DriftBlockCheck03.steps ⟨5,by decide⟩
  · exact DriftBlockCheck03.steps ⟨6,by decide⟩
  · exact DriftBlockCheck03.steps ⟨7,by decide⟩
  · exact DriftBlockCheck03.steps ⟨8,by decide⟩
  · exact DriftBlockCheck03.steps ⟨9,by decide⟩
  · exact DriftBlockCheck04.steps ⟨0,by decide⟩
  · exact DriftBlockCheck04.steps ⟨1,by decide⟩
  · exact DriftBlockCheck04.steps ⟨2,by decide⟩
  · exact DriftBlockCheck04.steps ⟨3,by decide⟩
  · exact DriftBlockCheck04.steps ⟨4,by decide⟩
  · exact DriftBlockCheck04.steps ⟨5,by decide⟩
  · exact DriftBlockCheck04.steps ⟨6,by decide⟩
  · exact DriftBlockCheck04.steps ⟨7,by decide⟩
  · exact DriftBlockCheck04.steps ⟨8,by decide⟩
  · exact DriftBlockCheck04.steps ⟨9,by decide⟩
  · exact DriftBlockCheck05.steps ⟨0,by decide⟩
  · exact DriftBlockCheck05.steps ⟨1,by decide⟩
  · exact DriftBlockCheck05.steps ⟨2,by decide⟩
  · exact DriftBlockCheck05.steps ⟨3,by decide⟩
  · exact DriftBlockCheck05.steps ⟨4,by decide⟩
  · exact DriftBlockCheck05.steps ⟨5,by decide⟩
  · exact DriftBlockCheck05.steps ⟨6,by decide⟩
  · exact DriftBlockCheck05.steps ⟨7,by decide⟩
  · exact DriftBlockCheck05.steps ⟨8,by decide⟩
  · exact DriftBlockCheck05.steps ⟨9,by decide⟩
  · exact DriftBlockCheck06.steps ⟨0,by decide⟩
  · exact DriftBlockCheck06.steps ⟨1,by decide⟩
  · exact DriftBlockCheck06.steps ⟨2,by decide⟩
  · exact DriftBlockCheck06.steps ⟨3,by decide⟩
  · exact DriftBlockCheck06.steps ⟨4,by decide⟩
  · exact DriftBlockCheck06.steps ⟨5,by decide⟩
  · exact DriftBlockCheck06.steps ⟨6,by decide⟩
  · exact DriftBlockCheck06.steps ⟨7,by decide⟩
  · exact DriftBlockCheck06.steps ⟨8,by decide⟩
  · exact DriftBlockCheck06.steps ⟨9,by decide⟩
  · exact DriftBlockCheck07.steps ⟨0,by decide⟩
  · exact DriftBlockCheck07.steps ⟨1,by decide⟩
  · exact DriftBlockCheck07.steps ⟨2,by decide⟩
  · exact DriftBlockCheck07.steps ⟨3,by decide⟩
  · exact DriftBlockCheck07.steps ⟨4,by decide⟩
  · exact DriftBlockCheck07.steps ⟨5,by decide⟩
  · exact DriftBlockCheck07.steps ⟨6,by decide⟩
  · exact DriftBlockCheck07.steps ⟨7,by decide⟩
  · exact DriftBlockCheck07.steps ⟨8,by decide⟩
  · exact DriftBlockCheck07.steps ⟨9,by decide⟩
  · exact DriftBlockCheck08.steps ⟨0,by decide⟩
  · exact DriftBlockCheck08.steps ⟨1,by decide⟩
  · exact DriftBlockCheck08.steps ⟨2,by decide⟩
  · exact DriftBlockCheck08.steps ⟨3,by decide⟩
  · exact DriftBlockCheck08.steps ⟨4,by decide⟩
  · exact DriftBlockCheck08.steps ⟨5,by decide⟩
  · exact DriftBlockCheck08.steps ⟨6,by decide⟩
  · exact DriftBlockCheck08.steps ⟨7,by decide⟩
  · exact DriftBlockCheck08.steps ⟨8,by decide⟩
  · exact DriftBlockCheck08.steps ⟨9,by decide⟩
  · exact DriftBlockCheck09.steps ⟨0,by decide⟩
  · exact DriftBlockCheck09.steps ⟨1,by decide⟩
  · exact DriftBlockCheck09.steps ⟨2,by decide⟩
  · exact DriftBlockCheck09.steps ⟨3,by decide⟩
  · exact DriftBlockCheck09.steps ⟨4,by decide⟩
  · exact DriftBlockCheck09.steps ⟨5,by decide⟩
  · exact DriftBlockCheck09.steps ⟨6,by decide⟩
  · exact DriftBlockCheck09.steps ⟨7,by decide⟩
  · exact DriftBlockCheck09.steps ⟨8,by decide⟩
  · exact DriftBlockCheck09.steps ⟨9,by decide⟩
  · exact DriftBlockCheck10.steps ⟨0,by decide⟩
  · exact DriftBlockCheck10.steps ⟨1,by decide⟩
  · exact DriftBlockCheck10.steps ⟨2,by decide⟩
  · exact DriftBlockCheck10.steps ⟨3,by decide⟩
  · exact DriftBlockCheck10.steps ⟨4,by decide⟩
  · exact DriftBlockCheck10.steps ⟨5,by decide⟩
  · exact DriftBlockCheck10.steps ⟨6,by decide⟩
  · exact DriftBlockCheck10.steps ⟨7,by decide⟩
  · exact DriftBlockCheck10.steps ⟨8,by decide⟩
  · exact DriftBlockCheck10.steps ⟨9,by decide⟩
  · exact DriftBlockCheck11.steps ⟨0,by decide⟩
  · exact DriftBlockCheck11.steps ⟨1,by decide⟩
  · exact DriftBlockCheck11.steps ⟨2,by decide⟩
  · exact DriftBlockCheck11.steps ⟨3,by decide⟩
  · exact DriftBlockCheck11.steps ⟨4,by decide⟩
  · exact DriftBlockCheck11.steps ⟨5,by decide⟩
  · exact DriftBlockCheck11.steps ⟨6,by decide⟩
  · exact DriftBlockCheck11.steps ⟨7,by decide⟩
  · exact DriftBlockCheck11.steps ⟨8,by decide⟩
  · exact DriftBlockCheck11.steps ⟨9,by decide⟩
  · exact DriftBlockCheck12.steps ⟨0,by decide⟩
  · exact DriftBlockCheck12.steps ⟨1,by decide⟩
  · exact DriftBlockCheck12.steps ⟨2,by decide⟩
  · exact DriftBlockCheck12.steps ⟨3,by decide⟩
  · exact DriftBlockCheck12.steps ⟨4,by decide⟩
  · exact DriftBlockCheck12.steps ⟨5,by decide⟩
  · exact DriftBlockCheck12.steps ⟨6,by decide⟩
  · exact DriftBlockCheck12.steps ⟨7,by decide⟩
  · exact DriftBlockCheck12.steps ⟨8,by decide⟩
  · exact DriftBlockCheck12.steps ⟨9,by decide⟩
  · exact DriftBlockCheck13.steps ⟨0,by decide⟩
  · exact DriftBlockCheck13.steps ⟨1,by decide⟩
  · exact DriftBlockCheck13.steps ⟨2,by decide⟩
  · exact DriftBlockCheck13.steps ⟨3,by decide⟩
  · exact DriftBlockCheck13.steps ⟨4,by decide⟩
  · exact DriftBlockCheck13.steps ⟨5,by decide⟩
  · exact DriftBlockCheck13.steps ⟨6,by decide⟩
  · exact DriftBlockCheck13.steps ⟨7,by decide⟩
  · exact DriftBlockCheck13.steps ⟨8,by decide⟩
  · exact DriftBlockCheck13.steps ⟨9,by decide⟩
  · exact DriftBlockCheck14.steps ⟨0,by decide⟩
  · exact DriftBlockCheck14.steps ⟨1,by decide⟩
  · exact DriftBlockCheck14.steps ⟨2,by decide⟩
  · exact DriftBlockCheck14.steps ⟨3,by decide⟩
  · exact DriftBlockCheck14.steps ⟨4,by decide⟩
  · exact DriftBlockCheck14.steps ⟨5,by decide⟩
  · exact DriftBlockCheck14.steps ⟨6,by decide⟩
  · exact DriftBlockCheck14.steps ⟨7,by decide⟩
  · exact DriftBlockCheck14.steps ⟨8,by decide⟩
  · exact DriftBlockCheck14.steps ⟨9,by decide⟩
  · exact DriftBlockCheck15.steps ⟨0,by decide⟩
  · exact DriftBlockCheck15.steps ⟨1,by decide⟩
  · exact DriftBlockCheck15.steps ⟨2,by decide⟩
  · exact DriftBlockCheck15.steps ⟨3,by decide⟩
  · exact DriftBlockCheck15.steps ⟨4,by decide⟩
  · exact DriftBlockCheck15.steps ⟨5,by decide⟩
  · exact DriftBlockCheck15.steps ⟨6,by decide⟩
  · exact DriftBlockCheck15.steps ⟨7,by decide⟩
  · exact DriftBlockCheck15.steps ⟨8,by decide⟩
  · exact DriftBlockCheck15.steps ⟨9,by decide⟩
  · exact DriftBlockCheck16.steps ⟨0,by decide⟩
  · exact DriftBlockCheck16.steps ⟨1,by decide⟩
end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockChecks
