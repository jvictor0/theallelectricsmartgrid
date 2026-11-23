import DSP.Filters.NaiveReuppingLadder.FloorFreeDynamics

/-! Scratch exploration: bounded audio states do not require a stable tangent
or convergence to a periodic orbit. These are exact-real statements only.
This file is not imported by the code generator. -/

noncomputable section
namespace DSP.NaiveReuppingLadder.Exploration
open Filter
open scoped Topology

theorem saturation_bound (c : NormalizedTanh.Coefficients ℝ) (hc : 0 < c.tanhGain)
    (x : ℝ) : |NormalizedTanh.apply c x| ≤ 1 / c.tanhGain := by
  change |Real.tanh (x * c.gain) / c.tanhGain| ≤ _
  rw [abs_div, abs_of_pos hc]
  apply div_le_div_of_nonneg_right _ hc.le
  exact abs_le.mpr ⟨(Real.neg_one_lt_tanh _).le, (Real.tanh_lt_one _).le⟩

theorem runtime_feedback_bounds (s : NaiveReuppingLadder ℝ)
    (hK : 0 ≤ s.baseFeedback) (hd : 0 ≤ s.dampingScale)
    (hp : 0 ≤ s.inputEnvelope.power) :
    0 ≤ effectiveFeedback s ∧ effectiveFeedback s ≤ s.baseFeedback := by
  have hf : 0 < (powerFloor : ℝ) := by norm_num [powerFloor]
  have hmax : 0 ≤ maxValue (0 : ℝ)
      (s.feedbackEnvelope.power-s.referencePowerGain*s.inputEnvelope.power) := by
    change 0 ≤ if 0 < s.feedbackEnvelope.power-s.referencePowerGain*s.inputEnvelope.power
      then s.feedbackEnvelope.power-s.referencePowerGain*s.inputEnvelope.power else 0
    split <;> linarith
  have he := div_nonneg hmax (add_nonneg hp hf.le)
  have hden : 1 ≤ s.dampingScale *
      (maxValue (0 : ℝ) (s.feedbackEnvelope.power-s.referencePowerGain*s.inputEnvelope.power) /
        (s.inputEnvelope.power+powerFloor)) + 1 := by
    linarith [mul_nonneg hd he]
  exact ⟨div_nonneg hK (by linarith), div_le_self hK hden⟩

theorem runtime_output_bound (s : NaiveReuppingLadder ℝ) (x : Sample ℝ)
    (hc : 0 < s.saturation.tanhGain)
    (hK : 0 ≤ s.baseFeedback) (hd : 0 ≤ s.dampingScale)
    (hp : 0 ≤ s.inputEnvelope.power) :
    |(step s x).output| ≤ (1+s.baseFeedback) / s.saturation.tanhGain := by
  have hk := runtime_feedback_bounds s hK hd hp
  change |NormalizedTanh.apply s.saturation _ * (effectiveFeedback s+1)| ≤ _
  rw [abs_mul, abs_of_nonneg (by linarith : 0 ≤ effectiveFeedback s+1)]
  calc
    _ ≤ (1/s.saturation.tanhGain)*(effectiveFeedback s+1) :=
      mul_le_mul_of_nonneg_right (saturation_bound _ hc _) (by linarith)
    _ ≤ (1/s.saturation.tanhGain)*(s.baseFeedback+1) :=
      mul_le_mul_of_nonneg_left (by linarith) (by positivity)
    _ = _ := by ring

theorem detector_power_nonnegative (s : BandPassedEnvelopeFollower ℝ) (x : Sample ℝ)
    (ha : 0 ≤ s.powerAlpha) (ha1 : s.powerAlpha ≤ 1) (hp : 0 ≤ s.power) :
    0 ≤ (s.step x).power := by
  let v := (TPTSVF4Pole.stepBandPass ⟨s.stage1,s.stage2⟩ x).stage2.bandPass
  change 0 ≤ s.power+s.powerAlpha*(v*v-s.power)
  nlinarith [mul_nonneg ha (mul_self_nonneg v),
    mul_nonneg (sub_nonneg.mpr ha1) hp]

abbrev runtimeTrajectory (s : NaiveReuppingLadder ℝ) (u : ℕ → ℝ) :=
  Step.states step (fun n => ⟨u n⟩) (reset s)

theorem runtime_invariants (s : NaiveReuppingLadder ℝ) (u : ℕ → ℝ) (n : ℕ)
    (ha : 0 ≤ s.inputEnvelope.powerAlpha) (ha1 : s.inputEnvelope.powerAlpha ≤ 1) :
    let t := runtimeTrajectory s u n
    t.saturation = s.saturation ∧ t.baseFeedback = s.baseFeedback ∧
      t.dampingScale = s.dampingScale ∧
      t.inputEnvelope.powerAlpha = s.inputEnvelope.powerAlpha ∧ 0 ≤ t.inputEnvelope.power := by
  induction n with
  | zero => exact ⟨rfl,rfl,rfl,rfl,le_refl 0⟩
  | succ n ih =>
    dsimp only at ih ⊢
    refine ⟨ih.1,ih.2.1,ih.2.2.1,ih.2.2.2.1,?_⟩
    exact detector_power_nonnegative (runtimeTrajectory s u n).inputEnvelope ⟨u n⟩
      (by rw [ih.2.2.2.1]; exact ha) (by rw [ih.2.2.2.1]; exact ha1) ih.2.2.2.2

