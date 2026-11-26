import DSP.AHD
import Mathlib.Data.Real.Basic
import Mathlib.Tactic.FieldSimp
import Mathlib.Tactic.Linarith
import Mathlib.Tactic.Ring

namespace DSP.AHD

theorem completionRetainsOvershoot (s : Memory ℝ) (samples remaining : ℝ) :
    endDistance (finishDecay s samples remaining) samples = remaining := by
  simp [endDistance, finishDecay]

theorem reverseEndpoint (s : Memory ℝ) (i : Input ℝ)
    (hr : 0 < i.phaseRatio) (hp : 0 < i.period) :
    let duration := completion i i.forwardStartOutput
    let atEnd := { i with phaseDistance := duration / (i.phaseRatio * i.period) }
    endDistance (prepareReverse s atEnd) (elapsed atEnd true) = 0 := by
  let duration := completion i i.forwardStartOutput
  change duration / (i.phaseRatio * i.period) * i.phaseRatio * i.period - duration = 0
  field_simp [ne_of_gt hr, ne_of_gt hp]
  ring

theorem reverseDecayValue (s : Memory ℝ) (travel : Travel ℝ) (delta : ℝ)
    (hd : delta < 0) (hi : 0 < travel.decayIncrement)
    (hInside : 0 < 1 + delta * travel.decayIncrement) :
    let next := ((advanceOne travel delta).run (resumeDecay s)).2
    next.stage = decay ∧ stageOutput next = -(delta * travel.decayIncrement) := by
  have hHold : isStage decay hold = false := by decide
  have hAttack : isStage decay attack = false := by decide
  have hDecay : isStage decay decay = true := by decide
  have hStep : ((advanceOne travel delta).run (resumeDecay s)).2 =
      { resumeDecay s with position := 1 + delta * travel.decayIncrement } := by
    simp [advanceOne, resumeDecay, below, atMost, hHold, hAttack,
      not_le.mpr hd, not_lt.mpr hd.le, not_le.mpr hi, hInside]; rfl
  change ((advanceOne travel delta).run (resumeDecay s)).2.stage = decay ∧
    stageOutput ((advanceOne travel delta).run (resumeDecay s)).2 =
      -(delta * travel.decayIncrement)
  rw [hStep]
  simp [resumeDecay, stageOutput, hHold, hAttack, hDecay]

end DSP.AHD
