#pragma once

#include "Math.hpp"
#include "SampleTimer.hpp"
#include "SnapshotUIState.hpp"
#include "Tuning.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <numeric>
#include <vector>

// Prepare once per frequency column, then reuse across every captured state.
// Public frequency APIs use Hz or volts/octave relative to middle C.
//
struct ResponseFrequency
{
    double m_cyclesPerSample;
    double m_sinHalf;
    double m_cosHalf;
    std::complex<double> m_zInverse;

    static ResponseFrequency CyclesPerSample(double frequency)
    {
        assert(std::isfinite(frequency) && frequency >= 0 && frequency <= 0.5);
        if (frequency == 0)
        {
            return {0, 0, 1, {1, 0}};
        }

        if (frequency == 0.5)
        {
            return {0.5, 1, 0, {-1, 0}};
        }

        float halfPhase = static_cast<float>(frequency * 0.5);
        double s = Math::Sin2pi(halfPhase);
        double c = Math::Cos2pi(halfPhase);
        return {frequency, s, c, {1 - 2 * s * s, -2 * s * c}};
    }

    static ResponseFrequency Hz(double frequency, double sampleRate)
    {
        assert(sampleRate > 0);
        return CyclesPerSample(std::clamp(frequency / sampleRate, 0.0, 0.5));
    }

    static ResponseFrequency VOct(double frequency, double sampleRate)
    {
        return Hz(Tuning::x_middleCHz * std::exp2(frequency), sampleRate);
    }
};

struct FilterResponseSnapshot
{
    virtual ~FilterResponseSnapshot() = default;
    virtual std::complex<float> ComplexResponse(const ResponseFrequency& frequency, size_t lane) const = 0;

    virtual float MagnitudeResponse(const ResponseFrequency& frequency, size_t lane) const
    {
        return std::abs(ComplexResponse(frequency, lane));
    }
};

template<typename Snapshot, size_t Size>
struct FilterSnapshotArray : FilterResponseSnapshot
{
    std::array<Snapshot, Size> m_lanes;

    std::complex<float> ComplexResponse(const ResponseFrequency& frequency, size_t lane) const override
    {
        assert(lane < Size);
        return m_lanes[lane].ComplexResponse(frequency, 0);
    }

    float MagnitudeResponse(const ResponseFrequency& frequency, size_t lane) const override
    {
        assert(lane < Size);
        return m_lanes[lane].MagnitudeResponse(frequency, 0);
    }
};

struct SampledTransferFunction
{
    using SampleId = size_t;

    struct OrderStatistics
    {
        float m_lower;
        float m_median;
        float m_upper;
    };

    struct Config
    {
        size_t m_capacity = 4096;
        size_t m_numSamples = 32;
        float m_lowerPercentile = 0.1f;
        float m_upperPercentile = 0.9f;
        // At the host sample rate, 32 captures span one 60 Hz paint on average.
        //
        uint32_t m_samplingDivisor = 25;
    };

    const Config m_config;
    const uint32_t m_samplingThreshold;
    std::vector<float> m_magnitudes;

    explicit SampledTransferFunction(const Config& config)
        : m_config(config)
        , m_samplingThreshold(UINT32_MAX / std::max(uint32_t{1}, config.m_samplingDivisor))
    {
        assert(config.m_capacity > 0 && config.m_numSamples > 0);
        assert(config.m_numSamples <= config.m_capacity && config.m_samplingDivisor > 0);
        assert(config.m_lowerPercentile >= 0 && config.m_lowerPercentile <= 0.5f);
        assert(config.m_upperPercentile >= 0.5f && config.m_upperPercentile <= 1);
        m_magnitudes.reserve(config.m_numSamples);
    }

    virtual ~SampledTransferFunction() = default;

    // IDs address the UI-owned batch and remain valid until the next Sample.
    // Sample once per plot, including all lanes of a multichannel plot.
    //
    virtual std::vector<SampleId> Sample() = 0;
    virtual std::complex<float> ComplexResponse(SampleId id, const ResponseFrequency& frequency, size_t lane) const = 0;

    virtual float MagnitudeResponse(SampleId id, const ResponseFrequency& frequency, size_t lane) const
    {
        return std::abs(ComplexResponse(id, frequency, lane));
    }

