#pragma once

#include "QuadUtils.hpp"
#include "Math.hpp"
#include "SampleTimer.hpp"
#include "ScopeWriter.hpp"
#include "Slew.hpp"

// Phase and slew state are double: at the engine rate a slow LFO's per-sample
// increment and slew coefficient are below float resolution.
//
struct QuadLFO
{
    QuadDouble m_phase;

    QuadFloat m_output;

    OPLowPassFilterGeneric<double, 4> m_slew;

    ScopeWriterHolder m_scopeWriterHolder;

    QuadLFO()
        : m_phase(0.0, 0.0, 0.0, 0.0)
    {
    }

    void ConfigureScopeWriter(ScopeWriter* scopeWriter, size_t scopeIx)
    {
        m_scopeWriterHolder = ScopeWriterHolder(scopeWriter, 0, scopeIx);
    }

    void SetSlew(float freq)
    {
        m_slew.SetAlphaFromNatFreq(static_cast<double>(freq));
    }

    static float GetPhase(int i, float phaseKnob)
    {
        if (phaseKnob < 1.0 / 3)
        {
            return 3 * phaseKnob;
        }
        else if (phaseKnob < 5.0 / 12.0)
        {
            phaseKnob = (phaseKnob - 1.0 / 3) * 3;
            return phaseKnob * (i % 2 == 0 ? 1 : -1);
        }
        else if (phaseKnob < 11.0 / 24.0)
        {
            phaseKnob = (phaseKnob - 5.0 / 12.0) * 3;
            float base = 0.25 * (i % 2 == 0 ? 1 : -1);
            return base + phaseKnob * (i / 2 == 0 ? 1 : -1);
        }
        else
        {
            phaseKnob = (phaseKnob - 11.0 / 24.0) * 3;
            float base = 0.25 * (i % 2 == 0 ? 1 : -1) + 0.125 * (i / 2 == 0 ? 1 : -1);
            return base + phaseKnob;
        }
    }

    struct Input
    {
        QuadFloat m_freq;
        QuadFloat m_phaseKnob;
        Input()
            : m_freq(1.0f, 1.0f, 1.0f, 1.0f)
            , m_phaseKnob(0.0f, 0.0f, 0.0f, 0.0f)
        {
        }
      
        QuadFloat GetPhaseOffset() const
        {
            return QuadFloat(
                QuadLFO::GetPhase(0, m_phaseKnob[1]),
                QuadLFO::GetPhase(1, m_phaseKnob[2]),
                QuadLFO::GetPhase(2, m_phaseKnob[3]),
                QuadLFO::GetPhase(3, m_phaseKnob[0]));
        }
    };

    void Process(const Input& input)
    {
        QuadDouble prevPhase = m_phase;
        QuadDouble freq = input.m_freq.Convert<double>();

        for (int i = 1; i < 4; ++i)
        {
            if (freq[i] / freq[0] < 1.0001 && 0.9999 < freq[i] / freq[0])
            {
                double toCatch = (std::abs(m_phase[i] - m_phase[0]) > 0.5 && m_phase[i] < m_phase[0]) ? m_phase[0] - 1 : m_phase[0];
                m_phase[i] = (1 - 5 * freq[i]) * m_phase[i] + 5 * freq[i] * toCatch;
            }
        }

        m_phase += freq;
        QuadDouble unwrappedPhase = m_phase;
        m_phase = m_phase.ModOne();

        QuadDouble phase = m_phase + input.GetPhaseOffset().Convert<double>();
        phase = phase.ModOne();
        m_slew.m_alpha = QuadOPLowPassFilter::AlphaFromNatFreqApprox(input.m_freq * 2.0f).Convert<double>();
        m_output = m_slew.Process(Math::Sin2pi(phase.Convert<float>()).Convert<double>()).Convert<float>();

        if (m_scopeWriterHolder.m_scopeWriter)
        {
            m_scopeWriterHolder.Write(m_output);
            for (size_t i = 0; i < 4; ++i)
            {
                if (m_phase[i] < prevPhase[i])
                {
                    SampleTop top = SampleTop::FromPhases(prevPhase[i], unwrappedPhase[i]);
                    m_scopeWriterHolder.RecordStartAtVoice(i, top.m_offset);
                }
            }
        }
    }
};
