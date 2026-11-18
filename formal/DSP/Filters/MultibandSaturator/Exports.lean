import DSP.Filters.MultibandSaturator.Response

namespace DSP.MultibandSaturator

structure Input (R : Type) where
  x : R
  monoBass : Bool

def step4Band {R : Type} [OfNat R 0] [OfNat R 2]
    [Add R] [Sub R] [Neg R] [Mul R] [FMA R] [Tanh R] [ChannelMean R]
    (state : State 4 R) (input : Input R) : State 4 R × Output 4 R :=
  stepWith input.monoBass ChannelMean.mean Tanh.tanh state ⟨input.x⟩

def prepare4Band {R : Type} [OfNat R 0] [OfNat R 1] [OfNat R 8]
    [Add R] [Mul R] [Div R] [FMA R] [Trig R] := prepareCoefficients (numBands := 4) (R := R)

/-- Copy the prepared bank and capture its response without recomputing tuning. -/
def install4Band {R : Type} (spec : PreparedSpec 4 R) (state : State 4 R) : State 4 R × Snapshot 4 R :=
  (installPrepared spec state,spec.snapshot)

def reset4Band {R : Type} [OfNat R 0] := reset (numBands := 4) (R := R)

def magnitudeSquared4Band {R : Type} [OfNat R 0] [OfNat R 1]
    [Add R] [Mul R] [Div R] := Snapshot.magnitudeSquared (n := 4) (R := R)

end DSP.MultibandSaturator
