#pragma once

#include "QuadUtils.hpp"
#include "AdaptiveWaveTable.hpp"
#include "Q.hpp"

struct Resynthesizer
{
    struct Grain
    {
        float Process()
        {
            if (m_index < BasicWaveTable::x_tableSize)
            {
                float value = m_buffer.m_table[m_index] * (0.5 - 0.5 * m_windowTable->m_table[m_index]);
                ++m_index;
                return value;
            }
            else
            {
                m_running = false;
                return 0.0f;
            }        
        }

        void Start()
        {
            m_index = 0;
            m_running = true;
        }

        size_t m_index;
        BasicWaveTable m_buffer;
        const WaveTable* m_windowTable;
        bool m_running;
    };

    struct Oscillator
    {
        static constexpr size_t x_numShifts = 3;

        struct Input
        {
            float m_gain[x_numShifts];
            Q m_shift[x_numShifts];

            Input()
            {
                for (size_t i = 0; i < x_numShifts; ++i)
                {
                    m_gain[i] = i == 0 ? 1.0f : 0.0f;
                    m_shift[i] = Q(1, 1);
                }
            }
        };

        Oscillator(Resynthesizer* owner)
            : m_owner(owner)
        {
            for (size_t i = 0; i < DiscreteFourierTransform::x_maxComponents; ++i)
            {
                m_synthesisPhase[i] = 0.0f;
            }
        }

        void Clear()
        {
            for (size_t i = 0; i < DiscreteFourierTransform::x_maxComponents; ++i)
            {
                m_synthesisPhase[i] = 0.0f;
            }
        }

        void FixupPhases(double deltaTime)
        {
            constexpr float N = static_cast<float>(BasicWaveTable::x_tableSize);
            constexpr float H = N / static_cast<float>(x_hopDenom);

            if (std::abs(deltaTime) < 1e-12)
            {
                return;
            }

            for (size_t i = 0; i < DiscreteFourierTransform::x_maxComponents; ++i)
            {    
                double omegaInstantaneous = m_owner->OmegaInstantaneous(i, deltaTime);
                m_synthesisPhase[i] = m_synthesisPhase[i] + omegaInstantaneous * H;
            }
        }

        void SynthesizeSingleShift(DiscreteFourierTransform& dft, float gain, Q shift)
        {
            for (size_t i = 0; i < DiscreteFourierTransform::x_maxComponents; ++i)
            {
                float mag = gain * m_owner->m_magnitudes[i];
                double phase = m_synthesisPhase[i] * shift.ToDouble();
                std::complex<float> component = std::polar(mag, static_cast<float>(phase));

                double exactIndex = static_cast<double>(i) * shift.ToDouble();
                size_t loIndex = static_cast<size_t>(exactIndex);
                size_t hiIndex = loIndex + 1;
                float hiFrac = static_cast<float>(exactIndex - loIndex);
                float loFrac = 1.0f - hiFrac;

                if (0 < loIndex && loIndex < DiscreteFourierTransform::x_maxComponents)
                {
                    dft.m_components[loIndex] += component * loFrac;
                }
                else if (loIndex == 0)
                {
                    dft.m_components[hiIndex] += component * loFrac;
                }

                if (0 < hiIndex && hiIndex < DiscreteFourierTransform::x_maxComponents)
                {
                    dft.m_components[hiIndex] += component * hiFrac;
                }
                else if (hiIndex == DiscreteFourierTransform::x_maxComponents)
                {
                    dft.m_components[loIndex] += component * hiFrac;
                }
            }
        }

        void Synthesize(DiscreteFourierTransform& dft, Input input)
        {
            for (size_t i = 0; i < x_numShifts; ++i)
            {
                SynthesizeSingleShift(dft, input.m_gain[i], input.m_shift[i]);
            }
        }

        double m_synthesisPhase[DiscreteFourierTransform::x_maxComponents];
        Resynthesizer* m_owner;
    };

    struct Input
    {
        Oscillator::Input m_oscillatorInput;
        double m_startTime;

        Input()
            : m_oscillatorInput()
            , m_startTime(0.0)
        {
        }
    };

    void Clear()
    {
        m_startTime = 0.0;
        m_oscillator.Clear();
        for (size_t i = 0; i < DiscreteFourierTransform::x_maxComponents; ++i)
        {
            m_analysisPhase[i] = 0.0f;
            m_analysisPhasePrev[i] = 0.0f;
            m_magnitudes[i] = 0.0f;
        }
    }

    Resynthesizer()
        : m_oscillator(this)
    {
        Clear();
    }

    void ProcessPhases(DiscreteFourierTransform& dft)
    {
        for (size_t i = 0; i < DiscreteFourierTransform::x_maxComponents; ++i)
        {
            m_analysisPhasePrev[i] = m_analysisPhase[i];
            m_analysisPhase[i] = std::arg(dft.m_components[i]);
            m_magnitudes[i] = std::abs(dft.m_components[i]);
        }
    }

    float DeltaPhi(int bin)
    {
        return m_analysisPhase[bin] - m_analysisPhasePrev[bin];
    }

    float OmegaBin(int bin)
    {
        constexpr float twoPi = 2.0f * static_cast<float>(M_PI);
        constexpr float N = static_cast<float>(BasicWaveTable::x_tableSize);
        return twoPi * static_cast<float>(bin) / N;
    }

    float OmegaInstantaneous(int bin, double deltaTime)
    {
        constexpr float twoPi = 2.0f * static_cast<float>(M_PI);
        
        float omegaBin = OmegaBin(bin);
        float deltaPhiExpected = omegaBin * deltaTime;
        float deltaPhi = DeltaPhi(bin);
        
        deltaPhi -= deltaPhiExpected;
        deltaPhi = std::fmod(deltaPhi + static_cast<float>(M_PI), twoPi);
        if (deltaPhi <= 0.0f)
        {
            deltaPhi += twoPi;
        }

        deltaPhi -= static_cast<float>(M_PI);
        
        return omegaBin + deltaPhi / deltaTime;
    }

    void Analyze(BasicWaveTable& buffer, DiscreteFourierTransform& dft)
    {
        dft.Transform(buffer);
        ProcessPhases(dft);
    }

    void Synthesize(Grain* grain, DiscreteFourierTransform& dft, Input input)
    {
        double deltaTime = input.m_startTime - m_startTime;
        m_startTime = input.m_startTime;

        m_oscillator.FixupPhases(deltaTime);

        DiscreteFourierTransform synthDft;
        m_oscillator.Synthesize(synthDft, input.m_oscillatorInput);
        synthDft.InverseTransform(grain->m_buffer, DiscreteFourierTransform::x_maxComponents);
        grain->Start();
    }

    static constexpr size_t x_hopDenom = 4;
    const WaveTable* m_windowTable;
    float m_magnitudes[DiscreteFourierTransform::x_maxComponents];
    float m_analysisPhase[DiscreteFourierTransform::x_maxComponents];
    float m_analysisPhasePrev[DiscreteFourierTransform::x_maxComponents];
    double m_startTime;
    Oscillator m_oscillator;
};
