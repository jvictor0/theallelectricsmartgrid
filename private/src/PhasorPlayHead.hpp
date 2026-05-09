#pragma once

#include "PhaseUtils.hpp"
#include "TheoryOfTime.hpp"
#include <algorithm>
#include <cmath>

struct PhasorPlayHead
{
    struct Input
    {
        TheoryOfTime* m_theoryOfTime;
        float m_start;
        float m_length;
        float m_speed;
        int m_loopIndex;
        int m_sampleIndex;

        Input()
            : m_theoryOfTime(nullptr)
            , m_start(0.0f)
            , m_length(1.0f)
            , m_speed(1.0f)
            , m_loopIndex(0)
            , m_sampleIndex(0)
        {
        }
    };

    float Process(Input const& input)
    {
        if (!input.m_theoryOfTime)
        {
            return 0.0f;
        }

        double length = static_cast<double>(input.m_length);
        if (length <= 0.0)
        {
            return 0.0f;
        }

        size_t sampleIdx =
            static_cast<size_t>(input.m_sampleIndex) % TheoryOfTimeBase::x_microBlockSize;
        TheoryOfTime* tot = input.m_theoryOfTime;
        size_t loopIx = static_cast<size_t>(std::max(0, input.m_loopIndex));

        double loopScaled =
            tot->GetIndirectPhasor(sampleIdx, loopIx) * static_cast<double>(input.m_speed) * length;
        double start = static_cast<double>(input.m_start);
        double wrapped = PhaseUtils::WrapMod(0, length, loopScaled) + start;
        double fractional = wrapped - std::floor(wrapped);
        return static_cast<float>(fractional);
    }
};
