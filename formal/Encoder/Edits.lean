import Encoder.Evaluation
import Mathlib.Tactic.Linarith
import Mathlib.Tactic.SplitIfs

namespace Encoder.Edits

-- @cpp combine: StateEncoderCell::Combine; neutral is normalized zero.
-- @head combine: the committed implementation is identical.
def neutral (bipolar : Bool) : Rat := if bipolar then 1/2 else 0

def combine (bipolar towardsZero : Bool) (value delta : Rat) : Rat :=
  if towardsZero then
    if value = neutral bipolar then value else
    let d := if toValue bipolar value < 0 then -delta else delta
    if toValue bipolar value * toValue bipolar (value+d) ≤ 0 then neutral bipolar
    else clamp01 (value+d)
  else clamp01 (value+delta)

theorem neutral_absorbs_shift (b : Bool) (d : Rat) :
    combine b true (neutral b) d = neutral b := by simp [combine]

theorem ordinary_turn (b : Bool) (v d : Rat) :
    combine b false v d = clamp01 (v+d) := rfl

theorem combine_bounded (b z : Bool) (v d : Rat) :
    UnitInterval (combine b z v d) := by
  have hn : UnitInterval (neutral b) := by cases b <;> unfold neutral UnitInterval <;> grind
  cases z with
  | false => exact clamp01_bounded _
  | true =>
    by_cases he : v = neutral b
    · simp only [combine, he, ↓reduceIte]; exact hn
    · simp only [combine, he, ↓reduceIte]
      generalize (if toValue b v < 0 then -d else d) = directed
      split
      · exact hn
      · exact clamp01_bounded _

-- @cpp recording: a single scene write in IncrementInternal.
-- @head recording: identical SetAndRecordValue transition.
def turn (timing : Timing) (s : Cell) (scene : Scene) (b z : Bool) (delta : Rat) : Cell :=
  if delta = 0 then s else edit timing s scene (combine b z (s.targets scene) delta)

theorem turn_valid (timing : Timing) (s : Cell) (scene : Scene) (b z : Bool) (delta : Rat)
    (hs : Valid timing s) : Valid timing (turn timing s scene b z delta) := by
  unfold turn
  split
  · exact hs
  · exact edit_valid timing s scene _ hs (combine_bounded b z _ delta)

-- Shift-turn preserves the side of bipolar zero, including overshoot and clamps.
theorem shift_preserves_sign (v d : Rat) :
    (v ≤ 1/2 → combine true true v d ≤ 1/2) ∧
    (1/2 ≤ v → 1/2 ≤ combine true true v d) := by
  unfold combine neutral toValue clamp01
  simp only [↓reduceIte]
  split_ifs <;> constructor <;> intro h <;> nlinarith

-- Counterclockwise delta decreases magnitude; clockwise can increase it.
-- The method name "TowardsZero" does not mean every signed delta shrinks.
theorem negative_delta_towards_zero (v d : Rat) (hd : d ≤ 0) :
    absRat (toValue true (combine true true v d)) ≤ absRat (toValue true v) := by
  unfold combine neutral toValue clamp01 absRat
  simp only [↓reduceIte]
  split_ifs <;> nlinarith

theorem positive_delta_away_from_zero (v d : Rat) (hv : UnitInterval v) (hd : 0 ≤ d) :
    absRat (toValue true v) ≤ absRat (toValue true (combine true true v d)) := by
  unfold combine neutral toValue clamp01 absRat
  simp only [↓reduceIte]
  split_ifs <;> nlinarith [hv.1, hv.2]

theorem crossing_clamps_to_zero (b : Bool) (v d : Rat)
    (h : toValue b v * toValue b (v + (if toValue b v < 0 then -d else d)) ≤ 0) :
    combine b true v d = neutral b := by
  simp [combine, h]

-- @cpp scene_edit: the Shift branch performs sequential scene target writes.
-- @head scene_edit: HEAD also edits targets, not the interpolating values.
def shiftedScenes (b : Bool) (values : Scene → Rat) (left right : Scene) (t d : Rat) : Scene → Rat :=
  if d = 0 then values
  else if t ≤ 0 then Function.update values left (combine b true (values left) d)
  else if 1 ≤ t then Function.update values right (combine b true (values right) d)
  else
    let first := Function.update values left (combine b true (values left) (d*(1-t)))
    Function.update first right (combine b true (first right) (d*t))

theorem shift_scene_endpoints (b : Bool) (v : Scene → Rat) (l r : Scene) (d : Rat) (hd : d ≠ 0) :
    shiftedScenes b v l r 0 d = Function.update v l (combine b true (v l) d) ∧
    shiftedScenes b v l r 1 d = Function.update v r (combine b true (v r) d) := by
  simp [shiftedScenes, hd]

theorem shift_distinct_scenes (b : Bool) (v : Scene → Rat) (l r : Scene) (t d : Rat)
    (different : l ≠ r) (ht : 0 < t ∧ t < 1) (hd : d ≠ 0) :
    shiftedScenes b v l r t d l = combine b true (v l) (d*(1-t)) ∧
    shiftedScenes b v l r t d r = combine b true (v r) (d*t) := by
  simp [shiftedScenes, hd, show ¬t ≤ 0 by grind, show ¬1 ≤ t by grind,
    Function.update, different, Ne.symm different]

