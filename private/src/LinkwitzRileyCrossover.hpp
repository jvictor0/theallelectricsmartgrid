#pragma once

#include <cmath>
#include <algorithm>
#include <complex>
#include "MultiChannelNumber.hpp"
#include "SampledTransferFunction.hpp"

// Legacy crossover name: the retained Qs produce fourth-order Butterworth
// branches, not LR4. Cutoff limits and scalar response are shared by all widths.
//
template<typename Number, size_t Size>
struct LinkwitzRileyCrossoverGeneric
{
    static constexpr float x_minCyclesPerSample = 0.0001f;
    static constexpr float x_maxCyclesPerSample = 0.499f;

    struct ComplexCrossoverOutput
    {
        std::complex<float> m_lowPass;
        std::complex<float> m_highPass;
    };

    // Preserve the existing fourth-order Butterworth pole pairs.
    //
    static float Q1()
    {
        return 1.0f / (2.0f * std::cos(M_PI / 8.0f));
    }

    static float Q2()
    {
        return 1.0f / (2.0f * std::cos(3.0f * M_PI / 8.0f));
    }

    static ComplexCrossoverOutput TransferFunction(float cutoffFreq, float freq)
    {
        Snapshot snapshot;
        snapshot.m_tangent = Math::TanPi(std::clamp(cutoffFreq, x_minCyclesPerSample, x_maxCyclesPerSample));
        return snapshot.Responses(ResponseFrequency::CyclesPerSample(freq));
    }

    struct ScalarCrossoverOutput
    {
        float m_lowPass;
        float m_highPass;
    };

    static ScalarCrossoverOutput FrequencyResponse(float cutoffFreq, float freq)
    {
        ComplexCrossoverOutput complex = TransferFunction(cutoffFreq, freq);
        return {std::abs(complex.m_lowPass), std::abs(complex.m_highPass)};
    }

    using Vector = MultiChannelNumber<Number, Size>;
    struct CrossoverOutput
    {
        Vector m_lowPass;
        Vector m_highPass;
    };

    struct Snapshot : FilterResponseSnapshot
    {
        double m_tangent = 0.1;

        ComplexCrossoverOutput Responses(const ResponseFrequency& frequency) const
        {
            double gc = m_tangent * frequency.m_cosHalf;
            double s = frequency.m_sinHalf;
            auto denominator = std::complex<double>(gc * gc - s * s, gc * s / Q1())
                * std::complex<double>(gc * gc - s * s, gc * s / Q2());
            return {static_cast<std::complex<float>>(gc * gc * gc * gc / denominator),
                static_cast<std::complex<float>>(s * s * s * s / denominator)};
        }

        std::complex<float> ComplexResponse(const ResponseFrequency& frequency, size_t lane) const override
        {
            assert(lane < 2);
            auto pair = Responses(frequency);
            return lane == 0 ? pair.m_lowPass : pair.m_highPass;
        }
    };

    using UIState = TransferFunctionGeneric<Snapshot>;

    Snapshot MakeSnapshot() const
    {
        Snapshot snapshot;
        snapshot.m_tangent = static_cast<double>(m_first.m_a2) / m_first.m_a1;
        return snapshot;
    }

    void PopulateUIStateSample(SampledSnapshotWriter<UIState>& writer) const
    {
        writer.PopulateUIStateSample([&]
        {
            return MakeSnapshot();
        });
    }

    // Linear trapezoidal SVF. Coefficients are shared across audio channels;
    // integrator states retain low-cutoff precision without DF1 pole cancellation.
    //
    struct Section
    {
        Number m_a1 = 1;
        Number m_a2 = 0;
        Number m_a3 = 0;
        Number m_k = 1;
        Vector m_ic1;
        Vector m_ic2;

        CrossoverOutput Process(const Vector& input)
        {
            Vector v3 = input - m_ic2;
            Vector v1 = m_ic1 * m_a1 + v3 * m_a2;
            Vector v2 = m_ic2 + m_ic1 * m_a2 + v3 * m_a3;
            m_ic1 = v1 * 2 - m_ic1;
            m_ic2 = v2 * 2 - m_ic2;
            return {v2, input - v1 * m_k - v2};
        }

