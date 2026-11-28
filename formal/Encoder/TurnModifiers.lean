import Encoder.Edits
import Mathlib.Tactic.NormNum

namespace Encoder.TurnModifiers

open Encoder.Edits

inductive Route where
  | value
  | shift
  | spread
  | spreadDescendants
  deriving DecidableEq

-- @cpp spread_turn: spread is dispatched only at the incoming turn.
def route (gestureParam shift spread : Bool) : Route :=
  if gestureParam then .value
  else if spread then
    if shift then .spreadDescendants else .spread
  else if shift then .shift else .value

-- @cpp fine_tune: scale the decoded or accelerated delta before calling Increment.
def inputDelta (fineTune : Bool) (delta : Rat) : Rat :=
  if fineTune then delta / 100 else delta

def inputTurn (gestureParam shift spread fineTune : Bool) (delta : Rat) : Route × Rat :=
  (route gestureParam shift spread, inputDelta fineTune delta)

theorem fine_tune_before_routing (gestureParam shift spread : Bool) (delta : Rat) :
    inputTurn gestureParam shift spread false delta = (route gestureParam shift spread, delta) ∧
    inputTurn gestureParam shift spread true delta = (route gestureParam shift spread, delta / 100) :=
  ⟨rfl, rfl⟩

theorem spread_dispatch :
    route false false true = .spread ∧ route false true true = .spreadDescendants ∧
    route false true false = .shift ∧ route false false false = .value := by decide

theorem gesture_turn_bypasses_shortcuts (shift spread : Bool) :
    route true shift spread = .value := rfl

abbrev Slot := Fin 15
def spreadSlot : Slot := 11

-- Paths put the last slot first. `existing` describes used modulator routes
-- before the turn, excluding unused cells allocated for the selection grid.
abbrev Path := List Slot
abbrev Targets := Path → Option Rat

def spreadTarget (delta : Rat) (target : Option Rat) : Option Rat :=
  if delta = 0 then target
  else some (combine true false (target.getD (neutral true)) delta)

-- @cpp spread_descendants: postorder traversal through the existing modulator tree.
def applySpread (available descendants : Bool) (existing : Path → Bool) (targets : Targets)
    (delta : Rat) : Targets
  | [] => targets []
  | [slot] =>
    if available = true ∧ slot = spreadSlot ∧ descendants = false
    then spreadTarget delta (targets [slot])
    else targets [slot]
  | slot :: ownSlot :: rest =>
    if available = true ∧ slot = spreadSlot ∧ descendants = true ∧ ownSlot ≠ spreadSlot ∧
        existing (ownSlot :: rest) = true
    then spreadTarget delta (targets (slot :: ownSlot :: rest))
    else targets (slot :: ownSlot :: rest)

theorem unavailable_spread_is_noop (descendants : Bool) (existing : Path → Bool)
    (targets : Targets) (delta : Rat) (path : Path) :
    applySpread false descendants existing targets delta path = targets path := by
  cases path with
  | nil => rfl
  | cons slot parent => cases parent <;> simp [applySpread]

theorem spread_only_edits_direct_slot (existing : Path → Bool) (targets : Targets) (delta : Rat) :
    applySpread true false existing targets delta [spreadSlot] =
      spreadTarget delta (targets [spreadSlot]) ∧
    ∀ path, path ≠ [spreadSlot] →
      applySpread true false existing targets delta path = targets path := by
  constructor
  · simp [applySpread]
  · intro path h
    cases path with
    | nil => rfl
    | cons slot parent =>
      cases parent with
      | nil =>
        have hn : slot ≠ spreadSlot := by simpa using h
        simp [applySpread, hn]
      | cons ownSlot rest => simp [applySpread]

theorem spread_descendant_slot (existing : Path → Bool) (targets : Targets) (delta : Rat)
    (slot : Slot) (rest : Path) :
    applySpread true true existing targets delta (spreadSlot :: slot :: rest) =
      if slot ≠ spreadSlot ∧ existing (slot :: rest) = true then
        spreadTarget delta (targets (spreadSlot :: slot :: rest))
      else targets (spreadSlot :: slot :: rest) := by simp [applySpread]

theorem spread_descendants_preserve_other_slots (existing : Path → Bool) (targets : Targets)
    (delta : Rat) (slot : Slot) (parent : Path) (h : slot ≠ spreadSlot) :
    applySpread true true existing targets delta [] = targets [] ∧
    applySpread true true existing targets delta [spreadSlot] = targets [spreadSlot] ∧
    applySpread true true existing targets delta (slot :: parent) = targets (slot :: parent) := by
  cases parent <;> simp [applySpread, h]

theorem zero_delta_preserves_slot (target : Option Rat) :
    spreadTarget 0 target = target := by simp [spreadTarget]

theorem spread_creates_from_neutral (delta : Rat) (h : delta ≠ 0) :
    spreadTarget delta none = some (clamp01 (1/2 + delta)) := by
  simp [spreadTarget, h, combine, neutral]

theorem spread_adds_signed_delta (value delta : Rat) (h : delta ≠ 0) :
    spreadTarget delta (some value) = some (clamp01 (value + delta)) := by
  simp [spreadTarget, h, combine]

theorem spread_keeps_slot_at_zero :
    spreadTarget (-1/20) (some (11/20)) = some (1/2) ∧
    spreadTarget (-1/20) (some (1/2)) = some (9/20) ∧
    spreadTarget (1/10) (some (9/20)) = some (11/20) := by
  norm_num [spreadTarget, combine, clamp01]

end Encoder.TurnModifiers
