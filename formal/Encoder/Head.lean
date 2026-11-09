import Encoder.Core
import Encoder.Schedule

namespace Encoder.Head

-- Pinned Git object, never the main branch or a checkout of it.
def revision : String := "0a0d206529be45b6ff3ac82824c65ebe73b49003"

-- @head publish: EncoderBankBank::ProcessHostFrame directly fills each slot.
-- @cpp publish: Matrix::Publish interpolation after its new root clamp.
-- Equality below uses the same supplied Output; it excludes the new clamp.
def publish (o : Output) (host ratio : Nat) (bipolar : Bool) : Nat → Lane → Rat :=
  if o.stamp = host then
    fun slot lane =>
      let delta := o.previous lane - o.current lane
      let value := o.current lane + delta * previousWeight slot ratio
      if bipolar then value * 2 - 1 else value
  else fun _ lane => if bipolar then o.current lane * 2 - 1 else o.current lane

theorem publication_equals_working_tree (o : Output) (host ratio : Nat) (b : Bool) :
    publish o host ratio b = Encoder.publish o host ratio b := by
  funext slot lane
  unfold publish Encoder.publish readOutput toValue
  split <;> cases b <;> simp_all <;> grind

-- @head recursive: Modulators::Compute recursively visits child cells.
-- @cpp schedule: the working tree can evaluate the same pure graph in order.
def evaluate {α : Type} [Inhabited α] (p : Schedule.Program α) (root : Nat) : α :=
  Schedule.recursive p root

-- This includes arbitrary signed depth curves, gestures and nested modulators
-- as pure node operations. Correct graph construction is an explicit boundary.
theorem prepared_output_equals_head {α : Type} [Inhabited α]
    (p : Schedule.Program α) (seed : Nat → α) (count root : Nat) (h : root < count) :
    Schedule.prepared p seed count root = evaluate p root :=
  Schedule.prepared_equals_recursive p seed count root h

theorem prepared_publication_equals_head (p : Schedule.Program (Lane → Rat))
    (seed : Nat → Lane → Rat) (count root host ratio : Nat) (b : Bool) (old : Output)
    (h : root < count) :
    Encoder.publish (setOutput old host (Schedule.prepared p seed count root)) host ratio b =
      publish (setOutput old host (evaluate p root)) host ratio b := by
  rw [prepared_output_equals_head p seed count root h, publication_equals_working_tree]

end Encoder.Head
