#pragma once

#include "QuadUtils.hpp"
#include "AdaptiveWaveTable.hpp"
#include "Q.hpp"
#include "Slew.hpp"
#include "PriorityQueue.hpp"
#include "NormGen.hpp"

struct Resynthesizer
{
    static constexpr size_t x_hopDenom = 4;
    static constexpr size_t x_numOscillators = 3;

    struct Grain
    {
        float Process()
        {                
            float value = m_buffer.m_table[m_index] * (0.5 - 0.5 * m_windowTable->m_table[m_index]);
            ++m_index;
            if (m_index == BasicWaveTable::x_tableSize)
            {
                m_running = false;
            }

            return value;
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

    struct PVDR
    {
        struct Result
        {
            int16_t m_bin;
            int16_t m_parent;
            double m_delta;

            Result()
                : m_bin(0)
                , m_parent(0)
                , m_delta(0.0f)
            {
            }
        };

        struct Entry
        {
            int16_t m_bin;
            bool m_isPrev;
            float m_magnitude;

            Entry()
                : m_bin(0)
                , m_isPrev(false)
                , m_magnitude(0.0f)
            {
            }

            Entry(int16_t bin, bool isPrev, Resynthesizer* owner)
            {
                m_bin = bin;
                m_isPrev = isPrev;
                m_magnitude = isPrev ? owner->m_prevTrueMagnitudes[bin] : owner->m_trueMagnitudes[bin];
            }

            bool operator<(const Entry& other) const
            {
                return m_magnitude < other.m_magnitude;
            }
        };

        void Analyze(Resynthesizer* owner)
        {
            PriorityQueue<Entry, DiscreteFourierTransform::x_maxComponents * 2> queue;
            bool computed[DiscreteFourierTransform::x_maxComponents];
            float maxMag = 0.0;
            for (size_t i = 1; i < DiscreteFourierTransform::x_maxComponents; ++i)
            {
                maxMag = std::max(maxMag, owner->m_trueMagnitudes[i]);
                maxMag = std::max(maxMag, owner->m_prevTrueMagnitudes[i]);
            }

            float tolerance = maxMag * 0.001;
            RGen rgen;
            for (size_t i = 1; i < DiscreteFourierTransform::x_maxComponents; ++i)
            {
                queue.Push(Entry(i, true, owner));
                float mag = owner->m_trueMagnitudes[i];
                if (mag < tolerance)
                {
                    computed[i] = true;
                    m_results[m_size].m_bin = i;
                    m_results[m_size].m_parent = i;
                    m_results[m_size].m_delta = rgen.UniGenRange(-M_PI, M_PI);
                    ++m_size;
                }
                else
                {
                    computed[i] = false;
                    queue.Push(Entry(i, true, owner));
                }
            }

            constexpr float N = static_cast<float>(BasicWaveTable::x_tableSize);
            constexpr float H = N / static_cast<float>(x_hopDenom);

            while (!queue.IsEmpty())
            {
                Entry entry = queue.Pop();               

                if (entry.m_isPrev)
                {
                    if (!computed[entry.m_bin])
                    {
                        m_results[m_size].m_bin = entry.m_bin;
                        m_results[m_size].m_parent = entry.m_bin;
                        m_results[m_size].m_delta = owner->m_omegaInstantaneous[entry.m_bin] * H;
                        ++m_size;
                        computed[entry.m_bin] = true;
                        queue.Push(Entry(entry.m_bin, false, owner));
                    }
                }
                else
                {
                    if (entry.m_bin + 1 < DiscreteFourierTransform::x_maxComponents && !computed[entry.m_bin + 1])
                    {
                        m_results[m_size].m_bin = entry.m_bin + 1;
                        m_results[m_size].m_parent = entry.m_bin;
                        //m_results[m_size].m_delta = (owner->m_phaseDerivative[entry.m_bin + 1] + owner->m_phaseDerivative[entry.m_bin]) / 2.0f;
                        m_results[m_size].m_delta = owner->m_analysisPhase[entry.m_bin + 1] - owner->m_analysisPhase[entry.m_bin];
                        ++m_size;
                        computed[entry.m_bin + 1] = true;
                        queue.Push(Entry(entry.m_bin + 1, false, owner));
                    }

                    if (0 < entry.m_bin - 1 && !computed[entry.m_bin - 1])
                    {
                        m_results[m_size].m_bin = entry.m_bin - 1;
                        m_results[m_size].m_parent = entry.m_bin;
                        //m_results[m_size].m_delta = - (owner->m_phaseDerivative[entry.m_bin] + owner->m_phaseDerivative[entry.m_bin - 1]) / 2.0f;
                        m_results[m_size].m_delta = owner->m_analysisPhase[entry.m_bin] - owner->m_analysisPhase[entry.m_bin - 1];
                        ++m_size;
                        computed[entry.m_bin - 1] = true;
                        queue.Push(Entry(entry.m_bin - 1, false, owner));
                    }
                }
            }
        }

        PVDR()
            : m_size(0)
        {
        }

        Result m_results[DiscreteFourierTransform::x_maxComponents];
        size_t m_size;
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

        void FixupPhases(float detune, PVDR* pvdr)
        {
            for (size_t i = 0; i < pvdr->m_size; ++i)
            {    
                PVDR::Result entry = pvdr->m_results[i];                
                double delta = entry.m_bin == entry.m_parent ? entry.m_delta * detune : entry.m_delta;
                m_synthesisPhase[entry.m_bin] = m_synthesisPhase[entry.m_parent] + delta;
            }
        }

        void SynthesizeSingleShift(DiscreteFourierTransform& dft, float gain, Q shift)
        {
            dft.m_components[0] = std::complex<float>(0, 0);
            for (size_t i = 1; i < DiscreteFourierTransform::x_maxComponents; ++i)
            {
                float mag = gain * m_owner->m_magnitudes[i].m_output;
                double phase = m_synthesisPhase[i] * shift.ToDouble();
                std::complex<float> component = m_owner->Polar(mag, phase);

                double exactIndex = static_cast<double>(i) * shift.ToDouble();
                ApplyShiftCoefficient(dft, exactIndex, component);
            }
        }

        static void ApplyShiftCoefficient(DiscreteFourierTransform& dft, double exactIndex, std::complex<float> coefficient)
        {
            size_t loIndex = static_cast<size_t>(exactIndex);
            size_t hiIndex = loIndex + 1;
            float hiFrac = static_cast<float>(exactIndex - loIndex);
            float loFrac = 1.0f - hiFrac;

            if (0 < loIndex && loIndex < DiscreteFourierTransform::x_maxComponents)
            {
                dft.m_components[loIndex] += coefficient * loFrac;
            }
            else if (loIndex == 0)
            {
                dft.m_components[hiIndex] += coefficient * loFrac;
            }

            if (0 < hiIndex && hiIndex < DiscreteFourierTransform::x_maxComponents)
            {
                dft.m_components[hiIndex] += coefficient * hiFrac;
            }
            else if (hiIndex == DiscreteFourierTransform::x_maxComponents)
            {
                dft.m_components[loIndex] += coefficient * hiFrac;
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

    std::complex<float> Polar(float mag, double phase)
    {
        float cosArg = phase / (2 * M_PI);
        float sinArg = cosArg + 0.75;
        cosArg = cosArg - std::floor(cosArg);
        sinArg = sinArg - std::floor(sinArg);
        return std::complex<float>(
            mag * m_cosTable->Evaluate(cosArg),
            mag * m_cosTable->Evaluate(sinArg));
    }

    struct Input
    {
        Q m_shift[Oscillator::x_numShifts - 1];
        float m_fade[Oscillator::x_numShifts - 1];
        double m_startTime;
        float m_unisonGain;
        float m_unisonDetune;
        float m_slewUp;
        float m_slewDown;
        float m_rmsThreshold;
        float m_rmsQuiet;
        float m_rmsLoud;
        float m_loudShift;

        float GetUnisonGainForOscillator(int index)
        {
            if (index == 0)
            {
                return std::sqrt(1.0f - 2.0f * m_unisonGain * m_unisonGain / 3.0f);
            }
            else
            {
                return m_unisonGain / std::sqrt(3.0f);
            }
        }

        Oscillator::Input MakeOscillatorInput(int index)
        {
            float unisonGain = GetUnisonGainForOscillator(index);
            Oscillator::Input input;
            input.m_gain[0] = std::max(0.0f, 1 - m_fade[0]);
            input.m_gain[1] = std::max(0.0f, m_fade[0] * (1 - m_fade[1]));
            input.m_gain[2] = std::max(0.0f, (1 - input.m_gain[0] - input.m_gain[1]));
            for (size_t i = 0; i < 3; ++i)
            {
                input.m_gain[i] *= unisonGain;
            }

            input.m_shift[0] = Q(1, 1);
            input.m_shift[1] = m_shift[0];
            input.m_shift[2] = m_shift[1];
            return input;
        }

        float GetUnisonDetune(int index)
        {
            if (index == 0)
            {
                return 1.0;
            }
            else if (index == 1)
            {
                return m_unisonDetune;
            }
            else
            {
                return 1.0 / m_unisonDetune;
            }
        }

        Input()
            : m_startTime(0.0)
            , m_unisonGain(0.0)
            , m_unisonDetune(1.0)
            , m_slewUp(0.5)
            , m_slewDown(0.5)
            , m_rmsThreshold(1.0)
            , m_rmsQuiet(0)
            , m_rmsLoud(1)
            , m_loudShift(0.0)
        {
            for (size_t i = 0; i < Oscillator::x_numShifts - 1; ++i)
            {
                m_shift[i] = Q(1, 1);
                m_fade[i] = 0.0f;
            }
        }
    };

    void Clear()
    {
        m_startTime = 0.0;
        for (size_t i = 0; i < x_numOscillators; ++i)
        {
            m_oscillators[i].Clear();
        }

        for (size_t i = 0; i < DiscreteFourierTransform::x_maxComponents; ++i)
        {
            m_analysisPhase[i] = 0.0f;
            m_analysisPhasePrev[i] = 0.0f;
            m_trueMagnitudes[i] = 0.0f;
            m_prevTrueMagnitudes[i] = 0.0f;
            m_omegaInstantaneous[i] = 0.0f;
            m_phaseDerivative[i] = 0.0f;
        }
    }

    Resynthesizer()
        : m_oscillators{ Oscillator(this), Oscillator(this), Oscillator(this) }
    {
        Clear();
        m_cosTable = &WaveTable::GetCosine();
    }

    void SetSlewUp(float cyclesPerSample)
    {
        float old = m_magnitudes[0].m_slewUp.m_alpha;
        m_magnitudes[0].SetSlewUp(cyclesPerSample);
        if (old != m_magnitudes[0].m_slewUp.m_alpha)
        {
            for (size_t i = 1; i < DiscreteFourierTransform::x_maxComponents; ++i)
            {
                m_magnitudes[i].m_slewUp.m_alpha = m_magnitudes[0].m_slewUp.m_alpha;
            }
        }
    }

    void SetSlewDown(float cyclesPerSample)
    {
        float old = m_magnitudes[0].m_slewDown.m_alpha;
        m_magnitudes[0].SetSlewDown(cyclesPerSample);
        if (old != m_magnitudes[0].m_slewDown.m_alpha)
        {
            for (size_t i = 1; i < DiscreteFourierTransform::x_maxComponents; ++i)
            {
                m_magnitudes[i].m_slewDown.m_alpha = m_magnitudes[0].m_slewDown.m_alpha;
            }
        }
    }

    void ProcessPhases(DiscreteFourierTransform& dft, double deltaTime)
    {
        for (size_t i = 1; i < DiscreteFourierTransform::x_maxComponents; ++i)
        {
            m_analysisPhasePrev[i] = m_analysisPhase[i];
            m_analysisPhase[i] = std::arg(dft.m_components[i]);
            m_prevTrueMagnitudes[i] = m_trueMagnitudes[i];
            m_trueMagnitudes[i] = std::abs(dft.m_components[i]);
            //m_magnitudes[i].Process(m_trueMagnitudes[i]);
            m_magnitudes[i].m_output = m_trueMagnitudes[i];
            SetAnalysisOmega(i, deltaTime);
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

    float PrincArg(float arg)
    {
        constexpr float twoPi = 2.0f * static_cast<float>(M_PI);
        arg /= twoPi;
        arg = (arg - std::floor(arg)) * twoPi;
        if (M_PI < arg)
        {
            arg -= 2 * M_PI;
        }

        return arg;
    }

    void SetAnalysisOmega(int bin, double deltaTime)
    {
        if (std::abs(deltaTime) < 1e-12)
        {
            return;
        }

        float omegaBin = OmegaBin(bin);
        float deltaPhiExpected = omegaBin * deltaTime;
        float deltaPhi = DeltaPhi(bin);
        
        deltaPhi -= deltaPhiExpected;
        deltaPhi = PrincArg(deltaPhi);
        
        float omegaAnalysis = omegaBin + deltaPhi / deltaTime;
        float t = std::min(1.0f, m_trueMagnitudes[bin] / m_magnitudes[bin].m_output);
        m_omegaInstantaneous[bin] = omegaAnalysis * t + (1.0f - t) * m_omegaInstantaneous[bin];        

        float forwardPhaseDerivative = bin == DiscreteFourierTransform::x_maxComponents - 1 
                                     ? 0
                                     : PrincArg(m_analysisPhase[bin + 1] - m_analysisPhase[bin]);
        float backwardPhaseDerivative = bin == 1
                                     ? 0
                                     : PrincArg(m_analysisPhase[bin] - m_analysisPhase[bin - 1]);
        m_phaseDerivative[bin] = (forwardPhaseDerivative + backwardPhaseDerivative) / 2.0f;
    }

    void SpectralDistortion(DiscreteFourierTransform& dft, Input& input)
    {
        float rmsTotal = 0.0f;
        float rmsQuiet = 1e-6f;
        float rmsLoud = 1e-6f;

        for (size_t i = 1; i < DiscreteFourierTransform::x_maxComponents; ++i)
        {
            float rms = dft.m_components[i].real() * dft.m_components[i].real() + dft.m_components[i].imag() * dft.m_components[i].imag();
            if (rms < input.m_rmsQuiet * input.m_rmsThreshold)
            {
                rmsQuiet += rms;
            }
            
            if (input.m_rmsLoud * input.m_rmsThreshold < rms)
            {
                rmsLoud += rms - input.m_rmsLoud * input.m_rmsThreshold;
            }

            rmsTotal += rms;
        }

        if (rmsTotal < input.m_rmsThreshold)
        {
            return;
        }

        float quietReduction = std::max(0.0f, std::sqrt((input.m_rmsThreshold - rmsTotal + rmsQuiet) / rmsQuiet));
        rmsTotal = rmsTotal + (quietReduction * quietReduction - 1.0f) * rmsQuiet;

        float loudReduction = 1.0;
        if (input.m_rmsThreshold < rmsTotal)
        {
            loudReduction = std::max(0.0f, (input.m_rmsThreshold - rmsTotal + rmsLoud) / rmsLoud);
        }

        std::complex<float> shiftCoefficient[DiscreteFourierTransform::x_maxComponents];
        bool anyShift = false;
        for (size_t i = 1; i < DiscreteFourierTransform::x_maxComponents; ++i)
        {
            float rms = dft.m_components[i].real() * dft.m_components[i].real() + dft.m_components[i].imag() * dft.m_components[i].imag();
            shiftCoefficient[i] = std::complex<float>(0, 0);
            if (rms < input.m_rmsQuiet * input.m_rmsThreshold)
            {
                dft.m_components[i] *= quietReduction;
            }           
            else if (loudReduction < 1 && input.m_rmsLoud * input.m_rmsThreshold < rms)
            {
                float thresh = input.m_rmsLoud * input.m_rmsThreshold;
                float factor = std::sqrt((thresh + (rms - thresh) * loudReduction) / rms);
                if (0 < input.m_loudShift)
                {
                    anyShift = true;
                    float mag = std::sqrt(rms);
                    float shiftMag = mag * (1 - factor) * input.m_loudShift;
                    shiftCoefficient[i] = std::pow(dft.m_components[i] / mag, input.m_shift[0].ToDouble());
                    shiftCoefficient[i] *= std::complex<float>(shiftMag, 0);
                }

                dft.m_components[i] *= factor;
            }
        }

        if (anyShift)
        {
            for (size_t i = 1; i < DiscreteFourierTransform::x_maxComponents; ++i)
            {
                float exactIndex = static_cast<double>(i) * input.m_shift[0].ToDouble();
                Oscillator::ApplyShiftCoefficient(dft, exactIndex, shiftCoefficient[i]);
            }
        }
    }

    void StartGrain(Grain* grain, Input& input)
    {
        DiscreteFourierTransform dft;

        double deltaTime = input.m_startTime - m_startTime;
        m_startTime = input.m_startTime;

        SetSlewUp(input.m_slewUp);
        SetSlewDown(input.m_slewDown);

        dft.Transform(grain->m_buffer);
        ProcessPhases(dft, deltaTime);
        
        PVDR pvdr;
        pvdr.Analyze(this);
                
        for (size_t i = 0; i < x_numOscillators; ++i)
        {
            m_oscillators[i].FixupPhases(input.GetUnisonDetune(i), &pvdr);
        }

        DiscreteFourierTransform synthDft;
        for (size_t i = 0; i < x_numOscillators; ++i)
        {
            m_oscillators[i].Synthesize(synthDft, input.MakeOscillatorInput(i));        
        }

        //SpectralDistortion(synthDft, input);

        synthDft.InverseTransform(grain->m_buffer, DiscreteFourierTransform::x_maxComponents);
        grain->Start();
    }

    static constexpr size_t GetGrainLaunchSamples()
    {
        return BasicWaveTable::x_tableSize / x_hopDenom;
    }

    const WaveTable* m_windowTable;
    BiDirectionalSlew m_magnitudes[DiscreteFourierTransform::x_maxComponents];
    float m_trueMagnitudes[DiscreteFourierTransform::x_maxComponents];
    float m_prevTrueMagnitudes[DiscreteFourierTransform::x_maxComponents];
    float m_omegaInstantaneous[DiscreteFourierTransform::x_maxComponents];
    float m_phaseDerivative[DiscreteFourierTransform::x_maxComponents];
    float m_analysisPhase[DiscreteFourierTransform::x_maxComponents];
    float m_analysisPhasePrev[DiscreteFourierTransform::x_maxComponents];
    double m_startTime;
    Oscillator m_oscillators[x_numOscillators];
    const WaveTable* m_cosTable;
};
