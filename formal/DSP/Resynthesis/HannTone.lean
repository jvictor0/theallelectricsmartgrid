import DSP.Resynthesis.Real

namespace DSP.Resynthesis

noncomputable section

/-- A real sinusoid described by its amplitude, frequency in turns per unit of
time, and phase in turns at time zero. -/
def sinusoid (amplitude frequency phase time : ℝ) : ℝ :=
  amplitude * Trig.cos2pi (frequency * time + phase)

def hannWeight (turns : ℝ) : ℝ := 0.5 * (1 - Trig.cos2pi turns)

/-- A window captured at an arbitrary start has the same frequency in its
local sample grid. The start is absorbed into the phase parameter. -/
def hannSinusoid {n : ℕ} (amplitude : ℝ) (bin : ℕ) (phase : ℝ) : Buffer ℝ n :=
  exactMath.hann ⟨fun t => sinusoid amplitude ((bin : ℝ) / n) phase t.val⟩

/-- The three positive-frequency coefficients of an interior windowed tone. -/
def hannSinusoidSpectrum {n : ℕ} (amplitude : ℝ) (bin : ℕ) (phase : ℝ) : DFT ℝ n :=
  ⟨fun k => exactMath.polar
    (if k.val = bin then amplitude / 4
     else if k.val + 1 = bin ∨ k.val = bin + 1 then -amplitude / 8
     else 0) phase⟩

end

end DSP.Resynthesis
