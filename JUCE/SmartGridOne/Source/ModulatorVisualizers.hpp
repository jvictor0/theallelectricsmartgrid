#pragma once

#include "EncoderEnums.hpp"
#include "BitSet.hpp"
#include "SmartGridOneModulators.hpp"
#include "SmartGridOneMainVisualizerComponent.hpp"
#include <array>
#include <memory>
#include <optional>

struct ModulatorVisualizers
{
    std::array<std::array<std::unique_ptr<SmartGridOneMainVisualizerComponent>, 15>, 3> m_views;
    int m_scopeVoiceOffset = -1;

    template<typename Modulator, typename Factory>
    void Add(Modulator modulator, Factory&& factory)
    {
        auto& view = m_views[static_cast<size_t>(SmartGridOneModulators::GetMode(modulator))]
            [static_cast<size_t>(modulator)];
        jassert(!view);
        view = factory(&m_scopeVoiceOffset);
    }

    template<typename Factory>
    void Add(std::nullopt_t, Factory&&)
    {
    }

    SmartGridOneMainVisualizerComponent* Get(SmartGridOne::EncoderBankMode mode, int index) const
    {
        if (index < 0 || index >= static_cast<int>(m_views[0].size()))
        {
            return nullptr;
        }

        using Mode = SmartGridOne::EncoderBankMode;
        switch (mode)
        {
            case Mode::VoiceWater:
            case Mode::VoiceFire:
            case Mode::VoiceEarth:
                return m_views[0][index].get();
            case Mode::Quad:
                return m_views[1][index].get();
            case Mode::Global:
                return m_views[2][index].get();
            default:
                return nullptr;
        }
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
