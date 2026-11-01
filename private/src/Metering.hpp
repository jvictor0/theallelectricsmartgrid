#pragma once

#include <atomic>
#include <cmath>
#include "Filter.hpp"
#include "Saturator.hpp"
#include "StereoUtils.hpp"
#include "QuadUtils.hpp"
#include "SampleTimer.hpp"

template <typename Number, size_t Size>
struct MeterGeneric
{
    using Vector = MultiChannelNumber<Number, Size>;

    static constexpr Number x_smoothingAlphaDown = static_cast<Number>(0.0002);
    static constexpr Number x_smoothingAlphaUp = static_cast<Number>(0.0008);
    static constexpr Number x_smoothingAlphaReduction = static_cast<Number>(0.0005);
    static constexpr Number x_peakSmoothingAlpha = static_cast<Number>(0.00005);
    static constexpr Number x_floor = static_cast<Number>(0.00000000001);

    static constexpr size_t x_peakHistorySize = SampleTimer::x_hostSampleRate;

    Vector m_rms;
    Vector m_peak;
    Vector m_reduction;
    size_t m_samplesSincePeak[Size];

    MeterGeneric()
        : m_rms(Vector::Splat(0))
        , m_peak(Vector::Splat(0))
        , m_reduction(Vector::Splat(1))
        , m_samplesSincePeak{}
    {
    }

    void Process(const Vector& input)
    {
        Vector inputSquared = input * input;
        Vector magnitude = input.Abs();
        for (size_t i = 0; i < Size; ++i)
        {
            Number alpha = m_rms[i] < inputSquared[i] ? x_smoothingAlphaUp : x_smoothingAlphaDown;
            m_rms[i] = m_rms[i] * (1 - alpha) + inputSquared[i] * alpha;

            if (m_peak[i] < magnitude[i])
            {
                m_peak[i] = magnitude[i];
                m_samplesSincePeak[i] = 0;
            }
            else if (m_samplesSincePeak[i] < x_peakHistorySize)
            {
                m_samplesSincePeak[i]++;
            }
            else
            {
                m_peak[i] = m_peak[i] * (1 - x_peakSmoothingAlpha) + magnitude[i] * x_peakSmoothingAlpha;
            }
        }
    }

    void ProcessReduction(const Vector& reduction)
    {
        m_reduction = m_reduction * (1 - x_smoothingAlphaReduction) + reduction * x_smoothingAlphaReduction;
    }

    Vector ProcessAndSaturate(const Vector& input, Vector* reduction)
    {
        Vector output = TanhSaturatorGeneric<Number, Size, false>::Tanh(input);
        Process(output);
        Vector floor = Vector::Splat(x_floor);
        *reduction = Vector::Max(floor, output.Abs()) / Vector::Max(floor, input.Abs());
        ProcessReduction(*reduction);
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
using QuadMeter = MeterGeneric<float, 4>;

using MeterReader = MeterReaderGeneric<float, 1>;
using StereoMeterReader = MeterReaderGeneric<float, 2>;
using QuadMeterReader = MeterReaderGeneric<float, 4>;