-- @cpp shift: BankedEncoderCell::Increment and recursive modulator traversal.
-- @head shift: same dispatch and traversal, without deferred-state synchronization.
def redirectsToModulators (gestureParam towardsZero shift : Bool) : Bool :=
  !gestureParam && !towardsZero && shift

theorem shift_dispatch : redirectsToModulators false false true = true ∧
    redirectsToModulators true false true = false ∧
    redirectsToModulators false true true = false := by decide

-- A finite rose tree represents existing modulator children only. Gesture edits
-- live inside the per-node edit function; gestures are not recursion edges here.
inductive Tree (α : Type) where
  | node (value : α) (children : List (Tree α))

def mapAll {α : Type} (edit : α → α) : Tree α → Tree α
  | .node v children => .node (edit v) (children.map (mapAll edit))

def shiftDescendants {α : Type} (edit : α → α) : Tree α → Tree α
  | .node v children => .node v (children.map (mapAll edit))

def root {α : Type} : Tree α → α | .node v _ => v

def atPath {α : Type} : Tree α → List Nat → Option α
  | .node v _, [] => some v
  | .node _ children, i::path => (children[i]?).bind (fun child => atPath child path)

theorem mapAll_atPath {α : Type} (edit : α → α) (path : List Nat) (tree : Tree α) :
    atPath (mapAll edit tree) path = (atPath tree path).map edit := by
  induction path generalizing tree with
  | nil => cases tree; simp [mapAll, atPath]
  | cons i path ih =>
    cases tree with
    | node v cs =>
      simp only [mapAll, atPath, List.getElem?_map]
      cases h : cs[i]? <;> simp [Option.map, Option.bind, ih]

theorem shift_keeps_root {α : Type} (edit : α → α) (tree : Tree α) :
    root (shiftDescendants edit tree) = root tree := by cases tree; rfl

theorem shift_edits_every_descendant {α : Type} (edit : α → α) (tree : Tree α)
    (i : Nat) (path : List Nat) :
    atPath (shiftDescendants edit tree) (i::path) = (atPath tree (i::path)).map edit := by
  cases tree with
  | node v cs =>
    simp only [shiftDescendants, atPath, List.getElem?_map]
    cases h : cs[i]? <;> simp [Option.map, Option.bind, mapAll_atPath]

theorem shift_preserves_missing {α : Type} (edit : α → α) (tree : Tree α)
    (i : Nat) (path : List Nat) (h : atPath tree (i::path) = none) :
    atPath (shiftDescendants edit tree) (i::path) = none := by
  rw [shift_edits_every_descendant, h]
  rfl

-- @cpp gesture_edit: Increment distributes a turn over effective gesture weights.
-- @head gesture_edit: identical quadratic weighting and base remainder.
def gestureDelta (delta weight total : Rat) : Rat := delta*weight*weight/total

theorem gesture_delta_nonpositive (delta weight total : Rat)
    (hd : delta ≤ 0) (hw : 0 ≤ weight) (ht : 0 < total) :
    gestureDelta delta weight total ≤ 0 := by
  exact div_nonpos_of_nonpos_of_nonneg
    (mul_nonpos_of_nonpos_of_nonneg (mul_nonpos_of_nonpos_of_nonneg hd hw) hw) (le_of_lt ht)

theorem negative_gesture_turn_reduces (value delta weight total : Rat)
    (hd : delta ≤ 0) (hw : 0 ≤ weight) (ht : 0 < total) :
    absRat (toValue true (combine true true value (gestureDelta delta weight total))) ≤
      absRat (toValue true value) :=
  negative_delta_towards_zero value _ (gesture_delta_nonpositive delta weight total hd hw ht)

def baseWeight (weights : List Rat) : Rat := (weights.map (fun w => w*(1-w))).sum

theorem weight_partition (weights : List Rat) :
    (weights.map (fun w => w*w)).sum + baseWeight weights = weights.sum := by
  induction weights with
  | nil => simp [baseWeight]
  | cons w ws ih => simp only [baseWeight, List.map_cons, List.sum_cons] at *; grind

theorem gesture_edit_partition (delta : Rat) (weights : List Rat) (h : weights.sum ≠ 0) :
    delta * (weights.map (fun w => w*w)).sum / weights.sum +
      delta * baseWeight weights / weights.sum = delta := by
  have hp := weight_partition weights
  grind

theorem gesture_allocations_sum (delta total : Rat) (weights : List Rat) :
    (weights.map (fun w => gestureDelta delta w total)).sum =
      delta * (weights.map (fun w => w*w)).sum / total := by
  induction weights with
  | nil => simp
  | cons w ws ih =>
    simp only [List.map_cons, List.sum_cons]
    rw [ih]
    unfold gestureDelta
    grind

theorem actual_edit_partition (delta : Rat) (weights : List Rat) (h : weights.sum ≠ 0) :
    (weights.map (fun w => gestureDelta delta w weights.sum)).sum +
      delta * baseWeight weights / weights.sum = delta := by
  rw [gesture_allocations_sum]
  exact gesture_edit_partition delta weights h

end Encoder.Edits
