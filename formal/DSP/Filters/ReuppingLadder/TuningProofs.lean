import DSP.Filters.ReuppingLadder.Tuning
import DSP.Nonlinear.Real
import DSP.Transcendental.Real

noncomputable section
namespace DSP.ReuppingLadder

theorem critical_bounds (r : ℝ) (hr : 0 < r) :
    0 < criticalForRatio r ∧ criticalForRatio r ≤ (r+2)^2 ∧
      criticalForRatio r ≤ ((r+2)/r)^4 := by
  have he : ((r+2)/r)^4-(r+2)^2 =
      (r+2)^2*(r+1)*(2-r)*(r^2+r+2)/r^4 := by field_simp; ring
  by_cases h : r < 2
  · have hn : 0 ≤ ((r+2)/r)^4-(r+2)^2 := by rw [he]; positivity
    simp only [criticalForRatio, IfLess.ifLess, if_pos h]
    constructor
    · positivity
    constructor <;> nlinarith
  · have hm : 2 ≤ r := le_of_not_gt h
    have hn : ((r+2)/r)^4 ≤ (r+2)^2 := by
      have hp : 0 ≤ (r+2)^2*(r+1)*(r-2)*(r^2+r+2)/r^4 := by positivity
      have hn : (r+2)^2*(r+1)*(2-r)*(r^2+r+2)/r^4 =
          -((r+2)^2*(r+1)*(r-2)*(r^2+r+2)/r^4) := by ring
      linarith
    have hx : maxValue r 2 = r := by simp only [maxValue, IfLess.ifLess, if_neg h]
    have hbase : 1+2/r = (r+2)/r := by field_simp
    simp only [criticalForRatio, IfLess.ifLess, if_neg h, hx, hbase]
    constructor
    · positivity
    constructor <;> nlinarith

theorem fromSine_properties (cutoff sine : ℝ) (hs : 0 < sine) :
    let t := prepareFromSine cutoff sine
    0 < t.alpha ∧ t.alpha < 1 ∧ 0 < t.poleRatio ∧
      t.alpha*(1+t.poleRatio) = t.poleRatio := by
  let root := Real.sqrt (1+sine*sine)
  have hroot : root^2 = 1+sine*sine := Real.sq_sqrt (by positivity)
  have hpos : 0 ≤ root := Real.sqrt_nonneg _
  have hdiff : 0 < root-sine := by nlinarith
  have ha : 0 < sine*2*(root-sine) := by positivity
  have hr : 0 < sine*2*(root+sine) := by positivity
  have he : (sine*2*(root-sine))*(1+sine*2*(root+sine)) = sine*2*(root+sine) := by
    nlinarith [hroot]
  have hu : sine*2*(root-sine) < 1 := by nlinarith
  exact ⟨ha,hu,hr,he⟩

theorem prepareTuning_properties (cutoff : ℝ) (hc : 0 < cutoff) :
    let t := prepareTuning cutoff
    0 < t.alpha ∧ t.alpha < 1 ∧ 0 < t.poleRatio ∧
      t.alpha*(1+t.poleRatio) = t.poleRatio := by
  have hcut : minValue (0.5 : ℝ) cutoff = min 0.5 cutoff := by
    by_cases h : cutoff < 0.5
    · simp [minValue, IfLess.ifLess, h, min_eq_right h.le]
    · simp [minValue, IfLess.ifLess, h, min_eq_left (le_of_not_gt h)]
  have hp : 0 < min (0.5 : ℝ) cutoff := lt_min (by norm_num) hc
  have hu : min (0.5 : ℝ) cutoff ≤ 0.5 := min_le_left _ _
  apply fromSine_properties
  change 0 < Real.sin (2*Real.pi*(minValue (0.5 : ℝ) cutoff*0.5))
  rw [hcut]
  apply Real.sin_pos_of_pos_of_lt_pi
  · positivity
  · nlinarith [Real.pi_pos, mul_le_mul_of_nonneg_left hu Real.pi_pos.le]

end DSP.ReuppingLadder
