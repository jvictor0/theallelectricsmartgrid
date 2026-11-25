import DSP.Filters.SquiggleBoyQuadReverb.Frozen.Expected
import DSP.Filters.SquiggleBoyQuadReverb.Frozen.SmallSignal
import DSP.Filters.SquiggleBoyQuadReverb.Frozen.Refinement
import DSP.Filters.SquiggleBoyQuadReverb.Frozen.Harmonic
import DSP.WanderingRandomLFO.Characteristic
import DSP.Filters.SchroederAllPass.WanderingProofs
import Mathlib.MeasureTheory.Function.Floor

namespace DSP.SquiggleBoyQuadReverb.Frozen

open DelayBuffer Filter Matrix MeasureTheory ResponseMeasurement WanderingSchroederAllPass
open scoped BigOperators Matrix.Norms.L2Operator

noncomputable section

abbrev IntroWander := Fin 8 → Lanes ℝ
abbrev TankWander := Fin 2 → Lanes ℝ

/-- One scalar clock readout for each of the forty stage and lane pairs. -/
abbrev WanderSample := IntroWander × TankWander

def introWanderLaw : Measure IntroWander :=
  Measure.pi fun _ : Fin 8 => Measure.pi fun _ : Fin 4 => WanderingRandomLFO.stationaryAmplitudeLaw

def tankWanderLaw : Measure TankWander :=
  Measure.pi fun _ : Fin 2 => Measure.pi fun _ : Fin 4 => WanderingRandomLFO.stationaryAmplitudeLaw

instance : IsProbabilityMeasure introWanderLaw := by
  unfold introWanderLaw
  infer_instance

instance : IsProbabilityMeasure tankWanderLaw := by
  unfold tankWanderLaw
  infer_instance

/-- The stationary freeze distribution for independent ideal per-lane clocks. -/
def wanderLaw : Measure WanderSample := introWanderLaw.prod tankWanderLaw

instance : IsProbabilityMeasure wanderLaw := by
  unfold wanderLaw
  infer_instance

private def setDiffusionClocks (wander : IntroWander) :
    (order : List (Fin 8)) → Diffusion order → Diffusion order
  | [], s => s
  | i :: order, s => ({s.1 with clock := wander i}, setDiffusionClocks wander order s.2)

private theorem diffusionHistory_setDiffusionClocks (wander : IntroWander)
    (order : List (Fin 8)) (s : Diffusion order) :
    diffusionHistory order (setDiffusionClocks wander order s) = diffusionHistory order s := by
  induction order with
  | nil => rfl
  | cons i order ih => simp only [diffusionHistory, setDiffusionClocks, ih]

/-- Install a freeze draw in the same clock fields used by the reverb. -/
def withWander (s : FrozenState) (x : WanderSample) : FrozenState :=
  {s with
    diffusion := setDiffusionClocks x.1 squiggleBoyReverbIntroOrder s.diffusion
    first := {s.first with clock := x.2 0}
    second := {s.second with clock := x.2 1}}

theorem histories_withWander (s : FrozenState) (x : WanderSample) :
    histories (withWander s x) = histories s := by
  simp only [histories, withWander, diffusionHistory_setDiffusionClocks]

/-- The production tap layout and prepared gains, at one sampled freeze. -/
def wanderSpec (s : FrozenState) (x : WanderSample) : Spec ℂ :=
  (prepared (withWander s x)).complex

/-- A sampled clock setting preserves silence and realizes the transfer used
in the expectation, under the frozen small-signal stability hypotheses. -/
theorem hasSmallSignalResponse_withWander (s : FrozenState) (x : WanderSample)
    (rest : histories s = 0) (i j : Fin 4) (theta : Phase)
    (hfreq : star (fourier 1 theta) * star (fourier 1 theta) ≠ 1)
    (gauge : StabilityGauge
      (LocalDynamics.statePart (columnTangent (prepared (withWander s x)) j)).toLinearMap)
    (blocks : BlockDenominators (wanderSpec s x) (fourier 1 theta)⁻¹)
    (hstable : ‖loopReturn (wanderSpec s x) (fourier 1 theta)⁻¹‖ < 1) :
    HasSmallSignalResponse (fourier 1 theta)
      (fun amplitude n => (Processor.run processor
        (fun k => (amplitude * ((fourier 1 theta)^k).re) • basisLane j) (withWander s x) n) i)
      (transfer (wanderSpec s x) (fourier 1 theta)⁻¹ i j) :=
  hasSmallSignalResponse (withWander s x) ((histories_withWander s x).trans rest)
    i j theta hfreq gauge blocks (returnDen_isUnit_det_of_norm_lt_one _ _ hstable)

