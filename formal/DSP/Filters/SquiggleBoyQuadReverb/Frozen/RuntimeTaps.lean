import DSP.Filters.SquiggleBoyQuadReverb.Frozen.Blocks
import DSP.Filters.DelayKernelsProofs

namespace DSP.SquiggleBoyQuadReverb.Frozen

open DelayBuffer WanderingSchroederAllPass

noncomputable section

theorem run_bind {S A B : Type} (a : StateM S A) (f : A → StateM S B) (s : S) :
    (a >>= f).run s = (f (a.run s).1).run (a.run s).2 := by
  show (match a.run s with | (x, s') => (f x).run s') = _
  cases a.run s
  rfl

theorem run_pure {S A : Type} (x : A) (s : S) : (pure x : StateM S A).run s = (x, s) := rfl

theorem run_get {S : Type} (s : S) : (get : StateM S S).run s = (s, s) := rfl

theorem run_set {S : Type} (s t : S) : (set t : StateM S PUnit).run s = (⟨⟩, t) := rfl

theorem run_focus {S T A : Type} (getChild : S → T) (setChild : S → T → S)
    (action : StateM T A) (s : S) :
    (focus getChild setChild action).run s =
      ((action.run (getChild s)).1, setChild s (action.run (getChild s)).2) := rfl

theorem run_pushFrame {n : ℕ} {R : Type} (s : DelayBuffer n R) (x : R) :
    (pushFrame x : StateM (DelayBuffer n R) PUnit).run s = (⟨⟩, s.push x) := rfl

/-- The exact control interpretation of the positive integer sample conversions. -/
instance : DelayBuffer.SampleIndex ℝ := ⟨Nat.cast, Nat.floor⟩

instance : DelayMath (Lanes ℝ) where
  exp2 x i := DelayMath.exp2 (x i)
  floor x i := DelayMath.floor (x i)
  phaseTurns y x i := DelayMath.phaseTurns (y i) (x i)
  decay x i := DelayMath.decay (x i)

def packedTap {n : ℕ} (hn : 0 < n) (oldest : QuadAges) (fraction : Lanes ℝ) : Tap n ℝ where
  age i j := ⟨((oldest.lane i).toNat + n - (UInt64.ofNat j.val).toNat % n) % n,
    Nat.mod_lt _ hn⟩
  fraction := fraction

theorem readCubicAges_frozen {n : ℕ} (hn : 0 < n) (s : DelayBuffer n (Lanes ℝ))
    (oldest : QuadAges) (fraction : Lanes ℝ) :
    (readCubicAges oldest fraction).run s =
      (tapRead (packedTap hn oldest fraction) s.history, s) := by
  apply Prod.ext
  · funext i
    let v := fun j : Fin 4 => s.history ((packedTap hn oldest fraction).age i j) i
    change cubicInterpolate (v 0) (v 1) (v 2) (v 3) (fraction i) =
      tapRead (packedTap hn oldest fraction) s.history i
    rw [tapRead_apply]
    exact cubicInterpolate_eq_sum (by norm_num : (0.5 : ℝ) = 1/2) v (fraction i)
  · rfl

def mainTap (delay : Lanes ℝ) : Tap squiggleBoyReverbDelayCapacity ℝ :=
  let (rounded, fraction) := fractionalPosition delay
  packedTap (by decide) (SampleAges.toAges rounded) fraction

theorem readMainDelay_frozen (s : DelayBuffer squiggleBoyReverbDelayCapacity (Lanes ℝ)) (d : Lanes ℝ) :
    (readMainDelay ⟨d⟩).run s = (tapRead (mainTap d) s.history, s) :=
  readCubicAges_frozen (by decide) s _ _

def apfTap {n : ℕ} (hn : 0 < n) (spec : TapSpec) (spline : Lanes ℝ) : Tap n ℝ :=
  let (oldest, fraction) := tapPosition spec spline
  packedTap hn oldest fraction

set_option maxRecDepth 2500 in
theorem apfStep_frozen {n : ℕ} (hn : 0 < n) (spec : TapSpec)
    (state : Storage n (Lanes ℝ) (Lanes ℝ)) (input : Lanes ℝ) :
    let (y, next) := (WanderingSchroederAllPass.step spec get input).run state
    next.clock = state.clock ∧
      (next.history.history, y) = allPass (apfTap hn spec state.clock) allPassGain
        (state.history.history, input) := by
  simp only [WanderingSchroederAllPass.step, run_bind, run_focus, run_get, run_pure, apfTap]
  let args := tapPosition spec state.clock
  have hgain (v : Lanes ℝ) : (allPassGain : Lanes ℝ) * v = (allPassGain : ℝ) • v := rfl
  have hy (v : Lanes ℝ) : -(allPassGain : Lanes ℝ) * input + v =
      v - (allPassGain : ℝ) • input := by
    rw [neg_mul, hgain, sub_eq_add_neg, add_comm]
  have hr := readCubicAges_frozen hn state.history args.1 args.2
  simp only [run_pushFrame, allPass_apply]
  rw [hr, push_eq_shift, ← DelayBuffer.history_push]
  simp only [DelayKernels.allPassStep, fma_eq, hy, hgain, add_comm, true_and, args]

end

end DSP.SquiggleBoyQuadReverb.Frozen