    OrderStatistics Response(const std::vector<SampleId>& samples, const ResponseFrequency& frequency, size_t lane)
    {
        assert(!samples.empty());
        m_magnitudes.clear();
        for (SampleId id : samples)
        {
            float magnitude = MagnitudeResponse(id, frequency, lane);
            assert(std::isfinite(magnitude) && magnitude >= 0);
            m_magnitudes.push_back(magnitude);
        }

        auto quantile = [&](float percentile)
        {
            size_t index = static_cast<size_t>(percentile * (m_magnitudes.size() - 1));
            std::nth_element(m_magnitudes.begin(), m_magnitudes.begin() + index, m_magnitudes.end());
            return m_magnitudes[index];
        };
        float lower = quantile(m_config.m_lowerPercentile);
        float median = quantile(0.5f);
        float upper = quantile(m_config.m_upperPercentile);
        return {lower, median, upper};
    }

    OrderStatistics ResponseHz(const std::vector<SampleId>& samples, double frequency, double sampleRate, size_t lane)
    {
        return Response(samples, ResponseFrequency::Hz(frequency, sampleRate), lane);
    }

    OrderStatistics ResponseVOct(const std::vector<SampleId>& samples, double frequency, double sampleRate, size_t lane)
    {
        return Response(samples, ResponseFrequency::VOct(frequency, sampleRate), lane);
    }
};

template<typename Snapshot>
struct TransferFunctionGeneric : SampledTransferFunction
{
    using SnapshotType = Snapshot;
    SampledSnapshotHistory<Snapshot> m_history;
    std::vector<Snapshot> m_readSnapshots;
    std::vector<Snapshot> m_pendingSnapshots;
    size_t m_readFirst = 0;

    TransferFunctionGeneric()
        : TransferFunctionGeneric(Config{})
    {
    }

    explicit TransferFunctionGeneric(const Config& config)
        : SampledTransferFunction(config)
        , m_history(config.m_capacity)
    {
        m_readSnapshots.reserve(config.m_numSamples);
        m_pendingSnapshots.reserve(config.m_numSamples);
    }

    std::vector<SampleId> Sample() override
    {
        m_pendingSnapshots.clear();
        size_t first = m_history.m_first.load(std::memory_order_acquire);
        size_t newest = m_history.m_published.load(std::memory_order_acquire);
        size_t available = newest >= first ? newest - first + 1 : 0;
        size_t count = std::min({available, m_history.m_capacity, m_config.m_numSamples});
        if (first != m_readFirst)
        {
            m_readSnapshots.clear();
            m_readFirst = first;
        }
        for (size_t offset = 0; offset < count; ++offset)
        {
            Snapshot snapshot;
            if (m_history.Read(newest - offset, snapshot))
            {
                m_pendingSnapshots.push_back(snapshot);
            }
        }

        // A machine switch during the copy invalidates the entire batch.
        // The next paint can pick up the new machine's history.
        //
        if (first != m_history.m_first.load(std::memory_order_acquire))
        {
            m_readSnapshots.clear();
        }
        else if (!m_pendingSnapshots.empty())
        {
            m_readSnapshots.swap(m_pendingSnapshots);
        }

        std::vector<SampleId> ids(m_readSnapshots.size());
        std::iota(ids.begin(), ids.end(), 0);
        return ids;
    }

    std::complex<float> ComplexResponse(SampleId id, const ResponseFrequency& frequency, size_t lane) const override
    {
        return m_readSnapshots.at(id).ComplexResponse(frequency, lane);
    }

    float MagnitudeResponse(SampleId id, const ResponseFrequency& frequency, size_t lane) const override
    {
        return m_readSnapshots.at(id).MagnitudeResponse(frequency, lane);
    }
};

// Audio-thread-only RNG and publication. Composite owners draw one coin and
// capture all of their coefficients in the callback, without child samplers.
//
template<typename UIState>
struct SampledSnapshotWriter
{
    UIState* m_uiState = nullptr;
    uint32_t m_rng = 0x9e3779b9;
    size_t m_source = 0;

    void Bind(UIState* state)
    {
        m_uiState = state;
    }

    void SetSource(size_t source)
    {
        if (source != m_source && m_uiState)
        {
            m_uiState->m_history.Clear();
        }

        m_source = source;
    }

    template<typename Capture>
    void PopulateUIStateSample(Capture capture)
    {
        if (!m_uiState || !SampleTimer::IsHostFrame())
        {
            return;
        }

        m_rng ^= m_rng << 13;
        m_rng ^= m_rng >> 17;
        m_rng ^= m_rng << 5;
        bool first = m_uiState->m_history.m_published.load(std::memory_order_relaxed)
            < m_uiState->m_history.m_first.load(std::memory_order_relaxed);
        if (first || m_rng <= m_uiState->m_samplingThreshold)
        {
            m_uiState->m_history.Push(capture());
        }
    }
};
