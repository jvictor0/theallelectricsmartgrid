#pragma once

#include "QuadUtils.hpp"
#include "AdaptiveWaveTable.hpp"
#include "Q.hpp"
#include "Slew.hpp"
#include "PriorityQueue.hpp"
#include "NormGen.hpp"
#include "Math.hpp"
#include "Array.hpp"
#include "MultiChannelComplex.hpp"
#include "SampleTimer.hpp"
#include "SpectralModel.hpp"

// Controls for one oscillator of one channel.
//
struct ResynthesizerOscillatorInput
{
    static constexpr size_t x_numShifts = 2;

    float m_gain[x_numShifts];
    Q m_shift[x_numShifts];

    ResynthesizerOscillatorInput()
    {
        for (size_t i = 0; i < x_numShifts; ++i)
        {
            m_gain[i] = i == 0 ? 1.0f : 0.0f;
            m_shift[i] = Q(1, 1);
        }
    }
};

// Controls for one channel. Every channel count shares this type, and a
// multichannel resynthesizer takes one per channel.
//
struct ResynthesizerInput
{
    static constexpr float x_immediateSlewUp = 0.5f;
    static constexpr size_t x_numShifts = ResynthesizerOscillatorInput::x_numShifts;

    Q m_shift[x_numShifts - 1];
    float m_fade[x_numShifts - 1];
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

    ResynthesizerOscillatorInput MakeOscillatorInput(int index)
    {
        float unisonGain = GetUnisonGainForOscillator(index);
        ResynthesizerOscillatorInput input;
        float fade0 = m_fade[0] * m_fade[0] * m_fade[0] * m_fade[0];
        input.m_gain[0] = std::max(0.0f, 1 - fade0);
        input.m_gain[1] = std::max(0.0f, fade0);
        for (size_t i = 0; i < x_numShifts; ++i)
        {
            input.m_gain[i] *= unisonGain;
        }

        input.m_shift[0] = Q(1, 1);
        input.m_shift[1] = m_shift[0];
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

    ResynthesizerInput()
        : m_unisonGain(0.0)
        , m_unisonDetune(1.0)
        , m_slewUp(x_immediateSlewUp)
        , m_slewDown(0.5)
        , m_rmsThreshold(1.0)
        , m_rmsQuiet(0)
        , m_rmsLoud(1)
        , m_loudShift(0.0)
    {
        for (size_t i = 0; i < x_numShifts - 1; ++i)
        {
            m_shift[i] = Q(1, 1);
            m_fade[i] = 0.0f;
        }
    }
};

// Phase vocoder resynthesis of Size independent channels. Transforms and per-bin
// analysis run across channels at once; phase propagation order and shifted
// synthesis depend on each channel's spectrum and run per channel.
//
template<typename Number, size_t Size>
struct ResynthesizerGeneric
{
    using Vector = MultiChannelNumber<Number, Size>;
    using DoubleVector = MultiChannelNumber<double, Size>;
    using Input = ResynthesizerInput;

    static constexpr size_t x_bits = 12;
    static constexpr size_t x_hopDenom = 4;
    static constexpr size_t x_numOscillators = 3;
    static constexpr float x_immediateSlewUp = ResynthesizerInput::x_immediateSlewUp;
    using Buffer = BasicWaveTableGeneric<Number, Size, x_bits>;
    using DFT = DiscreteFourierTransformGeneric<Number, Size, x_bits>;
    using ComplexVector = typename DFT::ComplexVector;
    static constexpr size_t x_tableSize = Buffer::x_tableSize;
    static constexpr size_t x_maxComponents = DFT::x_maxComponents;
    static constexpr size_t x_N = x_tableSize;
    static constexpr size_t x_H = x_N / x_hopDenom;

