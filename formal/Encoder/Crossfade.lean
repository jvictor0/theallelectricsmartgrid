import Encoder.Core

namespace Encoder.Crossfade

-- @cpp scene_flags: SceneManager::Process, including endpoint entry/exit.
-- @head scene_flags: shared arithmetic/control operation at the pinned revision.
structure Position where
  left : Scene
  right : Scene
  blend : Rat

def changed (old now : Position) : Prop :=
  old.left ≠ now.left ∨ old.right ≠ now.right ∨ old.blend ≠ now.blend

def changedScene (old now : Position) : Prop :=
  old.left ≠ now.left ∨ old.right ≠ now.right ∨
  (changed old now ∧ (now.blend = 0 ∨ now.blend = 1 ∨ old.blend = 0 ∨ old.blend = 1))

def blendOnly (old now : Position) : Prop := changed old now ∧ ¬ changedScene old now

theorem leaving_left_refreshes_tree (old now : Position)
    (atLeft : old.blend = 0) (moved : now.blend ≠ 0) :
    changed old now ∧ changedScene old now := by
  simp only [changedScene, changed]
  grind

theorem entering_left_refreshes_tree (old now : Position)
    (notLeft : old.blend ≠ 0) (atLeft : now.blend = 0) :
    changed old now ∧ changedScene old now := by
  simp only [changedScene, changed]
  grind

theorem no_blend_change_is_lost (old now : Position) (h : old.blend ≠ now.blend) :
    blendOnly old now ∨ (changed old now ∧ changedScene old now) := by
  simp only [blendOnly, changedScene, changed]
  grind

-- @cpp scene_edit: the unclamped ordinary IncrementInternal branch.
-- @head scene_edit: shared arithmetic/control operation at the pinned revision.
def incrementUnclamped (values : Scene → Rat) (left right : Scene) (t delta : Rat) : Scene → Rat :=
  let newLeft := values left + delta * (1-t)
  let newRight := values right + delta*t
  fun i => if i = right then newRight else if i = left then newLeft else values i

theorem distinct_scene_increment (values : Scene → Rat) (left right : Scene) (t delta : Rat)
    (distinct : left ≠ right) :
    blend (incrementUnclamped values left right t delta left)
      (incrementUnclamped values left right t delta right) t =
      blend (values left) (values right) t + delta*((1-t)*(1-t)+t*t) := by
  simp only [incrementUnclamped, distinct, ↓reduceIte]
  unfold blend
  grind

end Encoder.Crossfade
