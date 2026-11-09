#pragma once

#include "EncoderEnums.hpp"
#include "BitSet.hpp"
#include "SmartGridOneModulators.hpp"
#include "SmartGridOneMainVisualizerComponent.hpp"
#include "ModulatorTimeAxisComponent.hpp"
#include <array>
#include <memory>
#include <optional>
#include <utility>

struct ModulatorVisualizers
{
    using Views = std::array<std::unique_ptr<SmartGridOneMainVisualizerComponent>, SmartGridOneModulators::x_numSources>;
    Views m_views;
    std::array<Views, static_cast<size_t>(VoiceMachine::SourceMachine::NumSourceMachines)> m_sourceMachineViews;
    int m_scopeVoiceOffset = -1;
    TheNonagonSquiggleBoyInternal::UIState* m_uiState = nullptr;
    ModulatorTimeAxisComponent m_recentHistory;

    template<typename Modulator, typename Factory>
    void AddTo(Views& views, Modulator modulator, Factory&& factory)
    {
        for (size_t index = 0; index < SmartGridOneModulators::x_numSources; ++index)
        {
            auto source = static_cast<SmartGridOneModulators::Source>(index);
            auto ref = SmartGridOneModulators::GetRef(source, 0);
            if (ref.m_mode != SmartGridOneModulators::GetMode(modulator)
                || ref.m_index != static_cast<size_t>(modulator))
            {
                continue;
            }

            auto& view = views[index];
            jassert(!view);
            view = factory(&m_scopeVoiceOffset);
            view->SetVoiceTrack(ref.m_voice / 3);
            if (auto* timeView = dynamic_cast<ModulatorTimeAxisComponent*>(view.get()))
            {
                timeView->Configure(m_uiState, ref.m_mode, ref.m_index, &m_scopeVoiceOffset);
            }
        }
    }

    template<typename Modulator, typename Factory>
    void Add(Modulator modulator, Factory&& factory)
    {
        AddTo(m_views, modulator, std::forward<Factory>(factory));
    }

    template<typename Factory>
    void Add(VoiceMachine::SourceMachine machine, VoiceModulator modulator, Factory&& factory)
    {
        assert(modulator == VoiceModulator::SourceMod1 || modulator == VoiceModulator::SourceMod2);
        AddTo(m_sourceMachineViews[static_cast<size_t>(machine)], modulator, std::forward<Factory>(factory));
    }

    template<typename Factory>
    void Add(std::nullopt_t, Factory&&)
    {
    }

    SmartGridOneMainVisualizerComponent* Get(SmartGridOneModulators::Source source) const
    {
        size_t index = static_cast<size_t>(source);
        if (index >= m_views.size())
        {
            return nullptr;
        }

        auto ref = SmartGridOneModulators::GetRef(source, 0);
        if (ref.m_mode == SmartGridOneLayout::ParamMode::Voice
            && (ref.m_index == static_cast<size_t>(VoiceModulator::SourceMod1)
                || ref.m_index == static_cast<size_t>(VoiceModulator::SourceMod2)))
        {
            if (!m_uiState)
            {
                return nullptr;
            }

            size_t machine = static_cast<size_t>(m_uiState->m_squiggleBoyUIState
                .m_voiceSourceUIState[ref.m_voice].m_sourceMachine.load());
            return machine < m_sourceMachineViews.size() ? m_sourceMachineViews[machine][index].get() : nullptr;
        }

        return m_views[index].get();
    }

    SmartGridOneMainVisualizerComponent* Get(SmartGridOne::EncoderBankMode mode, int index) const
    {
        if (static_cast<size_t>(mode) >= SmartGridOneModulators::x_sourceIds.size()
            || index < 0 || index >= static_cast<int>(SmartGridOneModulators::x_numSourceIndices))
        {
            return nullptr;
        }

        return Get(SmartGridOneModulators::x_sourceIds[static_cast<size_t>(mode)][index]);
    }

