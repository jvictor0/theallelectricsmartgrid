import DSP.Filters.Tuning

/-! Prepare processing coefficients and their response snapshot together. These
pairs retain the original tuning intermediates; capture never reconstructs g
from rounded solved coefficients. The state projection is proved equal to the
existing updater, so processing, proof, and UI preparation share a contract. -/
namespace DSP
namespace OnePoleZOHLowPass
def prepareCoefficients {R : Type} [OfNat R 1] [Sub R] [ExpDecay R]
    (spec : LogicalSpec R) (state : OnePoleZOHLowPass R) : OnePoleZOHLowPass R × Snapshot R :=
  let alpha := 1 - ExpDecay.expNegTwoPi spec.cutoff
  ({state with alpha}, ⟨alpha⟩)
def prepareCoefficientsFloat := prepareCoefficients (R := Float)
end OnePoleZOHLowPass

namespace OnePoleTPTLowPass
def prepareCoefficients {R : Type} [OfNat R 1] [Add R] [Div R] [Trig R]
    (spec : LogicalSpec R) (state : OnePoleTPTLowPass R) : OnePoleTPTLowPass R × Snapshot R :=
  let g := Trig.tanPi spec.cutoff
  ({state with G := g / (1+g)}, ⟨g⟩)
def prepareCoefficientsFloat := prepareCoefficients (R := Float)
end OnePoleTPTLowPass

namespace OnePoleTPTHighPass
def prepareCoefficients {R : Type} [OfNat R 1] [Add R] [Div R] [Trig R]
    (spec : LogicalSpec R) (state : OnePoleTPTHighPass R) : OnePoleTPTHighPass R × Snapshot R :=
  let lpPrepared := OnePoleTPTLowPass.prepareCoefficients ⟨spec.cutoff⟩ state.lowPass
  let lp := lpPrepared.1
  let snapshot := lpPrepared.2
  ({state with lowPass := lp}, ⟨snapshot.g⟩)
def prepareCoefficientsFloat := prepareCoefficients (R := Float)
end OnePoleTPTHighPass

namespace TPTBaseWidth
def prepareCoefficients {R : Type} [OfNat R 1] [Add R] [Div R] [Trig R]
    (spec : LogicalSpec R) (state : TPTBaseWidth R) : TPTBaseWidth R × Snapshot R :=
  let hpPrepared := OnePoleTPTHighPass.prepareCoefficients spec.highPass state.highPass
  let hp := hpPrepared.1
  let hs := hpPrepared.2
  let lpPrepared := OnePoleTPTLowPass.prepareCoefficients spec.lowPass state.lowPass
  let lp := lpPrepared.1
  let ls := lpPrepared.2
  (⟨hp,lp⟩, ⟨hs,ls⟩)
def prepareCoefficientsFloat := prepareCoefficients (R := Float)
end TPTBaseWidth

namespace TPTSVF
def prepareCoefficients {R : Type} [OfNat R 1] [Add R] [Mul R] [Div R] [FMA R] [Trig R]
    (spec : LogicalSpec R) (state : TPTSVF R) : TPTSVF R × Snapshot R :=
  let g := Trig.tanPi spec.cutoff
  let k := 1 / spec.q
  let a1 := 1 / FMA.fma g (g+k) 1
  let a2 := g*a1
  ({state with k,a1,a2,a3 := g*a2}, ⟨g,k⟩)
def prepareCoefficientsFloat := prepareCoefficients (R := Float)
end TPTSVF

namespace MorphingTPTSVF
def prepareCoefficients {R : Type} [OfNat R 0] [OfNat R 1] [OfNat R 2]
    [Add R] [Sub R] [Mul R] [Div R] [FMA R] [IfLess R] [Trig R]
    (spec : LogicalSpec R) (state : MorphingTPTSVF R) : MorphingTPTSVF R × Snapshot R :=
  let svfPrepared := TPTSVF.prepareCoefficients spec.svf state.svf
  let svf := svfPrepared.1
  let snapshot := svfPrepared.2
  let half := 1/2
  let phase := IfLess.ifLess half spec.blend ((spec.blend-half)/2) (spec.blend/2)
  let cosine := Trig.cos2pi phase
  let sine := Trig.sin2pi phase
  ({state with svf, blend := spec.blend, cosine, sine},
    ⟨snapshot, IfLess.ifLess half spec.blend 0 cosine,
      IfLess.ifLess half spec.blend cosine sine, IfLess.ifLess half spec.blend sine 0⟩)
def prepareCoefficientsFloat := prepareCoefficients (R := Float)
end MorphingTPTSVF

namespace TPTSVF4Pole
def prepareCoefficients {R : Type} [OfNat R 1] [Add R] [Mul R] [Div R] [FMA R] [Trig R]
    (spec : LogicalSpec R) (state : TPTSVF4Pole R) : TPTSVF4Pole R × Snapshot R :=
  let s1Prepared := TPTSVF.prepareCoefficients spec.stage1 state.stage1
  let s1 := s1Prepared.1
  let c1 := s1Prepared.2
  let s2Prepared := TPTSVF.prepareCoefficients spec.stage2 state.stage2
  let s2 := s2Prepared.1
  let c2 := s2Prepared.2
  (⟨s1,s2⟩,⟨c1,c2⟩)
def prepareCoefficientsFloat := prepareCoefficients (R := Float)
end TPTSVF4Pole

namespace TPTLadder
def prepareCoefficients {R : Type} [OfNat R 1] [Add R] [Div R] [Trig R]
    (spec : LogicalSpec R) (state : TPTLadder R) : TPTLadder R × Snapshot R :=
  let stage : OnePoleTPTLowPass.LogicalSpec R := ⟨spec.cutoff⟩
  let s1Prepared := OnePoleTPTLowPass.prepareCoefficients stage state.stage1
  let s1 := s1Prepared.1
  let c1 := s1Prepared.2
  let s2Prepared := OnePoleTPTLowPass.prepareCoefficients stage state.stage2
  let s2 := s2Prepared.1
  let c2 := s2Prepared.2
  let s3Prepared := OnePoleTPTLowPass.prepareCoefficients stage state.stage3
  let s3 := s3Prepared.1
  let c3 := s3Prepared.2
  let s4Prepared := OnePoleTPTLowPass.prepareCoefficients stage state.stage4
  let s4 := s4Prepared.1
  let c4 := s4Prepared.2
  ({state with k := spec.feedback,stage1 := s1,stage2 := s2,stage3 := s3,stage4 := s4},
    ⟨c1.g,c2.g,c3.g,c4.g,spec.feedback⟩)
def prepareCoefficientsFloat := prepareCoefficients (R := Float)
end TPTLadder
end DSP
