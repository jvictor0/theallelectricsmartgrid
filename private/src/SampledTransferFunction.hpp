#pragma once

#include "Math.hpp"
#include "MultiChannelComplex.hpp"
#include "SampleTimer.hpp"
#include "SnapshotUIState.hpp"
#include "Tuning.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <numeric>
#include <limits>
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
    using ResponseVector = MultiChannelNumber<float, 1>;
    static constexpr size_t x_numLanes = 1;

    ResponseVector MagnitudeResponses(const ResponseFrequency& frequency) const
    {
        return MagnitudeResponse(frequency, 0);
    }

    virtual ~FilterResponseSnapshot() = default;
    virtual std::complex<float> ComplexResponse(const ResponseFrequency& frequency, size_t lane) const = 0;

    virtual float MagnitudeResponse(const ResponseFrequency& frequency, size_t lane) const
    {
        return std::abs(ComplexResponse(frequency, lane));
    }
};

// Scalar access is an adapter for tests and individual queries. Plot evaluation
// uses the vector methods and never extracts lanes inside the response math.
//
template<size_t Size>
struct VectorResponseSnapshot : FilterResponseSnapshot
{
    using ResponseVector = MultiChannelNumber<float, Size>;
    using DoubleVector = MultiChannelNumber<double, Size>;
    using ComplexVector = MultiChannelComplex<double, Size>;
    static constexpr size_t x_numLanes = Size;

    virtual ComplexVector ComplexResponses(const ResponseFrequency& frequency) const = 0;

    virtual ResponseVector MagnitudeResponses(const ResponseFrequency& frequency) const
    {
        return ComplexResponses(frequency).Abs().template Convert<float>();
    }

    std::complex<float> ComplexResponse(const ResponseFrequency& frequency, size_t lane) const override
    {
        assert(lane < Size);
        return static_cast<std::complex<float>>(ComplexResponses(frequency)[lane]);
    }

