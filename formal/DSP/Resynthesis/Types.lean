import DSP.ComplexValue
import Mathlib.Data.ZMod.Basic

namespace DSP.Resynthesis

structure Buffer (R : Type) (n : ℕ) where
  sample : ZMod n → R

/-- The stored half spectrum includes DC and excludes the source Nyquist bin. -/
structure DFT (R : Type) (n : ℕ) where
  coeff : Fin (n / 2) → ComplexValue R

/-- Index into the positive bins. Its native coefficient index is one greater. -/
abbrev ActiveBin (n : ℕ) := Fin (n / 2 - 1)

namespace ActiveBin

def coefficient {n : ℕ} (bin : ActiveBin n) : Fin (n / 2) :=
  ⟨bin.val + 1, by have h := bin.isLt; omega⟩

def above {n : ℕ} (bin : ActiveBin n) : Option (ActiveBin n) :=
  if h : bin.val + 1 < n / 2 - 1 then some ⟨bin.val + 1, h⟩ else none

def below {n : ℕ} (bin : ActiveBin n) : Option (ActiveBin n) :=
  if h : 0 < bin.val then some ⟨bin.val - 1, by have hb := bin.isLt; omega⟩ else none

end ActiveBin

/-- One value for each bin processed by the vocoder. -/
structure BinTable (A : Type) (n : ℕ) where
  values : Vector A (n / 2 - 1)

namespace BinTable

variable {A : Type} {n : ℕ}

def ofFn (f : ActiveBin n → A) : BinTable A n := ⟨Vector.ofFn f⟩

def get (table : BinTable A n) (bin : ActiveBin n) : A := table.values[bin]

def set (table : BinTable A n) (bin : ActiveBin n) (value : A) : BinTable A n :=
  ⟨table.values.set bin value⟩

end BinTable

abbrev PackedBuffer (R : Type) (n channels : ℕ) := Fin channels → Buffer R n

abbrev PackedDFT (R : Type) (n channels : ℕ) := Fin channels → DFT R n

/-- The exact interpretation evaluates the finite Fourier sums. The native
interpretation uses the existing packed transforms and their shared workspace. -/
structure Fourier (R : Type) (n m channels : ℕ) where
  fft : PackedBuffer R n channels → PackedDFT R n channels
  ifft : PackedDFT R n channels → PackedBuffer R m channels

end DSP.Resynthesis
