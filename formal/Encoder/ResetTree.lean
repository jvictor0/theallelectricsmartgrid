import Encoder.Reset
import Encoder.Curve
import Encoder.Head

namespace Encoder.Reset

/-! Logical tree before storage reclamation. Modulator children are bipolar;
gesture cells have the parent's polarity. Inactive/neutral cells may be retained
as ghost entries; the silence lemmas below justify their semantic removal.
Gesture subtrees do not enter the parent's formula, which uses banked values.
-/
structure GestureState where
  cell : Cell
  active : Scene → Bool
  weight : Rat
  recordedActivations : Nat
  deriving Inhabited

structure Node where
  cell : Cell
  gestures : List GestureState
  source : Source
  deriving Inhabited

abbrev Tree := Edits.Tree Node

-- Induction for arbitrary finite rose trees, including every existing child.
theorem treeInduction {P : Tree → Prop}
    (step : ∀ n children, (∀ child ∈ children, P child) → P (.node n children))
    (tree : Tree) : P tree := by
  apply Edits.Tree.rec (motive_1 := P) (motive_2 := fun cs => ∀ c ∈ cs, P c) step ?_ ?_ tree
  · simp
  · intro c cs hc hcs x hx
    rcases List.mem_cons.mp hx with rfl | hx
    · exact hc
    · exact hcs x hx

structure Scope where
  allScenes : Bool
  left : Scene
  right : Scene
  blend : Rat
  normalized : UnitInterval blend

def Scope.includes (scope : Scope) : Scene → Bool :=
  selected scope.allScenes scope.left scope.right scope.blend

def banked (scope : Scope) (values : Scene → Rat) : Rat :=
  Encoder.blend (values scope.left) (values scope.right) scope.blend

-- @cpp reset_gestures: ZeroModulators sets gesture targets and deactivates scope.
-- @head reset_gestures: identical writes and per-scene activation clearing.
def resetGesture (t : Timing) (scope : Scope) (bipolar : Bool) (g : GestureState) : GestureState :=
  {g with cell := writeScenes t scope.includes (Edits.neutral bipolar) (List.finRange 8) g.cell
          active := fun i => if scope.includes i then false else g.active i
          recordedActivations := g.recordedActivations + ((List.finRange 8).filter scope.includes).length}

theorem reset_gesture_records_activation (t : Timing) (scope : Scope) (b : Bool) (g : GestureState) :
    (resetGesture t scope b g).recordedActivations =
      g.recordedActivations + ((List.finRange 8).filter scope.includes).length := rfl

def resetNode (t : Timing) (scope : Scope) (bipolar : Bool) (v : Rat) (n : Node) : Node :=
  {n with cell := resetCell t scope.includes v n.cell
          gestures := n.gestures.map (resetGesture t scope bipolar)}

-- @cpp reset_tree: ZeroModulators recursively walks every allocated modulator.
-- @head reset_tree: same recursion, independent of the affecting bitmask.
def zeroTree (t : Timing) (scope : Scope) : Tree → Tree :=
  Edits.mapAll (resetNode t scope true (1/2))

def resetTree (t : Timing) (scope : Scope) (bipolar : Bool) (defaultValue : DefaultValue) : Tree → Tree
  | .node n children => .node (resetNode t scope bipolar defaultValue.value n) (children.map (zeroTree t scope))

-- Gesture current values need not be coherent: reset disables their contribution.
def NodeReady (t : Timing) (n : Node) : Prop := Ready t n.cell

def TreeReady (t : Timing) : Tree → Prop
  | .node n children => NodeReady t n ∧ ∀ child ∈ children, TreeReady t child

theorem resetGesture_ready (t : Timing) (scope : Scope) (b : Bool) (g : GestureState)
    (h : Ready t g.cell) : Ready t (resetGesture t scope b g).cell := by
  apply write_ready _ _ _ _ _ h
  cases b <;> unfold Edits.neutral UnitInterval <;> grind

theorem resetNode_ready (t : Timing) (scope : Scope) (b : Bool) (v : Rat) (n : Node)
    (h : NodeReady t n) (hv : UnitInterval v) : NodeReady t (resetNode t scope b v n) :=
  reset_ready t scope.includes v n.cell h hv

