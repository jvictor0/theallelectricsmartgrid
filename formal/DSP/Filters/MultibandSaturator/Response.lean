import DSP.Filters.MultibandSaturator
import DSP.Filters.LinkwitzRileySplitter.Weights

namespace DSP.MultibandSaturator

/-- Common-phase LR weights permit a real sum before squaring. -/
def Snapshot.magnitudeSquared {n : Nat} {R : Type} [OfNat R 0] [OfNat R 1]
    [Add R] [Mul R] [Div R] (s : Snapshot n R) (f : ResponseFrequency R) : R :=
  let amplitude := s.masterGain * sum (fun i => s.bandGain i *
    LinkwitzRileySplitter.bandWeight _ s.splitter.tree f.sinHalf f.cosHalf i.val)
  amplitude*amplitude

end DSP.MultibandSaturator
