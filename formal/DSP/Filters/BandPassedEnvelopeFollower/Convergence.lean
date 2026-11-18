import DSP.Filters.BandPassedEnvelopeFollower.Ripple
import DSP.Filters.TPTSVF.Convergence
import DSP.ResponseMeasurement
import Mathlib.Analysis.SpecialFunctions.Complex.Arg

noncomputable section
namespace DSP.BandPassedEnvelopeFollower
open Filter
open scoped Topology

theorem trajectory_stage1 (s : BandPassedEnvelopeFollower ℝ) (input : ℕ → ℝ) (n : ℕ) :
    (trajectory s input n).stage1 = TPTSVF.trajectory s.stage1 input n := by
  induction n with
  | zero => rfl
  | succ n ih =>
    change TPTSVF.step (trajectory s input n).stage1 ⟨input n⟩ = _
    rw [ih]
    rfl

theorem trajectory_stage2 (s : BandPassedEnvelopeFollower ℝ) (input : ℕ → ℝ) (n : ℕ) :
    (trajectory s input n).stage2 = TPTSVF.trajectory s.stage2
      (fun i => (trajectory s input (i + 1)).stage1.bandPass) n := by
  induction n with
  | zero => rfl
  | succ n ih =>
    change TPTSVF.step (trajectory s input n).stage2
      ⟨(trajectory s input (n + 1)).stage1.bandPass⟩ = _
    rw [ih]
    rfl

/-- Both actual SVFs settle, from arbitrary histories, even when the external
input itself only approaches a sinusoid. No bandpass-settling premise remains. -/
theorem prepared_bandPass_tendsto (spec : LogicalSpec ℝ) (s : BandPassedEnvelopeFollower ℝ)
    (hg : 0 < spec.integratorGain) (input : ℕ → ℝ) (amplitude z : ℂ) (hz : ‖z‖ = 1)
    (hu : Tendsto (fun n => input n - (amplitude * z ^ n).re) atTop (𝓝 0)) :
    Tendsto (fun n => bandPassOutput (computeCoefficients spec s) input n -
      (TPTSVF.Convergence.response spec.integratorGain z ^ 2 * amplitude * z ^ n).re)
      atTop (𝓝 0) := by
  let state := computeCoefficients spec s
  have ht1 := TPTSVF.prepareFromSnapshot_tuned
    (⟨spec.integratorGain, 1⟩ : TPTSVF.Snapshot ℝ) s.stage1 hg.le (by norm_num)
  have ht2 := TPTSVF.prepareFromSnapshot_tuned
    (⟨spec.integratorGain, 1⟩ : TPTSVF.Snapshot ℝ) s.stage2 hg.le (by norm_num)
  have h1 := TPTSVF.Convergence.bandPass_tendsto _ hg state.stage1 ht1 rfl input amplitude z hz hu
  have h2 := TPTSVF.Convergence.bandPass_tendsto _ hg state.stage2 ht2 rfl
    (fun n => (trajectory state input (n + 1)).stage1.bandPass)
    (TPTSVF.Convergence.response spec.integratorGain z * amplitude) z hz
    (by simpa only [trajectory_stage1] using h1)
  simpa only [bandPassOutput, trajectory_stage2, pow_two, mul_assoc] using h2

/-- Both time and amplitude limits, for the actual bandpass output. This
calibrates the proposed nonlinear-response measurement on a proved linear case. -/
theorem prepared_bandPass_smallSignal (spec : LogicalSpec ℝ) (s : BandPassedEnvelopeFollower ℝ)
    (hg : 0 < spec.integratorGain) (z : ℂ) (hz : ‖z‖ = 1)
    (hfrequency : star z * star z ≠ 1) :
    ResponseMeasurement.HasSmallSignalResponse z
      (fun amplitude => bandPassOutput (computeCoefficients spec s)
        (fun n => ((amplitude : ℂ) * z ^ n).re))
      (TPTSVF.Convergence.response spec.integratorGain z ^ 2) := by
  apply ResponseMeasurement.smallSignal_of_tracking _ _ hz hfrequency
  intro amplitude _
  exact prepared_bandPass_tendsto spec s hg _ (amplitude : ℂ) z hz
    (by simp only [sub_self]; exact tendsto_const_nhds)

def carrier (omega : ℝ) : ℂ := Complex.exp ((omega : ℂ) * Complex.I)

theorem carrier_norm (omega : ℝ) : ‖carrier omega‖ = 1 := by
  simp [carrier, Complex.norm_exp]

