import Encoder.Control

/-! Concrete ownership and handoff below the abstract prepared-root protocol.
Global change-detection history and group reconstruction history are distinct.
Gathering may precede reconstruction during a host pass, so only committing the
completed evaluation advances the reconstruction snapshot. Scene and gesture
inputs are held fixed here; Control/Prepared model their separate transitions.
-/
namespace Encoder.SourceSnapshots

structure Frame (Index : Type) where
  value : Index → Lane → Rat
  amplitude : Index → Lane → Rat

def changed {Index : Type} (before after : Frame Index) (i : Index) : Prop :=
  before.value i ≠ after.value i ∨ before.amplitude i ≠ after.amplitude i

abbrev Binding (sources groups : Nat) := Fin groups → Source → Option (Fin sources)

def project {sources : Nat} (binding : Source → Option (Fin sources))
    (frame : Frame (Fin sources)) : Frame Source where
  value slot := match binding slot with
    | some index => frame.value index
    | none => fun _ => 0
  amplitude slot := match binding slot with
    | some index => frame.amplitude index
    | none => fun _ => 1

structure Group where
  live : Frame Source
  saved : Frame Source
  changed : Source → Prop

structure Manager (sources groups : Nat) where
  live : Frame (Fin sources)
  compared : Frame (Fin sources)
  group : Fin groups → Group

-- @cpp changed: global comparisons are gathered once into each assigned slot.
def gather {sources groups : Nat} (binding : Binding sources groups)
    (s : Manager sources groups) : Manager sources groups :=
  { s with compared := s.live, group := fun g =>
      { s.group g with
        live := project (binding g) s.live
        changed := fun slot => match binding g slot with
          | some index => changed s.compared s.live index
          | none => False } }

theorem gathered_changes_match_local_comparison {sources groups : Nat}
    (binding : Binding sources groups) (s : Manager sources groups)
    (g : Fin groups) (slot : Source) :
    ((gather binding s).group g).changed slot ↔
      changed (project (binding g) s.compared) (project (binding g) s.live) slot := by
  cases h : binding g slot <;> simp [gather, project, changed, h]

theorem gather_preserves_reconstruction_snapshot {sources groups : Nat}
    (binding : Binding sources groups) (s : Manager sources groups) (g : Fin groups) :
    ((gather binding s).group g).saved = (s.group g).saved := rfl

-- @cpp changed: direct host-frame replay compares the group without committing it.
def gatherDirect (next : Frame Source) (g : Group) : Group :=
  { g with live := next, changed := fun slot => changed g.saved next slot }

theorem direct_and_global_change_flags_agree {sources groups : Nat}
    (binding : Binding sources groups) (s : Manager sources groups) (g : Fin groups)
    (previous : (s.group g).saved = project (binding g) s.compared) (slot : Source) :
    ((gather binding s).group g).changed slot ↔
      (gatherDirect (project (binding g) s.live) (s.group g)).changed slot := by
  simpa [gatherDirect, previous] using gathered_changes_match_local_comparison binding s g slot

-- @cpp host_gate: the host pass commits sources only after matrix evaluation.
def commit {sources groups : Nat} (s : Manager sources groups) : Manager sources groups :=
  { s with group := fun g => { s.group g with saved := (s.group g).live } }

theorem committed_snapshot_contains_values_and_amplitudes {sources groups : Nat}
    (binding : Binding sources groups) (s : Manager sources groups) (g : Fin groups) :
    ((commit (gather binding s)).group g).saved = project (binding g) s.live := rfl

theorem commit_preserves_change_flags {sources groups : Nat}
    (s : Manager sources groups) (g : Fin groups) :
    ((commit s).group g).changed = (s.group g).changed := rfl

structure Root where
  evaluated : Frame Source
  tree : Nat → Rat
  output : Lane → Rat
  deferred : Bool

-- @cpp prepared_process: the graph keeps child state deferred after evaluation.
def evaluatePrepared (evaluate : Frame Source → Nat → Rat)
    (rootOf : (Nat → Rat) → Lane → Rat) (g : Group) (r : Root) : Root :=
  { r with evaluated := g.live, output := rootOf (evaluate g.live), deferred := true }

-- @cpp synchronization: C++ reconstructs from the group's stored arrays.
def synchronize (evaluate : Frame Source → Nat → Rat) (g : Group) (r : Root) : Root :=
  if r.deferred then { r with tree := evaluate g.saved, deferred := false } else r

def Coherent (evaluate : Frame Source → Nat → Rat) (r : Root) : Prop :=
  r.deferred = false → r.tree = evaluate r.evaluated

theorem committed_host_then_synchronize_is_coherent {sources groups : Nat}
    (binding : Binding sources groups) (s : Manager sources groups) (g : Fin groups)
    (evaluate : Frame Source → Nat → Rat) (rootOf : (Nat → Rat) → Lane → Rat) (r : Root) :
    Coherent evaluate (synchronize evaluate ((commit (gather binding s)).group g)
      (evaluatePrepared evaluate rootOf ((gather binding s).group g) r)) := by
  simp [Coherent, synchronize, evaluatePrepared, commit, gather]

theorem pending_gather_uses_last_evaluated_inputs {sources groups : Nat}
    (binding : Binding sources groups) (s : Manager sources groups) (g : Fin groups)
    (evaluate : Frame Source → Nat → Rat) (r : Root)
    (saved : (s.group g).saved = r.evaluated) (pending : r.deferred = true) :
    (synchronize evaluate ((gather binding s).group g) r).tree = evaluate r.evaluated := by
  simp [synchronize, gather, pending, saved]

theorem synchronization_preserves_output (evaluate : Frame Source → Nat → Rat)
    (g : Group) (r : Root) : (synchronize evaluate g r).output = r.output := by
  unfold synchronize
  split <;> rfl

-- Model the landed omission: current sources advance, but saved arrays stay at
-- startup. The next reconstruction violates coherence even with correct flags.
def constantFrame (value amplitude : Rat) : Frame Source :=
  ⟨fun _ _ => value, fun _ _ => amplitude⟩

def missingCommitGroup : Group :=
  ⟨constantFrame (9/10) 1, constantFrame 0 1, fun _ => False⟩

def missingCommitRoot : Root :=
  ⟨constantFrame (9/10) 1, fun _ => 9/10, fun _ => 9/10, true⟩

theorem missing_commit_breaks_reconstruction :
    ¬ Coherent (fun frame _ => frame.value 0 0)
      (synchronize (fun frame _ => frame.value 0 0) missingCommitGroup missingCommitRoot) := by
  intro h
  have bad := congrFun (h rfl) 0
  norm_num [synchronize, missingCommitGroup, missingCommitRoot, constantFrame] at bad

theorem missing_amplitude_commit_breaks_reconstruction :
    ¬ Coherent (fun frame _ => frame.amplitude 0 0)
      (synchronize (fun frame _ => frame.amplitude 0 0)
        ⟨constantFrame 1 (1/4), constantFrame 1 1, fun _ => False⟩
        ⟨constantFrame 1 (1/4), fun _ => 1/4, fun _ => 1/4, true⟩) := by
  intro h
  have bad := congrFun (h rfl) 0
  norm_num [synchronize, constantFrame] at bad

end Encoder.SourceSnapshots
