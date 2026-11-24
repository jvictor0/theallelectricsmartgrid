namespace DSP

/-- Run an action on a child of the state. The common emitter recognizes
matching structure projections and updates as a borrow of that field. -/
def focus {S T A : Type} (getChild : S → T) (setChild : S → T → S)
    (action : StateM T A) : StateM S A :=
  modifyGet fun parent =>
    let (result, child) := action.run (getChild parent)
    (result, setChild parent child)

/-- Fixed heterogeneous storage for a sequence of statically sized children. -/
def SizedFields (F : Nat → Type) : List Nat → Type
  | [] => PUnit
  | size :: rest => F size × SizedFields F rest

end DSP
