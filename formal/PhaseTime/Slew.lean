import Mathlib.Analysis.SpecialFunctions.Pow.Real
import Mathlib.Tactic.Linarith
import Mathlib.Tactic.Ring
import Mathlib.Analysis.Calculus.Deriv.Comp

namespace PhaseTime.Slew

/-! Ideal exponential one-pole over phase distance. k includes 2π and the
nominal LFO cutoff ratio. Partition invariance below is for a constant input;
arbitrary changing waveforms also have ordinary integration/sampling error.
Reversing travel does not restore a dissipative filter's prior state. -/

-- @cpp phase_slew: use the amount of phase traversed, not a fixed real-time coefficient.
noncomputable def decay (k delta : ℝ) : ℝ := Real.exp (-k * |delta|)
noncomputable def step (k delta value target : ℝ) : ℝ :=
  target + decay k delta * (value - target)

theorem decay_bounds (k delta : ℝ) (hk : 0 ≤ k) : 0 < decay k delta ∧ decay k delta ≤ 1 := by
  constructor
  · exact Real.exp_pos _
  · apply Real.exp_le_one_iff.mpr
    have := mul_nonneg hk (abs_nonneg delta)
    linarith

theorem pause_holds (k value target : ℝ) : step k 0 value target = value := by
  simp [step, decay]
theorem equal_distance_equal_slew (k delta value target : ℝ) :
    step k (-delta) value target = step k delta value target := by simp [step, decay]
theorem contraction (k delta value target : ℝ) :
    |step k delta value target - target| = decay k delta * |value - target| := by
  simp only [step, decay, add_sub_cancel_left, abs_mul, abs_of_pos (Real.exp_pos _)]

theorem bounded (k delta value target : ℝ) (hk : 0 ≤ k) :
    min value target ≤ step k delta value target ∧ step k delta value target ≤ max value target := by
  have hd := decay_bounds k delta hk
  unfold step
  rcases le_total value target with h | h
  · rw [min_eq_left h, max_eq_right h]; constructor <;> nlinarith
  · rw [min_eq_right h, max_eq_left h]; constructor <;> nlinarith

theorem subdivision (k a b value target : ℝ) (ha : 0 ≤ a) (hb : 0 ≤ b) :
    step k b (step k a value target) target = step k (a + b) value target := by
  have hexp : decay k (a + b) = decay k b * decay k a := by
    simp only [decay, abs_of_nonneg ha, abs_of_nonneg hb, abs_of_nonneg (add_nonneg ha hb)]
    rw [show -k * (a + b) = -k * b + -k * a by ring, Real.exp_add]
  simp only [step, hexp]
  ring

-- Same phase displacement gives the same step at any positive real-time duration.
theorem tempo_invariance (k distance dt value target : ℝ) (hdt : dt ≠ 0) :
    step k ((distance / dt) * dt) value target = step k distance value target := by
  rw [div_mul_cancel₀ distance hdt]

-- The continuous mathematical law: phase-domain smoothing becomes a real-time
-- slew scaled by phase velocity. This is not a claim of exact sampled integration.
theorem forward_chain_rule (L Z Q : ℝ → ℝ) (t k speed : ℝ) (hv : 0 ≤ speed)
    (hq : HasDerivAt Q speed t)
    (hz : HasDerivAt Z (k * (L (Q t) - Z (Q t))) (Q t)) :
    HasDerivAt (fun t => Z (Q t)) (k * |speed| * (L (Q t) - Z (Q t))) t := by
  rw [abs_of_nonneg hv]
  simpa only [Function.comp_def, mul_assoc, mul_left_comm, mul_comm] using hz.comp t hq

end PhaseTime.Slew
