import DSP.Analysis.ForcedResponse
import DSP.ResponseMeasurement.Circle
import DSP.ResponseMeasurement.Uniform

/-! The time-first small-signal argument for a stable filtered observation.
The drive is a scalar probe. One probe per input supplies the columns of a
multiport transfer. -/
namespace DSP.ResponseMeasurement

open Filter
open scoped Topology

noncomputable section

theorem smallSignal_of_stable_update {E : Type*} [NormedAddCommGroup E] [NormedSpace ℝ E]
    [CompleteSpace E] {f : E × ℝ → E} (hf : f 0 = 0) (D : E × ℝ →L[ℝ] E)
    (hD : HasStrictFDerivAt f D 0)
    (continuousUpdate : Continuous f) (g : StabilityGauge (LocalDynamics.statePart D).toLinearMap)
    (out : E →L[ℝ] ℝ) (theta : Phase) (amplitude : ℂ)
    (hlinear : HasCoherentResponse (fourier 1 theta)
      (fun n => out (LocalDynamics.orbit D (fun k => ((fourier 1 theta)^k).re) 0 (n+1))) amplitude) :
    HasSmallSignalResponse (fourier 1 theta)
      (fun e n => out (LocalDynamics.orbit f (fun k => e * ((fourier 1 theta)^k).re) 0 (n+1))) amplitude := by
  let z := fourier 1 theta
  have hz : ‖z‖ = 1 := Circle.norm_coe _
  apply smallSignal_of_uniform_linearization z hz _
    (fun n => out (LocalDynamics.orbit D (fun k => (z^k).re) 0 (n+1))) _ hlinear
  · let F : C(E × ℝ,E) := ⟨f, continuousUpdate⟩
    let observe : C(E,ℝ) := ⟨out, out.continuous⟩
    let m : LocalDynamics.LocalContraction F g.size := LocalDynamics.localContraction D hD g
    let T := phaseRotation theta
    have hδ := div_pos m.radius_pos m.gain_pos
    filter_upwards [self_mem_nhdsWithin,
      (show ∀ᶠ e in 𝓝[>] (0 : ℝ), e < m.radius/m.gain from nhdsWithin_le_nhds (gt_mem_nhds hδ))]
      with e he heSmall
    change 0 < e at he
    let drive : C(Phase,ℝ) := ⟨fun x => e*(fourier 1 x).re, by fun_prop⟩
    have hu (x : Phase) : |drive x| ≤ e := by
      change |e * (fourier 1 x).re| ≤ e
      rw [abs_mul, abs_of_pos he]
      exact mul_le_of_le_one_right he.le ((Complex.abs_re_le_norm _).trans_eq (Circle.norm_coe _))
    have hs : m.gain * e ≤ m.radius := by
      have h := (lt_div_iff₀ m.gain_pos).mp heSmall
      nlinarith
    obtain ⟨settled, hsettled⟩ := LocalDynamics.pullback_converges F hf T drive g m e he.le hs hu
    let waveform : C(Phase,ℝ) := (observe.comp settled).comp ⟨T,T.continuous⟩
    have htrack := LocalDynamics.observation_tracks_pullback F T drive settled hsettled observe 0
    have huEq : (fun i => drive (T^[i] 0)) = fun i => e*(z^i).re := by
      funext i
      simp only [T, phaseRotation_iterate, drive, ContinuousMap.coe_mk, fourier_orbit, z]
    rw [huEq] at htrack
    have htrack' : Tendsto (fun n => out (LocalDynamics.orbit f
        (fun k => e*(z^k).re) 0 (n+1)) - waveform (n • theta)) atTop (𝓝 0) := by
      have h := htrack.comp (tendsto_add_atTop_nat 1)
      apply h.congr
      intro n
      dsimp only [Function.comp_def]
      rw [show T^[n+1] 0 = (n+1) • theta from phaseRotation_iterate theta (n+1)]
      simp [waveform, T, phaseRotation, add_nsmul, F, observe]
    obtain ⟨a, ha⟩ := circle_coherent_exists theta waveform
    exact ⟨a, ha.of_tracking hz htrack'⟩
  · intro eta heta
    obtain ⟨delta, hd, hbound⟩ := LocalDynamics.uniform_observation hf D hD g out.map_zero
      out out.hasFDerivAt eta heta
    refine ⟨delta, hd, fun e he heSmall n => ?_⟩
    have hu (i : ℕ) : |(z^i).re| ≤ 1 :=
      (Complex.abs_re_le_norm _).trans_eq (by simp [norm_pow,hz])
    simpa only [abs_of_pos he] using hbound e (by simpa only [abs_of_pos he] using heSmall)
      (fun i => (z^i).re) hu (n+1)

end
end DSP.ResponseMeasurement
