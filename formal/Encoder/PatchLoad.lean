import Encoder.ResetEvaluation
import Encoder.Motion

namespace Encoder.PatchLoad

/-! Patch loading is a distinct transition, not a knob edit. Values below are
normalized decoded targets; JSON parsing and float roundoff are outside the model.
The historical loader kept the root but replaced every descendant with a fresh
neutral cell before setting saved targets. This counterexample needs only one
modulation-depth child and no gestures.
-/
def writeTargets (t : Timing) (targets : Scene → Rat) (scenes : List Scene) (s : Cell) : Cell :=
  scenes.foldl (fun cell scene => setTarget t cell scene (targets scene)) s

def loadCell (t : Timing) (targets : Scene → Rat) (s : Cell) : Cell :=
  {writeTargets t targets (List.finRange 8) s with force := t.maximum}

-- @head patch_load: the pinned loader discards the old depth child.
def replaceDepth (t : Timing) (targets : Scene → Rat) : Cell :=
  loadCell t targets (initialFrom t (1/2))

theorem writeTargets_values (t : Timing) (targets : Scene → Rat) (scenes : List Scene) (s : Cell) :
    (writeTargets t targets scenes s).values = s.values := by
  induction scenes generalizing s with
  | nil => rfl
  | cons i rest ih =>
    simp only [writeTargets, List.foldl_cons]
    exact (ih _).trans (target_changes_do_not_jump t s i (targets i))

theorem writeTargets_same (t : Timing) (scenes : List Scene) (s : Cell) :
    writeTargets t s.targets scenes s = s := by
  induction scenes with
  | nil => rfl
  | cons i rest ih => simpa [writeTargets, unchanged_target_noop] using ih

theorem replacement_discards_current (t : Timing) (targets : Scene → Rat) :
    (replaceDepth t targets).values = fun _ => 1/2 := by
  exact writeTargets_values t targets _ _

-- The counterexample applies to any depth curve with the intended endpoints;
-- it is independent of how the exponential is implemented between them.
theorem old_reload_counterexample (t : Timing) (curve : Rat → Rat)
    (zero : curve 0 = 0) (one : curve 1 = 1) (scene : Scene) :
    mix (3/10) [⟨curve (2 * (initialFrom t 1).values scene - 1), 1, 1⟩] = 1 ∧
    mix (3/10) [⟨curve (2 * (replaceDepth t (fun _ => 1)).values scene - 1), 1, 1⟩] = 3/10 := by
  rw [replacement_discards_current]
  norm_num [initialFrom, zero, one, mix, accumulate, absRat, maxRat]

-- Slots are serialized indices, not compact active-list positions. Missing
-- entries remain explicit, so surviving paths are matched even across holes.
-- A node's children comprise its modulator and gesture slots. Activation flags
-- and constructor neutral values are retained as metadata; no pointer identity
-- or allocator behavior is needed to state the continuity property.
inductive Tree (α : Type) where
  | missing
  | node (data : α) (slots : List (Tree α))

structure Live where
  cell : Cell
  active : Scene → Bool
  neutral : Rat

structure Saved where
  targets : Scene → Rat
  active : Scene → Bool
  neutral : Rat

def save : Tree Live → Tree Saved
  | .missing => .missing
  | .node n slots => .node ⟨n.cell.targets, n.active, n.neutral⟩ (slots.map save)

def oldCell (t : Timing) (neutral : Rat) : Tree Live → Cell
  | .missing => initialFrom t neutral
  | .node n _ => n.cell

def oldSlots : Tree Live → List (Tree Live)
  | .missing => []
  | .node _ slots => slots

-- @cpp patch_load: FromJSON reuses the cell at the same serialized slot and
-- allocates only missing slots. Missing patch slots delete old subtrees.
mutual
  def load (t : Timing) : Tree Saved → Tree Live → Tree Live
    | .missing, _ => .missing
    | .node p slots, old =>
      .node ⟨loadCell t p.targets (oldCell t p.neutral old), p.active, p.neutral⟩
        (loadSlots t slots (oldSlots old))
  termination_by structural patch => patch

  def loadSlots (t : Timing) : List (Tree Saved) → List (Tree Live) → List (Tree Live)
    | [], _ => []
    | p :: ps, [] => load t p .missing :: loadSlots t ps []
    | p :: ps, o :: os => load t p o :: loadSlots t ps os
  termination_by structural patches => patches
end

def refresh (t : Timing) : Tree Live → Tree Live
  | .missing => .missing
  | .node n slots => .node {n with cell := {n.cell with force := t.maximum}} (slots.map (refresh t))

-- Reloading the saved targets preserves every cell, including a pending ramp;
-- only the force-refresh countdown is restarted.
theorem reload_preserves_state (t : Timing) (tree : Tree Live) :
    load t (save tree) tree = refresh t tree := by
  apply Tree.rec (motive_1 := fun tree => load t (save tree) tree = refresh t tree)
    (motive_2 := fun slots => loadSlots t (slots.map save) slots = slots.map (refresh t))
    (by simp [save, load, refresh]) ?_ (by rfl) ?_ tree
  · intro n slots ih
    simp only [save, load, oldCell, oldSlots, ih, loadCell, writeTargets_same, refresh]
  · intro n ns hn hns
    simp only [List.map_cons, loadSlots, hn, hns]

