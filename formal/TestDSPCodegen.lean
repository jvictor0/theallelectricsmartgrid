import DSP.Filters.Runtime
import DSP.Codegen.Cpp
import DSP.Tests.CodegenFixtures
import DSP.Filters.Responses
import DSP.Filters.LinkwitzRileySplitter
import DSP.Filters.Capture
import DSP.Filters.ConservativeReuppingLadder.Response
import DSP.Filters.MultibandSaturator.Exports
import DSP.Resynthesis.FrameStorage
import DSP.Resynthesis.PVDRStorage

#use_cpp_record DSP.ResponseFrequency as ResponseFrequencyGeneric from "ResponseFrequency.hpp"

#generate_cpp DSP.Tests.swapProcessor as SwapProcessor to "build/lean/generated/SwapProcessor.hpp"
#generate_cpp DSP.Tests.gainProcessor as GainProcessor to "build/lean/generated/GainProcessor.hpp"
#generate_cpp DSP.Tests.indexedStateFold as IndexedStateFold to "build/lean/generated/IndexedStateFold.hpp"
#generate_cpp DSP.Tests.boundedEdit as BoundedEdit to "build/lean/generated/BoundedEdit.hpp"
#generate_cpp DSP.Tests.boundedExit as BoundedExit to "build/lean/generated/BoundedExit.hpp"
#generate_cpp DSP.Tests.fmaProcessorFloat as FMAProcessor to "build/lean/generated/FMAProcessor.hpp"
#generate_cpp DSP.Tests.fmaProcessorFloat32 as FMAProcessor32 to "build/lean/generated/FMAProcessor32.hpp"
#generate_cpp_generic DSP.Tests.fmaProcessor as FMAProcessorGeneric to "build/lean/generated/FMAProcessorGeneric.hpp"

#generate_cpp_generic DSP.Tests.arithmeticProcessor as ArithmeticProcessorGeneric to "build/lean/generated/ArithmeticProcessorGeneric.hpp"

namespace DSP.Tests

structure RecordState where
  left : Float
  right : Float
  values : Vector Float 2

structure RecordInput where
  swap : Bool
  reset : Bool
  index : Fin 2

def recordPairUpdate (swap : Bool) : StateM RecordState Float :=
  modifyGet fun s => (s.left, if swap then { s with left := s.right, right := s.left }
    else { s with right := s.right + 1 })

def recordUpdate (input : RecordInput) : StateM RecordState Float := do
  let before ← get
  let left ← recordPairUpdate input.swap
  modify fun state => { state with values := state.values.set input.index state.left }
  if input.reset then set before
  return left + (← get : RecordState).right

structure FocusState where
  first : RecordState
  second : RecordState

def nestedUpdate (swap : Bool) : StateM FocusState Float :=
  focus FocusState.first (fun parent first => { parent with first }) (recordPairUpdate swap)

def invalidFocus (swap : Bool) : StateM FocusState Float :=
  focus FocusState.first (fun parent first => { parent with first, second := first })
    (recordPairUpdate swap)

def readFocusParent (_ : PUnit.{1}) : StateM FocusState Float :=
  modifyGet fun parent => (parent.second.left, parent)

def focusCapturesParent (_ : PUnit.{1}) : StateM FocusState Float := fun parent =>
  (focus FocusState.first (fun s first => { s with first }) (fun child =>
    let saved := (readFocusParent ⟨⟩).run parent
    (saved.1, child))).run parent

end DSP.Tests

#generate_cpp_state DSP.Tests.recordPairUpdate outlined as RecordPairUpdate to "build/lean/generated/RecordPairUpdate.hpp"
#generate_cpp_state DSP.Tests.recordUpdate as RecordUpdate to "build/lean/generated/RecordUpdate.hpp"
#generate_cpp_state DSP.Tests.readFocusParent outlined as ReadFocusParent to "build/lean/generated/ReadFocusParent.hpp"

