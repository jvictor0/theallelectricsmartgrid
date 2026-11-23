import DSP.Filters.NaiveReuppingLadder.NoiseResponse.RunAverage

noncomputable section
namespace NoiseDisplay.FiniteLookback

open Finset Filter MarkovPower RunAverage
open scoped Topology

variable {S A : Type*} [Fintype A]
variable (next : S → A → S) (w : A → ℝ)

/-- The word-tree expectation preserves pathwise bounds at the given length. -/
theorem tree_mono (hw : ∀ a, 0 ≤ w a) (k : ℕ) {f g : List A → ℝ}
    (h : ∀ xs, xs.length = k → f xs ≤ g xs) : tree w k f ≤ tree w k g := by
  induction k generalizing f g with
  | zero => exact h [] rfl
  | succ k ih =>
    apply sum_le_sum
    intro a _
    exact mul_le_mul_of_nonneg_left (ih fun xs hx => h (a :: xs) (by simp [hx])) (hw a)

/-- Fresh words act on the expected readout by iterating the record transition. -/
theorem tree_eq_atTime (r : S → ℝ) (k : ℕ) (i : S) :
    tree w k (fun xs => r (xs.foldl next i)) = atTime (recordStep next w) k r i := by
  induction k generalizing i with
  | zero => rfl
  | succ k ih =>
    change (∑ a, w a * tree w k (fun xs => r (xs.foldl next (next i a)))) =
      ∑ a, w a * atTime (recordStep next w) k r (next i a)
    simp only [ih]

private theorem record_bounded (hw : ∀ a, 0 ≤ w a) (one : ∑ a, w a = 1) :
    BoundedStep (recordStep next w) := by
  intro f b hf i
  change |∑ a, w a * f (next i a)| ≤ b
  calc
    _ ≤ ∑ a, |w a * f (next i a)| := abs_sum_le_sum_abs _ _
    _ ≤ ∑ a, w a * b := sum_le_sum fun a _ => by
      rw [abs_mul, abs_of_nonneg (hw a)]
      exact mul_le_mul_of_nonneg_left (hf _) (hw a)
    _ = b := by rw [← sum_mul, one, one_mul]

private theorem mean_tendsto [Fintype S] {P : (S → ℝ) →ₗ[ℝ] (S → ℝ)}
    (bounded : BoundedStep P) {r h v : S → ℝ} (fixed : P h = h)
    (decomp : r = h + (v - P v)) (i : S) :
    Tendsto (fun n : ℕ => (∑ j ∈ range n, atTime P j r i) / (n : ℝ)) atTop (𝓝 (h i)) := by
  apply Metric.tendsto_atTop.mpr
  intro ε positive
  let b := ∑ j, |v j|
  obtain ⟨m, hm⟩ := exists_nat_gt (2 * b / ε)
  refine ⟨m + 1, fun n hn => ?_⟩
  have np : 0 < n := by omega
  have large : 2 * b / ε < (n : ℝ) := hm.trans_le (by exact_mod_cast (show m ≤ n by omega))
  rw [Real.dist_eq]
  exact (mean_error P bounded fixed decomp np i
    (fun j => single_le_sum (fun s _ => abs_nonneg (v s)) (mem_univ j))).trans_lt
    ((div_lt_iff₀ (by exact_mod_cast np : (0 : ℝ) < n)).mpr
      (by linarith [(div_lt_iff₀ positive).mp large]))

/-- Uniform bounds on the expected readout after k fresh records bound the
    limiting expected Cesàro mean from a reset in the same forward-closed set. -/
