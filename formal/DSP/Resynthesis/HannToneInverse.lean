import DSP.Resynthesis.HannTone
import DSP.Resynthesis.FourierProofs
import Mathlib.Tactic.Ring

namespace DSP.Resynthesis

open scoped BigOperators ZMod

noncomputable section

private def tonePair {m : ℕ} (radius : ℝ) (bin : ℕ) (phase : ℝ) (k : ZMod m) : ℂ :=
  (if k = (bin : ZMod m) then
    ⟨radius * Trig.cos2pi phase, radius * Trig.sin2pi phase⟩ else 0) +
  (if k = -(bin : ZMod m) then
    ⟨radius * Trig.cos2pi phase, -(radius * Trig.sin2pi phase)⟩ else 0)

private theorem toneBinLocations {n m : ℕ} [NeZero m] (hnm : n ≤ m)
    {bin : ℕ} (hb : 0 < bin) (hb' : bin < n / 2) (k : ZMod m) :
    (k = (bin : ZMod m) ↔ k.val = bin) ∧
    (k = -(bin : ZMod m) ↔ m - k.val = bin) := by
  have hbm : bin < m := by omega
  have hval : (bin : ZMod m).val = bin := ZMod.val_natCast_of_lt hbm
  haveI : NeZero (bin : ZMod m) :=
    ⟨fun hz => by have := congrArg ZMod.val hz; rw [hval, ZMod.val_zero] at this; omega⟩
  have hneg : (-(bin : ZMod m)).val = m - bin := by
    rw [ZMod.val_neg_of_ne_zero, hval]
  have hk := ZMod.val_lt k
  constructor
  · rw [← (ZMod.val_injective m).eq_iff, hval]
  · rw [← (ZMod.val_injective m).eq_iff, hneg]
    omega

private theorem inverseSpectrum_hannTone {n m : ℕ} [NeZero m]
    (amplitude phase : ℝ) (bin : ℕ) (hb : 1 < bin) (hb' : bin + 1 < n / 2)
    (hnm : n ≤ m) (k : ZMod m) :
    inverseSpectrum (hannSinusoidSpectrum (n := n) amplitude bin phase) k =
      tonePair (amplitude / 4) bin phase k +
      tonePair (-amplitude / 8) (bin - 1) phase k +
      tonePair (-amplitude / 8) (bin + 1) phase k := by
  obtain ⟨hc, hnc⟩ := toneBinLocations hnm (bin := bin) (by omega) (by omega) k
  obtain ⟨hl, hnl⟩ := toneBinLocations hnm (bin := bin - 1) (by omega) (by omega) k
  obtain ⟨hh, hnh⟩ := toneBinLocations hnm (bin := bin + 1) (by omega) hb' k
  simp only [tonePair, hc, hnc, hl, hnl, hh, hnh]
  unfold inverseSpectrum
  have hk := ZMod.val_lt k
  have side (j : ℕ) : (j + 1 = bin ∨ j = bin + 1) ↔ (j = bin - 1 ∨ j = bin + 1) := by
    omega
  simp only [hannSinusoidSpectrum, exactMath, side]
  split
  · rename_i h
    have hnc : m - k.val ≠ bin := by omega
    have hnl : m - k.val ≠ bin - 1 := by omega
    have hnh : m - k.val ≠ bin + 1 := by omega
    simp only [hnc, hnl, hnh, if_false, add_zero]
    split_ifs <;> first | omega | simp <;> rfl
  · rename_i h
    have hc : k.val ≠ bin := by omega
    have hl : k.val ≠ bin - 1 := by omega
    have hh : k.val ≠ bin + 1 := by omega
    simp only [hc, hl, hh, if_false, zero_add]
    split
    · split_ifs <;> first | omega | simp <;> rfl
    · rename_i hn
      have hnc : m - k.val ≠ bin := by omega
      have hnl : m - k.val ≠ bin - 1 := by omega
      have hnh : m - k.val ≠ bin + 1 := by omega
      simp [hnc, hnl, hnh]

private theorem toneCharacter {m : ℕ} [NeZero m] (j : ℤ) :
    ZMod.stdAddChar (j : ZMod m) =
      Complex.exp ((2 * Real.pi * ((j : ℝ) / m) : ℝ) * Complex.I) := by
  rw [ZMod.stdAddChar_coe]
  push_cast
  ring_nf

private theorem toneSample {m : ℕ} [NeZero m] (radius phase : ℝ) (bin : ℕ) (t : ZMod m) :
    (∑ k : ZMod m, ZMod.stdAddChar (k * t) * tonePair radius bin phase k).re =
      2 * radius * Trig.cos2pi ((bin : ℝ) / m * t.val + phase) := by
  simp only [tonePair, mul_add, Finset.sum_add_distrib, mul_ite, mul_zero,
    Finset.sum_ite_eq', Finset.mem_univ, if_true]
  have hpos : (bin : ZMod m) * t = (((bin : ℤ) * (t.val : ℤ) : ℤ) : ZMod m) := by
    push_cast
    rw [ZMod.natCast_zmod_val]
  have hneg : -(bin : ZMod m) * t = ((-((bin : ℤ) * (t.val : ℤ)) : ℤ) : ZMod m) := by
    push_cast
    rw [ZMod.natCast_zmod_val, neg_mul]
  rw [hpos, hneg, toneCharacter, toneCharacter]
  simp only [Complex.add_re, Complex.mul_re, Complex.exp_ofReal_mul_I_re,
    Complex.exp_ofReal_mul_I_im, DSP.cos2pi_real, DSP.sin2pi_real]
  push_cast
  have hposAngle : 2 * Real.pi * ((bin : ℝ) / m * t.val + phase) =
      2 * Real.pi * ((bin : ℝ) * t.val / m) + 2 * Real.pi * phase := by ring
  have hnegAngle : 2 * Real.pi * (-((bin : ℝ) * t.val) / m) =
      -(2 * Real.pi * ((bin : ℝ) * t.val / m)) := by ring
  rw [hposAngle, Real.cos_add, hnegAngle, Real.cos_neg, Real.sin_neg]
  ring

/-- The interior Hann sidebands synthesize the same tone and window on any
larger output grid, with unchanged bin numbers, amplitude, and phase. -/
theorem ifft_hannSinusoidSpectrum {n m : ℕ} [NeZero m] (amplitude phase : ℝ) (bin : ℕ)
    (hb : 1 < bin) (hb' : bin + 1 < n / 2) (hnm : n ≤ m) :
    ifft (hannSinusoidSpectrum (n := n) amplitude bin phase) =
      hannSinusoid (n := m) amplitude bin phase := by
  apply congrArg Buffer.mk
  funext t
  change (ifft (hannSinusoidSpectrum (n := n) amplitude bin phase)).sample t =
    (hannSinusoid amplitude bin phase).sample t
  rw [ifft_sample_eq_sum]
  simp_rw [inverseSpectrum_hannTone amplitude phase bin hb hb' hnm, mul_add,
    Finset.sum_add_distrib, Complex.add_re, toneSample]
  change _ = sinusoid amplitude ((bin : ℝ) / m) phase t.val * hannWeight ((t.val : ℝ) / m)
  unfold sinusoid hannWeight
  simp only [DSP.cos2pi_real]
  have hcast : ((bin - 1 : ℕ) : ℝ) = (bin : ℝ) - 1 := by
    rw [Nat.cast_sub (by omega : 1 ≤ bin), Nat.cast_one]
  rw [hcast, Nat.cast_add, Nat.cast_one]
  set center := 2 * Real.pi * ((bin : ℝ) / m * t.val + phase)
  set shift := 2 * Real.pi * ((t.val : ℝ) / m)
  have hl : 2 * Real.pi * (((bin : ℝ) - 1) / m * t.val + phase) = center - shift := by
    dsimp [center, shift]
    ring
  have hh : 2 * Real.pi * (((bin : ℝ) + 1) / m * t.val + phase) = center + shift := by
    dsimp [center, shift]
    ring
  rw [hl, hh, Real.cos_add, Real.cos_sub]
  ring

end

end DSP.Resynthesis
