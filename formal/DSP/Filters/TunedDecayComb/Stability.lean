import DSP.Filters.TunedDecayComb.Realization
import DSP.Stability.Bilinear
import Mathlib.Analysis.SpecialFunctions.Pow.Real

noncomputable section
namespace DSP.TunedDecayComb

/-- The peak-normalized prototype has a global right-half-plane gain bound.
This sum of squares also identifies the frequency where the bound is attained. -/
theorem analog_lowpass_gap (g k gain u : ℝ) (s : ℂ)
    (hk : k ^ 2 = 2 * (1 - u)) (hg : gain ^ 2 = 1 - u ^ 2) :
    Complex.normSq (s ^ 2 + (k * g : ℝ) * s + (g ^ 2 : ℝ)) - (gain * g ^ 2) ^ 2 =
      (s.re ^ 2 + k * g * s.re) ^ 2 +
      2 * (s.re ^ 2 + k * g * s.re) * (g ^ 2 + s.im ^ 2) +
      (s.im ^ 2 - g ^ 2 * u) ^ 2 := by
  simp [Complex.normSq_apply, pow_two]
  linear_combination g ^ 2 * s.im ^ 2 * hk - g ^ 4 * hg

theorem analog_lowpass_bound (g k gain u : ℝ) (s : ℂ)
    (hg : 0 < g) (hk : 0 < k) (hv : 0 < gain) (hs : 0 ≤ s.re)
    (hk2 : k ^ 2 = 2 * (1 - u)) (hv2 : gain ^ 2 = 1 - u ^ 2) :
    s ^ 2 + (k * g : ℝ) * s + (g ^ 2 : ℝ) ≠ 0 ∧
    Complex.normSq ((gain * g ^ 2 : ℝ) /
      (s ^ 2 + (k * g : ℝ) * s + (g ^ 2 : ℝ))) ≤ 1 := by
  have he := analog_lowpass_gap g k gain u s hk2 hv2
  have hn : 0 ≤ s.re ^ 2 + k * g * s.re := by positivity
  have hprod : 0 ≤ 2 * (s.re ^ 2 + k * g * s.re) * (g ^ 2 + s.im ^ 2) := by positivity
  have hnum : 0 < (gain * g ^ 2) ^ 2 := by positivity
  have hd : 0 < Complex.normSq (s ^ 2 + (k * g : ℝ) * s + (g ^ 2 : ℝ)) := by
    nlinarith [sq_nonneg (s.re ^ 2 + k * g * s.re), sq_nonneg (s.im ^ 2 - g ^ 2 * u)]
  refine ⟨Complex.normSq_pos.mp hd, ?_⟩
  rw [Complex.normSq_div, Complex.normSq_ofReal, div_le_one hd]
  nlinarith [sq_nonneg (s.re ^ 2 + k * g * s.re), sq_nonneg (s.im ^ 2 - g ^ 2 * u)]

def prototypeDenominator (g k : ℝ) (q : ℂ) : ℂ :=
  (1 - q) ^ 2 + (k * g : ℝ) * (1 - q ^ 2) + (g ^ 2 : ℝ) * (1 + q) ^ 2

theorem cayley_re (q : ℂ) :
    ((1 - q) / (1 + q)).re = (1 - Complex.normSq q) / Complex.normSq (1 + q) := by
  simp [Complex.div_re, Complex.normSq_apply]
  ring

theorem prototype_cayley (g k : ℝ) (q : ℂ) (hq : 1 + q ≠ 0) :
    ((1 - q) / (1 + q)) ^ 2 + (k * g : ℝ) * ((1 - q) / (1 + q)) + (g ^ 2 : ℝ) =
      prototypeDenominator g k q / (1 + q) ^ 2 := by
  unfold prototypeDenominator
  field_simp
  ring

