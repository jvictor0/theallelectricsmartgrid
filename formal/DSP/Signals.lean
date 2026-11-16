import Mathlib.Data.Real.Basic

namespace DSP

/-! Exact real-valued signals. Digital index zero denotes the initial sample,
before any held input interval has been processed. -/

abbrev AnalogSignal := ℝ → ℝ
abbrev DigitalSignal := ℕ → ℝ

noncomputable def sampleTime (T : ℝ) (n : ℕ) : ℝ := (n : ℝ) * T

noncomputable def sample (T : ℝ) (y : AnalogSignal) : DigitalSignal :=
  fun n => y (sampleTime T n)

def ExactAtSamples (T : ℝ) (y : AnalogSignal) (v : DigitalSignal) : Prop :=
  ∀ n, sample T y n = v n

@[simp] theorem sampleTime_zero (T : ℝ) : sampleTime T 0 = 0 := by
  simp [sampleTime]

theorem sampleTime_succ (T : ℝ) (n : ℕ) :
    sampleTime T (n + 1) = sampleTime T n + T := by
  simp [sampleTime, add_mul]

theorem exactAtSamples_iff (T : ℝ) (y : AnalogSignal) (v : DigitalSignal) :
    ExactAtSamples T y v ↔ sample T y = v := by
  exact ⟨fun h => funext h, fun h n => congrFun h n⟩

end DSP
