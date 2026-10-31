#pragma once

#include "ModulationMatrixUIState.hpp"
#include "SmartGridOneParamVisualizers.hpp"
#include "VisualizerRegion.hpp"
#include <array>

struct LFOTargetVisualizers
{
    using Associations = SmartGridOneParamVisualizers;
    using Visualizer = Associations::Visualizer;

    enum class Page
    {
        Voice,
        Quad
    };

    static constexpr size_t x_maxVisualizers = 4;
    std::array<Visualizer, x_maxVisualizers> m_visualizers{};
    size_t m_count = 0;

    static LFOTargetVisualizers Find(const ModulationMatrixUIState& matrix, Page page, size_t trio)
    {
        assert(trio < SmartGridOneLayout::x_numTrios);
        size_t lfo1 = page == Page::Voice
            ? ModulationMatrixUIState::GetModulatorId(VoiceModulator::LFO1, trio)
            : ModulationMatrixUIState::GetModulatorId(QuadModulator::LFO1);
        size_t lfo2 = page == Page::Voice
            ? ModulationMatrixUIState::GetModulatorId(VoiceModulator::LFO2, trio)
            : ModulationMatrixUIState::GetModulatorId(QuadModulator::LFO2);
        LFOTargetVisualizers result;

        // Stable parameter order keeps the chosen four from changing between frames.
        // Both LFO columns already include the recursive badge semantics.
        //
        for (size_t i = 0; i < SmartGridOneLayout::x_numParams; ++i)
        {
            auto param = static_cast<Associations::Param>(i);
            if (Associations::IsLFOParam(param)
                || !(matrix.IsModulatedBy(param, trio, lfo1)
                    || matrix.IsModulatedBy(param, trio, lfo2)))
            {
                continue;
            }

            auto visualizer = Associations::Get(param);
            bool duplicate = false;
            for (size_t j = 0; j < result.m_count; ++j)
            {
                duplicate |= result.m_visualizers[j] == visualizer;
            }

            if (!duplicate)
            {
                result.m_visualizers[result.m_count++] = visualizer;
                if (result.m_count == x_maxVisualizers)
                {
                    break;
                }
            }
        }

        return result;
    }

    VisualizerRegion GetRegion(size_t index) const
    {
        assert(index < m_count && m_count <= x_maxVisualizers);
        constexpr std::array<std::array<VisualizerRegion, x_maxVisualizers>, x_maxVisualizers> x_regions =
        {{
            {VisualizerRegion::Full},
            {VisualizerRegion::Top, VisualizerRegion::Bottom},
            {VisualizerRegion::Top, VisualizerRegion::BottomLeft, VisualizerRegion::BottomRight},
            {VisualizerRegion::TopLeft, VisualizerRegion::TopRight,
                VisualizerRegion::BottomLeft, VisualizerRegion::BottomRight}
        }};
        return x_regions[m_count - 1][index];
    }
};
