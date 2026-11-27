import Mathlib.Algebra.Order.Archimedean.Real.Basic
import Mathlib.Algebra.Order.Floor.Ring
import Mathlib.Tactic.Linarith

namespace DSP.TapeHead

structure Controls (R : Type) where
  period : R
  speed : R
  fraction : R

/-- Keep the subtraction that computes the wrap width in the runtime expression.
In floating point, `write - (write - period)` can differ from `period`. -/
def projectWithFloor {R : Type} [Sub R] [Mul R] [Div R]
    (floor : R → R) (controls : Controls R) (write : R) : R :=
  let projected := write * controls.speed - controls.period * controls.fraction
  let lower := write - controls.period
  let width := write - lower
  projected - width * floor ((projected - lower) / width)

abbrev Loop := Fin 6

structure State where
  loopSelector : Vector Loop 4

structure Input where
  writePhase : Float
  cycleRatio : Vector Float 6
  crossed : Vector Bool 6
  requestedLoop : Vector Loop 4
  speed : Vector Float 4
  fraction : Vector Float32 4

structure Output where
  readPhase : Vector Float 4
  relativeReadPosition : Vector Float32 4
  loopPeriod : Vector Float 4

def selectLoop (crossed : Vector Bool 6) (current requested : Loop) : Loop :=
  if current.val = requested.val then current
  else if crossed.get current && crossed.get requested then requested
  else current

def process (input : Input) : StateM State Output := do
  let previous ← get
  let selected := Vector.ofFn fun lane : Fin 4 =>
    selectLoop input.crossed (previous.loopSelector.get lane) (input.requestedLoop.get lane)
  let periods := Vector.ofFn fun lane : Fin 4 => 1 / input.cycleRatio.get (selected.get lane)
  let phases := Vector.ofFn fun lane : Fin 4 =>
    let fraction := input.fraction.get lane
    let fraction := if fraction == 0 then 1 else fraction.toFloat
    projectWithFloor Float.floor ⟨periods.get lane, input.speed.get lane, fraction⟩ input.writePhase
  let relative := Vector.ofFn fun lane : Fin 4 =>
    let phase := phases.get lane
    (phase - Float.floor phase).toFloat32
  set (⟨selected⟩ : State)
  return ⟨phases, relative, periods⟩

def run (state : State) (input : Input) : State × Output :=
  let (output, next) := (process input).run state
  (next, output)

theorem project_phase_bounds (controls : Controls ℝ) (write : ℝ) (hp : 0 < controls.period) :
    let read := projectWithFloor (fun x => (⌊x⌋ : ℝ)) controls write
    write - controls.period ≤ read ∧ read < write := by
  let projected := write * controls.speed - controls.period * controls.fraction
  let lower := write - controls.period
  let width := write - lower
  have hw : 0 < width := by
    dsimp [width, lower]
    linarith
  have htop : lower + width = write := by
    dsimp [width]
    linarith
  have hlo := (le_div_iff₀ hw).mp (Int.floor_le ((projected - lower) / width))
  have hhi := (div_lt_iff₀ hw).mp (Int.lt_floor_add_one ((projected - lower) / width))
  change lower ≤ projected - width * (⌊(projected - lower) / width⌋ : ℝ) ∧
    projected - width * (⌊(projected - lower) / width⌋ : ℝ) < write
  constructor <;> nlinarith

end DSP.TapeHead
