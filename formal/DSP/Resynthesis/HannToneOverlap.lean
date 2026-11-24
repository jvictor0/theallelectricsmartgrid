import DSP.Resynthesis.HannTone
import Mathlib.Tactic.FieldSimp
import Mathlib.Tactic.Ring

namespace DSP.Resynthesis

theorem hann_quarter_square_sum (u : ℝ) :
    hannWeight u ^ 2 + hannWeight (u + 1 / 4) ^ 2 +
      hannWeight (u + 1 / 2) ^ 2 + hannWeight (u + 3 / 4) ^ 2 = 3 / 2 := by
  have q1 : 2 * Real.pi * (u + 1 / 4) = 2 * Real.pi * u + Real.pi / 2 := by ring
  have q2 : 2 * Real.pi * (u + 1 / 2) = 2 * Real.pi * u + Real.pi := by ring
  have q3 : 2 * Real.pi * (u + 3 / 4) = (2 * Real.pi * u + Real.pi / 2) + Real.pi := by ring
  simp only [hannWeight, cos2pi_real, q1, q2, q3, Real.cos_add_pi, Real.cos_add_pi_div_two, neg_neg]
  nlinarith [Real.sin_sq_add_cos_sq (2 * Real.pi * u)]

theorem hannSinusoid_twice_sample {m : ℕ} (amplitude : ℝ) (bin : ℕ) (phase : ℝ)
    (time : ZMod m) :
    (exactMath.hann (hannSinusoid amplitude bin phase)).sample time =
      hannWeight ((time.val : ℝ) / m) ^ 2 * sinusoid amplitude ((bin : ℝ) / m) phase time.val := by
  change sinusoid amplitude ((bin : ℝ) / m) phase time.val * hannWeight ((time.val : ℝ) / m) *
    hannWeight ((time.val : ℝ) / m) = _
  ring

/-- The four active frames at offset `offset` within an engine-rate hop.
The frame clock places successive frames a quarter window apart. -/
noncomputable def steadyOverlapAdd {hop : ℕ} (frames : ℕ → Buffer ℝ (4 * hop))
    (grain : ℕ) (offset : Fin hop) : ℝ :=
  ((frames grain).sample (offset.val : ZMod (4 * hop)) +
    (frames (grain - 1)).sample ((offset.val + hop : ℕ) : ZMod (4 * hop)) +
    (frames (grain - 2)).sample ((offset.val + 2 * hop : ℕ) : ZMod (4 * hop)) +
    (frames (grain - 3)).sample ((offset.val + 3 * hop : ℕ) : ZMod (4 * hop))) / (3 / 2)

/-- Once the four windowed grains have the intended pitch and scheduled
phases, the normalized overlap-add is exactly the continuous sinusoid at the
corresponding engine sample. -/
theorem steadyOverlapAdd_hannSinusoid {hop : ℕ} (frames : ℕ → Buffer ℝ (4 * hop))
    (amplitude phase : ℝ) (bin grain : ℕ) (offset : Fin hop) (ready : 3 ≤ grain)
    (matching : ∀ j, j ≤ grain → frames j = exactMath.hann
      (hannSinusoid amplitude bin (phase + ((bin : ℝ) / (4 * hop)) * (j * hop : ℕ)))) :
    steadyOverlapAdd frames grain offset =
      sinusoid amplitude ((bin : ℝ) / (4 * hop)) phase (grain * hop + offset.val : ℕ) := by
  have hhop : 0 < hop := Nat.zero_lt_of_lt offset.isLt
  letI : NeZero (4 * hop) := ⟨by omega⟩
  have hhopReal : (hop : ℝ) ≠ 0 := by exact_mod_cast hhop.ne'
  have sample_at (j : ℕ) (small : j ≤ 3) :
      (frames (grain - j)).sample ((offset.val + j * hop : ℕ) : ZMod (4 * hop)) =
        hannWeight ((offset.val : ℝ) / (4 * hop) + (j : ℝ) / 4) ^ 2 *
          sinusoid amplitude ((bin : ℝ) / (4 * hop)) phase (grain * hop + offset.val : ℕ) := by
    have index_lt : offset.val + j * hop < 4 * hop := by
      calc
        offset.val + j * hop < hop + j * hop := Nat.add_lt_add_right offset.isLt _
        _ ≤ 4 * hop := by
          have := Nat.mul_le_mul_right hop small
          omega
    have j_le : j ≤ grain := small.trans ready
    rw [matching _ (Nat.sub_le ..), hannSinusoid_twice_sample, ZMod.val_natCast_of_lt index_lt]
    have window : (((offset.val + j * hop : ℕ) : ℝ) / (↑(4 * hop) : ℝ)) =
        (offset.val : ℝ) / (4 * hop) + (j : ℝ) / 4 := by
      push_cast
      field_simp
    rw [window]
    congr 1
    unfold sinusoid
    congr 2
    push_cast [Nat.cast_sub j_le]
    ring
  have first := sample_at 0 (by decide)
  simp only [Nat.sub_zero, Nat.zero_mul, Nat.cast_zero, zero_div, add_zero] at first
  have second := sample_at 1 (by decide)
  simp only [Nat.one_mul, Nat.cast_one] at second
  unfold steadyOverlapAdd
  rw [first, second, sample_at 2 (by decide), sample_at 3 (by decide)]
  norm_num only [Nat.cast_ofNat]
  simp only [← add_mul]
  rw [hann_quarter_square_sum]
  ring

end DSP.Resynthesis
