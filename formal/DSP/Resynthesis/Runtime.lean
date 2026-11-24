import DSP.Resynthesis.Grain
import DSP.DelayMath

namespace DSP.Resynthesis.Runtime

structure OscillatorControl where
  gain : Bool
  value : Float32
  oscillator : Fin 3

def oscillatorControl32 (input : OscillatorControl) : Float32 :=
  if input.gain then gainFor Float32.sqrt input.value input.oscillator
  else detuneFor input.value input.oscillator

structure UnisonBaseInput where
  count : Fin 5

structure UnisonBase (R : Type) where
  lanes : Vector R 4

def unisonBase {R : Type} [OfNat R 40] [OfNat R 5] [OfNat R 1200] [OfScientific R]
    [Add R] [Mul R] [Div R] (ofNat : Nat → R) (exp2 : R → R)
    (input : UnisonBaseInput) : UnisonBase R :=
  ⟨Vector.ofFn fun lane =>
    if input.count.val = 4 then exp2 ((40 + 5 * ofNat lane.val) / 1200) else 1.03⟩

def unisonBase32 := unisonBase Float32.ofNat DelayMath.exp2Float32
def unisonBase64 := unisonBase Float.ofNat DelayMath.exp2Float

/-- The packed native spectrum keeps a DC slot alongside the active bins.
The vocoder analyzes and synthesizes only the active bins. -/
structure Magnitudes (R : Type) where
  values : Vector R 2048

structure SlewInput (R : Type) where
  target : Magnitudes R
  alpha : R

def slewAction {R : Type} [Add R] [Sub R] [Mul R] [IfLess R]
    (input : SlewInput R) : StateM (Magnitudes R) PUnit := do
  let previous : Magnitudes R ← get
  set (Magnitudes.mk (slewBins input.alpha previous.values input.target.values))

def slewSpectrumMagnitudes {R : Type} [Add R] [Sub R] [Mul R] [IfLess R]
    (previous : Magnitudes R) (input : SlewInput R) : Magnitudes R :=
  ((slewAction input).run previous).2

theorem slewSpectrumMagnitudes_active {R : Type} [Add R] [Sub R] [Mul R] [IfLess R]
    (previous : Magnitudes R) (input : SlewInput R) (bin : ActiveBin 4096) :
    (slewSpectrumMagnitudes previous input).values.get bin.coefficient =
      (slewBins input.alpha (Vector.ofFn fun k : ActiveBin 4096 => previous.values.get k.coefficient)
        (Vector.ofFn fun k : ActiveBin 4096 => input.target.values.get k.coefficient)).get bin := by
  change (slewBins input.alpha previous.values input.target.values).get bin.coefficient = _
  have getOfFn {count : ℕ} (f : Fin count → R) (i : Fin count) : (Vector.ofFn f).get i = f i :=
    Vector.getElem_ofFn i.isLt
  simp only [slewBins, getOfFn]

end DSP.Resynthesis.Runtime
