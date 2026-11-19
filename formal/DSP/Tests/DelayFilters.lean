import DSP.Filters.SchroederAllPass
import DSP.Filters.TunedDecayComb
import DSP.Filters.TunedDecayComb.Response
import DSP.Filters.DelayChannels

namespace DSP.Tests.DelayFilters

def close (a b : Float) : Bool := (a - b).abs ≤ 2.0e-10 * (1.0 + a.abs + b.abs)

def past (xs : Array Float) (delay : Nat) : Float :=
  if delay ≤ xs.size then xs[xs.size - delay]! else 0.0

def input (n : Nat) : Float :=
  if n == 0 then 1.0 else if n % 13 == 0 then -0.25 else
    0.1 * (n.toFloat * 0.137).sin

/-- Compare physical rings against an unbounded append-only write log. Every
head position wraps, and reads never modify the head or any other reader. -/
def checkBuffer (capacity : Nat) (hc : 0 < capacity) : IO Unit := do
  let mut s : DelayBuffer capacity Float := DelayBuffer.empty hc
  let mut writes : Array Float := #[]
  for n in [:capacity * 3 + 29] do
    for j in [0, capacity - 1, n % capacity] do
      if hj : j < capacity then
        unless s.read ⟨j, hj⟩ == past writes (j + 1) do
          throw (IO.userError s!"Ring history failed: capacity={capacity}, n={n}, age={j}")
    unless s.head.val == n % capacity do
      throw (IO.userError s!"Ring head failed at capacity={capacity}, n={n}")
    let x := n.toFloat * 0.25 - 17.0
    s := s.push x
    writes := writes.push x

def rawEnergy {capacity : Nat} (s : DelayBuffer capacity Float) (age : Fin capacity)
    (r : Float) : Float := Id.run do
  let mut result := 0.0
  let mut weight := 1.0
  for i in [:age.val + 1] do
    if hi : i < capacity then
      let x := s.read ⟨i, hi⟩
      result := result + weight * x * x
      weight := weight * r * r
  return result

def checkSchroeder : IO Unit := do
  for gain in [-0.99, -0.6, 0.0, 0.6, 0.99] do
    for j in [0, 2, 7] do
      if hj : j < 8 then
        let c : SchroederAllPass.Coefficients 8 Float := ⟨⟨j, hj⟩, gain⟩
        let mut s : SchroederAllPass.State 8 Float := ⟨DelayBuffer.empty (by decide), 0.0⟩
        let mut inputs := #[]
        let mut outputs := #[]
        let mut supplied := 0.0
        let mut emitted := 0.0
        for n in [:3000] do
          let x := input n
          let expected := past inputs (j + 1) - gain * x + gain * past outputs (j + 1)
          s := SchroederAllPass.step c s ⟨x⟩
          unless close s.y expected do
            throw (IO.userError s!"Schroeder direct recurrence failed: g={gain}, N={j+1}, n={n}")
          inputs := inputs.push x
          outputs := outputs.push expected
          supplied := supplied + x * x
          emitted := emitted + s.y * s.y
        let stored := rawEnergy s.buffer c.age 1.0 / (1.0 - gain * gain)
        unless close (emitted + stored) supplied do
          throw (IO.userError s!"Schroeder energy failed: g={gain}, N={j+1}")
  let c : SchroederAllPass.Coefficients 1 Float32 := ⟨⟨0, by decide⟩, 0.5⟩
  let mut s : SchroederAllPass.State 1 Float32 := ⟨DelayBuffer.empty (by decide), 0.0⟩
  for x in [1.0, 0.0, 0.0] do
    s := SchroederAllPass.step c s ⟨x⟩
  unless s.y == 0.375 do
    throw (IO.userError "Float32 one-slot Schroeder failed")

structure ReferenceBranch where
  apInput : Float := 0.0
  apOutput : Float := 0.0
  e1 : Float := 0.0
  e2 : Float := 0.0
  y1 : Float := 0.0
  y2 : Float := 0.0

