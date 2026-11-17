import DSP.Filters.Tuning
import DSP.Transcendental.Real
import DSP.Filters.Responses.Channels

noncomputable section
namespace DSP
namespace Tuning

/-- Open digital tuning interval. Frequencies at DC/Nyquist are response
queries, not valid finite TPT natural-frequency coefficient specifications. -/
def ValidCutoff (cutoff : ℝ) : Prop := 0 < cutoff ∧ cutoff < 1 / 2

def warp (cutoff : ℝ) : ℝ := Real.tan (Real.pi * cutoff)

theorem warp_pos (f : ℝ) (h : ValidCutoff f) : 0 < warp f := by
  apply Real.tan_pos_of_pos_of_lt_pi_div_two
  · exact mul_pos Real.pi_pos h.1
  · nlinarith [Real.pi_pos, h.2]

theorem cos_pos (f : ℝ) (h : ValidCutoff f) : 0 < Real.cos (Real.pi * f) := by
  apply Real.cos_pos_of_mem_Ioo
  constructor <;> nlinarith [Real.pi_pos, h.1, h.2]

theorem warp_mul_cos (f : ℝ) (h : ValidCutoff f) :
    warp f * Real.cos (Real.pi * f) = Real.sin (Real.pi * f) := by
  rw [warp, Real.tan_eq_sin_div_cos, div_mul_cancel₀ _ (ne_of_gt (cos_pos f h))]

theorem butterworth_q : (Trig.cos2pi (1 / 8) : ℝ) = Real.sqrt 2 / 2 := by
  rw [cos2pi_real, show 2 * Real.pi * (1 / 8) = Real.pi / 4 by ring, Real.cos_pi_div_four]

theorem butterworth_q_pos : 0 < (Trig.cos2pi (1 / 8) : ℝ) := by
  rw [butterworth_q]
  positivity

theorem butterworth_q_squared : (Trig.cos2pi (1 / 8) : ℝ)^2 = 1/2 := by
  rw [butterworth_q]
  nlinarith [Real.sq_sqrt (show (0:ℝ) ≤ 2 by norm_num)]

end Tuning

namespace OnePoleZOHLowPass
def LogicalSpec.Valid (spec : LogicalSpec ℝ) : Prop := Tuning.ValidCutoff spec.cutoff

theorem computeCoefficients_bounds (spec : LogicalSpec ℝ) (state : OnePoleZOHLowPass ℝ)
    (h : spec.Valid) :
    0 < (computeCoefficients spec state).alpha ∧ (computeCoefficients spec state).alpha < 1 := by
  simp only [computeCoefficients, expDecay_real]
  have hlt : Real.exp (-(2 * Real.pi * spec.cutoff)) < 1 :=
    Real.exp_lt_one_iff.mpr (by nlinarith [Real.pi_pos, h.1])
  have hpos := Real.exp_pos (-(2 * Real.pi * spec.cutoff))
  constructor <;> linarith
end OnePoleZOHLowPass

namespace OnePoleTPTLowPass
def LogicalSpec.Valid (spec : LogicalSpec ℝ) : Prop := Tuning.ValidCutoff spec.cutoff

theorem computeCoefficients_tuned (spec : LogicalSpec ℝ) (state : OnePoleTPTLowPass ℝ)
    (h : spec.Valid) :
    (computeCoefficients spec state).G * (1 + Tuning.warp spec.cutoff) = Tuning.warp spec.cutoff := by
  have hd : 1 + Tuning.warp spec.cutoff ≠ 0 := by linarith [Tuning.warp_pos spec.cutoff h]
  change Tuning.warp spec.cutoff / (1 + Tuning.warp spec.cutoff) * (1 + Tuning.warp spec.cutoff) = _
  exact div_mul_cancel₀ _ hd
end OnePoleTPTLowPass

namespace TPTSVF
def LogicalSpec.Valid (spec : LogicalSpec ℝ) : Prop :=
  Tuning.ValidCutoff spec.cutoff ∧ 0 < spec.q

theorem computeCoefficients_tuned (spec : LogicalSpec ℝ) (state : TPTSVF ℝ)
    (h : spec.Valid) : (computeCoefficients spec state).Tuned (Tuning.warp spec.cutoff) := by
  have hg := Tuning.warp_pos spec.cutoff h.1
  have hk : 0 < 1 / spec.q := one_div_pos.mpr h.2
  have hd : Tuning.warp spec.cutoff * (Tuning.warp spec.cutoff + 1 / spec.q) + 1 ≠ 0 := by positivity
  constructor
  · simp only [computeCoefficients, tanPi_real, fma_eq]
    change 1 / (Tuning.warp spec.cutoff * (Tuning.warp spec.cutoff + 1 / spec.q) + 1) *
      (1 + Tuning.warp spec.cutoff * (Tuning.warp spec.cutoff + 1 / spec.q)) = 1
    rw [add_comm 1, one_div_mul_cancel hd]
  · rfl
  · rfl
end TPTSVF
end DSP
