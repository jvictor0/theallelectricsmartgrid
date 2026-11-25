import DSP.Filters.SquiggleBoyQuadReverb.Frozen.Harmonic

namespace DSP.SquiggleBoyQuadReverb.Frozen

noncomputable section

def tapeMode (n : ℕ) (q : ℂ) (x : Lanes ℂ) : Tape n ℂ :=
  fun j => q ^ (j.val + 1) • x

def apfMode {n : ℕ} (p : Tap n ℂ) (q : ℂ) (x : Lanes ℂ) : Tape n ℂ :=
  tapeMode n q (x + (allPassGain : ℂ) • (allPassResponse p q * x))

def introMode (q : ℂ) : (order : List (Fin 8)) →
    IntroTaps order ℂ → Lanes ℂ → IntroHistory order ℂ
  | [], _, _ => fun j => j.elim0
  | i :: order, taps, x =>
    introCons (F := fun j => Tape (squiggleBoyReverbIntroAPFCapacity j) ℂ)
      (i := i) (order := order) (apfMode (taps 0) q x)
      (introMode q order (fun j => taps j.succ) (allPassResponse (taps 0) q * x))

def lowMemoryMode (alpha : Lanes ℂ) (q : ℂ) (x : Lanes ℂ) : Lanes ℂ :=
  fun i => (2 * alpha i * q / (1 + (2 * alpha i - 1)*q)) * x i

def dampingMode (p : Spec ℂ) (q : ℂ) (x : Lanes ℂ) : Lanes ℂ × Lanes ℂ :=
  (lowMemoryMode p.highAlpha q x,
    lowMemoryMode p.lowAlpha q (fun i => highResponse (p.highAlpha i) q * x i))

end

end DSP.SquiggleBoyQuadReverb.Frozen
