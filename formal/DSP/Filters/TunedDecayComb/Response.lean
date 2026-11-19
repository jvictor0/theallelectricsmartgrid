import DSP.Filters.TunedDecayComb.Controller
import DSP.ComplexValue

namespace DSP.TunedDecayComb

/-- A frozen response description, captured without reading audio memories.
`none` selects the settled branch; `some m` retains both phases in the loop. -/
structure Snapshot (capacity : Nat) (R : Type) where
  first : Coefficients capacity R
  second : Coefficients capacity R
  mix : Option R

def Controller.snapshot {capacity : Nat} {R : Type} (c : Controller capacity R) : Snapshot capacity R :=
  ⟨c.current, c.next, if c.active then some c.mix else none⟩

namespace Snapshot
variable {capacity : Nat} {R : Type}
variable [OfNat R 0] [OfNat R 1] [OfNat R 2] [Add R] [Sub R] [Mul R] [Div R]

def lowpass (c : Coefficients capacity R) (q : ComplexValue R) : ComplexValue R :=
  if c.bypass then ComplexValue.ofReal 1 else
    let rq := q.scale c.r
    let g2 := c.g * c.g
    let kg := c.k * c.g
    let onePlus := (ComplexValue.ofReal 1).add rq
    let numerator := (onePlus.mul onePlus).scale (c.gain * g2)
    let denominator := ((ComplexValue.ofReal (1 + kg + g2)).add
      (rq.scale (2 * (g2 - 1)))).add ((rq.mul rq).scale (1 - kg + g2))
    numerator.div denominator

def loop (c : Coefficients capacity R) (q low : ComplexValue R) : ComplexValue R :=
  let rq := q.scale c.r
  let ap := ((ComplexValue.ofReal c.a).add rq).div
    ((ComplexValue.ofReal 1).add (rq.scale c.a))
  (((low.scale c.sign).mul ap).scale c.delayGain).mul (q.pow (c.age.val + 1))

/-- Numerator and denominator keep the shared feedback topology explicit. -/
def evaluate (s : Snapshot capacity R) (frequency : ResponseFrequency R) :
    ComplexValue R × ComplexValue R :=
  let q := frequency.zInverse
  let first := lowpass s.first q
  let firstLoop := loop s.first q first
  match s.mix with
  | none => (first, (ComplexValue.ofReal 1).sub firstLoop)
  | some m =>
    let second := lowpass s.second q
    let secondLoop := loop s.second q second
    ((first.scale (1 - m)).add (second.scale m),
      (ComplexValue.ofReal 1).sub ((firstLoop.scale (1 - m)).add (secondLoop.scale m)))

def complexResponse (s : Snapshot capacity R) (frequency : ResponseFrequency R) : ComplexValue R :=
  let (n, d) := s.evaluate frequency
  n.div d

def magnitudeSquared [FMA R] [IfLess R]
    (s : Snapshot capacity R) (frequency : ResponseFrequency R) : R :=
  let (n, d) := s.evaluate frequency
  Response.ratio n.normSquared d.normSquared

end Snapshot
end DSP.TunedDecayComb
