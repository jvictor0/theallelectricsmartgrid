import DSP.Tests.NaiveReuppingLadder

/-! Exploratory finite-level measurements. This executes the existing Lean
ladder and its nested filters in binary64. It does not replace their equations.
Each tone starts from reset. Consecutive integer-period windows expose drift;
finite measurements are not a convergence theorem. Output floats are emitted
as IEEE bits so tiny powers survive the native Float string formatting. -/

namespace DSP.Tests.FiniteLevelProbe

def sampleRate : Float := 192000
def cutoffHz : Float := 1000
def tau : Float := 6.2831853071795864769

def prepare (nativeTanh detector : Bool) (driveKnob resonanceKnob : Float) : DSP.NaiveReuppingLadder Float :=
  letI : Tanh Float := ⟨if nativeTanh then Float.tanh else Tanh.rational⟩
  let gain := 0.25 * Float.exp (Float.log 20 * driveKnob)
  let resonance := (Float.exp (Float.log 49 * resonanceKnob)-1)/48
  let s := DSP.NaiveReuppingLadder.computeCoefficients ⟨cutoffHz/sampleRate, resonance, gain⟩
    (DSP.Tests.NaiveReuppingLadder.seeded 0)
  if detector then s else {s with dampingScale := 0}

structure Tone where
  sine : Float
  cosine : Float
  sine3 : Float
  cosine3 : Float
  deriving Inhabited

def tones (period : Nat) : Array Tone := Id.run do
  let mut result := #[]
  for j in [0:period] do
    let phase := tau*j.toFloat/period.toFloat
    result := result.push ⟨phase.sin, phase.cos, (3*phase).sin, (3*phase).cos⟩
  return result

structure Measurement where
  power : Float := 0
  mean : Float := 0
  realPart : Float := 0
  imagPart : Float := 0
  thirdReal : Float := 0
  thirdImag : Float := 0
  kMean : Float := 0
  kMin : Float := 1000000
  kMax : Float := 0
  peak : Float := 0

def accumulate (m : Measurement) (y k : Float) (t : Tone) : Measurement :=
  {power := m.power+y*y, mean := m.mean+y
   realPart := m.realPart+y*t.cosine, imagPart := m.imagPart-y*t.sine
   thirdReal := m.thirdReal+y*t.cosine3, thirdImag := m.thirdImag-y*t.sine3
   kMean := m.kMean+k, kMin := min m.kMin k, kMax := max m.kMax k
   peak := max m.peak y.abs}

def normalized (m : Measurement) (n : Nat) : Measurement :=
  let count := n.toFloat
  {m with
    power := m.power/count, mean := m.mean/count,
    realPart := 2*m.realPart/count, imagPart := 2*m.imagPart/count,
    thirdReal := 2*m.thirdReal/count, thirdImag := 2*m.thirdImag/count,
    kMean := m.kMean/count}

def fundamentalPower (m : Measurement) : Float := (m.realPart*m.realPart+m.imagPart*m.imagPart)/2
def thirdPower (m : Measurement) : Float := (m.thirdReal*m.thirdReal+m.thirdImag*m.thirdImag)/2
def bits (values : List Float) : String := String.intercalate "," (values.map (fun v => toString v.toBits))

def run (nativeTanh detector : Bool) (driveKnob amplitude : Float) (period scale : Nat)
    (trace : Bool := false) (resonanceKnob : Float := 1) (captureCycles : Nat := 0) : IO Unit := do
  let _ : Tanh Float := ⟨if nativeTanh then Float.tanh else Tanh.rational⟩
  let initial := prepare nativeTanh detector driveKnob resonanceKnob
  let mut state := initial
  let table := tones period
  let warmCycles := max 32 ((15360+period-1)/period)
  let measuredCycles := max 16 ((7680+period-1)/period)
  let warmSamples := warmCycles*period*scale
  let measuredSamples := measuredCycles*period*scale
  for i in [0:warmSamples] do
    state := DSP.NaiveReuppingLadder.step state ⟨amplitude*table[i%period]!.sine⟩
  let mut previous : Measurement := {}
  let mut current : Measurement := {}
  for window in [0:2] do
    let mut measured : Measurement := {}
    for i in [0:measuredSamples] do
      let t := table[i%period]!
      state := DSP.NaiveReuppingLadder.step state ⟨amplitude*t.sine⟩
      if state.output.isNaN || state.output.isInf then throw (IO.userError "nonfinite finite-level output")
      measured := accumulate measured state.output state.kEff t
      if trace && window == 1 && (captureCycles == 0 || i+captureCycles*period >= measuredSamples) then
        IO.println s!"sample,{if nativeTanh then "tanh" else "rational"},{if detector then "current" else "off"},{driveKnob},{amplitude},{resonanceKnob},{period},{i},{bits [state.output,state.kEff]}"
    previous := current
    current := normalized measured measuredSamples
  if !trace then
    IO.println s!"{if nativeTanh then "tanh" else "rational"},{if detector then "current" else "off"},{driveKnob},{amplitude},{period},{warmSamples},{measuredSamples},{bits [sampleRate/period.toFloat,initial.saturation.gain,initial.baseFeedback,current.kMean,current.kMin,current.kMax,current.realPart,current.imagPart,fundamentalPower current,current.power,thirdPower current,max 0 (current.power-fundamentalPower current),current.mean*current.mean,current.peak,previous.power,fundamentalPower previous,previous.realPart,previous.imagPart,resonanceKnob]}"

def crossing : Float :=
  let sine := Float.sin (3.14159265358979323846*cutoffHz/sampleRate)
  let ratio := 2*sine*((1+sine*sine).sqrt+sine)
  Float.asin (ratio/2)*sampleRate/3.14159265358979323846

