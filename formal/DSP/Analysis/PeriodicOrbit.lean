import DSP.Step
import DSP.Analysis.AttractingBlock
import Mathlib.Data.Nat.Periodic

/-! Relate a repeated sample block to the original time-varying sample run.
Convergence of the block map plus continuity of the finite prefixes gives
convergence at EVERY sample phase, not just once per period. -/

namespace DSP.Step

variable {S I : Type}

theorem states_add (p : Step S I) (u : ℕ → I) (x : S) (n m : ℕ) :
    states p u x (n+m) = states p (fun k => u (n+k)) (states p u x n) m := by
  induction m with
  | zero => simp
  | succ m ih => simp only [Nat.add_succ, states_succ, ih]

theorem states_period_shift (p : Step S I) (u : ℕ → I) (N : ℕ)
    (hu : ∀ n, u (N+n) = u n) (x : S) (n : ℕ) :
    states p u x (N+n) = states p u (states p u x N) n := by
  rw [states_add]
  simp_rw [hu]

/-- The actual sample run at block boundaries is iteration of the block map. -/
theorem states_blocks (p : Step S I) (u : ℕ → I) (N : ℕ)
    (hu : ∀ n, u (N+n) = u n) (x : S) (n : ℕ) :
    states p u x (n*N) = (fun y => states p u y N)^[n] x := by
  induction n generalizing x with
  | zero => simp
  | succ n ih =>
    rw [Nat.succ_mul, Nat.add_comm, states_period_shift p u N hu, ih]
    rfl

/-- A fixed point of the block map gives an exact periodic sample trajectory. -/
theorem states_periodic_of_fixed (p : Step S I) (u : ℕ → I) (N : ℕ)
    (hu : ∀ n, u (N+n) = u n) (x : S) (hx : states p u x N = x) (n : ℕ) :
    states p u x (N+n) = states p u x n := by
  rw [states_period_shift p u N hu, hx]

/-- All fixed sample phases converge if the block iterates converge and each
finite prefix is continuous. This makes no claim about convergence from points
outside the certified local basin. -/
theorem states_phase_tendsto [TopologicalSpace S] (p : Step S I) (u : ℕ → I) (N : ℕ)
    (hu : ∀ n, u (N+n) = u n) (x limit : S)
    (h : Filter.Tendsto (fun n => (fun y => states p u y N)^[n] x)
      Filter.atTop (nhds limit)) (j : ℕ)
    (hc : ContinuousAt (fun y => states p u y j) limit) :
    Filter.Tendsto (fun n => states p u x (n*N+j)) Filter.atTop (nhds (states p u limit j)) := by
  have hp (n k : ℕ) : u (n*N+k) = u k := by
    induction n with
    | zero => simp
    | succ n ih => rw [Nat.succ_mul]; simpa only [Nat.add_assoc, Nat.add_left_comm,
        Nat.add_comm, hu] using ih
  have he (n : ℕ) : states p u x (n*N+j) =
      states p u ((fun y => states p u y N)^[n] x) j := by
    rw [states_add, states_blocks p u N hu]
    simp_rw [hp]
  simp_rw [he]
  exact hc.tendsto.comp h

theorem states_continuous [TopologicalSpace S] (p : Step S I)
    (hp : ∀ input, Continuous (fun x => p x input)) (u : ℕ → I) (n : ℕ) :
    Continuous (fun x => states p u x n) := by
  induction n with
  | zero => exact continuous_id
  | succ n ih => exact (hp (u n)).comp ih

