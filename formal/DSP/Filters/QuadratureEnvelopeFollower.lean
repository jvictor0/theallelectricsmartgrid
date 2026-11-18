import DSP.Filters.BandPassedEnvelopeFollower

namespace DSP

/-- Experimental single-SVF detector. BP and LP form an equal-amplitude
quadrature pair at the SVF center; their half squared norm is then constant.
This component is not exported or installed in the runtime ladder. -/
structure QuadratureEnvelopeFollower (R : Type) where
  svf : TPTSVF R
  power : R
  powerAlpha : R

namespace QuadratureEnvelopeFollower

abbrev LogicalSpec := BandPassedEnvelopeFollower.LogicalSpec

def computeCoefficients {R : Type} [OfNat R 1] [Add R] [Mul R] [Div R] [FMA R]
    (spec : LogicalSpec R) (s : QuadratureEnvelopeFollower R) : QuadratureEnvelopeFollower R :=
  {s with
    svf := (TPTSVF.prepareFromSnapshot ⟨spec.integratorGain, 1⟩ s.svf).1,
    powerAlpha := spec.powerAlpha}

def energy {R : Type} [OfNat R 2] [Add R] [Mul R] [Div R] (s : TPTSVF R) : R :=
  (s.bandPass * s.bandPass + s.lowPass * s.lowPass) / 2

def observe {R : Type} [OfNat R 2] [Add R] [Sub R] [Mul R] [Div R]
    (s : QuadratureEnvelopeFollower R) (svf : TPTSVF R) : QuadratureEnvelopeFollower R :=
  {s with svf, power := OnePoleZOHLowPass.relax s.powerAlpha (energy svf) s.power}

def step {R : Type} [OfNat R 2] [Add R] [Sub R] [Neg R] [Mul R] [Div R] [FMA R]
    (s : QuadratureEnvelopeFollower R) (input : Sample R) : QuadratureEnvelopeFollower R :=
  observe s (TPTSVF.step s.svf input)

def reset {R : Type} [OfNat R 0] (s : QuadratureEnvelopeFollower R) : QuadratureEnvelopeFollower R :=
  {s with svf := BandPassedEnvelopeFollower.resetSVF s.svf, power := 0}

end QuadratureEnvelopeFollower
end DSP
