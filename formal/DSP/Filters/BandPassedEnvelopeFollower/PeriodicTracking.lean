import DSP.Filters.BandPassedEnvelopeFollower.Periodic

noncomputable section
namespace DSP.BandPassedEnvelopeFollower.Periodic
open Filter TPTSVF.Convergence
open scoped Topology

/-- Both taps use old and new integrator histories. -/
private theorem tap_tracks (v w : ℕ → ℂ)
    (h : Tendsto (fun n => v n-w n) atTop (𝓝 0)) :
    Tendsto (fun n => tap (v n) (v (n+1)) - tap (w n) (w (n+1))) atTop (𝓝 0) := by
  have h1 := firstState_tendsto h
  have h2 := h1.comp (tendsto_add_atTop_nat 1)
  simp only [firstState_sub, Function.comp_def] at h1 h2
  have ht := (h2.add h1).div_const 2
  simp only [zero_add, zero_div] at ht
  convert ht using 1
  ext n
  dsimp [tap]
  ring

/-- Every initial detector history tracks the same periodic waveform, including power. -/
theorem tracks_orbit (g a : ℝ) (hg : 0 < g) (ha : 0 < a) (ha1 : a ≤ 1)
    (u : ℕ → ℝ) (N : ℕ) (hN : 0 < N) (hu : Function.Periodic u N)
    (m : Memory) :
    Tendsto (fun n => Step.states (advance g a) u m n - orbit g a u N n)
      atTop (𝓝 0) := by
  let t := Step.states (advance g a) u m
  let r := orbit g a u N
  have hp := pole_norm_lt_one g hg
  have h1 : Tendsto (fun n => (t n).1 - (r n).1) atTop (𝓝 0) := by
    apply DSP.Asymptotic.stable_recurrence (pole g) hp _ (fun _ => 0) _ tendsto_const_nhds
    intro n
    change modalStep g (t n).1 (u n) - modalStep g (r n).1 (u n) = _
    unfold modalStep
    ring
  have hb := tap_tracks _ _ h1
  let b := fun n => tap (t n).1 (t (n+1)).1
  let br := fun n => tap (r n).1 (r (n+1)).1
  have h2 : Tendsto (fun n => (t n).2.1 - (r n).2.1) atTop (𝓝 0) := by
    apply DSP.Asymptotic.stable_recurrence (pole g) hp _
      (fun n => drive g * ((b n - br n : ℝ) : ℂ))
    · intro n
      change modalStep g (t n).2.1 (b n) - modalStep g (r n).2.1 (br n) = _
      simp only [modalStep, Complex.ofReal_sub]
      ring
    · have hc := (Complex.continuous_ofReal.tendsto 0).comp hb
      simpa only [Function.comp_def, Complex.ofReal_zero, mul_zero] using hc.const_mul (drive g)
  let y := fun n => tap (t n).2.1 (t (n+1)).2.1
  let yr := fun n => tap (r n).2.1 (r (n+1)).2.1
  have hr : Function.Periodic r N := orbit_periodic g a hg ha ha1 u N hN hu
  have hy : Function.Periodic yr N := by
    intro i
    change tap (r (i+N)).2.1 (r (i+N+1)).2.1 = _
    rw [show i+N+1 = (i+1)+N by omega, hr i, hr (i+1)]
  have hs := DSP.Asymptotic.square_tracking y yr (∑ i ∈ Finset.range N, |yr i|) (fun n => by
    rw [← hy.map_mod_nat n]
    exact Finset.single_le_sum (fun i _ => abs_nonneg (yr i))
      (Finset.mem_range.mpr (Nat.mod_lt n hN))) (tap_tracks _ _ h2)
  let e := fun n => (t n).2.2 - (r n).2.2
  have he : ∀ n, e n = DSP.Asymptotic.majorant (1-a) (fun i => a*(y i^2-yr i^2)) (e 0) n := by
    intro n
    induction n with
    | zero => rfl
    | succ n ih =>
      rw [DSP.Asymptotic.majorant, ← ih]
      change ((1-a)*(t n).2.2 + a*y n^2) - ((1-a)*(r n).2.2 + a*yr n^2) = _
      dsimp only [e]
      ring
  have h3 : Tendsto e atTop (𝓝 0) := by
    rw [show e = _ from funext he]
    apply DSP.Asymptotic.majorant_tendsto _ (by linarith) (by linarith)
    simpa only [mul_zero] using hs.const_mul a
  exact h1.prodMk_nhds (h2.prodMk_nhds h3)

theorem power_nonnegative (g a : ℝ) (hg : 0 < g) (ha : 0 < a) (ha1 : a ≤ 1)
    (u : ℕ → ℝ) (N : ℕ) (hN : 0 < N) (hu : Function.Periodic u N) (j : ℕ) :
    0 ≤ (orbit g a u N j).2.2 := by
  let t := Step.states (advance g a) u (0 : Memory)
  have ht (i : ℕ) : 0 ≤ (t i).2.2 := by
    induction i with
    | zero => exact le_rfl
    | succ n ih =>
      apply add_nonneg (mul_nonneg (sub_nonneg.mpr ha1) ih) (mul_nonneg ha.le (sq_nonneg _))
  have h := tracks_orbit g a hg ha ha1 u N hN hu 0
  have hr := orbit_periodic g a hg ha ha1 u N hN hu
  have hj : Tendsto (fun n : ℕ => n*N+j) atTop atTop := by
    apply tendsto_atTop_mono (g := fun n : ℕ => n*N+j) _ tendsto_id
    intro n
    have := Nat.le_mul_of_pos_right n hN
    exact this.trans (Nat.le_add_right _ _)
  have hs := (h.snd_nhds.snd_nhds.comp hj).add_const (orbit g a u N j).2.2
  have eqr (n : ℕ) : orbit g a u N (n*N+j) = orbit g a u N j := by
    simpa [Nat.add_comm] using hr.nat_mul n j
  change Tendsto (fun n => (t (n*N+j)).2.2 - (orbit g a u N (n*N+j)).2.2 +
    (orbit g a u N j).2.2) atTop (𝓝 (0+(orbit g a u N j).2.2)) at hs
  simp only [eqr, sub_add_cancel, zero_add] at hs
  exact ge_of_tendsto' hs (fun n => ht _)

end DSP.BandPassedEnvelopeFollower.Periodic
