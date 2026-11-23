import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockChecks
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockInitial
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockParameters

/-! A closed 162-sample replay for one entire curved input cell of the
drifting fixture. Physical parameters and cell membership are the only
premises. This establishes state enclosures and exact secant representations;
it does not yet establish trapping or attraction around the whole circle. -/

noncomputable section
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCell
open LeanCert.Core PolarTube Enclosure DriftBlockCellData DriftBlockInitial
open scoped Matrix

theorem point_eq_decode (θ : ℝ) (r : Normal) :
    point θ r = DriftChart.chart.decode ((θ:Real.Angle),r) := by
  apply congrArg (fun x => DriftChart.inverse *ᵥ x)
  congr 1

theorem replay (s : NaiveReuppingLadder ℝ) (h : LockedParameters.Parameters s)
    (θ : ℝ) (r : Normal) (hθ : θ ∈ DriftProfileCellData.angle)
    (hr : ∀ j, |r j| ≤ (1/500000:ℝ)) :
    ∀ n ≤ 162,
      (∀ i, Step.states (audioStep s) DriftBlockParameters.forcing centerPoint n i ∈ center n i) ∧
      (∀ i, Step.states (audioStep s) DriftBlockParameters.forcing (point θ r) n i ∈ whole n i) ∧
      ∃ m : Matrix (Fin 4) (Fin 4) ℝ, (∀ i j, m i j ∈ matrices n i j) ∧
        Step.states (audioStep s) DriftBlockParameters.forcing (point θ r) n-
          Step.states (audioStep s) DriftBlockParameters.forcing centerPoint n = m *ᵥ displacement θ r := by
  obtain ⟨m,hm,he⟩ := initial_difference θ r hθ hr
  exact centered_run s h.damping LockedData.coefficients (LockedParameters.valid s h)
    DriftBlockCellData.input center whole centerSaturation DriftBlockCellData.saturation matrices width
    24 162 DriftBlockChecks.steps DriftBlockParameters.forcing (fun n _ => DriftBlockParameters.input_mem n)
    (point θ r) centerPoint (displacement θ r) (whole_mem θ r hθ hr) center_mem m hm
    (displacement_bound θ r hθ hr) he

theorem states_mem (s : NaiveReuppingLadder ℝ) (h : LockedParameters.Parameters s)
    (θ : ℝ) (r : Normal) (hθ : θ ∈ DriftProfileCellData.angle)
    (hr : ∀ j, |r j| ≤ (1/500000:ℝ)) (n : ℕ) (hn : n ≤ 162) :
    ∀ i, Step.states (audioStep s) DriftBlockParameters.forcing
      (DriftChart.chart.decode ((θ:Real.Angle),r)) n i ∈ whole n i := by
  rw [← point_eq_decode]
  exact (replay s h θ r hθ hr n hn).2.1

theorem observations_mem (s : NaiveReuppingLadder ℝ) (h : LockedParameters.Parameters s)
    (θ : ℝ) (r : Normal) (hθ : θ ∈ DriftProfileCellData.angle)
    (hr : ∀ j, |r j| ≤ (1/500000:ℝ)) (n : ℕ) (hn : n < 162) :
    audioObservation s (Step.states (audioStep s) DriftBlockParameters.forcing
      (DriftChart.chart.decode ((θ:Real.Angle),r)) (n+1)) ∈
        outputRange LockedData.coefficients (DriftBlockCellData.saturation n 3) := by
  have hc := DriftBlockChecks.steps n hn
  simp only [centeredCheck, Bool.and_eq_true, decide_eq_true_eq] at hc
  exact output_mem s LockedData.coefficients (LockedParameters.valid s h)
    (whole (n+1)) (DriftBlockCellData.saturation n) 24 (hc.2 3) _
    (states_mem s h θ r hθ hr (n+1) (by omega))

theorem full_states_mem (s : NaiveReuppingLadder ℝ) (h : LockedParameters.Parameters s)
    (θ : ℝ) (r : Normal) (hθ : θ ∈ DriftProfileCellData.angle)
    (hr : ∀ j, |r j| ≤ (1/500000:ℝ))
    (hi : audioHistory s = DriftChart.chart.decode ((θ:Real.Angle),r))
    (n : ℕ) (hn : n ≤ 162) :
    ∀ i, audioHistory (Step.states step DriftBlockParameters.forcing s n) i ∈ whole n i := by
  rw [states_audioHistory s h.damping,hi]
  exact states_mem s h θ r hθ hr n hn

theorem full_observations_mem (s : NaiveReuppingLadder ℝ) (h : LockedParameters.Parameters s)
    (θ : ℝ) (r : Normal) (hθ : θ ∈ DriftProfileCellData.angle)
    (hr : ∀ j, |r j| ≤ (1/500000:ℝ))
    (hi : audioHistory s = DriftChart.chart.decode ((θ:Real.Angle),r))
    (n : ℕ) (hn : n < 162) :
    (Step.states step DriftBlockParameters.forcing s (n+1)).output ∈
      outputRange LockedData.coefficients (DriftBlockCellData.saturation n 3) := by
  rw [states_output_projection s h.damping,hi]
  exact observations_mem s h θ r hθ hr n hn

end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCell
