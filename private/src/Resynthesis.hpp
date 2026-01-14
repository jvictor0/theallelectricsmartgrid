#pragma once

#include "QuadUtils.hpp"
#include "AdaptiveWaveTable.hpp"
#include "Q.hpp"
#include "Slew.hpp"
#include "PriorityQueue.hpp"
#include "NormGen.hpp"
#include "Math.hpp"
#include "Array.hpp"

struct Resynthesizer
{
    static constexpr size_t x_bits = 12;
    static constexpr size_t x_hopDenom = 4;
    static constexpr size_t x_numOscillators = 3;
    typedef BasicWaveTableGeneric<x_bits> Buffer;
    typedef DiscreteFourierTransformGeneric<x_bits> DFT;
    static constexpr size_t x_tableSize = Buffer::x_tableSize;
    static constexpr size_t x_maxComponents = DFT::x_maxComponents;

    struct Grain
    {
        float Process()
        {                
            float value = m_buffer.m_table[m_index];
            ++m_index;
            if (m_index == x_tableSize)
            {
                m_running = false;
            }

            return value;
        }

        void Start()
        {
            m_index = 0;
            m_running = true;
            Math4096::Hann(m_buffer);
        }

        size_t m_index;
        Buffer m_buffer;
        bool m_running;
    };

    struct PVDR
    {
        struct Result
        {
            int16_t m_bin;
            int16_t m_parent;
            double m_delta;
            bool m_small;

            Result()
                : m_bin(0)
                , m_parent(0)
                , m_delta(0.0f)
                , m_small(false)
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
                float analysisMag = isPrev ? owner->m_prevAnalysisMagnitudes[bin] : owner->m_analysisMagnitudes[bin];
                m_magnitude = analysisMag;
            }

            bool operator<(const Entry& other) const
            {
                return m_magnitude < other.m_magnitude;
            }
        };

        void Analyze()
        {
            PriorityQueue<Entry, x_maxComponents * 2> queue;
            bool computed[x_maxComponents];
            float maxMag = 0.0;
            for (size_t i = 1; i < x_maxComponents; ++i)
            {
                maxMag = std::max(maxMag, m_owner->m_analysisMagnitudes[i]);
                maxMag = std::max(maxMag, m_owner->m_prevAnalysisMagnitudes[i]);
            }

            float tolerance = maxMag * 0.00000001;
            RGen rgen;
            for (size_t i = 1; i < x_maxComponents; ++i)
            {
                float mag = m_owner->m_analysisMagnitudes[i];
                if (mag < tolerance)
                {
                    computed[i] = true;
                    PushResult(i, i, rgen.UniGenRange(-0.5, 0.5), rgen, true);
                }
                else
                {
                    computed[i] = false;
                    queue.Push(Entry(i, true, m_owner));
                }
            }

            constexpr float N = static_cast<float>(x_tableSize);
            constexpr float H = N / static_cast<float>(x_hopDenom);

            while (!queue.IsEmpty())
            {
                Entry entry = queue.Pop();               

                if (entry.m_isPrev)
                {
                    if (!computed[entry.m_bin])
                    {
                        PushResult(entry.m_bin, entry.m_bin, m_owner->m_omegaInstantaneous[entry.m_bin] * H, rgen, false);
                        computed[entry.m_bin] = true;
                        queue.Push(Entry(entry.m_bin, false, m_owner));
                    }
                }
                else
                {
                    if (entry.m_bin + 1 < x_maxComponents && !computed[entry.m_bin + 1])
                    {
                        PushResult(
                            entry.m_bin + 1, 
                            entry.m_bin, 
                            m_owner->m_analysisPhase[entry.m_bin + 1] - m_owner->m_analysisPhase[entry.m_bin],
                            rgen,
                            false);
                        computed[entry.m_bin + 1] = true;
                        queue.Push(Entry(entry.m_bin + 1, false, m_owner));
                    }

                    if (0 < entry.m_bin - 1 && !computed[entry.m_bin - 1])
                    {
                        PushResult(
                            entry.m_bin - 1, 
                            entry.m_bin, 
                            m_owner->m_analysisPhase[entry.m_bin - 1] - m_owner->m_analysisPhase[entry.m_bin],
                            rgen,
                            false);
                        computed[entry.m_bin - 1] = true;
                        queue.Push(Entry(entry.m_bin - 1, false, m_owner));
                    }
                }
            }
        }