theorem cesaro_expected_sandwich [Fintype S] (hw : ∀ a, 0 ≤ w a) (one : ∑ a, w a = 1)
    (r : S → ℝ) (k : ℕ) (lower upper : ℝ) (reachable : S → Prop)
    (closed : ∀ i, reachable i → ∀ a, 0 < w a → reachable (next i a))
    (reset : S) (start : reachable reset)
    (bounds : ∀ i, reachable i →
      lower ≤ atTime (recordStep next w) k r i ∧ atTime (recordStep next w) k r i ≤ upper) :
    ∃ m, Tendsto
      (fun n : ℕ => (∑ j ∈ range n, atTime (recordStep next w) j r reset) / (n : ℝ))
      atTop (𝓝 m) ∧ lower ≤ m ∧ m ≤ upper := by
  let P := recordStep next w
  have bounded := record_bounded next w hw one
  obtain ⟨h, v, fixed, decomp⟩ := poisson_solution P bounded r
  have shifted (j : ℕ) : atTime P j r = h + (atTime P j v - P (atTime P j v)) := by
    induction j with
    | zero => exact decomp
    | succ k ih => simpa only [atTime, map_add, map_sub, fixed] using congrArg P ih
  have propagate (j : ℕ) (i : S) (reach : reachable i) :
      lower ≤ atTime P j (atTime P k r) i ∧
      atTime P j (atTime P k r) i ≤ upper := by
    induction j generalizing i with
    | zero => exact bounds i reach
    | succ j ih =>
      change lower ≤ (∑ a, w a * atTime P j (atTime P k r) (next i a)) ∧
        (∑ a, w a * atTime P j (atTime P k r) (next i a)) ≤ upper
      have step (a : A) :
          w a * lower ≤ w a * atTime P j (atTime P k r) (next i a) ∧
          w a * atTime P j (atTime P k r) (next i a) ≤ w a * upper := by
        rcases (hw a).eq_or_lt with hz | hp
        · simp [← hz]
        · exact ⟨mul_le_mul_of_nonneg_left (ih _ (closed i reach a hp)).1 hp.le,
            mul_le_mul_of_nonneg_left (ih _ (closed i reach a hp)).2 hp.le⟩
      constructor
      · calc
          _ = ∑ a, w a * lower := by rw [← sum_mul, one, one_mul]
          _ ≤ _ := sum_le_sum fun a _ => (step a).1
      · calc
          _ ≤ ∑ a, w a * upper := sum_le_sum fun a _ => (step a).2
          _ = _ := by rw [← sum_mul, one, one_mul]
  have avg {n : ℕ} (hn : 0 < n) :
      lower ≤ (∑ j ∈ range n, atTime P j (atTime P k r) reset) / (n : ℝ) ∧
      (∑ j ∈ range n, atTime P j (atTime P k r) reset) / (n : ℝ) ≤ upper := by
    have np : (0 : ℝ) < n := by exact_mod_cast hn
    constructor
    · apply (le_div_iff₀ np).mpr
      calc
        _ = ∑ _j ∈ range n, lower := by simp [mul_comm]
        _ ≤ _ := sum_le_sum fun j _ => (propagate j reset start).1
    · apply (div_le_iff₀ np).mpr
      calc
        _ ≤ ∑ _j ∈ range n, upper := sum_le_sum fun j _ => (propagate j reset start).2
        _ = _ := by simp [mul_comm]
  have t := mean_tendsto bounded fixed (shifted k) reset
  refine ⟨h reset, mean_tendsto bounded fixed decomp reset, ge_of_tendsto t ?_, le_of_tendsto t ?_⟩
  · exact (eventually_gt_atTop 0).mono fun n hn => (avg hn).1
  · exact (eventually_gt_atTop 0).mono fun n hn => (avg hn).2

/-- Average the pathwise envelope over the same fresh records used to advance the state.
    The pathwise bounds only need to cover the forward-closed set containing the reset. -/
theorem cesaro_sandwich [Fintype S] (hw : ∀ a, 0 ≤ w a) (one : ∑ a, w a = 1)
    (r : S → ℝ) (k : ℕ) (lo hi : List A → ℝ) (reachable : S → Prop)
    (closed : ∀ i, reachable i → ∀ a, 0 < w a → reachable (next i a))
    (reset : S) (start : reachable reset)
    (bounds : ∀ i, reachable i → ∀ xs, xs.length = k →
      lo xs ≤ r (xs.foldl next i) ∧ r (xs.foldl next i) ≤ hi xs) :
    ∃ m, Tendsto
      (fun n : ℕ => (∑ j ∈ range n, atTime (recordStep next w) j r reset) / (n : ℝ))
      atTop (𝓝 m) ∧ tree w k lo ≤ m ∧ m ≤ tree w k hi := by
  apply cesaro_expected_sandwich next w hw one r k _ _ reachable closed reset start
  intro i reach
  rw [← tree_eq_atTime]
  exact ⟨tree_mono w hw k (fun xs hx => (bounds i reach xs hx).1),
    tree_mono w hw k (fun xs hx => (bounds i reach xs hx).2)⟩

private theorem tree_add (k : ℕ) (f g : List A → ℝ) :
    tree w k (fun xs => f xs + g xs) = tree w k f + tree w k g := by
  induction k generalizing f g with
  | zero => rfl
  | succ k ih => simp only [tree, ih, mul_add, sum_add_distrib]

/-- A single signed correction per word list suffices if every incoming state's
    readout is between the reference and its corrected value. The positive and
    negative parts give separate upper and lower expected biases. -/
theorem cesaro_signed_envelope [Fintype S] (hw : ∀ a, 0 ≤ w a) (one : ∑ a, w a = 1)
    (r : S → ℝ) (k : ℕ) (ref correction : List A → ℝ) (reachable : S → Prop)
    (closed : ∀ i, reachable i → ∀ a, 0 < w a → reachable (next i a))
    (reset : S) (start : reachable reset)
    (bounds : ∀ i, reachable i → ∀ xs, xs.length = k →
      min 0 (correction xs) ≤ r (xs.foldl next i) - ref xs ∧
      r (xs.foldl next i) - ref xs ≤ max 0 (correction xs)) :
    ∃ m, Tendsto
      (fun n : ℕ => (∑ j ∈ range n, atTime (recordStep next w) j r reset) / (n : ℝ))
      atTop (𝓝 m) ∧
      tree w k (fun xs => min 0 (correction xs)) ≤ m - tree w k ref ∧
      m - tree w k ref ≤ tree w k (fun xs => max 0 (correction xs)) := by
  obtain ⟨m, lim, lower, upper⟩ :=
    cesaro_sandwich next w hw one r k (fun xs => ref xs + min 0 (correction xs))
      (fun xs => ref xs + max 0 (correction xs)) reachable closed reset start (by
        intro i reach xs hx
        obtain ⟨l, u⟩ := bounds i reach xs hx
        exact ⟨by linarith, by linarith⟩)
  rw [tree_add] at lower upper
  exact ⟨m, lim, by linarith, by linarith⟩

end NoiseDisplay.FiniteLookback
