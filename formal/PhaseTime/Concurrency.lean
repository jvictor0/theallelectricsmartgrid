import PhaseTime.History

namespace PhaseTime.Concurrency

/-! One writer, atomic scalar payloads, and a sequence counter, all in one
sequentially consistent order. Even means committed. The writer brackets its
scalar stores with counter increments and never waits for readers. Readers
accept an unchanged even counter; every other attempt retries.
Natural-number revisions assume the native counter does not wrap within a read.
Termination requires an eventual attempt between writes; no scheduling or
wall-clock bound is assumed or proved.
-/

structure State (Field Value : Type) where
  revision : Nat
  payload : Field → Value

-- @cpp snapshot_write: one writer, BeginWrite / scalar stores / EndWrite.
inductive Step {Field Value : Type} : State Field Value → State Field Value → Prop
  | idle (s) : Step s s
  | beginWrite (s) (h : s.revision % 2 = 0) :
      Step s {s with revision := s.revision + 1}
  | store (s) (h : s.revision % 2 = 1) (payload) :
      Step s {s with payload := payload}
  | endWrite (s) (h : s.revision % 2 = 1) :
      Step s {s with revision := s.revision + 1}

inductive Steps {Field Value : Type} : State Field Value → State Field Value → Prop
  | refl (s) : Steps s s
  | cons {a b c} : Step a b → Steps b c → Steps a c

variable {Field Value : Type}

theorem step_revision_mono {a b : State Field Value} (h : Step a b) :
    a.revision ≤ b.revision := by
  cases h <;> simp

theorem steps_revision_mono {a b : State Field Value} (h : Steps a b) :
    a.revision ≤ b.revision := by
  induction h with
  | refl => exact le_refl _
  | cons step _ ih => exact le_trans (step_revision_mono step) ih

theorem unchanged_even_step {a b : State Field Value} (h : Step a b)
    (he : a.revision % 2 = 0) (hr : a.revision = b.revision) : a.payload = b.payload := by
  cases h with
  | idle => rfl
  | beginWrite => simp at hr
  | store odd _ => omega
  | endWrite => simp at hr

theorem unchanged_even_steps {a b : State Field Value} (h : Steps a b)
    (he : a.revision % 2 = 0) (hr : a.revision = b.revision) : a.payload = b.payload := by
  induction h with
  | refl => rfl
  | @cons a b c step rest ih =>
    have hab := step_revision_mono step
    have hbc := steps_revision_mono rest
    have heq : a.revision = b.revision := by omega
    exact (unchanged_even_step step he heq).trans (ih (by omega) (by omega))

-- @cpp snapshot_read: every field load is between the two revision loads.
theorem accepted_fields_coherent (before after : State Field Value)
    (observed : Field → State Field Value)
    (ordered : ∀ field, Steps before (observed field) ∧ Steps (observed field) after)
    (even : before.revision % 2 = 0) (same : before.revision = after.revision) :
    (fun field => (observed field).payload field) = before.payload := by
  funext field
  have first := steps_revision_mono (ordered field).1
  have last := steps_revision_mono (ordered field).2
  have eq : before.revision = (observed field).revision := by omega
  rw [← unchanged_even_steps (ordered field).1 even eq]

theorem accepted_computation_coherent {α : Type} (before after : State Field Value)
    (observed : Field → State Field Value)
    (ordered : ∀ field, Steps before (observed field) ∧ Steps (observed field) after)
    (even : before.revision % 2 = 0) (same : before.revision = after.revision)
    (compute : (Field → Value) → α) :
    compute (fun field => (observed field).payload field) = compute before.payload := by
  rw [accepted_fields_coherent before after observed ordered even same]

-- The reader can return an optional history lookup. `none` INSIDE a successful
-- snapshot is absent history, not contention, and does not cause another retry.
def attempt {α : Type} (before after : Nat) (value : α) : Option α :=
  if before % 2 = 0 ∧ before = after then some value else none

theorem returned_attempt_valid {α : Type} (before after : Nat) (value returned : α)
    (h : attempt before after value = some returned) :
    before % 2 = 0 ∧ before = after ∧ value = returned := by
  by_cases hv : before % 2 = 0 ∧ before = after
  · rw [attempt, if_pos hv] at h
    exact ⟨hv.1, hv.2, Option.some.inj h⟩
  · simp [attempt, hv] at h

theorem busy_retries {α : Type} (before after : Nat) (value : α)
    (odd : before % 2 = 1) : attempt before after value = none := by
  simp [attempt, show ¬before % 2 = 0 by omega]

theorem changed_retries {α : Type} (before after : Nat) (value : α)
    (changed : before ≠ after) : attempt before after value = none := by
  simp [attempt, changed]

theorem quiet_returns {α : Type} (revision : Nat) (value : α)
    (even : revision % 2 = 0) : attempt revision revision value = some value := by
  simp [attempt, even]

theorem absent_history_finishes (revision : Nat) (even : revision % 2 = 0) :
    attempt revision revision (none : Option Rat) = some none := quiet_returns _ _ even

-- @cpp snapshot_retry: any number of failed attempts precedes the first success.
-- A retry is reader-local; no writer transition requires reader progress.
def ReturnsAt {α : Type} (attempts : Nat → Option α) (n : Nat) (value : α) : Prop :=
  attempts n = some value ∧ ∀ k, k < n → attempts k = none

theorem eventual_quiet_attempt_returns {α : Type} (attempts : Nat → Option α)
    (progress : ∃ n, ∃ value, attempts n = some value) :
    ∃ n value, ReturnsAt attempts n value := by
  classical
  let n := Nat.find progress
  obtain ⟨value, hv⟩ := Nat.find_spec progress
  refine ⟨n, value, hv, ?_⟩
  intro k hk
  cases h : attempts k with
  | none => rfl
  | some v => exact False.elim ((Nat.find_min progress hk) ⟨v, h⟩)

-- @cpp publication: samples and G are written before the published end is stored.
-- The accepted end bounds sample reads, but does not freeze the live inverse.
structure Publication where
  completed : Nat
  published : Nat
  accepted : Nat

def Publication.Valid (s : Publication) : Prop :=
  s.accepted ≤ s.published ∧ s.published ≤ s.completed

def Publication.write (s : Publication) : Publication := {s with completed := s.completed + 1}
def Publication.publish (s : Publication) : Publication := {s with published := s.completed}
def Publication.accept (s : Publication) (captured : Nat) : Publication :=
  {s with accepted := captured}

theorem write_preserves_publication (s : Publication) (h : s.Valid) : s.write.Valid := by
  simp only [Publication.Valid, Publication.write] at *
  omega

theorem publish_preserves_publication (s : Publication) (h : s.Valid) : s.publish.Valid := by
  simp only [Publication.Valid, Publication.publish] at *
  omega

theorem accept_preserves_publication (s : Publication) (h : s.Valid) (captured : Nat)
    (bound : captured ≤ s.published) : (s.accept captured).Valid := by
  exact ⟨bound, h.2⟩

theorem accepted_sample_completed (s : Publication) (h : s.Valid) (index : Nat)
    (within : index < s.accepted) : index < s.completed := by
  obtain ⟨_, _⟩ := h
  omega

-- Once the scalar loads are one committed snapshot, the existing history
-- contract applies. A future replacement can be absent even without contention.
theorem coherent_history_read (s : History) (valid : s.Valid) (q cutoff cursor t : Rat)
    (read : s.readAt q cutoff cursor = some t) :
    s.forward t = q ∧ t ≤ cutoff ∧ q ≤ cursor :=
  captured_read_sound s q cutoff cursor t valid read

end PhaseTime.Concurrency
