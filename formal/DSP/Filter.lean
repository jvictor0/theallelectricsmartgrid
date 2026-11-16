import DSP.Signals

namespace DSP

/-! Filter contracts under zero-order-held input. An analog filter relates a
sample period, held input sequence, initial state and continuous-time output.
It can specify differential equations without choosing a solver or assuming
that a response exists. Each concrete filter must state its own regularity
and boundary conditions.

The digital state may differ from the scalar output. Input u n advances the
state from sample n to sample n + 1; output at zero observes the initial state.
-/

abbrev AnalogFilter (State : Type*) :=
  ℝ → DigitalSignal → State → AnalogSignal → Prop

structure DigitalFilter (State : Type*) where
  step : ℝ → State → State
  output : State → ℝ

namespace DigitalFilter

variable {State : Type*}

def runState (filter : DigitalFilter State) (u : DigitalSignal)
    (initial : State) : ℕ → State
  | 0 => initial
  | n + 1 => filter.step (u n) (filter.runState u initial n)

def run (filter : DigitalFilter State) (u : DigitalSignal)
    (initial : State) : DigitalSignal :=
  fun n => filter.output (filter.runState u initial n)

@[simp] theorem runState_zero (filter : DigitalFilter State)
    (u : DigitalSignal) (initial : State) :
    filter.runState u initial 0 = initial := rfl

@[simp] theorem runState_succ (filter : DigitalFilter State)
    (u : DigitalSignal) (initial : State) (n : ℕ) :
    filter.runState u initial (n + 1) =
      filter.step (u n) (filter.runState u initial n) := rfl

@[simp] theorem run_zero (filter : DigitalFilter State)
    (u : DigitalSignal) (initial : State) :
    filter.run u initial 0 = filter.output initial := rfl

end DigitalFilter

def Implements {State : Type*} (T : ℝ)
    (analog : AnalogFilter State) (digital : DigitalFilter State) : Prop :=
  ∀ (u : DigitalSignal) (initial : State) (y : AnalogSignal),
    analog T u initial y → ExactAtSamples T y (digital.run u initial)

end DSP
