import DSP.Step

namespace DSP

/-- Linear trapezoidal state-variable filter, in the operation order of the
app's `LinearStateVariableFilter`. `k` is the damping 1/Q, and a1, a2, a3 are
the solved zero-delay loop coefficients for integrator gain g (ideally
tan(π f)): a1 = 1 / (1 + g (g + k)), a2 = g a1, a3 = g a2. `ic1` and `ic2`
are the integrator states; every response is stored after each sample. -/
structure TPTSVF (R : Type) where
  k : R
  a1 : R
  a2 : R
  a3 : R
  ic1 : R
  ic2 : R
  lowPass : R
  bandPass : R
  highPass : R
  notch : R

namespace TPTSVF

variable {R : Type} [Add R] [Sub R] [Mul R] [OfNat R 2]

def step (f : TPTSVF R) (input : Sample R) : TPTSVF R :=
  let v3 := input.x - f.ic2
  let v1 := f.a1 * f.ic1 + f.a2 * v3
  let v2 := f.ic2 + f.a2 * f.ic1 + f.a3 * v3
  let highPass := input.x - f.k * v1 - v2
  { f with
    ic1 := v1 * 2 - f.ic1
    ic2 := v2 * 2 - f.ic2
    lowPass := v2
    bandPass := v1
    highPass
    notch := v2 + highPass }

def stepFloat : Step (TPTSVF Float) (Sample Float) := step

end TPTSVF
end DSP
