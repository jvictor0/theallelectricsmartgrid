import DSP.Filters.TunedDecayComb
import DSP.Filters.SchroederAllPass
import DSP.DelayMath
import DSP.Nonlinear
import DSP.ComplexValue

namespace DSP.DelayKernels

/-- Ring addressing stays in the reusable storage component. These numeric
coefficients vectorize independently in every channel. Bypass is encoded 0/1. -/
structure Coefficients (R : Type) where
  delay : R
  a : R
  b : R
  r : R
  delayGain : R
  g : R
  k : R
  h : R
  gain : R
  sign : R
  bypass : R

structure LogicalSpec (R : Type) where
  frequency : R
  feedbackSign : R
  log2DecaySeconds : R
  dampOctaves : R
  sampleRate : R

def prepare {R : Type} [OfNat R 0] [OfNat R 1] [OfNat R 2] [OfScientific R]
    [Add R] [Sub R] [Mul R] [Div R] [Neg R] [IfLess R] [Trig R] [SquareRoot R] [DelayMath R]
    (_old : Coefficients R) (p : LogicalSpec R) : Coefficients R :=
  let cycles := IfLess.ifLess p.feedbackSign 0 (1 / 2) 1
  let bypass := IfLess.ifLess p.feedbackSign 0 0 (IfLess.ifLess 0 p.feedbackSign 0 1)
  let duration := DelayMath.exp2 p.log2DecaySeconds
  let cutoff := p.frequency * DelayMath.exp2 p.dampOctaves
  let cutoff := IfLess.ifLess cutoff 0.49 cutoff 0.49
  let t := Trig.tanPi p.frequency
  let v := Trig.tanPi cutoff / t
  let d := v * v - 1
  let shape := SquareRoot.sqrt (1 + d * d)
  let u := 1 / shape
  let g := t * SquareRoot.sqrt shape
  let k := SquareRoot.sqrt (2 * (1 - u))
  let gain := SquareRoot.sqrt ((1 - u) * (1 + u))
  let h := 1 / (1 + k * g + g * g)
  let phase := IfLess.ifLess bypass (1 / 2) (-(DelayMath.phaseTurns (k * SquareRoot.sqrt u) (1 - u))) 0
  let delay := (cycles + phase) / p.frequency
  let whole := DelayMath.floor delay - 1
  let whole := IfLess.ifLess 1 whole whole 1
  let fraction := delay - whole
  let td := Trig.tanPi (p.frequency * fraction)
  let a := (t - td) / (t + td)
  let b2 := 1 - a * a
  let b := SquareRoot.sqrt (IfLess.ifLess 0 b2 b2 0)
  let time := p.sampleRate * duration
  ⟨whole, a, b, DelayMath.decay (1 / time), DelayMath.decay (whole / time),
    g, k, h, gain, p.feedbackSign, bypass⟩

structure CombState (R : Type) where
  coefficients : Coefficients R
  memory : TunedDecayComb.Branch R

structure CombInput (R : Type) where
  x : R
  delayed : R

/-- Pointwise form of the proved branch recurrence. The numeric selector gives
independent bypass decisions in SIMD lanes, including clearing dormant state. -/
def combStep {R : Type} [OfNat R 0] [OfNat R 1] [OfNat R 2]
    [Add R] [Sub R] [Mul R] [Div R] [IfLess R]
    (s : CombState R) (input : CombInput R) : CombState R :=
  let c := s.coefficients
  let ap := NormalizedAllPass.step ⟨c.a, c.b, c.r, s.memory.allpass, 0⟩ ⟨input.delayed * c.delayGain⟩
  let e := input.x + c.sign * ap.y
  let i1 := c.r * s.memory.ic1
  let i2 := c.r * s.memory.ic2
  let v1 := c.h * (i1 + c.g * (e - i2))
  let v2 := i2 + c.g * v1
  ⟨c, ⟨ap.s, IfLess.ifLess c.bypass (1 / 2) (v1 * 2 - i1) 0,
    IfLess.ifLess c.bypass (1 / 2) (v2 * 2 - i2) 0,
    IfLess.ifLess c.bypass (1 / 2) (c.gain * v2) e⟩⟩

structure AllPassState (R : Type) where
  gain : R
  y : R
  written : R

def allPassStep {R : Type} [Neg R] [FMA R]
    (s : AllPassState R) (input : CombInput R) : AllPassState R :=
  let y := FMA.fma (-s.gain) input.x input.delayed
  ⟨s.gain, y, FMA.fma s.gain y input.x⟩

structure Snapshot (R : Type) where
  first : Coefficients R
  second : Coefficients R
  mix : R

structure Fraction (R : Type) where
  numerator : ComplexValue R
  denominator : ComplexValue R

section Response
variable {R : Type} [OfNat R 0] [OfNat R 1] [OfNat R 2]
  [Add R] [Sub R] [Mul R] [Div R] [Neg R] [IfLess R] [Trig R]

def lowpass (c : Coefficients R) (q : ComplexValue R) : ComplexValue R :=
  let rq := q.scale c.r
  let g2 := c.g * c.g
  let kg := c.k * c.g
  let plus := (ComplexValue.ofReal 1).add rq
  let n := (plus.mul plus).scale (c.gain * g2)
  let d := ((ComplexValue.ofReal (1 + kg + g2)).add
    (rq.scale (2 * (g2 - 1)))).add ((rq.mul rq).scale (1 - kg + g2))
  let l := n.div d
  ⟨IfLess.ifLess c.bypass (1 / 2) l.real 1, IfLess.ifLess c.bypass (1 / 2) l.imag 0⟩

def loop (c : Coefficients R) (frequency : ResponseFrequency R) (l : ComplexValue R) : ComplexValue R :=
  let rq := frequency.zInverse.scale c.r
  let ap := ((ComplexValue.ofReal c.a).add rq).div
    ((ComplexValue.ofReal 1).add (rq.scale c.a))
  let phase := -(c.delay * frequency.cyclesPerSample)
  let delayed : ComplexValue R := ⟨Trig.cos2pi phase, Trig.sin2pi phase⟩
  (((l.scale c.sign).mul ap).scale c.delayGain).mul delayed

def response (s : Snapshot R) (frequency : ResponseFrequency R) : Snapshot R × Fraction R :=
  let l0 := lowpass s.first frequency.zInverse
  let l1 := lowpass s.second frequency.zInverse
  let r0 := loop s.first frequency l0
  let r1 := loop s.second frequency l1
  (s, ⟨(l0.scale (1 - s.mix)).add (l1.scale s.mix),
    (ComplexValue.ofReal 1).sub ((r0.scale (1 - s.mix)).add (r1.scale s.mix))⟩)

end Response

end DSP.DelayKernels
