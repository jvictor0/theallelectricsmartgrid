import DSP.Response
import DSP.Filters.OnePoleTPTLowPass
import DSP.Filters.OnePoleTPTHighPass
import DSP.Filters.TPTBaseWidth
import DSP.Filters.MorphingTPTSVF
import DSP.Filters.TPTSVF4Pole
import DSP.Filters.TPTLadder

/-! Coefficient snapshots for frozen, tuned filters. Gains are the integrator
gains used to tune the processors (G=g/(1+g) for a one-pole); retaining these
avoids reconstructing g from rounded solved coefficients. Capture/tuning remains
the caller's responsibility. Each proof states the relation to processing state. -/
namespace DSP

namespace OnePoleTPTLowPass
structure Snapshot (R : Type) where
  g : R

def Snapshot.magnitudeSquared {R : Type} [OfNat R 0] [OfNat R 1]
    [Mul R] [Div R] [FMA R] [IfLess R] (s : Snapshot R) (f : ResponseFrequency R) : R :=
  let gc := s.g * f.cosHalf
  Response.ratio (gc * gc) (Response.normSquared gc f.sinHalf)
def Snapshot.magnitudeSquaredFloat := Snapshot.magnitudeSquared (R := Float)
end OnePoleTPTLowPass

namespace OnePoleTPTHighPass
structure Snapshot (R : Type) where
  g : R

def Snapshot.magnitudeSquared {R : Type} [OfNat R 0] [OfNat R 1]
    [Mul R] [Div R] [FMA R] [IfLess R] (s : Snapshot R) (f : ResponseFrequency R) : R :=
  Response.ratio (f.sinHalf * f.sinHalf) (Response.normSquared (s.g * f.cosHalf) f.sinHalf)
def Snapshot.magnitudeSquaredFloat := Snapshot.magnitudeSquared (R := Float)
end OnePoleTPTHighPass

namespace TPTBaseWidth
structure Snapshot (R : Type) where
  highPass : OnePoleTPTHighPass.Snapshot R
  lowPass : OnePoleTPTLowPass.Snapshot R

def Snapshot.magnitudeSquared {R : Type} [OfNat R 0] [OfNat R 1]
    [Mul R] [Div R] [FMA R] [IfLess R] (s : Snapshot R) (f : ResponseFrequency R) : R :=
  s.highPass.magnitudeSquared f * s.lowPass.magnitudeSquared f
def Snapshot.magnitudeSquaredFloat := Snapshot.magnitudeSquared (R := Float)
end TPTBaseWidth

namespace TPTSVF
structure Snapshot (R : Type) where
  g : R
  k : R

/-- Shared SVF algebra retains the relative phase of low, band and high outputs. -/
def Snapshot.weightedMagnitudeSquared {R : Type} [OfNat R 0] [OfNat R 1]
    [Sub R] [Mul R] [Div R] [FMA R] [IfLess R]
    (s : Snapshot R) (low band high : R) (f : ResponseFrequency R) : R :=
  let gc := s.g * f.cosHalf
  let gc2 := gc * gc
  let sin2 := f.sinHalf * f.sinHalf
  let cross := gc * f.sinHalf
  let nr := low * gc2 - high * sin2
  let ni := band * cross
  let dr := gc2 - sin2
  let di := s.k * cross
  Response.ratio (Response.normSquared nr ni) (Response.normSquared dr di)

def Snapshot.lowPassMagnitudeSquared {R : Type} [OfNat R 0] [OfNat R 1]
    [Sub R] [Mul R] [Div R] [FMA R] [IfLess R] (s : Snapshot R) (f : ResponseFrequency R) : R :=
  s.weightedMagnitudeSquared 1 0 0 f

def Snapshot.bandPassMagnitudeSquared {R : Type} [OfNat R 0] [OfNat R 1]
    [Sub R] [Mul R] [Div R] [FMA R] [IfLess R] (s : Snapshot R) (f : ResponseFrequency R) : R :=
  s.weightedMagnitudeSquared 0 1 0 f

def Snapshot.highPassMagnitudeSquared {R : Type} [OfNat R 0] [OfNat R 1]
    [Sub R] [Mul R] [Div R] [FMA R] [IfLess R] (s : Snapshot R) (f : ResponseFrequency R) : R :=
  s.weightedMagnitudeSquared 0 0 1 f

def Snapshot.notchMagnitudeSquared {R : Type} [OfNat R 0] [OfNat R 1]
    [Sub R] [Mul R] [Div R] [FMA R] [IfLess R] (s : Snapshot R) (f : ResponseFrequency R) : R :=
  s.weightedMagnitudeSquared 1 0 1 f

