import DSP.Codegen.Cpp
import PhaseTime.InverseRuntime

namespace PhaseTime.InverseRuntime

def interpolateFloat (lowPhase highPhase lowTime highTime query : Float) : Float :=
  interpolate ⟨lowPhase, lowTime⟩ ⟨highPhase, highTime⟩ query

def readIntervalFloat (lowPhase highPhase lowTime highTime query cutoff missing : Float) : Float :=
  readIntervalWith missing id ⟨lowPhase, lowTime⟩ ⟨highPhase, highTime⟩
    lowTime.isFinite highTime.isFinite query cutoff

def frontierClosesFloat (lowPhase highPhase frontier : Float) : Bool :=
  frontierCloses lowPhase highPhase frontier

def keepCutFloat (frontier highPhase newCut oldCut : Float) : Bool :=
  keepCut frontier highPhase newCut oldCut

open Lean Meta Elab Command DSP.Codegen

private def emitKernel (name : Name) (target : String) : MetaM (Array String) := do
  let definition ← getConstInfo name
  forallTelescope definition.type fun args result => do
    let arguments ← args.mapM fun arg => do
      let type ← scalarType (← inferType arg) none
      let cppName := (← arg.fvarId!.getDecl).userName.toString
      unless validIdentifier cppName do throwError "invalid inverse kernel parameter {cppName}"
      pure (arg, type, cppName)
    let resultType ← scalarType result none
    let locals := arguments.toList.map fun (arg, type, cppName) =>
      (arg.fvarId!, Value.scalar type cppName)
    let parameters := String.intercalate ", " (arguments.toList.map fun (_, type, cppName) => s!"{type} {cppName}")
    let (_, emission) ← (do
      let (type, output) ← scalar (← evaluate (mkAppN (mkConst name) args) locals)
      unless type == resultType do throwError "inverse kernel result type mismatch for {name}"
      emit s!"return {output};").run {
        calls := #[⟨``Arithmetic.lt, "Less", 2, "bool", false, false⟩,
          ⟨``Arithmetic.le, "AtMost", 2, "bool", false, false⟩,
          ⟨``Bool.and, "Both", 2, "bool", false, false⟩,
          ⟨``Bool.or, "Either", 2, "bool", false, false⟩,
          ⟨``Float.isFinite, "std::isfinite", 1, "bool", false, false⟩,
          ⟨``Bool.not, "Negate", 1, "bool", false, false⟩] }
    return #[s!"    static {resultType} {target}({parameters})", "    {"] ++
      (pruneTemporaries emission.lines).map (fun line => if line.isEmpty then "" else "        " ++ line) ++
      #["    }", ""]

/-- Storage lowering for the inverse representation. These loops and branches
    bind the runtime transitions to lock-free scalar fields and the revision
    API. The interpolation and conditional arithmetic are emitted from the
    elaborated kernels above. The native trace comparison checks this lowering. -/
private def storageLowering : String := r#"
template<typename Storage>
struct TimeWarpInverseCore
{
    // Keep the earliest minimum during a replacement visit confined to this
    // cell, including touches of its right edge. A visit after leaving the cell
    // replaces the cut and can reassign older islands to the fixed fallback.
    // An unfinished first traversal uses its entry and forward frontier.
    //
    static void BeginBranch(Storage& storage, double phase, double time)
    {
        double floorPhase = std::floor(phase);
        if (phase == floorPhase)
        {
            return;
        }

        size_t slot = storage.WrapIndex(static_cast<int64_t>(floorPhase));
        double seed = storage.m_seedPhase.load();
        bool retained = storage.m_highestWrittenIndex - static_cast<double>(Storage::x_maxSamples - 1) <= floorPhase
            && floorPhase <= storage.m_highestWrittenIndex;
        storage.BeginWrite();
        if (!retained || !std::isfinite(storage.m_writeHeadInverse[slot].load())
            || (std::floor(seed) == floorPhase && phase < seed))
        {
            storage.SeedCell(phase, time);
        }
        else if (std::isfinite(storage.m_fallbackRightTime[slot].load()))
        {
            double oldTurn = storage.m_turnPhase[slot].load();
            if (!std::isfinite(oldTurn) || !TimeWarpInverseKernels::KeepCut(
                    storage.m_frontierPhase.load(), floorPhase + 1.0, phase, oldTurn))
            {
                storage.m_islandReassignments += std::isfinite(oldTurn) && oldTurn < phase;
                double base = storage.m_writeHeadInverse[slot].load();
                storage.m_turnPhase[slot].store(phase);
                storage.m_turnTime[slot].store(static_cast<float>(time - base));
            }

            // A boundary touch may have stored an endpoint. Reopen against the
            // live frontier when this confined visit resumes below that edge.
            //
            storage.m_currentRightTime[slot].store(static_cast<float>(Storage::x_missing));
        }

        storage.m_frontierPhase.store(phase);
        storage.m_frontierTime.store(time);
        storage.EndWrite();
    }

