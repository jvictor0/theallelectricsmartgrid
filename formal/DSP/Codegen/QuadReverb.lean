import DSP.Codegen.Cpp
import DSP.Filters.SquiggleBoyQuadReverb.Loop

namespace DSP.Codegen
open Lean Meta Elab Command
open SquiggleBoyQuadReverb

private def vector (name : String) : Value := .scalar "Vector" name

private def kernel (source : Name) (arguments : Array Value)
    (finish : Value → GenM Unit) (calls : Array CallBinding := #[]) (resultBinders : Nat := 0) : MetaM (Array String) := do
  let info ← getConstInfo source
  forallTelescope info.type fun binders _ => do
    let params := binders.extract 0 (binders.size - resultBinders)
    let split := params.size - arguments.size
    let locals := ((params.extract split params.size).map Expr.fvarId!).zip arguments
    let (_, result) ← (do
      finish (← evaluate (mkAppN (mkConst source) params) locals.toList)).run {
        numberType := some params[0]!
        arithmetic := (params.extract 1 split).map Expr.fvarId!
        calls
        foldVectorConstants := true }
    return pruneTemporaries result.lines

private def returnVector (value : Value) : GenM Unit := do
  let ("Vector", result) ← scalar value | throwError "Expected a packed channel result"
  emit s!"return {result};"

private def returnPair (value : Value) : GenM Unit := do
  let .record #[a, b] := value | throwError "Expected two packed channel results"
  let (_, a) ← scalar a
  let (_, b) ← scalar b
  emit ("return {" ++ a ++ ", " ++ b ++ "};")

private def renderMethod (signature : String) (lines : Array String) : Array String :=
  #["    " ++ signature, "    {"] ++ lines.map (fun line => "        " ++ line) ++ #["    }", ""]

private def processingCalls : Array CallBinding := #[
  ⟨``SquiggleBoyWanderingIntroAPFs.step, "Process", 2, "", true, true⟩,
  ⟨``SquiggleBoyWanderingIntroAPFs.y, "Y", 1, "Vector", true, false⟩,
  ⟨``SquiggleBoyQuadReverb.drive, "Drive", 4, "Vector", false, false⟩,
  ⟨``SquiggleBoyQuadReverb.pushDelay, "Push", 2, "", true, true⟩,
  ⟨``DelayBuffer.readCubic, "ReadCubic", 2, "Vector", true, false⟩,
  ⟨``squiggleBoyReverbMix, "Mix", 1, "Vector", false, false⟩,
  ⟨``WanderingSchroederAllPassGeneric.step, "Process", 2, "", true, true⟩,
  ⟨``WanderingSchroederAllPassGeneric.y, "Y", 1, "Vector", true, false⟩,
  ⟨``SquiggleBoyQuadReverb.stepDamping, "Process", 2, "", true, true⟩,
  ⟨``SquiggleBoyQuadReverb.dampedSample, "Y", 1, "Vector", true, false⟩]

private def stateFields : Array Value := #[
  .scalar "Diffusion" "m_diffusion",
  .scalar "CubicDelay" "m_delay",
  vector "m_delaySamples",
  vector "m_feedback",
  .record #[vector "m_saturator.first", vector "m_saturator.second"],
  .scalar "AllPass" "m_firstAllPass",
  .scalar "Damping" "m_damping",
  .scalar "AllPass" "m_secondAllPass"]

private def specFields : Array Value :=
  #["delaySamples", "feedback", "dampingBase", "dampingWidth"].map (fun field =>
    vector ("spec.m_" ++ field))

private def renderProcess : MetaM (Array String) := do
  let lines ← kernel ``SquiggleBoyQuadReverb.process #[.record stateFields, .record #[vector "input"]]
    (fun result => do
      let .record #[_, output] := result | throwError "Expected reverb state and audio"
      returnVector output) processingCalls
  return renderMethod "Vector Process(const Vector& input)" lines

private def renderTuning : MetaM (Array String) := do
  let tune : CallBinding := ⟨``SquiggleBoyQuadReverb.tuneDamping, "Tune", 3, "", true, true⟩
  let lines ← kernel ``SquiggleBoyQuadReverb.computeCoefficients
    #[.record specFields, .record stateFields] (fun next => do
      let .record fields := next | throwError "Expected reverb state"
      for i in #[2, 3] do
        let (_, name) ← scalar stateFields[i]!
        let (_, value) ← scalar fields[i]!
        emit s!"{name} = {value};") #[tune]
  return renderMethod "void ComputeCoefficients(const LogicalSpec& spec)" lines

