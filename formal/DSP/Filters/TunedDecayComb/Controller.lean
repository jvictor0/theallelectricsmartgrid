import DSP.Filters.TunedDecayComb

namespace DSP.TunedDecayComb

/-- Scheduling of accepted coefficient changes. Parameter comparison and tuning
produce requests; requests received while fading coalesce to the latest one. -/
structure Controller (capacity : Nat) (R : Type) where
  state : State capacity R
  current : Coefficients capacity R
  next : Coefficients capacity R
  pending : Option (Coefficients capacity R)
  mix : R
  increment : R
  active : Bool

namespace Controller
variable {capacity : Nat} {R : Type}

def request (c : Controller capacity R) (desired : Coefficients capacity R) : Controller capacity R :=
  { c with pending := some desired }

def begin [OfNat R 0] (c : Controller capacity R) : Controller capacity R :=
  if c.active then c else
    match c.pending with
    | none => c
    | some desired =>
      { c with
        state := { c.state with second := c.state.first }
        next := desired
        pending := none
        mix := 0
        active := true }

def tick [Add R] [Sub R] [Mul R] [OfNat R 0] [OfNat R 1] [OfNat R 2]
    [LT R] [DecidableLT R] (c : Controller capacity R) (x : Sample R) : Controller capacity R :=
  let c := c.begin
  if c.active then
    let next := step c.current c.next (some c.mix) c.state x
    let mix := c.mix + c.increment
    if mix < 1 then { c with state := next, mix }
    else { c with state := { next with first := next.second }, current := c.next, mix := 0, active := false }
  else { c with state := step c.current c.next none c.state x }

def reset [OfNat R 0] (c : Controller capacity R) (desired : Coefficients capacity R) :
    Controller capacity R :=
  let zero : Branch R := ⟨0, 0, 0, 0⟩
  { c with
    state := ⟨DelayBuffer.empty (Nat.zero_lt_of_lt desired.age.isLt), zero, zero, 0⟩
    current := desired
    next := desired
    pending := none
    mix := 0
    active := false }

end Controller
end DSP.TunedDecayComb
