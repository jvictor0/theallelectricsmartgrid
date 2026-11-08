import Encoder.HeadStateMachine

namespace EncoderHeadRuntime

-- @head runtime: safety of arbitrary sequences of the modeled runtime operations.
-- This is reachability in Veil's generated transition system, not a second model.
theorem reachable_ready (timing : Encoder.HistoricalTiming) (initialValue : Encoder.DefaultValue) (ratio : Nat) (bipolar : Bool)
    (target frame : Type) [DecidableEq target] [Inhabited target]
    [DecidableEq frame] [Inhabited frame]
    (th : Theory timing initialValue ratio bipolar target frame)
    (s : State (FieldAbstractType timing initialValue ratio bipolar target frame))
    (h : (relationalTransitionSystem timing initialValue ratio bipolar target frame).reachable th s) :
    Encoder.Reset.Ready timing s.cell ∧
      (s.published = true → s.cache.values = Encoder.Head.publish s.output s.cacheHost ratio bipolar) := by
  letI : (value : target) → (r : Theory timing initialValue ratio bipolar target frame) →
      Decidable (Encoder.UnitInterval (r.numeric value)) := fun _ _ => Classical.propDecidable _
  induction h with
  | init s _ hi =>
    change initializer.ext.tr _ _ timing initialValue ratio bipolar target frame _ th default s at hi
    change ({ cell := Encoder.initialFrom timing initialValue.value,
              output := ⟨fun _ => initialValue.value, fun _ => initialValue.value, 0⟩,
              cache := ⟨fun _ _ => 0⟩, published := false, cacheHost := 0 } :
      State (FieldAbstractType timing initialValue ratio bipolar target frame)) = s at hi
    subst s
    exact ⟨Encoder.Reset.initial_ready timing initialValue.value initialValue.normalized, by simp⟩
  | step s s' _ hn ih =>
    rcases hn with ⟨label, hn⟩
    change (NextAct _ _ timing initialValue ratio bipolar target frame _ label).toTransitionDerived th s s' at hn
    cases label with
    | record_target scene value =>
      simp only [NextAct, record_target.ext.derived_eq] at hn
      change Encoder.UnitInterval (th.numeric value) ∧
        { s with cell := Encoder.edit timing s.cell scene (th.numeric value) } = s' at hn
      rcases hn with ⟨hv, rfl⟩
      exact ⟨Encoder.Reset.edit_ready timing s.cell scene _ ih.1 hv, ih.2⟩
    | set_target scene value =>
      simp only [NextAct, set_target.ext.derived_eq] at hn
      change Encoder.UnitInterval (th.numeric value) ∧
        { s with cell := Encoder.setTarget timing s.cell scene (th.numeric value) } = s' at hn
      rcases hn with ⟨hv, rfl⟩
      exact ⟨Encoder.Reset.setTarget_ready timing s.cell scene _ ih.1 hv, ih.2⟩
    | turn scene delta towardsZero =>
      simp only [NextAct, turn.ext.derived_eq] at hn
      change { s with cell := Encoder.Edits.turn timing s.cell scene bipolar towardsZero (th.numeric delta) } = s' at hn
      subst s'
      exact ⟨Encoder.Reset.turn_ready timing s.cell scene _ _ _ ih.1, ih.2⟩
    | reset allScenes left right position =>
      simp only [NextAct, reset.ext.derived_eq] at hn
      change Encoder.UnitInterval (th.numeric position) ∧
        { s with cell := Encoder.Reset.resetCell timing (Encoder.Reset.selected allScenes left right (th.numeric position)) initialValue.value s.cell } = s' at hn
      rcases hn with ⟨_, rfl⟩
      exact ⟨Encoder.Reset.reset_ready timing _ initialValue.value s.cell ih.1 initialValue.normalized, ih.2⟩
    | compute advance =>
      simp only [NextAct, compute.ext.derived_eq] at hn
      change { s with cell := Encoder.compute timing advance s.cell } = s' at hn
      subst s'
      exact ⟨Encoder.Reset.compute_ready timing s.cell advance ih.1, ih.2⟩
    | set_output host value =>
      simp only [NextAct, set_output.ext.derived_eq] at hn
      change { s with output := Encoder.setOutput s.output (th.timestamp host) (th.vector value),
                      published := false } = s' at hn
      subst s'
      exact ⟨ih.1, by simp⟩
    | publish host =>
      simp only [NextAct, publish.ext.derived_eq] at hn
      change { s with cache := ⟨Encoder.Head.publish s.output (th.timestamp host) ratio bipolar⟩,
                      cacheHost := th.timestamp host, published := true } = s' at hn
      subst s'
      exact ⟨ih.1, fun _ => rfl⟩

-- Existing safety contract follows from the stronger reachable ramp invariant.
theorem reachable_safe (timing : Encoder.HistoricalTiming) (initialValue : Encoder.DefaultValue) (ratio : Nat) (bipolar : Bool)
    (target frame : Type) [DecidableEq target] [Inhabited target]
    [DecidableEq frame] [Inhabited frame]
    (th : Theory timing initialValue ratio bipolar target frame)
    (s : State (FieldAbstractType timing initialValue ratio bipolar target frame))
    (h : (relationalTransitionSystem timing initialValue ratio bipolar target frame).reachable th s) :
    Encoder.Valid timing s.cell ∧
      (s.published = true → s.cache.values = Encoder.Head.publish s.output s.cacheHost ratio bipolar) := by
  have hr := reachable_ready timing initialValue ratio bipolar target frame th s h
  exact ⟨hr.1.1, hr.2⟩

end EncoderHeadRuntime