        void PushResult(int16_t bin, int16_t parent, double delta, RGen& rgen, bool small)
        {
            if (parent != bin)
            {
                uint16_t parentResultIdx = m_resultIndices[parent];
                if (m_results[parentResultIdx].m_bin != m_results[parentResultIdx].m_parent)
                {
                    delta = delta + m_results[parentResultIdx].m_delta;
                }

                parent = m_results[parentResultIdx].m_parent;
                float omegaInstantaneous = m_owner->m_omegaInstantaneous[parent];
                bool willAlias = Resynthesizer::WillAlias(omegaInstantaneous, bin);
                if (willAlias && false)
                {
                    delta = rgen.UniGenRange(-0.5, 0.5);
                    parent = bin;
                    small = true;
                }
            }

            m_results[m_size].m_bin = bin;
            m_results[m_size].m_parent = parent;
            m_results[m_size].m_delta = delta;
            m_results[m_size].m_small = small;
            m_resultIndices[bin] = m_size;
            ++m_size;
        }

        void AnalyzeNaive(Resynthesizer* owner)
        {
            constexpr float N = static_cast<float>(x_tableSize);
            constexpr float H = N / static_cast<float>(x_hopDenom);

            RGen rgen;

            for (size_t i = 1; i < x_maxComponents; ++i)
            {
                PushResult(i, i, owner->m_omegaInstantaneous[i] * H, rgen, false);
            }
        }

        bool IsLeader(int16_t bin)
        {
            return m_results[m_resultIndices[bin]].m_parent == bin && !m_results[m_resultIndices[bin]].m_small;
        }

        PVDR(Resynthesizer* owner)
            : m_size(0)
            , m_owner(owner)
        {
        }

        void Clear()
        {
            m_size = 0;
        }

        Result m_results[x_maxComponents];
        uint16_t m_resultIndices[x_maxComponents];
        size_t m_size;
        Resynthesizer* m_owner;
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
            for (size_t i = 0; i < x_maxComponents; ++i)
            {
                m_synthesisPhase[i] = 0.0f;
            }
        }

        void Clear()
        {
            for (size_t i = 0; i < x_maxComponents; ++i)
            {
                m_synthesisPhase[i] = 0.0f;
            }
        }

        void FixupPhases(float detune, PVDR* pvdr)
        {
            for (size_t i = 0; i < pvdr->m_size; ++i)
            {    
                PVDR::Result result = pvdr->m_results[i];  
                double delta = result.m_delta;
                if (result.m_bin != result.m_parent)
                {
                    m_owner->m_omegaInstantaneous[result.m_bin] = m_owner->m_omegaInstantaneous[result.m_parent];
                }
                else
                {
                    delta *= detune;
                }
                
                m_synthesisPhase[result.m_bin] = m_synthesisPhase[result.m_parent] + delta;
            }
        }

        void SynthesizeNoShift(DFT& dft, float gain)
        {
            for (size_t i = 1; i < x_maxComponents; ++i)
            {
                float mag = gain * m_owner->m_synthesisMagnitudes[i].m_output;
                dft.m_components[i] += Math::Polar2pi(mag, m_synthesisPhase[i]);
            }
        }

        void SynthesizeSingleShift(DFT& dft, float gain, Q shift, PVDR* pvdr)
        {
            if (shift.m_num == 1 && shift.m_denom == 1)
            {
                SynthesizeNoShift(dft, gain);
                return;
            }

            double shiftDouble = shift.ToDouble();
            double phases[x_maxComponents];
            int targetBins[x_maxComponents];
            for (size_t i = 0; i < pvdr->m_size; ++i)
            {
                PVDR::Result result = pvdr->m_results[i];
                if (result.m_small)
                {
                    continue;
                }
                
                if (result.m_bin == result.m_parent)
                {
                    targetBins[result.m_bin] = BinOmega(m_owner->m_omegaInstantaneous[result.m_bin] * shiftDouble);
                    phases[result.m_bin] = m_synthesisPhase[result.m_bin] * shiftDouble;    
                }
                else
                {
                    targetBins[result.m_bin] = targetBins[result.m_parent] - result.m_parent + result.m_bin;
                    phases[result.m_bin] = phases[result.m_parent] + result.m_delta;
                }

                if (0 < targetBins[result.m_bin] && 
                    targetBins[result.m_bin] < x_maxComponents &&
                    !WillAlias(m_owner->m_omegaInstantaneous[result.m_parent] * shiftDouble, targetBins[result.m_bin]))
                {
                    float mag = gain * m_owner->m_synthesisMagnitudes[result.m_bin].m_output;
                    std::complex<float> component = Math::Polar2pi(mag, phases[result.m_bin]);
                    dft.m_components[targetBins[result.m_bin]] += component;
                }
            }
        }

        static void ApplyShiftCoefficient(DFT& dft, double exactIndex, std::complex<float> coefficient)
        {
            size_t loIndex = static_cast<size_t>(exactIndex);
            size_t hiIndex = loIndex + 1;
            float hiFrac = static_cast<float>(exactIndex - loIndex);
            float loFrac = 1.0f - hiFrac;

            if (0 < loIndex && loIndex < x_maxComponents)
            {
                dft.m_components[loIndex] += coefficient * loFrac;
            }
            else if (loIndex == 0)
            {
                dft.m_components[hiIndex] += coefficient * loFrac;
            }

            if (0 < hiIndex && hiIndex < x_maxComponents)
            {
                dft.m_components[hiIndex] += coefficient * hiFrac;
            }
            else if (hiIndex == x_maxComponents)
            {
                dft.m_components[loIndex] += coefficient * hiFrac;
            }
        }

        void Synthesize(DFT& dft, Input input, PVDR* pvdr)
        {
            for (size_t i = 0; i < Oscillator::x_numShifts; ++i)
            {
                SynthesizeSingleShift(dft, input.m_gain[i], input.m_shift[i], pvdr);                
            }
        }

        double m_synthesisPhase[x_maxComponents];
        Resynthesizer* m_owner;
    };

    struct Input
    {
        Q m_shift[Oscillator::x_numShifts - 1];
        float m_fade[Oscillator::x_numShifts - 1];
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
            float fade0 = m_fade[0] * m_fade[0] * m_fade[0] * m_fade[0];
            float fade1 = m_fade[1] * m_fade[1] * m_fade[1] * m_fade[1];
            input.m_gain[0] = std::max(0.0f, 1 - fade0);
            input.m_gain[1] = std::max(0.0f, fade0 * (1 - fade1));
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
            : m_unisonGain(0.0)
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
        for (size_t i = 0; i < x_numOscillators; ++i)
        {
            m_oscillators[i].Clear();
        }

        for (size_t i = 0; i < x_maxComponents; ++i)
        {
            m_analysisPhase[i] = 0.0f;
            m_prevAnalysisPhase[i] = 0.0f;
            m_analysisMagnitudes[i] = 0.0f;
            m_prevAnalysisMagnitudes[i] = 0.0f;
            m_omegaInstantaneous[i] = 0.0f;
        }
    }

    Resynthesizer()
        : m_oscillators{ Oscillator(this), Oscillator(this), Oscillator(this) }
    {
        Clear();
    }

    void SetSlewUp(float cyclesPerSample)
    {
        float old = m_synthesisMagnitudes[0].m_slewUp.m_alpha;
        m_synthesisMagnitudes[0].SetSlewUp(cyclesPerSample);
        if (old != m_synthesisMagnitudes[0].m_slewUp.m_alpha)
        {
            for (size_t i = 1; i < x_maxComponents; ++i)
            {
                m_synthesisMagnitudes[i].m_slewUp.m_alpha = m_synthesisMagnitudes[0].m_slewUp.m_alpha;
            }
        }
    }

    void SetSlewDown(float cyclesPerSample)
    {
        float old = m_synthesisMagnitudes[0].m_slewDown.m_alpha;
        m_synthesisMagnitudes[0].SetSlewDown(cyclesPerSample);
        if (old != m_synthesisMagnitudes[0].m_slewDown.m_alpha)
        {
            for (size_t i = 1; i < x_maxComponents; ++i)
            {
                m_synthesisMagnitudes[i].m_slewDown.m_alpha = m_synthesisMagnitudes[0].m_slewDown.m_alpha;
            }
        }
    }

    void PrimePhases(DFT& dft)
    {
        for (size_t i = 1; i < x_maxComponents; ++i)
        {
            m_prevAnalysisPhase[i] = std::arg(dft.m_components[i]) / (2.0f * M_PI);
            m_prevAnalysisMagnitudes[i] = std::abs(dft.m_components[i]);
        }
    }

    void PrimeFromPrevious()
    {
        for (size_t i = 1; i < x_maxComponents; ++i)
        {
            m_prevAnalysisPhase[i] = m_analysisPhase[i];
            m_prevAnalysisMagnitudes[i] = m_analysisMagnitudes[i];
        }
    }

    void ProcessPhases(DFT& dft)
    {
        for (size_t i = 1; i < x_maxComponents; ++i)
        {
            m_analysisMagnitudes[i] = std::abs(dft.m_components[i]);            
            m_synthesisMagnitudes[i].m_output = m_analysisMagnitudes[i];
            m_analysisPhase[i] = std::arg(dft.m_components[i]) / (2.0f * M_PI);
            SetAnalysisOmega(i);
        }
    }

    static float OmegaBin(int bin)
    {
        return static_cast<float>(bin) / x_tableSize;
    }

    static int BinOmega(float omega)
    {
        return std::min<int>(std::max<int>(1, static_cast<int>(std::round(omega * x_tableSize))), x_tableSize - 1);
    }

    static bool WillAlias(float omegaInstantaneous, int bin)
    {
        bool result = 0.5f / x_H < std::abs(omegaInstantaneous - OmegaBin(bin));
        return result;
    }

    static double PrincArg(double arg)
    {
        arg = (arg - std::floor(arg));
        if (0.5f < arg)
        {
            arg -= 1.0f;
        }

        return arg;
    }

    static double LerpPhase(double phi1, double phi2, double alpha)
    {
        double diff = phi2 - phi1;
        diff = PrincArg(diff);
        return phi1 + alpha * diff;
    }

    void SetAnalysisOmega(int bin)
    {
        float omegaBin = OmegaBin(bin);
        float deltaPhiExpected = omegaBin * x_H;
        float deltaPhi = m_analysisPhase[bin] - m_prevAnalysisPhase[bin];
        
        deltaPhi -= deltaPhiExpected;
        deltaPhi = PrincArg(deltaPhi);
        
        float omegaAnalysis = omegaBin + deltaPhi / x_H;
        m_omegaInstantaneous[bin] = omegaAnalysis;
    }

    void SpectralDistortion(DFT& dft, Input& input)
    {
        float rmsTotal = 0.0f;
        float rmsQuiet = 1e-6f;
        float rmsLoud = 1e-6f;

        for (size_t i = 1; i < x_maxComponents; ++i)
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

        std::complex<float> shiftCoefficient[x_maxComponents];
        bool anyShift = false;
        for (size_t i = 1; i < x_maxComponents; ++i)
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
            for (size_t i = 1; i < x_maxComponents; ++i)
            {
                float exactIndex = static_cast<double>(i) * input.m_shift[0].ToDouble();
                Oscillator::ApplyShiftCoefficient(dft, exactIndex, shiftCoefficient[i]);
            }
        }
    }

    void Analyze(PVDR* pvdr, Buffer& buffer)
    {
        pvdr->Clear();
        DFT dft;
        dft.Transform(buffer);
        ProcessPhases(dft);
        pvdr->Analyze();
    }

    void PrimeAndAnalyze(PVDR* pvdr, Buffer& buffer)
    {
        PrimeFromPrevious();
        Analyze(pvdr, buffer);
    }

    void PrimeAnalysis(Buffer& waveTable)
    {
        DFT dft;
        dft.Transform(waveTable);
        PrimePhases(dft);
    }        

    void StartGrain(Grain* grain, Input& input)
    {        
        PVDR pvdr(this);

        SetSlewUp(input.m_slewUp);
        SetSlewDown(input.m_slewDown);

        Analyze(&pvdr, grain->m_buffer);
                
        for (size_t i = 0; i < x_numOscillators; ++i)
        {
            m_oscillators[i].FixupPhases(input.GetUnisonDetune(i), &pvdr);
        }

        DFT synthDft;
        for (size_t i = 0; i < x_numOscillators; ++i)
        {
            m_oscillators[i].Synthesize(synthDft, input.MakeOscillatorInput(i), &pvdr);        
        }

        //SpectralDistortion(synthDft, input);

        synthDft.InverseTransform(grain->m_buffer, x_maxComponents);
        grain->Start();
    }

    void Process(Buffer& previousWaveTable, Grain* grain, Input& input)
    {
        PrimeAnalysis(previousWaveTable);
        StartGrain(grain, input);
    }

    void PrintTopStuff()
    {
        double maxMag = 0.0;
        for (size_t i = 1; i < x_maxComponents; ++i)
        {
            maxMag = std::max<double>(maxMag, m_synthesisMagnitudes[i].m_output);
        }

        if (maxMag < 1e-4)
        {
            return;
        }

        INFO("========================");
        for (size_t i = 1; i < x_maxComponents; ++i)
        {
            if (m_synthesisMagnitudes[i].m_output > maxMag / 10)
            {
                INFO("%p High bin: %d, mag: %f, omega instantaneous: %f, an phase: %f, synth phase: %f", 
                    this, i, m_synthesisMagnitudes[i].m_output, 
                    m_omegaInstantaneous[i], m_analysisPhase[i], m_oscillators[0].m_synthesisPhase[i]);
            }
        }
    }

    static constexpr size_t GetGrainLaunchSamples()
    {
        return x_H;
    }

    static constexpr size_t x_N = x_tableSize;
    static constexpr size_t x_H = x_N / x_hopDenom;
    
    BiDirectionalSlew m_synthesisMagnitudes[x_maxComponents];
    float m_analysisMagnitudes[x_maxComponents];
    float m_prevAnalysisMagnitudes[x_maxComponents];
    float m_omegaInstantaneous[x_maxComponents];    
    float m_analysisPhase[x_maxComponents];
    float m_prevAnalysisPhase[x_maxComponents];
    Oscillator m_oscillators[x_numOscillators];
};

template <size_t Bits>
struct SpectralModelGeneric
{
    typedef BasicWaveTableGeneric<Bits> Buffer;
    typedef DiscreteFourierTransformGeneric<Bits> DFT;
    static constexpr size_t x_tableSize = Buffer::x_tableSize;
    static constexpr size_t x_maxComponents = DFT::x_maxComponents;
    static constexpr size_t x_hopDenom = 4;
    static constexpr size_t x_H = x_tableSize / x_hopDenom;
    static constexpr float x_deathMag = 1e-5f;
    static constexpr float x_mergeGainThreshold = 1e-3f;

    struct Input
    {
        float m_gainThreshold;
        size_t m_numAtoms;
        float m_slewUpAlpha;
        float m_slewDownAlpha;
        float m_omegaDensity;

        Input()
            : m_gainThreshold(0.001f)
            , m_numAtoms(64)
            , m_slewUpAlpha(1.0f)
            , m_slewDownAlpha(1.0f)
            , m_omegaDensity(1.0 / x_tableSize)
        {
        }
    };

    struct AnalysisAtom
    {
        float m_analysisOmega;
        float m_analysisMagnitude;
        
        AnalysisAtom()
            : m_analysisOmega(0.0f)
            , m_analysisMagnitude(0.0f)
        {
        }

        AnalysisAtom(float analysisOmega, float analysisMagnitude)
            : m_analysisOmega(analysisOmega)
            , m_analysisMagnitude(analysisMagnitude)
        {
        }

        static bool CmpReverseMagnitude(const AnalysisAtom& a, const AnalysisAtom& b)
        {
            return b.m_analysisMagnitude < a.m_analysisMagnitude;
        }

        static bool CmpOmega(const AnalysisAtom& a, const AnalysisAtom& b)
        {
            return a.m_analysisOmega < b.m_analysisOmega;
        }

        static bool CmpOmegaFloat(const AnalysisAtom& a, float omega)
        {
            return a.m_analysisOmega < omega;
        }
    };

    struct Atom : public AnalysisAtom
    {
        float m_synthesisOmega;
        float m_synthesisMagnitude;
        double m_synthesisPhase;

        Atom()
            : AnalysisAtom()
            , m_synthesisOmega(0.0f)
            , m_synthesisMagnitude(0.0f)
            , m_synthesisPhase(0.0f)
        {
        }

        Atom(float analysisOmega, float analysisMagnitude, float synthesisOmega, float synthesisMagnitude, float synthesisPhase)
            : AnalysisAtom(analysisOmega, analysisMagnitude)
            , m_synthesisOmega(synthesisOmega)
            , m_synthesisMagnitude(synthesisMagnitude)
            , m_synthesisPhase(synthesisPhase)
        {
        }

