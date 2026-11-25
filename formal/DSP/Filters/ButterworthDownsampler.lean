import DSP.Processor
import DSP.FMA

namespace DSP.ButterworthDownsampler

structure Coefficients (R : Type) where
  gain : R
  feedback1 : R
  feedback2 : R

structure Section (R : Type) where
  coefficients : Coefficients R
  delay1 : R
  delay2 : R

structure Sample (R : Type) where
  x : R

structure Cascade (S : Type) where
  first : S
  second : S

abbrev FourSections (S : Type) := Cascade (Cascade S)

def Cascade.process {S R : Type} (firstStep secondStep : Processor S (Sample R) (Sample R)) :
    Processor (Cascade S) (Sample R) (Sample R) := fun state input =>
  let (first, middle) := firstStep state.first input
  let (second, output) := secondStep state.second middle
  (⟨first, second⟩, output)

/-- The section numerator is `gain * (1 + z⁻¹)^2`. Feedback coefficients
store the negation of the denominator's nonconstant coefficients. -/
def process2 {R : Type} [Add R] [Mul R] [FMA R] :
    Processor (Section R) (Sample R) (Sample R) := fun state input =>
  let c := state.coefficients
  let scaled := c.gain * input.x
  let output := scaled + state.delay1
  let delay1 := FMA.fma c.feedback1 output (scaled + scaled + state.delay2)
  let delay2 := FMA.fma c.feedback2 output scaled
  ({ state with delay1, delay2 }, ⟨output⟩)

def process4 {R : Type} [Add R] [Mul R] [FMA R] :
    Processor (Cascade (Section R)) (Sample R) (Sample R) :=
  Cascade.process process2 process2

def process8 {R : Type} [Add R] [Mul R] [FMA R] :
    Processor (FourSections (Section R)) (Sample R) (Sample R) :=
  Cascade.process process4 process4

def Section.reset {R : Type} [OfNat R 0] (state : Section R) : Section R :=
  { state with delay1 := 0, delay2 := 0 }

def Cascade.map {S T : Type} (f : S → T) (state : Cascade S) : Cascade T :=
  ⟨f state.first, f state.second⟩

def reset2 {R : Type} [OfNat R 0] (state : Section R) : Section R × Sample R :=
  (state.reset, ⟨0⟩)

def reset4 {R : Type} [OfNat R 0] (state : Cascade (Section R)) : Cascade (Section R) × Sample R :=
  (state.map Section.reset, ⟨0⟩)

def reset8 {R : Type} [OfNat R 0] (state : FourSections (Section R)) :
    FourSections (Section R) × Sample R :=
  (state.map (Cascade.map Section.reset), ⟨0⟩)

/-- Bilinear transform of the unity-DC section with damping `1 / Q` and
prewarped cutoff `g = tan (π * cutoff)`. -/
def prepareSection {R : Type} [OfNat R 1] [OfNat R 2]
    [Add R] [Sub R] [Mul R] [Div R] (g damping : R) : Coefficients R :=
  let gg := g * g
  let kg := damping * g
  let a0 := 1 + kg + gg
  ⟨gg / a0, 2 * (1 - gg) / a0, (kg - 1 - gg) / a0⟩

def prepare2 {R : Type} [OfNat R 1] [OfNat R 2]
    [Add R] [Sub R] [Mul R] [Div R] [OfScientific R]
    (tan cos : R → R) (pi cutoff : R) : Coefficients R :=
  prepareSection (tan (pi * cutoff)) (2 * cos (0.25 * pi))

def prepare4 {R : Type} [OfNat R 1] [OfNat R 2]
    [Add R] [Sub R] [Mul R] [Div R] [OfScientific R]
    (tan cos : R → R) (pi cutoff : R) : Cascade (Coefficients R) :=
  let g := tan (pi * cutoff)
  ⟨prepareSection g (2 * cos (0.125 * pi)), prepareSection g (2 * cos (0.375 * pi))⟩

/-- Run the pole pairs in increasing Q, as in the native Butterworth cascade. -/
def prepare8 {R : Type} [OfNat R 1] [OfNat R 2]
    [Add R] [Sub R] [Mul R] [Div R] [OfScientific R]
    (tan cos : R → R) (pi cutoff : R) : FourSections (Coefficients R) :=
  let g := tan (pi * cutoff)
  let atAngle := fun fraction => prepareSection g (2 * cos (fraction * pi))
  ⟨⟨atAngle 0.0625, atAngle 0.1875⟩, ⟨atAngle 0.3125, atAngle 0.4375⟩⟩

-- The native constructor can run before the shared trigonometric tables are
-- initialized. Prepare once with libm in double, then round the coefficients
-- to the sample precision when installing them.
private def piFloat : Float := 3.1415926535897932384626433832795

def prepare2Float : Float → Coefficients Float := prepare2 Float.tan Float.cos piFloat

def prepare4Float : Float → Cascade (Coefficients Float) := prepare4 Float.tan Float.cos piFloat

def prepare8Float : Float → FourSections (Coefficients Float) := prepare8 Float.tan Float.cos piFloat

end DSP.ButterworthDownsampler
