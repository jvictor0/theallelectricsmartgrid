import DSP.Analysis.ResidualValidation

/-! Parameter cells for continuing an isolated periodic state. A cell can be a
subtype of a knob box with a weighted metric. Its uniform estimates refer to
the whole box and state ball, including points between observations. -/

noncomputable section
namespace DSP.ResidualValidation
open Metric
open scoped NNReal
variable {E P : Type*} [NormedAddCommGroup E] [NormedSpace ℝ E] [CompleteSpace E]
  [PseudoMetricSpace P]

/-- A single solver ball isolates a root throughout a parameter cell and controls
how far that root can move. The constant uses the solver contraction. -/
theorem isolated_branch (residual : P → E → E) (inverse : E →L[ℝ] E)
    (hi : Function.Injective inverse) (center : E) (radius defect variation : ℝ) (q : ℝ≥0)
    (hr : 0 ≤ radius) (hq : q < 1)
    (hc : ∀ p, LipschitzOnWith q (correction (residual p) inverse) (closedBall center radius))
    (hd : ∀ p, ‖inverse (residual p center)‖ ≤ defect)
    (htrap : defect + q * radius ≤ radius)
    (hv : ∀ p t x, x ∈ closedBall center radius →
      dist (correction (residual p) inverse x) (correction (residual t) inverse x) ≤
        variation * dist p t) :
    ∃ branch : P → E,
      (∀ p, branch p ∈ closedBall center radius ∧ residual p (branch p) = 0 ∧
        (∀ x ∈ closedBall center radius, residual p x = 0 → x = branch p)) ∧
      ∀ p t, dist (branch p) (branch t) ≤ variation * dist p t / (1 - q) := by
  classical
  have roots := fun p => exists_solution (residual p) inverse hi center radius defect q hr hq
    (hc p) (hd p) htrap
  choose b hb hz hu _ using roots
  refine ⟨b, fun p => ⟨hb p, hz p, hu p⟩, ?_⟩
  intro p t
  have fix (s : P) := (fixed_iff (residual s) inverse hi (b s)).mpr (hz s)
  have hdist : dist (b p) (b t) ≤ q * dist (b p) (b t) + variation * dist p t := by
    calc
      _ = dist (correction (residual p) inverse (b p))
          (correction (residual t) inverse (b t)) := by rw [fix p, fix t]
      _ ≤ _ := dist_triangle _ (correction (residual p) inverse (b t)) _
      _ ≤ _ := add_le_add ((hc p).dist_le_mul _ (hb p) _ (hb t)) (hv p t _ (hb t))
  apply (le_div_iff₀ (sub_pos.mpr (show (q : ℝ) < 1 from hq))).mpr
  nlinarith only [hdist]

omit [NormedSpace ℝ E] [CompleteSpace E] in
/-- An observable may depend directly on the knobs as well as on the solved state.
A checked state sensitivity bound therefore also bounds peaks between probes. -/
theorem observable_variation (branch : P → E) (observe : P → E → ℝ)
    (stateSlope knobSlope responseSlope : ℝ) (hs : 0 ≤ responseSlope)
    (hb : ∀ p t, dist (branch p) (branch t) ≤ stateSlope * dist p t)
    (ho : ∀ p t, dist (observe p (branch p)) (observe t (branch t)) ≤
      knobSlope * dist p t + responseSlope * dist (branch p) (branch t)) (p t : P) :
    dist (observe p (branch p)) (observe t (branch t)) ≤
      (knobSlope + responseSlope * stateSlope) * dist p t := by
  calc
    _ ≤ _ := ho p t
    _ ≤ knobSlope * dist p t + responseSlope * (stateSlope * dist p t) :=
      add_le_add (le_refl _) (mul_le_mul_of_nonneg_left (hb p t) hs)
    _ = _ := by ring

/-- A uniform power enclosure on a cell validates a linear fit over its entire
interior. The two multiplicative inequalities bound absolute error in dB when
the powers are positive. -/
theorem linear_fit_power_bound {power lo hi left right t factor : ℝ}
    (ht : 0 ≤ t) (ht' : t ≤ 1) (hf : 0 ≤ factor)
    (hl : lo ≤ power) (hh : power ≤ hi)
    (ha : left ≤ factor * lo) (hb : right ≤ factor * lo)
    (hc : hi ≤ factor * left) (hd : hi ≤ factor * right) :
    let fit := (1-t)*left + t*right
    fit ≤ factor*power ∧ power ≤ factor*fit := by
  dsimp
  constructor
  · calc
      _ ≤ (1-t)*(factor*lo) + t*(factor*lo) := add_le_add
        (mul_le_mul_of_nonneg_left ha (sub_nonneg.mpr ht'))
        (mul_le_mul_of_nonneg_left hb ht)
      _ = factor*lo := by ring
      _ ≤ _ := mul_le_mul_of_nonneg_left hl hf
  · calc
      _ ≤ hi := hh
      _ = (1-t)*hi + t*hi := by ring
      _ ≤ (1-t)*(factor*left) + t*(factor*right) := add_le_add
        (mul_le_mul_of_nonneg_left hc (sub_nonneg.mpr ht'))
        (mul_le_mul_of_nonneg_left hd ht)
      _ = _ := by ring

end DSP.ResidualValidation
