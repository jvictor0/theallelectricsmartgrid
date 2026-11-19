import DSP.DelayBuffer.History
import Mathlib.Data.Real.Basic
import Mathlib.Algebra.BigOperators.Group.Finset.Basic
import Mathlib.Tactic.LinearCombination
import Mathlib.Tactic.Ring

namespace DSP.DelayBuffer

variable {capacity : ℕ}

/-- Zero extension used only to sum finite histories in proofs. The executable
read API always requires a bounded index. -/
def sample (s : DelayBuffer capacity ℝ) (i : ℕ) : ℝ :=
  if h : i < capacity then s.read ⟨i, h⟩ else 0

def energy (s : DelayBuffer capacity ℝ) (count : ℕ) (r : ℝ) : ℝ :=
  ∑ i ∈ Finset.range count, r ^ (2 * i) * (sample s i) ^ 2

theorem energy_nonneg (s : DelayBuffer capacity ℝ) (count : ℕ) (r : ℝ) :
    0 ≤ energy s count r := by
  apply Finset.sum_nonneg
  intro i _
  exact mul_nonneg (by rw [pow_mul]; exact pow_nonneg (sq_nonneg r) i) (sq_nonneg _)

theorem sample_push_zero (s : DelayBuffer capacity ℝ) (x : ℝ) (h : 0 < capacity) :
    sample (s.push x) 0 = x := by
  simp [sample, h, read_push_zero]

theorem sample_push_succ (s : DelayBuffer capacity ℝ) (x : ℝ) (i : ℕ)
    (hi : i + 1 < capacity) : sample (s.push x) (i + 1) = sample s i := by
  simp only [sample, dif_pos hi, dif_pos (show i < capacity by omega)]
  rw [read_push_succ _ _ _ (by simp)]
  rfl

theorem energy_push (s : DelayBuffer capacity ℝ) (x r : ℝ) (n : ℕ)
    (hn : n < capacity) :
    energy (s.push x) (n + 1) r = x ^ 2 + r ^ 2 * energy s n r := by
  unfold energy
  rw [Finset.sum_range_succ']
  simp only [Nat.mul_zero, pow_zero, one_mul, sample_push_zero s x (by omega)]
  rw [Finset.mul_sum]
  have he : (∑ i ∈ Finset.range n, r ^ (2 * (i + 1)) * sample (s.push x) (i + 1) ^ 2) =
      ∑ i ∈ Finset.range n, r ^ 2 * (r ^ (2 * i) * sample s i ^ 2) := by
    apply Finset.sum_congr rfl
    intro i hi
    rw [sample_push_succ s x i (by have := Finset.mem_range.mp hi; omega)]
    rw [Nat.mul_add, pow_add]
    ring
  rw [he]
  ring

/-- The tap may be anywhere within capacity; only its active history contributes.
Radial attenuation is outside the raw buffer, weighted storage accounts for it. -/
theorem weighted_energy (s : DelayBuffer capacity ℝ) (x r : ℝ) (age : Fin capacity) :
    (r ^ (age.val + 1) * s.read age) ^ 2 + energy (s.push x) (age.val + 1) r =
      x ^ 2 + r ^ 2 * energy s (age.val + 1) r := by
  rw [energy_push s x r age.val age.isLt]
  have he : energy s (age.val + 1) r = energy s age.val r +
      r ^ (2 * age.val) * (s.read age) ^ 2 := by
    simp [energy, Finset.sum_range_succ, sample, age.isLt]
  rw [he, pow_succ, show r ^ (2 * age.val) = (r ^ age.val) ^ 2 by
    rw [Nat.mul_comm, pow_mul]]
  ring

theorem lossless_energy (s : DelayBuffer capacity ℝ) (x : ℝ) (age : Fin capacity) :
    (s.read age) ^ 2 + energy (s.push x) (age.val + 1) 1 =
      x ^ 2 + energy s (age.val + 1) 1 := by
  simpa using weighted_energy s x 1 age

end DSP.DelayBuffer