    float MagnitudeResponse(const ResponseFrequency& frequency, size_t lane) const override
    {
        assert(lane < Size);
        return MagnitudeResponses(frequency)[lane];
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
    std::vector<OrderStatistics> m_scalarQuery;

    explicit SampledTransferFunction(const Config& config)
        : m_config(config)
        , m_samplingThreshold(UINT32_MAX / std::max(uint32_t{1}, config.m_samplingDivisor))
    {
        assert(config.m_capacity > 0 && config.m_numSamples > 0);
        assert(config.m_numSamples <= config.m_capacity && config.m_samplingDivisor > 0);
        assert(config.m_lowerPercentile >= 0 && config.m_lowerPercentile <= 0.5f);
        assert(config.m_upperPercentile >= 0.5f && config.m_upperPercentile <= 1);

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

    virtual size_t NumLanes() const = 0;
    virtual void ResponseAll(const std::vector<SampleId>& samples, const ResponseFrequency& frequency,
        OrderStatistics* output) = 0;

    OrderStatistics Response(const std::vector<SampleId>& samples, const ResponseFrequency& frequency, size_t lane)
    {
        assert(lane < NumLanes());
        m_scalarQuery.resize(NumLanes());
        ResponseAll(samples, frequency, m_scalarQuery.data());
        return m_scalarQuery[lane];
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
    using ResponseVector = typename Snapshot::ResponseVector;
    static constexpr size_t x_numLanes = Snapshot::x_numLanes;

    struct Comparator
    {
        size_t m_low;
        size_t m_high;
    };

    std::vector<ResponseVector> m_magnitudes;
    std::vector<Comparator> m_sortNetwork;
    size_t m_sortSize = 0;
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
        PrepareSort(config.m_numSamples);
    }

    size_t NumLanes() const override
    {
        return x_numLanes;
    }

    void PrepareSort(size_t count)
    {
        size_t size = x_numLanes == 1 ? 4 : 1;
        while (size < count)
        {
            size *= 2;
        }

        if (size == m_sortSize)
        {
            return;
        }

        m_sortSize = size;
        m_magnitudes.resize(size);
        m_sortNetwork.clear();
        if constexpr (x_numLanes == 1)
        {
            return;
        }

        for (size_t block = 2; block <= size; block *= 2)
        {
            for (size_t stride = block / 2; stride > 0; stride /= 2)
            {
                for (size_t i = 0; i < size; ++i)
                {
                    size_t other = i ^ stride;
                    if (other > i)
                    {
                        bool ascending = (i & block) == 0;
                        m_sortNetwork.push_back({ascending ? i : other, ascending ? other : i});
                    }
                }
            }
        }
    }

    // A single output curve uses SIMD across captured magnitudes. Wider
    // responses use SIMD across independent output lanes instead.
    //
    void SortMonoMagnitudes()
    {
        using Quad = MultiChannelNumber<float, 4>;
        static_assert(sizeof(ResponseVector) == sizeof(float), "mono sorting stores one float per sample");
        auto load = [&](size_t index)
        {
            typename Quad::Vector vector;
            std::memcpy(&vector, m_magnitudes.data() + index, sizeof(vector));
            return Quad::FromVector(vector);
        };
        auto save = [&](size_t index, const Quad& value)
        {
            auto vector = value.Load();
            std::memcpy(m_magnitudes.data() + index, &vector, sizeof(vector));
        };
        for (size_t block = 2; block <= m_sortSize; block *= 2)
        {
            for (size_t stride = block / 2; stride > 0; stride /= 2)
            {
                for (size_t i = 0; i < m_sortSize; i += 4)
                {
                    bool ascending = (i & block) == 0;
                    auto a = load(i);
                    if (stride >= 4)
                    {
                        size_t other = i ^ stride;
                        if (other > i)
                        {
                            auto b = load(other);
                            auto low = Quad::Min(a, b);
                            auto high = Quad::Max(a, b);
                            save(i, ascending ? low : high);
                            save(other, ascending ? high : low);
                        }
                    }
                    else
                    {
                        auto v = a.Load();
                        auto shuffled = stride == 1 ? __builtin_shufflevector(v, v, 1, 0, 3, 2)
                            : __builtin_shufflevector(v, v, 2, 3, 0, 1);
                        auto b = Quad::FromVector(shuffled);
                        auto low = Quad::Min(a, b).Load();
                        auto high = Quad::Max(a, b).Load();
                        if (block == 2)
                        {
                            save(i, Quad::FromVector(__builtin_shufflevector(low, high, 0, 5, 6, 3)));
                        }
                        else if (stride == 1)
                        {
                            save(i, Quad::FromVector(ascending
                                ? __builtin_shufflevector(low, high, 0, 5, 2, 7)
                                : __builtin_shufflevector(high, low, 0, 5, 2, 7)));
                        }
                        else
                        {
                            save(i, Quad::FromVector(ascending
                                ? __builtin_shufflevector(low, high, 0, 1, 6, 7)
                                : __builtin_shufflevector(high, low, 0, 1, 6, 7)));
                        }
                    }
                }
            }
        }
    }

    virtual ResponseVector MagnitudeResponses(SampleId id, const ResponseFrequency& frequency) const
    {
        return m_readSnapshots[id].MagnitudeResponses(frequency);
    }

    void ResponseAll(const std::vector<SampleId>& samples, const ResponseFrequency& frequency,
        OrderStatistics* output) override
    {
        assert(!samples.empty() && samples.size() <= m_config.m_numSamples);
        PrepareSort(samples.size());
        for (size_t i = 0; i < samples.size(); ++i)
        {
            m_magnitudes[i] = MagnitudeResponses(samples[i], frequency);
            for (size_t lane = 0; lane < x_numLanes; ++lane)
            {
                assert(std::isfinite(m_magnitudes[i][lane]) && m_magnitudes[i][lane] >= 0);
            }
        }

        for (size_t i = samples.size(); i < m_sortSize; ++i)
        {
            m_magnitudes[i] = ResponseVector::Splat(std::numeric_limits<float>::infinity());
        }

        // Every compare/exchange independently orders all voices in SIMD lanes.
        // Padding permits arbitrary configured cohort sizes and percentiles.
        //
        if constexpr (x_numLanes == 1)
        {
            SortMonoMagnitudes();
        }
        else
        {
            for (const auto& pair : m_sortNetwork)
            {
                auto a = m_magnitudes[pair.m_low];
                auto b = m_magnitudes[pair.m_high];
                m_magnitudes[pair.m_low] = ResponseVector::Min(a, b);
                m_magnitudes[pair.m_high] = ResponseVector::Max(a, b);
            }
        }

        size_t lower = static_cast<size_t>(m_config.m_lowerPercentile * (samples.size() - 1));
        size_t median = (samples.size() - 1) / 2;
        size_t upper = static_cast<size_t>(m_config.m_upperPercentile * (samples.size() - 1));
        for (size_t lane = 0; lane < x_numLanes; ++lane)
        {
            output[lane] = {m_magnitudes[lower][lane], m_magnitudes[median][lane], m_magnitudes[upper][lane]};
        }
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

// UI-owned frequency grid and evaluated columns. Reuse across paints; evaluate
// every response lane before any paths are drawn. Frequency setup is shared.
//
struct SampledResponsePlot
{
    size_t m_numLanes = 0;
    bool m_logX = true;
    double m_sampleRate = 0;
    double m_minHz = 0;
    std::vector<ResponseFrequency> m_frequencies;
    std::vector<SampledTransferFunction::OrderStatistics> m_values;

    void Evaluate(SampledTransferFunction& response, size_t points, bool logX, double minHz, double sampleRate)
    {
        assert(points > 1 && minHz > 0 && minHz < sampleRate / 2);
        if (m_frequencies.size() != points || m_logX != logX || m_sampleRate != sampleRate || m_minHz != minHz)
        {
            m_logX = logX;
            m_sampleRate = sampleRate;
            m_minHz = minHz;
            m_frequencies.resize(points);
            double minVOct = std::log2(minHz / Tuning::x_middleCHz);
            double octaves = std::log2(sampleRate / (2 * minHz));
            for (size_t i = 0; i < points; ++i)
            {
                float position = static_cast<float>(i) / (points - 1);
                m_frequencies[i] = logX
                    ? ResponseFrequency::VOct(minVOct + octaves * position, sampleRate)
                    : ResponseFrequency::Hz(position * sampleRate / 2, sampleRate);
            }
        }

        m_numLanes = response.NumLanes();
        auto samples = response.Sample();
        m_values.resize(samples.empty() ? 0 : points * m_numLanes);
        if (samples.empty())
        {
            return;
        }

        for (size_t i = 0; i < points; ++i)
        {
            response.ResponseAll(samples, m_frequencies[i], m_values.data() + i * m_numLanes);
        }
    }

    const SampledTransferFunction::OrderStatistics& Get(size_t column, size_t lane) const
    {
        assert(lane < m_numLanes);
        return m_values[column * m_numLanes + lane];
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
