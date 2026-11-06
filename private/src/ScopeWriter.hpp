#pragma once

#include <atomic>
#include <array>
#include <memory>
#include <vector>
#include "AdaptiveWaveTable.hpp"
#include "ButterworthFilter.hpp"
#include "Filter.hpp"
#include "SampleEventHistory.hpp"
#include "SampleTimer.hpp"

// Common scalar read interface. Positions and cycle markers always use the
// original stored-sample timeline. Level l has spacing 4^l and a known FIR
// delay; publication exposes only completed samples. Ordinary vector storage
// follows this publication protocol, without pinning slots against overwrite.
//
struct ScopeWriter
{
    static constexpr size_t x_baseCapacity = SampleTimer::x_sampleRate / 4;
    static constexpr size_t x_numLevels = 6;
    static constexpr size_t x_levelRatio = 4;
    static constexpr size_t x_firTaps = 17;
    static constexpr size_t x_numStartIndices = 256;

    struct Markers
    {
        double m_startIndices[x_numStartIndices]{};
        size_t m_endIndices[x_numStartIndices]{};
        std::atomic<size_t> m_startIndexIndex{0};
        size_t m_advanceStartIndices = 0;
    };

    struct Level
    {
        size_t m_acceptedIndex = 0;
        size_t m_oldestIndex = 0;
    };

    struct AcceptedMarkers
    {
        double m_startIndices[x_numStartIndices]{};
        size_t m_endIndices[x_numStartIndices]{};
        size_t m_startIndexIndex = 0;
    };

    const size_t m_voices;
    const size_t m_scopes;
    const size_t m_decimation;
    const size_t m_capacity;
    size_t m_subIndex = 0;
    size_t m_index = 0;
    std::atomic<size_t> m_publishedIndex{0};
    size_t m_acceptedIndex = 0;
    size_t m_oldestHistoryIndex = 0;
    std::unique_ptr<Markers[]> m_markers;
    std::unique_ptr<AcceptedMarkers[]> m_acceptedMarkers;
    Level m_levels[x_numLevels];

    ScopeWriter(size_t voices, size_t scopes, size_t decimation, size_t capacity)
        : m_voices(voices)
        , m_scopes(scopes)
        , m_decimation(decimation)
        , m_capacity(capacity)
        , m_markers(std::make_unique<Markers[]>(voices * scopes))
        , m_acceptedMarkers(std::make_unique<AcceptedMarkers[]>(voices * scopes))
    {
        assert(voices > 0 && scopes > 0 && decimation > 0 && capacity > 1);
    }

    virtual ~ScopeWriter() = default;
    virtual float ReadStored(size_t scope, size_t voice, size_t index, size_t lod) const = 0;
    virtual void CommitSample() = 0;
    virtual void Downsample(size_t firstIndex, size_t endIndex) = 0;

    Markers& GetMarkers(size_t scope, size_t voice)
    {
        assert(scope < m_scopes && voice < m_voices);
        return m_markers[scope * m_voices + voice];
    }

    const Markers& GetMarkers(size_t scope, size_t voice) const
    {
        assert(scope < m_scopes && voice < m_voices);
        return m_markers[scope * m_voices + voice];
    }

    const AcceptedMarkers& GetAcceptedMarkers(size_t scope, size_t voice) const
    {
        assert(scope < m_scopes && voice < m_voices);
        return m_acceptedMarkers[scope * m_voices + voice];
    }

    static constexpr size_t Stride(size_t lod)
    {
        return static_cast<size_t>(1) << (2 * lod);
    }

    static constexpr double Origin(size_t lod)
    {
        // Each FIR emits at input index 3, then every four inputs. Subtract
        // its group delay and accumulate both offsets in original samples.
        //
        return (static_cast<double>(x_levelRatio - 1) - (x_firTaps - 1) / 2)
            * (Stride(lod) - 1) / (x_levelRatio - 1);
    }

    size_t MaxIndexes() const
    {
        return m_capacity;
    }

    size_t MaxIndexes(size_t lod) const
    {
        assert(lod < x_numLevels);
        return m_capacity * Stride(lod);
    }

    size_t AcceptedCount(size_t lod) const
    {
        return lod == 0 ? m_acceptedIndex : m_levels[lod].m_acceptedIndex;
    }

    size_t OldestStoredIndex(size_t lod) const
    {
        return lod == 0 ? m_oldestHistoryIndex : m_levels[lod].m_oldestIndex;
    }

