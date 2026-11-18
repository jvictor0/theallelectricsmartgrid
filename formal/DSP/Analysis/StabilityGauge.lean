import DSP.Analysis.SchurDecay
import Mathlib.Analysis.Seminorm

/-! A stable finite-dimensional linear update admits a norm equivalent to the
original norm in which one sample is contractive. We use a seminorm plus
explicit comparison bounds, avoiding any change to the runtime state type. -/

noncomputable section
namespace DSP

structure StabilityGauge {E : Type*} [NormedAddCommGroup E] [NormedSpace ℝ E]
    (A : E →ₗ[ℝ] E) where
  size : Seminorm ℝ E
  bound : ℝ
  bound_pos : 0 < bound
  norm_le : ∀ x, ‖x‖ ≤ size x
  le_norm : ∀ x, size x ≤ bound*‖x‖
  rate : ℝ
  rate_nonneg : 0 ≤ rate
  rate_lt_one : rate < 1
  contracts : ∀ x, size (A x) ≤ rate*size x

namespace StabilityGauge
open Finset

section Comap
variable {E F : Type*} [NormedAddCommGroup E] [NormedSpace ℝ E]
    [NormedAddCommGroup F] [NormedSpace ℝ F]

def comap {A : E →ₗ[ℝ] E} {B : F →ₗ[ℝ] F} (g : StabilityGauge A)
    (j : F →ₗ[ℝ] E) (hj : ∀ x, ‖j x‖ = ‖x‖)
    (hcomm : ∀ x, j (B x) = A (j x)) : StabilityGauge B where
  size := g.size.comp j
  bound := g.bound
  bound_pos := g.bound_pos
  norm_le x := by simpa only [Seminorm.comp_apply, hj] using g.norm_le (j x)
  le_norm x := by simpa only [Seminorm.comp_apply, hj] using g.le_norm (j x)
  rate := g.rate
  rate_nonneg := g.rate_nonneg
  rate_lt_one := g.rate_lt_one
  contracts x := by simpa only [Seminorm.comp_apply, hcomm] using g.contracts (j x)

end Comap

variable {E : Type*} [NormedAddCommGroup E] [NormedSpace ℂ E]

def powerSize (A : E →L[ℂ] E) (N : ℕ) : Seminorm ℝ E :=
  ∑ i ∈ range N, (normSeminorm ℝ E).comp ((A^i).restrictScalars ℝ).toLinearMap

theorem powerSize_apply (A : E →L[ℂ] E) (N : ℕ) (x : E) :
    powerSize A N x = ∑ i ∈ range N, ‖(A^i) x‖ := by
  induction N with
  | zero => simp [powerSize]
  | succ N ih =>
    rw [powerSize, sum_range_succ, add_apply, ← powerSize, ih, sum_range_succ]
    rfl

theorem norm_le_powerSize (A : E →L[ℂ] E) (N : ℕ) (hN : 0 < N) (x : E) :
    ‖x‖ ≤ powerSize A N x := by
  rw [powerSize_apply]
  have h := single_le_sum (fun i (_ : i ∈ range N) => norm_nonneg ((A^i) x))
    (show 0 ∈ range N by simpa using hN)
  simpa using h

theorem powerSize_le (A : E →L[ℂ] E) (N : ℕ) (x : E) :
    powerSize A N x ≤ (∑ i ∈ range N, ‖A^i‖)*‖x‖ := by
  rw [powerSize_apply, sum_mul]
  exact sum_le_sum (fun i _ => (A^i).le_opNorm x)

theorem powerSize_step (A : E →L[ℂ] E) (N : ℕ) (x : E) :
    powerSize A N (A x) + ‖x‖ = powerSize A N x + ‖(A^N) x‖ := by
  simp only [powerSize_apply]
  have h := sum_range_succ' (fun i => ‖(A^i) x‖) N
  rw [sum_range_succ] at h
  simpa only [pow_succ, mul_apply_eq_comp, pow_zero,
    one_apply_eq_self] using h.symm

def ofContractingPower (A : E →L[ℂ] E) (N : ℕ) (hN : 0 < N)
    (hA : ‖A^N‖ < 1) : StabilityGauge (A.restrictScalars ℝ).toLinearMap := by
  let C := 1 + ∑ i ∈ range N, ‖A^i‖
  have hsum : 0 ≤ ∑ i ∈ range N, ‖A^i‖ := sum_nonneg (fun _ _ => norm_nonneg _)
  have hC : 0 < C := by dsimp only [C]; linarith
  have hC1 : 1 ≤ C := by dsimp only [C]; linarith
  let d := 1-‖A^N‖
  have hd : 0 < d := sub_pos.mpr hA
  have hd1 : d ≤ 1 := by dsimp only [d]; linarith [norm_nonneg (A^N)]
  refine ⟨powerSize A N, C, hC, norm_le_powerSize A N hN,
    ?_, 1-d/C, ?_, ?_, ?_⟩
  · intro x
    have h := powerSize_le A N x
    dsimp only [C]
    nlinarith [norm_nonneg x]
  · have : d/C ≤ 1 := (div_le_one hC).mpr (hd1.trans hC1)
    linarith
  · have : 0 < d/C := div_pos hd hC
    linarith
  · intro x
    have hs := powerSize_step A N x
    have ha := (A^N).le_opNorm x
    have hp : powerSize A N x ≤ C*‖x‖ := by
      have h := powerSize_le A N x
      dsimp only [C]
      nlinarith [norm_nonneg x]
    have hm := mul_le_mul_of_nonneg_left hp (div_nonneg hd.le hC.le)
    have he : d/C*C = d := div_mul_cancel₀ d (ne_of_gt hC)
    rw [← mul_assoc, he] at hm
    change powerSize A N (A x) ≤ _
    dsimp only [d] at *
    nlinarith

def ofSpectrum [CompleteSpace E] [Nontrivial E] (A : E →L[ℂ] E)
    (hA : ∀ z ∈ spectrum ℂ A, ‖z‖ < 1) :
    StabilityGauge (A.restrictScalars ℝ).toLinearMap :=
  let h := SchurDecay.exists_contracting_power A hA
  ofContractingPower A h.choose h.choose_spec.1 h.choose_spec.2

end StabilityGauge
end DSP
