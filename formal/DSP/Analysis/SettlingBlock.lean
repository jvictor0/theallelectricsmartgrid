import DSP.Analysis.AttractingBlock
import DSP.Analysis.Asymptotic

/-! A periodically driven subsystem with an independently settling input.
Only differences in the coupled state need a contraction estimate. -/

noncomputable section
namespace DSP.SettlingBlock
open Filter Metric
open scoped Topology NNReal

variable {E D : Type*} [MetricSpace E] [MetricSpace D]

/-- A single finite entry after the independent input has settled sufficiently
is enough. The required entry time depends on that input history, never on
the trajectory of the coupled state. -/
theorem late_entry (F : D → E → E) (d : ℕ → D) (steady : D)
    (hd : Tendsto d atTop (𝓝 steady)) (center orbit : E) (r δ : ℝ) (K : ℝ≥0)
    (hr : 0 ≤ r) (hδ : 0 < δ) (hk : K < 1)
    (hl : ∀ z ∈ closedBall steady δ, LipschitzOnWith K (F z) (closedBall center r))
    (hc : ContinuousAt (fun z => F z center) steady)
    (ho : ContinuousAt (fun z => F z orbit) steady)
    (hmargin : dist (F steady center) center + K*r < r)
    (hin : orbit ∈ closedBall center r) (hf : F steady orbit = orbit) :
    ∃ start : ℕ, ∀ j ≥ start, ∀ x : ℕ → E, (∀ n, x (n+1) = F (d n) (x n)) →
      x j ∈ closedBall center r →
      (∀ n ≥ j, x n ∈ closedBall center r) ∧ Tendsto x atTop (𝓝 orbit) := by
  have hz := Metric.tendsto_atTop.mp hd δ hδ
  obtain ⟨n₁,hn₁⟩ := hz
  have hh : Tendsto (fun n => dist (F (d n) center) center + K*r) atTop
      (𝓝 (dist (F steady center) center + K*r)) :=
    ((hc.tendsto.comp hd).dist tendsto_const_nhds).add_const _
  obtain ⟨n₂,hn₂⟩ := eventually_atTop.mp (hh.eventually_lt_const hmargin)
  refine ⟨max n₁ n₂, ?_⟩
  intro j hj x hx hentry
  have hdry (n : ℕ) (hn : j ≤ n) : d n ∈ closedBall steady δ :=
    (hn₁ n ((le_max_left ..).trans (hj.trans hn))).le
  have hstay (n : ℕ) : x (j+n) ∈ closedBall center r := by
    induction n with
    | zero => simpa using hentry
    | succ n ih =>
      rw [Nat.add_succ, hx]
      apply AttractingBlock.mapsTo_ball _ center r K hr (hl _ (hdry _ (Nat.le_add_right _ _))) _ ih
      exact (hn₂ _ ((le_max_right ..).trans (hj.trans (Nat.le_add_right _ _)))).le
  refine ⟨fun n hn => by simpa [Nat.add_sub_of_le hn] using hstay (n-j), ?_⟩
  let err := fun n => dist (F (d (j+n)) orbit) orbit
  have he : Tendsto err atTop (𝓝 0) := by
    have h := ((ho.tendsto.comp hd).comp (tendsto_add_atTop_nat j)).dist
      (tendsto_const_nhds (x := orbit))
    simpa only [err, Function.comp_def, Nat.add_comm j, hf, dist_self] using h
  let bound := DSP.Asymptotic.majorant (K:ℝ) err (dist (x j) orbit)
  have hb (n : ℕ) : dist (x (j+n)) orbit ≤ bound n := by
    induction n with
    | zero => simp [bound, DSP.Asymptotic.majorant]
    | succ n ih =>
      rw [Nat.add_succ, hx]
      calc
        _ ≤ dist (F (d (j+n)) (x (j+n))) (F (d (j+n)) orbit) + err n := dist_triangle _ _ _
        _ ≤ K * dist (x (j+n)) orbit + err n :=
          add_le_add ((hl _ (hdry _ (Nat.le_add_right j n))).dist_le_mul _ (hstay n) _ hin) le_rfl
        _ ≤ bound (n+1) := add_le_add (mul_le_mul_of_nonneg_left ih K.coe_nonneg) le_rfl
  apply (tendsto_add_atTop_iff_nat j).mp
  apply tendsto_iff_dist_tendsto_zero.mpr
  apply squeeze_zero (fun _ => dist_nonneg) (fun n => by simpa [Nat.add_comm j] using hb n)
  exact DSP.Asymptotic.majorant_tendsto _ K.coe_nonneg hk _ _ he

/-- A common contraction controls branch motion between any two knob settings
whose fixed points lie in the same ball. -/
theorem parameter_displacement (F G : E → E) (c x y : E) (r ε : ℝ) (K : ℝ≥0)
    (hk : K < 1) (hl : LipschitzOnWith K F (closedBall c r))
    (hx : x ∈ closedBall c r) (hy : y ∈ closedBall c r)
    (hfx : F x = x) (hgy : G y = y) (he : dist (F y) (G y) ≤ ε) :
    dist x y ≤ ε/(1-K) := by
  have h := dist_triangle (F x) (F y) (G y)
  have hh := hl.dist_le_mul _ hx _ hy
  rw [hfx, hgy] at h
  rw [hfx] at hh
  rw [hgy] at he
  apply (le_div_iff₀ (by exact sub_pos.mpr (show (K:ℝ) < 1 from hk))).mpr
  nlinarith

end DSP.SettlingBlock
