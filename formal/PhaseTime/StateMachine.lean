import PhaseTime.Periodic
import Veil

set_option veil.smt.trust false
set_option veil.solver "grind"
set_option autoImplicit true

attribute [grind .] PhaseTime.initial_valid PhaseTime.advance_valid PhaseTime.resume_valid
  PhaseTime.free_valid PhaseTime.history_initial_valid PhaseTime.record_valid PhaseTime.retreat_valid
  PhaseTime.reassign_valid

/-! Audio-clock and inverse publication protocol. Observation actions are
pointwise exact forward writes or backwards observations. The implementation
scatters forward intervals into its finite grid; interpolation is excluded.
`forward` in the history is ghost evidence, not proposed runtime storage.
The view retains a real-time cutoff, not a frozen copy of the live inverse. -/
veil module PhaseRuntime

type number
immutable function numeric : number → Rat

individual clock : PhaseTime.Clock
individual history : PhaseTime.History
individual publishedTime : Rat
individual publishedPhase : Rat

#gen_state

after_init {
  clock := PhaseTime.initialClock
  history := PhaseTime.initialHistory
  publishedTime := 0
  publishedPhase := 0
}

-- @cpp clock_run: one ordinary running engine sample, including backwards travel.
action running_sample (phase : number) {
  require clock.running
  clock := clock.advance (numeric phase)
}

-- @cpp clock_rebase: restart preserves logical delay phase and rebases the musical origin.
action restart (phase : number) {
  require !clock.running
  clock := clock.resume (numeric phase)
}

-- @cpp clock_free: nominal forward travel while stopped, including the stop sample.
action stopped_sample (increment : number) {
  require 0 < numeric increment
  clock := clock.freeRun (numeric increment)
}

-- @cpp inverse_forward: newly traversed forward points replace the previous visit.
action forward_observation (stamp : number) {
  require history.cursor < clock.phase
  require history.now < numeric stamp
  history := history.record clock.phase (numeric stamp)
}

-- @cpp inverse_reverse: stationary/backwards travel preserves all chosen forward visits.
action reverse_observation (stamp : number) {
  require clock.phase ≤ history.cursor
  require history.now < numeric stamp
  history := history.retreat clock.phase (numeric stamp)
}

-- @cpp inverse_reassign: pointwise abstraction of choosing a fallback visit in a cell.
action reassign_cell_coordinate (phase : number) (stamp : number) {
  require history.forward (numeric stamp) = numeric phase
  require numeric stamp ≤ history.now
  history := history.reassign (numeric phase) (numeric stamp)
}

-- @cpp publication: capture phase and real cutoff together for one UI view.
action publish {
  publishedTime := history.now
  publishedPhase := history.cursor
}

invariant [clock_coherent] clock.Valid
invariant [inverse_sound] history.Valid

#gen_spec
#check_invariants
#gen_theorems

end PhaseRuntime
