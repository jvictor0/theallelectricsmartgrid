import DSP.Filters.NormalizedAllPass

namespace DSP.Tests.AllPass

/-- Compare the lattice against the independent input/output recurrence of
(a+r*q)/(1+a*r*q), then check its local energy balance during retuning. -/
def check : IO Unit := do
  for a in [-0.99, -0.6, 0.0, 0.6, 0.99] do
    for r in [0.0, 0.5, 1.0] do
      let b := (1.0 - a * a).sqrt
      let mut f : NormalizedAllPass Float := ⟨a, b, r, 0.0, 123.0⟩
      let mut previousInput := 0.0
      let mut previousOutput := 0.0
      for n in [:512] do
        let x := if n == 0 then 1.0 else if n == 71 then -0.5 else 0.0
        let expected := a * x + r * previousInput - a * r * previousOutput
        f := NormalizedAllPass.stepFloat f ⟨x⟩
        unless (f.y - expected).abs < 1.0e-12 && f.a == a && f.b == b && f.r == r do
          throw (IO.userError s!"Normalized allpass recurrence failed at a={a}, r={r}, n={n}")
        previousInput := x
        previousOutput := expected
  let mut f : NormalizedAllPass Float := ⟨0.0, 1.0, 1.0, 2.0, 0.0⟩
  let mut supplied := f.s * f.s
  let mut emitted := 0.0
  let mut lost := 0.0
  for n in [:512] do
    let a := if n % 2 == 0 then 0.6 else -0.8
    let r := if n % 3 == 0 then 1.0 else 0.75
    f := { f with a, b := (1.0 - a * a).sqrt, r }
    let x := if n % 5 == 0 then 0.5 else -0.25
    let next := NormalizedAllPass.stepFloat f ⟨x⟩
    let before := x * x + r * r * f.s * f.s
    let after := next.y * next.y + next.s * next.s
    unless (after - before).abs < 1.0e-12 do
      throw (IO.userError s!"Normalized allpass local energy failed at n={n}")
    supplied := supplied + x * x
    emitted := emitted + next.y * next.y
    lost := lost + (1.0 - r * r) * f.s * f.s
    f := next
  unless (emitted + f.s * f.s + lost - supplied).abs < 1.0e-10 do
    throw (IO.userError "Normalized allpass accumulated energy failed")
  IO.println "Normalized allpass: recurrence and modulated energy checks passed."

end DSP.Tests.AllPass