theorem prototype_lowpass_bound (g k gain u : ℝ) (q : ℂ)
    (hg : 0 < g) (hk : 0 < k) (hv : 0 < gain) (hq : Complex.normSq q ≤ 1)
    (hk2 : k ^ 2 = 2 * (1 - u)) (hv2 : gain ^ 2 = 1 - u ^ 2) :
    prototypeDenominator g k q ≠ 0 ∧
    Complex.normSq ((gain * g ^ 2 : ℝ) * (1 + q) ^ 2 / prototypeDenominator g k q) ≤ 1 := by
  by_cases hminus : 1 + q = 0
  · have hq' : q = -1 := by linear_combination hminus
    subst q
    norm_num [prototypeDenominator]
  have hs : 0 ≤ ((1 - q) / (1 + q)).re := by
    rw [cayley_re]
    exact div_nonneg (sub_nonneg.mpr hq) (Complex.normSq_nonneg _)
  obtain ⟨hd, hb⟩ := analog_lowpass_bound g k gain u ((1 - q) / (1 + q)) hg hk hv hs hk2 hv2
  rw [prototype_cayley g k q hminus] at hd hb
  have hd' : prototypeDenominator g k q ≠ 0 := by
    intro h
    exact hd (by rw [h, zero_div])
  refine ⟨hd', ?_⟩
  rw [div_div_eq_mul_div] at hb
  exact hb

theorem allpass_disk_bound (a : ℝ) (q : ℂ) (ha : |a| < 1)
    (hq : Complex.normSq q ≤ 1) :
    1 + (a : ℂ) * q ≠ 0 ∧ Complex.normSq (NormalizedAllPass.response (a : ℂ) 1 q) ≤ 1 := by
  have ha2 := (sq_lt_one_iff_abs_lt_one a).mpr ha
  have hgap : Complex.normSq (1 + (a : ℂ) * q) - Complex.normSq ((a : ℂ) + q) =
      (1 - a ^ 2) * (1 - Complex.normSq q) := by
    simp [Complex.normSq_apply]
    ring
  have hd : 1 + (a : ℂ) * q ≠ 0 := by
    intro he
    have he' : (a : ℂ) * q = -1 := by linear_combination he
    have hn := congrArg Complex.normSq he'
    norm_num [map_mul, Complex.normSq_ofReal] at hn
    have hm := mul_le_mul_of_nonneg_left hq (sq_nonneg a)
    nlinarith
  refine ⟨hd, ?_⟩
  simp only [NormalizedAllPass.response, mul_one, one_mul, Complex.normSq_div]
  apply (div_le_one (Complex.normSq_pos.mpr hd)).mpr
  nlinarith [mul_nonneg (by linarith : 0 ≤ 1 - a ^ 2) (sub_nonneg.mpr hq)]

/-- Explicit preparation contracts used by the pole theorem, including the
peak normalization which the generic sample core alone cannot guarantee. -/
def PeakNormalized {capacity : ℕ} (c : Coefficients capacity ℝ) : Prop :=
  0 < c.g ∧ 0 < c.k ∧ 0 < c.gain ∧
    ∃ u, c.k ^ 2 = 2 * (1 - u) ∧ c.gain ^ 2 = 1 - u ^ 2

variable {capacity : ℕ}

theorem complex_lowpass_denominator (c : Coefficients capacity ℝ) (q : ℂ) :
    lowpassDenominator (c.map Complex.ofReal) q = prototypeDenominator c.g c.k ((c.r : ℂ) * q) := by
  simp [lowpassDenominator, Coefficients.map, prototypeDenominator]
  ring

