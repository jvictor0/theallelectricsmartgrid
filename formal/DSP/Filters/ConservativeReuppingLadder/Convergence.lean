import DSP.Filters.ConservativeReuppingLadder.Sinusoid
import DSP.Analysis.ForcedResponse
import DSP.ResponseMeasurement.Circle

/-! Time-first small-signal response of the actual conservative ladder. -/

noncomputable section
namespace DSP.ConservativeReuppingLadder
open Filter ResponseMeasurement
open scoped Topology

def phaseRotation (theta : Phase) : Phase ≃ₜ Phase where
  toFun x := x+theta
  invFun x := x-theta
  left_inv x := by simp
  right_inv x := by simp
  continuous_toFun := continuous_id.add continuous_const
  continuous_invFun := continuous_id.sub continuous_const

theorem phaseRotation_iterate (theta : Phase) (n : ℕ) : (phaseRotation theta)^[n] 0 = n • theta := by
  induction n with
  | zero => simp
  | succ n ih => rw [Function.iterate_succ_apply',ih]; simp [phaseRotation,add_nsmul]

/-- Every sufficiently small sinusoid approaches a continuous waveform of
its digital phase. The caller supplies no convergence assumption. -/
theorem prepared_phase_response (spec : LogicalSpec ℝ) (s : ConservativeReuppingLadder ℝ)
    (hc : 0 < spec.cutoff) (hr0 : 0 ≤ spec.resonance) (hr1 : spec.resonance ≤ 1)
    (hg : 0 < spec.gain) (theta : Phase) :
    let f := computeCoefficients spec s
    ∃ delta > 0, ∀ epsilon, 0 < epsilon → epsilon < delta → ∃ waveform : C(Phase,ℝ),
      Tendsto (fun n =>
        (trajectory (reset f) (fun i => epsilon*((fourier 1 theta)^i).re) (n+1)).output-
          waveform (n • theta)) atTop (𝓝 0) := by
  dsimp only
  let f := computeCoefficients spec s
  let F : C(History ℝ × ℝ,History ℝ) := ⟨stateUpdate f,(stateUpdate_contDiff f).continuous⟩
  let out : C(History ℝ,ℝ) := ⟨observation f,(observation_contDiff f).continuous⟩
  let g := preparedGauge spec s hc hr0 hr1 hg
  let m : LocalDynamics.LocalContraction F g.size := LocalDynamics.localContraction
    (linearUpdate f).toContinuousLinearMap (stateUpdate_hasStrictFDerivAt f) g
  refine ⟨m.radius/m.gain,div_pos m.radius_pos m.gain_pos,?_⟩
  intro epsilon he heSmall
  let drive : C(Phase,ℝ) := ⟨fun x => epsilon*(fourier 1 x).re,by fun_prop⟩
  let T := phaseRotation theta
  have hu (x : Phase) : |drive x| ≤ epsilon := by
    change |epsilon*(fourier 1 x).re| ≤ epsilon
    rw [abs_mul,abs_of_pos he]
    apply mul_le_of_le_one_right he.le
    exact (Complex.abs_re_le_norm _).trans_eq (Circle.norm_coe _)
  have hs : m.gain*epsilon ≤ m.radius := by
    have h := (lt_div_iff₀ m.gain_pos).mp heSmall
    nlinarith
  obtain ⟨settled,hsettled⟩ := LocalDynamics.pullback_converges F (stateUpdate_zero f) T drive g m
    epsilon he.le hs hu
  let waveform : C(Phase,ℝ) := (out.comp settled).comp ⟨T,T.continuous⟩
  have htrack := LocalDynamics.observation_tracks_pullback F T drive settled hsettled out 0
  have huEq : (fun i => drive (T^[i] 0)) = fun i => epsilon*((fourier 1 theta)^i).re := by
    funext i
    simp only [T,phaseRotation_iterate,drive,ContinuousMap.coe_mk,fourier_orbit]
  rw [huEq] at htrack
  have htrack' : Tendsto (fun n =>
      (trajectory (reset f) (fun i => epsilon*((fourier 1 theta)^i).re) (n+1)).output-
        waveform (n • theta)) atTop (𝓝 0) := by
    have h := htrack.comp (tendsto_add_atTop_nat 1)
    apply h.congr
    intro n
    rw [output_eq_observation]
    dsimp only [Function.comp_def]
    rw [show T^[n+1] 0 = (n+1) • theta from phaseRotation_iterate theta (n+1)]
    simp [waveform,T,phaseRotation,add_nsmul,F,out]
  exact ⟨waveform,htrack'⟩

