import DSP.Analysis.LocalDynamics

/-! Stable linear recurrences reject vanishing input errors. This allows
sinusoidal settling to compose through a tree of filters. -/
noncomputable section
namespace DSP.LocalDynamics
open Filter
open scoped Topology

theorem decay_of_forcing (r : ℝ) (hr0 : 0 ≤ r) (hr1 : r < 1)
    (error forcing : ℕ → ℝ) (he : ∀ n, 0 ≤ error n)
    (hf : Tendsto forcing atTop (𝓝 0))
    (hstep : ∀ n, error (n+1) ≤ r*error n+forcing n) :
    Tendsto error atTop (𝓝 0) := by
  rw [Metric.tendsto_atTop]
  intro eta heta
  have htol : 0 < (eta/2)*(1-r) := by positivity
  obtain ⟨N,hN⟩ := eventually_atTop.mp (hf.eventually (gt_mem_nhds htol))
  have hb (k : ℕ) : error (N+k) ≤ r^k*error N+eta/2 := by
    induction k with
    | zero => simp; linarith
    | succ k ih =>
      have hs := hstep (N+k)
      have ht := hN (N+k) (by omega)
      have hm := mul_le_mul_of_nonneg_left ih hr0
      rw [pow_succ]
      have hi : N+(k+1) = N+k+1 := by omega
      rw [hi]
      nlinarith
  have hp := (tendsto_pow_atTop_nhds_zero_of_lt_one hr0 hr1).mul_const (error N)
  simp only [zero_mul] at hp
  obtain ⟨K,hK⟩ := eventually_atTop.mp (hp.eventually (gt_mem_nhds (by linarith : 0 < eta/2)))
  refine ⟨N+K,?_⟩
  intro n hn
  have hnk : K ≤ n-N := by omega
  have hnN : N+(n-N)=n := by omega
  have h := hb (n-N)
  rw [hnN] at h
  rw [Real.dist_eq,sub_zero,abs_of_nonneg (he n)]
  linarith [hK (n-N) hnk]

variable {E : Type*} [NormedAddCommGroup E] [NormedSpace ℝ E]

theorem linear_tracks_perturbed (D : E × ℝ →L[ℝ] E)
    (g : StabilityGauge (statePart D).toLinearMap)
    (u v : ℕ → ℝ) (initial : E) (reference : ℕ → E)
    (href : ∀ n, D (reference n,v n) = reference (n+1))
    (hu : Tendsto (fun n => u n-v n) atTop (𝓝 0)) :
    Tendsto (fun n => orbit D u initial n-reference n) atTop (𝓝 0) := by
  let e := fun n => orbit D u initial n-reference n
  have heq (n : ℕ) : e (n+1) = statePart D (e n)+D (0,u n-v n) := by
    simp only [e,orbit,← href n,← map_sub,← map_add,statePart,
      ContinuousLinearMap.comp_apply,ContinuousLinearMap.inl_apply]
    congr 1
    ext <;> simp
  have ht : Tendsto (fun n => g.bound*‖D‖*|u n-v n|) atTop (𝓝 0) := by
    simpa using hu.abs.const_mul (g.bound*‖D‖)
  have hb (n : ℕ) : g.size (e (n+1)) ≤ g.rate*g.size (e n)+g.bound*‖D‖*|u n-v n| := by
    rw [heq]
    apply (map_add_le_add g.size _ _).trans
    apply add_le_add (g.contracts _)
    apply (g.le_norm _).trans
    have hd := D.le_opNorm (0,u n-v n)
    have hd' : ‖D (0,u n-v n)‖ ≤ ‖D‖*|u n-v n| := by simpa using hd
    exact (mul_le_mul_of_nonneg_left hd' g.bound_pos.le).trans_eq (by ring)
  have h := decay_of_forcing g.rate g.rate_nonneg g.rate_lt_one
    (fun n => g.size (e n)) _ (fun n => apply_nonneg _ _) ht hb
  apply tendsto_zero_iff_norm_tendsto_zero.mpr
  exact squeeze_zero (fun _ => norm_nonneg _) (fun n => g.norm_le (e n)) h

end DSP.LocalDynamics
