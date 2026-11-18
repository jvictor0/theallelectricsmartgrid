import DSP.ResponseMeasurement.Uniform
import DSP.ResponseFrequency.Proofs
import Mathlib.Analysis.Fourier.AddCircle

/-! Every continuous waveform of one digital phase has a long-time mean,
including rational and irrational phase increments. Fourier density proves
existence without requiring an integer sample period. -/

noncomputable section
namespace DSP.ResponseMeasurement
open Filter Finset
open scoped Topology

abbrev Phase := AddCircle (1:ℝ)

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

theorem unitDelay_eq_fourier_inv (frequency : ℝ) :
    ResponseFrequency.unitDelay frequency =
      (fourier 1 (frequency : ResponseMeasurement.Phase))⁻¹ := by
  rw [fourier_coe_apply, ← Complex.exp_neg]
  unfold ResponseFrequency.unitDelay
  congr 1
  push_cast
  ring


theorem fourier_arg_add (m : ℤ) (x y : Phase) :
    fourier m (x+y) = fourier m x*fourier m y := by
  simp [fourier_apply,AddCircle.toCircle_add]

theorem fourier_orbit (m : ℤ) (theta : Phase) (n : ℕ) :
    fourier m (n • theta) = (fourier m theta)^n := by
  induction n with
  | zero => simp
  | succ n ih => rw [add_nsmul,one_nsmul,fourier_arg_add,ih,pow_succ]

theorem circle_mode_average_exists (m : ℤ) (theta : Phase) :
    ∃ a, Tendsto (average (fun n => fourier m (n • theta))) atTop (𝓝 a) := by
  simp_rw [fourier_orbit]
  have hz : ‖fourier m theta‖ = 1 := Circle.norm_coe _
  by_cases he : fourier m theta = 1
  · refine ⟨1,?_⟩
    simp only [he,one_pow]
    apply tendsto_const_nhds.congr'
    filter_upwards [eventually_ge_atTop 1] with n hn
    exact (average_const 1 n (by omega)).symm
  · exact ⟨0,average_mode_tendsto_zero _ hz he⟩

theorem circle_span_average_exists (theta : Phase) (f : C(Phase,ℂ))
    (hf : f ∈ Submodule.span ℂ (Set.range (fourier (T := (1:ℝ))))) :
    ∃ a, Tendsto (average (fun n => f (n • theta))) atTop (𝓝 a) := by
  induction hf using Submodule.span_induction with
  | mem f hf =>
    obtain ⟨m,rfl⟩ := hf
    exact circle_mode_average_exists m theta
  | zero =>
    refine ⟨0,?_⟩
    change Tendsto (fun count => average (fun _ => (0:ℂ)) count) _ _
    simp [average]
  | add f g _ _ hf hg =>
    obtain ⟨a,ha⟩ := hf
    obtain ⟨b,hb⟩ := hg
    refine ⟨a+b,?_⟩
    change Tendsto (fun count => average (fun n => f (n • theta)+g (n • theta)) count) _ _
    simpa [average,Finset.sum_add_distrib,smul_add] using ha.add hb
  | smul c f _ hf =>
    obtain ⟨a,ha⟩ := hf
    refine ⟨c*a,?_⟩
    change Tendsto (fun count => average (fun n => c*f (n • theta)) count) _ _
    simpa [average,ContinuousMap.smul_apply,smul_eq_mul,← Finset.mul_sum,
      Complex.real_smul,mul_assoc,mul_left_comm] using ha.const_mul c

theorem circle_average_exists (theta : Phase) (f : C(Phase,ℂ)) :
    ∃ a, Tendsto (average (fun n => f (n • theta))) atTop (𝓝 a) := by
  apply average_exists_of_approximation
  intro eta heta
  have hf : f ∈ (Submodule.span ℂ (Set.range (fourier (T := (1:ℝ))))).topologicalClosure := by
    rw [span_fourier_closure_eq_top]
    trivial
  obtain ⟨g,hg,hd⟩ := Metric.mem_closure_iff.mp hf eta heta
  refine ⟨fun n => g (n • theta),circle_span_average_exists theta g hg,?_⟩
  intro n
  exact ((f-g).norm_coe_le_norm (n • theta)).trans (by simpa only [dist_eq_norm] using hd.le)

theorem circle_coherent_exists (theta : Phase) (f : C(Phase,ℝ)) :
    ∃ a, HasCoherentResponse (fourier 1 theta) (fun n => f (n • theta)) a := by
  let h : C(Phase,ℂ) := ⟨fun x => (f x : ℂ)*star (fourier 1 x),by fun_prop⟩
  obtain ⟨a,ha⟩ := circle_average_exists theta h
  refine ⟨2*a,?_⟩
  change Tendsto (fun count => coherent (fourier 1 theta) (fun n => f (n • theta)) count) _ _
  simpa only [HasCoherentResponse,coherent,h,ContinuousMap.coe_mk,fourier_orbit,star_pow] using
    ha.const_mul (2:ℂ)

end DSP.ResponseMeasurement
