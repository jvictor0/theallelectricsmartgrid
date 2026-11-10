#pragma once

#include "EncoderBank.hpp"
#include "Oversample.hpp"
#include <array>
#include <cassert>
#include <vector>

struct ModulatorManager
{
    using Values = SmartGrid::BankedEncoderCell::ModulatorValues;
    using Gestures = SmartGrid::BankedEncoderCell::GestureValues;
    static constexpr size_t x_numSlots = SmartGrid::BankedEncoderCell::x_numModulators;
    static constexpr size_t x_noSource = static_cast<size_t>(-1);
    static constexpr size_t x_decimatorOrder = 2;
    using Decimator = DownsamplerGeneric<float, 4, x_decimatorOrder, SampleTimer::x_oversample>;

    struct Source
    {
        QuadFloat m_value;
        QuadFloat m_hostValue;
        QuadFloat m_amplitude = QuadFloat::Splat(1.0f);
        Decimator m_decimator;
        bool m_requiresDownsampling = true;
    };

    struct Group
    {
        std::array<size_t, x_numSlots> m_sources;
        Values m_values;

        Group()
        {
            m_sources.fill(x_noSource);
        }
    };

    // Storage is sized once, before encoders bind to group values.
    //
    std::vector<Source> m_sources;
    std::vector<Group> m_groups;
    Gestures m_gestureValues;

    ModulatorManager(size_t numSources, size_t numGroups)
        : m_sources(numSources)
        , m_groups(numGroups)
    {
        for (size_t i = 0; i < numGroups; ++i)
        {
            m_groups[i].m_values.m_groupIx = i;
        }
    }

    template<typename Index>
    Source& GetSource(Index index)
    {
        size_t source = static_cast<size_t>(index);
        assert(source < m_sources.size());
        return m_sources[source];
    }

    template<typename Index>
    void Set(Index index, size_t lane, float value)
    {
        assert(lane < 4);
        GetSource(index).m_value[lane] = value;
    }

    template<typename Index>
    void Set(Index index, QuadFloat value)
    {
        GetSource(index).m_value = value;
    }

    template<typename Index>
    void SetAmplitude(Index index, size_t lane, float amplitude)
    {
        assert(lane < 4);
        GetSource(index).m_amplitude[lane] = amplitude;
    }

    void Bind(size_t group, size_t slot, size_t source)
    {
        assert(group < m_groups.size());
        assert(slot < x_numSlots);
        assert(source < m_sources.size());
        m_groups[group].m_sources[slot] = source;
    }

    void ProcessSample()
    {
        for (auto& source : m_sources)
        {
            if (!source.m_requiresDownsampling)
            {
                continue;
            }

            source.m_hostValue = source.m_decimator.Process(source.m_value);
        }
    }

    void Gather()
    {
        for (auto& source : m_sources)
        {
            if (!source.m_requiresDownsampling)
            {
                source.m_hostValue = source.m_value;
            }
        }

        for (auto& group : m_groups)
        {
            for (size_t slot = 0; slot < x_numSlots; ++slot)
            {
                size_t index = group.m_sources[slot];
                if (index != x_noSource)
                {
                    const Source& source = m_sources[index];
                    group.m_values.m_value[slot] = source.m_value;
                    group.m_values.m_hostValue[slot] = source.m_hostValue;
                    group.m_values.m_amplitude[slot] = source.m_amplitude;
                }
            }
        }
    }

    void SetGestureWeights(const float* weights)
    {
        std::copy_n(weights, SmartGrid::BankedEncoderCell::x_numGestureParams, m_gestureValues.m_gestureWeights);
    }
};