open Lean Elab Command in
run_cmd liftTermElabM do
  let code ← DSP.Codegen.generateEffect ``DSP.Tests.recordPairUpdate "StateUpdate" (ownRecord := true)
  let [swap, increment] := code.splitOn "else\n" | throwError "expected one state update branch"
  if code.contains "state.m_values" || increment.contains "state.m_left = " then
    throwError "unmodified fields should not be written by an in-place branch"
  unless swap.contains "state.m_left = " && increment.contains "state.m_right = " do
    throwError "stores should remain in their own update branch"
  let focused ← DSP.Codegen.generateEffect ``DSP.Tests.nestedUpdate "FocusedUpdate" (ownRecord := true)
  unless focused.contains "RecordPairUpdate::Process(state.m_first," &&
      !focused.contains "state.m_second" do
    throwError "focus must pass the current child field without accessing its sibling"
  for (source, expected) in #[
      (``DSP.Tests.invalidFocus, "focus setter must replace only the selected field"),
      (``DSP.Tests.focusCapturesParent, "a focused action may only use its current child storage")] do
    let mut rejected := false
    try
      let _ ← DSP.Codegen.generateEffect source "InvalidFocus" (ownRecord := true)
    catch error =>
      unless (← error.toMessageData.toString).contains expected do throw error
      rejected := true
    unless rejected do throwError "C++ codegen accepted an invalid focus in {source}"

namespace DSP.Tests

open DSP.Resynthesis.PVDR

abbrev BorrowedPVDR := Storage Float32 Float Storage.Heap32 Nat 4096

def staleBorrowedBranch (input : Storage.Input) (state : BorrowedPVDR) : PUnit × BorrowedPVDR :=
  if input.schmitt then
    let next := (Storage.setGate 0 .high).run state
    (next.1, state)
  else (⟨⟩, state)

def staleEmptyFold (_ : Storage.Input) (state : BorrowedPVDR) : PUnit × BorrowedPVDR :=
  let next := (Storage.setGate 0 .high).run state
  let loop : StateM BorrowedPVDR PUnit := Fin.foldlM 0 (fun _ _ => pure ⟨⟩) ⟨⟩
  let old := loop.run state
  (next.1, old.2)

def staleLoopCapture (_ : Storage.Input) (state : BorrowedPVDR) : PUnit × BorrowedPVDR :=
  let step (_ : PUnit) (_ : Fin 2) : StateM BorrowedPVDR PUnit :=
    fun _ => (Storage.setGate 0 .high).run state
  (Fin.foldlM 2 step ⟨⟩).run state

def staleStorageArgument (_ : Storage.Input) (state : BorrowedPVDR) : PUnit × BorrowedPVDR :=
  (Storage.setGate 0 (((do
    Storage.setGate 1 .low
    Storage.gate 1 : StateM BorrowedPVDR DSP.Resynthesis.GateState).run state).1)).run state

def staleValueReturn (_ : Storage.Input) (state : BorrowedPVDR) : Float32 × BorrowedPVDR :=
  let next := ((do
    Storage.setGate 0 .high
    Storage.magnitude 0 : StateM BorrowedPVDR Float32).run state)
  (next.1, state)

structure BorrowedSampleInput where
  second : Bool
  replacement : Float

structure BorrowedSampleOutput where
  previous : Float
  current : Float

def readBorrowedSample (input : BorrowedSampleInput) :
    StateM (Resynthesis.FrameStorage.Scene Float Float) Float :=
  if input.second then Resynthesis.FrameStorage.getSample 0 1
  else Resynthesis.FrameStorage.getSample 0 0

def replaceBorrowedSample (input : BorrowedSampleInput) :
    StateM (Resynthesis.FrameStorage.Scene Float Float) BorrowedSampleOutput := do
  let previous ← Resynthesis.FrameStorage.getSample 0 0
  Resynthesis.FrameStorage.putSample 0 0 input.replacement
  let current ← Resynthesis.FrameStorage.getSample 0 0
  return ⟨previous, current⟩

def escapingScratch (input : BorrowedSampleInput) :
    StateM BorrowedPVDR (Codegen.Scratch Float 4 (fun i => i < 2)) :=
  pure (Codegen.Scratch.ofFn 4 2 (by decide) fun _ => input.replacement)

end DSP.Tests

#generate_cpp_effect DSP.Tests.readBorrowedSample
  as ReadBorrowedSample to "build/lean/generated/ReadBorrowedSample.hpp"
