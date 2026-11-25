import Lean

namespace DSP.Survey
open Lean

/-- Sampling and interpolation use equally spaced linear or logarithmic coordinates.
    In point mode each audit cell is an atom at the corresponding grid point. -/
structure Axis where
  name : String
  low : Float
  high : Float
  logScale : Bool := false
  knots : Nat := 2
  cells : Nat := 1
  points : Bool := false
  deriving ToJson, Inhabited

/-- Independent input histories start at reset. An optional prelude drives a
    reachable starting state before the measurement stimulus begins. -/
structure Stimulus where
  name : String := "reset"
  mass : Nat := 1
  rms : Float := 0.125
  prelude : Nat := 0
  preludeRms : Float := 0.125
  deriving ToJson, Inhabited

/-- Two-rate 4096-point Hann analysis with causal 4x and 8x decimation.
    edges are the band boundaries in Hz, splitHz selects the bass analysis. -/
structure DualRate where
  edges : Array Float
  splitHz : Float
  trainingTile : Nat := 256
  deriving ToJson, Inhabited

structure Recipe where
  axes : Array Axis
  /-- Align spectra by frequency divided by this positive control before blending
      neighbouring settings. Omit to blend at fixed absolute frequency. -/
  frequencyRelativeTo? : Option String := none
  stimuli : Array Stimulus := #[{}]
  /-- Row-major joint masses over axis cells, then stimulus. Empty uses
      equal axis-cell masses and the specified stimulus masses. -/
  masses : Array Nat := #[]
  sampleRate : Nat := 48000
  /-- Zero keeps the entire native band; otherwise truncate the noise DFT at this Hz. -/
  bandLimitHz : Float := 0
  fft : Nat := 4096
  warm : Nat := 4096
  frames : Nat := 4
  records : Nat := 4
  /-- Training repeats per vertex and stimulus; audit trials per positive-mass cell. -/
  training : Nat := 1
  audit : Nat := 64
  dualRate? : Option DualRate := none
  /-- Three nonuniform control meshes. The third axis indexes the spectral bases. -/
  spectralMesh? : Option (Array Nat) := none
  frequencies : Array Float
  floorDb : Float := -60
  budgetBytes : Nat := 10000000
  thresholdsDb : Array Float := #[0.5]
  /-- Requested pass fractions and total family risk in parts per million. -/
  coveragePPM : Array Nat := #[950000]
  riskPPM : Nat := 50000
  deriving ToJson, Inhabited
end DSP.Survey