/-- Independent direct-form recurrences for both rational branch factors. -/
def referenceBranch {capacity : Nat} (c : TunedDecayComb.Coefficients capacity Float)
    (s : ReferenceBranch) (x delayed : Float) : ReferenceBranch :=
  let apInput := c.delayGain * delayed
  let apOutput := c.a * apInput + c.r * s.apInput - c.a * c.r * s.apOutput
  let e := x + c.sign * apOutput
  let d0 := 1.0 + c.k * c.g + c.g * c.g
  let d1 := 2.0 * (c.g * c.g - 1.0) * c.r
  let d2 := (1.0 - c.k * c.g + c.g * c.g) * c.r * c.r
  let y := if c.bypass then e else
    (c.gain * c.g * c.g * (e + 2.0 * c.r * s.e1 + c.r * c.r * s.e2) -
      d1 * s.y1 - d2 * s.y2) / d0
  ⟨apInput, apOutput, e, s.e1, y, s.y1⟩

def coefficients (age : Fin 7) (a r sign : Float) (bypass : Bool) :
    TunedDecayComb.Coefficients 7 Float :=
  let g := 0.3
  let k := 1.2
  { age, a, b := (1.0 - a * a).sqrt, r, delayGain := r.pow (age.val + 1).toFloat,
    g, k, h := 1.0 / (1.0 + k * g + g * g), gain := 0.8, sign, bypass }

def checkComb : IO Unit := do
  let bypass := coefficients ⟨0, by decide⟩ 0.6 0.9 0.5 true
  let cleared := TunedDecayComb.branchStep bypass ⟨2.0, 3.0, -4.0, 0.0⟩ 0.25 0.0
  unless cleared.ic1 == 0.0 && cleared.ic2 == 0.0 && close cleared.y 0.97 do
    throw (IO.userError "Comb bypass failed to clear dormant integrators")
  for bypass in [false, true] do
    for mode in [:4] do
      let c₀ := coefficients ⟨0, by decide⟩ 0.6 0.9 0.5 bypass
      let c₁ := coefficients ⟨6, by decide⟩ (-0.8) 0.97 (-0.5) false
      let zero : TunedDecayComb.Branch Float := ⟨0.0, 0.0, 0.0, 0.0⟩
      let mut s : TunedDecayComb.State 7 Float := ⟨DelayBuffer.empty (by decide), zero, zero, 0.0⟩
      let mut ref₀ : ReferenceBranch := {}
      let mut ref₁ : ReferenceBranch := {}
      let mut outputs := #[]
      for n in [:3000] do
        let x := input n
        let mix : Option Float := if mode == 0 then none else
          some (if mode == 1 then 0.0 else if mode == 2 then 1.0 else (n % 101).toFloat / 100.0)
        ref₀ := referenceBranch c₀ ref₀ x (past outputs 1)
        let mut expected := ref₀.y1
        if let some m := mix then
          ref₁ := referenceBranch c₁ ref₁ x (past outputs 7)
          expected := (1.0 - m) * ref₀.y1 + m * ref₁.y1
        let next := TunedDecayComb.step c₀ c₁ mix s ⟨x⟩
        unless close next.y expected && next.buffer.head.val == (n + 1) % 7 do
          throw (IO.userError s!"Comb shared-write recurrence failed: bypass={bypass}, mode={mode}, n={n}")
        let d := s.buffer.read c₀.age * c₀.delayGain
        let ap := c₀.a * d + c₀.b * c₀.r * s.first.allpass
        let after := ap * ap + next.first.allpass * next.first.allpass +
          rawEnergy next.buffer c₀.age c₀.r
        let before := next.y * next.y + c₀.r * c₀.r *
          (s.first.allpass * s.first.allpass + rawEnergy s.buffer c₀.age c₀.r)
        unless close after before do
          throw (IO.userError s!"Comb delay/lattice energy failed at n={n}")
        outputs := outputs.push expected
        s := next

def sameBranch (a b : TunedDecayComb.Branch Float) : Bool :=
  a.allpass == b.allpass && a.ic1 == b.ic1 && a.ic2 == b.ic2 && a.y == b.y

