import DSP.Filters.ConservativeReuppingLadder.Stability
import DSP.Filters.ReuppingLadder.TuningProofs
import DSP.Filters.NormalizedTanh.Proofs
import Mathlib.Analysis.SpecialFunctions.Trigonometric.DerivHyp

noncomputable section
namespace DSP
deriving instance Functor for ConservativeReuppingLadder
namespace ConservativeReuppingLadder
open Channels

instance logicalSpecFunctor : Functor LogicalSpec where
  map f s := ⟨f s.cutoff, f s.resonance, f s.gain⟩

def history {R : Type} (s : ConservativeReuppingLadder R) : List R :=
  [s.stage1,s.stage2,s.stage3,s.stage4,s.input,s.output]

def coefficientFields {R : Type} (s : ConservativeReuppingLadder R) : List R :=
  [s.alpha,s.saturation.gain,s.saturation.tanhGain,s.k]

section Generic
variable {R : Type} [OfNat R 0] [OfNat R 1] [OfNat R 2] [OfScientific R]
  [Add R] [Sub R] [Mul R] [Div R] [FMA R] [IfLess R] [Tanh R] [Trig R] [SquareRoot R]

omit [OfNat R 0] [FMA R] in
theorem computeCoefficients_preserves_history (spec : LogicalSpec R) (s : ConservativeReuppingLadder R) :
    history (computeCoefficients spec s) = history s := rfl

omit [OfNat R 0] [FMA R] in
theorem computeCoefficients_idempotent (spec : LogicalSpec R) (s : ConservativeReuppingLadder R) :
    computeCoefficients spec (computeCoefficients spec s) = computeCoefficients spec s := rfl

omit [OfNat R 2] [OfScientific R] [IfLess R] [Trig R] [SquareRoot R] in
theorem step_preserves_coefficients (s : ConservativeReuppingLadder R) (x : Sample R) :
    coefficientFields (step s x) = coefficientFields s := rfl

omit [OfNat R 2] [OfScientific R] [IfLess R] [Trig R] [SquareRoot R] in
theorem step_feedback_delay (s : ConservativeReuppingLadder R) (x : Sample R) :
    (step s x).input = x.x-s.k*s.stage4 := rfl

omit [OfNat R 2] [OfScientific R] [IfLess R] [Trig R] [SquareRoot R] in
theorem step_channel {n : ℕ} (s : ConservativeReuppingLadder (Fin n → R))
    (x : Sample (Fin n → R)) (i : Fin n) :
    channel i (step s x) = step (channel i s) ⟨x.x i⟩ := rfl

omit [OfNat R 0] [FMA R] in
theorem computeCoefficients_channel {n : ℕ} (spec : LogicalSpec (Fin n → R))
    (s : ConservativeReuppingLadder (Fin n → R)) (i : Fin n) :
    channel i (computeCoefficients spec s) = computeCoefficients (channel i spec) (channel i s) := rfl

omit [OfNat R 1] [OfNat R 2] [OfScientific R] [Add R] [Sub R] [Mul R] [Div R]
  [FMA R] [IfLess R] [Tanh R] [Trig R] [SquareRoot R] in
theorem reset_preserves_coefficients (s : ConservativeReuppingLadder R) :
    coefficientFields (reset s) = coefficientFields s := rfl

omit [OfNat R 1] [OfNat R 2] [OfScientific R] [Add R] [Sub R] [Mul R] [Div R]
  [FMA R] [IfLess R] [Tanh R] [Trig R] [SquareRoot R] in
theorem reset_clears_history (s : ConservativeReuppingLadder R) :
    history (reset s) = (history s).map (fun _ => 0) := rfl
end Generic

theorem prepared_slope_pos (spec : LogicalSpec ℝ) (hg : 0 < spec.gain) :
    0 < NormalizedTanh.slopeZero (NormalizedTanh.prepare ⟨spec.gain⟩) := by
  change 0 < spec.gain / Real.tanh spec.gain
  rw [Real.tanh_eq_sinh_div_cosh]
  exact div_pos hg (div_pos (Real.sinh_pos_iff.mpr hg) (Real.cosh_pos _))

