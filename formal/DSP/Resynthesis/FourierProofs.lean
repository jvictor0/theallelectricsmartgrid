import DSP.Resynthesis.Fourier

namespace DSP.Resynthesis

open scoped BigOperators ZMod

/-- The inverse output is the real part of the synthesis sum on its output grid.
The factor in `ifft` cancels the inverse DFT's normalization. -/
theorem ifft_sample_eq_sum {n m : ℕ} [NeZero m] (spectrum : DFT ℝ n) (t : ZMod m) :
    (ifft spectrum).sample t =
      (∑ k : ZMod m, ZMod.stdAddChar (k * t) * inverseSpectrum spectrum k).re := by
  have hm : (m : ℂ) ≠ 0 := NeZero.ne _
  simp only [ifft, ZMod.invDFT_apply, smul_eq_mul, ← mul_assoc, mul_inv_cancel₀ hm, one_mul]

/-- A spectrum whose only nonzero stored coefficient is DC produces the same
real sample at every output index, independently of the output length. -/
theorem ifft_sample_eq_dc {n m : ℕ} [NeZero m] (spectrum : DFT ℝ n) (t : ZMod m)
    (h : 0 < n / 2)
    (only_dc : ∀ k : Fin (n / 2), k.val ≠ 0 → spectrum.coeff k = ⟨0, 0⟩) :
    (ifft spectrum).sample t = (spectrum.coeff ⟨0, h⟩).real := by
  rw [ifft_sample_eq_sum, Finset.sum_eq_single (0 : ZMod m)]
  · simp [inverseSpectrum, ZMod.val_zero, h]
  · intro k _ hk
    have hk_positive : k.val ≠ 0 := fun hz => hk ((ZMod.val_eq_zero k).mp hz)
    have hk_mirror_positive : m - k.val ≠ 0 := Nat.sub_ne_zero_of_lt (ZMod.val_lt k)
    unfold inverseSpectrum
    split
    · rw [only_dc _ hk_positive]
      change _ * (0 : ℂ) = 0
      exact mul_zero _
    · split
      · rw [only_dc _ hk_mirror_positive]
        change _ * (⟨0, -(0 : ℝ)⟩ : ℂ) = 0
        rw [neg_zero]
        change _ * (0 : ℂ) = 0
        exact mul_zero _
      · simp
  · simp

end DSP.Resynthesis