    double OldestIndex(size_t lod) const
    {
        return std::max(0.0, Origin(lod) + static_cast<double>(OldestStoredIndex(lod)) * Stride(lod));
    }

    double LatestIndex(size_t lod) const
    {
        size_t count = AcceptedCount(lod);
        return count == 0 ? -1.0 : Origin(lod) + static_cast<double>(count - 1) * Stride(lod);
    }

    size_t GetLod(double samplesBack) const
    {
        return GetLod(samplesBack, m_acceptedIndex);
    }

    size_t GetLod(double samplesBack, size_t publishedIndex) const
    {
        double oldest = static_cast<double>(publishedIndex) - 1.0 - samplesBack;
        if (!std::isfinite(samplesBack) || samplesBack < 0.0 || oldest < 0.0)
        {
            return x_numLevels;
        }

        for (size_t lod = 0; lod < x_numLevels; ++lod)
        {
            if (LatestIndex(lod) >= 0.0 && oldest >= OldestIndex(lod))
            {
                return lod;
            }
        }

        return x_numLevels;
    }

    float Read(size_t scope, size_t voice, double index, size_t lod) const
    {
        assert(scope < m_scopes && voice < m_voices && lod < x_numLevels);
        double position = (index - Origin(lod)) / Stride(lod);
        assert(std::isfinite(position) && position >= 0.0);
        size_t first = static_cast<size_t>(position);
        double fraction = position - static_cast<double>(first);
        float value = ReadStored(scope, voice, first, lod);
        return fraction == 0.0 ? value : value * (1.0 - fraction)
            + ReadStored(scope, voice, first + 1, lod) * fraction;
    }

    float ReadSample(size_t scope, size_t voice, size_t index) const
    {
        assert(scope < m_scopes && voice < m_voices);
        return ReadStored(scope, voice, index, 0);
    }

    float ReadSample(size_t scope, size_t voice, size_t index, size_t lod) const
    {
        return Read(scope, voice, static_cast<double>(index), lod);
    }

    bool ReadHistory(size_t scope, size_t voice, double index, size_t lod, float& value) const
    {
        value = 0.0f;
        if (scope >= m_scopes || voice >= m_voices || lod >= x_numLevels
            || !std::isfinite(index) || index < OldestIndex(lod) || index > LatestIndex(lod))
        {
            return false;
        }

        value = Read(scope, voice, index, lod);
        return true;
    }

    float ReadDisplay(size_t scope, size_t voice, double index, size_t lod) const
    {
        float value = 0.0f;
        if (lod < x_numLevels)
        {
            ReadHistory(scope, voice, std::min(index, LatestIndex(lod)), lod, value);
        }

        return value;
    }

    void AcceptSamples()
    {
        AcceptSamples(m_publishedIndex.load());
    }

    // UI-thread work, once before painting. The captured boundary stays fixed
    // throughout the frame. A stalled UI may read overwritten level-zero slots;
    // catch-up deliberately keeps indexing the ring without an ownership protocol.
    //
    void AcceptSamples(size_t endIndex)
    {
        assert(endIndex >= m_acceptedIndex && endIndex <= m_publishedIndex.load());
        if (endIndex == m_acceptedIndex)
        {
            return;
        }

        double lastAcceptedSample = static_cast<double>(endIndex - 1);
        for (size_t stream = 0; stream < m_scopes * m_voices; ++stream)
        {
            const Markers& source = m_markers[stream];
            AcceptedMarkers& target = m_acceptedMarkers[stream];
            size_t count = source.m_startIndexIndex.load();
            size_t oldest = count > x_numStartIndices ? count - x_numStartIndices : 0;
            while (count > oldest && source.m_startIndices[(count - 1) % x_numStartIndices] > lastAcceptedSample)
            {
                --count;
            }

            target.m_startIndexIndex = count;
            for (size_t marker = 0; marker < x_numStartIndices; ++marker)
            {
                target.m_startIndices[marker] = source.m_startIndices[marker];
                target.m_endIndices[marker] = std::min(source.m_endIndices[marker], endIndex);
            }
        }

        Downsample(m_acceptedIndex, endIndex);
        m_acceptedIndex = endIndex;
        for (size_t lod = 0; lod < x_numLevels; ++lod)
        {
            Level& level = m_levels[lod];
            level.m_acceptedIndex = endIndex / Stride(lod);
            level.m_oldestIndex = level.m_acceptedIndex > m_capacity ? level.m_acceptedIndex - m_capacity : 0;
        }

        m_oldestHistoryIndex = m_levels[0].m_oldestIndex;
    }

