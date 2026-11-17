import DSP.Transcendental
import DSP.Filters.OnePoleZOHLowPass.Response
import DSP.Filters.Responses

/-! Logical tuning in cycles/sample. TPT cutoff means the prewarped natural
frequency; Q is per SVF section. Composite specifications contain section specs.
No clamps or app-specific resonance remapping are applied: callers satisfy the
validity predicates in Tuning.Proofs. Record updates preserve runtime history. -/
namespace DSP

namespace OnePoleZOHLowPass
structure LogicalSpec (R : Type) where
  cutoff : R

def computeCoefficients {R : Type} [OfNat R 1] [Sub R] [ExpDecay R]
    (spec : LogicalSpec R) (state : OnePoleZOHLowPass R) : OnePoleZOHLowPass R :=
  { state with alpha := 1 - ExpDecay.expNegTwoPi spec.cutoff }
def computeCoefficientsFloat := computeCoefficients (R := Float)
end OnePoleZOHLowPass

namespace OnePoleTPTLowPass
structure LogicalSpec (R : Type) where
  cutoff : R

def computeCoefficients {R : Type} [OfNat R 1] [Add R] [Div R] [Trig R]
    (spec : LogicalSpec R) (state : OnePoleTPTLowPass R) : OnePoleTPTLowPass R :=
  let g := Trig.tanPi spec.cutoff
  { state with G := g / (1 + g) }
def computeCoefficientsFloat := computeCoefficients (R := Float)
end OnePoleTPTLowPass

namespace OnePoleTPTHighPass
structure LogicalSpec (R : Type) where
  cutoff : R

def computeCoefficients {R : Type} [OfNat R 1] [Add R] [Div R] [Trig R]
    (spec : LogicalSpec R) (state : OnePoleTPTHighPass R) : OnePoleTPTHighPass R :=
  { state with lowPass := OnePoleTPTLowPass.computeCoefficients ⟨spec.cutoff⟩ state.lowPass }
def computeCoefficientsFloat := computeCoefficients (R := Float)
end OnePoleTPTHighPass

namespace TPTBaseWidth
structure LogicalSpec (R : Type) where
  highPass : OnePoleTPTHighPass.LogicalSpec R
  lowPass : OnePoleTPTLowPass.LogicalSpec R

def computeCoefficients {R : Type} [OfNat R 1] [Add R] [Div R] [Trig R]
    (spec : LogicalSpec R) (state : TPTBaseWidth R) : TPTBaseWidth R :=
  { highPass := OnePoleTPTHighPass.computeCoefficients spec.highPass state.highPass
    lowPass := OnePoleTPTLowPass.computeCoefficients spec.lowPass state.lowPass }
def computeCoefficientsFloat := computeCoefficients (R := Float)
end TPTBaseWidth

namespace TPTSVF
structure LogicalSpec (R : Type) where
  cutoff : R
  q : R

def computeCoefficients {R : Type} [OfNat R 1] [Add R] [Mul R] [Div R] [FMA R] [Trig R]
    (spec : LogicalSpec R) (state : TPTSVF R) : TPTSVF R :=
  let g := Trig.tanPi spec.cutoff
  let k := 1 / spec.q
  let a1 := 1 / FMA.fma g (g + k) 1
  let a2 := g * a1
  { state with k, a1, a2, a3 := g * a2 }
def computeCoefficientsFloat := computeCoefficients (R := Float)
end TPTSVF

namespace MorphingTPTSVF
structure LogicalSpec (R : Type) where
  svf : TPTSVF.LogicalSpec R
  blend : R

def computeCoefficients {R : Type} [OfNat R 1] [OfNat R 2] [Add R] [Sub R]
    [Mul R] [Div R] [FMA R] [IfLess R] [Trig R]
    (spec : LogicalSpec R) (state : MorphingTPTSVF R) : MorphingTPTSVF R :=
  let half := 1 / 2
  let phase := IfLess.ifLess half spec.blend ((spec.blend - half) / 2) (spec.blend / 2)
  { state with
    svf := TPTSVF.computeCoefficients spec.svf state.svf
    blend := spec.blend, cosine := Trig.cos2pi phase, sine := Trig.sin2pi phase }
def computeCoefficientsFloat := computeCoefficients (R := Float)
end MorphingTPTSVF

namespace TPTSVF4Pole
structure LogicalSpec (R : Type) where
  stage1 : TPTSVF.LogicalSpec R
  stage2 : TPTSVF.LogicalSpec R

def computeCoefficients {R : Type} [OfNat R 1] [Add R] [Mul R] [Div R] [FMA R] [Trig R]
    (spec : LogicalSpec R) (state : TPTSVF4Pole R) : TPTSVF4Pole R :=
  { stage1 := TPTSVF.computeCoefficients spec.stage1 state.stage1
    stage2 := TPTSVF.computeCoefficients spec.stage2 state.stage2 }
def computeCoefficientsFloat := computeCoefficients (R := Float)
end TPTSVF4Pole

namespace TPTLadder
structure LogicalSpec (R : Type) where
  cutoff : R
  feedback : R

/-- Equal natural-frequency stages, raw loop feedback k in [0,4). No DC-gain
compensation or overall -3dB retuning: at k=0 the cutoff magnitude is 1/4. -/
def computeCoefficients {R : Type} [OfNat R 1] [Add R] [Div R] [Trig R]
    (spec : LogicalSpec R) (state : TPTLadder R) : TPTLadder R :=
  let stage : OnePoleTPTLowPass.LogicalSpec R := ⟨spec.cutoff⟩
  { state with
    k := spec.feedback
    stage1 := OnePoleTPTLowPass.computeCoefficients stage state.stage1
    stage2 := OnePoleTPTLowPass.computeCoefficients stage state.stage2
    stage3 := OnePoleTPTLowPass.computeCoefficients stage state.stage3
    stage4 := OnePoleTPTLowPass.computeCoefficients stage state.stage4 }
def computeCoefficientsFloat := computeCoefficients (R := Float)
end TPTLadder

namespace LinkwitzRileyCrossover
structure LogicalSpec (R : Type) where
  cutoff : R

/-- Standard LR4: two identical Butterworth second-order sections in each
branch. Q=cos(pi/4)=1/sqrt(2), unlike the app's unequal Butterworth-4 sections. -/
def computeCoefficients {R : Type} [OfNat R 1] [OfNat R 8]
    [Add R] [Mul R] [Div R] [FMA R] [Trig R]
    (spec : LogicalSpec R) (state : LinkwitzRileyCrossover R) : LinkwitzRileyCrossover R :=
  let stage : TPTSVF.LogicalSpec R := ⟨spec.cutoff, Trig.cos2pi (1 / 8)⟩
  { first := TPTSVF.computeCoefficients stage state.first
    lowSecond := TPTSVF.computeCoefficients stage state.lowSecond
    highSecond := TPTSVF.computeCoefficients stage state.highSecond }
def computeCoefficientsFloat := computeCoefficients (R := Float)
end LinkwitzRileyCrossover
end DSP