private theorem measurable_apfTapResponse {n : ℕ} (hn : 0 < n)
    (spec : TapSpec) (q : ℂ) (i : Fin 4) :
    Measurable (fun x : Lanes ℝ => tapResponse (apfTap hn spec x).complex q i) := by
  let offset (x : ℝ) : ℝ :=
    WanderingSchroederAllPassGeneric.offsetSamples (SampleIndex.ofWord spec.depth) x
  let rounded (x : ℝ) : ℝ := -((⌊-offset x⌋ : ℤ) : ℝ)
  have ho : Continuous offset := by
    simp only [offset, WanderingSchroederAllPassGeneric.offsetSamples_real]
    exact continuous_const.mul (continuous_const.add (continuous_const.max
      (continuous_const.min continuous_id)))
  have hr : Measurable rounded :=
    ((measurable_of_countable (fun k : ℤ => (k : ℝ))).comp ho.measurable.neg.floor).neg
  have hf : Measurable (fun x => ((rounded x - offset x : ℝ) : ℂ)) :=
    Complex.continuous_ofReal.measurable.comp (hr.sub ho.measurable)
  unfold tapResponse
  refine Finset.measurable_sum _ fun j _ => ?_
  let phase (k : ℕ) : ℂ :=
    q ^ (((UInt32.ofNat k).toNat + n - (UInt64.ofNat j.val).toNat % n) % n + 1)
  have hp : Measurable (fun x => phase ⌊(tapBase spec : Lanes ℝ) i + rounded x⌋₊) :=
    (measurable_of_countable phase).comp (measurable_const.add hr).nat_floor
  have hw : Continuous (fun t : ℂ => cubicWeights t j) := by
    fin_cases j <;> simp [cubicWeights] <;> fun_prop
  change Measurable (fun x : Lanes ℝ =>
    cubicWeights ((rounded (x i) - offset (x i) : ℝ) : ℂ) j *
      phase ⌊(tapBase spec : Lanes ℝ) i + rounded (x i)⌋₊)
  exact ((hw.measurable.comp hf).fun_mul hp).comp (measurable_pi_apply i)

private theorem measurable_apfResponse {n : ℕ} (hn : 0 < n)
    (spec : TapSpec) (q : ℂ) (i : Fin 4) :
    Measurable (fun x : Lanes ℝ => allPassResponse (apfTap hn spec x).complex q i) := by
  simp only [allPassResponse, SchroederAllPass.response, pow_one]
  have h := measurable_apfTapResponse hn spec q i
  exact (h.sub measurable_const).div (measurable_const.sub (measurable_const.mul h))

private theorem measurable_introWander (q : ℂ) (order : List (Fin 8)) (s : Diffusion order)
    (i : Fin 4) :
    Measurable (fun x : IntroWander =>
      introResponse q order (fun j => (diffusionTaps order (setDiffusionClocks x order s) j).complex) i) := by
  induction order with
  | nil => exact measurable_const
  | cons k order ih =>
    simp only [introResponse, diffusionTaps, setDiffusionClocks, introCons,
      Fin.cases_zero, Fin.cases_succ, Pi.mul_apply]
    exact ((measurable_apfResponse _ (Runtime.introTap k) q i).comp (measurable_pi_apply k)).mul (ih s.2)

private theorem measurable_quadDiagonal :
    Measurable (diagonal : Lanes ℂ → QuadMatrix) := by
  apply Continuous.measurable
  change Continuous (fun v : Lanes ℂ => (fun i j : Fin 4 => diagonal v i j))
  refine continuous_pi fun i => continuous_pi fun j => ?_
  by_cases h : i = j
  · subst j
    simpa only [diagonal_apply_eq] using (continuous_apply i : Continuous (fun v : Lanes ℂ => v i))
  · simp only [diagonal_apply_ne _ h]
    exact continuous_const

theorem wanderSpec_fixedControls (s : FrozenState) (x : WanderSample) (q : ℂ) :
    let p := (prepared s).complex
    let w := wanderSpec s x
    (dampingResponse w q, w.satGain, w.main, w.feedback) =
      (dampingResponse p q, p.satGain, p.main, p.feedback) := by
  have hs : (withWander s x).settings = s.settings := rfl
  have hd : (withWander s x).damping = s.damping := rfl
  simp only [wanderSpec, Spec.complex, prepared, hs, hd]
  unfold dampingResponse
  rfl

