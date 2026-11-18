import DSP.Nonlinear
import DSP.Step

namespace DSP
namespace NormalizedTanh

structure Coefficients (R : Type) where
  gain : R
  tanhGain : R

structure LogicalSpec (R : Type) where
  gain : R

/-- Gain must be positive. As in the app, zero gain is outside this API's domain. -/
def prepare {R : Type} [Tanh R] (spec : LogicalSpec R) : Coefficients R :=
  ⟨spec.gain, Tanh.tanh spec.gain⟩

def apply {R : Type} [Mul R] [Div R] [Tanh R] (c : Coefficients R) (x : R) : R :=
  Tanh.tanh (x * c.gain) / c.tanhGain

def slopeZero {R : Type} [Div R] (c : Coefficients R) : R := c.gain / c.tanhGain

end NormalizedTanh

structure NormalizedTanh (R : Type) where
  coefficients : NormalizedTanh.Coefficients R
  output : R

namespace NormalizedTanh
def step {R : Type} [Mul R] [Div R] [Tanh R]
    (state : NormalizedTanh R) (input : Sample R) : NormalizedTanh R :=
  {state with output := apply state.coefficients input.x}

def computeCoefficients {R : Type} [Tanh R]
    (spec : LogicalSpec R) (state : NormalizedTanh R) : NormalizedTanh R :=
  {state with coefficients := prepare spec}

def reset {R : Type} [OfNat R 0] (state : NormalizedTanh R) : NormalizedTanh R :=
  {state with output := 0}

def stepFloat := step (R := Float)
def computeCoefficientsFloat := computeCoefficients (R := Float)
def resetFloat := reset (R := Float)
end NormalizedTanh
end DSP
