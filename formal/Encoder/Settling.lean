import Encoder.Motion

namespace Encoder.Settling

/-! The current runtime's countdown is a conservative proof budget. The actual
filter may retire as soon as all scenes are exact. These lemmas relate the
budgeted trajectory to the literal one-pole and justify skipping idle scenes.
-/

-- @cpp ramp: moving scenes execute the pole; settled scenes execute no filter.
structure SceneState where
  value : Rat
  target : Rat
  moving : Bool

def tick (s : SceneState) : SceneState :=
  if s.moving then
    let next := OnePole.step s.value s.target
    ⟨next, s.target, decide (next ≠ s.target)⟩
  else s

theorem idle_noop (s : SceneState) (h : s.moving = false) : tick s = s := by
  simp [tick, h]

theorem tick_matches_pole (s : SceneState)
    (h : s.moving = false → s.value = s.target) :
    (tick s).value = OnePole.step s.value s.target := by
  cases hm : s.moving
  · simp [tick, hm, h hm, OnePole.settled_noop]
  · simp [tick, hm]

theorem landing_retires (s : SceneState) (h : s.moving = true)
    (hn : |OnePole.raw s.value s.target - s.target| ≤ OnePole.epsilon) :
    (tick s).value = s.target ∧ (tick s).moving = false := by
  simp [tick, h, OnePole.step, hn]

theorem moving_step (t : Timing) (s : Cell) (i : Scene)
    (ht : t.onePole = true) (hr : 1 < s.remaining) :
    move t s i = OnePole.step (s.values i) (s.targets i) := by
  simp [move, ht, show s.remaining ≠ 0 by omega, show s.remaining ≠ 1 by omega]

theorem trajectory (t : Timing) (s : Cell) (n : Nat) (i : Scene)
    (ht : t.onePole = true) (hr : n < s.remaining) (hf : n < s.force) :
    (run t n s).values i = OnePole.run n (s.values i) (s.targets i) := by
  induction n generalizing s with
  | zero => rfl
  | succ n ih =>
    have hp : 0 < s.force := by omega
    have hc : (compute t true s).remaining = s.remaining - 1 := by simp [compute, hp]
    have hfc : (compute t true s).force = s.force - 1 := by simp [compute, hp]
    rw [run, ih (compute t true s) (by omega) (by omega)]
    have hm := moving_step t s i ht (by omega)
    simpa only [compute, true_and, hp, ↓reduceIte, hm] using
      OnePole.run_step n (s.values i) (s.targets i)

-- The last budget step cannot truncate a still-audible tail: the literal pole
-- has already reached the exact target at that step, without using a counter.
theorem budget_landing_matches_pole (s : Cell) (i : Scene)
    (hr : s.remaining = 15001) (hf : 15001 ≤ s.force)
    (hv : UnitInterval (s.values i)) (ht : UnitInterval (s.targets i)) :
    (run currentTiming 15001 s).values i =
      OnePole.run 15001 (s.values i) (s.targets i) := by
  rw [(ramp_finishes currentTiming 15000 s hr hf).1, OnePole.run_lands _ _ hv ht]

theorem exact_values_stay (t : Timing) (s : Cell) (n : Nat)
    (hv : s.values = s.targets) (ht : ∀ i, UnitInterval (s.targets i)) :
    (run t n s).values = s.targets := by
  induction n generalizing s with
  | zero => exact hv
  | succ n ih =>
    have hm : move t s = s.targets := by
      funext i
      simp only [move, hv]
      split
      · rfl
      · split
        · rfl
        · split
          · exact OnePole.settled_noop _
          · simpa using Motion.normalized_clamp (s.targets i) (ht i)
    have hc : (compute t true s).values = (compute t true s).targets := by
      unfold compute
      split <;> simp_all
    simpa only [run, compute_preserves_targets] using
      ih (compute t true s) hc (by simpa only [compute_preserves_targets])

theorem early_retirement_preserves_values (t : Timing) (s : Cell) (n : Nat)
    (hv : s.values = s.targets) (ht : ∀ i, UnitInterval (s.targets i)) :
    (run t n { s with remaining := 0, force := 0 }).values = (run t n s).values := by
  exact (exact_values_stay t { s with remaining := 0, force := 0 } n hv ht).trans
    (exact_values_stay t s n hv ht).symm

theorem cadence_independent (age1 age2 : Nat) :
    duration currentTiming age1 = duration currentTiming age2 := rfl

end Encoder.Settling
