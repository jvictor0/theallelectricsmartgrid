import DSP.Survey.Generate
import DSP.Filters.NaiveReuppingLadder

namespace SurveyExamples.NaiveLadder
open DSP

structure Config where
  drive : Float32
  resonance : Float32
  cutoffHz : Float32

private def silence : NaiveReuppingLadder Float32 :=
  let svf : TPTSVF Float32 := ⟨0,0,0,0,0,0,0,0,0,0,0⟩
  let detector : BandPassedEnvelopeFollower Float32 := ⟨svf,svf,0,0⟩
  ⟨0,⟨1,1⟩,0,0,0,0,0,0,0,0,0,detector,detector,0,0,0,0,0⟩

def prepare (c : Config) : NaiveReuppingLadder Float32 :=
  let resonance := (DelayMath.exp2 (5.614709844115208 * c.resonance) - 1) / 48
  let gain := 0.25 * DelayMath.exp2 (4.321928094887363 * c.drive)
  NaiveReuppingLadder.computeCoefficients ⟨c.cutoffHz / 192000, resonance, gain⟩ silence

def process (s : NaiveReuppingLadder Float32) (x : Float32) :=
  let t := s.step ⟨x⟩
  (t, t.output)

/-- A short finite-record pilot across the complete knob box. -/
def recipe : Survey.Recipe := {
  axes := #[{ name := "drive", low := 0, high := 1, knots := 3, cells := 2 },
            { name := "resonance", low := 0, high := 1, knots := 3, cells := 2 },
            { name := "cutoffHz", low := 20, high := 20000, logScale := true, knots := 7, cells := 2 }]
  sampleRate := 192000, bandLimitHz := 24000, fft := 4096, warm := 8192, frames := 4
  records := 2, training := 2, audit := 32
  stimuli := #[{ name := "reset", rms := 0.1767766952966369 }]
  frequencies := #[20,32,50,80,125,200,315,500,800,1250,2000,3150,5000,8000,12500,20000]
  thresholdsDb := #[0.5, 2, 6], coveragePPM := #[500000,800000,950000]
}
end SurveyExamples.NaiveLadder
