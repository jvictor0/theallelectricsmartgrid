import DSP.Filters.NaiveReuppingLadder.PeriodicWaveRegion
import DSP.Filters.NaiveReuppingLadder.DetectorDrySensitivity
import DSP.Filters.BandPassedEnvelopeFollower.TransientGain

noncomputable section
namespace DSP.NaiveReuppingLadder.PeriodicWave.Setup
open Asymptotic DetectorDynamics BandPassedEnvelopeFollower.Periodic Filter
open scoped Topology
variable (s : Setup)

abbrev trace (z : Joint) := prefixRun s.k s.g s.a s.u z
def lastStage (z : Joint) (i : ℕ) := (s.trace z i).2.1.2.2.2
private def dryPower (z : Joint) (i : ℕ) := max 0 (s.trace z i).1.2.2
private def wetPower (z : Joint) (i : ℕ) := (s.trace z i).2.2.2.2
private def coeff (z : Joint) (i : ℕ) := reduction s.k.K s.k.D s.k.R (s.dryPower z i) (s.wetPower z i)
private def inputToFirst (z : Joint) (i : ℕ) := s.u i-s.coeff z i*s.lastStage z i

private theorem pole_gap (x y v w : ℕ → ℝ) (B : ℝ) (h : Bound (difference x y) B)
    (hv : ∀ i, v (i+1) = OnePoleZOHLowPass.onePoleStep s.k.alpha (x i) (v i))
    (hw : ∀ i, w (i+1) = OnePoleZOHLowPass.onePoleStep s.k.alpha (y i) (w i)) :
    Bound (difference v w) B := by
  apply Bound.smooth _ s.alpha s.alpha_le h
  intro i
  dsimp only [difference]
  rw [hv,hw,OnePoleZOHLowPass.onePoleStep_eq,OnePoleZOHLowPass.onePoleStep_eq,fma_eq,fma_eq]
  rw [show s.k.alpha*x i+(1-s.k.alpha)*v i-(s.k.alpha*y i+(1-s.k.alpha)*w i) =
    (1-s.k.alpha)*(v i-w i)+s.k.alpha*(x i-y i) by ring]
  refine (abs_add_le _ _).trans ?_
  rw [abs_mul,abs_mul,abs_of_nonneg (sub_nonneg.mpr s.alpha_le),abs_of_nonneg s.alpha.le]

private theorem last_bound (z : Joint) : Bound (fun i => |s.lastStage z i|) s.M := by
  let v := fun i => NormalizedTanh.apply s.k.sat (s.trace z (i+1)).2.1.2.2.1
  have h := s.pole_gap v (fun _ => 0) (s.lastStage z) (fun _ => 0) s.M
    (Bound.of_le (fun i => by simp only [difference,sub_zero]; exact Exploration.saturation_bound _ s.satDen _))
    (fun _ => rfl) (fun _ => by simp)
  exact h.mono (fun i => by simp [difference])

private theorem product_gap (a b x y : ℝ) : |a*x-b*y| ≤ |a| * |x-y| + |a-b| * |y| := by
  rw [show a*x-b*y = a*(x-y)+(a-b)*y by ring]
  simpa only [abs_mul] using abs_add_le (a*(x-y)) ((a-b)*y)

private theorem coeff_bound (z : Joint) (i : ℕ) : |s.coeff z i| ≤ s.k.K := by
  let p := s.dryPower z i
  have hd : 1 ≤ 1+s.k.D*(max 0 (s.wetPower z i-s.k.R*p)/(p+powerFloor)) :=
    le_add_of_nonneg_right (mul_nonneg s.damping (div_nonneg (le_max_left _ _)
      (add_pos_of_nonneg_of_pos (le_max_left _ _) floor_pos).le))
  unfold coeff reduction
  rw [abs_of_nonneg (div_nonneg s.feedback (zero_le_one.trans hd))]
  exact div_le_self s.feedback hd