        void Reset()
        {
            m_ic1 = Vector();
            m_ic2 = Vector();
        }
    };

    Section m_first;
    Section m_lowSecond;
    Section m_highSecond;
    float m_cyclesPerSample;

    LinkwitzRileyCrossoverGeneric()
    {
        SetCyclesPerSample(0.1f);
    }

    CrossoverOutput Process(const Vector& input)
    {
        CrossoverOutput first = m_first.Process(input);
        return {m_lowSecond.Process(first.m_lowPass).m_lowPass,
            m_highSecond.Process(first.m_highPass).m_highPass};
    }

    void SetCyclesPerSample(float cyclesPerSample)
    {
        SetCyclesPerSampleBatch<1>(this, MultiChannelNumber<float, 1>::Splat(cyclesPerSample));
    }

    // Batch independent cutoffs across lanes, then share each coefficient set
    // across its audio channels. Three crossovers use a padded four-lane vector.
    //
    template<size_t Count>
    static void SetCyclesPerSampleBatch(LinkwitzRileyCrossoverGeneric* crossovers,
        const MultiChannelNumber<float, Count>& cyclesPerSample)
    {
        using Packed = MultiChannelNumber<float, Count>;
        Packed frequency = Packed::Min(Packed::Splat(x_maxCyclesPerSample),
            Packed::Max(Packed::Splat(x_minCyclesPerSample), cyclesPerSample));

        // Reflect tan(pi*f) about f=1/4 to keep the approximation in [0, pi/4].
        // At high cutoffs compute the SVF coefficients using the reciprocal
        // tangent, avoiding large intermediate values.
        //
        Packed angle = Packed::Min(frequency, Packed::Splat(0.5f) - frequency) * static_cast<float>(M_PI);
        Packed squared = angle * angle;
        Packed numerator = Packed::FusedMultiplyAdd(squared,
            Packed::FusedMultiplyAdd(squared, Packed::Splat(378) - squared, Packed::Splat(-17325)), Packed::Splat(135135));
        Packed denominator = Packed::FusedMultiplyAdd(squared,
            Packed::FusedMultiplyAdd(squared,
                Packed::FusedMultiplyAdd(squared, Packed::Splat(-28), Packed::Splat(3150)), Packed::Splat(-62370)), Packed::Splat(135135));
        Packed tangent = angle * numerator / denominator;
        Packed tangentSquared = tangent * tangent;
        Packed sum = Packed::Splat(1) + tangentSquared;

        for (size_t section = 0; section < 2; ++section)
        {
            float q = section == 0 ? Q1() : Q2();
            Packed damping = tangent / q;
            Packed reciprocal = Packed::Splat(1) / (sum + damping);
            Packed small = tangentSquared * reciprocal;
            Packed a1 = Packed::IfLess(frequency, Packed::Splat(0.25f), reciprocal, small);
            Packed a2 = tangent * reciprocal;
            Packed a3 = Packed::IfLess(frequency, Packed::Splat(0.25f), small, reciprocal);
            for (size_t lane = 0; lane < Count; ++lane)
            {
                auto& crossover = crossovers[lane];
                crossover.m_cyclesPerSample = frequency[lane];
                Section& target = section == 0 ? crossover.m_first : crossover.m_lowSecond;
                target.m_a1 = a1[lane];
                target.m_a2 = a2[lane];
                target.m_a3 = a3[lane];
                target.m_k = 1.0f / q;
                if (section == 1)
                {
                    crossover.m_highSecond.m_a1 = target.m_a1;
                    crossover.m_highSecond.m_a2 = target.m_a2;
                    crossover.m_highSecond.m_a3 = target.m_a3;
                    crossover.m_highSecond.m_k = target.m_k;
                }
            }
        }
    }

    void Reset()
    {
        m_first.Reset();
        m_lowSecond.Reset();
        m_highSecond.Reset();
    }
};

using LinkwitzRileyCrossover = LinkwitzRileyCrossoverGeneric<float, 1>;
