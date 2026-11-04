import Encoder.Reset
import Encoder.Evaluation
import Encoder.Curve
import Mathlib.Tactic.Linarith
import Mathlib.Tactic.Ring

namespace Encoder.Motion

/-! Behavioral obligations for moving controls. These describe the values an
encoder exposes, independently of recursive/prepared execution and storage.
The arithmetic is exact; native floating-point error remains a test obligation.
-/

-- @cpp numeric: interpolation changes proportionally to the blend motion.
theorem blend_motion (left right before after : Rat) :
    blend left right after - blend left right before = (right-left)*(after-before) := by
  unfold blend
  ring

theorem blend_motion_bound (left right before after : Rat)
    (hl : UnitInterval left) (hr : UnitInterval right) :
    |blend left right after - blend left right before| ≤ |after-before| := by
  rw [blend_motion, abs_mul]
  have h : |right-left| ≤ 1 := by
    rw [abs_le]
    unfold UnitInterval at hl hr
    constructor <;> linarith
  calc
    |right-left| * |after-before| ≤ 1 * |after-before| :=
      mul_le_mul_of_nonneg_right h (abs_nonneg _)
    _ = |after-before| := one_mul _

-- @cpp ramp: a ramp step stays between its current value and target.
theorem normalized_clamp (x : Rat) (h : UnitInterval x) : clamp01 x = x := by
  unfold clamp01 UnitInterval at *
  split <;> (try split) <;> grind

theorem ramp_step_between (s : Cell) (i : Scene)
    (hv : UnitInterval (s.values i)) (ht : UnitInterval (s.targets i)) :
    min (s.values i) (s.targets i) ≤ move s i ∧
      move s i ≤ max (s.values i) (s.targets i) := by
  unfold move
  split
  · exact ⟨min_le_left _ _, le_max_left _ _⟩
  · split
    · exact ⟨min_le_right _ _, le_max_right _ _⟩
    · have hn : 1 ≤ s.remaining := by omega
      have hnq : (1 : Rat) ≤ s.remaining := by exact_mod_cast hn
      have hpos : (0 : Rat) < s.remaining := lt_of_lt_of_le (by decide) hnq
      have hinv : 0 ≤ (1 : Rat) / s.remaining := le_of_lt (one_div_pos.mpr hpos)
      have hle : (1 : Rat) / s.remaining ≤ 1 := by
        apply (div_le_iff₀ hpos).mpr
        simpa using hnq
      have heq : s.values i + (s.targets i - s.values i) / (s.remaining : Rat) =
          s.values i * (1 - 1 / (s.remaining : Rat)) + s.targets i / (s.remaining : Rat) := by ring
      rw [heq]
      have lo : min (s.values i) (s.targets i) ≤
          s.values i * (1 - 1 / (s.remaining : Rat)) + s.targets i / (s.remaining : Rat) := by
        have ha := mul_le_mul_of_nonneg_right (min_le_left (s.values i) (s.targets i)) (sub_nonneg.mpr hle)
        have hb := mul_le_mul_of_nonneg_right (min_le_right (s.values i) (s.targets i)) hinv
        simp only [div_eq_mul_inv, one_mul] at *
        nlinarith
      have hi : s.values i * (1 - 1 / (s.remaining : Rat)) + s.targets i / (s.remaining : Rat) ≤
          max (s.values i) (s.targets i) := by
        have ha := mul_le_mul_of_nonneg_right (le_max_left (s.values i) (s.targets i)) (sub_nonneg.mpr hle)
        have hb := mul_le_mul_of_nonneg_right (le_max_right (s.values i) (s.targets i)) hinv
        simp only [div_eq_mul_inv, one_mul] at *
        nlinarith
      rw [normalized_clamp _ ⟨le_trans (le_min hv.1 ht.1) lo,
        le_trans hi (max_le hv.2 ht.2)⟩]
      exact ⟨lo, hi⟩

-- @cpp refresh: a fully settled subtree needs only one forced scene refresh.
-- Unlike a moving ramp, extending its counter does not change its trajectory.
theorem stationary_run (t : Timing) (s : Cell) (n : Nat)
    (hr : s.remaining = 0) (ha : s.age = t.maximum) :
    (run t n s).values = s.values ∧ (run t n s).age = t.maximum ∧
      (run t n s).remaining = 0 := by
  induction n generalizing s with
  | zero => simp [run, hr, ha]
  | succ n ih =>
    have hm : move s = s.values := by
      funext i
      simp [move, hr]
    have step : (compute t true s).values = s.values ∧
        (compute t true s).age = t.maximum ∧ (compute t true s).remaining = 0 := by
      unfold compute
      split <;> simp [hm, hr, ha]
    have next := ih (compute t true s) step.2.2 step.2.1
    simpa only [run, step.1] using next

theorem settled_refresh_preserves_ramp_values (t : Timing) (s : Cell) (n : Nat)
    (hr : s.remaining = 0) (ha : s.age = t.maximum) :
    (run t n { s with force := 1 }).values =
      (run t n { s with force := t.maximum }).values ∧
    (run t n { s with force := 1 }).age =
      (run t n { s with force := t.maximum }).age := by
  have short := stationary_run t { s with force := 1 } n hr ha
  have long := stationary_run t { s with force := t.maximum } n hr ha
  exact ⟨short.1.trans long.1.symm, short.2.1.trans long.2.1.symm⟩

-- @cpp gestures: a single fader moves continuously through zero weight.
theorem gesture_zero_weight_motion (base target w : Rat) (hw : 0 ≤ w) :
    gestureBase base [⟨w,target⟩] = blend base target w := by
  unfold gestureBase
  simp only [List.foldl_cons, List.foldl_nil, zero_add]
  split
  · rename_i h
    exact mul_div_cancel_left₀ _ (ne_of_gt h)
  · have hz : w = 0 := by linarith
    simp [hz, blend]

end Encoder.Motion