theorem lowpass_disk_bound (c : Coefficients capacity ℝ) (q : ℂ)
    (hc : PeakNormalized c) (hq : Complex.normSq ((c.r : ℂ) * q) ≤ 1) :
    (c.bypass = false → lowpassDenominator (c.map Complex.ofReal) q ≠ 0) ∧
      ‖lowpassResponse (c.map Complex.ofReal) q‖ ≤ 1 := by
  obtain ⟨hg, hk, hv, u, hk2, hv2⟩ := hc
  obtain ⟨hd, hb⟩ := prototype_lowpass_bound c.g c.k c.gain u ((c.r : ℂ) * q) hg hk hv hq hk2 hv2
  refine ⟨fun _ => by rwa [complex_lowpass_denominator], ?_⟩
  cases hby : c.bypass with
  | true => simp [lowpassResponse, Coefficients.map, hby]
  | false =>
    have he : lowpassResponse (c.map Complex.ofReal) q =
        (c.gain * c.g ^ 2 : ℝ) * (1 + (c.r : ℂ) * q) ^ 2 /
          prototypeDenominator c.g c.k ((c.r : ℂ) * q) := by
      unfold lowpassResponse
      rw [complex_lowpass_denominator]
      simp [Coefficients.map, hby]
    rw [he]
    rw [Complex.normSq_eq_norm_sq] at hb
    nlinarith [norm_nonneg ((c.gain * c.g ^ 2 : ℝ) * (1 + (c.r : ℂ) * q) ^ 2 /
      prototypeDenominator c.g c.k ((c.r : ℂ) * q))]

/-- Strict contractivity inside the prototype disk comes from the positive
integer delay, even when the lowpass and allpass attain unit boundary gain. -/
theorem return_norm_lt_one (c : Coefficients capacity ℝ) (q : ℂ)
    (hc : PeakNormalized c) (ha : |c.a| < 1) (hs : |c.sign| ≤ 1)
    (hr : c.delayGain = c.r ^ (c.age.val + 1)) (hq : ‖(c.r : ℂ) * q‖ < 1) :
    Regular (c.map Complex.ofReal) q ∧ ‖returnResponse (c.map Complex.ofReal) q‖ < 1 := by
  have hq2 : Complex.normSq ((c.r : ℂ) * q) ≤ 1 := by
    rw [Complex.normSq_eq_norm_sq]
    nlinarith [norm_nonneg ((c.r : ℂ) * q)]
  obtain ⟨haD, haB⟩ := allpass_disk_bound c.a ((c.r : ℂ) * q) ha hq2
  obtain ⟨hlD, hlB⟩ := lowpass_disk_bound c q hc hq2
  have hap : ‖NormalizedAllPass.response (c.a : ℂ) c.r q‖ ≤ 1 := by
    have he : NormalizedAllPass.response (c.a : ℂ) c.r q =
        NormalizedAllPass.response (c.a : ℂ) 1 ((c.r : ℂ) * q) := by
      simp [NormalizedAllPass.response, mul_assoc]
    rw [he]
    rw [Complex.normSq_eq_norm_sq] at haB
    nlinarith [norm_nonneg (NormalizedAllPass.response (c.a : ℂ) 1 ((c.r : ℂ) * q))]
  have hsign : ‖(c.sign : ℂ)‖ ≤ 1 := by simpa using hs
  refine ⟨⟨?_, hlD⟩, ?_⟩
  · simpa [Coefficients.map, mul_assoc] using haD
  · have hrad : (c.map Complex.ofReal).delayGain =
        (c.map Complex.ofReal).r ^ ((c.map Complex.ofReal).age.val + 1) := by
      simpa [Coefficients.map] using congrArg Complex.ofReal hr
    rw [radial_return _ _ hrad, norm_mul, norm_mul, norm_mul, norm_pow]
    have hprod : ‖lowpassResponse (c.map Complex.ofReal) q‖ * ‖(c.sign : ℂ)‖ *
        ‖NormalizedAllPass.response (c.a : ℂ) c.r q‖ ≤ 1 := by
      calc
        _ ≤ 1 * 1 * 1 := mul_le_mul (mul_le_mul hlB hsign (norm_nonneg _) (by norm_num))
          hap (norm_nonneg _) (by norm_num)
        _ = 1 := by ring
    calc
      _ ≤ 1 * ‖(c.r : ℂ) * q‖ ^ (c.age.val + 1) :=
        mul_le_mul_of_nonneg_right hprod (pow_nonneg (norm_nonneg _) _)
      _ < 1 := by simpa using pow_lt_one₀ (norm_nonneg ((c.r : ℂ) * q)) hq (by omega : c.age.val + 1 ≠ 0)