def periodFor (frequency : Float) : Nat := (sampleRate/frequency+0.5).toUInt64.toNat

def periods : List Nat := Id.run do
  let mut result : List Nat := []
  for j in [0:145] do
    let frequency := 50*Float.exp (Float.log (24000/50)*j.toFloat/144)
    result := periodFor frequency :: result
  for ratio in [0.2, 1/3, 0.5, 1, 2, 3] do
    result := periodFor (crossing*ratio) :: result
  return result.mergeSort (· ≥ ·) |>.eraseDups

def header : String :=
  "model,detector,driveKnob,amplitude,period,warmSamples,measuredSamples,frequencyHz_bits,gain_bits,kBase_bits,kMean_bits,kMin_bits,kMax_bits,re_bits,im_bits,fundamentalPower_bits,totalPower_bits,thirdPower_bits,residualPower_bits,dcPower_bits,peak_bits,previousPower_bits,previousFundamentalPower_bits,previousRe_bits,previousIm_bits,resonanceKnob_bits"

def ringdown (detector : Bool) (resonance : Float) : IO Unit := do
  let mut state := prepare false detector 1 resonance
  let period := periodFor (crossing/3)
  let table := tones period
  for i in [0:period*64] do
    state := DSP.NaiveReuppingLadder.step state ⟨table[i%period]!.sine⟩
  for block in [0:10] do
    let mut power : Float := 0
    for _ in [0:19200] do
      state := DSP.NaiveReuppingLadder.step state ⟨0⟩
      power := power+state.output*state.output
    IO.println s!"{detector},{resonance},{block},{bits [power/19200,state.kEff]}"

end DSP.Tests.FiniteLevelProbe

open DSP.Tests.FiniteLevelProbe in
def main (args : List String) : IO Unit := do
  if args == ["quick"] then
    IO.println header
    for detector in [false,true] do
      for ratio in [1/3,1,3] do
        run false detector 1 1 (periodFor (crossing*ratio)) 1
  else if args == ["sweep"] then
    IO.println header
    for drive in [0.5,0.75,1] do
      for amplitude in [1,0.25,0.0625] do
        for detector in [false,true] do
          IO.eprintln s!"sweep rational drive={drive} amplitude={amplitude} detector={detector}"
          for period in periods do
            run false detector drive amplitude period 1
  else if args == ["tanh"] then
    IO.println header
    for amplitude in [1,0.25,0.0625] do
      for detector in [false,true] do
        IO.eprintln s!"sweep tanh drive=1 amplitude={amplitude} detector={detector}"
        for period in periods do
          run true detector 1 amplitude period 1
  else if args == ["mid"] then
    IO.println header
    for resonance in [0.5,0.75] do
      for drive in [0.5,0.75,1] do
        for amplitude in [1,0.25,0.0625] do
          for detector in [false,true] do
            IO.eprintln s!"sweep rational resonance={resonance} drive={drive} amplitude={amplitude} detector={detector}"
            for period in periods do
              run false detector drive amplitude period 1 false resonance
  else if args == ["long"] then
    IO.println header
    for model in [false,true] do
      for amplitude in [1,0.25,0.0625] do
        for detector in [false,true] do
          for ratio in [0.2,1/3,0.5,1,2,3] do
            run model detector 1 amplitude (periodFor (crossing*ratio)) 8
  else if args == ["trace"] then
    for amplitude in [1,0.25] do
      for detector in [false,true] do
        for ratio in [1/3,1,3] do
          run false detector 1 amplitude (periodFor (crossing*ratio)) 8 true
  else if args == ["mid-trace"] then
    for amplitude in [1,0.25] do
      for detector in [false,true] do
        for ratio in [1/3,1,3] do
          run false detector 1 amplitude (periodFor (crossing*ratio)) 8 true 0.5
  else if args == ["mid-long"] then
    IO.println header
    for drive in [0.5,0.75,1] do
      for amplitude in [1,0.25,0.0625] do
        for detector in [false,true] do
          for ratio in [0.2,1/3,0.5,1,2,3] do
            run false detector drive amplitude (periodFor (crossing*ratio)) 8 false 0.5
  else if args == ["spectra"] then
    for resonance in [0.5,0.75,1] do
      for drive in [0.5,0.75,1] do
        for amplitude in [1,0.25,0.0625] do
          for detector in [false,true] do
            IO.eprintln s!"spectra resonance={resonance} drive={drive} amplitude={amplitude} detector={detector}"
            for ratio in [1/3,1,3] do
              run false detector drive amplitude (periodFor (crossing*ratio)) 8 true resonance 32
  else if args.head? == some "refine" && args.length == 2 then
    IO.println header
    let content ← IO.FS.readFile args[1]!
    for line in content.splitOn "\n" do
      if !line.isEmpty then
        match line.splitOn "," with
        | [drive,resonance,amplitudeIndex,detector,period,scale] =>
          let amplitude := if amplitudeIndex == "0" then 1 else if amplitudeIndex == "1" then 0.25 else 0.0625
          run false (detector == "1") (drive.toNat!.toFloat/100) amplitude period.toNat! scale.toNat! false (resonance.toNat!.toFloat/100)
        | _ => throw (IO.userError "invalid refinement row")
  else if args == ["ringdown"] then
    for resonance in [0.5,1] do
      for detector in [false,true] do
        ringdown detector resonance
  else
    throw (IO.userError "expected quick, sweep, tanh, mid, long, trace, mid-trace, mid-long, spectra, or refine <csv>")
