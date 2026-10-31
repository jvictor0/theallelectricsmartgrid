#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include "BitSet.hpp"
#include "EncoderEnums.hpp"
#include "SmartGridOneLayout.hpp"
#include "SmartGridOneModulators.hpp"

struct ModulationMatrixUIState
{
    using Layout = SmartGridOneLayout;
    using Param = Layout::Param;
    using Mode = SmartGridOne::EncoderBankMode;

    static constexpr size_t x_numParams = Layout::x_numParamInstances;
    static constexpr size_t x_numModulatorsPerMode = SmartGridOneModulators::x_skins[0].size();
    static constexpr size_t x_numModulators = static_cast<size_t>(Mode::NumModes) * x_numModulatorsPerMode;

    static_assert(std::atomic<bool>::is_always_lock_free,
        "Publishing modulation flags must not lock the audio thread");

    // Rows are parameter instances; columns are mode + modulator slot, so the
    // three voice tracks, quad, and global modulators have distinct identities.
    // Unused parameter rows and cross-mode cells remain false.
    //
    std::array<std::array<std::atomic<bool>, x_numModulators>, x_numParams> m_modulatedBy;

    ModulationMatrixUIState()
    {
        for (auto& row : m_modulatedBy)
        {
            for (auto& cell : row)
            {
                std::atomic_init(&cell, false);
            }
        }
    }

    static constexpr size_t GetParamId(Param param, size_t trio)
    {
        size_t id = Layout::GetParamInstanceId(param, trio);
        return Layout::GetParamMode(param) == Layout::ParamMode::Voice ? id : id - trio;
    }

    static constexpr Param GetParam(size_t paramId)
    {
        assert(paramId < x_numParams);
        return static_cast<Param>(paramId / Layout::x_numTrios);
    }

    static constexpr size_t GetTrio(size_t paramId)
    {
        assert(paramId < x_numParams);
        return paramId % Layout::x_numTrios;
    }

    static constexpr Mode GetMode(size_t paramId)
    {
        auto mode = Layout::GetParamMode(GetParam(paramId));
        return mode == Layout::ParamMode::Voice ? static_cast<Mode>(GetTrio(paramId))
            : mode == Layout::ParamMode::Quad ? Mode::Quad : Mode::Global;
    }

    static constexpr size_t GetModulatorId(Mode mode, size_t index)
    {
        assert(static_cast<size_t>(mode) < static_cast<size_t>(Mode::NumModes));
        assert(index < x_numModulatorsPerMode);
        return static_cast<size_t>(mode) * x_numModulatorsPerMode + index;
    }

    static constexpr size_t GetModulatorId(VoiceModulator modulator, size_t trio)
    {
        assert(trio < Layout::x_numTrios);
        return GetModulatorId(static_cast<Mode>(trio), static_cast<size_t>(modulator));
    }

    static constexpr size_t GetModulatorId(QuadModulator modulator)
    {
        return GetModulatorId(Mode::Quad, static_cast<size_t>(modulator));
    }

    static constexpr size_t GetModulatorId(GlobalModulator modulator)
    {
        return GetModulatorId(Mode::Global, static_cast<size_t>(modulator));
    }

    // Called by the audio thread during UI publication with each base encoder's
    // cached badge bits. These include nested modulation depths and active-scene
    // gesture assignments, not instantaneous signal values or dependencies
    // through other base parameters. Each cell is atomic, not a whole snapshot.
    //
    void SetModulatorsAffecting(size_t paramId, BitSet16 modulators)
    {
        assert(paramId == GetParamId(GetParam(paramId), GetTrio(paramId)));
        size_t firstModulator = GetModulatorId(GetMode(paramId), 0);
        for (size_t index = 0; index < x_numModulatorsPerMode; ++index)
        {
            m_modulatedBy[paramId][firstModulator + index].store(
                modulators.Get(static_cast<int>(index)), std::memory_order_relaxed);
        }
    }

    bool IsModulatedBy(size_t paramId, size_t modulatorId) const
    {
        assert(paramId < x_numParams);
        assert(modulatorId < x_numModulators);
        return m_modulatedBy[paramId][modulatorId].load(std::memory_order_relaxed);
    }

    bool IsModulatedBy(Param param, size_t trio, size_t modulatorId) const
    {
        return IsModulatedBy(GetParamId(param, trio), modulatorId);
    }

    // UI-thread reverse lookup, independent of the selected page or track.
    // The callback receives (Param, trio); shared parameters always use trio 0.
    //
    template<typename Func>
    void ForEachParamModulatedBy(size_t modulatorId, Func func) const
    {
        assert(modulatorId < x_numModulators);
        for (size_t paramId = 0; paramId < x_numParams; ++paramId)
        {
            if (IsModulatedBy(paramId, modulatorId))
            {
                func(GetParam(paramId), GetTrio(paramId));
            }
        }
    }
};
