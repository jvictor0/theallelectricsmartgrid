import Init

namespace DSP

/-- Average the arithmetic lanes and broadcast to every lane. -/
class ChannelMean (R : Type) where
  mean : R → R

namespace ChannelMean

/-- Ascending left-to-right sum, including the initial zero. -/
def sum {R : Type} [OfNat R 0] [Add R] : {n : Nat} → (Fin n → R) → R
  | 0, _ => 0
  | n + 1, values => sum (fun i => values i.castSucc) + values (Fin.last n)

def channels {n : Nat} [NeZero n] {R : Type}
    [OfNat R 0] [OfNat R n] [Add R] [Div R] (values : Fin n → R) : Fin n → R :=
  let mean := sum values / (OfNat.ofNat n : R)
  fun _ => mean

instance {n : Nat} [NeZero n] {R : Type} [OfNat R 0] [OfNat R n] [Add R] [Div R] :
    ChannelMean (Fin n → R) := ⟨channels⟩

end ChannelMean
end DSP
