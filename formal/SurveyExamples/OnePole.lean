import DSP.Survey.Generate
import DSP.Filters.OnePoleZOHLowPass

namespace SurveyExamples.OnePole
structure Config where
  alpha : Float32

def prepare (c : Config) : DSP.OnePoleZOHLowPass Float32 := ⟨c.alpha, 0⟩
def process (s : DSP.OnePoleZOHLowPass Float32) (x : Float32) :=
  let t := s.process x
  (t, t.y)

def recipe : DSP.Survey.Recipe := {
  axes := #[{ name := "alpha", low := 0.04, high := 0.8, logScale := true, knots := 5, cells := 2 }]
  fft := 256, frames := 8, warm := 512, records := 2, training := 2, audit := 256
  frequencies := #[187.5, 375, 750, 1500, 3000, 6000, 12000, 20000]
  coveragePPM := #[500000, 800000, 950000], thresholdsDb := #[0.5, 1.0]
}
#survey_processor SurveyExamples.OnePole.prepare SurveyExamples.OnePole.process using recipe
end SurveyExamples.OnePole