    SmartGridOneMainVisualizerComponent* Get(SmartGridOneModulators::Group group, int slot) const
    {
        if (static_cast<size_t>(group) >= SmartGridOneModulators::x_numGroups
            || slot < 0 || slot >= static_cast<int>(SmartGridOneModulators::x_numSlots))
        {
            return nullptr;
        }

        return Get(SmartGridOneModulators::x_groups[static_cast<size_t>(group)][slot]);
    }

    SmartGridOneMainVisualizerComponent* GetForParameter(const EncoderUIState& encoder)
    {
        EncoderUIState::ModulationWeights snapshot;
        if (!encoder.ReadModulationWeights(snapshot) || snapshot.m_modulatorGroup >= SmartGridOneModulators::x_numGroups)
        {
            return nullptr;
        }

        using BankMode = SmartGridOne::EncoderBankMode;
        using Mode = SmartGridOneLayout::ParamMode;
        Mode mode;
        size_t firstVoice = 0;
        switch (snapshot.m_mode)
        {
            case BankMode::VoiceWater:
            case BankMode::VoiceFire:
            case BankMode::VoiceEarth:
                mode = Mode::Voice;
                firstVoice = 3 * static_cast<size_t>(snapshot.m_mode);
                break;
            case BankMode::Quad:
                mode = Mode::Quad;
                break;
            case BankMode::Global:
                mode = Mode::Global;
                break;
            default:
                return nullptr;
        }

        std::array<WeightedModulators, 4> weights;
        size_t voices = std::min(snapshot.m_numVoices, weights.size());
        SmartGridOneMainVisualizerComponent* selected = nullptr;
        SmartGridOneMainVisualizerComponent* singleSource = nullptr;
        double longest = 0.0;
        size_t usedSources = 0;
        for (size_t index = 0; index < WeightedModulators::x_capacity; ++index)
        {
            auto* view = Get(static_cast<SmartGridOneModulators::Group>(snapshot.m_modulatorGroup), static_cast<int>(index));
            if (!view)
            {
                continue;
            }

            bool used = false;
            for (size_t voice = 0; voice < voices; ++voice)
            {
                float weight = snapshot.m_weights[voice][index];
                if (weight != 0.0f)
                {
                    auto source = SmartGridOneModulators::GetRef(
                        SmartGridOneModulators::x_groups[snapshot.m_modulatorGroup][index], voice);
                    weights[voice].Add(source, weight);
                    used = true;
                }
            }

            if (used)
            {
                ++usedSources;
                singleSource = view;
            }

            double extent = used ? view->GetTimeExtent() : 0.0;
            if (extent > longest)
            {
                longest = extent;
                selected = view;
            }
        }

        if (usedSources == 0)
        {
            return nullptr;
        }

        if (usedSources == 1)
        {
            singleSource->ClearWeightedModulators();
            return singleSource;
        }

        if (!selected)
        {
            if (!m_uiState)
            {
                return nullptr;
            }

            m_recentHistory.Configure(m_uiState, mode, 0, &m_scopeVoiceOffset);
            m_recentHistory.m_fallback = true;
            m_recentHistory.SetVoiceTrack(firstVoice / 3);
            selected = &m_recentHistory;
        }

        selected->ClearWeightedModulators();
        for (size_t voice = 0; voice < voices; ++voice)
        {
            weights[voice].Normalize();
            selected->SetWeightedModulators(voice, weights[voice]);
        }

        return selected;
    }

    SmartGridOneMainVisualizerComponent* GetSingle(SmartGridOne::EncoderBankMode mode, BitSet16 modulators) const
    {
        SmartGridOneMainVisualizerComponent* result = nullptr;
        unsigned int bits = modulators.m_bits;
        while (bits != 0)
        {
            int index = __builtin_ctz(bits);
            bits &= bits - 1;
            if (auto* view = Get(mode, index))
            {
                if (result)
                {
                    return nullptr;
                }

                result = view;
            }
        }

        return result;
    }
};