#generate_cpp_effect DSP.Tests.replaceBorrowedSample
  as ReplaceBorrowedSample to "build/lean/generated/ReplaceBorrowedSample.hpp"

open Lean Elab Command in
run_cmd liftTermElabM do
  for (source, expected) in #[
      (``DSP.Tests.staleBorrowedBranch, "branch returned an old storage snapshot"),
      (``DSP.Tests.staleEmptyFold, "old storage snapshot"),
      (``DSP.Tests.staleLoopCapture, "effect used an old state snapshot"),
      (``DSP.Tests.staleStorageArgument, "a storage argument invalidated the current state"),
      (``DSP.Tests.staleValueReturn, "action returned an old state snapshot"),
      (``DSP.Tests.escapingScratch, "scratch storage must stay local")] do
    let mut rejected := false
    try
      let _ ← DSP.Codegen.generateEffect source "StaleBorrowedStorage"
    catch error =>
      let message ← error.toMessageData.toString
      unless (message.splitOn expected).length > 1 do
        throwError "unexpected borrowed storage rejection for {source}: {message}"
      rejected := true
    unless rejected do throwError "C++ codegen accepted an invalid storage operation in {source}"
  logInfo "Borrowed storage and local scratch rejection checks passed."

#generate_cpp_filter DSP.Tests.zohStepFloat32 response DSP.Tests.zohMagnitudeSquaredFloat32
  as OnePoleZOHLowPass32 to "build/lean/generated/OnePoleZOHLowPass32.hpp"
#generate_cpp_function_generic DSP.Tests.frequencyProbe
  as FrequencyProbeGeneric method Evaluate to "build/lean/generated/FrequencyProbeGeneric.hpp"
#generate_cpp_function DSP.Tests.frequencyProbeFloat
  as FrequencyProbe method Evaluate to "build/lean/generated/FrequencyProbe.hpp"
#generate_cpp_function DSP.Tests.frequencyProbeFloat32
  as FrequencyProbe32 method Evaluate to "build/lean/generated/FrequencyProbe32.hpp"
#generate_cpp_function_generic DSP.Tests.constantFunction
  as ConstantFunctionGeneric method Evaluate to "build/lean/generated/ConstantFunctionGeneric.hpp"
#generate_cpp_function_generic DSP.Tests.snapshotFunction
  as SnapshotFunctionGeneric method Evaluate to "build/lean/generated/SnapshotFunctionGeneric.hpp"

-- Exact table constants must not become a native transcendental evaluation.
-- The C++ tuning oracle checks both precisions against Math4096 bit for bit.
open Lean Elab Command in
run_cmd liftTermElabM do
  let (_, lines) ← DSP.Codegen.generateParts ``DSP.LinkwitzRileySplitter.prepareSection
    "ConstantPreparation" true none "Input" (preparation := true)
  let code := String.intercalate "\n" lines.toList
  unless (code.splitOn "Math4096::CosEighth(1)").length == 2 do
    throwError "constant LR damping must use the constexpr Math4096 table entry"
  unless (code.splitOn "Math4096::TanPi(input)").length == 2 do
    throwError "runtime cutoff must retain Math4096 tangent"
  if (code.splitOn "Math4096::Cos2pi(").length > 1 then
    throwError "constant cosine still performs runtime table interpolation"
  for (value, expected) in [(0.125, some 1), (-0.125, some 7), (2.125, some 1),
      (0.0625, none), (0.1, none)] do
    unless DSP.Codegen.eighthCycle value == expected do
      throwError "incorrect constant cosine phase recognition"
  logInfo "Constant Math4096 preparation regression checks passed."

open Lean Elab Command in
run_cmd liftTermElabM do
  for (function, cppName, expected) in #[
      (``DSP.Tests.scalarPreparationResult, "ScalarPreparation", "preparation must return a record"),
      (``DSP.Tests.arrayPreparation, "ArrayPreparation", "preparation input must be numeric or a record"),
      (``DSP.LinkwitzRileySplitter.prepareSection, "Prepared", "preparation name conflicts"),
      (``DSP.LinkwitzRileySplitter.prepareSection, "Prepare", "preparation name conflicts")] do
    let mut rejected := false
    try
      let _ ← DSP.Codegen.generatePreparationGeneric function cppName
    catch error =>
      let message ← error.toMessageData.toString
      unless (message.splitOn expected).length > 1 do
        throwError "unexpected preparation rejection for {function}: {message}"
      rejected := true
    unless rejected do throwError "C++ codegen incorrectly accepted preparation {function}"
  logInfo "Standalone preparation rejection checks passed."