        void Merge(const AnalysisAtom& analysisAtom, Input& input)
        {
            m_synthesisMagnitude = BiDirectionalSlew::Process(m_synthesisMagnitude, analysisAtom.m_analysisMagnitude, input.m_slewUpAlpha, input.m_slewDownAlpha);
            AnalysisAtom::m_analysisOmega = analysisAtom.m_analysisOmega;
            AnalysisAtom::m_analysisMagnitude = analysisAtom.m_analysisMagnitude;
            m_synthesisOmega = analysisAtom.m_analysisOmega;
        }

        void MergeNoMatch(Input& input)
        {
            m_synthesisMagnitude = Slew::Process(m_synthesisMagnitude, 0.0, input.m_slewDownAlpha);
            AnalysisAtom::m_analysisMagnitude = 0.0;
            m_synthesisOmega = AnalysisAtom::m_analysisOmega;
        }

        void UpdatePhase()
        {
            m_synthesisPhase += x_H * m_synthesisOmega;
        }

        static bool CmpReverseMagnitude(const Atom& a, const Atom& b)
        {
            return AnalysisAtom::CmpReverseMagnitude(a, b);
        }
    };

    using AnalysisAtomArray = Array<AnalysisAtom, x_maxComponents>;
    using AtomArray = Array<Atom, x_maxComponents>;

    void ExtractAnalysisAtoms(DFT& dft, AnalysisAtomArray& analysisAtoms, const Input& input)
    {
        analysisAtoms.Clear();

        // Compute magnitudes
        //
        float mags[x_maxComponents];
        for (size_t i = 1; i < x_maxComponents; ++i)
        {
            mags[i] = std::abs(dft.m_components[i]);            
        }

        // Find local maxima and apply parabolic interpolation
        //
        constexpr float x_logEps = 1e-20f;

        for (int k = static_cast<int>(x_maxComponents) - 2; 2 <= k; --k)
        {
            float mag = mags[k];
            if (mags[k - 1] < mag && mags[k + 1] < mag && input.m_gainThreshold <= mag)
            {
                // Log-domain parabolic interpolation
                //
                float magLo = std::max(mags[k - 1], x_logEps);
                float magMid = std::max(mag, x_logEps);
                float magHi = std::max(mags[k + 1], x_logEps);

                float alpha = std::log(magLo);
                float beta = std::log(magMid);
                float gamma = std::log(magHi);
                float denom = alpha - 2.0f * beta + gamma;

                float p = 0.0f;
                float peakMag = mag;
                if (1e-10f < std::abs(denom))
                {
                    p = 0.5f * (alpha - gamma) / denom;
                    peakMag = std::exp(beta - 0.25f * (alpha - gamma) * p);
                }

                float peakOmega = (static_cast<float>(k) + p) / static_cast<float>(x_tableSize);
                analysisAtoms.Add(AnalysisAtom(peakOmega, peakMag));
            }
        }

        if (input.m_numAtoms < analysisAtoms.Size())
        {
            analysisAtoms.Sort(AnalysisAtom::CmpReverseMagnitude);
            analysisAtoms.ShrinkIfNecessary(input.m_numAtoms);
        }

        analysisAtoms.Sort(AnalysisAtom::CmpOmega);
    }