theorem unitDelay_eq_carrier_inv (frequency : ℝ) :
    ResponseFrequency.unitDelay frequency = (carrier (2 * Real.pi * frequency))⁻¹ := by
  unfold ResponseFrequency.unitDelay carrier
  have he : (((-(2 * (Real.pi * frequency))) : ℝ) : ℂ) * Complex.I =
      -((((2 * Real.pi * frequency) : ℝ) : ℂ) * Complex.I) := by push_cast; ring
  rw [he, Complex.exp_neg]

/-- The gain obtained from time-domain settling is the existing snapshot's H. -/
theorem convergence_response_eq_snapshot (g frequency : ℝ) (hg : 0 < g) :
    TPTSVF.Convergence.response g (carrier (2 * Real.pi * frequency)) =
      (⟨g, 1⟩ : TPTSVF.Snapshot ℝ).bandPassResponse frequency := by
  let z := carrier (2 * Real.pi * frequency)
  have hz : ‖z‖ = 1 := carrier_norm _
  have hn : z ≠ 0 := norm_pos_iff.mp (by rw [hz]; norm_num)
  rw [TPTSVF.Convergence.response_eq_transfer g hg z hz, TPTSVF.Snapshot.bandPassResponse_delay,
    unitDelay_eq_carrier_inv]
  have h := (⟨(g : ℂ), 1⟩ : TPTSVF.Snapshot ℂ).weightedTransfer_scale
    0 1 0 (1 - z⁻¹) (1 + z⁻¹) z hn
  simpa only [TPTSVF.Snapshot.bandPassTransfer, mul_sub, mul_add, mul_one, mul_inv_cancel₀ hn,
    Functor.map, Complex.ofReal_one, z] using h

theorem convergence_cascade_eq_snapshot (g frequency : ℝ) (hg : 0 < g) :
    TPTSVF.Convergence.response g (carrier (2 * Real.pi * frequency)) ^ 2 =
      (detectorSnapshot g).bandPassResponse frequency := by
  rw [convergence_response_eq_snapshot g frequency hg]
  simp only [detectorSnapshot, TPTSVF4Pole.Snapshot.bandPassResponse,
    TPTSVF4Pole.Snapshot.bandPassTransfer, TPTSVF.Snapshot.bandPassResponse, Functor.map, pow_two]

theorem carrier_power_re (amplitude : ℂ) (omega : ℝ) (n : ℕ) :
    (amplitude * carrier omega ^ n).re =
      OnePoleZOHLowPass.Ripple.sinusoid ‖amplitude‖ omega amplitude.arg n := by
  have he : carrier omega ^ n = Complex.exp (((omega * n : ℝ) : ℂ) * Complex.I) := by
    rw [carrier, ← Complex.exp_nat_mul]
    congr 1
    push_cast
    ring
  rw [he, Complex.mul_re, Complex.exp_ofReal_mul_I_re, Complex.exp_ofReal_mul_I_im]
  unfold OnePoleZOHLowPass.Ripple.sinusoid
  rw [Real.cos_add]
  linear_combination -Real.cos (omega * n) * (Complex.norm_mul_cos_arg amplitude) +
    Real.sin (omega * n) * (Complex.norm_mul_sin_arg amplitude)

/-- Complete detector settling: both SVFs, the squarer, and the actual EMA.
The limit waveform contains the persistent twice-frequency power ripple. -/
theorem prepared_power_tendsto (spec : LogicalSpec ℝ) (s : BandPassedEnvelopeFollower ℝ)
    (hg : 0 < spec.integratorGain) (ha : 0 < spec.powerAlpha) (hupper : spec.powerAlpha ≤ 1)
    (amplitude : ℂ) (omega : ℝ) (input : ℕ → ℝ)
    (hu : Tendsto (fun n => input n - (amplitude * carrier omega ^ n).re) atTop (𝓝 0)) :
    let outputAmplitude := TPTSVF.Convergence.response spec.integratorGain (carrier omega) ^ 2 * amplitude
    Tendsto (fun n => powerOutput (computeCoefficients spec s) input n -
      OnePoleZOHLowPass.Ripple.steadyPower spec.powerAlpha ‖outputAmplitude‖ omega outputAmplitude.arg n)
      atTop (𝓝 0) := by
  dsimp only
  let outputAmplitude := TPTSVF.Convergence.response spec.integratorGain (carrier omega) ^ 2 * amplitude
  have hband := prepared_bandPass_tendsto spec s hg input amplitude (carrier omega) (carrier_norm omega) hu
  have hsquared := Asymptotic.square_tracking _ _ ‖outputAmplitude‖
    (fun n => by
      change |(outputAmplitude * carrier omega ^ n).re| ≤ ‖outputAmplitude‖
      calc
        _ ≤ ‖outputAmplitude * carrier omega ^ n‖ := Complex.abs_re_le_norm _
        _ = _ := by rw [norm_mul, norm_pow, carrier_norm, one_pow, mul_one]) hband
  apply powerOutput_tendsto_ripple _ input ‖outputAmplitude‖ omega outputAmplitude.arg ha hupper
  simpa only [carrier_power_re, Function.comp_def, outputAmplitude]
    using hsquared.comp (tendsto_add_atTop_nat 1)