-- The coupled processor needs its old q values for the output. Store q0 only
-- after that read, but do not retain it across the independent output tail.
-- The native differential suite checks swaps and conditional execution too.
open Lean Elab Command in
run_cmd liftTermElabM do
  let code ← DSP.Codegen.generateGeneric ``DSP.Coupled.processor "ScheduledProcessor"
  let [beforeStore, afterStore] := code.splitOn "m_state.m_q[0] = "
    | throwError "coupled processor should store q0 once"
  unless (beforeStore.splitOn "m_state.m_model.m_C[0] * m_state.m_q[0]").length == 2 do
    throwError "coupled processor overwrites q0 before its old-state output read"
  unless (afterStore.splitOn "m_state.m_model.m_D").length == 2 do
    throwError "coupled processor delays state stores until after independent output work"
  if (code.splitOn "m_state.m_model.m_A[0][0] = ").length > 1 then
    throwError "processing should not rewrite unchanged coefficients"
  logInfo "Processor store scheduling regression checks passed."

open Lean Elab Command in
run_cmd liftTermElabM do
  for (name, expected) in #[
      (``DSP.Tests.unsupportedField, "unsupported field type"),
      (``DSP.Tests.unsupportedExtern, "unsupported external implementation"),
      (``DSP.Tests.unsupportedLiteral, "unsupported external implementation"),
      (``DSP.Tests.unsupportedDecision, "unsupported external implementation"),
      (``DSP.Tests.unsupportedStaticCount, "unsupported external implementation"),
      (``DSP.Tests.unsupportedSize, "256-element export limit"),
      (``DSP.Tests.unsupportedUnsafe, "unsafe/partial definition"),
      (``DSP.Tests.wrongState, "returned state type differs"),
      (``DSP.Tests.wrongStateOnly, "returned state type differs")] do
    let mut rejected := false
    try
      let _ ← DSP.Codegen.generate name "TestProcessor"
    catch error =>
      let message ← error.toMessageData.toString
      unless (message.splitOn expected).length > 1 do
        throwError "unexpected rejection for {name}: {message}"
      rejected := true
    unless rejected do throwError "C++ codegen incorrectly accepted {name}"
  logInfo "C++ codegen rejection checks passed."

open Lean Elab Command in
run_cmd liftTermElabM do
  for (name, cppName, expected) in #[
      (``DSP.Tests.swapProcessor, "TestProcessor", "expects one arithmetic type parameter"),
      (``DSP.Tests.unsupportedGenericOrder, "TestProcessor", "unsupported generic parameter"),
      (``DSP.Tests.unsupportedGenericLiteral, "TestProcessor", "unsupported external implementation"),
      (``DSP.Tests.wrongGenericState, "TestProcessor", "returned state type differs"),
      (``DSP.Tests.arithmeticProcessor, "Vector", "processor name conflicts")] do
    let mut rejected := false
    try
      let _ ← DSP.Codegen.generateGeneric name cppName
    catch error =>
      let message ← error.toMessageData.toString
      unless (message.splitOn expected).length > 1 do
        throwError "unexpected generic rejection for {name}: {message}"
      rejected := true
    unless rejected do throwError "C++ codegen incorrectly accepted generic {name}"
  logInfo "Generic C++ codegen rejection checks passed."

