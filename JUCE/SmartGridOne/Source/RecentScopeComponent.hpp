#pragma once

#include "ModulatorTimeAxisComponent.hpp"

struct RecentScopeComponent : ModulatorTimeAxisComponent
{
    double m_windowMilliseconds;

    explicit RecentScopeComponent(double windowMilliseconds)
        : m_windowMilliseconds(windowMilliseconds)
    {
        assert(std::isfinite(windowMilliseconds) && windowMilliseconds > 0.0);
    }

    ModulatorTimeAxis GetTimeAxis(size_t) const override
    {
        if (!m_historyUIState)
        {
            return {};
        }

        auto source = GetSource(0);
        auto scope = SmartGridOneModulatorHistory::GetControlScope(*m_historyUIState, source);
        if (!scope.m_writer || scope.m_writer->m_acceptedIndex == 0)
        {
            return {};
        }

        double first = static_cast<double>(m_historyUIState->m_squiggleBoyUIState.m_firstScopeSample.load());
        double end = first + scope.m_writer->m_acceptedIndex - 1;
        double window = SampleTimer::SecondsToSamples(m_windowMilliseconds * 0.001);
        return {std::max(first, end - window), end};
    }
};
