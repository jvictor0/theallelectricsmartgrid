import DSP.Codegen.Cpp
import DSP.Tests.CodegenFixtures
import DSP.Filters.Responses
import DSP.Filters.LinkwitzRileySplitter
import DSP.Filters.Capture

#use_cpp_record DSP.ResponseFrequency as ResponseFrequencyGeneric from "ResponseFrequency.hpp"

#generate_cpp DSP.Tests.swapProcessor as SwapProcessor to "build/lean/generated/SwapProcessor.hpp"
#generate_cpp DSP.Tests.gainProcessor as GainProcessor to "build/lean/generated/GainProcessor.hpp"
#generate_cpp DSP.Tests.fmaProcessorFloat as FMAProcessor to "build/lean/generated/FMAProcessor.hpp"
#generate_cpp DSP.Tests.fmaProcessorFloat32 as FMAProcessor32 to "build/lean/generated/FMAProcessor32.hpp"
#generate_cpp_generic DSP.Tests.fmaProcessor as FMAProcessorGeneric to "build/lean/generated/FMAProcessorGeneric.hpp"

#generate_cpp_generic DSP.Tests.arithmeticProcessor as ArithmeticProcessorGeneric to "build/lean/generated/ArithmeticProcessorGeneric.hpp"

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
