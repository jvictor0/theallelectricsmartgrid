import DSP.Filters.ButterworthDownsampler.Factors
import DSP.Filters.ButterworthDownsampler.SectionResponse

noncomputable section

namespace DSP.ButterworthDownsampler

private theorem cos_of_fraction_pos (fraction : ℝ) (h₀ : 0 ≤ fraction) (h₁ : fraction < 1 / 2) :
    0 < Real.cos (fraction * Real.pi) := by
  apply Real.cos_pos_of_mem_Ioo
  exact ⟨by nlinarith [Real.pi_pos], by nlinarith [Real.pi_pos]⟩

private theorem prepared_section_power (cutoff frequency fraction : ℝ)
    (hc₀ : 0 < cutoff) (hc₁ : cutoff < 1 / 2) (hf₀ : 0 ≤ frequency) (hf₁ : frequency < 1 / 2)
    (ha₀ : 0 ≤ fraction) (ha₁ : fraction < 1 / 2) :
    (prepareSection (Tuning.warp cutoff) (2 * Real.cos (fraction * Real.pi))).transfer.power frequency =
      1 / sectionPowerDenominator (fraction * Real.pi) (Tuning.warp frequency / Tuning.warp cutoff) :=
  section_power _ _ _ (Tuning.warp_pos cutoff ⟨hc₀, hc₁⟩) (cos_of_fraction_pos fraction ha₀ ha₁) hf₀ hf₁

theorem power2 (cutoff frequency : ℝ) (hc₀ : 0 < cutoff) (hc₁ : cutoff < 1 / 2)
    (hf₀ : 0 ≤ frequency) (hf₁ : frequency < 1 / 2) :
    (transfer2 cutoff).power frequency = 1 / (1 + (Tuning.warp frequency / Tuning.warp cutoff) ^ 4) := by
  change (prepareSection (Tuning.warp cutoff) (2 * Real.cos (0.25 * Real.pi))).transfer.power frequency = _
  rw [prepared_section_power cutoff frequency 0.25 hc₀ hc₁ hf₀ hf₁ (by norm_num) (by norm_num), factors2]

theorem power4 (cutoff frequency : ℝ) (hc₀ : 0 < cutoff) (hc₁ : cutoff < 1 / 2)
    (hf₀ : 0 ≤ frequency) (hf₁ : frequency < 1 / 2) :
    (transfer4 cutoff).power frequency = 1 / (1 + (Tuning.warp frequency / Tuning.warp cutoff) ^ 8) := by
  change ((prepareSection (Tuning.warp cutoff) (2 * Real.cos (0.125 * Real.pi))).transfer *
    (prepareSection (Tuning.warp cutoff) (2 * Real.cos (0.375 * Real.pi))).transfer).power frequency = _
  have atAngle (fraction : ℝ) (h₀ : 0 ≤ fraction) (h₁ : fraction < 1 / 2) :=
    prepared_section_power cutoff frequency fraction hc₀ hc₁ hf₀ hf₁ h₀ h₁
  rw [Transfer.power_mul, atAngle 0.125 (by norm_num) (by norm_num),
    atAngle 0.375 (by norm_num) (by norm_num), one_div_mul_one_div, factors4]

theorem power8 (cutoff frequency : ℝ) (hc₀ : 0 < cutoff) (hc₁ : cutoff < 1 / 2)
    (hf₀ : 0 ≤ frequency) (hf₁ : frequency < 1 / 2) :
    (transfer8 cutoff).power frequency = 1 / (1 + (Tuning.warp frequency / Tuning.warp cutoff) ^ 16) := by
  change (((prepareSection (Tuning.warp cutoff) (2 * Real.cos (0.0625 * Real.pi))).transfer *
    (prepareSection (Tuning.warp cutoff) (2 * Real.cos (0.1875 * Real.pi))).transfer) *
    ((prepareSection (Tuning.warp cutoff) (2 * Real.cos (0.3125 * Real.pi))).transfer *
    (prepareSection (Tuning.warp cutoff) (2 * Real.cos (0.4375 * Real.pi))).transfer)).power frequency = _
  have atAngle (fraction : ℝ) (h₀ : 0 ≤ fraction) (h₁ : fraction < 1 / 2) :=
    prepared_section_power cutoff frequency fraction hc₀ hc₁ hf₀ hf₁ h₀ h₁
  simp only [Transfer.power_mul, atAngle 0.0625 (by norm_num) (by norm_num),
    atAngle 0.1875 (by norm_num) (by norm_num), atAngle 0.3125 (by norm_num) (by norm_num),
    atAngle 0.4375 (by norm_num) (by norm_num), one_div_mul_one_div, factors8]

