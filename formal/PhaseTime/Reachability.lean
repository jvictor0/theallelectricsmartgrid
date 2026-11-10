import PhaseTime.StateMachine

namespace PhaseRuntime

-- @cpp runtime: induction over Veil's generated transition relation.
theorem reachable_safe (number : Type) [DecidableEq number] [Inhabited number]
    (th : Theory number) (s : State (FieldAbstractType number))
    (h : (relationalTransitionSystem number).reachable th s) :
    s.clock.Valid ∧ s.history.Valid := by
  induction h with
  | init s _ hi =>
    change initializer.ext.tr _ _ number _ th default s at hi
    change ({ clock := PhaseTime.initialClock, history := PhaseTime.initialHistory,
              publishedTime := 0, publishedPhase := 0 } : State (FieldAbstractType number)) = s at hi
    subst s
    exact ⟨PhaseTime.initial_valid, PhaseTime.history_initial_valid⟩
  | step s s' _ hn ih =>
    rcases hn with ⟨label, hn⟩
    change (NextAct _ _ number _ label).toTransitionDerived th s s' at hn
    cases label with
    | running_sample p =>
      simp only [NextAct, running_sample.ext.derived_eq] at hn
      change s.clock.running = true ∧ {s with clock := s.clock.advance (th.numeric p)} = s' at hn
      rcases hn with ⟨_, rfl⟩
      exact ⟨PhaseTime.advance_valid _ _, ih.2⟩
    | restart p =>
      simp only [NextAct, restart.ext.derived_eq] at hn
      change (!s.clock.running) = true ∧ {s with clock := s.clock.resume (th.numeric p)} = s' at hn
      rcases hn with ⟨_, rfl⟩
      exact ⟨PhaseTime.resume_valid _ _, ih.2⟩
    | stopped_sample d =>
      simp only [NextAct, stopped_sample.ext.derived_eq] at hn
      change 0 < th.numeric d ∧ {s with clock := s.clock.freeRun (th.numeric d)} = s' at hn
      rcases hn with ⟨_, rfl⟩
      exact ⟨PhaseTime.free_valid _ _, ih.2⟩
    | forward_observation t =>
      simp only [NextAct, forward_observation.ext.derived_eq] at hn
      change s.history.cursor < s.clock.phase ∧ s.history.now < th.numeric t ∧
        {s with history := s.history.record s.clock.phase (th.numeric t)} = s' at hn
      rcases hn with ⟨_, ht, rfl⟩
      exact ⟨ih.1, PhaseTime.record_valid _ _ _ ih.2 ht⟩
    | reverse_observation t =>
      simp only [NextAct, reverse_observation.ext.derived_eq] at hn
      change s.clock.phase ≤ s.history.cursor ∧ s.history.now < th.numeric t ∧
        {s with history := s.history.retreat s.clock.phase (th.numeric t)} = s' at hn
      rcases hn with ⟨_, ht, rfl⟩
      exact ⟨ih.1, PhaseTime.retreat_valid _ _ _ ih.2 ht⟩
    | reassign_cell_coordinate p t =>
      simp only [NextAct, reassign_cell_coordinate.ext.derived_eq] at hn
      change s.history.forward (th.numeric t) = th.numeric p ∧ th.numeric t ≤ s.history.now ∧
        {s with history := s.history.reassign (th.numeric p) (th.numeric t)} = s' at hn
      rcases hn with ⟨hf, ht, rfl⟩
      exact ⟨ih.1, PhaseTime.reassign_valid _ _ _ ih.2 hf ht⟩
    | publish =>
      simp only [NextAct, publish.ext.derived_eq] at hn
      change {s with publishedTime := s.history.now, publishedPhase := s.history.cursor} = s' at hn
      subst s'
      exact ih

-- Reads use the published cursor even when the live writer has moved since capture.
theorem reachable_published_read_sound (number : Type) [DecidableEq number] [Inhabited number]
    (th : Theory number) (s : State (FieldAbstractType number))
    (hr : (relationalTransitionSystem number).reachable th s) (q t : Rat)
    (hread : s.history.readAt q s.publishedTime s.publishedPhase = some t) :
    s.history.forward t = q ∧ t ≤ s.publishedTime ∧ q ≤ s.publishedPhase := by
  exact PhaseTime.captured_read_sound _ _ _ _ _ (reachable_safe number th s hr).2 hread

end PhaseRuntime