    void SearchAndMerge(AnalysisAtomArray& analysisAtoms, Atom& atom, Input& input)
    {
        float lowerOmega = atom.m_analysisOmega - input.m_omegaDensity;
        auto it = std::lower_bound(analysisAtoms.begin(), analysisAtoms.end(), lowerOmega, AnalysisAtom::CmpOmegaFloat);
        auto bestIt = it;
        for (; it != analysisAtoms.end(); ++it)
        {
            if (atom.m_analysisOmega + input.m_omegaDensity < it->m_analysisOmega)
            {
                break;
            }
            if (bestIt->m_analysisMagnitude < it->m_analysisMagnitude)
            {
                bestIt = it;
            }
        }

        if (bestIt == analysisAtoms.end())
        {
            atom.MergeNoMatch(input);
            return;
        }

        if (bestIt->m_analysisMagnitude / atom.m_synthesisMagnitude < x_mergeGainThreshold)
        {
            atom.MergeNoMatch(input);
        }
        else
        {
            atom.Merge(*bestIt, input);
        }

        for (auto forIt = bestIt; forIt != analysisAtoms.end(); ++forIt)
        {
            if (atom.m_analysisOmega + input.m_omegaDensity < forIt->m_analysisOmega)
            {
                break;
            }
            
            forIt->m_analysisMagnitude = -1.0f;
        }

        for (auto revIt = bestIt - 1; analysisAtoms.begin() <= revIt; --revIt)
        {
            if (revIt->m_analysisOmega < atom.m_analysisOmega - input.m_omegaDensity)
            {
                break;
            }

            revIt->m_analysisMagnitude = -1.0f;
        }
    }

    void ExtractAtoms(Buffer& buffer, Input& input)
    {
        DFT dft;
        dft.Transform(buffer);
        AnalysisAtomArray analysisAtoms;
        ExtractAnalysisAtoms(dft, analysisAtoms, input);
        m_atoms.Sort(Atom::CmpReverseMagnitude);
        for (size_t i = 0; i < m_atoms.Size(); ++i)
        {
            SearchAndMerge(analysisAtoms, m_atoms[i], input);
        }

        for (AnalysisAtom& analysisAtom : analysisAtoms)
        {
            if (x_deathMag <= analysisAtom.m_analysisMagnitude)
            {
                float initMag = std::max(Slew::Process(0, analysisAtom.m_analysisMagnitude, input.m_slewUpAlpha), x_deathMag);
                Atom newAtom(analysisAtom.m_analysisOmega, analysisAtom.m_analysisMagnitude, analysisAtom.m_analysisOmega, initMag, 0);
                m_atoms.Add(newAtom);
            }
        }

        m_atoms.Sort(Atom::CmpReverseMagnitude);
        m_atoms.ShrinkIfNecessary(input.m_numAtoms);
        while (!m_atoms.Empty() && m_atoms.Back().m_synthesisMagnitude < x_deathMag)
        {
            m_atoms.Pop();
        }
    }

    AtomArray m_atoms;
};

using SpectralModel = SpectralModelGeneric<12>;