open Lean Elab Command in
run_cmd liftTermElabM do
  for (name, cppName, methodName, generic, expected) in #[
      (``DSP.Tests.functionWithoutSnapshot, "Response", "Evaluate", false, "expects a Snapshot record"),
      (``DSP.Tests.functionRecordInput, "Response", "Evaluate", false, "numeric or an exported record"),
      (``DSP.Tests.functionRecordResult, "Response", "Evaluate", false, "input's numeric type"),
      (``DSP.Tests.functionExtern, "Response", "Evaluate", false, "unsupported external implementation"),
      (``DSP.Tests.functionWrongArity, "Response", "Evaluate", false, "two runtime arguments"),
      (``DSP.Tests.functionNoArguments, "Response", "Evaluate", true, "expects one arithmetic type parameter"),
      (``DSP.Tests.functionMixedNumbers, "Response", "Evaluate", false, "input's numeric type"),
      (``DSP.Tests.wrongGenericState, "Response", "Evaluate", true, "numeric or an exported record"),
      (``DSP.Tests.constantFunction, "Snapshot", "Evaluate", true, "function name conflicts"),
      (``DSP.Tests.constantFunction, "Response", "Response", true, "function name conflicts"),
      (``DSP.Tests.constantFunction, "Response", "Vector", true, "function name conflicts"),
      (``DSP.Tests.constantFunction, "Response", "Snapshot", true, "function name conflicts"),
      (``DSP.Tests.constantFunction, "Response", "Input", true, "function name conflicts"),
      (``DSP.Tests.constantFunction, "ResponseFrequencyGeneric", "Evaluate", true, "exported record"),
      (``DSP.Tests.constantFunction, "Response", "evaluate", true, "method name must be")] do
    let mut rejected := false
    try
      let _ ← DSP.Codegen.generateFunction name cppName methodName generic
    catch error =>
      let message ← error.toMessageData.toString
      unless (message.splitOn expected).length > 1 do
        throwError "unexpected function rejection for {name}: {message}"
      rejected := true
    unless rejected do throwError "C++ codegen incorrectly accepted function {name}"
  logInfo "Pure-function C++ codegen rejection checks passed."

open Lean Elab Command in
run_cmd liftTermElabM do
  for (name, cppName, expected) in #[
      (``DSP.Tests.GainState, "TestRecord", "one number type parameter"),
      (``DSP.Tests.functionWithoutSnapshot, "TestRecord", "expects a structure"),
      (``DSP.Tests.UnsupportedRecord, "TestRecord", "unsupported field type"),
      (``DSP.ResponseFrequency, "Number", "record name must be")] do
    let mut rejected := false
    try
      let _ ← DSP.Codegen.generateRecord name cppName
    catch error =>
      let message ← error.toMessageData.toString
      unless (message.splitOn expected).length > 1 do
        throwError "unexpected record rejection for {name}: {message}"
      rejected := true
    unless rejected do throwError "C++ codegen incorrectly accepted record {name}"
  logInfo "Shared-record C++ codegen rejection checks passed."

