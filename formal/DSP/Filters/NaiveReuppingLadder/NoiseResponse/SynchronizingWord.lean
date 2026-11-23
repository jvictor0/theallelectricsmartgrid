import DSP.Filters.NaiveReuppingLadder.NoiseResponse.RunAverage

noncomputable section
open Finset Filter MeasureTheory
open scoped Topology

namespace NoiseDisplay.SynchronizingWord
open RunAverage

variable {S A : Type*} [Fintype S] [Fintype A]
variable (next : S → A → S) (w : A → ℝ)

omit [Fintype S] [Fintype A] in
private theorem fold_closed (reachable : S → Prop)
    (closed : ∀ i, reachable i → ∀ a, 0 < w a → reachable (next i a))
    (xs : List A) (supported : ∀ a ∈ xs, 0 < w a) (i : S) (hi : reachable i) :
    reachable (xs.foldl next i) := by
  induction xs generalizing i with
  | nil => exact hi
  | cons a xs ih =>
    exact ih (fun b hb => supported b (List.mem_cons_of_mem _ hb)) _
      (closed i hi a (supported a (List.mem_cons_self)))

private theorem harmonic_upper (hw : ∀ a, 0 ≤ w a) (one : ∑ a, w a = 1)
    (reachable : S → Prop) (closed : ∀ i, reachable i → ∀ a, 0 < w a → reachable (next i a))
    (reset : S) (start : reachable reset)
    (word : List A) (supported : ∀ a ∈ word, 0 < w a) (target : S)
    (merges : ∀ i, reachable i → word.foldl next i = target)
    (h : S → ℝ) (harmonic : recordStep next w h = h) :
    ∀ i, reachable i → h i ≤ h target := by
  classical
  let states := univ.filter reachable
  have mem (i : S) : i ∈ states ↔ reachable i := by simp [states]
  obtain ⟨top, ht, max⟩ := exists_max_image states h ⟨reset, (mem reset).mpr start⟩
  have advance (i : S) (reach : reachable i) (atMax : h i = h top) (a : A) (pos : 0 < w a) :
      h (next i a) = h top := by
    let gap := fun b => w b * (h top - h (next i b))
    have nonneg (b : A) : 0 ≤ gap b := by
      rcases (hw b).eq_or_lt with zero | positive
      · simp [gap, ← zero]
      · exact mul_nonneg positive.le
          (sub_nonneg.mpr (max _ ((mem _).mpr (closed i reach b positive))))
    have eq : ∑ b, w b * h (next i b) = h i := congrFun harmonic i
    have total : ∑ b, gap b = 0 := by
      simp only [gap, mul_sub, sum_sub_distrib, ← sum_mul _ _ (h top), one, one_mul, eq, atMax,
        sub_self]
    have za := (sum_eq_zero_iff_of_nonneg (fun b _ => nonneg b)).mp total a (mem_univ a)
    exact (sub_eq_zero.mp ((mul_eq_zero.mp za).resolve_left pos.ne')).symm
  have traverse (xs : List A) (xsSupported : ∀ a ∈ xs, 0 < w a)
      (i : S) (reach : reachable i) (atMax : h i = h top) :
      h (xs.foldl next i) = h top := by
    induction xs generalizing i with
    | nil => exact atMax
    | cons a xs ih =>
      have pos := xsSupported a List.mem_cons_self
      exact ih (fun b hb => xsSupported b (List.mem_cons_of_mem _ hb)) _
        (closed i reach a pos) (advance i reach atMax a pos)
  have topReach := (mem top).mp ht
  have targetTop : h target = h top := by
    rw [← merges top topReach]
    exact traverse word supported top topReach rfl
  intro i hi
  rw [targetTop]
  exact max i ((mem i).mpr hi)

/-- A supported word with a common destination forces every harmonic reward
    to be constant on the forward-closed state set. -/
theorem harmonic_constant (hw : ∀ a, 0 ≤ w a) (one : ∑ a, w a = 1)
    (reachable : S → Prop) (closed : ∀ i, reachable i → ∀ a, 0 < w a → reachable (next i a))
    (reset : S) (start : reachable reset)
    (word : List A) (supported : ∀ a ∈ word, 0 < w a) (target : S)
    (merges : ∀ i, reachable i → word.foldl next i = target)
    (h : S → ℝ) (harmonic : recordStep next w h = h) :
    ∀ i, reachable i → h i = h target := by
  have upper := harmonic_upper next w hw one reachable closed reset start word supported target
    merges h harmonic
  have lower := harmonic_upper next w hw one reachable closed reset start word supported target
    merges (-h) (by rw [map_neg, harmonic])
  exact fun i hi => le_antisymm (upper i hi) (neg_le_neg_iff.mp (lower i hi))

omit [Fintype S] in
private theorem tree_zero_off_support (hw : ∀ a, 0 ≤ w a) (n : ℕ) (f : List A → ℝ)
    (zero : ∀ xs, xs.length = n → (∀ a ∈ xs, 0 < w a) → f xs = 0) :
    tree w n f = 0 := by
  induction n generalizing f with
  | zero => exact zero [] rfl (by simp)
  | succ n ih =>
    simp only [tree]
    apply sum_eq_zero
    intro a _
    rcases (hw a).eq_or_lt with h0 | hp
    · simp [← h0]
    · rw [ih _ (fun xs hx hs => zero (a::xs) (by simp [hx]) (by simpa [hp] using hs)), mul_zero]

omit [Fintype S] in
private theorem fresh_supported {Ω : Type*} [MeasurableSpace Ω] {μ : Measure Ω}
    (input : ℕ → Ω → A) (fresh : Fresh w μ input) (hw : ∀ a, 0 ≤ w a) :
    ∀ᵐ ω ∂μ, ∀ n, ∀ a ∈ records input n ω, 0 < w a := by
  classical
  apply ae_all_iff.mpr
  intro n
  let bad : List A → ℝ := fun xs => if ∀ a ∈ xs, 0 < w a then 0 else 1
  obtain ⟨int, average⟩ := fresh n bad
  have z := tree_zero_off_support w hw n bad (by intro xs _ h; exact if_pos h)
  have nonneg (ω : Ω) : 0 ≤ bad (records input n ω) := by simp [bad, apply_ite]
  have ae := (integral_eq_zero_iff_of_nonneg nonneg int).mp (average.trans z)
  filter_upwards [ae] with ω h
  simpa [bad] using h

/-- A synchronizing word gives a deterministic limit for the original
    one-record average. Almost every record stream works from every state in
    the forward-closed set, including a state selected from that stream. -/
theorem realized_power {Ω : Type*} [MeasurableSpace Ω] {μ : Measure Ω}
    [IsProbabilityMeasure μ] (input : ℕ → Ω → A) (fresh : Fresh w μ input)
    (hw : ∀ a, 0 ≤ w a) (one : ∑ a, w a = 1) (r : S → A → ℝ)
    (reachable : S → Prop) (closed : ∀ i, reachable i → ∀ a, 0 < w a → reachable (next i a))
    (reset : S) (start : reachable reset)
    (word : List A) (supported : ∀ a ∈ word, 0 < w a) (target : S)
    (merges : ∀ i, reachable i → word.foldl next i = target) :
    ∃ m : ℝ, ∀ᵐ ω ∂μ, ∀ i, reachable i →
      Tendsto (fun n : ℕ => readout next r i (records input n ω)/n) atTop (𝓝 m) := by
  obtain ⟨h, fixed, result⟩ := RunAverage.realized_power next w input fresh hw one r
  have constant := harmonic_constant next w hw one reachable closed reset start word supported
    target merges h fixed
  refine ⟨h target, ae_all_iff.mpr fun i => ?_⟩
  obtain ⟨Z, limit, stable, _⟩ := result i
  filter_upwards [limit, stable, fresh_supported w input fresh hw] with ω lim settled hs reach
  obtain ⟨j, hj⟩ := eventually_atTop.mp settled
  have eq := constant _ (fold_closed next w reachable closed _ (hs j) i reach)
  rw [← eq, hj j le_rfl]
  exact lim

/-- The same event gives deterministic original-record limits for all bins
    at one fixed analyzer phase. The phase is carried by the state/reward
    specification, and no regrouping of the measured records is needed. -/
theorem realized_spectrum {B Ω : Type*} [Fintype B] [MeasurableSpace Ω] {μ : Measure Ω}
    [IsProbabilityMeasure μ] (input : ℕ → Ω → A) (fresh : Fresh w μ input)
    (hw : ∀ a, 0 ≤ w a) (one : ∑ a, w a = 1) (r : B → S → A → ℝ)
    (reachable : S → Prop) (closed : ∀ i, reachable i → ∀ a, 0 < w a → reachable (next i a))
    (reset : S) (start : reachable reset)
    (word : List A) (supported : ∀ a ∈ word, 0 < w a) (target : S)
    (merges : ∀ i, reachable i → word.foldl next i = target) :
    ∃ m : B → ℝ, ∀ᵐ ω ∂μ, ∀ b i, reachable i →
      Tendsto (fun n : ℕ => readout next (r b) i (records input n ω)/n) atTop (𝓝 (m b)) := by
  choose m limits using fun b => realized_power next w input fresh hw one (r b) reachable
    closed reset start word supported target merges
  exact ⟨m, ae_all_iff.mpr limits⟩

/-- Three independent positive-mass record cylinders suffice if any triple of
    supported records in those cylinders erases the incoming state. The terminal
    state may depend on all three records, including their unprescribed samples. -/
theorem realized_spectrum_of_three_cylinders
    {B Ω : Type*} [Fintype B] [MeasurableSpace Ω] {μ : Measure Ω}
    [IsProbabilityMeasure μ] (input : ℕ → Ω → A) (fresh : Fresh w μ input)
    (hw : ∀ a, 0 ≤ w a) (one : ∑ a, w a = 1) (r : B → S → A → ℝ)
    (reachable : S → Prop) (closed : ∀ i, reachable i → ∀ a, 0 < w a → reachable (next i a))
    (reset : S) (start : reachable reset)
    (C₁ C₂ C₃ : Finset A) (mass₁ : 0 < ∑ a ∈ C₁, w a)
    (mass₂ : 0 < ∑ a ∈ C₂, w a) (mass₃ : 0 < ∑ a ∈ C₃, w a)
    (erases : ∀ a ∈ C₁, ∀ b ∈ C₂, ∀ c ∈ C₃,
      0 < w a → 0 < w b → 0 < w c → ∀ i j, reachable i → reachable j →
        [a, b, c].foldl next i = [a, b, c].foldl next j) :
    ∃ m : B → ℝ, ∀ᵐ ω ∂μ, ∀ b i, reachable i →
      Tendsto (fun n : ℕ => readout next (r b) i (records input n ω)/n) atTop (𝓝 (m b)) := by
  classical
  have witness (C : Finset A) (mass : 0 < ∑ a ∈ C, w a) :
      ∃ a ∈ C, 0 < w a := by
    by_contra absent
    push Not at absent
    exact mass.not_ge (sum_nonpos absent)
  obtain ⟨a, ha, pa⟩ := witness C₁ mass₁
  obtain ⟨b, hb, pb⟩ := witness C₂ mass₂
  obtain ⟨c, hc, pc⟩ := witness C₃ mass₃
  refine realized_spectrum next w input fresh hw one r reachable closed reset start [a, b, c]
    ?_ ([a, b, c].foldl next reset) ?_
  · intro letter mem
    simp only [List.mem_cons, List.not_mem_nil, or_false] at mem
    rcases mem with rfl | rfl | rfl
    · exact pa
    · exact pb
    · exact pc
  · exact fun i hi => erases a ha b hb c hc pa pb pc i reset hi start

end NoiseDisplay.SynchronizingWord
