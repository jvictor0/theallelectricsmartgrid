import DSP.Filters.SquiggleBoyQuadReverb.Frozen.Transfer
import Mathlib.Analysis.CStarAlgebra.Matrix
import Mathlib.Analysis.Normed.Ring.Units
import Mathlib.MeasureTheory.Constructions.Polish.Basic
import Mathlib.MeasureTheory.Integral.DominatedConvergence

namespace DSP.SquiggleBoyQuadReverb.Frozen

open Matrix MeasureTheory
open scoped BigOperators Matrix.Norms.L2Operator

noncomputable section

scoped instance : MeasurableSpace QuadMatrix := borel QuadMatrix
scoped instance : BorelSpace QuadMatrix := ⟨rfl⟩

/-- One trip around the feedback path, including the saved wet sample. -/
def loopReturn (p : Spec ℂ) (q : ℂ) : QuadMatrix :=
  q • (forward p q * diagonal p.feedback)

/-- The input to the wet output for a single pass, including input diffusion. -/
def directTransfer (p : Spec ℂ) (q : ℂ) : QuadMatrix :=
  forward p q * diagonal (introResponse q _ p.intro)

/-- The contribution after (n) returns through the same frozen tank. -/
def returnTerm (p : Spec ℂ) (q : ℂ) (n : ℕ) : QuadMatrix :=
  loopReturn p q ^ n * directTransfer p q

theorem returnDen_isUnit_det_of_norm_lt_one (p : Spec ℂ) (q : ℂ)
    (h : ‖loopReturn p q‖ < 1) : IsUnit (returnDen p q).det :=
  (Matrix.isUnit_iff_isUnit_det _).mp (isUnit_one_sub_of_norm_lt_one h)

theorem transfer_hasSum (p : Spec ℂ) (q : ℂ) (h : ‖loopReturn p q‖ < 1) :
    HasSum (returnTerm p q) (transfer p q) := by
  have hs := (hasSum_geom_series_inverse (loopReturn p q) h).mul_right (directTransfer p q)
  unfold transfer
  rw [Matrix.nonsing_inv_eq_ringInverse]
  exact hs

private theorem norm_pow_mul_le_of_bounds {loop drive : QuadMatrix} {r b : ℝ}
    (hloop : ‖loop‖ ≤ r) (hdrive : ‖drive‖ ≤ b) (n : ℕ) :
    ‖loop ^ n * drive‖ ≤ r ^ n * b :=
  (norm_mul_le _ _).trans <| mul_le_mul
    ((norm_pow_le _ n).trans (pow_le_pow_left₀ (norm_nonneg _) hloop n))
    hdrive (norm_nonneg _) (pow_nonneg ((norm_nonneg _).trans hloop) n)

private theorem norm_returnTerm_le {p : Spec ℂ} {q : ℂ} {r b : ℝ}
    (hloop : ‖loopReturn p q‖ ≤ r) (hdrive : ‖directTransfer p q‖ ≤ b) (n : ℕ) :
    ‖returnTerm p q n‖ ≤ r ^ n * b :=
  norm_pow_mul_le_of_bounds hloop hdrive n

/-- The coherent average of the solved frozen reverb. The measure is joint:
one draw fixes the entire network for all return terms. -/
def expectedTransfer {Ω : Type*} [MeasurableSpace Ω] (μ : Measure Ω)
    (p : Ω → Spec ℂ) (q : ℂ) : QuadMatrix :=
  ∫ x, transfer (p x) q ∂μ

section Probability

variable {Ω : Type*} [MeasurableSpace Ω] {μ : Measure Ω} [IsProbabilityMeasure μ]
  {p : Ω → Spec ℂ} {q : ℂ} {r b : ℝ}

private theorem measurable_returnTerm
    (hloop : Measurable (fun x => loopReturn (p x) q))
    (hdrive : Measurable (fun x => directTransfer (p x) q)) (n : ℕ) :
    Measurable (fun x => returnTerm (p x) q n) :=
  (hloop.pow_const n).mul hdrive

