import DSP.Filters.NaiveReuppingLadder.DetectorResponse

noncomputable section
namespace DSP.NaiveReuppingLadder.DetectorDynamics
open BandPassedEnvelopeFollower.Periodic Filter Metric
open scoped Topology NNReal

/-- The wet coordinates may be rescaled or mixed before bounding a whole period. -/
abbrev chartReturn (k : Knobs) (g a : ℝ) (u : ℕ → ℝ) (N : ℕ)
    (C : Wet ≃L[ℝ] Wet) (d : Memory) (w : Wet) :=
  C (wetReturn k g a u N d (C.symm w))

def blockRun (k : Knobs) (g a : ℝ) (u : ℕ → ℝ) (N : ℕ) (z : Joint) (n : ℕ) :=
  prefixRun k g a u z (n*N)

private theorem block_succ (k : Knobs) (g a : ℝ) (u : ℕ → ℝ) (N : ℕ)
    (hu : Function.Periodic u N) (z : Joint) (n : ℕ) :
    blockRun k g a u N z (n+1) = prefixRun k g a u (blockRun k g a u N z n) N := by
  have shift (j : ℕ) : u (n*N+j) = u j := by simpa [Nat.add_comm] using hu.nat_mul n j
  dsimp only [blockRun,prefixRun]
  rw [Nat.succ_mul, Step.states_add]
  simp_rw [shift]

/-- The dry detector is entirely determined by the input on every periodic trajectory. -/
theorem dry_of_fixed (k : Knobs) (g a : ℝ) (hg : 0 < g) (ha : 0 < a) (ha1 : a ≤ 1)
    (u : ℕ → ℝ) (N : ℕ) (hN : 0 < N) (hu : Function.Periodic u N) (z : Joint)
    (hz : prefixRun k g a u z N = z) : z.1 = orbit g a u N 0 := by
  have h := dry_block_limit k g a hg ha ha1 u N hN hu z
  have he (n : ℕ) : blockRun k g a u N z n = z := by
    induction n with
    | zero => simp [blockRun]
    | succ n ih => rw [block_succ _ _ _ _ _ hu, ih, hz]
  change Tendsto (fun n => (blockRun k g a u N z n).1) _ _ at h
  simp only [he] at h
  exact tendsto_nhds_unique tendsto_const_nhds h

