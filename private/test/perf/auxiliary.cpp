#include "AHD.hpp"
#include "AdaptiveWaveTable.hpp"
#include "Bench.hpp"
#include "Metering.hpp"
#include <cstring>

constexpr size_t x_rate = SampleTimer::x_sampleRate;

template <int Mode>
struct Envelopes
{
    TheoryOfTimeBase m_clock;
    AHDGeneric<3> m_core;
    AHDGeneric<3>::Input m_input;

    AHD::Input& Input(size_t lane)
    {
        return m_input.m_channels[lane];
    }

    Envelopes()
    {
        for (size_t lane = 0; lane < 3; ++lane)
        {
            auto& input = Input(lane);
            input.m_theoryOfTime = &m_clock;
            input.m_attackIncrement = 1.0f / (x_rate * (0.03f + 0.01f * lane));
            input.m_decayIncrement = 1.0f / (x_rate * (0.15f + 0.02f * lane));
            input.m_holdLoops = 0.1;
            input.m_envelopePeriodSamples = x_rate;
            input.m_amplitude = 0.7f + lane * 0.1f;
        }
    }

    std::array<float, 4> Process(size_t n)
    {
        double phase = static_cast<double>(n) / x_rate;
        m_clock.m_sample.m_modulatedPhase = phase;
        for (size_t lane = 0; lane < 3; ++lane)
        {
            auto& input = Input(lane);
            if constexpr (Mode != 0)
            {
                if (n % (x_rate / 2) == 0)
                {
                    input.m_trig = true;
                    input.m_startGlobalPhase = phase;
                }
            }

            if constexpr (Mode == 2)
            {
                input.m_release = n % (x_rate / 2) == x_rate / 20;
            }
        }

        std::array<float, 4> result{};
        auto output = m_core.Process(m_input);
        for (size_t lane = 0; lane < 3; ++lane)
        {
            result[lane] = output[lane];
        }

        return result;
    }
};

struct Empty
{
    Signals m_signals{x_rate};

    std::array<float, 4> Process(size_t n)
    {
        auto result = m_signals.m_audio[n % m_signals.m_period];
        result[3] = 0;
        return result;
    }
};

template <size_t Size>
struct Meters
{
    using Core = MeterGeneric<float, Size>;
    Signals m_signals{x_rate};
    Core m_core[3 / Size];

    std::array<float, 4> Process(size_t n)
    {
        std::array<float, 4> result{};
        for (size_t group = 0; group < 3 / Size; ++group)
        {
            typename Core::Vector input;
            typename Core::Vector reduction;
            for (size_t lane = 0; lane < Size; ++lane)
            {
                input[lane] = m_signals.m_audio[n % m_signals.m_period][group * Size + lane];
            }

            auto output = m_core[group].ProcessAndSaturate(input, &reduction);
            for (size_t lane = 0; lane < Size; ++lane)
            {
                result[group * Size + lane] =
                    output[lane] + reduction[lane] * 0.001f + m_core[group].m_rms[lane] * 0.001f;
            }
        }

        return result;
    }
};

template <size_t OutputBits>
struct Inverse
{
    using DFT = DiscreteFourierTransformGeneric<float, 4, 12>;
    using Table = BasicWaveTableGeneric<float, 4, OutputBits>;
    DFT m_dft;
    Table m_output;
    typename DFT::ComplexVector m_workspace[1 << OutputBits];
    Inverse()
    {
        for (size_t c = 0; c < 4; ++c)
        {
            for (size_t k = 1; k < DFT::x_maxComponents; ++k)
            {
                std::complex<float> value(0.03f * std::cos(k * (0.13f + c * 0.07f)) / k,
                                          0.03f * std::sin(k * (0.21f + c * 0.03f)) / k);
                m_dft.m_components[k].real()[c] = value.real();
                m_dft.m_components[k].imag()[c] = value.imag();
            }
        }
    }

    __attribute__((noinline)) void Process()
    {
        m_dft.InverseTransform(m_output, DFT::x_maxComponents, m_workspace);
        asm volatile("" : : "g"(&m_output) : "memory");
    }

    float Value(size_t c, size_t i)
    {
        return m_output.m_table[i][c];
    }
};

template <size_t OutputBits>
void MeasureInverse(const ComponentOptions& options)
{
    auto work = std::make_unique<Inverse<OutputBits>>();
    const size_t count = std::max<size_t>(1, static_cast<size_t>(options.m_seconds * 1000));
    for (size_t i = 0; i < 32; ++i)
    {
        work->Process();
    }

    double start = CPUSeconds();
    for (size_t i = 0; i < count; ++i)
    {
        work->Process();
    }

    double cpu = CPUSeconds() - start;
    double sum = 0;
    for (size_t c = 0; c < 4; ++c)
    {
        for (size_t i = 0; i < (1 << OutputBits); ++i)
        {
            sum += std::abs(work->Value(c, i));
        }
    }

    Require(std::isfinite(sum), "Nonfinite inverse transform output");
    std::printf("{\"kind\":\"%s\",\"mode\":0,\"iterations\":%zu,\"cpu_seconds\":%.9f,\"us_per_transform\":%."
                "6f,\"checksum\":%.9g,\"finite\":true}\n",
                options.m_kind, count, cpu, cpu * 1e6 / count, sum);
}

int main(int argc, char** argv)
{
    return BenchmarkMain(argc, argv, [](const ComponentOptions& options)
    {
        std::string kind(options.m_kind);
        if (kind == "ifft" || kind == "ifft_upsample")
        {
            Require(options.m_lanes == 4 && options.m_mode == 0 && !options.m_dump,
                    "IFFT requires LANES 4, MODE 0, no output dump");
            if (kind == "ifft")
            {
                MeasureInverse<12>(options);
            }
            else
            {
                MeasureInverse<14>(options);
            }
        }
        else if (kind == "meter")
        {
            Require((options.m_lanes == 1 || options.m_lanes == 3) && options.m_mode == 0,
                    "Meter requires LANES 1 or 3, MODE 0");
            if (options.m_lanes == 3)
            {
                Meters<3> work;
                Measure(work, options.m_kind, 0, x_rate, options.m_seconds, 1, options.m_dump);
            }
            else
            {
                Meters<1> work;
                Measure(work, options.m_kind, 0, x_rate, options.m_seconds, 1, options.m_dump);
            }
        }
        else if (kind == "empty")
        {
            Require(options.m_lanes == 3 && options.m_mode == 0, "Empty requires LANES 3, MODE 0");
            Empty work;
            Measure(work, options.m_kind, 0, x_rate, options.m_seconds, 1, options.m_dump);
        }
        else
        {
            Require(kind == "ahd" && options.m_lanes == 3 && options.m_mode <= 2,
                    "AHD requires LANES 3, MODE 0..2");
            if (options.m_mode == 0)
            {
                Envelopes<0> work;
                Measure(work, options.m_kind, 0, x_rate, options.m_seconds, 1, options.m_dump);
            }
            else if (options.m_mode == 1)
            {
                Envelopes<1> work;
                Measure(work, options.m_kind, 1, x_rate, options.m_seconds, 1, options.m_dump);
            }
            else
            {
                Envelopes<2> work;
                Measure(work, options.m_kind, 2, x_rate, options.m_seconds, 1, options.m_dump);
            }
        }
    });
}
