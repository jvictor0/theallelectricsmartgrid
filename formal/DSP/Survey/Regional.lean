import Mathlib.MeasureTheory.Measure.MeasureSpace
import Mathlib.Algebra.Order.BigOperators.Group.Finset
import Mathlib.Algebra.Order.Field.Basic

open MeasureTheory Finset
open scoped BigOperators

namespace DSP.Survey

/-- The event that at least one accepted fixed claim overstates its coverage. -/
def falseClaim {Ω C T Q : Type*} (truth : C → T → ℝ) (level : Q → ℝ)
    (pass : Ω → C → T → Q → Prop) : Set Ω :=
  {x | ∃ c t q, pass x c t q ∧ truth c t < level q}

/-- A union bound over the complete family of fixed claims. Dependence among the
tests is allowed. -/
theorem measure_falseClaim_le_sum {Ω C T Q : Type*}
    [MeasurableSpace Ω] [Fintype C] [Fintype T] [Fintype Q]
    (μ : Measure Ω) (truth : C → T → ℝ) (level : Q → ℝ)
    (pass : Ω → C → T → Q → Prop) (risk : C × T × Q → ENNReal)
    (localRisk : ∀ c t q, μ {x | pass x c t q ∧ truth c t < level q} ≤ risk (c, t, q)) :
    μ (falseClaim truth level pass) ≤ ∑ i, risk i := by
  let bad (i : C × T × Q) : Set Ω :=
    {x | pass x i.1 i.2.1 i.2.2 ∧ truth i.1 i.2.1 < level i.2.2}
  have union_eq : falseClaim truth level pass = ⋃ i, bad i := by
    ext x
    rw [Set.mem_iUnion]
    exact ⟨fun ⟨c, t, q, h⟩ => ⟨(c, t, q), h⟩,
      fun ⟨⟨c, t, q⟩, h⟩ => ⟨c, t, q, h⟩⟩
  rw [union_eq]
  calc
    μ (⋃ i, bad i) ≤ ∑' i, μ (bad i) := measure_iUnion_le bad
    _ = ∑ i, μ (bad i) := tsum_fintype _
    _ ≤ ∑ i, risk i := sum_le_sum fun i _ => localRisk i.1 i.2.1 i.2.2

/-- An omitted claim contributes the unconditional lower endpoint zero. -/
def lowerEndpoint {Q : Type*} (level : Q → ℝ) (q : Option Q) : ℝ :=
  q.elim 0 level

/-- Region mass, weighted by the cell masses. -/
def regionMass {C : Type*} (w : C → ℝ) (r : Finset C) : ℝ :=
  ∑ c ∈ r, w c

/-- Weighted sum of a cell quantity over a region. -/
def regionTotal {C : Type*} (w : C → ℝ) (r : Finset C) (v : C → ℝ) : ℝ :=
  ∑ c ∈ r, w c * v c

/-- Every region and endpoint selection is valid off the false-claim event,
including selections made after seeing the outcome. Positive region mass also
gives the bound on conditional coverage. -/
theorem regionalCoverage_of_not_falseClaim {Ω C T Q : Type*}
    (truth : C → T → ℝ) (level : Q → ℝ) (pass : Ω → C → T → Q → Prop)
    (truth_nonneg : ∀ c t, 0 ≤ truth c t) {x : Ω}
    (good : x ∉ falseClaim truth level pass)
    (w : C → ℝ) (w_nonneg : ∀ c, 0 ≤ w c)
    (r : Finset C) (t : T) (q : C → Option Q)
    (accepted : ∀ c ∈ r, ∀ a, q c = some a → pass x c t a) :
    regionTotal w r (fun c => lowerEndpoint level (q c)) ≤
        regionTotal w r (fun c => truth c t) ∧
    (0 < regionMass w r →
      regionTotal w r (fun c => lowerEndpoint level (q c)) / regionMass w r ≤
        regionTotal w r (fun c => truth c t) / regionMass w r) := by
  have bound : regionTotal w r (fun c => lowerEndpoint level (q c)) ≤
      regionTotal w r (fun c => truth c t) := by
    apply sum_le_sum
    intro c hc
    apply mul_le_mul_of_nonneg_left _ (w_nonneg c)
    change lowerEndpoint level (q c) ≤ truth c t
    cases hq : q c with
    | none => simpa [lowerEndpoint] using truth_nonneg c t
    | some a =>
      change level a ≤ truth c t
      exact le_of_not_gt fun overstates => good ⟨c, t, a, accepted c hc a hq, overstates⟩
  exact ⟨bound, fun hmass => div_le_div_of_nonneg_right bound hmass.le⟩

/-- The event that some accepted choice of region, threshold and endpoints
overstates the weighted total or conditional coverage. -/
def regionalFailure {Ω C T Q : Type*}
    (truth : C → T → ℝ) (level : Q → ℝ) (pass : Ω → C → T → Q → Prop)
    (w : C → ℝ) : Set Ω :=
  {x | ∃ (r : Finset C) (t : T) (q : C → Option Q),
    (∀ c ∈ r, ∀ a, q c = some a → pass x c t a) ∧
    (regionTotal w r (fun c => truth c t) <
      regionTotal w r (fun c => lowerEndpoint level (q c)) ∨
      (0 < regionMass w r ∧
        regionTotal w r (fun c => truth c t) / regionMass w r <
          regionTotal w r (fun c => lowerEndpoint level (q c)) / regionMass w r))}

/-- Any regional failure entails a falsely accepted fixed claim. -/
theorem regionalFailure_subset_falseClaim {Ω C T Q : Type*}
    (truth : C → T → ℝ) (level : Q → ℝ) (pass : Ω → C → T → Q → Prop)
    (truth_nonneg : ∀ c t, 0 ≤ truth c t)
    (w : C → ℝ) (w_nonneg : ∀ c, 0 ≤ w c) :
    regionalFailure truth level pass w ⊆ falseClaim truth level pass := by
  intro x hx
  by_contra good
  obtain ⟨r, t, q, accepted, wrong⟩ := hx
  obtain ⟨total, conditional⟩ :=
    regionalCoverage_of_not_falseClaim truth level pass truth_nonneg good w w_nonneg r t q accepted
  rcases wrong with wrong | ⟨mass_pos, wrong⟩
  · exact (not_lt_of_ge total) wrong
  · exact (not_lt_of_ge (conditional mass_pos)) wrong

/-- Simultaneous weighted and conditional regional bounds with a sum of fixed
claim risks as the failure budget. -/
theorem measure_regionalFailure_le_sum {Ω C T Q : Type*}
    [MeasurableSpace Ω] [Fintype C] [Fintype T] [Fintype Q]
    (μ : Measure Ω) (truth : C → T → ℝ) (level : Q → ℝ)
    (pass : Ω → C → T → Q → Prop) (truth_nonneg : ∀ c t, 0 ≤ truth c t)
    (risk : C × T × Q → ENNReal)
    (localRisk : ∀ c t q, μ {x | pass x c t q ∧ truth c t < level q} ≤ risk (c, t, q))
    (w : C → ℝ) (w_nonneg : ∀ c, 0 ≤ w c) :
    μ (regionalFailure truth level pass w) ≤ ∑ i, risk i :=
  (measure_mono (regionalFailure_subset_falseClaim truth level pass truth_nonneg w w_nonneg)).trans
    (measure_falseClaim_le_sum μ truth level pass risk localRisk)

end DSP.Survey