def checkController : IO Unit := do
  let c₀ := coefficients ⟨0, by decide⟩ 0.6 0.9 0.5 false
  let c₁ := coefficients ⟨6, by decide⟩ (-0.8) 0.97 (-0.5) false
  let discarded := coefficients ⟨2, by decide⟩ 0.0 0.93 0.0 true
  let latest := coefficients ⟨3, by decide⟩ 0.2 0.94 0.5 false
  let zero : TunedDecayComb.Branch Float := ⟨0.0, 0.0, 0.0, 0.0⟩
  let mut c : TunedDecayComb.Controller 7 Float :=
    ⟨⟨DelayBuffer.empty (by decide), zero, zero, 0.0⟩, c₀, c₀, none, 0.0, 0.25, false⟩
  for n in [:11] do c := c.tick ⟨input n⟩
  c := c.request c₁
  for transition in [:2] do
    let before := c.state
    let old := if transition == 0 then c₀ else c₁
    let target := if transition == 0 then c₁ else latest
    let mut expected := { before with second := before.first }
    for j in [:4] do
      let x := input (11 + transition * 4 + j)
      expected := TunedDecayComb.step old target (some (j.toFloat * 0.25)) expected ⟨x⟩
      c := c.tick ⟨x⟩
      unless c.state.y == expected.y && c.state.buffer.head.val == (12 + transition * 4 + j) % 7 do
        throw (IO.userError s!"Controller output/shared write failed: transition={transition}, sample={j}")
      unless c.active == (j < 3) && c.mix == (if j < 3 then (j+1).toFloat * 0.25 else 0.0) do
        throw (IO.userError "Controller fade timing or promotion failed")
      if j < 3 then
        unless sameBranch c.state.first expected.first && sameBranch c.state.second expected.second do
          throw (IO.userError "Controller branch copy or update failed")
      else
        unless sameBranch c.state.first expected.second && c.current.age.val == target.age.val do
          throw (IO.userError "Controller promotion lost the new branch state")
      if transition == 0 && j == 0 then
        c := (c.request discarded).request latest
    if transition == 0 then
      unless (c.pending.map (·.age.val)) == some latest.age.val do
        throw (IO.userError "Controller failed to retain the latest pending request")
  c := c.reset discarded
  unless !c.active && c.pending.isNone && c.mix == 0.0 && c.state.y == 0.0 &&
      sameBranch c.state.first zero && sameBranch c.state.second zero do
    throw (IO.userError "Controller reset failed")
  for j in [:7] do
    if hj : j < 7 then
      unless c.state.buffer.read ⟨j, hj⟩ == 0.0 do
        throw (IO.userError "Controller reset retained delay history")

/-- Store channel values between calls. Returning a function from a helper
allows eta expansion to defer its work; vectors force evaluation here. -/
structure StoredChannels where
  buffer : DelayBuffer 7 (Vector Float 3)
  first : Vector (TunedDecayComb.Branch Float) 3
  second : Vector (TunedDecayComb.Branch Float) 3
  y : Vector Float 3

def StoredChannels.unpack (s : StoredChannels) : TunedDecayComb.Channels.State 7 (Fin 3) Float :=
  ⟨s.buffer.map Vector.get, s.first.get, s.second.get, s.y.get⟩

def StoredChannels.pack (s : TunedDecayComb.Channels.State 7 (Fin 3) Float) : StoredChannels :=
  ⟨s.buffer.map Vector.ofFn, Vector.ofFn s.first, Vector.ofFn s.second, Vector.ofFn s.y⟩