    bool IsWriteSample() const
    {
        return m_subIndex == 0;
    }

    void AdvanceIndex()
    {
        if (IsWriteSample())
        {
            CommitSample();
        }

        if (++m_subIndex == m_decimation)
        {
            m_subIndex = 0;
            ++m_index;
        }
    }

    void Publish()
    {
        for (size_t i = 0; i < m_scopes * m_voices; ++i)
        {
            Markers& markers = m_markers[i];
            if (markers.m_advanceStartIndices > 0)
            {
                markers.m_startIndexIndex.store(markers.m_startIndexIndex.load() + markers.m_advanceStartIndices);
                markers.m_advanceStartIndices = 0;
            }
        }

        m_publishedIndex.store(m_index);
    }

    void RecordStart(size_t scope, size_t voice)
    {
        RecordStart(scope, voice, 0.0);
    }

    // Offsets stay in engine samples; markers use original stored samples.
    //
    void RecordStart(size_t scope, size_t voice, double sampleOffset)
    {
        Markers& markers = GetMarkers(scope, voice);
        size_t index = (markers.m_startIndexIndex.load() + markers.m_advanceStartIndices) % x_numStartIndices;
        markers.m_startIndices[index] = static_cast<double>(m_index)
            + (static_cast<double>(m_subIndex) + sampleOffset) / m_decimation;
        ++markers.m_advanceStartIndices;
    }

    void RecordEnd(size_t scope, size_t voice)
    {
        Markers& markers = GetMarkers(scope, voice);
        size_t index = (markers.m_startIndexIndex.load() + markers.m_advanceStartIndices - 1) % x_numStartIndices;
        markers.m_endIndices[index] = m_index + (IsWriteSample() ? 0 : 1);
    }
};

template<size_t Size>
struct ScopeWriterInput : ScopeWriter
{
    using Vector = MultiChannelNumber<float, Size>;

    ScopeWriterInput(size_t scopes, size_t decimation, size_t capacity)
        : ScopeWriter(Size, scopes, decimation, capacity)
    {
    }

    virtual void Write(size_t scope, const Vector& value) = 0;
    virtual Vector ReadStoredVector(size_t scope, size_t index, size_t lod) const = 0;

    Vector ReadAcceptedVector(size_t scope, size_t index) const
    {
        assert(scope < this->m_scopes);
        return index >= this->m_oldestHistoryIndex && index < this->m_acceptedIndex
            ? ReadStoredVector(scope, index, 0) : Vector();
    }
};

template<size_t Size, size_t NumScopes>
struct ScopeWriterGeneric : ScopeWriterInput<Size>
{
    using Base = ScopeWriterInput<Size>;
    using Vector = typename Base::Vector;
    using Downsampler = FIRDownsamplerGeneric<float, Size, ScopeWriter::x_firTaps, ScopeWriter::x_levelRatio>;
    static constexpr size_t x_numVoices = Size;
    static constexpr size_t x_numScopes = NumScopes;

    struct Storage
    {
        std::vector<Vector> m_buffer[ScopeWriter::x_numLevels];
        Downsampler m_downsamplers[ScopeWriter::x_numLevels - 1];
        bool m_written = false;
    };

    Storage m_storage[NumScopes];

    ScopeWriterGeneric()
        : ScopeWriterGeneric(1)
    {
    }

    explicit ScopeWriterGeneric(size_t decimation)
        : ScopeWriterGeneric(decimation, ScopeWriter::x_baseCapacity / decimation)
    {
    }

    ScopeWriterGeneric(size_t decimation, size_t capacity)
        : Base(NumScopes, decimation, capacity)
    {
        for (Storage& storage : m_storage)
        {
            for (auto& buffer : storage.m_buffer)
            {
                buffer.resize(capacity);
            }
        }
    }

    void Store(size_t scope, size_t index, size_t lod, const Vector& value)
    {
        m_storage[scope].m_buffer[lod][index % this->m_capacity] = value;
    }

    void Write(size_t scope, const Vector& value) override
    {
        assert(scope < NumScopes);
        if (this->IsWriteSample())
        {
            Store(scope, this->m_index, 0, value);
            m_storage[scope].m_written = true;
        }
    }

