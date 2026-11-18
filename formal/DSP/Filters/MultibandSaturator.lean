import DSP.Filters.LinkwitzRileySplitter.Prepared
import DSP.Nonlinear
import DSP.Channels.Arithmetic
import DSP.ChannelMean

/-! A pure multiband mastering core. Split with the shared compensated LR tree,
apply unnormalized saturation after each band gain, sum, then apply master gain
and saturation. Metering receives observations; it never participates in state
updates. SmallSignal and MonoBassSmallSignal prove the time-first response;
Response contains its magnitude-squared kernel. -/
namespace DSP.MultibandSaturator

structure LogicalSpec (numBands : Nat) (R : Type) where
  splitter : LinkwitzRileySplitter.LogicalSpec numBands R
  bandGain : Fin numBands → R
  masterGain : R

structure PreparedSpec (numBands : Nat) (R : Type) where
  splitter : LinkwitzRileySplitter.PreparedSpec numBands R
  bandGain : Fin numBands → R
  masterGain : R

structure State (numBands : Nat) (R : Type) where
  splitter : LinkwitzRileySplitter.State numBands R
  bandGain : Fin numBands → R
  masterGain : R

/-- Exact audio values at a saturation boundary. Reduction floors, smoothing,
host-frame accumulation and publication belong to the C++ meter. -/
structure SaturationObservation (R : Type) where
  before : R
  after : R

structure Output (numBands : Nat) (R : Type) where
  bands : Fin numBands → SaturationObservation R
  master : SaturationObservation R
  /-- Saturated mono bass, before master gain/saturation; zero in independent mode. -/
  sub : R

structure Snapshot (numBands : Nat) (R : Type) where
  splitter : LinkwitzRileySplitter.Snapshot numBands R
  bandGain : Fin numBands → R
  masterGain : R

variable {R : Type} {numBands : Nat}

def zero [OfNat R 0] : State numBands R :=
  ⟨LinkwitzRileySplitter.zero numBands, fun _ => 0, 0⟩

def reset [OfNat R 0] (state : State numBands R) : State numBands R :=
  {state with splitter := LinkwitzRileySplitter.reset state.splitter}

/-- Cutoffs are positive and below half the processing sample rate. Gains are
linear amplitudes, including zero for a muted band. No app knob mapping occurs here. -/
def prepare [OfNat R 0] [OfNat R 1] [OfNat R 8]
    [Add R] [Mul R] [Div R] [FMA R] [Trig R]
    (spec : LogicalSpec numBands R) : PreparedSpec numBands R :=
  ⟨LinkwitzRileySplitter.prepare spec.splitter,
    (StateSpace.snapshot numBands spec.bandGain).values, spec.masterGain⟩

def installPrepared (spec : PreparedSpec numBands R) (state : State numBands R) : State numBands R :=
  ⟨LinkwitzRileySplitter.installPrepared spec.splitter state.splitter,
    spec.bandGain, spec.masterGain⟩

def PreparedSpec.snapshot (spec : PreparedSpec numBands R) : Snapshot numBands R :=
  ⟨spec.splitter.snapshot, spec.bandGain, spec.masterGain⟩

def computeCoefficients [OfNat R 0] [OfNat R 1] [OfNat R 8]
    [Add R] [Mul R] [Div R] [FMA R] [Trig R]
    (spec : LogicalSpec numBands R) (state : State numBands R) : State numBands R :=
  installPrepared (prepare spec) state

/-- Audio coefficients and their response snapshot come from one preparation. -/
def prepareCoefficients [OfNat R 0] [OfNat R 1] [OfNat R 8]
    [Add R] [Mul R] [Div R] [FMA R] [Trig R]
    (spec : LogicalSpec numBands R) (state : State numBands R) :
    State numBands R × Snapshot numBands R :=
  let prepared := prepare spec
  (installPrepared prepared state, prepared.snapshot)

/-- Ascending left-to-right sum, starting at zero like the app's band accumulator.
The finite recursion specializes to straight-line arithmetic at a fixed width. -/
abbrev sum [OfNat R 0] [Add R] : {n : Nat} → (Fin n → R) → R := ChannelMean.sum

section Processing
variable [NeZero numBands] [OfNat R 0] [OfNat R 2]
  [Add R] [Sub R] [Neg R] [Mul R] [FMA R]

/-- Routing and saturation are explicit pure functions, allowing the later
linearization to reuse this exact program. Mono mode averages raw bass before
band gain, and adds its saturated value after the higher bands, as in the app. -/
def stepWith (monoBass : Bool) (mixBass saturate : R → R)
    (state : State numBands R) (input : Sample R) : State numBands R × Output numBands R :=
  let splitter := LinkwitzRileySplitter.step state.splitter input
  let bands := StateSpace.snapshot numBands fun i =>
    let raw := if i.val == 0 && monoBass then mixBass (splitter.bands i) else splitter.bands i
    let before := raw * state.bandGain i
    (⟨before, saturate before⟩ : SaturationObservation R)
  let combined := sum fun i => if i.val == 0 && monoBass then 0 else (bands.values i).after
  let sub := if monoBass then (bands.values 0).after else 0
  let beforeMaster := (combined + sub) * state.masterGain
  ({state with splitter}, ⟨bands.values, ⟨beforeMaster, saturate beforeMaster⟩, sub⟩)

/-- Independent audio channels: pointwise arithmetic and no cross-channel mix. -/
def step [Tanh R] : State numBands R → Sample R → State numBands R × Output numBands R :=
  stepWith false id Tanh.tanh

end Processing

/-- Average all audio channels and broadcast. Coefficient preparation can use
a different lane count; cutoff lanes never pass through this audio operation. -/
abbrev meanChannels {numChannels : Nat} [NeZero numChannels]
    [OfNat R 0] [OfNat R numChannels] [Add R] [Div R]
    (values : Fin numChannels → R) : Fin numChannels → R := ChannelMean.channels values

/-- App mono-bass mode uses shared band gains across channels. With that
contract the bass observation and sub are identical in every lane. The upper
bands and master saturation still process each audio channel independently. -/
def stepMonoBass {numChannels : Nat} [NeZero numChannels] [NeZero numBands]
    [OfNat R 0] [OfNat R 2] [OfNat R numChannels]
    [Add R] [Sub R] [Neg R] [Mul R] [Div R] [FMA R] [Tanh R]
    (state : State numBands (Fin numChannels → R)) (input : Sample (Fin numChannels → R)) :
    State numBands (Fin numChannels → R) × Output numBands (Fin numChannels → R) :=
  stepWith true meanChannels Tanh.tanh state input

end DSP.MultibandSaturator
