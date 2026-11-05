#pragma once

#include <atomic>
#include <cmath>
#include "Filter.hpp"
#include "Saturator.hpp"
#include "StereoUtils.hpp"
#include "QuadUtils.hpp"
#include "SampleTimer.hpp"

// Levels update once per host frame from the frame's mean square, peak
// magnitude and mean reduction, so smoothing and the peak hold are in host
// frames. Saturation runs every engine sample.
//
template <typename Number, size_t Size>
struct MeterGeneric
{
    using Vector = MultiChannelNumber<Number, Size>;
    static constexpr size_t x_oversample = SampleTimer::x_oversample;

    static constexpr Number x_smoothingAlphaDown = static_cast<Number>(0.0002);
    static constexpr Number x_smoothingAlphaUp = static_cast<Number>(0.0008);
    static constexpr Number x_smoothingAlphaReduction = static_cast<Number>(0.0005);
    static constexpr Number x_peakSmoothingAlpha = static_cast<Number>(0.00005);
    static constexpr Number x_floor = static_cast<Number>(0.00000000001);

    static constexpr size_t x_peakHistorySize = SampleTimer::x_hostSampleRate;

    Vector m_rms;
    Vector m_peak;
    Vector m_reduction;
    using Counts = MultiChannelNumber<uint32_t, Size>;
    Counts m_samplesSincePeak;
    Vector m_frameSquares;
    Vector m_frameMagnitude;
    Vector m_frameReduction;
    size_t m_frameSamples;

    MeterGeneric()
        : m_rms(Vector::Splat(0))
        , m_peak(Vector::Splat(0))
        , m_reduction(Vector::Splat(1))
        , m_samplesSincePeak{}
        , m_frameSquares(Vector::Splat(0))
        , m_frameMagnitude(Vector::Splat(0))
        , m_frameReduction(Vector::Splat(0))
        , m_frameSamples(0)
    {
    }

    void Accumulate(const Vector& input)
    {
        m_frameSquares += input * input;
        m_frameMagnitude = Vector::Max(m_frameMagnitude, input.Abs());
    }

    bool EndSample()
    {
        ++m_frameSamples;
        if (m_frameSamples < x_oversample)
        {
            return false;
        }

        m_frameSamples = 0;
        return true;
    }

    void ProcessHostFrame()
    {
        Vector inputSquared = m_frameSquares / static_cast<Number>(x_oversample);
        Vector magnitude = m_frameMagnitude;
        m_frameSquares = Vector::Splat(0);
        m_frameMagnitude = Vector::Splat(0);
        Vector alpha = Vector::IfLess(m_rms, inputSquared,
            Vector::Splat(x_smoothingAlphaUp), Vector::Splat(x_smoothingAlphaDown));
        m_rms = Vector::FusedMultiplyAdd(inputSquared, alpha, m_rms * (Vector::Splat(1) - alpha));

        auto newPeak = m_peak.Load() < magnitude.Load();
        Vector decayed = Vector::FusedMultiplyAdd(magnitude, Vector::Splat(x_peakSmoothingAlpha),
            m_peak * (1 - x_peakSmoothingAlpha));
        Vector held = Vector::IfLess(m_samplesSincePeak.template Convert<Number>(),
            Vector::Splat(x_peakHistorySize), m_peak, decayed);
        m_peak = Vector::FromVector(newPeak ? magnitude.Load() : held.Load());
        Counts next = Counts::IfLess(m_samplesSincePeak, Counts::Splat(x_peakHistorySize),
            m_samplesSincePeak + 1, m_samplesSincePeak);
        auto reset = __builtin_convertvector(newPeak, typename Counts::Vector);
        m_samplesSincePeak = Counts::FromVector(reset ? Counts().Load() : next.Load());
    }

    void Process(const Vector& input)
    {
        Accumulate(input);
        if (EndSample())
        {
            ProcessHostFrame();
        }
    }

    void ProcessReduction(const Vector& reduction)
    {
        m_reduction = m_reduction * (1 - x_smoothingAlphaReduction) + reduction * x_smoothingAlphaReduction;
    }

    Vector ProcessAndSaturate(const Vector& input, Vector* reduction)
    {
        Vector output = TanhSaturatorGeneric<Number, Size, false>::Tanh(input);
        Vector floor = Vector::Splat(x_floor);
        *reduction = Vector::Max(floor, output.Abs()) / Vector::Max(floor, input.Abs());
        Accumulate(output);
        m_frameReduction += *reduction;
        if (EndSample())
        {
            ProcessHostFrame();
            ProcessReduction(m_frameReduction / static_cast<Number>(x_oversample));
            m_frameReduction = Vector::Splat(0);
        }

        return output;
    }