    void CommitSample() override
    {
        for (size_t scope = 0; scope < NumScopes; ++scope)
        {
            Storage& storage = m_storage[scope];
            if (!storage.m_written)
            {
                Store(scope, this->m_index, 0, Vector());
            }

            storage.m_written = false;
        }
    }

    void Downsample(size_t firstIndex, size_t endIndex) override
    {
        for (size_t scope = 0; scope < NumScopes; ++scope)
        {
            Storage& storage = m_storage[scope];
            for (size_t sample = firstIndex; sample < endIndex; ++sample)
            {
                Vector value = ReadStoredVector(scope, sample, 0);
                size_t index = sample;
                for (size_t lod = 1; lod < ScopeWriter::x_numLevels; ++lod)
                {
                    Downsampler& downsampler = storage.m_downsamplers[lod - 1];
                    if (!downsampler.Process(value))
                    {
                        break;
                    }

                    value = downsampler.GetOutput();
                    index /= ScopeWriter::x_levelRatio;
                    Store(scope, index, lod, value);
                }
            }
        }
    }

    Vector ReadStoredVector(size_t scope, size_t index, size_t lod) const override
    {
        return m_storage[scope].m_buffer[lod][index % this->m_capacity];
    }

    float ReadStored(size_t scope, size_t voice, size_t index, size_t lod) const override
    {
        return m_storage[scope].m_buffer[lod][index % this->m_capacity][voice];
    }

    size_t SampleStorageBytes() const
    {
        return NumScopes * ScopeWriter::x_numLevels * this->m_capacity * sizeof(Vector);
    }
};

template<size_t Size>
struct ScopeWriterHolderGeneric
{
    using Vector = MultiChannelNumber<float, Size>;
    ScopeWriterInput<Size>* m_scopeWriter = nullptr;
    size_t m_scopeIx = 0;

    ScopeWriterHolderGeneric() = default;

    ScopeWriterHolderGeneric(ScopeWriterInput<Size>* scopeWriter, size_t scopeIx)
        : m_scopeWriter(scopeWriter)
        , m_scopeIx(scopeIx)
    {
    }

    void Write(const Vector& value)
    {
        if (m_scopeWriter)
        {
            m_scopeWriter->Write(m_scopeIx, value);
        }
    }

    bool IsWriteSample() const
    {
        return m_scopeWriter && m_scopeWriter->IsWriteSample();
    }

    void RecordStartAtVoice(size_t voice, double offset)
    {
        if (m_scopeWriter)
        {
            m_scopeWriter->RecordStart(m_scopeIx, voice, offset);
        }
    }

    void RecordEndAtVoice(size_t voice)
    {
        if (m_scopeWriter)
        {
            m_scopeWriter->RecordEnd(m_scopeIx, voice);
        }
    }

    void RecordStart()
    {
        RecordStartAtVoice(0, 0.0);
    }

    void RecordStart(double offset)
    {
        RecordStartAtVoice(0, offset);
    }

    void RecordEnd()
    {
        RecordEndAtVoice(0);
    }
};

using ScopeWriterHolder = ScopeWriterHolderGeneric<1>;

// Interpolate at sub-bin resolution and retain narrow peaks when one display
// point covers several FFT bins. Frequencies use cycles per engine sample.
//
template<size_t NumBins, typename MagnitudeAt>
float SpectrumPeakMagnitudeDb(float firstFrequency, float lastFrequency, MagnitudeAt magnitudeAt)
{
    auto position = [](float frequency)
    {
        return std::clamp(frequency * 2 * NumBins, 0.0f, static_cast<float>(NumBins - 1));
    };
    auto interpolate = [&](float index)
    {
        size_t first = static_cast<size_t>(index);
        float fraction = index - first;
        return magnitudeAt(first) * (1.0f - fraction)
            + magnitudeAt(std::min(first + 1, NumBins - 1)) * fraction;
    };

    float first = position(firstFrequency);
    float last = position(lastFrequency);
    float magnitude = std::max(interpolate(first), interpolate(last));
    for (size_t bin = static_cast<size_t>(std::ceil(first)); bin <= static_cast<size_t>(last); ++bin)
    {
        magnitude = std::max(magnitude, magnitudeAt(bin));
    }

    return 20.0f * std::log10(std::max(0.00001f, magnitude));
}