theorem zeroTree_ready (t : Timing) (scope : Scope) (tree : Tree) (h : TreeReady t tree) :
    TreeReady t (zeroTree t scope tree) := by
  revert h
  induction tree using treeInduction with
  | step n children ih =>
    intro h
    simp only [TreeReady] at h
    simp only [zeroTree, Edits.mapAll, TreeReady]
    refine ⟨resetNode_ready t scope true (1/2) n h.1 (by unfold UnitInterval; grind), ?_⟩
    intro child hc
    rcases List.mem_map.mp hc with ⟨old, ho, rfl⟩
    exact ih old ho (h.2 old ho)

theorem resetTree_ready (t : Timing) (scope : Scope) (b : Bool) (d : DefaultValue)
    (tree : Tree) (h : TreeReady t tree) : TreeReady t (resetTree t scope b d tree) := by
  cases tree with
  | node n cs =>
    simp only [TreeReady] at h
    simp only [resetTree, TreeReady]
    refine ⟨resetNode_ready t scope b d.value n h.1 d.normalized, ?_⟩
    intro child hc
    rcases List.mem_map.mp hc with ⟨old, ho, rfl⟩
    exact zeroTree_ready t scope old (h.2 old ho)

theorem reset_root_target (t : Timing) (scope : Scope) (b : Bool) (d : DefaultValue)
    (tree : Tree) (i : Scene) :
    (Edits.root (resetTree t scope b d tree)).cell.targets i =
      if scope.includes i then d.value else (Edits.root tree).cell.targets i := by
  cases tree
  exact reset_targets _ _ _ _ _

theorem reset_descendant (t : Timing) (scope : Scope) (b : Bool) (d : DefaultValue)
    (tree : Tree) (i : Nat) (path : List Nat) :
    Edits.atPath (resetTree t scope b d tree) (i::path) =
      (Edits.atPath tree (i::path)).map (resetNode t scope true (1/2)) := by
  cases tree with
  | node n cs =>
    simp only [resetTree, zeroTree, Edits.atPath, List.getElem?_map]
    cases h : cs[i]? <;> simp [Option.map, Option.bind, Edits.mapAll_atPath]

theorem reset_preserves_unselected (t : Timing) (scope : Scope) (b : Bool) (v : Rat)
    (n : Node) (i : Scene) (h : scope.includes i = false) :
    (resetNode t scope b v n).cell.targets i = n.cell.targets i ∧
    (resetNode t scope b v n).cell.values = n.cell.values ∧
    ∀ g ∈ n.gestures,
      (resetGesture t scope b g).cell.targets i = g.cell.targets i ∧
      (resetGesture t scope b g).active i = g.active i := by
  refine ⟨by simp [resetNode, reset_targets, h], reset_no_jump _ _ _ _, ?_⟩
  intro g _
  simp [resetGesture, write_targets, h]

-- @cpp reset_scope: selected scenes are exactly those carrying nonzero blend weight.
-- @head reset_scope: endpoint selection therefore permits inactive scenes to differ.
theorem banked_selected_constant (scope : Scope) (values : Scene → Rat) (v : Rat)
    (h : ∀ i, scope.includes i = true → values i = v) : banked scope values = v := by
  have hl := h scope.left
  have hr := h scope.right
  rcases scope.normalized with ⟨h0, h1⟩
  unfold banked Encoder.blend
  simp only [Scope.includes, selected, Bool.or_eq_true, decide_eq_true_eq] at hl hr
  by_cases ha : scope.allScenes = true
  · have l := hl (Or.inl ha)
    have r := hr (Or.inl ha)
    rw [l,r]; grind
  · by_cases hleft : scope.blend < 1
    · have l := hl (Or.inr (Or.inl ⟨trivial,hleft⟩))
      by_cases hright : 0 < scope.blend
      · have r := hr (Or.inr (Or.inr ⟨trivial,hright⟩)); rw [l,r]; grind
      · have hz : scope.blend = 0 := by grind
        rw [l,hz]; grind
    · have he : scope.blend = 1 := by grind
      have r := hr (Or.inr (Or.inr ⟨trivial, by grind⟩))
      rw [r,he]; grind

def effectiveWeight (scope : Scope) (g : GestureState) : Rat :=
  banked scope (fun i => if g.active i then g.weight else 0)

theorem reset_gesture_weight_zero (t : Timing) (scope : Scope) (b : Bool) (g : GestureState) :
    effectiveWeight scope (resetGesture t scope b g) = 0 := by
  apply banked_selected_constant
  intro i hi
  simp [resetGesture, hi]

