import DSP.Filters.SchroederAllPass.Proofs
import Mathlib.Analysis.SpecialFunctions.Trigonometric.Bounds

namespace DSP.ModulatedMode

noncomputable section

/-- Frozen, scalar small-signal power relative to the same mode at resonance.
`retention` is the amplitude retained per round trip. -/
def powerRatio (retention : ℝ) (phase : ℂ) : ℝ :=
  (1 - retention) ^ 2 / Complex.normSq (1 - (retention : ℂ) * phase)

theorem denominator_eq (retention : ℝ) {phase : ℂ} (unit : Complex.normSq phase = 1) :
    Complex.normSq (1 - (retention : ℂ) * phase) =
      (1 - retention) ^ 2 + retention * Complex.normSq (1 - phase) := by
  simp only [Complex.normSq_apply, Complex.sub_re, Complex.sub_im, Complex.one_re,
    Complex.one_im, Complex.mul_re, Complex.mul_im, Complex.ofReal_re,
    Complex.ofReal_im, zero_mul, sub_zero] at unit ⊢
  linear_combination retention * (retention - 1) * unit

/-- A power target can be read directly as a required round-trip phase
displacement. At half power the required squared chord is (1-ρ)²/ρ. -/
theorem powerRatio_le_iff {retention target : ℝ} {phase : ℂ} (hρ : 0 ≤ retention)
    (hρ' : retention < 1) (unit : Complex.normSq phase = 1) :
    powerRatio retention phase ≤ target ↔
      (1 - target) * (1 - retention) ^ 2 ≤ target * retention * Complex.normSq (1 - phase) := by
  have denom : 0 < (1 - retention) ^ 2 + retention * Complex.normSq (1 - phase) :=
    add_pos_of_pos_of_nonneg (pow_pos (sub_pos.mpr hρ') 2)
      (mul_nonneg hρ (Complex.normSq_nonneg _))
  rw [powerRatio, denominator_eq retention unit, div_le_iff₀ denom]
  constructor <;> intro h <;> linarith only [h]

/-- Driven scalar mode in the rotating frame, with one supplied phase per
round trip. The drive has unit amplitude and the mode starts at rest. -/
def amplitude (retention : ℝ) (phase : Nat → ℂ) : Nat → ℂ
  | 0 => 0
  | n + 1 => 1 + (retention : ℂ) * phase n * amplitude retention phase n

/-- Aligned round trips give the geometric buildup. Any phase trajectory has
at most this much buildup, with equality possible for an aligned trajectory. -/
theorem amplitude_le_geometric {retention : ℝ} (hρ : 0 ≤ retention) {phase : Nat → ℂ}
    (unit : ∀ n, ‖phase n‖ = 1) (n : Nat) :
    ‖amplitude retention phase n‖ ≤ ∑ k ∈ Finset.range n, retention ^ k := by
  induction n with
  | zero => simp [amplitude]
  | succ n ih =>
    rw [amplitude, Finset.sum_range_succ']
    simp only [pow_zero, pow_succ', ← Finset.mul_sum, add_comm _ (1 : ℝ)]
    calc
      _ ≤ ‖(1 : ℂ)‖ + ‖(retention : ℂ) * phase n * amplitude retention phase n‖ :=
        norm_add_le _ _
      _ = 1 + retention * ‖amplitude retention phase n‖ := by
        simp [unit, Real.norm_eq_abs, abs_of_nonneg hρ]
      _ ≤ _ := add_le_add_right (mul_le_mul_of_nonneg_left ih hρ) _

theorem amplitude_aligned (retention : ℝ) (n : Nat) :
    amplitude retention (fun _ => 1) n = ((∑ k ∈ Finset.range n, retention ^ k : ℝ) : ℂ) := by
  induction n with
  | zero => simp [amplitude]
  | succ n ih =>
    rw [amplitude, ih, Finset.sum_range_succ']
    simp only [pow_zero, pow_succ', ← Finset.mul_sum, Complex.ofReal_add,
      Complex.ofReal_mul, Complex.ofReal_one, mul_one]
    exact add_comm _ _

/-- The delay phasor for an ideal fractional read at a digital frequency. -/
def delayPhasor (frequency delay : ℝ) : ℂ :=
  Complex.exp (Complex.I * ((-(2 * Real.pi * frequency * delay) : ℝ) : ℂ))

theorem delayPhasor_norm (frequency delay : ℝ) : ‖delayPhasor frequency delay‖ = 1 := by
  simp [delayPhasor, Complex.norm_exp, Complex.mul_re]

theorem norm_delayPhasor_sub_le (frequency before after : ℝ) :
    ‖delayPhasor frequency after - delayPhasor frequency before‖ ≤
      2 * Real.pi * |frequency| * |after - before| := by
  let a := -(2 * Real.pi * frequency * after)
  let b := -(2 * Real.pi * frequency * before)
  have factor : delayPhasor frequency after - delayPhasor frequency before =
      (Complex.exp (Complex.I * ((a - b : ℝ) : ℂ)) - 1) * delayPhasor frequency before := by
    have angle : Complex.I * ((a - b : ℝ) : ℂ) + Complex.I * (b : ℂ) =
        Complex.I * (a : ℂ) := by push_cast; ring
    rw [sub_mul, one_mul]
    change Complex.exp (Complex.I * (a : ℂ)) - Complex.exp (Complex.I * (b : ℂ)) =
      Complex.exp (Complex.I * ((a - b : ℝ) : ℂ)) * Complex.exp (Complex.I * (b : ℂ)) -
        Complex.exp (Complex.I * (b : ℂ))
    rw [← Complex.exp_add, angle]
  rw [factor, norm_mul, delayPhasor_norm, mul_one]
  calc
    _ ≤ ‖a - b‖ := Real.norm_exp_I_mul_ofReal_sub_one_le
    _ = _ := by
      rw [Real.norm_eq_abs, show a - b = -(2 * Real.pi * frequency) * (after - before) by
        dsimp [a, b]; ring, abs_mul, abs_neg, abs_mul,
        abs_of_nonneg (by positivity : 0 ≤ 2 * Real.pi)]

/-- The frozen Schroeder response with an ideal, possibly fractional delay. -/
def frozenAllPass (gain frequency delay : ℝ) : ℂ :=
  SchroederAllPass.response (gain : ℂ) (delayPhasor frequency delay) 1

theorem frozenAllPass_normSq {gain frequency delay : ℝ} (hg : |gain| < 1) :
    Complex.normSq (frozenAllPass gain frequency delay) = 1 := by
  apply SchroederAllPass.response_unit _ _ _ hg
  simp [Complex.normSq_eq_norm_sq, delayPhasor_norm]

/-- Change of the complex frozen response as a tap wanders. The coefficient
is four for the reverb's 0.6 Schroeder gain. -/
theorem norm_frozenAllPass_sub_le {gain frequency before after : ℝ} (hg : |gain| < 1) :
    ‖frozenAllPass gain frequency after - frozenAllPass gain frequency before‖ ≤
      (1 + |gain|) / (1 - |gain|) * (2 * Real.pi * |frequency| * |after - before|) := by
  let z := delayPhasor frequency after
  let w := delayPhasor frequency before
  let g : ℂ := gain
  have positive := sub_pos.mpr hg
  have lower {p : ℂ} (hp : ‖p‖ = 1) : 1 - |gain| ≤ ‖1 - g * p‖ := by
    have h := norm_sub_norm_le (1 : ℂ) (g * p)
    simpa [g, norm_mul, hp, Real.norm_eq_abs] using h
  have hz := lower (delayPhasor_norm frequency after)
  have hw := lower (delayPhasor_norm frequency before)
  have hz' : 1 - g * z ≠ 0 := norm_pos_iff.mp (positive.trans_le hz)
  have hw' : 1 - g * w ≠ 0 := norm_pos_iff.mp (positive.trans_le hw)
  have identity : frozenAllPass gain frequency after - frozenAllPass gain frequency before =
      ((1 - g ^ 2) * (z - w)) / ((1 - g * z) * (1 - g * w)) := by
    change SchroederAllPass.response g z 1 - SchroederAllPass.response g w 1 = _
    simp only [SchroederAllPass.response, pow_one]
    rw [div_sub_div _ _ hz' hw']
    congr 1
    ring
  have numerator : ‖1 - g ^ 2‖ = (1 + |gain|) * (1 - |gain|) := by
    rw [show 1 - g ^ 2 = ((1 - gain ^ 2 : ℝ) : ℂ) by push_cast; rfl,
      Complex.norm_real, Real.norm_eq_abs, abs_of_nonneg (by nlinarith [sq_lt_one_iff_abs_lt_one gain |>.mpr hg]),
      ← sq_abs gain]
    ring
  rw [identity, norm_div, norm_mul, norm_mul, numerator]
  calc
    _ ≤ ((1 + |gain|) * (1 - |gain|) * ‖z - w‖) / ((1 - |gain|) * (1 - |gain|)) :=
      div_le_div_of_nonneg_left (by positivity) (mul_pos positive positive)
        (mul_le_mul hz hw positive.le (norm_nonneg _))
    _ = (1 + |gain|) / (1 - |gain|) * ‖z - w‖ := by field_simp
    _ ≤ _ := mul_le_mul_of_nonneg_left (norm_delayPhasor_sub_le frequency before after) (by positivity)

/-- At an initially resonant frequency, changing one ideal APF tap produces
the chord in the scalar frozen-mode denominator. Other loop factors are held fixed. -/
theorem frozenAllPass_mode_power {gain frequency before after retention : ℝ}
    (hg : |gain| < 1) :
    let a := frozenAllPass gain frequency before
    let b := frozenAllPass gain frequency after
    powerRatio retention (b / a) =
      (1 - retention) ^ 2 / ((1 - retention) ^ 2 + retention * Complex.normSq (a - b)) := by
  dsimp only
  let a := frozenAllPass gain frequency before
  let b := frozenAllPass gain frequency after
  have ha : Complex.normSq a = 1 := frozenAllPass_normSq hg
  have hb : Complex.normSq b = 1 := frozenAllPass_normSq hg
  have ha' : a ≠ 0 := by intro eq; simp [eq] at ha
  change powerRatio retention (b / a) = _
  rw [powerRatio, denominator_eq retention (by rw [Complex.normSq_div, hb, ha, div_one])]
  rw [← div_self ha', ← sub_div, Complex.normSq_div, ha, div_one]

end
end DSP.ModulatedMode