def Snapshot.lowPassMagnitudeSquaredFloat := Snapshot.lowPassMagnitudeSquared (R := Float)
def Snapshot.bandPassMagnitudeSquaredFloat := Snapshot.bandPassMagnitudeSquared (R := Float)
def Snapshot.highPassMagnitudeSquaredFloat := Snapshot.highPassMagnitudeSquared (R := Float)
def Snapshot.notchMagnitudeSquaredFloat := Snapshot.notchMagnitudeSquared (R := Float)
def Snapshot.allPassMagnitudeSquared {R : Type} [OfNat R 0] [OfNat R 1]
    [Sub R] [Mul R] [Div R] [FMA R] [IfLess R] (s : Snapshot R) (f : ResponseFrequency R) : R :=
  s.weightedMagnitudeSquared 1 (0 - s.k) 1 f

def Snapshot.allPassMagnitudeSquaredFloat := Snapshot.allPassMagnitudeSquared (R := Float)
end TPTSVF

namespace MorphingTPTSVF
structure Snapshot (R : Type) where
  svf : TPTSVF.Snapshot R
  low : R
  band : R
  high : R

def Snapshot.magnitudeSquared {R : Type} [OfNat R 0] [OfNat R 1]
    [Sub R] [Mul R] [Div R] [FMA R] [IfLess R] (s : Snapshot R) (f : ResponseFrequency R) : R :=
  s.svf.weightedMagnitudeSquared s.low s.band s.high f

def Snapshot.magnitudeSquaredFloat := Snapshot.magnitudeSquared (R := Float)
end MorphingTPTSVF

namespace TPTSVF4Pole
structure Snapshot (R : Type) where
  stage1 : TPTSVF.Snapshot R
  stage2 : TPTSVF.Snapshot R

def Snapshot.lowPassMagnitudeSquared {R : Type} [OfNat R 0] [OfNat R 1]
    [Sub R] [Mul R] [Div R] [FMA R] [IfLess R] (s : Snapshot R) (f : ResponseFrequency R) : R :=
  s.stage1.lowPassMagnitudeSquared f * s.stage2.lowPassMagnitudeSquared f

def Snapshot.lowPassMagnitudeSquaredFloat := Snapshot.lowPassMagnitudeSquared (R := Float)

def Snapshot.bandPassMagnitudeSquared {R : Type} [OfNat R 0] [OfNat R 1]
    [Sub R] [Mul R] [Div R] [FMA R] [IfLess R] (s : Snapshot R) (f : ResponseFrequency R) : R :=
  s.stage1.bandPassMagnitudeSquared f * s.stage2.bandPassMagnitudeSquared f

def Snapshot.bandPassMagnitudeSquaredFloat := Snapshot.bandPassMagnitudeSquared (R := Float)

def Snapshot.highPassMagnitudeSquared {R : Type} [OfNat R 0] [OfNat R 1]
    [Sub R] [Mul R] [Div R] [FMA R] [IfLess R] (s : Snapshot R) (f : ResponseFrequency R) : R :=
  s.stage1.highPassMagnitudeSquared f * s.stage2.highPassMagnitudeSquared f

def Snapshot.highPassMagnitudeSquaredFloat := Snapshot.highPassMagnitudeSquared (R := Float)
end TPTSVF4Pole


namespace TPTLadder
structure Snapshot (R : Type) where
  g1 : R
  g2 : R
  g3 : R
  g4 : R
  k : R

/-- Multiply the four complex stage denominators before closing the feedback
loop. Unequal stage gains are supported, as in the stream realization proof. -/
def Snapshot.magnitudeSquared {R : Type} [OfNat R 0] [OfNat R 1]
    [Sub R] [Neg R] [Mul R] [Div R] [FMA R] [IfLess R]
    (s : Snapshot R) (f : ResponseFrequency R) : R :=
  let a := s.g1 * f.cosHalf
  let b := s.g2 * f.cosHalf
  let c := s.g3 * f.cosHalf
  let d := s.g4 * f.cosHalf
  let t := f.sinHalf
  let r12 := FMA.fma a b (-(t * t))
  let i12 := FMA.fma a t (t * b)
  let r123 := FMA.fma r12 c (-(i12 * t))
  let i123 := FMA.fma r12 t (i12 * c)
  let n := a * b * c * d
  let dr := FMA.fma s.k n (FMA.fma r123 d (-(i123 * t)))
  let di := FMA.fma r123 t (i123 * d)
  Response.ratio (n * n) (Response.normSquared dr di)

def Snapshot.magnitudeSquaredFloat := Snapshot.magnitudeSquared (R := Float)
end TPTLadder
end DSP
