import PhaseTime.Clock

namespace PhaseTime

/-! A pointwise exact inverse. Each `record` represents one forward-observed
coordinate. This remains the ideal reference model. InverseGrid separately
bounds the concrete branch-aware interpolation and relative-storage error;
finite retention and hardware floating-point semantics remain outside it. Missing history is explicitly optional. -/
structure History where
  forward : Rat → Rat
  inverse : Rat → Option Rat
  now : Rat
  cursor : Rat
  deriving Inhabited

def History.Valid (s : History) : Prop :=
  ∀ q t, s.inverse q = some t → s.forward t = q ∧ t ≤ s.now

def initialHistory : History := ⟨fun _ => 0, fun _ => none, 0, 0⟩

-- @cpp inverse_forward: replace the inverse at a newly forward-written coordinate.
def History.record (s : History) (q t : Rat) : History :=
  ⟨fun time => if time = t then q else s.forward time,
   fun phase => if phase = q then some t else s.inverse phase, t, q⟩

-- @cpp inverse_reverse: backwards/stationary observations do not rewrite the inverse.
def History.retreat (s : History) (q t : Rat) : History :=
  ⟨fun time => if time = t then q else s.forward time, s.inverse, t, q⟩

theorem history_initial_valid : initialHistory.Valid := by simp [History.Valid, initialHistory]

theorem record_valid (s : History) (q t : Rat) (hs : s.Valid) (ht : s.now < t) :
    (s.record q t).Valid := by
  intro p u hu
  by_cases hp : p = q
  · subst p
    simp [History.record] at hu
    subst u
    simp [History.record]
  · have hold : s.inverse p = some u := by simpa [History.record, hp] using hu
    have hv := hs p u hold
    have hne : u ≠ t := by linarith [hv.2]
    simpa [History.record, hne] using And.intro hv.1 (le_trans hv.2 (le_of_lt ht))

theorem retreat_valid (s : History) (q t : Rat) (hs : s.Valid) (ht : s.now < t) :
    (s.retreat q t).Valid := by
  intro p u hu
  have hv := hs p u hu
  have hne : u ≠ t := by linarith [hv.2]
  simpa [History.retreat, hne] using And.intro hv.1 (le_trans hv.2 (le_of_lt ht))

-- @cpp inverse_reassign: a discarded island may select another valid historical visit.
def History.reassign (s : History) (q t : Rat) : History :=
  { s with inverse := fun phase => if phase = q then some t else s.inverse phase }

theorem reassign_valid (s : History) (q t : Rat) (hs : s.Valid)
    (hf : s.forward t = q) (ht : t ≤ s.now) : (s.reassign q t).Valid := by
  intro p u hu
  by_cases hp : p = q
  · subst p
    simp [History.reassign] at hu
    subst u
    exact ⟨hf, ht⟩
  · exact hs p u (by simpa [History.reassign, hp] using hu)

theorem reassign_selects_history (s : History) (q t : Rat) :
    (s.reassign q t).inverse q = some t := by simp [History.reassign]

theorem reassign_preserves_clock (s : History) (q t : Rat) :
    (s.reassign q t).now = s.now ∧ (s.reassign q t).cursor = s.cursor := ⟨rfl, rfl⟩

theorem latest_forward_wins (s : History) (q t : Rat) : (s.record q t).inverse q = some t := by
  simp [History.record]
theorem other_coordinates_unchanged (s : History) (q p t : Rat) (h : p ≠ q) :
    (s.record q t).inverse p = s.inverse p := by simp [History.record, h]
theorem rewind_keeps_inverse (s : History) (q t : Rat) : (s.retreat q t).inverse = s.inverse := rfl

-- @cpp inverse_read: only coordinates behind the cursor and published real times are readable.
def History.readAt (s : History) (q cutoff cursor : Rat) : Option Rat :=
  if q ≤ cursor then (s.inverse q).filter (fun t => t ≤ cutoff) else none

def History.read (s : History) (q cutoff : Rat) : Option Rat :=
  s.readAt q cutoff s.cursor

theorem captured_read_sound (s : History) (q cutoff cursor t : Rat) (hs : s.Valid)
    (h : s.readAt q cutoff cursor = some t) :
    s.forward t = q ∧ t ≤ cutoff ∧ q ≤ cursor := by
  by_cases hq : q ≤ cursor
  · cases hi : s.inverse q with
    | none => simp [History.readAt, hq, hi] at h
    | some u =>
      simp [History.readAt, hq, hi] at h
      rcases h with ⟨rfl, ht⟩
      exact ⟨(hs q u hi).1, ht, hq⟩
  · simp [History.readAt, hq] at h

theorem future_unavailable (s : History) (q cutoff : Rat) (h : s.cursor < q) :
    s.read q cutoff = none := by simp [History.read, History.readAt, not_le_of_gt h]

theorem published_read_sound (s : History) (q cutoff t : Rat) (hs : s.Valid)
    (h : s.read q cutoff = some t) :
    s.forward t = q ∧ t ≤ s.now ∧ t ≤ cutoff ∧ q ≤ s.cursor := by
  by_cases hq : q ≤ s.cursor
  · cases hi : s.inverse q with
    | none => simp [History.read, History.readAt, hq, hi] at h
    | some u =>
      simp [History.read, History.readAt, hq, hi] at h
      rcases h with ⟨rfl, ht⟩
      exact ⟨(hs q u hi).1, (hs q u hi).2, ht, hq⟩
  · simp [History.read, History.readAt, hq] at h

theorem overwritten_after_publication (s : History) (q t cutoff : Rat) (ht : cutoff < t) :
    (s.record q t).read q cutoff = none := by
  simpa [History.read, History.readAt, History.record] using ht

theorem repeated_phase_chooses_last_forward :
    let s := ((initialHistory.record 0 1).retreat (-1) 2).record 0 3
    s.forward 1 = 0 ∧ s.forward 3 = 0 ∧ s.inverse 0 = some 3 := by
  norm_num [History.record, History.retreat, initialHistory]

theorem right_inverse (s : History) (q t : Rat) (hs : s.Valid) (h : s.inverse q = some t) :
    s.forward t = q := (hs q t h).1

-- @cpp scope_read: This is S ∘ G = L, with the actual hypothesis S = L ∘ F stated explicitly.
theorem scope_composition (s : History) (L S : Rat → Rat) (q t : Rat)
    (hs : s.Valid) (h : s.inverse q = some t) (signal : ∀ t, S t = L (s.forward t)) :
    S t = L q := by rw [signal, right_inverse s q t hs h]

-- @cpp warp_scope: invert unwarped phase and add the recorded displacement.
-- The origin is constant over the queried observations, as in the LFO contract.
theorem warp_scope_composition (s : History) (F displacement : Rat → Rat)
    (q t origin : Rat) (hs : s.Valid) (h : s.inverse q = some t)
    (signal : ∀ t, displacement t = F t - (s.forward t - origin)) :
    q - origin + displacement t = F t := by
  rw [signal, right_inverse s q t hs h]
  ring

-- A tempo edit has no operation on the inverse; its result depends only on phase.
theorem tempo_independent_read (s : History) (q cutoff tempo₁ tempo₂ : Rat) :
    (fun (_ : Rat) => s.read q cutoff) tempo₁ = (fun (_ : Rat) => s.read q cutoff) tempo₂ := rfl

end PhaseTime