/-- The cutoff supplied to coefficient preparation is the half-power point
for each realized cascade, with cutoff in cycles per processing sample. -/
theorem power_at_cutoff (cutoff : ℝ) (h₀ : 0 < cutoff) (h₁ : cutoff < 1 / 2) :
    (transfer2 cutoff).power cutoff = 1 / 2 ∧
      (transfer4 cutoff).power cutoff = 1 / 2 ∧ (transfer8 cutoff).power cutoff = 1 / 2 := by
  have h := Tuning.warp_pos cutoff ⟨h₀, h₁⟩
  simp only [power2 cutoff cutoff h₀ h₁ h₀.le h₁, power4 cutoff cutoff h₀ h₁ h₀.le h₁,
    power8 cutoff cutoff h₀ h₁ h₀.le h₁, div_self h.ne', one_pow]
  norm_num

theorem power_at_dc (cutoff : ℝ) (h₀ : 0 < cutoff) (h₁ : cutoff < 1 / 2) :
    (transfer2 cutoff).power 0 = 1 ∧ (transfer4 cutoff).power 0 = 1 ∧ (transfer8 cutoff).power 0 = 1 := by
  have h : (0 : ℝ) < 1 / 2 := by norm_num
  norm_num [power2 cutoff 0 h₀ h₁ le_rfl h, power4 cutoff 0 h₀ h₁ le_rfl h,
    power8 cutoff 0 h₀ h₁ le_rfl h, Tuning.warp]

theorem power_at_nyquist (cutoff : ℝ) (h₀ : 0 < cutoff) (h₁ : cutoff < 1 / 2) :
    (transfer2 cutoff).power (1 / 2) = 0 ∧
      (transfer4 cutoff).power (1 / 2) = 0 ∧ (transfer8 cutoff).power (1 / 2) = 0 := by
  have phase : 2 * (Real.pi * (1 / 2)) = Real.pi := by ring
  have delay : ResponseFrequency.unitDelay (1 / 2) = -1 := by
    apply Complex.ext
    · rw [ResponseFrequency.unitDelay_re, phase]
      simp
    · rw [ResponseFrequency.unitDelay_im, phase]
      simp
  have sectionAtNyquist (fraction : ℝ) (ha₀ : 0 ≤ fraction) (ha₁ : fraction < 1 / 2) :
      (prepareSection (Real.tan (Real.pi * cutoff))
        (2 * Real.cos (fraction * Real.pi))).transfer.power (1 / 2) = 0 := by
    have hk := mul_pos (by norm_num : 0 < (2 : ℝ)) (cos_of_fraction_pos fraction ha₀ ha₁)
    rw [Transfer.power, show Real.tan (Real.pi * cutoff) = Tuning.warp cutoff from rfl,
      section_response _ _ _ (Tuning.warp_pos cutoff ⟨h₀, h₁⟩) hk, delay]
    simp [Response.svf]
  simp only [transfer2, transfer4, transfer8, Cascade.transfer, tuned2, tuned4, tuned8,
    prepare2, prepare4, prepare8, Transfer.power_mul,
    sectionAtNyquist 0.25 (by norm_num) (by norm_num), sectionAtNyquist 0.125 (by norm_num) (by norm_num),
    sectionAtNyquist 0.0625 (by norm_num) (by norm_num), zero_mul, and_self]

end DSP.ButterworthDownsampler