/-- End-to-end settling, with amplitude and phase supplied by the already
proved two-bandpass snapshot transfer rather than an assumed sinusoidal output. -/
theorem prepared_power_tendsto_snapshot (spec : LogicalSpec ℝ) (s : BandPassedEnvelopeFollower ℝ)
    (hg : 0 < spec.integratorGain) (ha : 0 < spec.powerAlpha) (hupper : spec.powerAlpha ≤ 1)
    (amplitude : ℂ) (frequency : ℝ) (input : ℕ → ℝ)
    (hu : Tendsto (fun n => input n - (amplitude * carrier (2 * Real.pi * frequency) ^ n).re)
      atTop (𝓝 0)) :
    let outputAmplitude := (detectorSnapshot spec.integratorGain).bandPassResponse frequency * amplitude
    Tendsto (fun n => powerOutput (computeCoefficients spec s) input n -
      OnePoleZOHLowPass.Ripple.steadyPower spec.powerAlpha ‖outputAmplitude‖
        (2 * Real.pi * frequency) outputAmplitude.arg n) atTop (𝓝 0) := by
  simpa only [convergence_cascade_eq_snapshot spec.integratorGain frequency hg]
    using prepared_power_tendsto spec s hg ha hupper amplitude (2 * Real.pi * frequency) input hu

/-- A nonzero settled bandpass tone gives an eventually positive denominator
for the floor-free controller. This says nothing about its startup/silence
policy, and the lower bound shrinks quadratically with stimulus amplitude. -/
theorem prepared_power_eventually_positive (spec : LogicalSpec ℝ) (s : BandPassedEnvelopeFollower ℝ)
    (hg : 0 < spec.integratorGain) (ha : 0 < spec.powerAlpha) (hupper : spec.powerAlpha < 1)
    (amplitude : ℂ) (omega : ℝ) (input : ℕ → ℝ)
    (hu : Tendsto (fun n => input n - (amplitude * carrier omega ^ n).re) atTop (𝓝 0))
    (hfrequency : Real.sin omega ≠ 0)
    (houtput : TPTSVF.Convergence.response spec.integratorGain (carrier omega) ^ 2 * amplitude ≠ 0) :
    ∃ lowerBound : ℝ, 0 < lowerBound ∧ ∀ᶠ n in atTop,
      lowerBound ≤ powerOutput (computeCoefficients spec s) input n := by
  let outputAmplitude := TPTSVF.Convergence.response spec.integratorGain (carrier omega) ^ 2 * amplitude
  let mean := ‖outputAmplitude‖ ^ 2 / 2
  let rho := OnePoleZOHLowPass.Ripple.ratio spec.powerAlpha omega
  let lowerBound := mean * (1 - rho) / 2
  have hm : 0 < mean := div_pos (sq_pos_of_pos (norm_pos_iff.mpr houtput)) (by norm_num)
  have hr : rho < 1 := OnePoleZOHLowPass.Ripple.ratio_lt_one _ _ ha hupper hfrequency
  have hl : 0 < lowerBound := div_pos (mul_pos hm (sub_pos.mpr hr)) (by norm_num)
  have ht := prepared_power_tendsto spec s hg ha hupper.le amplitude omega input hu
  obtain ⟨N, hN⟩ := Metric.tendsto_atTop.mp ht lowerBound hl
  refine ⟨lowerBound, hl, ?_⟩
  filter_upwards [eventually_ge_atTop N] with n hn
  have he := hN n hn
  simp only [Real.dist_eq, sub_zero] at he
  have hb := OnePoleZOHLowPass.Ripple.steadyPower_deviation_le spec.powerAlpha
    ‖outputAmplitude‖ omega outputAmplitude.arg
    (ne_of_gt (OnePoleZOHLowPass.Ripple.denominator_pos _ _ ha hupper.le)) n
  change |_ - mean| ≤ mean * rho at hb
  have hlow := (abs_le.mp hb).1
  have herror := (abs_lt.mp he).1
  change -(mean * (1 - rho) / 2) <
    powerOutput (computeCoefficients spec s) input n -
      OnePoleZOHLowPass.Ripple.steadyPower spec.powerAlpha ‖outputAmplitude‖ omega outputAmplitude.arg n
    at herror
  dsimp only [lowerBound]
  linarith

end DSP.BandPassedEnvelopeFollower
