import Init

namespace DSP

/-- The two numeric components of a complex value, matching the app's
MultiChannelComplex storage when `R` is a channel-valued number. -/
structure ComplexValue (R : Type) where
  real : R
  imag : R

/-- Frequency data shared by response kernels. Preparation belongs outside the
per-snapshot evaluation. The complex delay stores two components in `R`.
The record stores data; its exact trigonometric meaning is specified separately. -/
structure ResponseFrequency (R : Type) where
  cyclesPerSample : R
  sinHalf : R
  cosHalf : R
  zInverse : ComplexValue R

namespace ResponseFrequency

def map {R S : Type} (f : R → S) (p : ResponseFrequency R) : ResponseFrequency S :=
  ⟨f p.cyclesPerSample, f p.sinHalf, f p.cosHalf, ⟨f p.zInverse.real, f p.zInverse.imag⟩⟩

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
    zInverse := ⟨1 - 2 * sinHalf * sinHalf, -(2 * sinHalf * cosHalf)⟩ }

end ResponseFrequency
end DSP
