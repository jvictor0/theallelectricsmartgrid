import DSP.Resynthesis.Fourier
import DSP.Resynthesis.Grain
import DSP.IfLess.Real
import DSP.Transcendental.Real
import Mathlib.Analysis.SpecialFunctions.Complex.Arg

namespace DSP.Resynthesis

noncomputable section

def exactMath : GrainMath ℝ ℝ where
  widen := id
  natSample := Nat.cast
  natPhase := Nat.cast
  intPhase := Int.cast
  phaseFloor x := Int.floor x
  phaseRound x := if x < 0 then -Int.floor (-x + 1 / 2) else Int.floor (x + 1 / 2)
  sqrt := Real.sqrt
  magnitude z := ‖(⟨z.real, z.imag⟩ : ℂ)‖
  phase z := Complex.arg ⟨z.real, z.imag⟩ / (2 * Real.pi)
  polar r phase := ⟨r * Trig.cos2pi phase, r * Trig.sin2pi phase⟩
  thresholds peak := (peak * 0.000251188643150958, peak * 0.001)
  slewCoefficient cutoff := 1 - ExpDecay.expNegTwoPi (min 0.499 cutoff)
  hann {m} buffer := ⟨fun t => buffer.sample t * (0.5 * (1 - Trig.cos2pi ((t.val : ℝ) / m)))⟩

end

end DSP.Resynthesis
