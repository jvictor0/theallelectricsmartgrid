import DSP.SampleRateReducer
import DSP.Filters.DelayKernelsProofs

namespace DSP.SampleRateReducer

theorem process_bypass (input : Input ℝ) (state : State ℝ)
    (freq : 1 ≤ input.freq) (phase : state.phase < 1) :
    (process input).run state = (input.sample, state) := by
  simp [process, IfLess.ifLess, not_lt.mpr freq, phase]
  rfl

theorem process_hold (input : Input ℝ) (state : State ℝ)
    (freq : input.freq < 1) (phase : state.phase + input.freq < 1) :
    (process input).run state = (state.output, ⟨state.phase + input.freq, state.output⟩) := by
  simp [process, IfLess.ifLess, freq, phase]
  rfl

theorem process_capture (input : Input ℝ) (state : State ℝ)
    (freq : input.freq < 1) (crossed : 1 ≤ state.phase + input.freq)
    (phase : state.phase + input.freq < 2) :
    (process input).run state = (input.sample, ⟨state.phase + input.freq - 1, input.sample⟩) := by
  have wrap : ⌊state.phase + input.freq⌋ = (1 : ℤ) := by
    rw [Int.floor_eq_iff]
    simpa [one_add_one_eq_two] using And.intro crossed phase
  simp only [process, StateT.run_modifyGet, IfLess.ifLess, freq, if_true,
    not_lt.mpr crossed, if_false, DelayMath.floor, wrap, Int.cast_one]
  rfl

end DSP.SampleRateReducer
