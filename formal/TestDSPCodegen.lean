import DSP.Codegen.Cpp
import DSP.Tests.CodegenFixtures

#generate_cpp DSP.Tests.swapProcessor as SwapProcessor to "build/lean/generated/SwapProcessor.hpp"
#generate_cpp DSP.Tests.gainProcessor as GainProcessor to "build/lean/generated/GainProcessor.hpp"
#generate_cpp DSP.Tests.fmaProcessorFloat as FMAProcessor to "build/lean/generated/FMAProcessor.hpp"
#generate_cpp DSP.Tests.fmaProcessorFloat32 as FMAProcessor32 to "build/lean/generated/FMAProcessor32.hpp"
#generate_cpp_generic DSP.Tests.fmaProcessor as FMAProcessorGeneric to "build/lean/generated/FMAProcessorGeneric.hpp"

#generate_cpp_generic DSP.Tests.arithmeticProcessor as ArithmeticProcessorGeneric to "build/lean/generated/ArithmeticProcessorGeneric.hpp"

#generate_cpp_function DSP.Tests.zohMagnitudeSquaredFloat
  as OnePoleZOHLowPassResponse method MagnitudeSquared to "build/lean/generated/OnePoleZOHLowPassResponse.hpp"
#generate_cpp_function DSP.Tests.zohMagnitudeSquaredFloat32
  as OnePoleZOHLowPassResponse32 method MagnitudeSquared to "build/lean/generated/OnePoleZOHLowPassResponse32.hpp"
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
      (``DSP.Tests.functionRecordInput, "Response", "Evaluate", false, "numeric second argument"),
      (``DSP.Tests.functionRecordResult, "Response", "Evaluate", false, "input's numeric type"),
      (``DSP.Tests.functionExtern, "Response", "Evaluate", false, "unsupported external implementation"),
      (``DSP.Tests.functionWrongArity, "Response", "Evaluate", false, "two runtime arguments"),
      (``DSP.Tests.wrongGenericState, "Response", "Evaluate", true, "numeric second argument"),
      (``DSP.Tests.constantFunction, "Snapshot", "Evaluate", true, "function name conflicts"),
      (``DSP.Tests.constantFunction, "Response", "Response", true, "function name conflicts"),
      (``DSP.Tests.constantFunction, "Response", "Vector", true, "function name conflicts"),
      (``DSP.Tests.constantFunction, "Response", "Snapshot", true, "function name conflicts"),
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