private def renderKernels : MetaM (Array String) := do
  let mixOutputs ← kernel ``ParallelSchroederAllPassCascades.mixOutputs
    (#[ "shortBranch", "longBranch" ].map vector) returnVector
  let cutoffs ← kernel ``SquiggleBoyQuadReverb.dampingCutoffs
    (#["base", "width"].map vector) returnPair
  let drive ← kernel ``SquiggleBoyQuadReverb.drive
    #[.record #[vector "saturator.first", vector "saturator.second"], vector "input",
      vector "previous", vector "feedback"] returnVector
  let saturator ← kernel ``NormalizedTanh.prepare #[.record #[vector "gain"]] returnPair
  let gain ← kernel ``SquiggleBoyQuadReverb.allPassGain #[] returnVector
  let saturatorGain ← kernel ``SquiggleBoyQuadReverb.saturatorSpec #[] (fun value => do
    let .record #[gain] := value | throwError "Expected saturator spec"
    returnVector gain)
  let cubicPosition ← kernel ``DelayBuffer.cubicPosition
    (#["period", "samples"].map vector) returnPair
  let fractionalPosition ← kernel ``DelayBuffer.fractionalPosition #[vector "samples"] returnPair
  let offset ← kernel ``WanderingSchroederAllPassGeneric.offsetSamples
    (#["depth", "spline"].map vector) returnVector
  let interpolate ← kernel ``DelayBuffer.cubicInterpolate
    (#["older", "center", "newer", "newest", "fraction"].map vector) returnVector
  let mix ← kernel ``squiggleBoyReverbMix #[.vector ((List.range 4).toArray.map fun i =>
      vector s!"Vector::Splat(input[{i}])")] (fun value => do
    let .vector lanes := value | throwError "Expected four mixer outputs"
    let scalars ← lanes.toList.mapM fun lane => do
      let (_, name) ← scalar lane
      return name ++ "[0]"
    emit ("return {" ++ String.intercalate ", " scalars ++ "};")) #[] 1
  return renderMethod "static Vector AllPassGain()" gain ++
    renderMethod "static Vector SaturatorGain()" saturatorGain ++
    renderMethod "static std::pair<Vector, Vector> PrepareSaturator(const Vector& gain)" saturator ++
    renderMethod "static Vector MixOutputs(const Vector& shortBranch, const Vector& longBranch)" mixOutputs ++
    renderMethod "static std::pair<Vector, Vector> DampingCutoffs(const Vector& base, const Vector& width)" cutoffs ++
    renderMethod "static Vector Drive(const std::pair<Vector, Vector>& saturator, const Vector& input, const Vector& previous, const Vector& feedback)" drive ++
    renderMethod "static std::pair<Vector, Vector> CubicPosition(const Vector& period, const Vector& samples)" cubicPosition ++
    renderMethod "static std::pair<Vector, Vector> FractionalPosition(const Vector& samples)" fractionalPosition ++
    renderMethod "static Vector WanderOffset(const Vector& depth, const Vector& spline)" offset ++
    renderMethod "static Vector Interpolate(const Vector& older, const Vector& center, const Vector& newer, const Vector& newest, const Vector& fraction)" interpolate ++
    renderMethod "static Vector Mix(const Vector& input)" mix

private def renderStorage : Array String := #[
  "    struct FixedTaps {};",
  "",
  "    template<size_t Capacity, std::uint64_t Seed, size_t Tap0, size_t Tap1 = Tap0, size_t Tap2 = Tap0, size_t Tap3 = Tap0>",
  "    struct AllPass",
  "    {",
  "        static_assert(x_depth + 3 <= Tap0 && Tap0 + x_depth + 1 <= Capacity);",
  "        static_assert(x_depth + 3 <= Tap1 && Tap1 + x_depth + 1 <= Capacity);",
  "        static_assert(x_depth + 3 <= Tap2 && Tap2 + x_depth + 1 <= Capacity);",
  "        static_assert(x_depth + 3 <= Tap3 && Tap3 + x_depth + 1 <= Capacity);",
  "        using Stream = std::conditional_t<Framed, SmoothRandomLFOFramedGeneric<Number>, SmoothRandomLFOStreamGeneric<Number>>;",
  "        using Modulator = std::conditional_t<Wandering, Stream, FixedTaps>;",
  "",
  "        CircularBufferGeneric<Number, 4, Capacity> m_history;",
  "        Vector m_gain = AllPassGain();",
  "        Vector m_output;",
  "        Modulator m_lfo = InitModulator();",
  "",
  "        static Modulator InitModulator()",
  "        {",
  "            if constexpr (Wandering)",
  "            {",
  "                return Modulator(x_lambdaSamples, Seed);",
  "            }",
  "            else",
  "            {",
  "                return {};",
  "            }",
  "        }",
  "",
  "        Vector ReadTaps(size_t ageOffset) const",
  "        {",
  "            if constexpr (Tap0 == Tap1 && Tap0 == Tap2 && Tap0 == Tap3)",
  "            {",
  "                return m_history.ReadAge((Tap0 + ageOffset) % Capacity);",
  "            }",
  "            else",
  "            {",
  "                return {m_history.ReadAge((Tap0 + ageOffset) % Capacity)[0],",
  "                    m_history.ReadAge((Tap1 + ageOffset) % Capacity)[1],",
  "                    m_history.ReadAge((Tap2 + ageOffset) % Capacity)[2],",
  "                    m_history.ReadAge((Tap3 + ageOffset) % Capacity)[3]};",
  "            }",
  "        }",
  "",
  "        Vector ReadDelayed()",
  "        {",
  "            if constexpr (Wandering)",
  "            {",
  "                const auto offset = WanderOffset(Vector::Splat(x_depth), Vector::Splat(m_lfo.Process()));",
  "                const auto [rounded, fraction] = FractionalPosition(offset);",
  "                const auto center = static_cast<size_t>(rounded[0]) + Capacity - x_depth - 1;",
  "                return Interpolate(ReadTaps(center + 1), ReadTaps(center),",
  "                    ReadTaps(center - 1), ReadTaps(center - 2), fraction);",
  "            }",
  "            else",
  "            {",
  "                return ReadTaps(Capacity - 1);",
  "            }",
  "        }",
  "",
  "        Vector Process(const Vector& audio)",
  "        {",
  "            SchroederAllPassCoreGeneric<Number, 4> core{{m_gain}, {}};",
  "            typename SchroederAllPassCoreGeneric<Number, 4>::Input input{audio, ReadDelayed()};",
  "            core.Process(input);",
  "            m_output = core.m_output.m_y;",
  "            m_history.Push(core.m_output.m_written);",
  "            return m_output;",
  "        }",
  "",
  "        Vector Y() const",
  "        {",
  "            return m_output;",
  "        }",
  "",
  "    };",
  "",
  "    struct Damping",
  "    {",
  "        TPTBaseWidthGeneric<Number, 4> m_filter{};",
  "",
  "        void Tune(const Vector& base, const Vector& width)",
  "        {",
  "            const auto [high, low] = DampingCutoffs(base, width);",
  "            m_filter.ComputeCoefficients({{high}, {low}});",
  "        }",
  "",
  "        void Process(const Vector& audio)",
  "        {",
  "            typename TPTBaseWidthGeneric<Number, 4>::Input input{audio};",
  "            m_filter.Process(input);",
  "        }",
  "",
  "        Vector Y() const",
  "        {",
  "            return m_filter.m_output.m_x;",
  "        }",
  "",
  "    };",
  "",
  "    template<size_t Capacity>",
  "    struct CubicDelay",
  "    {",
  "        CircularBufferGeneric<Number, 4, Capacity> m_history;",
  "",
  "        void Push(const Vector& audio)",
  "        {",
  "            m_history.Push(audio);",
  "        }",
  "",
  "        Vector ReadCubic(const Vector& samples) const",
  "        {",
  "            const auto [rounded, fraction] = CubicPosition(Vector::Splat(2 * Capacity), samples);",
  "            Vector older, center, newer, newest;",
  "            for (int ch = 0; ch < 4; ++ch)",
  "            {",
  "                const auto whole = static_cast<size_t>(rounded[ch]);",
  "                older[ch] = m_history.ReadAge(whole % Capacity)[ch];",
  "                center[ch] = m_history.ReadAge((whole + Capacity - 1) % Capacity)[ch];",
  "                newer[ch] = m_history.ReadAge((whole + Capacity - 2) % Capacity)[ch];",
  "                newest[ch] = m_history.ReadAge((whole + 2 * Capacity - 3) % Capacity)[ch];",
  "            }",
  "",
  "            return Interpolate(older, center, newer, newest, fraction);",
  "        }",
  "",
  "    };",
  ""]