/-- Exact cancellation of the three in-loop zero-input slopes. -/
theorem prepared_loopGain (spec : LogicalSpec ℝ) (s : ConservativeReuppingLadder ℝ) (hg : 0 < spec.gain) :
    let f := computeCoefficients spec s
    f.k * NormalizedTanh.slopeZero f.saturation ^ 3 =
      spec.resonance*resonanceMargin*(ReuppingLadder.prepareTuning spec.cutoff).criticalFeedback := by
  have hn := ne_of_gt (prepared_slope_pos spec hg)
  dsimp only [computeCoefficients]
  field_simp

/-- Strict pole stability for the full valid tuning range, including the
Nyquist branch. This theorem is about the proved tangent recurrence. -/
theorem prepared_roots_lt_one (spec : LogicalSpec ℝ) (s : ConservativeReuppingLadder ℝ)
    (hc : 0 < spec.cutoff) (hr0 : 0 ≤ spec.resonance) (hr1 : spec.resonance ≤ 1)
    (hg : 0 < spec.gain) (z : ℂ)
    (hz : characteristic (computeCoefficients spec s).alpha
      ((computeCoefficients spec s).k * NormalizedTanh.slopeZero (computeCoefficients spec s).saturation^3) z = 0) :
    ‖z‖ < 1 := by
  let t := ReuppingLadder.prepareTuning spec.cutoff
  let alpha := t.alpha
  let r := t.poleRatio
  let Q := spec.resonance*resonanceMargin*t.criticalFeedback
  have hp := ReuppingLadder.prepareTuning_properties spec.cutoff hc
  change 0 < alpha ∧ alpha < 1 ∧ 0 < r ∧ alpha*(1+r) = r at hp
  have he : t.criticalFeedback = ReuppingLadder.criticalForRatio r := rfl
  have hb := ReuppingLadder.critical_bounds r hp.2.2.1
  rw [← he] at hb
  have hQ : 0 ≤ Q := by
    have hpos := hb.1
    dsimp only [Q,resonanceMargin]
    positivity
  have hcrit : Q < t.criticalFeedback := by
    have hm : spec.resonance*(resonanceMargin : ℝ) < 1 := by
      norm_num [resonanceMargin] at *
      linarith
    exact (mul_lt_mul_of_pos_right hm hb.1).trans_eq (one_mul _)
  have hn : Q < ((2-alpha)/alpha)^4 := by
    have hid : (r+2)/r = (2-alpha)/alpha := by
      field_simp [ne_of_gt hp.1, ne_of_gt hp.2.2.1]
      linear_combination 2*hp.2.2.2
    rw [← hid]
    exact hcrit.trans_le hb.2.2
  have hcross : Q*(1-alpha)^2 < (2-alpha)^2 := by
    have ht := mul_lt_mul_of_pos_right (hcrit.trans_le hb.2.1)
      (sq_pos_of_pos (sub_pos.mpr hp.2.1))
    have hid : (r+2)*(1-alpha) = 2-alpha := by linear_combination -hp.2.2.2
    rw [← mul_pow, hid] at ht
    exact ht
  apply roots_lt_one alpha Q hp.1 hp.2.1 hQ hn hcross z
  rw [prepared_loopGain spec s hg] at hz
  exact hz

/-- The prepared small-signal transfer has no pole at any digital frequency. -/
theorem prepared_denominator_ne_zero (spec : LogicalSpec ℝ) (s : ConservativeReuppingLadder ℝ)
    (hc : 0 < spec.cutoff) (hr0 : 0 ≤ spec.resonance) (hr1 : spec.resonance ≤ 1)
    (hg : 0 < spec.gain) (q : ℂ) (hq : ‖q‖ = 1) :
    let f := computeCoefficients spec s
    (1-(1-(f.alpha : ℂ))*q)^4+
      ((f.k * NormalizedTanh.slopeZero f.saturation^3 : ℝ) : ℂ)*(f.alpha : ℂ)^4*q ≠ 0 := by
  exact denominator_ne_zero_of_roots _ _
    (prepared_roots_lt_one spec s hc hr0 hr1 hg) q hq

end ConservativeReuppingLadder
end DSP