    // Caller holds the writer revision or validates it after reading. Fallback
    // and replacement never borrow an endpoint from the neighboring cell.
    //
    static double ReadPayload(const Storage& storage, double gridPhase, double cutoff)
    {
        double floorPhase = std::floor(gridPhase);
        size_t slot = storage.WrapIndex(static_cast<int64_t>(floorPhase));
        double base = storage.m_writeHeadInverse[slot].load();
        double seed = storage.m_seedPhase.load();
        double lowPhase = std::floor(seed) == floorPhase ? seed : floorPhase;
        double lowTime = base;
        double highPhase = floorPhase + 1.0;
        double highTime = storage.m_fallbackRightTime[slot].load();
        double frontierPhase = storage.m_frontierPhase.load();
        double frontierTime = storage.m_frontierTime.load();
        if (gridPhase == frontierPhase && std::isfinite(frontierTime))
        {
            return frontierTime <= cutoff ? frontierTime : Storage::x_missing;
        }

        double turn = storage.m_turnPhase[slot].load();
        if (std::isfinite(turn) && gridPhase >= turn)
        {
            lowPhase = turn;
            lowTime = base + static_cast<double>(storage.m_turnTime[slot].load());
            highTime = base + static_cast<double>(storage.m_currentRightTime[slot].load());
        }

        if (!std::isfinite(highTime) && TimeWarpInverseKernels::FrontierCloses(lowPhase, highPhase, frontierPhase))
        {
            highPhase = frontierPhase;
            highTime = frontierTime;
        }

        return TimeWarpInverseKernels::ReadInterval(lowPhase, highPhase, lowTime, highTime,
            gridPhase, cutoff, Storage::x_missing);
    }

    static void ScatterBufferToInverse(Storage& storage, double low, double high, double lowTime, double highTime)
    {
        int64_t first = static_cast<int64_t>(std::ceil(low));
        int64_t last = static_cast<int64_t>(std::floor(high));
        for (int64_t index = first; index <= last;)
        {
            size_t count = static_cast<size_t>(std::min<int64_t>(TimeWarpInverseKernels::x_batchCapacity, last - index + 1));
            std::array<double, TimeWarpInverseKernels::x_batchCapacity> times;
            for (size_t i = 0; i < count; ++i)
            {
                const double phase = static_cast<double>(index + static_cast<int64_t>(i));
                times[i] = TimeWarpInverseKernels::Interpolate(low, high, lowTime, highTime, phase);
            }

            storage.BeginWrite();
            if (static_cast<double>(index) > low)
            {
                storage.FinishCell(index - 1, times[0]);
            }

            for (size_t i = 0; i + 1 < count; ++i)
            {
                storage.CompleteCell(index + static_cast<int64_t>(i), times[i], times[i + 1]);
            }

            int64_t finalIndex = index + static_cast<int64_t>(count) - 1;
            storage.EnterCell(finalIndex, times[count - 1]);
            storage.m_frontierPhase.store(static_cast<double>(finalIndex));
            storage.m_frontierTime.store(times[count - 1]);
            storage.EndWrite();
            index += static_cast<int64_t>(count);
        }

        storage.m_highestWrittenIndex = std::max(storage.m_highestWrittenIndex, std::floor(high));
        storage.m_oldestWarpedTime = (storage.m_highestWrittenIndex - static_cast<double>(Storage::x_maxSamples - 1)) / storage.m_resolution;
    }

    static void Write(Storage& storage, double warpedTime, double realTime)
    {
        assert(std::isfinite(warpedTime) && std::isfinite(realTime));
        bool previousObservation = storage.m_hasObservation;
        storage.m_previousObservedRealTime = previousObservation ? storage.m_observedRealTime : realTime;
        storage.m_previousObservedWarpedTime = previousObservation ? storage.m_observedWarpedTime : warpedTime;
        storage.m_observedWarpedTime = warpedTime;
        storage.m_observedRealTime = realTime;
        storage.m_hasObservation = true;
        if (previousObservation && warpedTime > storage.m_lastWarpedTime)
        {
            double low = storage.m_lastWarpedTime * storage.m_resolution;
            double high = warpedTime * storage.m_resolution;
            if (!storage.m_hasForward || storage.m_descending)
            {
                BeginBranch(storage, low, storage.m_previousRealTime);
            }

            ScatterBufferToInverse(storage, low, high, storage.m_previousRealTime, realTime);
            storage.BeginWrite();
            storage.m_frontierPhase.store(high);
            storage.m_frontierTime.store(realTime);
            storage.EndWrite();
            storage.m_hasForward = true;
            storage.m_descending = false;
        }
        else if (previousObservation && warpedTime < storage.m_lastWarpedTime)
        {
            storage.m_descending = true;
        }

        storage.m_lastWarpedTime = warpedTime;
        storage.m_previousRealTime = realTime;
    }
};
"#

def generateCore : MetaM String := do
  let methods ← #[(``interpolateFloat, "Interpolate"), (``readIntervalFloat, "ReadInterval"),
    (``frontierClosesFloat, "FrontierCloses"),
    (``keepCutFloat, "KeepCut")].mapM fun (source, target) => emitKernel source target
  let lines := #["#pragma once", "", "// Generated from PhaseTime.InverseRuntime. Do not edit.", "//", "",
    "#include <algorithm>", "#include <array>", "#include <cassert>", "#include <cmath>", "#include <cstdint>", "",
    "struct TimeWarpInverseKernels", "{", s!"    static constexpr unsigned x_batchCapacity = {batchCapacity};", "",
    "    static bool Less(double a, double b)", "    {", "        return a < b;", "    }", "",
    "    static bool AtMost(double a, double b)", "    {", "        return a <= b;", "    }", "",
    "    static bool Both(bool a, bool b)", "    {", "        return a && b;", "    }", "",
    "    static bool Either(bool a, bool b)", "    {", "        return a || b;", "    }", "",
    "    static bool Negate(bool value)", "    {", "        return !value;", "    }", ""] ++
    methods.foldl (· ++ ·) #[] ++ #["};", ""]
  return String.intercalate "\n" lines.toList ++ storageLowering

elab "#generate_time_warp_inverse " path:str : command => do
  let code ← liftTermElabM generateCore
  let output : System.FilePath := path.getString
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code

end PhaseTime.InverseRuntime
