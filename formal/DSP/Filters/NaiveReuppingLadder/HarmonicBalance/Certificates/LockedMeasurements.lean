import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedOrbit
import DSP.ResponseMeasurement.IntervalBounds

/-! Certified coherent and total powers of the exact locked orbit, including
the final saturator and the one-sample output convention. The approximation
values are fixed rational proposals; their error is bounded against the exact
sample orbit without treating a truncated balance solution as exact. -/

namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedMeasurements
open Enclosure LockedData LockedCoordinates LockedParameters LeanCert.Core DSP.IntervalArithmetic
open DSP.ResponseMeasurement Metric
set_option maxRecDepth 100000
set_option maxHeartbeats 0

noncomputable def output (s : NaiveReuppingLadder ℝ) (x : Vector) (n : ℕ) : ℝ :=
  audioObservation s (Step.states (audioStep s) forcing x (n+1))
noncomputable def carrier : ℂ := Complex.exp ((delta:ℂ)*Complex.I)
def samples : Fin 170 → IntervalRat := fun n => outputRange coefficients (smallSaturation n.val 3)
def cosine : Fin 170 → IntervalRat := fun n => LockedData.carrier n.val 0
def sine : Fin 170 → IntervalRat := fun n => LockedData.carrier n.val 1

def coherentBound : IntervalRat := ⟨0.35523868, 0.35523877, by norm_num⟩
def totalBound : IntervalRat := ⟨4.97991252, 4.97991274, by norm_num⟩

private theorem coherentCheck : contains coherentBound
    (coherentPowerBox (projectionBox samples cosine) (projectionBox samples sine)) = true := by decide +kernel
private theorem totalCheck : contains totalBound (powerBox samples) = true := by decide +kernel

theorem samples_mem (s : NaiveReuppingLadder ℝ) (h : Parameters s)
    (w : Vector) (hw : w ∈ closedBall (0:Vector) (1/100000000)) :
    ∀ n : Fin 170, output s (decode center inverse w) n.val ∈ samples n := by
  have hv := values_run s h.damping coefficients (valid s h) LockedData.input
    smallHistory smallSaturation 24 170 LockedChecks.smallValues forcing forcing_mem
    (decode center inverse w) (small_start w hw)
  intro n
  have hc := LockedChecks.smallValues n.val n.isLt
  simp only [valuesCheck, Bool.and_eq_true, decide_eq_true_eq] at hc
  exact output_mem s coefficients (valid s h) (smallHistory (n.val+1)) (smallSaturation n.val)
    24 (hc.1.1.1.2 3) _ (hv (n.val+1) (by omega))

theorem powers_mem (s : NaiveReuppingLadder ℝ) (h : Parameters s)
    (w : Vector) (hw : w ∈ closedBall (0:Vector) (1/100000000)) :
    ‖coherent carrier (output s (decode center inverse w)) 170‖^2/2 ∈ coherentBound ∧
    meanSquare (output s (decode center inverse w)) 170 ∈ totalBound := by
  have hs := samples_mem s h w hw
  obtain ⟨hr, hi⟩ := coherent_parts_mem delta (output s (decode center inverse w)) 170
    samples cosine sine hs (fun n => (carrier_mem n.val n.isLt).1) (fun n => (carrier_mem n.val n.isLt).2)
  exact ⟨contains_sound coherentCheck (coherentPower_mem _ _ _ hr hi),
    contains_sound totalCheck (meanSquare_mem _ _ samples hs)⟩

/-- Error of the frozen quadratic response at this one fixture. This also
includes all neglected harmonics and numerical/truncation error: the reference
numbers are compared directly with rigorous powers of the actual sample orbit. -/
theorem approximation_error (s : NaiveReuppingLadder ℝ) (h : Parameters s)
    (w : Vector) (hw : w ∈ closedBall (0:Vector) (1/100000000)) :
    |‖coherent carrier (output s (decode center inverse w)) 170‖^2/2-0.35523845801455806| ≤ 0.00000032 ∧
    |meanSquare (output s (decode center inverse w)) 170-4.979911677333437| ≤ 0.0000011 := by
  obtain ⟨hc, ht⟩ := powers_mem s h w hw
  norm_num [IntervalRat.mem_def, coherentBound, totalBound] at hc ht
  constructor <;> rw [abs_le] <;> constructor <;> linarith

end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedMeasurements
