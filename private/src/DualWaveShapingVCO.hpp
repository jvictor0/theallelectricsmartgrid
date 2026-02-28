#pragma once

#include "BitCrush.hpp"
#include "Math.hpp"
#include "PhaseUtils.hpp"
#include "SampleTimer.hpp"
#include "ScopeWriter.hpp"
#include "Slew.hpp"
#include "VectorPhaseShaper.hpp"

struct DualWaveShapingVCO
{
    static constexpr size_t x_oversample = 4;
    static constexpr size_t x_uBlockSize = SampleTimer::x_controlFrameRate * x_oversample;

    VectorPhaseShaperInternal m_vco[2];

    BitRateReducer m_bitRateReducer;

    bool m_top;
    float m_output;

    bool m_uBlockTop[SampleTimer::x_controlFrameRate];
    float m_uBlockOutput[x_uBlockSize];

    ScopeWriterHolder m_scopeWriter[2];

    PhaseUtils::ExpParam m_morphHarmonics[2];

    // Slewed parameters
    //
    ParamSlew m_vSlew[2];
    ParamSlew m_dSlew[2];
    ParamSlew m_wtBlendSlew[2];
    ParamSlew m_baseFreqSlew;
    ParamSlew m_fadeSlew;
    ParamSlew m_bitCrushAmountSlew;
    ParamSlew m_offsetFreqFactorSlew;
    ParamSlew m_detuneSlew;
    ParamSlew m_crossModIndexSlew[2];

    struct Input
    {
        float m_v[2];
        float m_d[2];
        float m_baseFreq;

        float m_morphHarmonics[2];
        float m_wtBlend[2];

        PhaseUtils::ExpParam m_offsetFreqFactor;
        PhaseUtils::ExpParam m_detune;

        PhaseUtils::ZeroedExpParam m_crossModIndex[2];

        float m_fade;

        float m_bitCrushAmount;

        Input()
            : m_v{0.5, 0.5}
            , m_d{0.5, 0.5}
            , m_baseFreq(PhaseUtils::VOctToNatural(0.0, 1.0 / 48000.0))
            , m_morphHarmonics{0.0, 0.0}
            , m_wtBlend{0, 0}
            , m_offsetFreqFactor(4)
            , m_detune(1.03)
            , m_fade(0)
            , m_bitCrushAmount(0)
        {
        }
    };

    DualWaveShapingVCO()
        : m_output(0)
        , m_vSlew{ParamSlew(x_oversample), ParamSlew(x_oversample)}
        , m_dSlew{ParamSlew(x_oversample), ParamSlew(x_oversample)}
        , m_wtBlendSlew{ParamSlew(x_oversample), ParamSlew(x_oversample)}
        , m_baseFreqSlew(x_oversample)
        , m_fadeSlew(x_oversample)
        , m_bitCrushAmountSlew(x_oversample)
        , m_offsetFreqFactorSlew(x_oversample)
        , m_detuneSlew(x_oversample)
        , m_crossModIndexSlew{ParamSlew(x_oversample), ParamSlew(x_oversample)}
    {
    }

    void ProcessUBlock(const Input& input)
    {
        m_output = 0;

        // Update slew targets
        //
        for (int j = 0; j < 2; ++j)
        {
            m_vSlew[j].Update(input.m_v[j]);
            m_dSlew[j].Update(input.m_d[j]);
            m_wtBlendSlew[j].Update(input.m_wtBlend[j]);
            m_crossModIndexSlew[j].Update(input.m_crossModIndex[j].m_expParam);
        }

        m_baseFreqSlew.Update(input.m_baseFreq);
        m_fadeSlew.Update(input.m_fade);
        m_bitCrushAmountSlew.Update(input.m_bitCrushAmount);
        m_offsetFreqFactorSlew.Update(input.m_offsetFreqFactor.m_expParam);
        m_detuneSlew.Update(input.m_detune.m_expParam);

        bool top[2] = {false, false};

        for (size_t i = 0; i < x_uBlockSize; ++i)
        {
            VectorPhaseShaperInternal::Input vcoInput[2];

            vcoInput[0].m_useVoct = false;
            vcoInput[1].m_useVoct = false;

            for (int j = 0; j < 2; ++j)
            {
                vcoInput[j].m_v = m_vSlew[j].Process();
                vcoInput[j].m_d = m_dSlew[j].Process();
                vcoInput[j].m_wtBlend = m_wtBlendSlew[j].Process();
            }

            float baseFreq = m_baseFreqSlew.Process() / x_oversample;
            float offsetFreqFactor = m_offsetFreqFactorSlew.Process();
            float detune = m_detuneSlew.Process();

            vcoInput[0].m_phaseMod = m_crossModIndexSlew[0].Process() * m_vco[1].m_out;
            vcoInput[0].m_freq = baseFreq * detune;
            vcoInput[1].m_freq = baseFreq * offsetFreqFactor / detune;

            for (int j = 0; j < 2; ++j)
            {
                vcoInput[j].m_morphHarmonics = m_morphHarmonics[j].Update(vcoInput[j].m_freq, vcoInput[j].m_maxFreq, input.m_morphHarmonics[j]);
            }

            m_vco[0].Process(vcoInput[0], 0 /*unused*/);
            vcoInput[1].m_phaseMod = m_vco[0].m_out * m_crossModIndexSlew[1].Process();
            m_vco[1].Process(vcoInput[1], 0 /*unused*/);

            top[0] = top[0] || m_vco[0].m_top;
            top[1] = top[1] || m_vco[1].m_top;

            float fade = m_fadeSlew.Process();
            float mixed = m_vco[0].m_out * Math::Cos2pi(fade / 4) + m_vco[1].m_out * Math::Cos2pi(fade / 4 + 0.75);

            float bitCrushAmount = m_bitCrushAmountSlew.Process();
            m_bitRateReducer.SetAmount(bitCrushAmount);
            float crushed = m_bitRateReducer.Process(mixed);

            m_uBlockOutput[i] = crushed;

            // Base-rate operations every x_oversample samples
            //
            if ((i + 1) % x_oversample == 0)
            {
                size_t baseIndex = i / x_oversample;

                m_scopeWriter[0].Write(baseIndex, m_vco[0].m_out);
                m_scopeWriter[1].Write(baseIndex, m_vco[1].m_out);

                if (top[0])
                {
                    m_scopeWriter[0].RecordStart(baseIndex);
                }

                if (top[1])
                {
                    m_scopeWriter[1].RecordStart(baseIndex);
                }

                m_uBlockTop[baseIndex] = top[0];
                top[0] = false;
                top[1] = false;
            }
        }
    }

    float Process(const Input& input)
    {
        if (SampleTimer::IsControlFrame())
        {
            ProcessUBlock(input);
        }

        m_output = m_uBlockOutput[SampleTimer::GetUBlockIndex()];
        m_top = m_uBlockTop[SampleTimer::GetUBlockIndex()];

        return m_output;
    }
};
