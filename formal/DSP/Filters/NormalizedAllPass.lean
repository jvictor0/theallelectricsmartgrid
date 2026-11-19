import DSP.Step

namespace DSP

/-- One-state normalized allpass lattice, with optional radial memory loss.
This is the allpass update in `TunedDecayComb::Branch::Process`: `s` is its
`m_allpass`, and the input is the already attenuated integer-delay read.
Normalization `a²+b²=1` is a proof hypothesis, not a floating-point assertion.
The lossless section has `r=1`; the comb uses `0<r<1`. -/
structure NormalizedAllPass (R : Type) where
  a : R
  b : R
  r : R
  s : R
  y : R

namespace NormalizedAllPass

variable {R : Type} [Add R] [Sub R] [Mul R]

/-- Read the old memory once, then compute both new memory and current output.
Separate multiply/add operations retain the handwritten comb's operation order. -/
def step (f : NormalizedAllPass R) (input : Sample R) : NormalizedAllPass R :=
  let memory := f.r * f.s
  let y := f.a * input.x + f.b * memory
  { f with s := f.b * input.x - f.a * memory, y }

def stepFloat : Step (NormalizedAllPass Float) (Sample Float) := step

end NormalizedAllPass
end DSP