template<size_t Size>
struct WindowedFFTGeneric
{
    static constexpr size_t x_bits = 10 + SampleTimer::x_oversampleBits;
    using Transform = DiscreteFourierTransformGeneric<float, Size, x_bits>;
    using Table = typename Transform::Table;
    using Vector = typename Transform::Vector;
    using WorkTransform = DiscreteFourierTransformGeneric<float, Size == 1 ? 4 : Size, x_bits - (Size == 1 ? 2 : 0)>;

    struct Workspace
    {
        Table m_samples;
        typename WorkTransform::ComplexVector m_fft[WorkTransform::x_tableSize];
    };

    Transform m_dft;
    OPLowPassFilterGeneric<float, Size> m_filters[Transform::x_maxComponents];
    ScopeWriterInput<Size>* m_scopeWriter = nullptr;
    size_t m_scopeIx = 0;
    float m_gain = 2.0f;
    float m_smoothingFrequency = 4.0f / 60.0f;
    std::unique_ptr<Workspace> m_workspace = std::make_unique<Workspace>();

    WindowedFFTGeneric()
    {
        SetSmoothingFrequency(m_smoothingFrequency);
    }

    WindowedFFTGeneric(ScopeWriterInput<Size>* scopeWriter, size_t scopeIx)
        : WindowedFFTGeneric()
    {
        m_scopeWriter = scopeWriter;
        m_scopeIx = scopeIx;
    }

    WindowedFFTGeneric(const WindowedFFTGeneric& other)
        : WindowedFFTGeneric()
    {
        *this = other;
    }

    WindowedFFTGeneric& operator=(const WindowedFFTGeneric& other)
    {
        m_scopeWriter = other.m_scopeWriter;
        m_scopeIx = other.m_scopeIx;
        m_gain = other.m_gain;
        SetSmoothingFrequency(other.m_smoothingFrequency);
        return *this;
    }

    void SetSmoothingFrequency(float frequency)
    {
        m_smoothingFrequency = frequency;
        for (auto& filter : m_filters)
        {
            filter.SetAlphaFromNatFreq(frequency);
        }
    }

    static const std::array<float, Transform::x_tableSize>& HannWindow()
    {
        static const auto x_window = []
        {
            std::array<float, Transform::x_tableSize> window{};
            for (size_t i = 0; i < window.size(); ++i)
            {
                float phase = 2.0f * static_cast<float>(M_PI) * i / (window.size() - 1);
                window[i] = 0.5f * (1.0f - std::cos(phase));
            }

            return window;
        }();
        return x_window;
    }

    void Compute()
    {
        Compute(m_scopeIx);
    }

    void Compute(size_t scopeIx)
    {
        assert(m_scopeWriter);
        const auto& window = HannWindow();
        size_t endIndex = m_scopeWriter->m_acceptedIndex;
        Table& samples = m_workspace->m_samples;
        for (size_t sample = 0; sample < Transform::x_tableSize; ++sample)
        {
            samples.m_table[sample] = endIndex + sample < Transform::x_tableSize ? Vector()
                : m_scopeWriter->ReadAcceptedVector(scopeIx, endIndex + sample - Transform::x_tableSize) * window[sample];
        }

        if constexpr (Size == 1)
        {
            // Four interleaved subsequences use the four SIMD lanes. Their
            // quarter-length transforms are combined with full-length twiddles,
            // so even a mono display uses SIMD without duplicating the signal.
            //
            using PackedVector = typename WorkTransform::Vector;
            using PackedComplex = typename WorkTransform::ComplexVector;
            constexpr size_t x_quarter = WorkTransform::x_tableSize;
            for (size_t sample = 0; sample < x_quarter; ++sample)
            {
                PackedVector input(samples.m_table[4 * sample][0], samples.m_table[4 * sample + 1][0],
                    samples.m_table[4 * sample + 2][0], samples.m_table[4 * sample + 3][0]);
                m_workspace->m_fft[sample] = PackedComplex(input, PackedVector());
            }

            WorkTransform::FFT(m_workspace->m_fft, x_quarter);
            for (size_t bin = 0; bin < Transform::x_maxComponents; ++bin)
            {
                PackedComplex twiddle;
                for (size_t lane = 0; lane < 4; ++lane)
                {
                    size_t index = (Transform::x_tableSize - bin * lane % Transform::x_tableSize) % Transform::x_tableSize;
                    twiddle.SetChannel(lane, MathGeneric<x_bits>::RootOfUnityByIndex(index));
                }

                PackedComplex result = m_workspace->m_fft[bin % x_quarter] * twiddle;
                m_dft.m_components[bin] = std::complex<float>(result.real().Sum(), result.imag().Sum())
                    / static_cast<float>(Transform::x_tableSize);
            }
        }
        else
        {
            m_dft.Transform(samples, m_workspace->m_fft);
        }

        for (size_t bin = 0; bin < Transform::x_maxComponents; ++bin)
        {
            Vector magnitude = Vector::Max(m_dft.m_components[bin].Abs() * m_gain, Vector::Splat(0.00001f));
            m_filters[bin].Process(magnitude);
        }
    }

