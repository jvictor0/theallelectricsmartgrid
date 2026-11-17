import Init

namespace DSP

/-- Frequency data shared by response kernels. Preparation belongs outside the
per-snapshot evaluation. Real and imaginary delay components stay in `R`, so
floating-point and channel implementations need no executable complex type.
The record stores data; its exact trigonometric meaning is specified separately. -/
structure ResponseFrequency (R : Type) where
  cyclesPerSample : R
  sinHalf : R
  cosHalf : R
  zInverseReal : R
  zInverseImag : R

namespace ResponseFrequency

def map {R S : Type} (f : R → S) (p : ResponseFrequency R) : ResponseFrequency S :=
  ⟨f p.cyclesPerSample, f p.sinHalf, f p.cosHalf, f p.zInverseReal, f p.zInverseImag⟩

instance : Functor ResponseFrequency where
  map := map

/-- One plotted frequency shared by all channels. Mapping can also convert
precision or select one lane from independently prepared channel frequencies. -/
def broadcast {ι R : Type} (p : ResponseFrequency R) : ResponseFrequency (ι → R) :=
  (fun value _ => value) <$> p

@[simp] theorem map_broadcast {ι R : Type} (p : ResponseFrequency R) (i : ι) :
    (fun value => value i) <$> (broadcast p : ResponseFrequency (ι → R)) = p := rfl

/-- Build the full cache from already prepared half-angle sine and cosine.
There are no trigonometric calls here. Inexact inputs remain inexact data. -/
def fromHalfAngle {R : Type} [OfNat R 1] [OfNat R 2] [Sub R] [Mul R] [Neg R]
    (cyclesPerSample sinHalf cosHalf : R) : ResponseFrequency R :=
  { cyclesPerSample, sinHalf, cosHalf
    zInverseReal := 1 - 2 * sinHalf * sinHalf
    zInverseImag := -(2 * sinHalf * cosHalf) }

end ResponseFrequency
end DSP
