namespace DSP.Codegen

/-- Local scratch storage. A slot has a value only where `initialized` holds. -/
structure Scratch (α : Type) (capacity : Nat) (initialized : Nat → Prop) where
  private mk ::
  private slot : (i : Fin capacity) → initialized i.val → α

/-- Fill the active prefix of a fixed-capacity scratch buffer. The suffix has
    no values to initialize and no indices that can be read. -/
def Scratch.ofFn {α : Type} (capacity count : Nat) (_fits : count ≤ capacity)
    (fill : Fin count → α) : Scratch α capacity (fun i => i < count) :=
  ⟨fun i ready => fill ⟨i.val, ready⟩⟩

def Scratch.get {α : Type} {capacity : Nat} {initialized : Nat → Prop}
    (scratch : Scratch α capacity initialized) (i : Fin capacity) (ready : initialized i.val) : α :=
  scratch.slot i ready

@[simp] theorem Scratch.get_ofFn {α : Type} {capacity count : Nat} (fits : count ≤ capacity)
    (fill : Fin count → α) (i : Fin capacity) (ready : i.val < count) :
    (Scratch.ofFn capacity count fits fill).get i ready = fill ⟨i.val, ready⟩ :=
  rfl

end DSP.Codegen