    float GetMagDb(size_t lane, float frequency)
    {
        return GetPeakMagDb(lane, frequency, frequency);
    }

    float GetMagDb(float frequency)
    {
        static_assert(Size == 1);
        return GetMagDb(0, frequency);
    }

    float GetPeakMagDb(size_t lane, float firstFrequency, float lastFrequency)
    {
        assert(lane < Size);
        return SpectrumPeakMagnitudeDb<Transform::x_maxComponents>(firstFrequency, lastFrequency,
            [this, lane](size_t bin)
            {
                return m_filters[bin].m_output[lane];
            });
    }

    float GetPeakMagDb(float firstFrequency, float lastFrequency)
    {
        static_assert(Size == 1);
        return GetPeakMagDb(0, firstFrequency, lastFrequency);
    }
};

using WindowedFFT = WindowedFFTGeneric<1>;
using TrioWindowedFFT = WindowedFFTGeneric<3>;

struct QuadWindowedFFT : WindowedFFTGeneric<4>
{
    QuadWindowedFFT()
    {
        m_gain = 1.0f;
        SetSmoothingFrequency(1.0f / 60.0f);
    }

    QuadWindowedFFT(ScopeWriterInput<4>* scopeWriter, size_t scopeIx)
        : QuadWindowedFFT()
    {
        m_scopeWriter = scopeWriter;
        m_scopeIx = scopeIx;
    }
};

struct ScopeReader
{
    const ScopeWriter* m_scopeWriter;
    size_t m_voiceIx;
    size_t m_scopeIx;
    size_t m_numXSamples;
    
    double m_startIndex;
    double m_transferXSample;
    double m_transferIndex;
    double m_postTransferIndex;

    float m_startY;
    float m_prevStartY;

    bool m_empty;
    size_t m_lod = 0;

    ScopeReader(
        const ScopeWriter* scopeWriter,
        size_t voiceIx, 
        size_t scopeIx,
        size_t numXSamples,
        size_t numCycles)
        : m_scopeWriter(scopeWriter)
        , m_voiceIx(voiceIx)
        , m_scopeIx(scopeIx)
        , m_numXSamples(numXSamples)
        , m_startIndex(0.0)
        , m_transferXSample(0.0)
        , m_transferIndex(0.0)
        , m_postTransferIndex(0.0)
        , m_startY(0.0f)
        , m_prevStartY(0.0f)
        , m_empty(false)
    {
        size_t lastStartIndexIndex = m_scopeWriter->GetAcceptedMarkers(m_scopeIx, m_voiceIx).m_startIndexIndex;
        m_startIndex = m_scopeWriter->GetAcceptedMarkers(m_scopeIx, m_voiceIx).m_startIndices[(lastStartIndexIndex - 1) % ScopeWriter::x_numStartIndices];
        double prevStartIndex = m_scopeWriter->GetAcceptedMarkers(m_scopeIx, m_voiceIx).m_startIndices[(lastStartIndexIndex - 2) % ScopeWriter::x_numStartIndices];

        double acceptedIndex = static_cast<double>(m_scopeWriter->m_acceptedIndex);
        if (acceptedIndex == 0.0 || m_numXSamples == 0)
        {
            m_empty = true;
            return;
        }

        m_transferIndex = acceptedIndex - 1.0;
        if (m_startIndex < 0.0 || m_startIndex > m_transferIndex)
        {
            m_empty = true;
            return;
        }

        double endIndex = m_scopeWriter->GetAcceptedMarkers(m_scopeIx, m_voiceIx).m_endIndices[(lastStartIndexIndex - 1) % ScopeWriter::x_numStartIndices];
        if (m_startIndex < endIndex)
        {
            m_transferIndex = endIndex - 1;
        }

        double prevNumSamples = m_startIndex - prevStartIndex;
        double prevEndIndex = m_scopeWriter->GetAcceptedMarkers(m_scopeIx, m_voiceIx).m_endIndices[(lastStartIndexIndex - 2) % ScopeWriter::x_numStartIndices];
        if (prevStartIndex < prevEndIndex)
        {
            prevNumSamples = prevEndIndex - prevStartIndex;
        }

        if (prevNumSamples <= 0.0 || prevNumSamples <= m_transferIndex - m_startIndex)
        {
            // No transfer.
            //
            m_transferXSample = m_numXSamples;
        }
        else
        {
            // Assume number of pre transfer samples is same as previous cycle length.
            //
            double wayThrough = (m_transferIndex - m_startIndex) / prevNumSamples;
            m_transferXSample = wayThrough * m_numXSamples;
            m_postTransferIndex = prevStartIndex + (m_transferIndex - m_startIndex);
        }

        double oldestRequired = m_transferXSample < m_numXSamples
            ? std::min(m_startIndex, m_postTransferIndex) : m_startIndex;
        m_lod = m_scopeWriter->GetLod(acceptedIndex - 1.0 - oldestRequired, static_cast<size_t>(acceptedIndex));
        m_empty = m_lod == ScopeWriter::x_numLevels;
        if (!m_empty)
        {
            m_startY = m_scopeWriter->ReadDisplay(m_scopeIx, m_voiceIx, m_startIndex, m_lod);
            if (prevStartIndex >= m_scopeWriter->OldestIndex(m_lod))
            {
                m_prevStartY = m_scopeWriter->ReadDisplay(m_scopeIx, m_voiceIx, prevStartIndex, m_lod);
            }
        }
    }

