import DSP.Analysis.LocalDynamics
import Mathlib.Topology.ContinuousMap.Compact

/-! Settling under a continuous drive on a compact invertible phase space.
This includes rational and irrational digital sinusoidal frequencies. -/

noncomputable section
namespace DSP.LocalDynamics
open Filter
open scoped Topology

variable {E X : Type*} [NormedAddCommGroup E] [NormedSpace ℝ E]
    [TopologicalSpace X] [CompactSpace X]

def pullback (f : C(E × ℝ,E)) (T : X ≃ₜ X) (u : C(X,ℝ)) : ℕ → C(X,E)
  | 0 => 0
  | n+1 => ⟨fun x => f (pullback f T u n (T.symm x),u (T.symm x)),by fun_prop⟩

omit [CompactSpace X] in
theorem pullback_bound (f : C(E × ℝ,E)) (hf : f (0,0) = 0)
    (T : X ≃ₜ X) (u : C(X,ℝ)) {p : Seminorm ℝ E} (m : LocalContraction f p)
    (epsilon : ℝ) (he : 0 ≤ epsilon) (hsmall : m.gain*epsilon ≤ m.radius)
    (hu : ∀ x, |u x| ≤ epsilon) (n : ℕ) (x : X) :
    p (pullback f T u n x) ≤ m.gain*epsilon := by
  have hi : epsilon ≤ m.radius := (le_mul_of_one_le_left he m.gain_ge_one).trans hsmall
  induction n generalizing x with
  | zero => simpa only [pullback,ContinuousMap.zero_apply,map_zero] using mul_nonneg m.gain_pos.le he
  | succ n ih =>
    have h := m.estimate (pullback f T u n (T.symm x)) 0 (u (T.symm x)) 0
      ((ih _).trans hsmall) (by simpa using m.radius_pos.le)
      ((hu _).trans hi) (by simpa using m.radius_pos.le)
    simp only [hf,sub_zero] at h
    exact h.trans ((add_le_add (mul_le_mul_of_nonneg_left (ih _) m.rate_nonneg)
      (mul_le_mul_of_nonneg_left (hu _) m.forcing_nonneg)).trans
        (by nlinarith [mul_le_mul_of_nonneg_right m.gain_step he]))

omit [CompactSpace X] in
theorem pullback_distance (f : C(E × ℝ,E)) (hf : f (0,0) = 0)
    (T : X ≃ₜ X) (u : C(X,ℝ)) {p : Seminorm ℝ E} (m : LocalContraction f p)
    (epsilon : ℝ) (he : 0 ≤ epsilon) (hsmall : m.gain*epsilon ≤ m.radius)
    (hu : ∀ x, |u x| ≤ epsilon) (n : ℕ) (x : X) :
    p (pullback f T u (n+1) x-pullback f T u n x) ≤ m.rate^n*(m.gain*epsilon) := by
  have hb := pullback_bound f hf T u m epsilon he hsmall hu
  have hi : epsilon ≤ m.radius := (le_mul_of_one_le_left he m.gain_ge_one).trans hsmall
  induction n generalizing x with
  | zero => simpa only [pullback,ContinuousMap.zero_apply,sub_zero,pow_zero,one_mul] using hb 1 x
  | succ n ih =>
    have h := m.estimate (pullback f T u (n+1) (T.symm x))
      (pullback f T u n (T.symm x)) (u (T.symm x)) (u (T.symm x))
      ((hb _ _).trans hsmall) ((hb _ _).trans hsmall) ((hu _).trans hi) ((hu _).trans hi)
    simp only [sub_self,abs_zero,mul_zero,add_zero] at h
    exact h.trans ((mul_le_mul_of_nonneg_left (ih _) m.rate_nonneg).trans_eq (by rw [pow_succ]; ring))

omit [NormedSpace ℝ E] [CompactSpace X] in
theorem orbit_eq_pullback (f : C(E × ℝ,E)) (T : X ≃ₜ X) (u : C(X,ℝ)) (x : X) (n : ℕ) :
    orbit f (fun i => u (T^[i] x)) 0 n = pullback f T u n (T^[n] x) := by
  induction n with
  | zero => rfl
  | succ n ih =>
    rw [orbit,ih,Function.iterate_succ_apply']
    change _ = f (pullback f T u n (T.symm (T (T^[n] x))),u (T.symm (T (T^[n] x))))
    rw [T.symm_apply_apply]

theorem pullback_converges [CompleteSpace E] (f : C(E × ℝ,E)) (hf : f (0,0) = 0)
    (T : X ≃ₜ X) (u : C(X,ℝ)) {A : E →ₗ[ℝ] E} (g : StabilityGauge A)
    (m : LocalContraction f g.size) (epsilon : ℝ) (he : 0 ≤ epsilon)
    (hsmall : m.gain*epsilon ≤ m.radius) (hu : ∀ x, |u x| ≤ epsilon) :
    ∃ settled : C(X,E), Tendsto (pullback f T u) atTop (𝓝 settled) := by
  apply cauchySeq_tendsto_of_complete
  apply cauchySeq_of_le_geometric m.rate (m.gain*epsilon) m.rate_lt_one
  intro n
  rw [dist_comm,dist_eq_norm]
  apply (ContinuousMap.norm_le _
    (mul_nonneg (mul_nonneg m.gain_pos.le he) (pow_nonneg m.rate_nonneg n))).mpr
  intro x
  exact (g.norm_le _).trans ((pullback_distance f hf T u m epsilon he hsmall hu n x).trans_eq (by ring))

omit [NormedSpace ℝ E] in
theorem observation_tracks_pullback (f : C(E × ℝ,E)) (T : X ≃ₜ X) (u : C(X,ℝ))
    (settled : C(X,E)) (h : Tendsto (pullback f T u) atTop (𝓝 settled))
    (out : C(E,ℝ)) (x : X) :
    Tendsto (fun n => out (orbit f (fun i => u (T^[i] x)) 0 n)-out (settled (T^[n] x)))
      atTop (𝓝 0) := by
  have ho : Tendsto (fun n => out.comp (pullback f T u n)) atTop (𝓝 (out.comp settled)) :=
    (ContinuousMap.continuous_postcomp out).continuousAt.tendsto.comp h
  have hn := (ho.sub_const (out.comp settled)).norm
  simp only [sub_self,norm_zero] at hn
  apply tendsto_zero_iff_norm_tendsto_zero.mpr
  apply squeeze_zero (fun _ => norm_nonneg _) _ hn
  intro n
  rw [orbit_eq_pullback]
  exact (out.comp (pullback f T u n)-out.comp settled).norm_coe_le_norm (T^[n] x)

end DSP.LocalDynamics