structure Observable where
  values : Scene → Rat
  active : Scene → Bool

def observe : Tree Live → Tree Observable
  | .missing => .missing
  | .node n slots => .node ⟨n.cell.values, n.active⟩ (slots.map observe)

def advance (t : Timing) (frames : Nat) : Tree Live → Tree Live
  | .missing => .missing
  | .node n slots => .node {n with cell := run t frames n.cell} (slots.map (advance t frames))

def Settled (t : Timing) : Tree Live → Prop
  | .missing => True
  | .node n slots => n.cell.values = n.cell.targets ∧ n.cell.remaining = 0 ∧
      n.cell.age = t.maximum ∧ ∀ child ∈ slots, Settled t child

theorem observe_refresh (t : Timing) (tree : Tree Live) :
    observe (refresh t tree) = observe tree := by
  apply Tree.rec (motive_1 := fun tree => observe (refresh t tree) = observe tree)
    (motive_2 := fun slots => (slots.map (refresh t)).map observe = slots.map observe)
    (by simp [observe, refresh]) ?_ (by rfl) ?_ tree
  · intro n slots ih
    simp only [refresh, observe, ih]
  · intro n ns hn hns
    simp only [List.map_cons, hn, hns]

theorem reload_preserves_observation (t : Timing) (tree : Tree Live) :
    observe (load t (save tree) tree) = observe tree := by
  rw [reload_preserves_state, observe_refresh]

theorem settled_refresh_trajectory (t : Timing) (frames : Nat) (tree : Tree Live)
    (h : Settled t tree) : observe (advance t frames (refresh t tree)) = observe tree := by
  apply Tree.rec
    (motive_1 := fun tree => Settled t tree → observe (advance t frames (refresh t tree)) = observe tree)
    (motive_2 := fun slots => (∀ child ∈ slots, Settled t child) →
      ((slots.map (refresh t)).map (advance t frames)).map observe = slots.map observe)
    (by intro _; simp [observe, advance, refresh]) ?_ (by intro _; rfl) ?_ tree h
  · intro n slots ih hn
    simp only [Settled] at hn
    have values := (Motion.stationary_run t {n.cell with force := t.maximum} frames hn.2.1 hn.2.2.1).1
    simp only [refresh, advance, observe, values, ih hn.2.2.2]
  · intro n ns hn hns hs
    simp only [List.map_cons, hn (hs n (by simp)), hns (fun c hc => hs c (by simp [hc]))]

-- Any pure recursive evaluator of current values/activation and unchanged
-- external inputs sees the same output for every subsequent host frame.
-- This also covers scene blending and nested modulation without committing to
-- a particular depth approximation. Cached output/history are tested in C++.
theorem reload_preserves_output_trajectory {Output : Type} (t : Timing) (frames : Nat)
    (tree : Tree Live) (evaluate : Tree Observable → Output) (h : Settled t tree) :
    evaluate (observe (advance t frames (load t (save tree) tree))) = evaluate (observe tree) := by
  rw [reload_preserves_state, settled_refresh_trajectory t frames tree h]

-- The exact-rational reading of the production exponential polynomial also
-- exhibits the excursion; no exact endpoint assumption is required here.
theorem old_polynomial_reload_counterexample (t : Timing) (scene : Scene) :
    mix (3/10) [⟨Curve.approximate (2 * (initialFrom t 1).values scene - 1), 1, 1⟩] > 9/10 ∧
    mix (3/10) [⟨Curve.approximate (2 * (replaceDepth t (fun _ => 1)).values scene - 1), 1, 1⟩] = 3/10 := by
  rw [replacement_discards_current]
  norm_num [initialFrom, Curve.approximate, Curve.magnitude, Curve.exp2Approx,
    Curve.log2Nine, Curve.polynomial, mix, accumulate, absRat, maxRat]

-- @cpp patch_decode: the production comparison is in the serialized domain,
-- so self-load is a no-op even when encode/decode are not exact inverses.
def readTarget (t : Timing) (encode decode : Rat → Rat) (s : Cell) (scene : Scene) (saved : Rat) : Cell :=
  if saved = encode (s.targets scene) then s else setTarget t s scene (decode saved)

theorem encoded_self_load (t : Timing) (encode decode : Rat → Rat) (s : Cell) (scene : Scene) :
    readTarget t encode decode s scene (encode (s.targets scene)) = s := by
  simp [readTarget]

theorem normalized_decode_agrees (t : Timing) (s : Cell) (scene : Scene) (value : Rat) :
    readTarget t id id s scene value = setTarget t s scene value := by
  unfold readTarget
  split
  · rename_i h
    simpa [h] using (unchanged_target_noop t s scene).symm
  · rfl

theorem matching_cell_keeps_current (t : Timing) (targets : Scene → Rat) (s : Cell) :
    (loadCell t targets s).values = s.values := writeTargets_values t targets _ s

theorem missing_patch_removes_path (t : Timing) (old : Tree Live) :
    load t .missing old = .missing := by simp [load]

theorem new_path_starts_at_neutral (t : Timing) (p : Saved) (slots : List (Tree Saved)) :
    (oldCell t p.neutral (load t (.node p slots) .missing)).values = fun _ => p.neutral := by
  simp only [load, oldCell, loadCell]
  exact writeTargets_values t p.targets _ _

end Encoder.PatchLoad