open scoped NNReal Topology in
/-- Combined local existence and sample-by-sample attraction criterion for a
periodically driven processor. Uniform contraction is required for its entire
block map; individual sample steps may expand. -/
theorem exists_attracting_periodicOrbit [MetricSpace S] [CompleteSpace S]
    (p : Step S I) (hp : ∀ input, Continuous (fun x => p x input))
    (u : ℕ → I) (N : ℕ) (hu : ∀ n, u (N+n) = u n)
    (center : S) (radius : ℝ) (K : ℝ≥0) (hr : 0 ≤ radius) (hk : K < 1)
    (hl : LipschitzOnWith K (fun x => states p u x N) (Metric.closedBall center radius))
    (he : dist (states p u center N) center + K*radius ≤ radius) :
    ∃ orbit ∈ Metric.closedBall center radius,
      states p u orbit N = orbit ∧
      (∀ n, states p u orbit (N+n) = states p u orbit n) ∧
      (∀ x ∈ Metric.closedBall center radius, ∀ j,
        Filter.Tendsto (fun n => states p u x (n*N+j)) Filter.atTop (𝓝 (states p u orbit j))) ∧
      dist center orbit ≤ dist center (states p u center N)/(1-K) := by
  obtain ⟨orbit, hmem, hfix, _, hattract, herror⟩ :=
    AttractingBlock.exists_attracting_fixedPoint (fun x => states p u x N) center radius K hr hk hl he
  refine ⟨orbit, hmem, hfix, states_periodic_of_fixed p u N hu orbit hfix, ?_, herror⟩
  intro x hx j
  exact states_phase_tendsto p u N hu x orbit (hattract x hx) j
    (states_continuous p hp u j).continuousAt

/-- Convergence along all finitely many residue classes gives convergence of
the entire sequence. There is no restriction to complete measurement windows. -/
theorem tendsto_of_residue_classes {E : Type*} [MetricSpace E] (f : ℕ → E)
    (limit : E) (N : ℕ) (hN : 0 < N)
    (h : ∀ j : Fin N, Filter.Tendsto (fun n => f (n*N+j)) Filter.atTop (nhds limit)) :
    Filter.Tendsto f Filter.atTop (nhds limit) := by
  apply Metric.tendsto_atTop.mpr
  intro ε hε
  have he (j : Fin N) := Metric.tendsto_atTop.mp (h j) ε hε
  choose threshold hthreshold using he
  let bound := Finset.univ.sup threshold
  refine ⟨bound*N, ?_⟩
  intro n hn
  let j : Fin N := ⟨n%N, Nat.mod_lt n hN⟩
  have hj : threshold j ≤ bound := Finset.le_sup (f := threshold) (Finset.mem_univ j)
  have hdiv : bound ≤ n/N := (Nat.le_div_iff_mul_le hN).mpr hn
  have hh := hthreshold j (n/N) (hj.trans hdiv)
  simpa only [j, Nat.div_add_mod'] using hh

/-- Continuous observations of the attracted trajectory track the exact
periodic observation at ALL sample times. This is the hypothesis needed by
coherent_of_periodic_tracking and meanSquare_of_periodic_tracking. -/
theorem states_observer_tracking [MetricSpace S] (p : Step S I)
    (hp : ∀ input, Continuous (fun x => p x input)) (u : ℕ → I)
    (N : ℕ) (hN : 0 < N) (hu : ∀ n, u (N+n) = u n) (x orbit : S)
    (hfix : states p u orbit N = orbit)
    (h : Filter.Tendsto (fun n => (fun y => states p u y N)^[n] x)
      Filter.atTop (nhds orbit)) (observe : S → ℝ) (ho : Continuous observe) :
    Filter.Tendsto (fun n => observe (states p u x n)-observe (states p u orbit n))
      Filter.atTop (nhds 0) := by
  apply tendsto_of_residue_classes _ 0 N hN
  intro j
  have hperiod : Function.Periodic (fun n => states p u orbit n) N := by
    intro n
    simpa only [Nat.add_comm] using states_periodic_of_fixed p u N hu orbit hfix n
  have he (n : ℕ) : states p u orbit (n*N+j) = states p u orbit j := by
    simpa [Nat.add_comm] using hperiod.nat_mul n (j:ℕ)
  simp_rw [he]
  have hs := states_phase_tendsto p u N hu x orbit h j
    (states_continuous p hp u j).continuousAt
  simpa only [sub_self, Function.comp_def] using ((ho.tendsto _).comp hs).sub_const (observe (states p u orbit j))

end DSP.Step
