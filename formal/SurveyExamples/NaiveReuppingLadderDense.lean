import SurveyExamples.NaiveLadderProcessor

namespace SurveyExamples.NaiveLadder
open DSP

private def bandEdges : Array Float :=
  -- The voice overlay draws twelve octaves from half an engine FFT bin to Nyquist.
  (List.range 145).toArray.map fun i => (96000 / 4096) * Float.pow 2 (Float.ofNat i / 12)

/-- Dense ladder control grid and a semitone survey of the complete log display. -/
def denseRecipe : Survey.Recipe := { recipe with
  axes := #[{ name := "drive", low := 0, high := 1, knots := 73, cells := 2 },
            { name := "resonance", low := 0, high := 1, knots := 73, cells := 2 },
            { name := "cutoffHz", low := 20, high := 96000, logScale := true, knots := 443, cells := 5 }]
  dualRate? := some { edges := bandEdges, splitHz := 640 }
  bandLimitHz := 96000
  spectralMesh? := some #[31,39,333]
  frequencies := (List.range (bandEdges.size - 1)).toArray.map fun i => Float.sqrt (bandEdges[i]! * bandEdges[i+1]!)
  fft := 4096, warm := 786432, frames := 256, records := 16
  training := 1, audit := 512, thresholdsDb := #[0.5,1,2]
}

#survey_processor SurveyExamples.NaiveLadder.prepare SurveyExamples.NaiveLadder.process using denseRecipe
end SurveyExamples.NaiveLadder
