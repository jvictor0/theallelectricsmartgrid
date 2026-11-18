import DSP.Filters.BandPassedEnvelopeFollower
import DSP.Filters.OnePoleZOHLowPass.Ripple
import DSP.Filters.Responses.Proofs
import DSP.Filters.Responses.Realizes
import DSP.Filters.Capture.Proofs

/-! The smoothing result is attached to the actual composed detector. The
bandpass gain below reuses both existing SVF response proofs. Convergence of
the smoothed power is conditional on the bandpass waveform approaching its
sinusoidal response. Convergence.lean proves that spectral-to-trajectory bridge
for the SVFs; neither file proves settling of the nonlinear ladder feedback loop. -/

noncomputable section

namespace DSP.BandPassedEnvelopeFollower

open OnePoleZOHLowPass Filter
open scoped Topology

abbrev trajectory (state : BandPassedEnvelopeFollower ℝ) (input : ℕ → ℝ) :=
  Step.states step (fun n => ⟨input n⟩) state

/-- Both observations are AFTER processing sample n. -/
def bandPassOutput (state : BandPassedEnvelopeFollower ℝ) (input : ℕ → ℝ) (n : ℕ) : ℝ :=
  (trajectory state input (n + 1)).stage2.bandPass

def powerOutput (state : BandPassedEnvelopeFollower ℝ) (input : ℕ → ℝ) (n : ℕ) : ℝ :=
  (trajectory state input (n + 1)).power

theorem trajectory_powerAlpha (state : BandPassedEnvelopeFollower ℝ) (input : ℕ → ℝ) (n : ℕ) :
    (trajectory state input n).powerAlpha = state.powerAlpha :=
  Step.states_invariant step (·.powerAlpha) (fun _ _ => rfl) _ state n

theorem powerOutput_step (state : BandPassedEnvelopeFollower ℝ) (input : ℕ → ℝ) (n : ℕ) :
    powerOutput state input (n + 1) = relax state.powerAlpha
      (bandPassOutput state input (n + 1) ^ 2) (powerOutput state input n) := by
  change (step (trajectory state input (n + 1)) ⟨input (n + 1)⟩).power = _
  change relax (trajectory state input (n + 1)).powerAlpha _ _ = _
  rw [trajectory_powerAlpha]
  simp only [pow_two]
  rfl

/-- Exact startup error when the two-bandpass output is already a sinusoid. -/
theorem powerOutput_error_eq (state : BandPassedEnvelopeFollower ℝ) (input : ℕ → ℝ)
    (amplitude omega phase : ℝ) (ha : 0 < state.powerAlpha) (hupper : state.powerAlpha ≤ 1)
    (hband : ∀ n, bandPassOutput state input n = Ripple.sinusoid amplitude omega phase n) (n : ℕ) :
    powerOutput state input n - Ripple.steadyPower state.powerAlpha amplitude omega phase n =
      (1 - state.powerAlpha) ^ n *
        (powerOutput state input 0 - Ripple.steadyPower state.powerAlpha amplitude omega phase 0) := by
  apply Ripple.error_eq _ _ _ _
    (ne_of_gt (Ripple.denominator_pos _ _ ha hupper)) (powerOutput state input) _ n
  intro i
  rw [powerOutput_step, hband]

/-- Bandpass startup errors may tend to zero rather than vanish at time zero.
The hypothesis concerns the squared bandpass signal, before smoothing. -/
theorem powerOutput_tendsto_ripple (state : BandPassedEnvelopeFollower ℝ) (input : ℕ → ℝ)
    (amplitude omega phase : ℝ) (ha : 0 < state.powerAlpha) (hupper : state.powerAlpha ≤ 1)
    (hband : Tendsto (fun n => bandPassOutput state input (n + 1) ^ 2 -
      Ripple.sinusoid amplitude omega phase (n + 1) ^ 2) atTop (𝓝 0)) :
    Tendsto (fun n => powerOutput state input n -
      Ripple.steadyPower state.powerAlpha amplitude omega phase n) atTop (𝓝 0) := by
  exact Ripple.tendsto_tracking _ ha hupper _ _ _ _ (powerOutput_step state input)
    (fun n => (Ripple.steadyPower_step _ _ _ _
      (ne_of_gt (Ripple.denominator_pos _ _ ha hupper)) n).symm) hband

/-- The SAME two Q=1 SVF snapshots prepared by computeCoefficients. -/
def detectorSnapshot (gain : ℝ) : TPTSVF4Pole.Snapshot ℝ :=
  ⟨⟨gain, 1⟩, ⟨gain, 1⟩⟩

/-- Project the actual detector onto its existing SVF cascade realization;
the envelope is downstream and does not feed back into the bandpasses. -/
theorem prepared_bandPass_realizes (spec : LogicalSpec ℝ) (state : BandPassedEnvelopeFollower ℝ)
    (hg : 0 ≤ spec.integratorGain) (hr1 : state.stage1.AtRest) (hr2 : state.stage2.AtRest) :
    Bilinear.Realizes step (computeCoefficients spec state) (·.stage2.bandPass)
      ((Bilinear.const <$> detectorSnapshot spec.integratorGain).bandPassTransfer Bilinear.sigma 1) := by
  have h1 := TPTSVF.prepareFromSnapshot_tuned
    (⟨spec.integratorGain, 1⟩ : TPTSVF.Snapshot ℝ) state.stage1 hg (by norm_num)
  have h2 := TPTSVF.prepareFromSnapshot_tuned
    (⟨spec.integratorGain, 1⟩ : TPTSVF.Snapshot ℝ) state.stage2 hg (by norm_num)
  have h := TPTSVF4Pole.realizes_bandPass_snapshot
    (⟨(computeCoefficients spec state).stage1, (computeCoefficients spec state).stage2⟩ : TPTSVF4Pole ℝ)
    spec.integratorGain spec.integratorGain h1 ⟨hr1.ic1, hr1.ic2⟩ h2 ⟨hr2.ic1, hr2.ic2⟩
  exact Bilinear.Realizes.project step TPTSVF4Pole.stepBandPass
    (fun s => ⟨s.stage1, s.stage2⟩) (fun _ _ => rfl) h