def checkChannels : IO Unit := do
  let c₀ (ch : Fin 3) := coefficients ⟨ch.val * 3, by omega⟩ 0.6 0.9 0.5 (ch.val == 1)
  let c₁ (ch : Fin 3) := coefficients ⟨6 - ch.val * 3, by omega⟩ (-0.8) 0.97 (-0.5) false
  let zero : TunedDecayComb.Branch Float := ⟨0.0, 0.0, 0.0, 0.0⟩
  let mut multi := StoredChannels.pack
    ⟨(DelayBuffer.empty (R := Float) (by decide)).map (fun x _ => x),
      fun _ => zero, fun _ => zero, fun _ => 0.0⟩
  let mut scalar : Vector (TunedDecayComb.State 7 Float) 3 := Vector.ofFn
    fun _ => ⟨DelayBuffer.empty (by decide), zero, zero, 0.0⟩
  let apc : SchroederAllPass.Channels.Coefficients 7 (Fin 3) Float :=
    ⟨fun ch => (c₀ ch).age, fun ch => if ch.val == 1 then -0.7 else 0.6⟩
  let mut aps : DelayBuffer 7 (Vector Float 3) :=
    (DelayBuffer.empty (R := Float) (by decide)).map (fun x => Vector.replicate 3 x)
  let mut apscalar : Vector (SchroederAllPass.State 7 Float) 3 := Vector.ofFn
    fun _ => ⟨DelayBuffer.empty (by decide), 0.0⟩
  for n in [:400] do
    let x (ch : Fin 3) := input (n + ch.val * 7) * (ch.val + 1).toFloat
    let mix := if n % 5 == 0 then none else some ((n % 31).toFloat / 30.0)
    multi := StoredChannels.pack (TunedDecayComb.Channels.step c₀ c₁ mix multi.unpack ⟨x⟩)
    scalar := Vector.ofFn (fun ch => TunedDecayComb.step (c₀ ch) (c₁ ch) mix (scalar.get ch) ⟨x ch⟩)
    let apnext := SchroederAllPass.Channels.step apc ⟨aps.map Vector.get, fun _ => 0.0⟩ ⟨x⟩
    aps := apnext.buffer.map Vector.ofFn
    let apy := Vector.ofFn apnext.y
    apscalar := Vector.ofFn (fun ch => SchroederAllPass.step ⟨apc.age ch, apc.gain ch⟩ (apscalar.get ch) ⟨x ch⟩)
    for ch in [0, 1, 2] do
      if hc : ch < 3 then
        let ch : Fin 3 := ⟨ch, hc⟩
        unless multi.y.get ch == (scalar.get ch).y && apy.get ch == (apscalar.get ch).y do
          throw (IO.userError s!"Delay channel projection failed: channel={ch.val}, n={n}")
        for j in [:7] do
          if hj : j < 7 then
            unless (multi.buffer.read ⟨j, hj⟩).get ch == (scalar.get ch).buffer.read ⟨j, hj⟩ &&
                (aps.read ⟨j, hj⟩).get ch == (apscalar.get ch).buffer.read ⟨j, hj⟩ do
              throw (IO.userError "Delay channel history projection failed")

/-- Compare the executable response with a Fourier sum of actual impulse
samples, covering DC, Nyquist, opposite feedback signs and frozen mixing. -/
def checkResponse : IO Unit := do
  let c₀ := coefficients ⟨0, by decide⟩ 0.6 0.9 0.5 false
  let c₁ := coefficients ⟨6, by decide⟩ (-0.8) 0.97 (-0.5) false
  for mix in [none, some 0.0, some 0.3, some 1.0] do
    let snapshot : TunedDecayComb.Snapshot 7 Float := ⟨c₀, c₁, mix⟩
    for f in [0.0, 0.037, 0.25, 0.5] do
      let angle := 3.141592653589793 * f
      let frequency := ResponseFrequency.fromHalfAngle f angle.sin angle.cos
      let response := snapshot.complexResponse frequency
      let zero : TunedDecayComb.Branch Float := ⟨0.0, 0.0, 0.0, 0.0⟩
      let mut s : TunedDecayComb.State 7 Float := ⟨DelayBuffer.empty (by decide), zero, zero, 0.0⟩
      let mut re := 0.0
      let mut im := 0.0
      for n in [:2000] do
        s := TunedDecayComb.step c₀ c₁ mix s ⟨if n == 0 then 1.0 else 0.0⟩
        re := re + s.y * (2.0 * angle * n.toFloat).cos
        im := im - s.y * (2.0 * angle * n.toFloat).sin
      unless close re response.real && close im response.imag &&
          close (re * re + im * im) (snapshot.magnitudeSquared frequency) do
        throw (IO.userError s!"Comb impulse/response mismatch at frequency={f}")

def check : IO Unit := do
  checkBuffer 1 (by decide)
  checkBuffer 3 (by decide)
  checkBuffer 8 (by decide)
  checkBuffer 4096 (by decide)
  checkBuffer 16384 (by decide)
  checkSchroeder
  checkComb
  checkController
  checkChannels
  checkResponse
  IO.println "Delay filters: ring history, Schroeder/comb recurrence and energy, controller, channels and impulse/response checks passed."

end DSP.Tests.DelayFilters