/-- Uniform bound for EVERY input stream and sample count. It is an absolute
output bound, not a gain relative to input amplitude and not convergence. -/
theorem all_time_output_bound (s : NaiveReuppingLadder ℝ) (u : ℕ → ℝ) (n : ℕ)
    (hc : 0 < s.saturation.tanhGain)
    (hK : 0 ≤ s.baseFeedback) (hd : 0 ≤ s.dampingScale)
    (ha : 0 ≤ s.inputEnvelope.powerAlpha) (ha1 : s.inputEnvelope.powerAlpha ≤ 1) :
    |(runtimeTrajectory s u (n+1)).output| ≤ (1+s.baseFeedback)/s.saturation.tanhGain := by
  have hi := runtime_invariants s u n ha ha1
  have h := runtime_output_bound (runtimeTrajectory s u n) ⟨u n⟩
    (by rw [hi.1]; exact hc) (by rw [hi.2.1]; exact hK)
    (by rw [hi.2.2.1]; exact hd) hi.2.2.2.2
  change |((runtimeTrajectory s u n).step ⟨u n⟩).output| ≤ _
  simpa only [hi.1,hi.2.1] using h

theorem onePole_bound (a x history M : ℝ) (ha : 0 ≤ a) (ha1 : a ≤ 1)
    (hx : |x| ≤ M) (hh : |history| ≤ M) :
    |OnePoleZOHLowPass.onePoleStep a x history| ≤ M := by
  simp only [OnePoleZOHLowPass.onePoleStep_eq, fma_eq]
  calc
    |a*x + (1-a)*history| ≤ |a*x| + |(1-a)*history| := abs_add_le _ _
    _ = a*|x| + (1-a)*|history| := by
      rw [abs_mul, abs_mul, abs_of_nonneg ha, abs_of_nonneg (sub_nonneg.mpr ha1)]
    _ ≤ a*M + (1-a)*M := add_le_add
      (mul_le_mul_of_nonneg_left hx ha)
      (mul_le_mul_of_nonneg_left hh (sub_nonneg.mpr ha1))
    _ = M := by ring

/-- This is a bound on the actual nonlinear audio step for ANY bounded
feedback sequence. No assumption that the detector settles is needed. -/
theorem audio_box_preserved (s : NaiveReuppingLadder ℝ) (k x U K M : ℝ)
    (ha : 0 ≤ s.alpha) (ha1 : s.alpha ≤ 1)
    (hk : 0 ≤ k) (hkK : k ≤ K) (hM : 0 ≤ M)
    (hsat : ∀ v, |NormalizedTanh.apply s.saturation v| ≤ M)
    (hx : |x| ≤ U)
    (h1 : |s.stage1| ≤ U+K*M) (h2 : |s.stage2| ≤ M)
    (h3 : |s.stage3| ≤ M) (h4 : |s.stage4| ≤ M) :
    let t := FloorFree.advance s k ⟨x⟩
    |t.stage1| ≤ U+K*M ∧ |t.stage2| ≤ M ∧ |t.stage3| ≤ M ∧
      |t.stage4| ≤ M ∧ |t.output| ≤ (1+K)*M := by
  have hd : |x-k*s.stage4| ≤ U+K*M := by
    calc
      |x-k*s.stage4| ≤ |x|+|k*s.stage4| := abs_sub _ _
      _ = |x|+k*|s.stage4| := by rw [abs_mul, abs_of_nonneg hk]
      _ ≤ U+K*M := add_le_add hx (mul_le_mul hkK h4 (abs_nonneg _) (hk.trans hkK))
  dsimp only [FloorFree.advance, FloorFree.advanceWith]
  refine ⟨onePole_bound _ _ _ _ ha ha1 hd h1,
    onePole_bound _ _ _ _ ha ha1 (hsat _) h2,
    onePole_bound _ _ _ _ ha ha1 (hsat _) h3,
    onePole_bound _ _ _ _ ha ha1 (hsat _) h4, ?_⟩
  rw [abs_mul, abs_of_nonneg (by linarith : 0 ≤ k+1)]
  calc
    _ ≤ M*(k+1) := mul_le_mul_of_nonneg_right (hsat _) (by linarith)
    _ ≤ M*(K+1) := mul_le_mul_of_nonneg_left (by linarith) hM
    _ = _ := by ring

/-- The positive floor makes the controller approach base feedback whenever
both detector powers approach zero. The floor-free ratio lacks this property.
This is a building block, not yet a full trajectory-derivative theorem. -/
theorem fixedFloor_controller_limit {ι : Type} {l : Filter ι}
    (K d R floor : ℝ) (hf : floor ≠ 0) (p q : ι → ℝ)
    (hp : Tendsto p l (𝓝 0)) (hq : Tendsto q l (𝓝 0)) :
    Tendsto (fun i => K / (d * (max 0 (q i - R*p i) / (p i+floor)) + 1)) l (𝓝 K) := by
  have he : Tendsto (fun i => max 0 (q i-R*p i)) l (𝓝 0) := by
    simpa using (tendsto_const_nhds (x := (0 : ℝ))).max
      (hq.sub (hp.const_mul R))
  have hr : Tendsto (fun i => max 0 (q i-R*p i)/(p i+floor)) l (𝓝 0) := by
    simpa only [div_eq_mul_inv, zero_mul] using
      he.mul ((hp.add_const floor).inv₀ (by simpa using hf))
  have hden : Tendsto (fun i => d*(max 0 (q i-R*p i)/(p i+floor))+1) l (𝓝 1) := by
    simpa using (hr.const_mul d).add_const 1
  simpa only [div_eq_mul_inv, inv_one, mul_one] using
    (tendsto_const_nhds (x := K)).mul (hden.inv₀ (by norm_num))

end DSP.NaiveReuppingLadder.Exploration