/-- For an arbitrary starting history in sample time, both the fourth stage and
the emitted output track the unique periodic waveform throughout the small-gain region. -/
theorem sample_tracking (b : s.Wave) (hb : s.update b = b) (pMin : ℝ)
    (hp : 0 ≤ pMin) (hlo : ∀ i, pMin ≤ (s.dry i).2.2) (hsmall : s.L pMin < 1) (z : Joint) :
    let ref := s.assemble b 0
    Tendsto (difference (s.lastStage z) (s.lastStage ref)) atTop (𝓝 0) ∧
      Tendsto (fun n => readout s.k (s.trace z n) (s.u n)-readout s.k (s.trace ref n) (s.u n))
        atTop (𝓝 0) := by
  let ref := s.assemble b 0
  let E := s.envelopeGain s.M
  let Lq := s.k.K*s.k.D/(pMin+powerFloor)
  let H := s.k.K+s.M*Lq*E
  have hE := s.envelopeGain_nonneg s.M s.M_pos.le
  have hLq : 0 ≤ Lq := div_nonneg (mul_nonneg s.feedback s.damping) (add_nonneg hp floor_pos.le)
  have hH : 0 ≤ H := add_nonneg s.feedback (mul_nonneg (mul_nonneg s.M_pos.le hLq) hE)
  have hL : 0 ≤ s.L pMin := mul_nonneg (pow_nonneg s.S_nonneg _) hH
  have heq (n : ℕ) : s.dryPower ref n = (s.dry n).2.2 := by
    change max 0 (prefixRun _ _ _ _ (s.assemble b 0) n).1.2.2 = _
    rw [s.assemble_run b hb]
    exact max_eq_right (s.dry_nonneg n)
  have dry : Bound (difference (s.dryPower z) (s.dryPower ref)) 0 := by
    have hh := (tracks_orbit s.g s.a s.detectorGain s.detectorAlpha s.detectorAlpha_le
      s.u s.N s.positivePeriod s.periodic z.1).snd_nhds.snd_nhds
    have ht : Tendsto (fun i => (s.trace z i).1.2.2-(s.dry i).2.2) atTop (𝓝 0) := by
      change Tendsto (fun n => (Step.states (advance s.g s.a) s.u z.1 n).2.2 - (s.dry n).2.2) atTop (𝓝 0) at hh
      simpa only [trace,dry_prefix] using hh
    apply (Bound.of_tendsto (by simpa only [abs_zero] using ht.abs)).mono
    intro i
    rw [difference,heq i]
    change |max 0 _ - (s.dry i).2.2| ≤ _
    simpa only [max_comm _ (0:ℝ),max_eq_right (s.dry_nonneg i)] using
      abs_max_sub_max_le_abs (s.trace z i).1.2.2 (s.dry i).2.2 0
  have wet (B : ℝ) (h : Bound (difference (s.lastStage z) (s.lastStage ref)) B) :
      Bound (difference (s.wetPower z) (s.wetPower ref)) (E*B) :=
    transient_power_difference s.g s.a s.detectorGain s.detectorAlpha s.detectorAlpha_le
      (fun i => (s.trace z i).2.2) (fun i => (s.trace ref i).2.2)
      (fun i => s.lastStage z (i+1)) (fun i => s.lastStage ref (i+1))
      (fun _ => rfl) (fun _ => rfl) s.M B (s.last_bound z).shift (s.last_bound ref).shift h.shift
  have ctl (B : ℝ) (h : Bound (difference (s.lastStage z) (s.lastStage ref)) B) :
      Bound (difference (s.coeff z) (s.coeff ref)) (Lq*(E*B)) := by
    let D := (s.k.K/(powerFloor:ℝ))*(1+s.k.D*|s.k.R|)
    have hD : 0 ≤ D := mul_nonneg (div_nonneg s.feedback floor_pos.le)
      (add_nonneg zero_le_one (mul_nonneg s.damping (abs_nonneg _)))
    have hh := (dry.scale hD).add ((wet B h).scale hLq)
    rw [mul_zero,zero_add] at hh
    apply hh.mono
    intro i
    let p := s.dryPower ref i
    let Q := s.wetPower z i
    let q := s.wetPower ref i
    have hp' : 0 ≤ p := le_max_left _ _
    have hden (v : ℝ) : 1 ≤ 1+s.k.D*(max 0 (v-s.k.R*p)/(p+powerFloor)) :=
      le_add_of_nonneg_right (mul_nonneg s.damping (div_nonneg (le_max_left _ _) (add_nonneg hp' floor_pos.le)))
    have hq := reduction_wet_lipschitz s.k.K s.k.D s.k.R p Q q 1 s.feedback s.damping hp'
      zero_lt_one (hden Q) (hden q)
    simp only [one_pow,mul_one] at hq
    have hcoef : s.k.K*s.k.D/(p+powerFloor) ≤ Lq :=
      div_le_div_of_nonneg_left (mul_nonneg s.feedback s.damping) (add_pos_of_nonneg_of_pos hp floor_pos)
        (add_le_add (heq i ▸ hlo i : pMin ≤ s.dryPower ref i) le_rfl)
    exact (abs_sub_le (s.coeff z i) (reduction s.k.K s.k.D s.k.R p Q) (s.coeff ref i)).trans
      (add_le_add (reduction_dry_lipschitz _ _ _ _ p Q s.feedback s.damping (le_max_left _ _) hp')
        (hq.trans (mul_le_mul_of_nonneg_right hcoef (abs_nonneg _))))
  have hstart : Bound (difference (s.lastStage z) (s.lastStage ref)) (s.M+s.M) :=
    ((s.last_bound z).add (s.last_bound ref)).mono (fun i => abs_sub _ _)
  have hloop (B : ℝ) (h : Bound (difference (s.lastStage z) (s.lastStage ref)) B) :
      Bound (difference (s.lastStage z) (s.lastStage ref)) (s.L pMin*B) := by
    have hfeedback := (ctl B h).mul (s.last_bound ref) (fun _ => abs_nonneg _) (fun _ => abs_nonneg _)
    have hsum := (h.scale s.feedback).add hfeedback
    have hdrive : Bound (difference (s.inputToFirst z) (s.inputToFirst ref)) (H*B) := by
      rw [show H*B = s.k.K*B+Lq*(E*B)*s.M by dsimp [H]; ring]
      apply hsum.mono
      intro i
      dsimp only [inputToFirst,difference]
      rw [sub_sub_sub_cancel_left,abs_sub_comm]
      exact (product_gap _ _ _ _).trans (add_le_add
        (mul_le_mul_of_nonneg_right (s.coeff_bound z i) (abs_nonneg _)) le_rfl)
    let A := fun t i => (s.trace t i).2.1
    have h1 := s.pole_gap (s.inputToFirst z) (s.inputToFirst ref)
      (fun i => (A z i).1) (fun i => (A ref i).1) (H*B) hdrive (fun _ => rfl) (fun _ => rfl)
    have chain (v w x y : ℕ → ℝ) (B₁ : ℝ) (hx : Bound (difference x y) B₁)
        (hv : ∀ i, v (i+1) = OnePoleZOHLowPass.onePoleStep s.k.alpha (NormalizedTanh.apply s.k.sat (x (i+1))) (v i))
        (hw : ∀ i, w (i+1) = OnePoleZOHLowPass.onePoleStep s.k.alpha (NormalizedTanh.apply s.k.sat (y (i+1))) (w i)) :
        Bound (difference v w) (s.S*B₁) :=
      s.pole_gap _ _ v w _ ((hx.shift.scale s.S_nonneg).mono (fun i => s.sat_difference (x (i+1)) (y (i+1)))) hv hw
    have h2 := chain (fun i => (A z i).2.1) (fun i => (A ref i).2.1) _ _ _ h1 (fun _ => rfl) (fun _ => rfl)
    have h3 := chain (fun i => (A z i).2.2.1) (fun i => (A ref i).2.2.1) _ _ _ h2 (fun _ => rfl) (fun _ => rfl)
    have h4 := chain (s.lastStage z) (s.lastStage ref) _ _ _ h3 (fun _ => rfl) (fun _ => rfl)
    change Bound _ (s.S^3*H*B)
    rwa [show s.S^3*H*B = s.S*(s.S*(s.S*(H*B))) by ring]
  have hlast := Bound.tendsto_of_feedback (fun _ => abs_nonneg _) hL hsmall (add_nonneg s.M_pos.le s.M_pos.le)
    hstart (fun A _ => hloop A)
  have hdz := ctl 0 (Bound.of_tendsto hlast)
  simp only [mul_zero] at hdz
  obtain ⟨v,hv,hbnd⟩ := hdz
  have hcoef := squeeze_zero (fun i => abs_nonneg (s.coeff z i - s.coeff ref i)) hbnd hv
  refine ⟨hlast,?_⟩
  apply tendsto_zero_iff_norm_tendsto_zero.mpr
  have hh := ((hlast.comp (tendsto_add_atTop_nat 1)).const_mul ((s.k.K+1)*s.S)).add
    (hcoef.const_mul s.M)
  simp only [mul_zero,add_zero] at hh
  apply squeeze_zero (fun _ => norm_nonneg _) _ hh
  intro i
  let sat := NormalizedTanh.apply s.k.sat
  change |sat (s.lastStage z (i+1))*(s.coeff z i+1)-sat (s.lastStage ref (i+1))*(s.coeff ref i+1)| ≤ _
  rw [mul_comm (sat _),mul_comm (sat _)]
  have h1 := product_gap (s.coeff z i+1) (s.coeff ref i+1) (sat (s.lastStage z (i+1))) (sat (s.lastStage ref (i+1)))
  have hk : |s.coeff z i+1| ≤ s.k.K+1 := (abs_add_le _ _).trans (by simpa using add_le_add (s.coeff_bound z i) (le_refl (1:ℝ)))
  have h2 := mul_le_mul hk (s.sat_difference (s.lastStage z (i+1)) (s.lastStage ref (i+1)))
    (abs_nonneg _) (by linarith [s.feedback])
  have h3 := mul_le_mul_of_nonneg_left (Exploration.saturation_bound s.k.sat s.satDen (s.lastStage ref (i+1)))
    (abs_nonneg (s.coeff z i-s.coeff ref i))
  simp only [add_sub_add_right_eq_sub] at h1
  refine h1.trans ((add_le_add h2 h3).trans_eq ?_)
  dsimp only [Function.comp_def,difference,M,sat]
  ring

/-- Long-run fundamental phasor and total mean-square of the actual sample
stream from any prepared history with nonnegative dry power, including reset. -/
theorem measured_from_history (b : s.Wave) (hb : s.update b = b) (pMin : ℝ)
    (hp : 0 ≤ pMin) (hlo : ∀ i, pMin ≤ (s.dry i).2.2) (hsmall : s.L pMin < 1)
    (initial : NaiveReuppingLadder ℝ) (hk : knobs initial = s.k)
    (hd : Tuned initial.inputEnvelope s.g s.a) (hw : Tuned initial.feedbackEnvelope s.g s.a)
    (hpow : 0 ≤ initial.inputEnvelope.power) (z : ℂ) (hz : ‖z‖ = 1) (hN : z^s.N = 1) :
    let y := fun i => (run initial s.u (i+1)).output
    let ref := fun i => readout s.k (s.assemble b i) (s.u i)
    ResponseMeasurement.HasCoherentResponse z y (ResponseMeasurement.coherent z ref s.N) ∧
      ResponseMeasurement.HasMeanSquare y (ResponseMeasurement.meanSquare ref s.N) := by
  let y := fun i => (run initial s.u (i+1)).output
  let ref := fun i => readout s.k (s.assemble b i) (s.u i)
  have he (n : ℕ) : DetectorDynamics.history (run initial s.u n) = s.trace (DetectorDynamics.history initial) n := by
    rw [physical_run initial _ _ hd hw s.detectorAlpha.le s.detectorAlpha_le hpow,hk]
  have ht (n : ℕ) : Tuned (run initial s.u n).inputEnvelope s.g s.a ∧ 0 ≤ (run initial s.u n).inputEnvelope.power := by
    induction n with
    | zero => exact ⟨hd,hpow⟩
    | succ n ih => exact ⟨tuned_step _ _ _ _ ih.1, Exploration.detector_power_nonnegative _ _
        (ih.1.2.2.2.2.symm ▸ s.detectorAlpha.le) (ih.1.2.2.2.2.symm ▸ s.detectorAlpha_le) ih.2⟩
  have hy (i : ℕ) : y i = readout s.k (s.trace (DetectorDynamics.history initial) i) (s.u i) := by
    change ((run initial s.u i).step _).output = _
    rw [physical_readout _ _ (ht i).2,knobs_run,hk,he]
  have hr : Function.Periodic ref s.N := fun i => by
    dsimp only [ref]
    rw [s.assemble_period b i,s.periodic i]
  have ht' := (s.sample_tracking b hb pMin hp hlo hsmall (DetectorDynamics.history initial)).2
  have limit : Tendsto (fun i => y i-ref i) atTop (𝓝 0) := by
    simpa only [hy,ref,trace,s.assemble_run b hb] using ht'
  exact ⟨ResponseMeasurement.coherent_of_periodic_tracking z hz y ref s.N s.positivePeriod hN hr limit,
    ResponseMeasurement.meanSquare_of_periodic_tracking y ref s.N s.positivePeriod hr limit⟩

end DSP.NaiveReuppingLadder.PeriodicWave.Setup