def bandPassPowerGain (gain frequency : ℝ) : ℝ :=
  (detectorSnapshot gain).bandPassMagnitudeSquared (ResponseFrequency.ofCyclesPerSample frequency)

theorem bandPassPowerGain_eq_normSq (gain frequency : ℝ) :
    bandPassPowerGain gain frequency =
      Complex.normSq ((detectorSnapshot gain).bandPassResponse frequency) :=
  (detectorSnapshot gain).bandPassMagnitudeSquared_eq_normSq frequency

theorem bandPassPowerGain_nonneg (gain frequency : ℝ) : 0 ≤ bandPassPowerGain gain frequency := by
  rw [bandPassPowerGain_eq_normSq]
  exact Complex.normSq_nonneg _

/-- Includes attenuation by BOTH bandpasses, before squaring and smoothing. -/
def meanPower (amplitude gain frequency : ℝ) : ℝ :=
  amplitude ^ 2 / 2 * bandPassPowerGain gain frequency

def absoluteRippleBound (alpha amplitude gain frequency : ℝ) : ℝ :=
  meanPower amplitude gain frequency * Ripple.ratio alpha (2 * Real.pi * frequency)

/-- The ripple bound is relative to original input power, with the full
bandpass attenuation retained. hresponse identifies the sinusoid's amplitude. -/
theorem steadyPower_deviation_le (alpha amplitude gain frequency bandAmplitude phase : ℝ)
    (ha : 0 < alpha) (hupper : alpha ≤ 1)
    (hresponse : bandAmplitude ^ 2 = amplitude ^ 2 * bandPassPowerGain gain frequency) (n : ℕ) :
    |Ripple.steadyPower alpha bandAmplitude (2 * Real.pi * frequency) phase n -
      meanPower amplitude gain frequency| ≤ absoluteRippleBound alpha amplitude gain frequency := by
  have hm : bandAmplitude ^ 2 / 2 = meanPower amplitude gain frequency := by
    rw [hresponse, meanPower]
    ring
  simpa only [hm, absoluteRippleBound] using Ripple.steadyPower_deviation_le
    alpha bandAmplitude (2 * Real.pi * frequency) phase
    (ne_of_gt (Ripple.denominator_pos _ _ ha hupper)) n

theorem bandPassPowerGain_dc (gain : ℝ) : bandPassPowerGain gain 0 = 0 := by
  simp [bandPassPowerGain, detectorSnapshot, TPTSVF4Pole.Snapshot.bandPassMagnitudeSquared,
    TPTSVF.Snapshot.bandPassMagnitudeSquared, TPTSVF.Snapshot.weightedMagnitudeSquared,
    ResponseFrequency.ofCyclesPerSample, ResponseFrequency.fromHalfAngle,
    Response.normSquared, fma_eq, Response.ratio, IfLess.ifLess]

theorem bandPassPowerGain_nyquist (gain : ℝ) : bandPassPowerGain gain (1 / 2) = 0 := by
  have hc : Real.cos (Real.pi * (2 : ℝ)⁻¹) = 0 := by
    simpa only [div_eq_mul_inv] using Real.cos_pi_div_two
  simp [bandPassPowerGain, detectorSnapshot, TPTSVF4Pole.Snapshot.bandPassMagnitudeSquared,
    TPTSVF.Snapshot.bandPassMagnitudeSquared, TPTSVF.Snapshot.weightedMagnitudeSquared,
    ResponseFrequency.ofCyclesPerSample, ResponseFrequency.fromHalfAngle,
    Response.normSquared, fma_eq, Response.ratio, IfLess.ifLess, hc]

theorem absoluteRippleBound_dc (alpha amplitude gain : ℝ) :
    absoluteRippleBound alpha amplitude gain 0 = 0 := by
  simp [absoluteRippleBound, meanPower, bandPassPowerGain_dc]

theorem absoluteRippleBound_nyquist (alpha amplitude gain : ℝ) :
    absoluteRippleBound alpha amplitude gain (1 / 2) = 0 := by
  unfold absoluteRippleBound meanPower
  rw [bandPassPowerGain_nyquist]
  simp

/-- A sinusoidal coefficient wobble creates sum/difference frequencies.
In particular a 2ω wobble on an ω signal contributes at ω and 3ω. This identity
alone makes no claim about the closed loop's gain at either frequency. -/
theorem modulation_components (center depth amplitude carrier wobble : ℝ) :
    (center + depth * Real.cos wobble) * (amplitude * Real.cos carrier) =
      center * amplitude * Real.cos carrier + depth * amplitude / 2 *
        (Real.cos (carrier + wobble) + Real.cos (carrier - wobble)) := by
  rw [Real.cos_add, Real.cos_sub]
  ring

end DSP.BandPassedEnvelopeFollower
