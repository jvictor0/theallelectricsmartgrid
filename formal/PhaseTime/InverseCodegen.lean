import DSP.Codegen.Cpp
import PhaseTime.InverseIndex

namespace PhaseTime.InverseRuntime

structure ReadInput where
  phase : Float
  cutoff : Float
  missing : Float

structure WriteInput where
  phase : Float
  time : Float

def WriteInput.observation (input : WriteInput) : Point Float := ⟨input.phase, input.time⟩

structure WriteInput.InRange {n : Nat} (input : WriteInput) (s : State Float Float32 n) : Prop where
  phase : PhaseInGrid (input.phase * s.gridResolution)
  previous : PhaseInGrid ((s.previous.getD input.observation).phase * s.gridResolution)
  highest : GridBound (s.highest.getD 0)
  capacity : GridBound (Int.ofNat n)
  /-- The native scale is a power of two at least one. A forward observation
      therefore has a positive native interpolation denominator. -/
  forwardWidth : Arithmetic.lt (s.previous.getD input.observation).phase input.phase = true →
    0 < input.phase * s.gridResolution - (s.previous.getD input.observation).phase * s.gridResolution

variable {n : Nat}

def readBoundedM (input : ReadInput) (range : PhaseInGrid input.phase)
    (decode : Float → Float32 → Option Float) : Action Float n (Option Float) :=
  let index := fun (_ : Unit) => gridFloor input.phase range
  readPayloadUsingM input.phase input.cutoff (fun _ => (index ()).val)
    (fun _ => (Int64.ofInt (gridNext (index ()))).toFloat) decode

/-- Every replacement in an accepted storage snapshot has bounded base and
    relative timestamps. The native adapter supplies the resulting finite
    decoding contract at the reader boundary. -/
def DecodablePayload (payload : Published Float Float32 n) : Prop :=
  ∀ slot : Fin (n + 1), ∀ base ∈ (payload.cells.get slot).base,
    ∀ branch ∈ (payload.cells.get slot).replacement,
      (decodeTimestamp base branch.start).isFinite = true ∧
      ∀ delta ∈ branch.stop, (decodeTimestamp base delta).isFinite = true

def beginGridBranchM (edge : Point Float) (index : GridIndex)
    (hindex : index.val = Arithmetic.floor edge.phase) (highest extra : GridIndex) (hasHighest : Bool) :
    Action Float n PUnit :=
  let retained := if hasHighest then
      if gridLowerEdge highest extra ≤ index.val then
        if index.val ≤ highest.val then true else false
      else false
    else false
  beginBranchUsingM edge index.val
    (prepareBranchUsingM edge index.val hindex (pure retained) (fun _ => (Int64.ofInt (gridNext index)).toFloat))

def writeGridForwardM (left right : Point Float) (resolution : Float)
    (leftBound : PhaseInGrid left.phase) (rightBound : PhaseInGrid right.phase)
    (_width : 0 < right.phase - left.phase)
    (highest extra : GridIndex) (hasHighest : Bool) : Action Float n PUnit := do
  openVisitUsingM (beginGridBranchM left (gridFloor left.phase leftBound) rfl highest extra hasHighest)
  let first := gridCeil left.phase leftBound
  let last := gridFloor right.phase rightBound
  boundedScatterM left right first last
  let newest := if hasHighest then (if highest.val ≤ last.val then last else highest) else last
  publishForwardM right newest.val ((Int64.ofInt (gridLowerEdge newest extra)).toFloat / resolution)

open Storage in
def writeBoundedM (input : WriteInput) (s : State Float Float32 n) (range : input.InRange s) :
    PUnit × State Float Float32 n :=
  let prior := (previous : Action Float n (Option (Point Float))).run s
  let resolution := (gridResolution : Action Float n Float).run prior.2
  let high := (highest : Action Float n (Option Int)).run resolution.2
  let extra := (capacityMinusOne : Action Float n Int).run high.2
  let observation := input.observation
  let old := prior.1.getD observation
  let left : Point Float := ⟨old.phase * resolution.1, old.time⟩
  let right : Point Float := ⟨observation.phase * resolution.1, observation.time⟩
  (writeObservedM observation old prior.1.isSome fun advances =>
    writeGridForwardM left right resolution.1 range.previous range.phase (range.forwardWidth advances)
      ⟨high.1.getD 0, range.highest⟩ ⟨extra.1, range.capacity⟩ high.1.isSome).run extra.2

-- Native storage binds the ring operations for its own capacity. The Lean
-- capacity index is used only by the logical interpretations of those bindings.
def readPayloadFloat (input : ReadInput) (range : PhaseInGrid input.phase)
    (s : State Float Float32 0) (_decodable : DecodablePayload s.payload) :
    Float × State Float Float32 0 :=
  ((·.getD input.missing) <$> readBoundedM input range
    (fun base delta => some (decodeTimestamp base delta))).run s

def writeFloat (input : WriteInput) (s : State Float Float32 0) (range : input.InRange s) :
    PUnit × State Float Float32 0 :=
  writeBoundedM input s range

end PhaseTime.InverseRuntime