    float Get(double sample)
    {
        if (m_empty)
        {
            return 0;
        }
        else if (sample < m_transferXSample || m_transferXSample == static_cast<double>(m_numXSamples))
        {
            double wayThrough = sample / m_transferXSample;
            double index = m_startIndex + wayThrough * (m_transferIndex - m_startIndex);
            float value = m_scopeWriter->ReadDisplay(m_scopeIx, m_voiceIx, index, m_lod);
            return value;
        }
        else
        {
            double wayThrough = (sample - m_transferXSample) / (static_cast<double>(m_numXSamples) - m_transferXSample);
            double index = m_postTransferIndex + wayThrough * (m_startIndex - m_postTransferIndex);
            float value = m_scopeWriter->ReadDisplay(m_scopeIx, m_voiceIx, index, m_lod);
            return value;
        }
    }
};

struct ScopeReaderFactory
{
    ScopeWriter* m_scopeWriter;
    size_t m_voiceIx;
    size_t m_scopeIx;
    size_t m_numXSamples;
    size_t m_numCycles;

    ScopeReaderFactory(
        ScopeWriter* scopeWriter, 
        size_t voiceIx, 
        size_t scopeIx, 
        size_t numXSamples, 
        size_t numCycles)
        : m_scopeWriter(scopeWriter)
        , m_voiceIx(voiceIx)
        , m_scopeIx(scopeIx)
        , m_numXSamples(numXSamples)
        , m_numCycles(numCycles)
    {
    }

    void SetVoiceIx(size_t voiceIx)
    {
        m_voiceIx = voiceIx;
    }

    void SetScopeIx(size_t scopeIx)
    {
        m_scopeIx = scopeIx;
    }

    ScopeReader Create()
    {
        return ScopeReader(m_scopeWriter, m_voiceIx, m_scopeIx, m_numXSamples, m_numCycles);
    }
};

template<typename T>
struct PeriodicEventWriter
{
    static constexpr size_t x_numEvents = 16384;
    T m_events[x_numEvents];
    std::atomic<ssize_t> m_index;
    std::atomic<ssize_t> m_lastStartIndex;

    PeriodicEventWriter()
        : m_index(0)
        , m_lastStartIndex(0)
    {
    }

    void RecordStartIndex()
    {
        m_lastStartIndex.store(m_index.load());
    }

    ssize_t RecordEvent(T& value)
    {
        ssize_t index = m_index.load();
        m_events[index % x_numEvents] = value;
        m_index.store(m_index.load() + 1);
        return index;
    }

    T& Get(ssize_t index)
    {
        return m_events[index % x_numEvents];
    }
};