theorem integrable_resolvedMul_of_contraction {loop drive : Ω → QuadMatrix}
    (hr0 : 0 ≤ r) (hr : r < 1) (hloopMeas : Measurable loop) (hdriveMeas : Measurable drive)
    (hloop : ∀ᵐ x ∂μ, ‖loop x‖ ≤ r) (hdrive : ∀ᵐ x ∂μ, ‖drive x‖ ≤ b) :
    Integrable (fun x => Ring.inverse (1 - loop x) * drive x) μ := by
  let f (n : ℕ) (x : Ω) := loop x ^ n * drive x
  have hm : Measurable (fun x => ∑' n, f n x) :=
    Measurable.tsum (fun n => (hloopMeas.pow_const n).fun_mul hdriveMeas)
  have hs : HasSum (fun n : ℕ => r ^ n * b) (b / (1 - r)) := by
    simpa [div_eq_mul_inv, mul_comm] using (hasSum_geometric_of_lt_one hr0 hr).mul_right b
  have hb : ∀ᵐ x ∂μ, ‖∑' n, f n x‖ ≤ b / (1 - r) := by
    filter_upwards [hloop, hdrive] with x hl hd
    exact tsum_of_norm_bounded hs (norm_pow_mul_le_of_bounds hl hd)
  have hi := Integrable.mono' (integrable_const (b / (1 - r))) hm.aestronglyMeasurable hb
  exact hi.congr (hloop.mono fun x hx =>
    ((hasSum_geom_series_inverse (loop x) (hx.trans_lt hr)).mul_right (drive x)).tsum_eq)

theorem integrable_transfer_of_contraction (hr0 : 0 ≤ r) (hr : r < 1)
    (hloopMeas : Measurable (fun x => loopReturn (p x) q))
    (hdriveMeas : Measurable (fun x => directTransfer (p x) q))
    (hloop : ∀ᵐ x ∂μ, ‖loopReturn (p x) q‖ ≤ r)
    (hdrive : ∀ᵐ x ∂μ, ‖directTransfer (p x) q‖ ≤ b) :
    Integrable (fun x => transfer (p x) q) μ := by
  simpa only [transfer, Matrix.nonsing_inv_eq_ringInverse, returnDen, loopReturn, directTransfer] using
    integrable_resolvedMul_of_contraction hr0 hr hloopMeas hdriveMeas hloop hdrive

theorem hasSum_expectedTransfer (hr0 : 0 ≤ r) (hr : r < 1)
    (hloopMeas : Measurable (fun x => loopReturn (p x) q))
    (hdriveMeas : Measurable (fun x => directTransfer (p x) q))
    (hloop : ∀ᵐ x ∂μ, ‖loopReturn (p x) q‖ ≤ r)
    (hdrive : ∀ᵐ x ∂μ, ‖directTransfer (p x) q‖ ≤ b) :
    HasSum (fun n => ∫ x, returnTerm (p x) q n ∂μ) (expectedTransfer μ p q) := by
  let f (n : ℕ) (x : Ω) := returnTerm (p x) q n
  have hbound (n : ℕ) : ∀ᵐ x ∂μ, ‖f n x‖ ≤ r ^ n * b := by
    filter_upwards [hloop, hdrive] with x hl hd using norm_returnTerm_le hl hd n
  have hfi (n : ℕ) : Integrable (f n) μ := by
    have hm : Measurable (f n) := measurable_returnTerm hloopMeas hdriveMeas n
    refine Integrable.mono' (integrable_const (r ^ n * b)) hm.aestronglyMeasurable (hbound n)
  have hnorm (n : ℕ) : ‖∫ x, ‖f n x‖ ∂μ‖ ≤ r ^ n * b := by
    have hpos : 0 ≤ ∫ x, ‖f n x‖ ∂μ := integral_nonneg (fun x => norm_nonneg (f n x))
    rw [Real.norm_eq_abs, abs_of_nonneg hpos]
    simpa only [integral_const, probReal_univ, one_smul] using
      integral_mono_ae (hfi n).norm (integrable_const (r ^ n * b)) (hbound n)
  have hs := ((summable_geometric_of_lt_one hr0 hr).mul_right b).of_norm_bounded hnorm
  have hp : ∀ᵐ x ∂μ, (∑' n, f n x) = transfer (p x) q :=
    hloop.mono fun x hx => (transfer_hasSum (p x) q (hx.trans_lt hr)).tsum_eq
  unfold expectedTransfer
  rw [← integral_congr_ae hp]
  exact hasSum_integral_of_summable_integral_norm hfi hs

/-- An absolutely convergent expansion of the expected closed-loop transfer. -/
theorem expectedTransfer_eq_tsum (hr0 : 0 ≤ r) (hr : r < 1)
    (hloopMeas : Measurable (fun x => loopReturn (p x) q))
    (hdriveMeas : Measurable (fun x => directTransfer (p x) q))
    (hloop : ∀ᵐ x ∂μ, ‖loopReturn (p x) q‖ ≤ r)
    (hdrive : ∀ᵐ x ∂μ, ‖directTransfer (p x) q‖ ≤ b) :
    expectedTransfer μ p q = ∑' n, ∫ x, returnTerm (p x) q n ∂μ :=
  (hasSum_expectedTransfer hr0 hr hloopMeas hdriveMeas hloop hdrive).tsum_eq.symm

/-- With a uniform contraction margin, a finite mean of return terms has a
geometrically bounded error in the Euclidean operator norm. -/
theorem expectedTransfer_truncation_le (hr0 : 0 ≤ r) (hr : r < 1)
    (hloopMeas : Measurable (fun x => loopReturn (p x) q))
    (hdriveMeas : Measurable (fun x => directTransfer (p x) q))
    (hloop : ∀ᵐ x ∂μ, ‖loopReturn (p x) q‖ ≤ r)
    (hdrive : ∀ᵐ x ∂μ, ‖directTransfer (p x) q‖ ≤ b) (N : ℕ) :
    ‖expectedTransfer μ p q - ∑ n ∈ Finset.range N, ∫ x, returnTerm (p x) q n ∂μ‖ ≤
      r ^ N * b / (1 - r) := by
  let a (n : ℕ) : QuadMatrix := ∫ x, returnTerm (p x) q n ∂μ
  have ha : HasSum a (expectedTransfer μ p q) :=
    hasSum_expectedTransfer hr0 hr hloopMeas hdriveMeas hloop hdrive
  have haBound (n : ℕ) : ‖a n‖ ≤ r ^ n * b := by
    have hp : ∀ᵐ x ∂μ, ‖returnTerm (p x) q n‖ ≤ r ^ n * b := by
      filter_upwards [hloop, hdrive] with x hl hd using norm_returnTerm_le hl hd n
    simpa only [a, probReal_univ, mul_one] using norm_integral_le_of_norm_le_const hp
  have ht : HasSum (fun n : ℕ => r ^ (n + N) * b) (r ^ N * b / (1 - r)) := by
    rw [show (fun n : ℕ => r ^ (n + N) * b) = (fun n => (r ^ N * b) * r ^ n) by
      funext n
      rw [pow_add]
      ring]
    simpa only [div_eq_mul_inv] using (hasSum_geometric_of_lt_one hr0 hr).mul_left (r ^ N * b)
  have hns := ht.summable.of_nonneg_of_le (fun _ => norm_nonneg _) (fun n => haBound (n + N))
  have hsplit := ha.summable.sum_add_tsum_nat_add N
  change ‖expectedTransfer μ p q - ∑ n ∈ Finset.range N, a n‖ ≤ _
  rw [← ha.tsum_eq, ← hsplit, add_sub_cancel_left]
  refine (norm_tsum_le_tsum_norm hns).trans ?_
  rw [← ht.tsum_eq]
  exact hns.tsum_le_tsum (fun n => haBound (n + N)) ht.summable

end Probability

end

end DSP.SquiggleBoyQuadReverb.Frozen