    Vector ProcessAndSaturate(const Vector& input)
    {
        Vector reduction;
        return ProcessAndSaturate(input, &reduction);
    }

    static Number GetRMSDbFS(Number rms)
    {
        return 10 * std::log10(std::max(x_floor, rms));
    }

    static Number GetPeakDbFS(Number peak)
    {
        return 20 * std::log10(std::max(x_floor, peak));
    }
};

// Publishes a meter's per-channel levels to the UI thread. The channel-free
// getters read the only channel of a mono reader.
//
template <typename Number, size_t Size>
struct MeterReaderGeneric
{
    using Meter = MeterGeneric<Number, Size>;

    Meter* m_meter;

    std::atomic<Number> m_rms[Size];
    std::atomic<Number> m_peak[Size];
    std::atomic<Number> m_reduction[Size];

    MeterReaderGeneric()
        : MeterReaderGeneric(nullptr)
    {
    }

    MeterReaderGeneric(Meter* meter)
        : m_meter(meter)
    {
        for (size_t i = 0; i < Size; ++i)
        {
            m_rms[i].store(0);
            m_peak[i].store(0);
            m_reduction[i].store(0);
        }
    }

    void SetMeter(Meter* meter)
    {
        m_meter = meter;
    }

    void Process()
    {
        for (size_t i = 0; i < Size; ++i)
        {
            m_rms[i].store(m_meter->m_rms[i]);
            m_peak[i].store(m_meter->m_peak[i]);
            m_reduction[i].store(m_meter->m_reduction[i]);
        }
    }

    Number GetRMSDbFS(size_t i) const
    {
        return Meter::GetRMSDbFS(m_rms[i].load());
    }

    Number GetPeakDbFS(size_t i) const
    {
        return Meter::GetPeakDbFS(m_peak[i].load());
    }

    Number GetReductionDbFS(size_t i) const
    {
        return Meter::GetPeakDbFS(m_reduction[i].load());
    }

    Number GetRMSLinear(size_t i) const
    {
        return m_rms[i].load();
    }

    Number GetPeakLinear(size_t i) const
    {
        return m_peak[i].load();
    }

    Number GetRMSDbFSNormalized(size_t i) const
    {
        return std::max<Number>(0, (60 + GetRMSDbFS(i)) / 60);
    }

    Number GetPeakDbFSNormalized(size_t i) const
    {
        return std::max<Number>(0, (60 + GetPeakDbFS(i)) / 60);
    }

    Number GetReductionDbFSNormalized(size_t i) const
    {
        return std::max<Number>(0, (12 + GetReductionDbFS(i)) / 12);
    }

    Number GetRMSDbFS() const
    {
        static_assert(Size == 1, "channel-free getters require one channel");
        return GetRMSDbFS(0);
    }

    Number GetPeakDbFS() const
    {
        static_assert(Size == 1, "channel-free getters require one channel");
        return GetPeakDbFS(0);
    }

    Number GetReductionDbFS() const
    {
        static_assert(Size == 1, "channel-free getters require one channel");
        return GetReductionDbFS(0);
    }

    Number GetRMSLinear() const
    {
        static_assert(Size == 1, "channel-free getters require one channel");
        return GetRMSLinear(0);
    }

    Number GetPeakLinear() const
    {
        static_assert(Size == 1, "channel-free getters require one channel");
        return GetPeakLinear(0);
    }

    Number GetRMSDbFSNormalized() const
    {
        static_assert(Size == 1, "channel-free getters require one channel");
        return GetRMSDbFSNormalized(0);
    }

    Number GetPeakDbFSNormalized() const
    {
        static_assert(Size == 1, "channel-free getters require one channel");
        return GetPeakDbFSNormalized(0);
    }

    Number GetReductionDbFSNormalized() const
    {
        static_assert(Size == 1, "channel-free getters require one channel");
        return GetReductionDbFSNormalized(0);
    }
};

using Meter = MeterGeneric<float, 1>;
using StereoMeter = MeterGeneric<float, 2>;
using TrioMeter = MeterGeneric<float, 3>;
using QuadMeter = MeterGeneric<float, 4>;

using MeterReader = MeterReaderGeneric<float, 1>;
using StereoMeterReader = MeterReaderGeneric<float, 2>;
using TrioMeterReader = MeterReaderGeneric<float, 3>;
using QuadMeterReader = MeterReaderGeneric<float, 4>;
