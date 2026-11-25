import DSP.Filters.ButterworthDownsampler.Transfer
import Mathlib.Tactic.LinearCombination

noncomputable section

namespace DSP.ButterworthDownsampler

theorem factors2 (x : ℝ) :
    sectionPowerDenominator (0.25 * Real.pi) x = 1 + x ^ 4 := by
  unfold sectionPowerDenominator
  rw [show 2 * (0.25 * Real.pi) = Real.pi / 2 by ring, Real.cos_pi_div_two]
  ring

theorem factors4 (x : ℝ) :
    sectionPowerDenominator (0.125 * Real.pi) x *
      sectionPowerDenominator (0.375 * Real.pi) x = 1 + x ^ 8 := by
  unfold sectionPowerDenominator
  rw [show 2 * (0.125 * Real.pi) = Real.pi / 4 by ring,
    show 2 * (0.375 * Real.pi) = Real.pi - Real.pi / 4 by ring, Real.cos_pi_sub]
  have hcos : Real.cos (Real.pi / 4) ^ 2 = 1 / 2 := by
    rw [Real.cos_pi_div_four, div_pow, Real.sq_sqrt (by norm_num)]
    norm_num
  calc
    _ = (1 + x ^ 4) ^ 2 - 4 * Real.cos (Real.pi / 4) ^ 2 * x ^ 4 := by ring
    _ = _ := by rw [hcos]; ring

theorem factors8 (x : ℝ) :
    (sectionPowerDenominator (0.0625 * Real.pi) x *
      sectionPowerDenominator (0.1875 * Real.pi) x) *
      (sectionPowerDenominator (0.3125 * Real.pi) x *
        sectionPowerDenominator (0.4375 * Real.pi) x) = 1 + x ^ 16 := by
  unfold sectionPowerDenominator
  rw [show 2 * (0.0625 * Real.pi) = Real.pi / 8 by ring,
    show 2 * (0.1875 * Real.pi) = Real.pi / 2 - Real.pi / 8 by ring,
    show 2 * (0.3125 * Real.pi) = Real.pi - (Real.pi / 2 - Real.pi / 8) by ring,
    show 2 * (0.4375 * Real.pi) = Real.pi - Real.pi / 8 by ring]
  simp only [Real.cos_pi_sub, Real.cos_pi_div_two_sub]
  let a := Real.cos (Real.pi / 8)
  let b := Real.sin (Real.pi / 8)
  have hs : a ^ 2 + b ^ 2 = 1 := Real.cos_sq_add_sin_sq _
  have hd : Real.sin (Real.pi / 4) = 2 * b * a := by
    rw [show Real.pi / 4 = 2 * (Real.pi / 8) by ring, Real.sin_two_mul]
  have hsin : Real.sin (Real.pi / 4) ^ 2 = 1 / 2 := by
    rw [Real.sin_pi_div_four, div_pow, Real.sq_sqrt (by norm_num)]
    norm_num
  have hp : a ^ 2 * b ^ 2 = 1 / 8 := by
    rw [hd] at hsin
    linear_combination hsin / 4
  change ((1 + 2 * a * x ^ 2 + x ^ 4) * (1 + 2 * b * x ^ 2 + x ^ 4)) *
    ((1 + 2 * (-b) * x ^ 2 + x ^ 4) * (1 + 2 * (-a) * x ^ 2 + x ^ 4)) = 1 + x ^ 16
  calc
    _ = (1 + x ^ 4) ^ 4 - 4 * (a ^ 2 + b ^ 2) * x ^ 4 * (1 + x ^ 4) ^ 2 +
        16 * (a ^ 2 * b ^ 2) * x ^ 8 := by ring
    _ = _ := by rw [hs, hp]; ring

end DSP.ButterworthDownsampler
