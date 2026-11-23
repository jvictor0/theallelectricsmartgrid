import DSP.ResponseMeasurement.ErrorBounds
import DSP.Analysis.IntervalArithmetic

/-! Certified finite-window powers from real sine/cosine and sample boxes.
These bounds include every output harmonic; they assume no harmonic truncation. -/

noncomputable section
namespace DSP.ResponseMeasurement
open LeanCert.Core DSP.IntervalArithmetic

private theorem carrier_parts (omega : ℝ) (n : ℕ) :
    (star (Complex.exp ((omega:ℂ)*Complex.I))^n).re = Real.cos ((n:ℝ)*omega) ∧
    (star (Complex.exp ((omega:ℂ)*Complex.I))^n).im = -Real.sin ((n:ℝ)*omega) := by
  have he : (n:ℂ)*((omega:ℂ)*Complex.I) = (((n:ℝ)*omega:ℝ):ℂ)*Complex.I := by push_cast; ring
  rw [← star_pow, ← Complex.exp_nat_mul, he]
  simp only [Complex.star_def, Complex.conj_re, Complex.conj_im,
    Complex.exp_ofReal_mul_I_re, Complex.exp_ofReal_mul_I_im, and_self]

theorem coherent_re_exp (omega : ℝ) (signal : ℕ → ℝ) (N : ℕ) :
    (coherent (Complex.exp ((omega:ℂ)*Complex.I)) signal N).re =
      2*(N:ℝ)⁻¹*∑ n ∈ Finset.range N, signal n*Real.cos ((n:ℝ)*omega) := by
  simp only [coherent, average, Complex.real_smul, Complex.mul_re, Complex.ofReal_re,
    Complex.ofReal_im, zero_mul, sub_zero, Complex.re_sum, Complex.re_ofNat,
    Complex.im_ofNat, (carrier_parts omega _).1]
  ring

theorem coherent_im_exp (omega : ℝ) (signal : ℕ → ℝ) (N : ℕ) :
    (coherent (Complex.exp ((omega:ℂ)*Complex.I)) signal N).im =
      -(2*(N:ℝ)⁻¹*∑ n ∈ Finset.range N, signal n*Real.sin ((n:ℝ)*omega)) := by
  simp only [coherent, average, Complex.real_smul, Complex.mul_im, Complex.ofReal_re,
    Complex.ofReal_im, zero_mul, add_zero, Complex.im_sum, Complex.re_ofNat,
    Complex.im_ofNat, (carrier_parts omega _).2, mul_neg, Finset.sum_neg_distrib]
  ring

def projectionBox {N : ℕ} (signal cosine : Fin N → IntervalRat) : IntervalRat :=
  IntervalRat.scale (2/(N:ℚ)) (sum (fun n => (signal n).mul (cosine n)))

def powerBox {N : ℕ} (signal : Fin N → IntervalRat) : IntervalRat :=
  IntervalRat.scale (1/(N:ℚ)) (sum (fun n => (signal n).mul (signal n)))

def coherentPowerBox (realPart imaginaryPart : IntervalRat) : IntervalRat :=
  IntervalRat.scale (1/2) ((realPart.mul realPart).add (imaginaryPart.mul imaginaryPart))

theorem coherent_parts_mem (omega : ℝ) (signal : ℕ → ℝ) (N : ℕ)
    (samples cosine sine : Fin N → IntervalRat)
    (hs : ∀ n, signal n ∈ samples n)
    (hc : ∀ n, Real.cos ((n:ℝ)*omega) ∈ cosine n)
    (hi : ∀ n, Real.sin ((n:ℝ)*omega) ∈ sine n) :
    (coherent (Complex.exp ((omega:ℂ)*Complex.I)) signal N).re ∈ projectionBox samples cosine ∧
    -(coherent (Complex.exp ((omega:ℂ)*Complex.I)) signal N).im ∈ projectionBox samples sine := by
  have hcos := IntervalRat.mem_scale (2/(N:ℚ)) (mem_sum (fun n => IntervalRat.mem_mul (hs n) (hc n)))
  have hsin := IntervalRat.mem_scale (2/(N:ℚ)) (mem_sum (fun n => IntervalRat.mem_mul (hs n) (hi n)))
  rw [coherent_re_exp, coherent_im_exp, neg_neg]
  constructor
  · simpa only [projectionBox, Rat.cast_div, Rat.cast_ofNat, Rat.cast_natCast, Rat.cast_mul, Rat.cast_inv,
      div_eq_mul_inv, ← Fin.sum_univ_eq_sum_range] using hcos
  · simpa only [projectionBox, Rat.cast_div, Rat.cast_ofNat, Rat.cast_natCast, Rat.cast_mul, Rat.cast_inv,
      div_eq_mul_inv, ← Fin.sum_univ_eq_sum_range] using hsin

theorem meanSquare_mem (signal : ℕ → ℝ) (N : ℕ) (samples : Fin N → IntervalRat)
    (hs : ∀ n, signal n ∈ samples n) : meanSquare signal N ∈ powerBox samples := by
  have h := IntervalRat.mem_scale (1/(N:ℚ)) (mem_sum (fun n => IntervalRat.mem_mul (hs n) (hs n)))
  simpa only [meanSquare, powerBox, Rat.cast_div, Rat.cast_one, Rat.cast_natCast, Rat.cast_inv,
    one_div, ← pow_two, ← Fin.sum_univ_eq_sum_range] using h

theorem coherentPower_mem (z : ℂ) (realPart imaginaryPart : IntervalRat)
    (hr : z.re ∈ realPart) (hi : -z.im ∈ imaginaryPart) :
    ‖z‖^2/2 ∈ coherentPowerBox realPart imaginaryPart := by
  have h := IntervalRat.mem_scale (1/2)
    (IntervalRat.mem_add (IntervalRat.mem_mul hr hr) (IntervalRat.mem_mul hi hi))
  rw [Complex.sq_norm]
  simpa only [coherentPowerBox, Complex.normSq_apply, Rat.cast_div, Rat.cast_one,
    Rat.cast_ofNat, Rat.cast_mul, Rat.cast_inv, one_mul, neg_mul_neg, div_eq_mul_inv, mul_comm (2:ℝ)⁻¹] using h

end DSP.ResponseMeasurement
