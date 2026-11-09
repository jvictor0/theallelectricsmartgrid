#pragma once

#include "ModulatorVisualizers.hpp"
#include "EncoderModulatorComponents.hpp"
#include "GangedRandomLFOComponent.hpp"
#include "ScopeComponent.hpp"
#include "RecentScopeComponent.hpp"
#include "EnvelopeFollowerComponent.hpp"

inline void InitializeEncoderModulatorVisualizers(
    ModulatorVisualizers& visualizers, TheNonagonSquiggleBoyInternal::UIState* uiState)
{
    visualizers.m_uiState = uiState;
#define H(modulator, ctor) \
    visualizers.Add(modulator, [&]([[maybe_unused]] int* scopeVoiceOffset) \
    { \
        return ctor; \
    });
#define G(name, slot, ctor, mode, color, modulator) H(modulator, ctor)
#include "ForEachModulationVisualizer.hpp"
#undef G
#undef H
#define F(tap, envelope, audio) \
    visualizers.Add(QuadModulator::envelope, [&]([[maybe_unused]] int*) \
    { \
        return std::make_unique<EnvelopeFollowerComponent>(uiState, QuadModulator::envelope); \
    }); \
    visualizers.Add(QuadModulator::audio, [&]([[maybe_unused]] int*) \
    { \
        return std::make_unique<RecentScopeComponent>(100.0); \
    });
#include "ForEachMixerModulationTap.hpp"
#undef F
#define F(machine, modulator, ctor) \
    visualizers.Add(VoiceMachine::SourceMachine::machine, VoiceModulator::modulator, \
        [&]([[maybe_unused]] int* scopeVoiceOffset) \
        { \
            return ctor; \
        });
#include "ForEachSourceModulationVisualizer.hpp"
#undef F
}
