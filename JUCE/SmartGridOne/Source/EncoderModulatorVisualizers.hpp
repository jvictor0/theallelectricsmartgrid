#pragma once

#include "ModulatorVisualizers.hpp"
#include "EncoderModulatorComponents.hpp"
#include "GangedRandomLFOComponent.hpp"
#include "ScopeComponent.hpp"

inline void InitializeEncoderModulatorVisualizers(
    ModulatorVisualizers& visualizers, TheNonagonSquiggleBoyInternal::UIState* uiState)
{
#define H(modulator, ctor) \
    visualizers.Add(modulator, [&]([[maybe_unused]] int* scopeVoiceOffset) \
    { \
        return ctor; \
    });
#define G(name, slot, ctor, mode, color, modulator) H(modulator, ctor)
#include "ForEachModulationVisualizer.hpp"
#undef G
#undef H
}
