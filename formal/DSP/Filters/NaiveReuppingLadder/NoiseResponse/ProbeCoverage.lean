import DSP.Filters.NaiveReuppingLadder.NoiseResponse.RunAverage

noncomputable section
namespace NoiseDisplay.ProbeCoverage
open Finset MeasureTheory RunAverage

variable {A : Type*} [Fintype A]

/-- Probability that a new group of stock probes scores at most t. Each
    group may share noise internally. The held-out groups are independent. -/
def coverage (w score : A → ℝ) (t : ℝ) : ℝ :=
  ∑ a ∈ univ.filter (fun a => score a ≤ t), w a

private theorem inadequate_mass (w score : A → ℝ) (hw : ∀ a, 0 ≤ w a)
    (p : ℝ) (hp : 0 ≤ p) :
    ∑ a ∈ univ.filter (fun a => coverage w score (score a) < p), w a ≤ p := by
  classical
  let bad := univ.filter (fun a => coverage w score (score a) < p)
  change ∑ a ∈ bad, w a ≤ p
  by_cases hb : bad.Nonempty
  · obtain ⟨top, htop, greatest⟩ := bad.exists_max_image score hb
    have small : coverage w score (score top) < p := (mem_filter.mp htop).2
    apply le_trans _ small.le
    apply sum_le_sum_of_subset_of_nonneg _ (fun a _ _ => hw a)
    exact fun a ha => mem_filter.mpr ⟨mem_univ a, greatest a ha⟩
  · simpa [not_nonempty_iff_eq_empty.mp hb] using hp

private theorem tree_zero (w : A → ℝ) (n : ℕ) : tree w n (fun _ => 0) = 0 := by
  induction n with
  | zero => rfl
  | succ n ih => simp [tree,ih]

def inadequate (w score : A → ℝ) (p : ℝ) : List A → ℝ
  | [] => 1
  | a :: xs => if coverage w score (score a) < p then inadequate w score p xs else 0

theorem inadequate_iff (w score : A → ℝ) (p : ℝ) (xs : List A) :
    inadequate w score p xs = 1 ↔ ∀ a ∈ xs, coverage w score (score a) < p := by
  induction xs with
  | nil => simp [inadequate]
  | cons a xs ih =>
    by_cases ha : coverage w score (score a) < p <;> simp [inadequate, ha, ih]

