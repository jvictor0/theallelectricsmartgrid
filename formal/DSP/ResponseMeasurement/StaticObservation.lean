import DSP.ResponseMeasurement.Circle
import Mathlib.Topology.MetricSpace.Lipschitz
import Mathlib.Analysis.Calculus.FDeriv.Basic

/-! A differentiable memoryless observation of a settled linear filter bank.
The time limit is established before taking the small-amplitude limit. -/
noncomputable section
namespace DSP.ResponseMeasurement
open Filter
open scoped Topology
variable {E : Type*} [NormedAddCommGroup E] [NormedSpace ℝ E]

omit [NormedSpace ℝ E] in
theorem lipschitz_tracks {out : E → ℝ} {C : NNReal} (hc : LipschitzWith C out)
    (u v : ℕ → E) (h : Tendsto (fun n => u n-v n) atTop (𝓝 0)) :
    Tendsto (fun n => out (u n)-out (v n)) atTop (𝓝 0) := by
  apply tendsto_zero_iff_norm_tendsto_zero.mpr
  have ht : Tendsto (fun n => (C:ℝ)*‖u n-v n‖) atTop (𝓝 0) := by simpa using h.norm.const_mul (C:ℝ)
  exact squeeze_zero (fun _ => norm_nonneg _) (fun n => by
    simpa only [dist_eq_norm] using hc.dist_le_mul (u n) (v n)) ht

theorem uniform_static_observation {out : E → ℝ} (hz : out 0=0)
    (L : E →L[ℝ] ℝ) (hL : HasFDerivAt out L 0) (b : ℕ → E)
    (B : ℝ) (hB : 0 < B) (hb : ∀ n, ‖b n‖ ≤ B) :
    ∀ eta > 0, ∃ delta > 0, ∀ epsilon, 0 < epsilon → epsilon < delta →
      ∀ n, |out (epsilon • b n)-epsilon*L (b n)| ≤ eta*epsilon := by
  intro eta heta
  have htol : 0 < eta/B := div_pos heta hB
  have hre : ∀ᶠ x in 𝓝 (0:E), |out x-L x| ≤ (eta/B)*‖x‖ := by
    simpa only [hz,sub_zero,Real.norm_eq_abs] using hL.isLittleO.def htol
  obtain ⟨r,hr,hball⟩ := Metric.mem_nhds_iff.mp hre
  refine ⟨r/B,div_pos hr hB,?_⟩
  intro e he hed n
  have heB : e*B < r := (lt_div_iff₀ hB).mp hed
  have hen : ‖e • b n‖ ≤ e*B := by
    rw [norm_smul,Real.norm_eq_abs,abs_of_pos he]
    exact mul_le_mul_of_nonneg_left (hb n) he.le
  have h := hball (show e • b n ∈ Metric.ball 0 r by
    rw [Metric.mem_ball,dist_zero_right]; exact hen.trans_lt heB)
  change |out (e • b n)-L (e • b n)| ≤ (eta/B)*‖e • b n‖ at h
  simp only [map_smul,smul_eq_mul] at h
  exact h.trans ((mul_le_mul_of_nonneg_left hen htol.le).trans_eq (by field_simp))

theorem HasSmallSignalResponse.of_tracking {z H : ℂ} (hz : ‖z‖=1)
    {reference output : ℝ → ℕ → ℝ} (hr : HasSmallSignalResponse z reference H)
    (ht : ∀ e > 0, Tendsto (fun n => output e n-reference e n) atTop (𝓝 0)) :
    HasSmallSignalResponse z output H := by
  obtain ⟨settled,hs,hlim⟩ := hr
  refine ⟨settled,?_,hlim⟩
  filter_upwards [hs,self_mem_nhdsWithin] with e he hepos
  exact he.of_tracking hz (ht e hepos)

theorem smallSignal_of_static_observation (theta : Phase) (wave : C(Phase,E))
    {out : E → ℝ} (hc : Continuous out) (hz : out 0=0)
    (L : E →L[ℝ] ℝ) (hL : HasFDerivAt out L 0) (H : ℂ)
    (hlinear : HasCoherentResponse (fourier 1 theta) (fun n => L (wave (n • theta))) H)
    (output : ℝ → ℕ → ℝ)
    (ht : ∀ e > 0, Tendsto (fun n => output e n-out (e • wave (n • theta))) atTop (𝓝 0)) :
    HasSmallSignalResponse (fourier 1 theta) output H := by
  have hnorm : ‖fourier 1 theta‖=1 := Circle.norm_coe _
  apply HasSmallSignalResponse.of_tracking hnorm (reference := fun e n => out (e • wave (n • theta))) _ ht
  apply smallSignal_of_uniform_linearization _ hnorm _ _ H hlinear
  · apply Filter.Eventually.of_forall
    intro e
    exact circle_coherent_exists theta ⟨fun x => out (e • wave x),hc.comp (by fun_prop)⟩
  · exact uniform_static_observation hz L hL _ (1+‖wave‖) (by positivity)
      (fun n => (wave.norm_coe_le_norm _).trans (by linarith))

end DSP.ResponseMeasurement