/-- The inner time limit exists at every sufficiently small positive
amplitude, including at frequencies with no integer sample period. -/
theorem prepared_settled_response (spec : LogicalSpec ℝ) (s : ConservativeReuppingLadder ℝ)
    (hc : 0 < spec.cutoff) (hr0 : 0 ≤ spec.resonance) (hr1 : spec.resonance ≤ 1)
    (hg : 0 < spec.gain) (theta : Phase) :
    let f := computeCoefficients spec s
    ∃ delta > 0, ∀ epsilon, 0 < epsilon → epsilon < delta → ∃ a,
      HasCoherentResponse (fourier 1 theta)
        (fun n => (trajectory (reset f) (fun i => epsilon*((fourier 1 theta)^i).re) (n+1)).output) a := by
  dsimp only
  obtain ⟨delta,hd,hphase⟩ := prepared_phase_response spec s hc hr0 hr1 hg theta
  refine ⟨delta,hd,?_⟩
  intro epsilon he heSmall
  obtain ⟨waveform,htrack⟩ := hphase epsilon he heSmall
  obtain ⟨a,ha⟩ := circle_coherent_exists theta waveform
  exact ⟨a,ha.of_tracking (Circle.norm_coe _) htrack⟩

/-- The original nested limit, with time taken to infinity first, equals the
proved digital transfer. The hypotheses are solely tuning and the usual
non-DC/non-Nyquist requirement of the coherent measurement. -/
theorem prepared_hasSmallSignalResponse (spec : LogicalSpec ℝ) (s : ConservativeReuppingLadder ℝ)
    (hc : 0 < spec.cutoff) (hr0 : 0 ≤ spec.resonance) (hr1 : spec.resonance ≤ 1)
    (hg : 0 < spec.gain) (theta : Phase)
    (hfrequency : star (fourier 1 theta)*star (fourier 1 theta) ≠ 1) :
    let f := computeCoefficients spec s
    HasSmallSignalResponse (fourier 1 theta)
      (fun epsilon n => (trajectory (reset f) (fun i => epsilon*((fourier 1 theta)^i).re) (n+1)).output)
      (transfer (f.alpha:ℂ) ((NormalizedTanh.slopeZero f.saturation:ℝ):ℂ) (f.k:ℂ) (fourier 1 theta)⁻¹) := by
  dsimp only
  let f := computeCoefficients spec s
  let z := fourier 1 theta
  have hz : ‖z‖ = 1 := Circle.norm_coe _
  apply smallSignal_of_uniform_linearization z hz _
    (fun n => (linearTrajectory (reset f) (fun i => (z^i).re) (n+1)).output)
  · exact coherent_of_sinusoid_tracking z _ hz hfrequency _
      (prepared_linear_tracks spec s hc hr0 hr1 hg z hz)
  · obtain ⟨delta,hd,hsettled⟩ := prepared_settled_response spec s hc hr0 hr1 hg theta
    filter_upwards [self_mem_nhdsWithin,
      (show ∀ᶠ e in 𝓝[>] (0:ℝ), e < delta from nhdsWithin_le_nhds (gt_mem_nhds hd))]
      with e he heSmall
    exact hsettled e he heSmall
  · intro eta heta
    obtain ⟨delta,hd,hbound⟩ := prepared_uniform_output spec s hc hr0 hr1 hg eta heta
    refine ⟨delta,hd,?_⟩
    intro epsilon he heSmall
    have hu (i : ℕ) : |(z^i).re| ≤ 1 := by
      exact (Complex.abs_re_le_norm _).trans_eq (by simp [norm_pow,hz])
    simpa only [abs_of_pos he] using hbound epsilon (by simpa only [abs_of_pos he] using heSmall)
      (fun i => (z^i).re) hu

end DSP.ConservativeReuppingLadder
