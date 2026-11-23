import DSP.Filters.NaiveReuppingLadder.NoiseResponse.RunAverage

open scoped BigOperators
open Finset MeasureTheory

namespace NoiseDisplay.LagMarginal

open RunAverage

variable {S A Ω : Type*}

/-- The record power after exactly `n` records from the reset state. -/
def recordPower (next : S → A → S) (r : S → A → ℝ) (i : Ω → S)
    (input : ℕ → Ω → A) (n : ℕ) (ω : Ω) : ℝ :=
  r ((records input n ω).foldl next (i ω)) (input n ω)

/-- Older and younger states consume the same records from index one onward.
Only the older state has consumed record zero. -/
def observedDifference (next : S → A → S) (r : S → A → ℝ) (i : Ω → S)
    (input : ℕ → Ω → A) (n : ℕ) (ω : Ω) : ℝ :=
  let tail := records (fun k => input (k + 1)) n ω
  let a := input (n + 1) ω
  r (tail.foldl next (next (i ω) (input 0 ω))) a - r (tail.foldl next (i ω)) a

private theorem observedDifference_eq (next : S → A → S) (r : S → A → ℝ) (i : Ω → S)
    (input : ℕ → Ω → A) (n : ℕ) (ω : Ω) :
    observedDifference next r i input n ω =
      recordPower next r i input (n + 1) ω -
        recordPower next r i (fun k => input (k + 1)) n ω := by
  simp [observedDifference, recordPower, records, List.range_succ_eq_map,
    List.map_map, Function.comp_def, Nat.succ_eq_add_one]

/-- A measure-preserving shift identifies the younger marginal for any record
alphabet, including continuous noise laws. -/
theorem expected_difference_of_shift [MeasurableSpace Ω] {μ : Measure Ω}
    (next : S → A → S) (r : S → A → ℝ) (i : Ω → S) (input : ℕ → Ω → A)
    (T : Ω → Ω) (hT : MeasurePreserving T μ μ)
    (shift : ∀ k ω, input k (T ω) = input (k + 1) ω) (keepInitial : ∀ ω, i (T ω) = i ω)
    (hY : ∀ n, Integrable (recordPower next r i input n) μ) (n : ℕ) :
    (∫ ω, observedDifference next r i input n ω ∂μ) =
      (∫ ω, recordPower next r i input (n + 1) ω ∂μ) -
        ∫ ω, recordPower next r i input n ω ∂μ := by
  have hshift (ω : Ω) : recordPower next r i (fun k => input (k + 1)) n ω =
      recordPower next r i input n (T ω) := by
    simp only [recordPower, records, shift, keepInitial]
  have hmap : AEStronglyMeasurable (recordPower next r i input n) (Measure.map T μ) := by
    rw [hT.map_eq]
    exact (hY n).aestronglyMeasurable
  have hmean := integral_map hT.measurable.aemeasurable hmap
  rw [hT.map_eq] at hmean
  simp_rw [observedDifference_eq, hshift]
  have hyoung : Integrable (fun ω => recordPower next r i input n (T ω)) μ :=
    hT.integrable_comp_of_integrable (hY n)
  rw [integral_sub (hY (n + 1)) hyoung, ← hmean]

end NoiseDisplay.LagMarginal
