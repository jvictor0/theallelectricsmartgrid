import DSP.Resynthesis.HannTone

namespace DSP.Resynthesis

open scoped ZMod
open AddChar

noncomputable section

private def turnExp (p : ℝ) : ℂ := Complex.exp ((↑(2 * Real.pi * p) : ℂ) * Complex.I)

private theorem turnExp_add (p q : ℝ) : turnExp (p + q) = turnExp p * turnExp q := by
  unfold turnExp
  rw [← Complex.exp_add]
  congr 1
  push_cast
  ring

private theorem cos_eq_turnExp (p : ℝ) :
    (↑(Trig.cos2pi p) : ℂ) = (turnExp p + turnExp (-p)) / 2 := by
  have h : 2 * Real.pi * -p = -(2 * Real.pi * p) := by ring
  rw [turnExp, turnExp, Complex.exp_ofReal_mul_I, h, Complex.exp_ofReal_mul_I,
    Real.cos_neg, Real.sin_neg, Complex.ofReal_neg, cos2pi_real]
  ring

private def carrier {n : ℕ} [NeZero n] (b : ZMod n) (t : ZMod n) : ℂ :=
  ZMod.stdAddChar (b * t)

private theorem carrier_cast {n : ℕ} [NeZero n] (b : ℤ) (t : ZMod n) :
    carrier (b : ZMod n) t = turnExp ((b : ℝ) * (t.val / n)) := by
  conv_lhs => rw [carrier, ← ZMod.natCast_zmod_val t, ← Int.cast_natCast t.val,
    ← Int.cast_mul, ZMod.stdAddChar_coe]
  rw [turnExp]
  push_cast
  congr 1
  ring

private theorem carrier_add {n : ℕ} [NeZero n] (a b t : ZMod n) :
    carrier (a + b) t = carrier a t * carrier b t := by
  simp only [carrier, add_mul, map_add_eq_mul]

private theorem dft_carrier {n : ℕ} [NeZero n] (b k : ZMod n) :
    𝓕 (carrier b) k = if k = b then (n : ℂ) else 0 := by
  rw [ZMod.dft_apply]
  have hs : (∑ t : ZMod n, ZMod.stdAddChar (-(t * k)) • carrier b t) =
      ∑ t : ZMod n, ZMod.stdAddChar ((b - k) * t) := by
    apply Finset.sum_congr rfl
    intro t _
    simp only [carrier, smul_eq_mul, ← map_add_eq_mul]
    congr 1
    ring
  rw [hs]
  by_cases h : k = b
  · simp [h]
  · rw [if_neg h]
    have hsub : b - k ≠ 0 := sub_ne_zero.mpr (Ne.symm h)
    exact sum_eq_zero_of_ne_one (ZMod.isPrimitive_stdAddChar n hsub)

private theorem hann_expansion {n : ℕ} [NeZero n] (amplitude phase : ℝ) (bin : ℕ) :
    (fun t => ((hannSinusoid (n := n) amplitude bin phase).sample t : ℂ)) =
      ((amplitude : ℂ) / 4 * turnExp phase) • carrier (bin : ZMod n)
      + (-(amplitude : ℂ) / 8 * turnExp phase) • carrier ((bin : ZMod n) - 1)
      + (-(amplitude : ℂ) / 8 * turnExp phase) • carrier ((bin : ZMod n) + 1)
      + ((amplitude : ℂ) / 4 * turnExp (-phase)) • carrier (-(bin : ZMod n))
      + (-(amplitude : ℂ) / 8 * turnExp (-phase)) • carrier (-(bin : ZMod n) + 1)
      + (-(amplitude : ℂ) / 8 * turnExp (-phase)) • carrier (-(bin : ZMod n) - 1) := by
  funext t
  have hc (b : ℤ) : turnExp ((b : ℝ) * (t.val / n)) = carrier (b : ZMod n) t :=
    (carrier_cast b t).symm
  have hcarrier :
      turnExp ((bin : ℝ) / n * t.val) = carrier (bin : ZMod n) t := by
    simpa only [Int.cast_natCast, div_mul_eq_mul_div, mul_div_assoc] using hc (bin : ℤ)
  have hnegCarrier :
      turnExp (-((bin : ℝ) / n * t.val)) = carrier (-(bin : ZMod n)) t := by
    simpa only [Int.cast_neg, Int.cast_natCast, neg_mul, div_mul_eq_mul_div, mul_div_assoc]
      using hc (-(bin : ℤ))
  have hone : turnExp (t.val / n) = carrier 1 t := by simpa using hc 1
  have hnegOne : turnExp (-(t.val / n)) = carrier (-1) t := by simpa using hc (-1)
  change (↑(amplitude * Trig.cos2pi ((bin : ℝ) / n * t.val + phase) *
      (0.5 * (1 - Trig.cos2pi ((t.val : ℝ) / n)))) : ℂ) = _
  push_cast
  simp only [cos_eq_turnExp, neg_add, turnExp_add,
    hcarrier, hnegCarrier, hone, hnegOne, Pi.add_apply, Pi.smul_apply, smul_eq_mul,
    sub_eq_add_neg, carrier_add]
  norm_num
  ring