private def renderDiffusion : Array String := Id.run do
  let capacities := #[squiggleBoyReverbShortCapacities, squiggleBoyReverbLongCapacities]
  let names := #["short", "long"]
  let mut fields := #[]
  let mut body := #[]
  for branch in List.finRange 2 do
    let name := names[branch.val]!
    body := body.push s!"            Vector {name}Audio = input;"
    for stage in List.finRange 4 do
      let capacity := (capacities[branch.val]!)[stage.val]!
      let tap := squiggleBoyReverbIntroAPFsDelayTime branch stage
      let member := s!"m_{name}{stage.val}"
      let seed := (squiggleBoyReverbIntroSeed branch stage).toNat
      fields := fields.push s!"        AllPass<{capacity}, {seed}, {tap}> {member};"
      body := body.push s!"            {name}Audio = {member}.Process({name}Audio);"
  return #["    struct Diffusion", "    {"] ++ fields ++
    #["        Vector m_output;", "", "        void Process(const Vector& input)", "        {"] ++
    body ++ #["            m_output = MixOutputs(shortAudio, longAudio);", "        }", "",
      "        Vector Y() const", "        {", "            return m_output;", "        }", "",
      "    };", ""]

private def renderState : Array String := Id.run do
  let taps := fun branch => String.intercalate ", " ((List.finRange 4).map fun ch =>
    toString (squiggleBoyReverbLoopAPFsDelayTime branch ch))
  return #["    Diffusion m_diffusion;",
    "    CubicDelay<x_delayCapacity> m_delay;",
    "    Vector m_delaySamples;", "    Vector m_feedback;",
    "    const std::pair<Vector, Vector> m_saturator = PrepareSaturator(SaturatorGain());",
    s!"    AllPass<{squiggleBoyReverbFirstCapacity}, {(squiggleBoyReverbLoopSeed 0).toNat}, {taps 0}> m_firstAllPass;",
    "    Damping m_damping;",
    s!"    AllPass<{squiggleBoyReverbSecondCapacity}, {(squiggleBoyReverbLoopSeed 1).toNat}, {taps 1}> m_secondAllPass;",
    "", "};", ""]

