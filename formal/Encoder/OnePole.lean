import Mathlib.Analysis.SpecialFunctions.Pow.Real
import Mathlib.Tactic.Linarith
import Mathlib.Tactic.NormNum
import Mathlib.Tactic.Ring

namespace Encoder.OnePole

/-! Fixed host-rate control smoothing. The rational coefficient is the exact
binary64 representation of -expm1(-1/(0.020*48000)). Arithmetic below is exact;
IEEE rounding and early-exit implementation correspondence need native tests.
-/

def alpha : Rat := 1200334617814821 / 1152921504606846976
def decay : Rat := 1 - alpha
def epsilon : Rat := 1 / 1000000

def raw (value target : Rat) : Rat := value + alpha * (target - value)

def step (value target : Rat) : Rat :=
  if value = target then target else
  let next := raw value target
  if |next - target| ≤ epsilon then target else next

theorem alpha_bounds : 0 < alpha ∧ alpha < 1 := by norm_num [alpha]
theorem decay_bounds : 0 ≤ decay ∧ decay ≤ 999/1000 := by norm_num [decay, alpha]
theorem epsilon_positive : 0 < epsilon := by norm_num [epsilon]

theorem raw_residual (value target : Rat) :
    raw value target - target = decay * (value - target) := by
  unfold raw decay
  ring

theorem raw_distance (value target : Rat) :
    |raw value target - target| = decay * |value - target| := by
  rw [raw_residual, abs_mul, abs_of_nonneg decay_bounds.1]

theorem raw_between (value target : Rat) :
    min value target ≤ raw value target ∧ raw value target ≤ max value target := by
  have ha := alpha_bounds
  unfold raw
  rcases le_total value target with h | h
  · rw [min_eq_left h, max_eq_right h]
    constructor <;> nlinarith
  · rw [min_eq_right h, max_eq_left h]
    constructor <;> nlinarith

theorem step_between (value target : Rat) :
    min value target ≤ step value target ∧ step value target ≤ max value target := by
  unfold step
  split
  · exact ⟨min_le_right _ _, le_max_right _ _⟩
  · dsimp only
    split
    · exact ⟨min_le_right _ _, le_max_right _ _⟩
    · exact raw_between value target

theorem step_bounded (value target : Rat)
    (hv : 0 ≤ value ∧ value ≤ 1) (ht : 0 ≤ target ∧ target ≤ 1) :
    0 ≤ step value target ∧ step value target ≤ 1 := by
  have h := step_between value target
  exact ⟨le_trans (le_min hv.1 ht.1) h.1, le_trans h.2 (max_le hv.2 ht.2)⟩

theorem settled_noop (target : Rat) : step target target = target := by simp [step]

theorem step_distance (value target : Rat) :
    |step value target - target| ≤ decay * |value - target| := by
  unfold step
  split
  · simp only [sub_self, abs_zero]
    exact mul_nonneg decay_bounds.1 (abs_nonneg _)
  · dsimp only
    split
    · simp only [sub_self, abs_zero]
      exact mul_nonneg decay_bounds.1 (abs_nonneg _)
    · exact le_of_eq (raw_distance value target)

theorem near_target_lands (value target : Rat) (h : |value-target| ≤ epsilon) :
    step value target = target := by
  have hd : decay ≤ 1 := by have := decay_bounds.2; norm_num at *; linarith
  have hn : |raw value target - target| ≤ epsilon := by
    rw [raw_distance]
    calc
      decay * |value-target| ≤ 1 * |value-target| := mul_le_mul_of_nonneg_right hd (abs_nonneg _)
      _ ≤ epsilon := by simpa using h
  simp [step, hn]

def run : Nat → Rat → Rat → Rat
  | 0, value, _ => value
  | n+1, value, target => step (run n value target) target

theorem run_distance (n : Nat) (value target : Rat) :
    |run n value target - target| ≤ decay^n * |value-target| := by
  induction n with
  | zero => simp [run]
  | succ n ih =>
    calc
      |run (n+1) value target - target| ≤ decay * |run n value target - target| := step_distance _ _
      _ ≤ decay * (decay^n * |value-target|) := mul_le_mul_of_nonneg_left ih decay_bounds.1
      _ = decay^(n+1) * |value-target| := by rw [pow_succ]; ring

-- A conservative kernel-checked bound avoids evaluating a rational with
-- hundreds of thousands of digits. It is a proof budget, not extra idle work.
theorem decay_budget : decay^15000 ≤ epsilon := by
  have block : ((999 : Rat)/1000)^100 ≤ 10/11 := by norm_num
  have tail : ((10 : Rat)/11)^150 ≤ epsilon := by norm_num [epsilon]
  calc
    decay^15000 ≤ ((999 : Rat)/1000)^15000 := pow_le_pow_left₀ decay_bounds.1 decay_bounds.2 _
    _ = (((999 : Rat)/1000)^100)^150 := by rw [← pow_mul]
    _ ≤ ((10 : Rat)/11)^150 := pow_le_pow_left₀ (by positivity) block _
    _ ≤ epsilon := tail

theorem run_lands (value target : Rat)
    (hv : 0 ≤ value ∧ value ≤ 1) (ht : 0 ≤ target ∧ target ≤ 1) :
    run 15001 value target = target := by
  apply near_target_lands
  have hdist : |value-target| ≤ 1 := by rw [abs_le]; constructor <;> linarith
  calc
    |run 15000 value target - target| ≤ decay^15000 * |value-target| := run_distance _ _ _
    _ ≤ decay^15000 * 1 := mul_le_mul_of_nonneg_left hdist (pow_nonneg decay_bounds.1 _)
    _ ≤ epsilon := by simpa using decay_budget

theorem stationary_run (n : Nat) (target : Rat) : run n target target = target := by
  induction n with
  | zero => rfl
  | succ n ih => simpa [run, ih] using settled_noop target

theorem run_step (n : Nat) (value target : Rat) :
    run n (step value target) target = run (n+1) value target := by
  induction n with
  | zero => rfl
  | succ n ih => simp only [run, ih]

end Encoder.OnePole