    // Analysis runs on host-rate buffers. Synthesis interpolates the same
    // spectrum to the engine rate, so a frame covers the analysis window's
    // duration in engine samples.
    //
    static constexpr size_t x_synthesisBits = x_bits + SampleTimer::x_oversampleBits;
    using SynthesisFrame = BasicWaveTableGeneric<Number, Size, x_synthesisBits>;
    static constexpr size_t x_synthesisFrameSize = SynthesisFrame::x_tableSize;

    // Phase propagation order for one channel.
    //
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
            Number m_magnitude;

            Entry()
                : m_bin(0)
                , m_isPrev(false)
                , m_magnitude(0.0f)
            {
            }

            Entry(int16_t bin, bool isPrev, Number magnitude)
                : m_bin(bin)
                , m_isPrev(isPrev)
                , m_magnitude(magnitude)
            {
            }

            bool operator<(const Entry& other) const
            {
                return m_magnitude < other.m_magnitude;
            }
        };

        Entry MakeEntry(int16_t bin, bool isPrev) const
        {
            Number magnitude = isPrev ? m_owner->m_prevAnalysisMagnitudes[bin][m_channel] : m_owner->m_analysisMagnitudes[bin][m_channel];
            return Entry(bin, isPrev, magnitude);
        }

        void Analyze()
        {
            PriorityQueue<Entry, x_maxComponents * 2> queue;
            bool computed[x_maxComponents];
            Number maxMag = 0.0;
            for (size_t i = 1; i < x_maxComponents; ++i)
            {
                maxMag = std::max(maxMag, m_owner->m_analysisMagnitudes[i][m_channel]);
                maxMag = std::max(maxMag, m_owner->m_prevAnalysisMagnitudes[i][m_channel]);
            }

            Number tolerance = maxMag * 0.00000001;
            RGen rgen;
            for (size_t i = 1; i < x_maxComponents; ++i)
            {
                Number mag = m_owner->m_analysisMagnitudes[i][m_channel];
                if (mag < tolerance)
                {
                    computed[i] = true;
                    PushResult(i, i, rgen.UniGenRange(-0.5, 0.5), rgen, true);
                }
                else
                {
                    computed[i] = false;
                    queue.Push(MakeEntry(i, true));
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
                        PushResult(entry.m_bin, entry.m_bin, m_owner->m_omegaInstantaneous[entry.m_bin][m_channel] * H, rgen, false);
                        computed[entry.m_bin] = true;
                        queue.Push(MakeEntry(entry.m_bin, false));
                    }
                }
                else
                {
                    if (entry.m_bin + 1 < x_maxComponents && !computed[entry.m_bin + 1])
                    {
                        PushResult(
                            entry.m_bin + 1, 
                            entry.m_bin, 
                            m_owner->m_analysisPhase[entry.m_bin + 1][m_channel] - m_owner->m_analysisPhase[entry.m_bin][m_channel],
                            rgen,
                            false);
                        computed[entry.m_bin + 1] = true;
                        queue.Push(MakeEntry(entry.m_bin + 1, false));
                    }

                    if (0 < entry.m_bin - 1 && !computed[entry.m_bin - 1])
                    {
                        PushResult(
                            entry.m_bin - 1, 
                            entry.m_bin, 
                            m_owner->m_analysisPhase[entry.m_bin - 1][m_channel] - m_owner->m_analysisPhase[entry.m_bin][m_channel],
                            rgen,
                            false);
                        computed[entry.m_bin - 1] = true;
                        queue.Push(MakeEntry(entry.m_bin - 1, false));
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
                double omegaInstantaneous = m_owner->m_omegaInstantaneous[parent][m_channel];
                bool willAlias = ResynthesizerGeneric::WillAlias(omegaInstantaneous, bin);
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

        void AnalyzeNaive()
        {
            constexpr float N = static_cast<float>(x_tableSize);
            constexpr float H = N / static_cast<float>(x_hopDenom);

            RGen rgen;

            for (size_t i = 1; i < x_maxComponents; ++i)
            {
                PushResult(i, i, m_owner->m_omegaInstantaneous[i][m_channel] * H, rgen, false);
            }
        }

        bool IsLeader(int16_t bin)
        {
            return m_results[m_resultIndices[bin]].m_parent == bin && !m_results[m_resultIndices[bin]].m_small;
        }

        PVDR()
            : PVDR(nullptr, 0)
        {
        }

        PVDR(ResynthesizerGeneric* owner, size_t channel)
            : m_size(0)
            , m_owner(owner)
            , m_channel(channel)
        {
        }

        void Clear()
        {
            m_size = 0;
        }

        Result m_results[x_maxComponents];
        uint16_t m_resultIndices[x_maxComponents];
        size_t m_size;
        ResynthesizerGeneric* m_owner;
        size_t m_channel;
    };

    struct Oscillator
    {
        using Input = ResynthesizerOscillatorInput;
        static constexpr size_t x_numShifts = Input::x_numShifts;

        Oscillator(ResynthesizerGeneric* owner)
            : m_owner(owner)
        {
            Clear();
        }

        void Clear()
        {
            for (size_t i = 0; i < x_maxComponents; ++i)
            {
                m_synthesisPhase[i] = DoubleVector::Splat(0);
            }
        }

        // Each channel follows its own propagation order, one PVDR per channel.
        //
        void FixupPhases(const Vector& detune, PVDR* pvdr)
        {
            for (size_t channel = 0; channel < Size; ++channel)
            {
                for (size_t i = 0; i < pvdr[channel].m_size; ++i)
                {    
                    typename PVDR::Result result = pvdr[channel].m_results[i];  
                    double delta = result.m_delta;
                    if (result.m_bin != result.m_parent)
                    {
                        m_owner->m_omegaInstantaneous[result.m_bin][channel] = m_owner->m_omegaInstantaneous[result.m_parent][channel];
                    }
                    else
                    {
                        delta *= detune[channel];
                    }
                    
                    m_synthesisPhase[result.m_bin][channel] = m_synthesisPhase[result.m_parent][channel] + delta;
                }
            }
        }

        void SynthesizeNoShift(DFT& dft, const Vector& gain)
        {
            for (size_t i = 1; i < x_maxComponents; ++i)
            {
                Vector mag = gain * m_owner->m_synthesisMagnitudes[i];
                dft.m_components[i] += Math::Polar2pi(mag, m_synthesisPhase[i]);
            }
        }

        void SynthesizeShiftedChannel(DFT& dft, size_t channel, Number gain, Q shift, PVDR& pvdr)
        {
            double shiftDouble = shift.ToDouble();
            double phases[x_maxComponents];
            int targetBins[x_maxComponents];
            for (size_t i = 0; i < pvdr.m_size; ++i)
            {
                typename PVDR::Result result = pvdr.m_results[i];
                if (result.m_small)
                {
                    continue;
                }
                
                if (result.m_bin == result.m_parent)
                {
                    targetBins[result.m_bin] = BinOmega(m_owner->m_omegaInstantaneous[result.m_bin][channel] * shiftDouble);
                    phases[result.m_bin] = m_synthesisPhase[result.m_bin][channel] * shiftDouble;    
                }
                else
                {
                    targetBins[result.m_bin] = targetBins[result.m_parent] - result.m_parent + result.m_bin;
                    phases[result.m_bin] = phases[result.m_parent] + result.m_delta;
                }

                if (0 < targetBins[result.m_bin] && 
                    targetBins[result.m_bin] < x_maxComponents &&
                    !WillAlias(m_owner->m_omegaInstantaneous[result.m_parent][channel] * shiftDouble, targetBins[result.m_bin]))
                {
                    Number mag = gain * m_owner->m_synthesisMagnitudes[result.m_bin][channel];
                    std::complex<float> component = Math::Polar2pi(mag, phases[result.m_bin]);
                    ComplexVector& target = dft.m_components[targetBins[result.m_bin]];
                    target.m_real[channel] += component.real();
                    target.m_imag[channel] += component.imag();
                }
            }
        }

        static bool IsUnity(Q shift)
        {
            return shift.m_num == 1 && shift.m_denom == 1;
        }

        // Unshifted channels synthesize together. Shifted channels scatter into
        // their own target bins.
        //
        void SynthesizeSingleShift(DFT& dft, const Vector& gain, const Q* shift, PVDR* pvdr)
        {
            Vector unshiftedGain = Vector::Splat(0);
            bool anyUnshifted = false;
            for (size_t channel = 0; channel < Size; ++channel)
            {
                if (IsUnity(shift[channel]))
                {
                    unshiftedGain[channel] = gain[channel];
                    anyUnshifted = true;
                }
            }

            if (anyUnshifted)
            {
                SynthesizeNoShift(dft, unshiftedGain);
            }

            for (size_t channel = 0; channel < Size; ++channel)
            {
                if (!IsUnity(shift[channel]))
                {
                    SynthesizeShiftedChannel(dft, channel, gain[channel], shift[channel], pvdr[channel]);
                }
            }
        }

        static void ApplyShiftCoefficient(DFT& dft, double exactIndex, std::complex<float> coefficient)
        {
            static_assert(Size == 1, "ApplyShiftCoefficient is mono only");
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

        // One input per channel.
        //
        void Synthesize(DFT& dft, const Input* input, PVDR* pvdr)
        {
            for (size_t i = 0; i < x_numShifts; ++i)
            {
                Vector gain;
                Q shift[Size];
                for (size_t channel = 0; channel < Size; ++channel)
                {
                    gain[channel] = input[channel].m_gain[i];
                    shift[channel] = input[channel].m_shift[i];
                }

                SynthesizeSingleShift(dft, gain, shift, pvdr);
            }
        }

        DoubleVector m_synthesisPhase[x_maxComponents];
        ResynthesizerGeneric* m_owner;
    };

    void Clear()
    {
        for (size_t i = 0; i < x_numOscillators; ++i)
        {
            m_oscillators[i].Clear();
        }

        for (size_t i = 0; i < x_maxComponents; ++i)
        {
            m_analysisPhase[i] = Vector::Splat(0);
            m_prevAnalysisPhase[i] = Vector::Splat(0);
            m_analysisMagnitudes[i] = Vector::Splat(0);
            m_prevAnalysisMagnitudes[i] = Vector::Splat(0);
            m_omegaInstantaneous[i] = DoubleVector::Splat(0);
        }
    }

    ResynthesizerGeneric()
        : m_oscillators{ Oscillator(this), Oscillator(this), Oscillator(this) }
    {
        m_slewUpAlpha = Vector::Splat(0);
        for (size_t i = 0; i < x_maxComponents; ++i)
        {
            m_synthesisMagnitudes[i] = Vector::Splat(0);
        }

        for (size_t channel = 0; channel < Size; ++channel)
        {
            m_pvdr[channel].m_owner = this;
            m_pvdr[channel].m_channel = channel;
        }

        Clear();
    }

    ResynthesizerGeneric(const ResynthesizerGeneric&) = delete;
    ResynthesizerGeneric& operator=(const ResynthesizerGeneric&) = delete;

    void SetSlewUp(const Vector& cyclesPerSample)
    {
        Vector alpha = OPLowPassFilterGeneric<Number, Size>::AlphaFromNatFreq(cyclesPerSample);
        for (size_t channel = 0; channel < Size; ++channel)
        {
            m_slewUpAlpha[channel] = cyclesPerSample[channel] >= x_immediateSlewUp ? static_cast<Number>(1) : alpha[channel];
        }
    }

    // Analysis phase in cycles. Divides in double, as std::arg / (2.0f * M_PI)
    // did.
    //
    static Vector AnalysisPhase(const ComplexVector& component)
    {
        return (component.Arg().template Convert<double>() / (2.0 * M_PI)).template Convert<Number>();
    }

    void PrimePhases(DFT& dft)
    {
        for (size_t i = 1; i < x_maxComponents; ++i)
        {
            m_prevAnalysisPhase[i] = AnalysisPhase(dft.m_components[i]);
            m_prevAnalysisMagnitudes[i] = dft.m_components[i].Abs();
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
            m_analysisMagnitudes[i] = dft.m_components[i].Abs();
            const Vector& source = m_synthesisMagnitudes[i];
            const Vector& target = m_analysisMagnitudes[i];
            m_synthesisMagnitudes[i] = Vector::IfLess(source, target, source + (target - source) * m_slewUpAlpha, target);
            m_analysisPhase[i] = AnalysisPhase(dft.m_components[i]);
            SetAnalysisOmega(i);
        }
    }

    static float OmegaBin(int bin)
    {
        return static_cast<float>(bin) / x_tableSize;
    }

    static int BinOmega(double omega)
    {
        return std::min<int>(std::max<int>(1, static_cast<int>(std::round(omega * x_tableSize))), x_tableSize - 1);
    }

    static bool WillAlias(double omegaInstantaneous, int bin)
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

    static DoubleVector PrincArg(const DoubleVector& arg)
    {
        DoubleVector wrapped = arg - arg.Floor();
        return DoubleVector::IfLess(DoubleVector::Splat(0.5f), wrapped, wrapped - 1.0f, wrapped);
    }

    static double LerpPhase(double phi1, double phi2, double alpha)
    {
        double diff = phi2 - phi1;
        diff = PrincArg(diff);
        return phi1 + alpha * diff;
    }

    void SetAnalysisOmega(int bin)
    {
        double omegaBin = OmegaBin(bin);
        double deltaPhiExpected = omegaBin * x_H;
        // Preserve the measured phase difference through wrapping and frequency storage.
        //
        DoubleVector deltaPhi = m_analysisPhase[bin].template Convert<double>() - m_prevAnalysisPhase[bin].template Convert<double>();
        
        deltaPhi = PrincArg(deltaPhi - deltaPhiExpected);
        
        m_omegaInstantaneous[bin] = deltaPhi / x_H + omegaBin;
    }

    void SpectralDistortion(DFT& dft, Input& input)
    {
        static_assert(Size == 1, "SpectralDistortion is mono only");
        float rmsTotal = 0.0f;
        float rmsQuiet = 1e-6f;
        float rmsLoud = 1e-6f;

        for (size_t i = 1; i < x_maxComponents; ++i)
        {
            float rms = dft.m_components[i][0].real() * dft.m_components[i][0].real() + dft.m_components[i][0].imag() * dft.m_components[i][0].imag();
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
            float rms = dft.m_components[i][0].real() * dft.m_components[i][0].real() + dft.m_components[i][0].imag() * dft.m_components[i][0].imag();
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
                    shiftCoefficient[i] = std::pow(dft.m_components[i][0] / mag, input.m_shift[0].ToDouble());
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

    // One PVDR per channel.
    //
    void Analyze(PVDR* pvdr, Buffer& buffer)
    {
        for (size_t channel = 0; channel < Size; ++channel)
        {
            pvdr[channel].Clear();
        }

        m_analysisDft.Transform(buffer, m_workspace);
        ProcessPhases(m_analysisDft);
        for (size_t channel = 0; channel < Size; ++channel)
        {
            pvdr[channel].Analyze();
        }
    }

    void PrimeAndAnalyze(PVDR* pvdr, Buffer& buffer)
    {
        PrimeFromPrevious();
        Analyze(pvdr, buffer);
    }

    void PrimeAnalysis(Buffer& waveTable)
    {
        m_analysisDft.Transform(waveTable, m_workspace);
        PrimePhases(m_analysisDft);
    }        

    // Analyzes a Hann-windowed host-rate buffer and writes the Hann-windowed
    // engine-rate synthesis frame. One input per channel.
    //
    void Synthesize(Buffer& buffer, SynthesisFrame& frame, Input* input)
    {        
        Vector slewUp;
        for (size_t channel = 0; channel < Size; ++channel)
        {
            slewUp[channel] = input[channel].m_slewUp;
        }

        SetSlewUp(slewUp);

        Analyze(m_pvdr, buffer);
                
        for (size_t i = 0; i < x_numOscillators; ++i)
        {
            Vector detune;
            for (size_t channel = 0; channel < Size; ++channel)
            {
                detune[channel] = input[channel].GetUnisonDetune(i);
            }

            m_oscillators[i].FixupPhases(detune, m_pvdr);
        }

        m_synthDft.Init();
        for (size_t i = 0; i < x_numOscillators; ++i)
        {
            typename Oscillator::Input oscillatorInput[Size];
            for (size_t channel = 0; channel < Size; ++channel)
            {
                oscillatorInput[channel] = input[channel].MakeOscillatorInput(i);
            }

            m_oscillators[i].Synthesize(m_synthDft, oscillatorInput, m_pvdr);        
        }

        //SpectralDistortion(m_synthDft, input[0]);

        m_synthDft.InverseTransform(frame, x_maxComponents, m_workspace);
        MathGeneric<x_synthesisBits>::Hann(frame);
    }

    // One input per channel.
    //
    void Process(Buffer& previousWaveTable, Buffer& buffer, SynthesisFrame& frame, Input* input)
    {
        PrimeAnalysis(previousWaveTable);
        Synthesize(buffer, frame, input);
    }

    void PrintTopStuff()
    {
        for (size_t channel = 0; channel < Size; ++channel)
        {
            double maxMag = 0.0;
            for (size_t i = 1; i < x_maxComponents; ++i)
            {
                maxMag = std::max<double>(maxMag, m_synthesisMagnitudes[i][channel]);
            }

            if (maxMag < 1e-4)
            {
                continue;
            }

            INFO("========================");
            for (size_t i = 1; i < x_maxComponents; ++i)
            {
                if (m_synthesisMagnitudes[i][channel] > maxMag / 10)
                {
                    INFO("%p channel %zu High bin: %zu, mag: %f, omega instantaneous: %f, an phase: %f, synth phase: %f", 
                        this, channel, i, m_synthesisMagnitudes[i][channel], 
                        m_omegaInstantaneous[i][channel], m_analysisPhase[i][channel], m_oscillators[0].m_synthesisPhase[i][channel]);
                }
            }
        }
    }

    // Analysis hop in host samples.
    //
    static constexpr size_t GetGrainLaunchSamples()
    {
        return x_H;
    }

    // Synthesis hop in engine samples: the analysis hop's duration.
    //
    static constexpr size_t GetSynthesisHopSamples()
    {
        return x_H * SampleTimer::x_oversample;
    }

    Vector m_synthesisMagnitudes[x_maxComponents];
    Vector m_slewUpAlpha;
    Vector m_analysisMagnitudes[x_maxComponents];
    Vector m_prevAnalysisMagnitudes[x_maxComponents];
    DoubleVector m_omegaInstantaneous[x_maxComponents];
    Vector m_analysisPhase[x_maxComponents];
    Vector m_prevAnalysisPhase[x_maxComponents];
    Oscillator m_oscillators[x_numOscillators];
    PVDR m_pvdr[Size];
    DFT m_analysisDft;
    DFT m_synthDft;
    // Shared by the analysis transform and the larger synthesis transform.
    //
    ComplexVector m_workspace[x_synthesisFrameSize];
};

using Resynthesizer = ResynthesizerGeneric<float, 1>;

#include "SpectralModel.hpp"
