import DSP.Filters.Responses

namespace DSP.Tests

/-- Distinct stage gains and dampings expose accidental stage/lane aliasing. -/
def responseValues {R : Type} [OfNat R 0] [OfNat R 1] [OfNat R 2] [Add R] [Sub R] [Neg R]
    [Mul R] [Div R] [FMA R] [IfLess R]
    (g1 g2 g3 g4 k1 k2 _k3 low band high : R) (f : ResponseFrequency R) : List R :=
  let svf : TPTSVF.Snapshot R := ⟨g1, k1⟩
  let pair : TPTSVF4Pole.Snapshot R := ⟨svf, ⟨g2, k2⟩⟩
  [OnePoleTPTLowPass.Snapshot.magnitudeSquared ⟨g1⟩ f,
   OnePoleTPTHighPass.Snapshot.magnitudeSquared ⟨g1⟩ f,
   TPTBaseWidth.Snapshot.magnitudeSquared ⟨⟨g1⟩, ⟨g2⟩⟩ f,
   svf.lowPassMagnitudeSquared f, svf.bandPassMagnitudeSquared f,
   svf.highPassMagnitudeSquared f, svf.notchMagnitudeSquared f, svf.allPassMagnitudeSquared f,
   MorphingTPTSVF.Snapshot.magnitudeSquared ⟨svf, low, band, high⟩ f,
   pair.lowPassMagnitudeSquared f, pair.bandPassMagnitudeSquared f, pair.highPassMagnitudeSquared f,
   TPTLadder.Snapshot.magnitudeSquared ⟨g1,g2,g3,g4,k1⟩ f]
end DSP.Tests