/-- Uniform hypotheses for any parameter family. The Lipschitz bound concerns
nine wet directions, even for a start with fourteen independent memories.
A strict residual margin accommodates the independently decaying dry transient. -/
theorem periodic_attractor (k : Knobs) (hd : 0 ≤ k.D) (g a : ℝ)
    (hg : 0 < g) (ha : 0 < a) (ha1 : a ≤ 1)
    (u : ℕ → ℝ) (N : ℕ) (hN : 0 < N) (hu : Function.Periodic u N)
    (C : Wet ≃L[ℝ] Wet) (c : Wet) (r δ : ℝ) (K : ℝ≥0)
    (hr : 0 ≤ r) (hδ : 0 < δ) (hk : K < 1)
    (hl : ∀ d ∈ closedBall (orbit g a u N 0) δ,
      LipschitzOnWith K (chartReturn k g a u N C d) (closedBall c r))
    (hmargin : dist (chartReturn k g a u N C (orbit g a u N 0) c) c + K*r < r) :
    ∃ p : Joint, C p.2 ∈ closedBall c r ∧ prefixRun k g a u p N = p ∧
      (∀ z : Joint, C z.2 ∈ closedBall c r → prefixRun k g a u z N = z → z = p) ∧
      (∀ w : Wet, C w ∈ closedBall c r →
        Tendsto (blockRun k g a u N (p.1,w)) atTop (𝓝 p)) ∧
      (∀ z : Joint, ∃ start : ℕ, ∀ j ≥ start,
        C (blockRun k g a u N z j).2 ∈ closedBall c r →
          Tendsto (blockRun k g a u N z) atTop (𝓝 p)) ∧
      dist c (C p.2) ≤ dist c (chartReturn k g a u N C (orbit g a u N 0) c)/(1-K) := by
  let d₀ := orbit g a u N 0
  let F := chartReturn k g a u N C
  have hL := hl d₀ (mem_closedBall_self hδ.le)
  obtain ⟨v,hv,hfix,huniq,hwarm,herr⟩ :=
    AttractingBlock.exists_attracting_fixedPoint (F d₀) c r K hr hk hL hmargin.le
  let p : Joint := (d₀,C.symm v)
  have hp : prefixRun k g a u p N = p := by
    apply Prod.ext
    · rw [dry_prefix]; exact dry_fixed g a hg ha ha1 u N hN hu
    · apply C.injective
      simpa only [p,ContinuousLinearEquiv.apply_symm_apply] using hfix
  have hcont (w : Wet) : Continuous (fun d => F d w) :=
    C.continuous.comp ((Step.states_continuous _ (continuous_step k hd g a) u N).snd.comp
      (continuous_id.prodMk continuous_const))
  refine ⟨p, by simpa [p] using hv, hp, ?_, ?_, ?_, by simpa [p] using herr⟩
  · intro z hz hfixed
    have hzd := dry_of_fixed k g a hg ha ha1 u N hN hu z hfixed
    apply Prod.ext hzd
    apply C.injective
    rw [show C p.2 = v from C.apply_symm_apply v]
    apply huniq _ hz
    change C (wetReturn k g a u N d₀ (C.symm (C z.2))) = C z.2
    rw [C.symm_apply_apply, show d₀ = z.1 from hzd.symm]
    exact congrArg (fun z : Joint => C z.2) hfixed
  · intro w hw
    have he (n : ℕ) : blockRun k g a u N (p.1,w) n = (p.1, C.symm ((F d₀)^[n] (C w))) := by
      induction n with
      | zero => simp [blockRun]
      | succ n ih =>
        rw [block_succ _ _ _ _ _ hu, ih, Function.iterate_succ_apply']
        apply Prod.ext
        · rw [dry_prefix]; exact dry_fixed g a hg ha ha1 u N hN hu
        · exact (C.symm_apply_apply _).symm
    rw [show blockRun k g a u N (p.1,w) = _ from funext he]
    simpa only [p, Function.comp_def] using
      (tendsto_const_nhds (x := d₀)).prodMk_nhds ((C.symm.continuous.tendsto v).comp (hwarm _ hw))
  · intro z
    let t := blockRun k g a u N z
    have hdry : Tendsto (fun n => (t n).1) atTop (𝓝 d₀) :=
      dry_block_limit k g a hg ha ha1 u N hN hu z
    obtain ⟨start,hs⟩ := SettlingBlock.late_entry F _ d₀ hdry c v r δ K hr hδ hk hl
      (hcont c).continuousAt (hcont v).continuousAt hmargin hv hfix
    refine ⟨start, ?_⟩
    intro j hj hx
    have hrec (n : ℕ) : C (t (n+1)).2 = F (t n).1 (C (t n).2) := by
      change C (blockRun _ _ _ _ _ _ (n+1)).2 = C (wetReturn _ _ _ _ _ _ (C.symm (C _)))
      rw [C.symm_apply_apply, block_succ _ _ _ _ _ hu]
    have hwet := (hs j hj (fun n => C (t n).2) hrec hx).2
    have hback := (C.symm.continuous.tendsto v).comp hwet
    simpa only [Function.comp_def, C.symm_apply_apply] using hdry.prodMk_nhds hback

/-- The settled waveform determines both the complex input-frequency projection
and total mean-square power of the entering state, with no harmonic truncation. -/
theorem response_of_entry (k : Knobs) (hd : 0 ≤ k.D) (g a : ℝ) (u : ℕ → ℝ)
    (N : ℕ) (hN : 0 < N) (hu : Function.Periodic u N) (z p : Joint)
    (hf : prefixRun k g a u p N = p) (ht : Tendsto (blockRun k g a u N z) atTop (𝓝 p))
    (carrier : ℂ) (hc : ‖carrier‖ = 1) (hperiod : carrier^N = 1) :
    let signal := fun q n => readout k (prefixRun k g a u q n) (u n)
    ResponseMeasurement.HasCoherentResponse carrier (signal z)
      (ResponseMeasurement.coherent carrier (signal p) N) ∧
    ResponseMeasurement.HasMeanSquare (signal z) (ResponseMeasurement.meanSquare (signal p) N) := by
  let signal := fun q n => readout k (prefixRun k g a u q n) (u n)
  have shift (i : ℕ) : u (N+i) = u i := by simpa [Nat.add_comm] using hu i
  have href : Function.Periodic (prefixRun k g a u p) N := fun i => by
    rw [Nat.add_comm]; exact Step.states_periodic_of_fixed _ u N shift _ hf i
  have hp : Function.Periodic (signal p) N := fun i => by dsimp [signal]; rw [href i, hu i]
  have hit : Tendsto (fun n => signal z n-signal p n) atTop (𝓝 0) := by
    apply Step.tendsto_of_residue_classes _ _ N hN
    intro j
    have hh : Tendsto (fun n => (fun q => prefixRun k g a u q N)^[n] z) atTop (𝓝 p) := by
      change Tendsto (fun n => Step.states _ u z (n*N)) _ _ at ht
      simpa only [prefixRun, Step.states_blocks _ u N shift] using ht
    have hj := Step.states_phase_tendsto _ u N shift z p hh j
      (Step.states_continuous _ (continuous_step k hd g a) u j).continuousAt
    have hs := (((readout_cont k hd (u j)).tendsto _).comp hj).sub_const (signal p j)
    have hi (n : ℕ) : u (n*N+j) = u j := by simpa [Nat.add_comm] using hu.nat_mul n (j:ℕ)
    have hi' (n : ℕ) : signal p (n*N+j) = signal p j := by simpa [Nat.add_comm] using hp.nat_mul n (j:ℕ)
    simp only [hi']
    simpa only [signal, Function.comp_def, hi, sub_self] using hs
  exact ⟨ResponseMeasurement.coherent_of_periodic_tracking carrier hc _ _ N hN hperiod hp hit,
    ResponseMeasurement.meanSquare_of_periodic_tracking _ _ N hN hp hit⟩

end DSP.NaiveReuppingLadder.DetectorDynamics
