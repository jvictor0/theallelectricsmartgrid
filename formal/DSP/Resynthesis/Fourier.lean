import DSP.Resynthesis.Types
import Mathlib.Analysis.Fourier.ZMod

namespace DSP.Resynthesis

open scoped ZMod

noncomputable section

def fft {n : ℕ} [NeZero n] (buffer : Buffer ℝ n) : DFT ℝ n where
  coeff k :=
    let z := 𝓕 (fun t => (buffer.sample t : ℂ)) (k.val : ZMod n) / (n : ℂ)
    ⟨z.re, z.im⟩

/-- Lift a stored half spectrum to the inverse transform grid. The bin numbers
are unchanged, including when the inverse grid has more samples. -/
def inverseSpectrum {n m : ℕ} (spectrum : DFT ℝ n) (k : ZMod m) : ℂ :=
  if h : k.val < n / 2 then
    let z := spectrum.coeff ⟨k.val, h⟩
    ⟨z.real, z.imag⟩
  else if h : m - k.val < n / 2 then
    let z := spectrum.coeff ⟨m - k.val, h⟩
    ⟨z.real, -z.imag⟩
  else 0

def ifft {n m : ℕ} [NeZero m] (spectrum : DFT ℝ n) : Buffer ℝ m where
  sample t := ((m : ℂ) * (𝓕⁻ (inverseSpectrum spectrum)) t).re

def exactFourier {n m channels : ℕ} [NeZero n] [NeZero m] : Fourier ℝ n m channels where
  fft buffer lane := fft (buffer lane)
  ifft spectrum lane := ifft (spectrum lane)

end

end DSP.Resynthesis