private theorem all_probability (w score : A → ℝ) (p : ℝ) (n : ℕ) :
    tree w n (inadequate w score p) =
      (∑ a ∈ univ.filter (fun a => coverage w score (score a) < p), w a)^n := by
  induction n with
  | zero => simp [tree, inadequate]
  | succ n ih =>
    rw [tree, pow_succ', sum_mul, sum_filter]
    apply sum_congr rfl
    intro a _
    by_cases ha : coverage w score (score a) < p
    · simp [inadequate, ha, ih]
    · simp [inadequate, ha, tree_zero]

/-- If the largest observed error covers less than p of new groups, all
    observed scores have coverage below p. The failure probability is at most
    p^n even when the score distribution has atoms. -/
theorem tolerance {Ω : Type*} [MeasurableSpace Ω] {μ : Measure Ω} (w score : A → ℝ)
    (input : ℕ → Ω → A) (fresh : Fresh w μ input) (hw : ∀ a, 0 ≤ w a)
    (p : ℝ) (hp : 0 ≤ p) (n : ℕ) :
    (∫ ω, inadequate w score p (records input n ω) ∂μ)
      ≤ p^n := by
  classical
  rw [(fresh n (inadequate w score p)).2, all_probability]
  exact pow_le_pow_left₀ (sum_nonneg fun a _ => hw a) (inadequate_mass w score hw p hp) _

theorem stock_confidence : (9632/10000:ℚ)^80 < 1/20 := by norm_num

/-- An independent fixed-setting calibration can be performed before the
    physical error of its frozen comparison curve is certified. -/
theorem transfer_coverage {C : Type*} [PseudoMetricSpace C] (w : A → ℝ)
    (hw : ∀ a, 0 ≤ w a) (probe : A → C) (reference physical : C)
    (radius allowance : ℝ) (certified : dist reference physical ≤ allowance) :
    coverage w (fun a => dist (probe a) reference) radius ≤
      coverage w (fun a => dist (probe a) physical) (radius+allowance) := by
  apply sum_le_sum_of_subset_of_nonneg _ (fun a _ _ => hw a)
  intro a ha
  exact mem_filter.mpr ⟨mem_univ a, (dist_triangle _ reference _).trans
    (add_le_add (mem_filter.mp ha).2 certified)⟩

theorem fixed_probe_confidence : (95/100:ℚ)^72 < 1/40 := by norm_num

/-- A successful word erases the designated memories for every incoming
    state. Independent words then give a geometric bound even when the two
    histories are chosen before seeing the words. -/
theorem no_erasing_word (w : A → ℝ) (good : A → Bool) (n : ℕ) :
    tree w n (fun xs => if xs.any good then (0:ℝ) else 1) =
      (∑ a ∈ univ.filter (fun a => good a = false), w a)^n := by
  induction n with
  | zero => simp [tree]
  | succ n ih =>
    rw [tree,pow_succ',sum_mul,sum_filter]
    apply sum_congr rfl
    intro a _
    cases h : good a
    · simp only [List.any_cons, h, Bool.false_or, if_true, ih]
    · simp [h,tree_zero]

/-- Exact failure budgets for the 16 independent complete-state erasure
    checks and the 1024 independent finite-block overflow checks. -/
theorem block_audit_confidence :
    (717/1000:ℚ)^16 < 1/200 ∧ (994/1000:ℚ)^1024 < 1/400 := by
  constructor
  · norm_num
  · calc
      _ = ((994/1000:ℚ)^64)^16 := by rw [← pow_mul]
      _ ≤ (682/1000:ℚ)^16 := pow_le_pow_left₀ (by positivity) (by norm_num) _
      _ < 1/400 := by norm_num

/-- A holdout test of expected state-energy drift. A null hypothesis E[X] ≤ m
    gives E[15/16 + X/(16m)] ≤ 1, even when a few paths amplify enormously. -/
theorem wager_mean (w x : A → ℝ) (one : ∑ a, w a = 1)
    {m : ℝ} (hm : 0 < m) (null : ∑ a, w a * x a ≤ m) :
    ∑ a, w a * (15/16 + x a/(16*m)) ≤ 1 := by
  simp_rw [mul_add, ← mul_div_assoc]
  rw [sum_add_distrib, ← sum_div, ← sum_mul, one, ← sum_div]
  have bound : (∑ a, w a * x a) / (16*m) ≤ 1/16 :=
    (div_le_iff₀ (by positivity)).mpr (by linarith)
  linarith

/-- Product test under iid records. The event is counted directly on the
    probability tree, so a zero gap and arbitrarily large gains are allowed. -/
theorem product_test (w factor : A → ℝ) (hw : ∀ a, 0 ≤ w a) (hf : ∀ a, 0 ≤ factor a)
    (unit : ∑ a, w a * factor a ≤ 1) (n : ℕ) {threshold : ℝ} (pos : 0 < threshold) :
    tree w n (fun xs => if threshold ≤ (xs.map factor).prod then (1:ℝ) else 0)
      ≤ 1/threshold := by
  have go (k : ℕ) (c : ℝ) (hc : 0 ≤ c) :
      tree w k (fun xs => if threshold ≤ c * (xs.map factor).prod then (1:ℝ) else 0)
        ≤ c/threshold := by
    induction k generalizing c with
    | zero =>
      simp only [tree,List.map_nil,List.prod_nil,mul_one]
      split_ifs with h
      · exact (one_le_div pos).mpr h
      · exact div_nonneg hc pos.le
    | succ k ih =>
      simp only [tree,List.map_cons,List.prod_cons,← mul_assoc]
      calc
        _ ≤ ∑ a, w a * ((c * factor a)/threshold) := sum_le_sum fun a _ =>
          mul_le_mul_of_nonneg_left (ih _ (mul_nonneg hc (hf a))) (hw a)
        _ = (c/threshold) * ∑ a, w a * factor a := by
          rw [mul_sum]; exact sum_congr rfl fun _ _ => by ring
        _ ≤ c/threshold := by simpa using mul_le_mul_of_nonneg_left unit (div_nonneg hc pos.le)
  simpa using go n 1 zero_le_one

end NoiseDisplay.ProbeCoverage
