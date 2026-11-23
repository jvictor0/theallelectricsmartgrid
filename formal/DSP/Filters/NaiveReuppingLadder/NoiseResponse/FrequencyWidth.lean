import DSP.Filters.NaiveReuppingLadder.NoiseResponse.Display

namespace NoiseDisplay
open Finset

/-- A positive visible discrepancy above t needs both enough headroom
    above the drawing floor and a large enough raw discrepancy. -/
theorem signed_exceedance {a b t : ℝ} (ht : 0 ≤ t) :
    t < clampDb a - clampDb b ↔ -60 + t < a ∧ t < a-b := by
  unfold clampDb
  rcases le_total a (-60) with ha | ha <;> rcases le_total b (-60) with hb | hb
  all_goals simp only [max_eq_left, max_eq_right, *]
  all_goals constructor <;> intro h <;> first | obtain ⟨_, _⟩ := h; linarith | constructor <;> linarith

/-- Corner extrema at the two spectral knots can come from different controls.
    Their interpolant still bounds the raw level at every control in the cell.
    The same argument applied to signed differences bounds the other test. -/
theorem frequency_corner_envelope {ι : Type*} [Fintype ι]
    (w left right : ι → ℝ) {L R t : ℝ} (hw : ∀ i, 0 ≤ w i)
    (hs : ∑ i, w i = 1) (ht : 0 ≤ t) (hu : t ≤ 1)
    (hl : ∀ i, left i ≤ L) (hr : ∀ i, right i ≤ R) :
    ∑ i, w i * ((1-t)*left i + t*right i) ≤ (1-t)*L+t*R := by
  calc
    _ ≤ ∑ i, w i * ((1-t)*L+t*R) := sum_le_sum fun i _ =>
      mul_le_mul_of_nonneg_left (add_le_add
        (mul_le_mul_of_nonneg_left (hl i) (sub_nonneg.mpr hu))
        (mul_le_mul_of_nonneg_left (hr i) ht)) (hw i)
    _ = _ := by rw [←sum_mul, hs, one_mul]

/-- Outward percentages for the integer segment-width counts from the
    hash-checked original archive, over all controls and over an average cell. -/
theorem checked_width_percentages :
    (100*301102:ℚ)/(239*4096) < 30.76 ∧
    (100*84174:ℚ)/(239*4096) < 8.60 ∧
    (100*8390:ℚ)/(239*4096) < 0.858 ∧
    (100*12041187716:ℚ)/(1866240*239*4096) < 0.660 ∧
    (100*456555110:ℚ)/(1866240*239*4096) < 0.0250 ∧
    (100*1413050:ℚ)/(1866240*239*4096) < 0.000078 := by
  norm_num

end NoiseDisplay
