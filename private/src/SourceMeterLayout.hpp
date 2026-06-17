#pragma once

#include "SourceMixer.hpp"

struct SourceMeterLayout
{
    struct Slots
    {
        float m_leftReductionX0;
        float m_leftReductionX1;
        float m_leftMeterX0;
        float m_leftMeterX1;
        float m_rightMeterX0;
        float m_rightMeterX1;
        float m_rightReductionX0;
        float m_rightReductionX1;
    };

    static Slots Make(float groupX0, float groupX1, const SourceMixer::SourceConfig& config)
    {
        float groupWidth = groupX1 - groupX0;
        if (config.IsStereo())
        {
            float slotWidth = groupWidth / 4.0f;
            return {
                groupX0,
                groupX0 + slotWidth,
                groupX0 + slotWidth,
                groupX0 + 2.0f * slotWidth,
                groupX0 + 2.0f * slotWidth,
                groupX0 + 3.0f * slotWidth,
                groupX0 + 3.0f * slotWidth,
                groupX1
            };
        }

        float slotWidth = groupWidth / 2.0f;
        return {
            groupX0 + slotWidth,
            groupX1,
            groupX0,
            groupX0 + slotWidth,
            groupX0,
            groupX0 + slotWidth,
            groupX0 + slotWidth,
            groupX1
        };
    }
};