def generateQuadReverb (name : String) : MetaM String := do
  unless validIdentifier name do throwError "C++ codegen: invalid reverb name"
  let tuning ← renderTuning
  let process ← renderProcess
  let arithmetic ← renderKernels
  let wander := squiggleBoyReverbWanderSpec true
  let lambda ← hexLiteral wander.lambdaSamples.toBits.toNat 52 11 1023 ""
  let header := #["#pragma once", "", "// Generated from DSP.SquiggleBoyQuadReverb. Do not edit.",
    "// Compile with strict floating-point arithmetic and implicit contraction disabled.", "//", "",
    "#include \"CircularBufferGeneric.hpp\"", "#include \"SchroederAllPassCoreGeneric.hpp\"",
    "#include \"SmoothRandomLFOStreamGeneric.hpp\"", "#include \"TPTBaseWidthGeneric.hpp\"",
    "#include <cstdint>", "#include <type_traits>", "#include <utility>", "",
    "template<typename Number, bool Wandering = true, bool Framed = true>", s!"struct {name}", "{",
    "    using Vector = MultiChannelNumber<Number, 4>;",
    "    using DampingUIState = typename TPTBaseWidthGeneric<Number, 4>::UIState;",
    s!"    static constexpr size_t x_delayCapacity = {squiggleBoyReverbDelayCapacity};", "",
    "    struct LogicalSpec", "    {",
    "        Vector m_delaySamples;", "        Vector m_feedback;", "        Vector m_dampingBase;",
    "        Vector m_dampingWidth;", "    };", "",
    s!"    explicit {name}(const LogicalSpec& spec)", "    {", "        ComputeCoefficients(spec);",
    "    }", "",
    "    auto MakeDampingSnapshot() const",
    "    {",
    "        return m_damping.m_filter.MakeSnapshot();",
    "    }", ""]
  return String.intercalate "\n" (header ++ tuning ++ process ++ #["private:",
    s!"    static constexpr size_t x_depth = Wandering ? {wander.depth} : 0;",
    s!"    static constexpr double x_lambdaSamples = {lambda};", ""] ++ arithmetic ++ renderStorage ++
    renderDiffusion ++ renderState).toList

elab "#generate_cpp_quad_reverb " name:ident " to " path:str : command => do
  let code ← liftTermElabM <| generateQuadReverb name.getId.toString
  let output : System.FilePath := path.getString
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  logInfo m!"Generated {output} from DSP.SquiggleBoyQuadReverb"

end DSP.Codegen
