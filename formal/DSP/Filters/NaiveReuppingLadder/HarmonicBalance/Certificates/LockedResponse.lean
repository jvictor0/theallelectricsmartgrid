import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedMeasurements

/-! Closed response theorem for one locally attracting locked regime. Time
limits are taken over arbitrary window lengths, not only whole periods. The
error budgets concern the exact tanh sample map with the detector disabled. -/

noncomputable section
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedResponse
open Enclosure LockedData LockedCoordinates LockedParameters LockedMeasurements
open DSP.ResponseMeasurement Metric Filter
open scoped Topology

theorem carrier_unit : ‖LockedMeasurements.carrier‖ = 1 := Complex.norm_exp_ofReal_mul_I delta

theorem carrier_period : LockedMeasurements.carrier^170 = 1 := by
  rw [LockedMeasurements.carrier, ← Complex.exp_nat_mul]
  have he : (170:ℕ)*((delta:ℂ)*Complex.I) = (3:ℕ)*(2*Real.pi*Complex.I) := by
    dsimp [delta]
    push_cast
    ring
  rw [he]
  exact Complex.exp_nat_mul_two_pi_mul_I 3

private theorem forcing_period (n : ℕ) : forcing (170+n) = forcing n := by
  simpa only [Nat.add_comm] using forcing_periodic n

theorem output_periodic (s : NaiveReuppingLadder ℝ) (x : Vector)
    (hx : LockedOrbit.block s x = x) : Function.Periodic (LockedMeasurements.output s x) 170 := by
  intro n
  dsimp only [LockedMeasurements.output]
  have he : n+170+1 = 170+(n+1) := by omega
  rw [he, Step.states_periodic_of_fixed (audioStep s) forcing 170 forcing_period x hx]

theorem limits (s : NaiveReuppingLadder ℝ) (h : Parameters s) (x orbit : Vector)
    (hf : LockedOrbit.block s orbit = orbit)
    (ha : Tendsto (fun n => (LockedOrbit.block s)^[n] x) atTop (𝓝 orbit)) :
    HasCoherentResponse LockedMeasurements.carrier (LockedMeasurements.output s x)
      (coherent LockedMeasurements.carrier (LockedMeasurements.output s orbit) 170) ∧
    HasMeanSquare (LockedMeasurements.output s x) (meanSquare (LockedMeasurements.output s orbit) 170) := by
  have ht := Step.states_observer_tracking (audioStep s) (audioStep_continuous s h.damping)
    forcing 170 (by norm_num) forcing_period x orbit hf ha
    (audioObservation s) (audioObservation_continuous s)
  have ht' : Tendsto (fun n => LockedMeasurements.output s x n-LockedMeasurements.output s orbit n) atTop (𝓝 0) :=
    ht.comp (tendsto_add_atTop_nat 1)
  exact ⟨coherent_of_periodic_tracking _ carrier_unit _ _ 170 (by norm_num) carrier_period
      (output_periodic s orbit hf) ht',
    meanSquare_of_periodic_tracking _ _ 170 (by norm_num) (output_periodic s orbit hf) ht'⟩

/-- Existence, attraction, all-window time limits, and approximation errors at
the locked fixture. The only premises specify the physical coefficients; every
numerical enclosure obligation has been discharged by kernel-checked lemmas. -/
theorem exists_response (s : NaiveReuppingLadder ℝ) (h : Parameters s) :
    ∃ w ∈ closedBall (0:Vector) (1/100000000),
      let orbit := decode center inverse w
      LockedOrbit.block s orbit = orbit ∧
      Function.Periodic (LockedMeasurements.output s orbit) 170 ∧
      (∀ v ∈ closedBall (0:Vector) (1/10000),
        HasCoherentResponse LockedMeasurements.carrier (LockedMeasurements.output s (decode center inverse v))
          (coherent LockedMeasurements.carrier (LockedMeasurements.output s orbit) 170) ∧
        HasMeanSquare (LockedMeasurements.output s (decode center inverse v)) (meanSquare (LockedMeasurements.output s orbit) 170)) ∧
      ‖coherent LockedMeasurements.carrier (LockedMeasurements.output s orbit) 170‖^2/2 ∈ coherentBound ∧
      meanSquare (LockedMeasurements.output s orbit) 170 ∈ totalBound ∧
      |‖coherent LockedMeasurements.carrier (LockedMeasurements.output s orbit) 170‖^2/2-0.35523845801455806| ≤ 0.00000032 ∧
      |meanSquare (LockedMeasurements.output s orbit) 170-4.979911677333437| ≤ 0.0000011 := by
  obtain ⟨w, hw, hf, ha⟩ := LockedOrbit.exists_attracting s h
  obtain ⟨hc, hp⟩ := powers_mem s h w hw
  obtain ⟨hec, hep⟩ := approximation_error s h w hw
  exact ⟨w, hw, hf, output_periodic s _ hf, fun v hv => limits s h _ _ hf (ha v hv), hc, hp, hec, hep⟩

/-- These are measurements of the existing processor's output samples, with
its histories projected into the proof coordinates. The unused detector may
continue updating; damping zero removes its influence on the audio step. -/
theorem full_output (s : NaiveReuppingLadder ℝ) (h : Parameters s) (n : ℕ) :
    (Step.states step forcing s (n+1)).output = LockedMeasurements.output s (audioHistory s) n :=
  states_output_projection s h.damping forcing n

/-- Direct statement about the full existing filter run, for initial audio
histories in the certified local basin. Arbitrary startup detector histories
are permitted because the detector is disabled in the specified model. -/
theorem exists_full_response (s : NaiveReuppingLadder ℝ) (h : Parameters s)
    (v : Vector) (hv : v ∈ closedBall (0:Vector) (1/10000))
    (hi : audioHistory s = decode center inverse v) :
    ∃ C : ℂ, ∃ P : ℝ,
      HasCoherentResponse LockedMeasurements.carrier
        (fun n => (Step.states step forcing s (n+1)).output) C ∧
      HasMeanSquare (fun n => (Step.states step forcing s (n+1)).output) P ∧
      ‖C‖^2/2 ∈ coherentBound ∧ P ∈ totalBound ∧
      |‖C‖^2/2-0.35523845801455806| ≤ 0.00000032 ∧
      |P-4.979911677333437| ≤ 0.0000011 := by
  obtain ⟨w, _, _, _, hl, hc, hp, hec, hep⟩ := exists_response s h
  have ho : (fun n => (Step.states step forcing s (n+1)).output) =
      LockedMeasurements.output s (decode center inverse v) := by
    funext n
    rw [full_output s h, hi]
  rw [ho]
  exact ⟨_, _, (hl v hv).1, (hl v hv).2, hc, hp, hec, hep⟩

end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.LockedResponse