private theorem dft_hann_expansion {n : ℕ} [NeZero n] (amplitude phase : ℝ) (bin : ℕ)
    (k : ZMod n) :
    𝓕 (fun t => ((hannSinusoid (n := n) amplitude bin phase).sample t : ℂ)) k =
      ((amplitude : ℂ) / 4 * turnExp phase) * (if k = (bin : ZMod n) then (n : ℂ) else 0)
      + (-(amplitude : ℂ) / 8 * turnExp phase) * (if k = (bin : ZMod n) - 1 then (n : ℂ) else 0)
      + (-(amplitude : ℂ) / 8 * turnExp phase) * (if k = (bin : ZMod n) + 1 then (n : ℂ) else 0)
      + ((amplitude : ℂ) / 4 * turnExp (-phase)) * (if k = -(bin : ZMod n) then (n : ℂ) else 0)
      + (-(amplitude : ℂ) / 8 * turnExp (-phase)) * (if k = -(bin : ZMod n) + 1 then (n : ℂ) else 0)
      + (-(amplitude : ℂ) / 8 * turnExp (-phase)) * (if k = -(bin : ZMod n) - 1 then (n : ℂ) else 0) := by
  simp only [hann_expansion, map_add, map_smul, Pi.add_apply, Pi.smul_apply, smul_eq_mul,
    dft_carrier]

theorem fft_hannSinusoid {n : ℕ} [NeZero n] (amplitude phase : ℝ) (bin : ℕ)
    (hlo : 1 < bin) (hhi : bin + 1 < n / 2) :
    fft (hannSinusoid (n := n) amplitude bin phase) =
      hannSinusoidSpectrum amplitude bin phase := by
  unfold fft hannSinusoidSpectrum
  congr 1
  funext k
  have hk := k.isLt
  have hpos (j : ℕ) (hj : j < n / 2) : (k.val : ZMod n) = (j : ZMod n) ↔ k.val = j := by
    constructor
    · intro h
      have hv := congrArg ZMod.val h
      simpa only [ZMod.val_natCast_of_lt (show k.val < n by omega),
        ZMod.val_natCast_of_lt (show j < n by omega)] using hv
    · intro h
      rw [h]
  have hneg (j : ℕ) (hj0 : 0 < j) (hj : j < n / 2) :
      (k.val : ZMod n) ≠ -(j : ZMod n) := by
    intro h
    have hz : ((k.val + j : ℕ) : ZMod n) = 0 := by
      rw [Nat.cast_add, h, neg_add_cancel]
    have hv := congrArg ZMod.val hz
    rw [ZMod.val_natCast_of_lt (show k.val + j < n by omega), ZMod.val_zero] at hv
    omega
  have hbelow : ((bin - 1 : ℕ) : ZMod n) = (bin : ZMod n) - 1 := by
    rw [Nat.cast_sub (show 1 ≤ bin by omega), Nat.cast_one]
  have habove : ((bin + 1 : ℕ) : ZMod n) = (bin : ZMod n) + 1 := by
    rw [Nat.cast_add, Nat.cast_one]
  have hnegativeBelow : -(bin : ZMod n) + 1 = -((bin - 1 : ℕ) : ZMod n) := by
    rw [hbelow]
    ring
  have hnegativeAbove : -(bin : ZMod n) - 1 = -((bin + 1 : ℕ) : ZMod n) := by
    rw [habove]
    ring
  have hn : (n : ℂ) ≠ 0 := NeZero.ne _
  have hvalue :
      𝓕 (fun t => ((hannSinusoid (n := n) amplitude bin phase).sample t : ℂ))
          (k.val : ZMod n) / n =
        (↑(if k.val = bin then amplitude / 4
           else if k.val + 1 = bin ∨ k.val = bin + 1 then -amplitude / 8
           else 0) : ℂ) * turnExp phase := by
    rw [dft_hann_expansion, hnegativeBelow, hnegativeAbove, ← hbelow, ← habove]
    simp only [if_neg (hneg bin (by omega) (by omega)),
      if_neg (hneg (bin - 1) (by omega) (by omega)), if_neg (hneg (bin + 1) (by omega) hhi),
      mul_zero, add_zero, hpos bin (by omega), hpos (bin - 1) (by omega), hpos (bin + 1) hhi]
    split_ifs <;> first
    | omega
    | simp [mul_div_cancel_right₀ _ hn]
  rw [hvalue]
  generalize (if k.val = bin then amplitude / 4
      else if k.val + 1 = bin ∨ k.val = bin + 1 then -amplitude / 8
      else 0) = coefficient
  have hreal : (turnExp phase).re = Trig.cos2pi phase :=
    Complex.exp_ofReal_mul_I_re _
  have himag : (turnExp phase).im = Trig.sin2pi phase :=
    Complex.exp_ofReal_mul_I_im _
  simp only [exactMath, Complex.mul_re, Complex.mul_im, Complex.ofReal_re,
    Complex.ofReal_im, zero_mul, sub_zero, add_zero, hreal, himag]

end

end DSP.Resynthesis