theorem settled_denominator_ne_zero (c : Coefficients capacity ℝ) (q : ℂ)
    (hc : PeakNormalized c) (ha : |c.a| < 1) (hs : |c.sign| ≤ 1)
    (hr : c.delayGain = c.r ^ (c.age.val + 1)) (hq : ‖(c.r : ℂ) * q‖ < 1) :
    1 - returnResponse (c.map Complex.ofReal) q ≠ 0 := by
  have h := (return_norm_lt_one c q hc ha hs hr hq).2
  intro he
  have hr1 : returnResponse (c.map Complex.ofReal) q = 1 := by linear_combination -he
  rw [hr1, norm_one] at h
  exact lt_irrefl _ h

/-- A frozen crossfade is also stable outside both branch radii. This does not
identify a moving crossfade with a single stationary transfer function. -/
theorem mixed_denominator_ne_zero (c₀ c₁ : Coefficients capacity ℝ) (m : ℝ) (q : ℂ)
    (hm : 0 ≤ m ∧ m ≤ 1) (hc₀ : PeakNormalized c₀) (hc₁ : PeakNormalized c₁)
    (ha₀ : |c₀.a| < 1) (ha₁ : |c₁.a| < 1) (hs₀ : |c₀.sign| ≤ 1) (hs₁ : |c₁.sign| ≤ 1)
    (hr₀ : c₀.delayGain = c₀.r ^ (c₀.age.val + 1))
    (hr₁ : c₁.delayGain = c₁.r ^ (c₁.age.val + 1))
    (hq₀ : ‖(c₀.r : ℂ) * q‖ < 1) (hq₁ : ‖(c₁.r : ℂ) * q‖ < 1) :
    1 - ((1 - (m : ℂ)) * returnResponse (c₀.map Complex.ofReal) q +
      (m : ℂ) * returnResponse (c₁.map Complex.ofReal) q) ≠ 0 := by
  have h₀ := (return_norm_lt_one c₀ q hc₀ ha₀ hs₀ hr₀ hq₀).2
  have h₁ := (return_norm_lt_one c₁ q hc₁ ha₁ hs₁ hr₁ hq₁).2
  have hw₀ : ‖1 - (m : ℂ)‖ = 1 - m := by
    rw [← Complex.ofReal_one, ← Complex.ofReal_sub, Complex.norm_real, Real.norm_eq_abs,
      abs_of_nonneg (by linarith)]
  have hw₁ : ‖(m : ℂ)‖ = m := by simpa using abs_of_nonneg hm.1
  have hnorm := norm_add_le ((1 - (m : ℂ)) * returnResponse (c₀.map Complex.ofReal) q)
    ((m : ℂ) * returnResponse (c₁.map Complex.ofReal) q)
  rw [norm_mul, norm_mul, hw₀, hw₁] at hnorm
  have hlt : (1 - m) * ‖returnResponse (c₀.map Complex.ofReal) q‖ +
      m * ‖returnResponse (c₁.map Complex.ofReal) q‖ < 1 := by
    have hmax : max ‖returnResponse (c₀.map Complex.ofReal) q‖
        ‖returnResponse (c₁.map Complex.ofReal) q‖ < 1 := max_lt h₀ h₁
    have h0 := mul_le_mul_of_nonneg_left (le_max_left
      ‖returnResponse (c₀.map Complex.ofReal) q‖ ‖returnResponse (c₁.map Complex.ofReal) q‖) (by linarith : 0 ≤ 1 - m)
    have h1 := mul_le_mul_of_nonneg_left (le_max_right
      ‖returnResponse (c₀.map Complex.ofReal) q‖ ‖returnResponse (c₁.map Complex.ofReal) q‖) hm.1
    nlinarith
  intro he
  have he' : (1 - (m : ℂ)) * returnResponse (c₀.map Complex.ofReal) q +
      (m : ℂ) * returnResponse (c₁.map Complex.ofReal) q = 1 := by linear_combination -he
  rw [he', norm_one] at hnorm
  linarith

/-- Cleared polynomial denominator in the delay variable. Keeping the internal
section factors makes the pole bound valid before any numerator cancellations. -/
def closedDenominator {F : Type} [Field F] (c : Coefficients capacity F) (q : F) : F :=
  (1 + c.a * c.r * q) * (if c.bypass then 1 else lowpassDenominator c q) -
  c.sign * c.delayGain * q ^ (c.age.val + 1) * (c.a + c.r * q) *
    (if c.bypass then 1 else c.gain * c.g ^ 2 * (1 + c.r * q) ^ 2)

theorem closedDenominator_factor {F : Type} [Field F] (c : Coefficients capacity F) (q : F)
    (hr : Regular c q) :
    closedDenominator c q = (1 + c.a * c.r * q) *
      (if c.bypass then 1 else lowpassDenominator c q) * (1 - returnResponse c q) := by
  obtain ⟨ha, hl⟩ := hr
  cases hb : c.bypass with
  | true =>
    simp only [closedDenominator, returnResponse, lowpassResponse, hb, if_true,
      NormalizedAllPass.response]
    field_simp
  | false =>
    have hd := hl hb
    simp only [closedDenominator, returnResponse, lowpassResponse, hb, Bool.false_eq_true,
      if_false, NormalizedAllPass.response]
    field_simp

theorem closedDenominator_ne_zero (c : Coefficients capacity ℝ) (q : ℂ)
    (hc : PeakNormalized c) (ha : |c.a| < 1) (hs : |c.sign| ≤ 1)
    (hr : c.delayGain = c.r ^ (c.age.val + 1)) (hq : ‖(c.r : ℂ) * q‖ < 1) :
    closedDenominator (c.map Complex.ofReal) q ≠ 0 := by
  have hreg := (return_norm_lt_one c q hc ha hs hr hq).1
  rw [closedDenominator_factor _ _ hreg]
  apply mul_ne_zero (mul_ne_zero hreg.1 ?_) (settled_denominator_ne_zero c q hc ha hs hr hq)
  cases hb : c.bypass with
  | true => simp [Coefficients.map, hb]
  | false => simpa [Coefficients.map, hb] using hreg.2 hb

/-- Every stationary pole of the cleared transfer has radius at most r.
The zero-frequency-variable singularity z=0 is harmless for this bound. -/
theorem pole_radius_bound (c : Coefficients capacity ℝ) (z : ℂ)
    (hc : PeakNormalized c) (ha : |c.a| < 1) (hs : |c.sign| ≤ 1) (hr0 : 0 ≤ c.r)
    (hr : c.delayGain = c.r ^ (c.age.val + 1))
    (hp : closedDenominator (c.map Complex.ofReal) z⁻¹ = 0) : ‖z‖ ≤ c.r := by
  by_contra hn
  have hz : c.r < ‖z‖ := lt_of_not_ge hn
  have hz0 : 0 < ‖z‖ := lt_of_le_of_lt hr0 hz
  have hq : ‖(c.r : ℂ) * z⁻¹‖ < 1 := by
    rw [norm_mul, norm_inv, Complex.norm_real, Real.norm_eq_abs, abs_of_nonneg hr0,
      ← div_eq_mul_inv]
    exact (div_lt_one hz0).mpr hz
  exact closedDenominator_ne_zero c z⁻¹ hc ha hs hr hq hp

end DSP.TunedDecayComb
