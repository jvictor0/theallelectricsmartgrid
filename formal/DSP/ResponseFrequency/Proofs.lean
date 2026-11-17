import DSP.ResponseFrequency
import Mathlib.Analysis.SpecialFunctions.Trigonometric.Basic
import Mathlib.Analysis.Complex.Trigonometric
import Mathlib.Tactic.Ring

noncomputable section

namespace DSP.ResponseFrequency

/-- Exact mathematical preparation. Runtime callers supply their chosen
floating-point approximation instead; no exact trig law is asserted for it. -/
def ofCyclesPerSample (frequency : ℝ) : ResponseFrequency ℝ :=
  fromHalfAngle frequency (Real.sin (Real.pi * frequency)) (Real.cos (Real.pi * frequency))

/-- A proposition about real data, kept out of the executable record. -/
def Represents (prepared : ResponseFrequency ℝ) (frequency : ℝ) : Prop :=
  prepared = ofCyclesPerSample frequency

theorem ofCyclesPerSample_represents (frequency : ℝ) :
    (ofCyclesPerSample frequency).Represents frequency := rfl

@[simp] theorem ofCyclesPerSample_cyclesPerSample (frequency : ℝ) :
    (ofCyclesPerSample frequency).cyclesPerSample = frequency := rfl

@[simp] theorem ofCyclesPerSample_sinHalf (frequency : ℝ) :
    (ofCyclesPerSample frequency).sinHalf = Real.sin (Real.pi * frequency) := rfl

@[simp] theorem ofCyclesPerSample_cosHalf (frequency : ℝ) :
    (ofCyclesPerSample frequency).cosHalf = Real.cos (Real.pi * frequency) := rfl

/-- One sample of delay at `frequency` cycles per sample: exp(-2πif). -/
def unitDelay (frequency : ℝ) : ℂ :=
  Complex.exp (((-(2 * (Real.pi * frequency)) : ℝ) : ℂ) * Complex.I)

@[simp] theorem unitDelay_re (frequency : ℝ) :
    (unitDelay frequency).re = Real.cos (2 * (Real.pi * frequency)) := by
  rw [unitDelay, Complex.exp_ofReal_mul_I_re, Real.cos_neg]

@[simp] theorem unitDelay_im (frequency : ℝ) :
    (unitDelay frequency).im = -Real.sin (2 * (Real.pi * frequency)) := by
  rw [unitDelay, Complex.exp_ofReal_mul_I_im, Real.sin_neg]

/-- Read the stored real components as an exact complex number for proofs. -/
def delay (prepared : ResponseFrequency ℝ) : ℂ :=
  ⟨prepared.zInverse.real, prepared.zInverse.imag⟩

theorem ofCyclesPerSample_delay (frequency : ℝ) :
    (ofCyclesPerSample frequency).delay = unitDelay frequency := by
  apply Complex.ext
  · simp [delay, ofCyclesPerSample, fromHalfAngle, Real.cos_two_mul_eq_one_sub]
    ring
  · simp [delay, ofCyclesPerSample, fromHalfAngle, Real.sin_two_mul]

theorem ofCyclesPerSample_halfAngle (frequency : ℝ) :
    (ofCyclesPerSample frequency).sinHalf ^ 2 +
      (ofCyclesPerSample frequency).cosHalf ^ 2 = 1 := by
  simp only [ofCyclesPerSample_sinHalf, ofCyclesPerSample_cosHalf, Real.sin_sq_add_cos_sq]

end DSP.ResponseFrequency