-- @cpp compute: all visited cell ramps advance at host rate; targets stay fixed.
-- @head compute: identical per-cell advancing operation.
def advanceNode (t : Timing) (frames : Nat) (n : Node) : Node :=
  {n with cell := run t frames n.cell
          gestures := n.gestures.map (fun g => {g with cell := run t frames g.cell})}

def advanceTree (t : Timing) (frames : Nat) : Tree → Tree := Edits.mapAll (advanceNode t frames)

theorem reset_base_settles (t : Timing) (scope : Scope) (b : Bool) (v : Rat) (n : Node)
    (h : NodeReady t n) (hv : UnitInterval v) :
    banked scope (advanceNode t t.maximum (resetNode t scope b v n)).cell.values = v := by
  apply banked_selected_constant
  intro i hi
  simpa only [advanceNode, resetNode, hi, ↓reduceIte] using
    reset_finishes t scope.includes v n.cell h hv i

theorem advanced_reset_gestures_zero (t : Timing) (scope : Scope) (b : Bool) (v : Rat)
    (n : Node) (g : GestureState)
    (h : g ∈ (advanceNode t t.maximum (resetNode t scope b v n)).gestures) :
    effectiveWeight scope g = 0 := by
  rcases List.mem_map.mp h with ⟨old, ho, rfl⟩
  rcases List.mem_map.mp ho with ⟨original, _, rfl⟩
  exact reset_gesture_weight_zero t scope b original

-- @cpp reset_registry: full reset enumerates named owned cells; ResetGrid visits
-- only cells placed in the selected bank. Registry enumeration is a boundary.
-- @head reset_registry: unchanged enumeration and bank-local reset scope.
structure Entry where
  tree : Option Tree
  named : Bool
  placed : Nat → Bool
  bipolar : Bool
  defaultValue : DefaultValue

def resetEntry (t : Timing) (scope : Scope) (e : Entry) : Entry :=
  {e with tree := e.tree.map (resetTree t scope e.bipolar e.defaultValue)}

def resetRegistry {count : Nat} (t : Timing) (scope : Scope) (allParameters : Bool) (bank : Nat)
    (entries : Fin count → Entry) : Fin count → Entry := fun i =>
  let e := entries i
  if (if allParameters then e.named else e.placed bank) then resetEntry t scope e else e

theorem full_reset_reaches_hidden {count : Nat} (t : Timing) (scope : Scope) (entries : Fin count → Entry)
    (bank : Nat) (i : Fin count) (h : (entries i).named = true) :
    resetRegistry t scope true bank entries i = resetEntry t scope (entries i) := by
  simp [resetRegistry, h]

theorem bank_reset_preserves_unplaced {count : Nat} (t : Timing) (scope : Scope) (entries : Fin count → Entry)
    (bank : Nat) (i : Fin count) (h : (entries i).placed bank = false) :
    resetRegistry t scope false bank entries i = entries i := by simp [resetRegistry, h]

theorem bank_reset_reaches_placed {count : Nat} (t : Timing) (scope : Scope) (entries : Fin count → Entry)
    (bank : Nat) (i : Fin count) (h : (entries i).placed bank = true) :
    resetRegistry t scope false bank entries i = resetEntry t scope (entries i) := by
  simp [resetRegistry, h]

theorem reset_preserves_missing_root (t : Timing) (scope : Scope) (e : Entry) (h : e.tree = none) :
    (resetEntry t scope e).tree = none := by simp [resetEntry, h]

theorem reset_preserves_configured_default (t : Timing) (scope : Scope) (e : Entry) :
    (resetEntry t scope e).defaultValue = e.defaultValue := rfl

theorem reset_forces_every_descendant (t : Timing) (scope : Scope) (b : Bool) (d : DefaultValue)
    (tree : Tree) (i : Nat) (path : List Nat) (n : Node) (h : Edits.atPath tree (i::path) = some n) :
    Edits.atPath (resetTree t scope b d tree) (i::path) = some (resetNode t scope true (1/2) n) ∧
    (resetNode t scope true (1/2) n).cell.force = t.maximum := by
  rw [reset_descendant, h]
  exact ⟨rfl, rfl⟩

end Encoder.Reset