open Lean Elab Command in
run_cmd liftTermElabM do
  let low := ``DSP.OnePoleTPTLowPass.Snapshot.magnitudeSquared
  let high := ``DSP.OnePoleTPTHighPass.Snapshot.magnitudeSquared
  for (responses, expected) in #[
      (#[], "at least one response"),
      (#[(low, "MagnitudeSquared"), (low, "MagnitudeSquared")], "duplicate or reserved"),
      (#[(low, "Process")], "duplicate or reserved"),
      (#[(low, "State")], "duplicate or reserved"),
      (#[(low, "LogicalSpec")], "duplicate or reserved"),
      (#[(low, "ComputeCoefficients")], "duplicate or reserved"),
      (#[(low, "Low"), (high, "High")], "same snapshot and frequency types")] do
    let mut rejected := false
    try
      let _ ← DSP.Codegen.generateFilterResponses ``DSP.OnePoleTPTLowPass.step responses "TestFilter" true
    catch error =>
      let message ← error.toMessageData.toString
      unless (message.splitOn expected).length > 1 do
        throwError "unexpected response-list rejection: {message}"
      rejected := true
    unless rejected do throwError "C++ codegen incorrectly accepted invalid response list"
  logInfo "Multiple-response C++ codegen rejection checks passed."

namespace DSP.Tests

def coefficientWrongPrecision := OnePoleTPTLowPass.computeCoefficients (R := Float32)
def coefficientWrongReturn (spec : OnePoleTPTLowPass.LogicalSpec Float)
    (state : OnePoleTPTLowPass Float) : OnePoleTPTLowPass Float × Sample Float :=
  (OnePoleTPTLowPass.computeCoefficients spec state, ⟨0⟩)

def tptStepFloat32 := OnePoleTPTLowPass.step (R := Float32)
def tptResponseFloat32 := OnePoleTPTLowPass.Snapshot.magnitudeSquared (R := Float32)
end DSP.Tests

#generate_cpp_tuned_filter DSP.Tests.tptStepFloat32 DSP.Tests.coefficientWrongPrecision
  [DSP.Tests.tptResponseFloat32 as MagnitudeSquared]
  as OnePoleTPTLowPass32 to "build/lean/generated/OnePoleTPTLowPass32.hpp"

open Lean Elab Command in
run_cmd liftTermElabM do
  for (step, updater, responseFn, generic, expected) in #[
      (``DSP.OnePoleTPTLowPass.step, ``DSP.TPTSVF.computeCoefficients,
        ``DSP.OnePoleTPTLowPass.Snapshot.magnitudeSquared, true, "different state record"),
      (``DSP.OnePoleTPTLowPass.stepFloat, ``DSP.Tests.coefficientWrongPrecision,
        ``DSP.OnePoleTPTLowPass.Snapshot.magnitudeSquaredFloat, false, "different numeric type"),
      (``DSP.OnePoleTPTLowPass.stepFloat, ``DSP.Tests.coefficientWrongReturn,
        ``DSP.OnePoleTPTLowPass.Snapshot.magnitudeSquaredFloat, false, "must return its State")] do
    let mut rejected := false
    try
      let _ ← DSP.Codegen.generateFilterResponses step #[(responseFn,"MagnitudeSquared")]
        "TestFilter" generic (some updater)
    catch error =>
      let message ← error.toMessageData.toString
      unless (message.splitOn expected).length > 1 do
        throwError "unexpected coefficient rejection for {updater}: {message}"
      rejected := true
    unless rejected do throwError "C++ codegen incorrectly accepted coefficient updater {updater}"
  logInfo "Coefficient updater rejection checks passed."

namespace DSP.Tests

def splitterTwoBandTuning {R : Type} [OfNat R 1] [OfNat R 8] [Add R] [Mul R]
    [Div R] [FMA R] [Trig R] : LinkwitzRileySplitter.LogicalSpec 2 R →
      LinkwitzRileySplitter.State 2 R → LinkwitzRileySplitter.State 2 R :=
  LinkwitzRileySplitter.computeCoefficients
def splitterWrongCapture {R : Type} (spec : TPTSVF.LogicalSpec R) : TPTSVF.Snapshot R :=
  ⟨spec.cutoff,spec.q⟩
def splitterScalarCapture {R : Type} (spec : LinkwitzRileySplitter.LogicalSpec 4 R) : R :=
  spec.tree.value

def captureWrongPrecision (spec : OnePoleTPTLowPass.LogicalSpec Float)
    (state : OnePoleTPTLowPass Float) : OnePoleTPTLowPass Float × OnePoleTPTLowPass.Snapshot Float32 :=
  (OnePoleTPTLowPass.computeCoefficients spec state, ⟨0⟩)
end DSP.Tests

open Lean Elab Command in
run_cmd liftTermElabM do
  for (tuning, capture, expected) in #[
      (``DSP.Tests.splitterTwoBandTuning, ``DSP.LinkwitzRileySplitter.makeSnapshot4Band, "different state record"),
      (``DSP.LinkwitzRileySplitter.computeCoefficients4Band, ``DSP.Tests.splitterWrongCapture, "different logical specification"),
      (``DSP.LinkwitzRileySplitter.computeCoefficients4Band, ``DSP.Tests.splitterScalarCapture, "must return a record")] do
    let mut rejected := false
    try
      let _ ← DSP.Codegen.generateTunedSnapshot ``DSP.LinkwitzRileySplitter.step4Band tuning capture "TestSplitter"
    catch error =>
      let message ← error.toMessageData.toString
      unless (message.splitOn expected).length > 1 do
        throwError "unexpected snapshot rejection: {message}"
      rejected := true
    unless rejected do throwError "C++ codegen incorrectly accepted incompatible splitter signatures"
  logInfo "Tuned snapshot signature rejection checks passed."

namespace DSP.Tests

def nativeNatDecision (a b : Nat) : Decidable (a < b) := Nat.decLt a b
@[implemented_by nativeNatDecision] def alternateNatDecision (a b : Nat) : Decidable (a < b) :=
  Nat.decLt a b

def staticExternDecision (state : SwapState) (input : SwapInput) : SwapState :=
  @ite SwapState (3 < 4) (alternateNatDecision 3 4) state ⟨input.value,input.value⟩

@[reducible] def reverseNatOrder : LT Nat := ⟨fun a b => Nat.lt b a⟩
def staticCustomOrder (state : SwapState) (input : SwapInput) : SwapState :=
  @ite SwapState (@LT.lt Nat reverseNatOrder 3 4) (Nat.decLt 4 3) state ⟨input.value,input.value⟩

end DSP.Tests

open Lean Elab Command in
run_cmd liftTermElabM do
  for name in #[``DSP.Tests.staticExternDecision, ``DSP.Tests.staticCustomOrder] do
    let mut rejected := false
    try
      let _ ← DSP.Codegen.generate name "TestProcessor"
    catch error =>
      let message ← error.toMessageData.toString
      unless (message.splitOn "unsupported external implementation").length > 1 do
        throwError "unexpected static comparison rejection: {message}"
      rejected := true
    unless rejected do throwError "C++ codegen bypassed the static decision boundary for {name}"
  logInfo "Static topology comparison boundary checks passed."

open Lean Elab Command in
run_cmd liftTermElabM do
  for (updater, expected) in #[
      (``DSP.OnePoleTPTLowPass.computeCoefficientsFloat, "State and the response Snapshot"),
      (``DSP.Tests.coefficientWrongReturn, "State and the response Snapshot"),
      (``DSP.Tests.captureWrongPrecision, "State and the response Snapshot")] do
    let mut rejected := false
    try
      let _ ← DSP.Codegen.generateFilterResponses ``DSP.OnePoleTPTLowPass.stepFloat
        #[(``DSP.OnePoleTPTLowPass.Snapshot.magnitudeSquaredFloat,"MagnitudeSquared")]
        "TestFilter" false (some updater) true
    catch error =>
      let message ← error.toMessageData.toString
      unless (message.splitOn expected).length > 1 do
        throwError "unexpected capture rejection: {message}"
      rejected := true
    unless rejected do throwError "C++ codegen incorrectly accepted snapshot capture {updater}"
  logInfo "Snapshot capture rejection checks passed."

open Lean Elab Command in
run_cmd liftTermElabM do
  for (updater, expected) in #[
      (``DSP.TPTSVF.prepareCoefficientsFloat, "prepared updater input must be the response Snapshot"),
      (``DSP.OnePoleTPTLowPass.prepareCoefficientsFloat, "different state record"),
      (``DSP.TPTSVF.prepareFromSnapshot, "two runtime arguments")] do
    let mut rejected := false
    try
      let _ ← DSP.Codegen.generateFilterResponses ``DSP.TPTSVF.stepFloat
        #[(``DSP.TPTSVF.Snapshot.bandPassMagnitudeSquaredFloat,"BandPassMagnitudeSquared")]
        "TestFilter" false (some ``DSP.TPTSVF.prepareCoefficientsFloat) true (some updater)
    catch error =>
      let message ← error.toMessageData.toString
      unless (message.splitOn expected).length > 1 do
        throwError "unexpected prepared updater rejection: {message}"
      rejected := true
    unless rejected do throwError "C++ codegen incorrectly accepted prepared updater {updater}"
  logInfo "Prepared snapshot updater rejection checks passed."

def DSP.Tests.unrelatedReset {R : Type} (state : DSP.OnePoleZOHLowPass R) := state

open Lean Elab Command in
run_cmd liftTermElabM do
  for (resetName, expected) in #[
      (``DSP.Tests.unrelatedReset, "reset must take and return"),
      (``DSP.ConservativeReuppingLadder.resetFloat, "expects one arithmetic type parameter")] do
    let mut rejected := false
    try
      let _ ← DSP.Codegen.generateFilterResponses ``DSP.ConservativeReuppingLadder.step
        #[(``DSP.ConservativeReuppingLadder.Snapshot.magnitudeSquared, "MagnitudeSquared")]
        "SampledReset" true (some ``DSP.ConservativeReuppingLadder.prepareCoefficients) true
        (reset := some resetName)
    catch error =>
      let message ← error.toMessageData.toString
      unless (message.splitOn expected).length > 1 do
        throwError "unexpected sampled reset rejection: {message}"
      rejected := true
    unless rejected do throwError "C++ codegen incorrectly accepted mismatched sampled reset"
  logInfo "Sampled filter reset signature rejection checks passed."

namespace DSP.Tests

def scalarPreparation {R : Type} (input : R) := input

def invalidPreparation {R : Type} [Add R] (a b : R) : DSP.Sample R := ⟨a+b⟩

end DSP.Tests

open Lean Elab Command in
run_cmd liftTermElabM do
  for (name, expected) in #[
      (``DSP.Tests.scalarPreparation, "preparation must return a record"),
      (``DSP.Tests.invalidPreparation, "unsupported generic parameter")] do
    let mut rejected := false
    try
      let _ ← DSP.Codegen.generateParts name "TestPreparation" true none "Input" (preparation := true)
    catch error =>
      let message ← error.toMessageData.toString
      unless (message.splitOn expected).length > 1 do
        throwError "unexpected preparation rejection: {message}"
      rejected := true
    unless rejected do throwError "C++ codegen accepted invalid preparation {name}"
  logInfo "Prepared bank function boundary checks passed."

namespace DSP.Tests

def wrongCompactReset {R : Type} [OfNat R 0] (s : ConservativeReuppingLadder.Runtime.State R) :
    ConservativeReuppingLadder.Runtime.State R × TPTSVF.Runtime.Output R :=
  (s, ⟨0,0,0,0,0⟩)

def unexpectedResetOutput {R : Type} [OfNat R 0] (s : OnePoleZOHLowPass R) :
    OnePoleZOHLowPass R × Sample R := (s, ⟨0⟩)

-- The returned observation must read the old memory before Reset clears it.
def resetWithOldMemory {R : Type} [OfNat R 0] (s : OnePoleTPTLowPass.Runtime.State R) :
    OnePoleTPTLowPass.Runtime.State R × Sample R := (⟨s.G, 0⟩, ⟨s.s⟩)

def resetWithOldMemoryFloat := resetWithOldMemory (R := Float)

end DSP.Tests

#generate_cpp_sampled_filter DSP.OnePoleTPTLowPass.Runtime.processFloat DSP.OnePoleTPTLowPass.Runtime.prepareCoefficientsFloat
  resetWith DSP.Tests.resetWithOldMemoryFloat
  [DSP.OnePoleTPTLowPass.Snapshot.magnitudeSquaredFloat as MagnitudeSquared]
  as CompactResetProbe to "build/lean/generated/CompactResetProbe.hpp"

open Lean Elab Command in
run_cmd liftTermElabM do
  for (stepName, tuningName, responseName, resetName, expected) in #[
      (``DSP.ConservativeReuppingLadder.Runtime.process, ``DSP.ConservativeReuppingLadder.Runtime.prepareCoefficients,
        ``DSP.ConservativeReuppingLadder.Snapshot.magnitudeSquared, ``DSP.Tests.wrongCompactReset,
        "reset output differs"),
      (``DSP.OnePoleZOHLowPass.step, ``DSP.OnePoleZOHLowPass.prepareCoefficients,
        ``DSP.OnePoleZOHLowPass.Snapshot.magnitudeSquared, ``DSP.Tests.unexpectedResetOutput,
        "reset returns Output for a state-only processor")] do
    let mut rejected := false
    try
      let _ ← DSP.Codegen.generateFilterResponses stepName #[(responseName, "MagnitudeSquared")]
        "ResetOutput" true (some tuningName) true (reset := some resetName)
    catch error =>
      let message ← error.toMessageData.toString
      unless (message.splitOn expected).length > 1 do
        throwError "unexpected compact reset rejection: {message}"
      rejected := true
    unless rejected do throwError "C++ codegen accepted an unrelated reset output"
  logInfo "Compact reset output identity checks passed."
