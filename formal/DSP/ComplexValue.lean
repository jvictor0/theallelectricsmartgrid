import DSP.Response

namespace DSP.ComplexValue
variable {R : Type}

def ofReal [OfNat R 0] (x : R) : ComplexValue R := ⟨x, 0⟩
def add [Add R] (a b : ComplexValue R) : ComplexValue R :=
  ⟨a.real + b.real, a.imag + b.imag⟩
def sub [Sub R] (a b : ComplexValue R) : ComplexValue R :=
  ⟨a.real - b.real, a.imag - b.imag⟩
def scale [Mul R] (x : R) (a : ComplexValue R) : ComplexValue R :=
  ⟨x * a.real, x * a.imag⟩
def mul [Add R] [Sub R] [Mul R] (a b : ComplexValue R) : ComplexValue R :=
  ⟨a.real * b.real - a.imag * b.imag, a.real * b.imag + a.imag * b.real⟩
def div [Add R] [Sub R] [Mul R] [Div R] (a b : ComplexValue R) : ComplexValue R :=
  let d := b.real * b.real + b.imag * b.imag
  ⟨(a.real * b.real + a.imag * b.imag) / d,
    (a.imag * b.real - a.real * b.imag) / d⟩

/-- Reference arithmetic for an integer delay. A backend may replace this with
prepared phase data once its correspondence has been established. -/
def pow [OfNat R 0] [OfNat R 1] [Add R] [Sub R] [Mul R]
    (a : ComplexValue R) : Nat → ComplexValue R
  | 0 => ofReal 1
  | n + 1 => mul (pow a n) a

def normSquared [Mul R] [FMA R] (a : ComplexValue R) : R :=
  Response.normSquared a.real a.imag

end DSP.ComplexValue