theorem measurable_wanderForward (s : FrozenState) (q : ℂ) :
    Measurable (fun x => forward (wanderSpec s x) q) := by
  let p := (prepared s).complex
  let input (x : WanderSample) : Lanes ℂ := fun i =>
    allPassResponse (wanderSpec s x).second q i * dampingResponse p q i *
      p.satGain i * allPassResponse (wanderSpec s x).first q i
  let mixOut (v : Lanes ℂ) : QuadMatrix :=
    diagonal v * LinearMap.toMatrix' mix * diagonal (q⁻¹ • tapResponse p.main q)
  have ha (i : Fin 4) : Measurable (fun x : WanderSample => allPassResponse (wanderSpec s x).first q i) := by
    change Measurable (fun x : WanderSample =>
      allPassResponse (apfTap (by decide : 0 < squiggleBoyReverbFirstCapacity)
        (Runtime.feedbackTap 0) (x.2 0)).complex q i)
    exact (measurable_apfResponse (by decide) (Runtime.feedbackTap 0) q i).comp
      ((measurable_pi_apply 0).comp measurable_snd)
  have hb (i : Fin 4) : Measurable (fun x : WanderSample => allPassResponse (wanderSpec s x).second q i) := by
    change Measurable (fun x : WanderSample =>
      allPassResponse (apfTap (by decide : 0 < squiggleBoyReverbSecondCapacity)
        (Runtime.feedbackTap 1) (x.2 1)).complex q i)
    exact (measurable_apfResponse (by decide) (Runtime.feedbackTap 1) q i).comp
      ((measurable_pi_apply 1).comp measurable_snd)
  have h : Measurable input := measurable_pi_iff.mpr fun i =>
    (((hb i).mul_const (dampingResponse p q i)).mul_const (p.satGain i)).fun_mul (ha i)
  have hout : Measurable mixOut :=
    (measurable_quadDiagonal.mul_const _).mul_const _
  have heq : (fun x => forward (wanderSpec s x) q) = mixOut ∘ input := by
    funext x
    have hc := wanderSpec_fixedControls s x q
    have hd := congrArg Prod.fst hc
    have hg := congrArg (fun a => a.2.1) hc
    have hm := congrArg (fun a => a.2.2.1) hc
    dsimp only at hd hg hm
    simp only [forward, Function.comp_apply, mixOut, input, hd, hg, hm, p]
  rw [heq]
  exact hout.comp h

theorem measurable_wanderLoopReturn (s : FrozenState) (q : ℂ) :
    Measurable (fun x => loopReturn (wanderSpec s x) q) := by
  let fb := diagonal (prepared s).complex.feedback
  have heq : (fun x => loopReturn (wanderSpec s x) q) =
      (fun x => q • (forward (wanderSpec s x) q * fb)) := by
    funext x
    have hf := congrArg (fun a => a.2.2.2) (wanderSpec_fixedControls s x q)
    dsimp only at hf
    simp only [loopReturn, hf, fb]
  rw [heq]
  exact ((measurable_wanderForward s q).mul_const fb).fun_const_smul q

theorem measurable_wanderIntro (s : FrozenState) (q : ℂ) :
    Measurable (fun x : IntroWander => diagonal (introResponse q _ (wanderSpec s (x, 0)).intro)) :=
  measurable_quadDiagonal.comp (measurable_pi_iff.mpr fun i =>
    measurable_introWander q squiggleBoyReverbIntroOrder s.diffusion i)

theorem measurable_wanderDirectTransfer (s : FrozenState) (q : ℂ) :
    Measurable (fun x => directTransfer (wanderSpec s x) q) := by
  have h : Measurable (fun x : WanderSample => introResponse q _ (wanderSpec s x).intro) :=
    measurable_pi_iff.mpr fun i =>
      (measurable_introWander q squiggleBoyReverbIntroOrder s.diffusion i).comp measurable_fst
  exact (measurable_wanderForward s q).fun_mul (measurable_quadDiagonal.comp h)

/-- The expected coherent wet transfer of the frozen reverb under the
independent per-lane model for the stationary wandering clocks. -/
def expectedWanderTransfer (s : FrozenState) (q : ℂ) : QuadMatrix :=
  expectedTransfer wanderLaw (wanderSpec s) q

/-- The expected transfer sums the means of repeated traversals of the same
draw, with a uniform norm bound on the loop return. -/
theorem expectedWanderTransfer_eq_tsum (s : FrozenState) (q : ℂ) {r b : ℝ}
    (hr0 : 0 ≤ r) (hr : r < 1)
    (hloop : ∀ᵐ x ∂wanderLaw, ‖loopReturn (wanderSpec s x) q‖ ≤ r)
    (hdrive : ∀ᵐ x ∂wanderLaw, ‖directTransfer (wanderSpec s x) q‖ ≤ b) :
    expectedWanderTransfer s q = ∑' n, ∫ x, returnTerm (wanderSpec s x) q n ∂wanderLaw :=
  expectedTransfer_eq_tsum hr0 hr (measurable_wanderLoopReturn s q)
    (measurable_wanderDirectTransfer s q) hloop hdrive

end

end DSP.SquiggleBoyQuadReverb.Frozen
