import DSP.Codegen.Cpp
import DSP.Survey.Recipe

namespace DSP.Survey
open Lean Meta Elab Command DSP.Codegen

private partial def flat : Shape → String → Nat → MetaM (Value × Nat)
  | .scalar "float", base, i => pure (.scalar "float" s!"{base}[{i}]", i+1)
  | .record fields, base, i => do
    let mut offset := i
    let mut values := #[]
    for (_, shape) in fields do
      let (v, n) ← flat shape base offset
      offset := n
      values := values.push v
    return (.record values, offset)
  | .vector n shape, base, i => do
    let mut offset := i
    let mut values := #[]
    for _ in [:n] do
      let (v, k) ← flat shape base offset
      offset := k
      values := values.push v
    return (.vector values, offset)
  | _, _, _ => throwError "survey storage and controls must have Float32 leaves"

private partial def leaves : Value → MetaM (Array String)
  | .scalar _ x => pure #[x]
  | .record xs | .vector xs => return (← xs.mapM leaves).flatMap id
  | _ => throwError "survey state values must be scalar Float32 records or fixed vectors"

private def initializeSource (prepare : Name) : MetaM (Expr × Array String × Array String) := do
  forallTelescopeReducing (← getConstInfo prepare).type fun args stateTy => do
    unless args.size == 1 do throwError "prepare must take a single configuration record"
    let shape ← fieldShape (← inferType args[0]!) none
    let .record fields := shape | throwError "configuration must be a named record"
    let names ← fields.mapM fun (name, s) => do
      let .scalar "float" := s | throwError "configuration axes must be Float32"
      pure (name.drop 2).toString
    let (config, _) ← flat shape "config" 0
    let (out, emission) ← (evaluate (mkAppN (mkConst prepare) args)
      [(args[0]!.fvarId!, config)]).run {}
    let lines := (← leaves out).mapIdx fun i v => s!"state[{i}] = {v};"
    return (stateTy, names, pruneTemporaries (emission.lines ++ lines))

private def sampleSource (step : Name) (stateTy : Expr) : MetaM (Nat × Array String) := do
  forallTelescopeReducing (← getConstInfo step).type fun args result => do
    unless args.size == 2 && (← isDefEq (← inferType args[0]!) stateTy) &&
        (← isDefEq (← inferType args[1]!) (mkConst ``Float32)) &&
        (← isDefEq result (← mkAppM ``Prod #[stateTy, mkConst ``Float32])) do
      throwError "step must have type State → Float32 → State × Float32 for the prepared State"
    let (state, size) ← flat (← fieldShape stateTy none) "state" 0
    let (_, emission) ← (do
      let out ← evaluate (mkAppN (mkConst step) args)
        [(args[0]!.fvarId!, state), (args[1]!.fvarId!, .scalar "float" "sample")]
      let value ← snapshot (← selectField out 1)
      let before ← leaves state
      let after ← leaves (← selectField out 0)
      let mut stores := #[]
      for i in [:after.size] do
        if before[i]! != after[i]! then
          let (_, saved) ← scalar (← savedScalar "float" after[i]!)
          stores := stores.push s!"state[{i}] = {saved};"
      for store in stores do emit store
      emit s!"return {(← scalar value).2};").run {}
    if emission.usesMath then throwError "host lookup operations belong in prepare for this survey backend"
    return (size, pruneTemporaries emission.lines)

private def lines (xs : Array String) := String.intercalate "\n" xs.toList

register_option dsp.survey.exportDir : String := {
  defValue := ""
  descr := "Output directory for a generated DSP survey"
}

private def exportDir : CommandElabM System.FilePath := do
  let dir := dsp.survey.exportDir.get (← getOptions)
  if !dir.isEmpty then
    return dir
  return (← IO.getEnv "DSP_SURVEY_EXPORT").getD "build/response-survey/generated"

def writeRecipe (dir : String) (recipe : Recipe) : IO Unit :=
  IO.FS.writeFile (System.FilePath.mk dir / "recipe.json") (toJson recipe).pretty

/-- Export evaluated Lean arithmetic for one mono trial. Configuration and state
    layouts come from the elaborated types of the two supplied functions. -/
elab "#survey_processor " prepare:ident step:ident " using " recipe:term : command => do
  let (stateTy, names, init) ← liftTermElabM <| initializeSource prepare.getId
  let (size, sample) ← liftTermElabM <| sampleSource step.getId stateTy
  let dir ← exportDir
  IO.FS.createDirAll dir
  let body := lines sample
  let host := lines (#["#include <cmath>", "#include \"Math.hpp\"", "#include \"MultiChannelNumber.hpp\"",
    "extern \"C\" void SurveyPrepare(const float* config, float* state)", "{"] ++ init ++
    #["}", "static float Step(float* state, float sample)", "{", body, "}",
      "extern \"C\" void SurveyRun(float* state, const float* input, float* output, unsigned count)",
      "{", "    for (unsigned i = 0; i < count; ++i)", "    {",
      "        output[i] = Step(state, input[i]);", "    }", "}"])
  let gpu := lines #["#include <metal_stdlib>", "using namespace metal;",
    "static float Step(thread float* state, float sample)", "{",
    body.replace "std::" "metal::", "}",
    "kernel void SurveyRun(device const float* initial [[buffer(0)]],",
    "    device const float* input [[buffer(1)]], device float* output [[buffer(2)]],",
    "    constant uint& count [[buffer(3)]], uint row [[thread_position_in_grid]])", "{",
    s!"    float state[{max 1 size}];", s!"    for (uint i = 0; i < {size}; ++i) state[i] = initial[row * {size} + i];",
    "    for (uint i = 0; i < count; ++i) {", "        uint index = row * count + i;",
    "        output[index] = Step(state, input[index]);", "    }", "}"]
  IO.FS.writeFile (dir / "processor.cpp") host
  IO.FS.writeFile (dir / "processor.metal") gpu
  IO.FS.writeFile (dir / "processor.json") (Json.mkObj [("axes", toJson names),
    ("stateSize", toJson size), ("prepare", toJson prepare.getId.toString),
    ("step", toJson step.getId.toString)]).pretty
  let path := Syntax.mkStrLit dir.toString
  elabCommand (← `(command| #eval DSP.Survey.writeRecipe $path $recipe))

end DSP.Survey
