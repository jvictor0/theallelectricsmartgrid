import Encoder.GestureLifecycle

namespace Encoder.Gestures

-- A keyed view of the 16 optional gesture slots; allocator/index-array storage
-- is not modeled. Modulator children retain their existing order and topology.
structure Node where
  cell : Cell
  slots : Gesture → Option State
  source : Source

abbrev Tree := Edits.Tree Node

-- @cpp gesture_clear: recursively clear one gesture ID through every allocated child.
-- @head gesture_clear: same traversal independent of the active dependency mask.
def clearNode (selected : Gesture → Bool) (scenes : List Scene) (n : Node) : Node :=
  {n with slots := fun id => if selected id then (n.slots id).map (deactivate scenes) else n.slots id}

def clearTree (id : Gesture) (scenes : List Scene) : Tree → Tree :=
  Edits.mapAll (clearNode (fun j => decide (j = id)) scenes)

theorem clear_selected (selected : Gesture → Bool) (scenes : List Scene) (n : Node) (id : Gesture)
    (h : selected id = true) :
    (clearNode selected scenes n).slots id = (n.slots id).map (deactivate scenes) := by
  simp [clearNode,h]

theorem clear_unselected (selected : Gesture → Bool) (scenes : List Scene) (n : Node) (id : Gesture)
    (h : selected id = false) : (clearNode selected scenes n).slots id = n.slots id := by
  simp [clearNode,h]

theorem clear_missing (selected : Gesture → Bool) (scenes : List Scene) (n : Node) (id : Gesture)
    (h : n.slots id = none) : (clearNode selected scenes n).slots id = none := by
  simp [clearNode,h]

theorem clear_preserves_base_and_route (selected : Gesture → Bool) (scenes : List Scene) (n : Node) :
    (clearNode selected scenes n).cell = n.cell ∧ (clearNode selected scenes n).source = n.source :=
  ⟨rfl,rfl⟩

theorem clear_every_descendant (id : Gesture) (scenes : List Scene) (tree : Tree) (path : List Nat) :
    Edits.atPath (clearTree id scenes tree) path =
      (Edits.atPath tree path).map (clearNode (fun j => decide (j = id)) scenes) :=
  Edits.mapAll_atPath _ _ _

theorem clear_preserves_topology (id : Gesture) (scenes : List Scene) (tree : Tree) (path : List Nat) :
    (Edits.atPath (clearTree id scenes tree) path).isSome = (Edits.atPath tree path).isSome := by
  rw [clear_every_descendant]
  simp

-- @cpp gesture_shift_press: the selected-gesture branch only edits this node's
-- linked gestures. It does not recursively clear matching IDs in modulators.
-- @head gesture_shift_press: same local selection and ancestor forcing.
def shiftPressSelected (t : Timing) (selected : Gesture → Bool) (scenes : List Scene) : Tree → Tree
  | .node n cs => .node {clearNode selected scenes n with cell := {n.cell with force := t.maximum}} cs

theorem shift_press_keeps_descendants (t : Timing) (selected : Gesture → Bool) (scenes : List Scene)
    (tree : Tree) (i : Nat) (path : List Nat) :
    Edits.atPath (shiftPressSelected t selected scenes tree) (i::path) = Edits.atPath tree (i::path) := by
  cases tree
  rfl

theorem shift_press_keeps_base_targets (t : Timing) (selected : Gesture → Bool) (scenes : List Scene)
    (tree : Tree) :
    (Edits.root (shiftPressSelected t selected scenes tree)).cell.values = (Edits.root tree).cell.values ∧
    (Edits.root (shiftPressSelected t selected scenes tree)).cell.targets = (Edits.root tree).cell.targets ∧
    (Edits.root (shiftPressSelected t selected scenes tree)).cell.force = t.maximum := by
  cases tree
  exact ⟨rfl,rfl,rfl⟩

structure Entry where
  tree : Option Tree
  named : Bool
  visible : Bool
  placed : Nat → Bool

-- @cpp gesture_clear_registry: owner-level ClearGesture visits named roots, not pages.
-- @head gesture_clear_registry: same hidden-root coverage and force update.
def forceRoot (t : Timing) : Tree → Tree
  | .node n cs => .node {n with cell := {n.cell with force := t.maximum}} cs

def clearEntry (t : Timing) (id : Gesture) (scenes : List Scene) (e : Entry) : Entry :=
  if e.named then {e with tree := e.tree.map (fun tree => forceRoot t (clearTree id scenes tree))} else e

def clearRegistry {count : Nat} (t : Timing) (id : Gesture) (scenes : List Scene)
    (entries : Fin count → Entry) : Fin count → Entry := fun i => clearEntry t id scenes (entries i)

theorem clear_reaches_hidden (t : Timing) (id : Gesture) (scenes : List Scene) (e : Entry)
    (h : e.named = true) :
    (clearEntry t id scenes e).tree = e.tree.map (fun tree => forceRoot t (clearTree id scenes tree)) := by
  simp [clearEntry,h]

theorem clear_unnamed_unchanged (t : Timing) (id : Gesture) (scenes : List Scene) (e : Entry)
    (h : e.named = false) : clearEntry t id scenes e = e := by simp [clearEntry,h]

-- @cpp gesture_clear_bank: bank-local deletion visits only its placed base cells.
-- @head gesture_clear_bank: the bank walk and corrective forcing are unchanged.
def clearBank {count : Nat} (t : Timing) (id : Gesture) (scenes : List Scene) (bank : Nat)
    (entries : Fin count → Entry) : Fin count → Entry := fun i =>
  let e := entries i
  if e.placed bank then
    {e with tree := e.tree.map (fun tree => forceRoot t (clearTree id scenes tree))}
  else e

theorem clear_bank_preserves_unplaced {count : Nat} (t : Timing) (id : Gesture) (scenes : List Scene)
    (bank : Nat) (entries : Fin count → Entry) (i : Fin count) (h : (entries i).placed bank = false) :
    clearBank t id scenes bank entries i = entries i := by simp [clearBank,h]

theorem clear_bank_reaches_placed {count : Nat} (t : Timing) (id : Gesture) (scenes : List Scene)
    (bank : Nat) (entries : Fin count → Entry) (i : Fin count) (h : (entries i).placed bank = true) :
    (clearBank t id scenes bank entries i).tree =
      (entries i).tree.map (fun tree => forceRoot t (clearTree id scenes tree)) := by simp [clearBank,h]

theorem clear_forces_root (t : Timing) (id : Gesture) (scenes : List Scene) (tree : Tree) :
    (Edits.root (forceRoot t (clearTree id scenes tree))).cell.force = t.maximum := by
  cases tree
  simp [clearTree, Edits.mapAll, forceRoot, Edits.root]

theorem clear_is_dirty (t : Timing) (id : Gesture) (scenes : List Scene) (tree : Tree)
    (inputs : Inputs) :
    dirty false (Edits.root (forceRoot t (clearTree id scenes tree))).cell.force [] [] inputs inputs = true := by
  rw [clear_forces_root]
  simp [dirty, Nat.ne_of_gt (Nat.lt_of_lt_of_le t.positive t.ordered)]

end Encoder.Gestures