struct NonagonNoteWriter
{
    static constexpr size_t x_numVoices = 9;
    using TimbreHistory = SampleEventHistory<3, 256>;
    // Keep immutable per-voice events for UI queries. The display note ring also
    // edits note ends and copies held notes at cycle boundaries.
    //
    TimbreHistory m_timbreHistory[x_numVoices];
    struct EventData
    {
        uint8_t m_voiceIx;
        float m_voltPerOct;
        float m_startPosition;
        float m_endPosition;
        float m_timbre[3];
        size_t m_startSample = 0;

        EventData()
            : m_voiceIx(0)
            , m_voltPerOct(0)
            , m_startPosition(0)
            , m_endPosition(-1)
            , m_timbre{0, 0, 0}
        {
        }
    };

    static constexpr size_t x_numEvents = PeriodicEventWriter<EventData>::x_numEvents;

    PeriodicEventWriter<EventData> m_events;

    ssize_t m_lastNoteIndex[x_numVoices];
    std::atomic<float> m_minPitch;
    std::atomic<float> m_maxPitch;
    std::atomic<float> m_lastPeriodMinPitch;
    std::atomic<float> m_lastPeriodMaxPitch;
    std::atomic<float> m_curPosition;

    NonagonNoteWriter()
    {
        for (size_t i = 0; i < x_numVoices; ++i)
        {
            m_lastNoteIndex[i] = -1;
        }
    }

    void RecordNote(EventData& eventData)
    {
        TimbreHistory::Event event;
        event.m_sample = eventData.m_startSample;
        for (size_t i = 0; i < 3; ++i)
        {
            event.m_values[i] = eventData.m_timbre[i];
        }

        m_timbreHistory[eventData.m_voiceIx].Record(event);
        RecordNoteSegment(eventData);
    }

    void RecordNoteSegment(EventData& eventData)
    {
        bool isFirstEvent = m_events.m_lastStartIndex.load() == m_events.m_index.load();
        if (m_lastNoteIndex[eventData.m_voiceIx] != -1)
        {
            RecordNoteEnd(eventData.m_voiceIx, eventData.m_startPosition);
        }

        m_lastNoteIndex[eventData.m_voiceIx] = m_events.RecordEvent(eventData);

        if (isFirstEvent)
        {
            m_minPitch.store(eventData.m_voltPerOct);
            m_maxPitch.store(eventData.m_voltPerOct);
        }
        else
        {
            m_minPitch.store(std::min(m_minPitch.load(), eventData.m_voltPerOct));
            m_maxPitch.store(std::max(m_maxPitch.load(), eventData.m_voltPerOct));
        }
    }

    void RecordNoteEnd(size_t voiceIx, float startPosition)
    {
        ssize_t currentIndex = m_events.m_index.load();
        if (m_lastNoteIndex[voiceIx] != -1 && currentIndex - m_lastNoteIndex[voiceIx] < x_numEvents)
        {
            assert(m_events.m_events[m_lastNoteIndex[voiceIx] % x_numEvents].m_voiceIx == voiceIx);
            m_events.m_events[m_lastNoteIndex[voiceIx] % x_numEvents].m_endPosition = startPosition;
        }

        m_lastNoteIndex[voiceIx] = -1;
    }

    void RecordStartIndex()
    {
        m_lastPeriodMinPitch.store(m_minPitch.load());
        m_lastPeriodMaxPitch.store(m_maxPitch.load());

        m_events.RecordStartIndex();

        for (size_t i = 0; i < x_numVoices; ++i)
        {
            if (m_lastNoteIndex[i] != -1 && m_events.m_index.load() - m_lastNoteIndex[i] < x_numEvents)
            {
                EventData eventData = m_events.m_events[m_lastNoteIndex[i] % x_numEvents];
                RecordNoteEnd(i, 1.0);
                eventData.m_startPosition = 0;
                RecordNoteSegment(eventData);
            }
        }
    }

    void SetCurPosition(float position)
    {
        m_curPosition.store(position);
    }

    float GetCurPosition()
    {
        return m_curPosition.load();
    }

    EventData& Get(ssize_t index)
    {
        return m_events.Get(index);
    }

    ssize_t GetIndex()
    {
        return m_events.m_index.load();
    }

    ssize_t GetLastStartIndex()
    {
        return m_events.m_lastStartIndex.load();
    }
    
    float GetMinPitch()
    {
        return std::min(m_minPitch.load(), m_lastPeriodMinPitch.load());
    }

    float GetMaxPitch()
    {
        return std::max(m_maxPitch.load(), m_lastPeriodMaxPitch.load());
    }
};
