import DSP.Filters.QuadratureEnvelopeFollower
import DSP.Filters.TPTSVF.Quadrature

/-! Experimental detector: reuse the actual Q=1 SVF and one-pole smoother.
At the SVF center, BP and LP settle to quadratures and the detector power
settles to a constant, from arbitrary histories. This is a driven-detector
theorem, not a convergence theorem for the adaptive ladder feedback loop. -/

noncomputable section
namespace DSP.QuadratureEnvelopeFollower
open Filter
open scoped Topology

abbrev trajectory (s : QuadratureEnvelopeFollower ℝ) (input : ℕ → ℝ) :=
  Step.states step (fun n => ⟨input n⟩) s

theorem energy_nonneg (s : TPTSVF ℝ) : 0 ≤ energy s := by
  unfold energy
  simp only [← sq]
  positivity

theorem quadrature_energy (w : ℂ) : (w.re * w.re + w.im * w.im) / 2 = ‖w‖ ^ 2 / 2 := by
  rw [Complex.sq_norm]
  rfl

/-- Unequal quadrature amplitudes leave a twice-frequency term. Equal
amplitudes cancel it exactly, before any smoothing. -/
theorem unequal_quadrature_energy (a b phase : ℝ) :
    ((a * Real.cos phase) ^ 2 + (b * Real.sin phase) ^ 2) / 2 =
      (a ^ 2 + b ^ 2) / 4 + (a ^ 2 - b ^ 2) / 4 * Real.cos (2 * phase) := by
  rw [Real.cos_two_mul]
  nlinarith [Real.sin_sq_add_cos_sq phase]

theorem trajectory_svf (s : QuadratureEnvelopeFollower ℝ) (input : ℕ → ℝ) (n : ℕ) :
    (trajectory s input n).svf = TPTSVF.trajectory s.svf input n := by
  induction n with
  | zero => rfl
  | succ n ih =>
    change TPTSVF.step (trajectory s input n).svf ⟨input n⟩ = _
    rw [ih]
    rfl

theorem trajectory_powerAlpha (s : QuadratureEnvelopeFollower ℝ) (input : ℕ → ℝ) (n : ℕ) :
    (trajectory s input n).powerAlpha = s.powerAlpha :=
  Step.states_invariant step (·.powerAlpha) (fun _ _ => rfl) _ s n

theorem trajectory_power_step (s : QuadratureEnvelopeFollower ℝ) (input : ℕ → ℝ) (n : ℕ) :
    (trajectory s input (n + 1)).power = OnePoleZOHLowPass.relax s.powerAlpha
      (energy (TPTSVF.trajectory s.svf input (n + 1))) (trajectory s input n).power := by
  change OnePoleZOHLowPass.relax (trajectory s input n).powerAlpha
    (energy (TPTSVF.step (trajectory s input n).svf ⟨input n⟩)) _ = _
  rw [trajectory_powerAlpha, trajectory_svf]
  rfl

/-- The SVF's startup transient also disappears in the squared-norm
measurement. No steady-sinusoid assumption is imposed on its histories. -/
theorem prepared_energy_tendsto (spec : LogicalSpec ℝ) (s : QuadratureEnvelopeFollower ℝ)
    (hg : 0 < spec.integratorGain) (input : ℕ → ℝ) (A z : ℂ)
    (hcenter : (1 - (spec.integratorGain : ℂ) * Complex.I) * z =
      1 + (spec.integratorGain : ℂ) * Complex.I)
    (hu : Tendsto (fun n => input n - (A * z ^ n).re) atTop (𝓝 0)) :
    Tendsto (fun n => energy (trajectory (computeCoefficients spec s) input (n + 1)).svf -
      ‖A‖ ^ 2 / 2) atTop (𝓝 0) := by
  let state := computeCoefficients spec s
  have ht := TPTSVF.prepareFromSnapshot_tuned
    (⟨spec.integratorGain, 1⟩ : TPTSVF.Snapshot ℝ) s.svf hg.le (by norm_num)
  obtain ⟨hb, hl⟩ := TPTSVF.Quadrature.outputs_tendsto _ hg state.svf ht rfl input A z hcenter hu
  have hz := TPTSVF.Quadrature.center_norm_one _ z hcenter
  have hn (n : ℕ) : ‖A * z ^ n‖ = ‖A‖ := by rw [norm_mul, norm_pow, hz, one_pow, mul_one]
  have hb2 := Asymptotic.square_tracking _ _ ‖A‖
    (fun n => (Complex.abs_re_le_norm _).trans_eq (hn n)) hb
  have hl2 := Asymptotic.square_tracking _ _ ‖A‖
    (fun n => (Complex.abs_im_le_norm _).trans_eq (hn n)) hl
  have h := (hb2.add hl2).div_const 2
  simp only [zero_add, zero_div] at h
  apply h.congr
  intro n
  rw [trajectory_svf]
  have he := quadrature_energy (A * z ^ n)
  rw [hn] at he
  unfold energy
  dsimp only [state]
  nlinarith

/-- At the exact SVF center, any stable smoothing rate settles to constant
power. There is no persistent measurement ripple to trade against response
time; the coupled controller may still create other dynamics. -/
theorem prepared_power_tendsto (spec : LogicalSpec ℝ) (s : QuadratureEnvelopeFollower ℝ)
    (hg : 0 < spec.integratorGain) (ha : 0 < spec.powerAlpha) (hupper : spec.powerAlpha ≤ 1)
    (input : ℕ → ℝ) (A z : ℂ)
    (hcenter : (1 - (spec.integratorGain : ℂ) * Complex.I) * z =
      1 + (spec.integratorGain : ℂ) * Complex.I)
    (hu : Tendsto (fun n => input n - (A * z ^ n).re) atTop (𝓝 0)) :
    Tendsto (fun n => (trajectory (computeCoefficients spec s) input n).power - ‖A‖ ^ 2 / 2)
      atTop (𝓝 0) := by
  apply OnePoleZOHLowPass.Ripple.tendsto_tracking spec.powerAlpha ha hupper
    (fun n => energy (TPTSVF.trajectory (computeCoefficients spec s).svf input (n + 1)))
    (fun _ => ‖A‖ ^ 2 / 2) _ (fun _ => ‖A‖ ^ 2 / 2)
  · exact trajectory_power_step (computeCoefficients spec s) input
  · intro n
    simp [OnePoleZOHLowPass.relax]
  · simpa only [trajectory_svf] using prepared_energy_tendsto spec s hg input A z hcenter hu

end DSP.QuadratureEnvelopeFollower
