#include "../Bench.hpp"
#include "../../support/DirectFormIButterworth.hpp"
#include "Oversample.hpp"

namespace
{
    using Quad = MultiChannelNumber<float, 4>;
    constexpr size_t x_ratio = SampleTimer::x_oversample;
    constexpr size_t x_rate = SampleTimer::x_sampleRate;
    constexpr size_t x_period = 256;

    template<size_t Order>
    struct LegacyDecimator
    {
        DirectFormIButterworth<float, 4, Order> m_filter;
        Quad m_output;

        LegacyDecimator()
        {
            m_filter.SetCyclesPerSample(0.40f / static_cast<float>(x_ratio));
        }

        Quad Process(const Quad& input)
        {
            m_output = m_filter.Process(input);
            return m_output;
        }
    };

    template<typename Filter>
    struct Source
    {
        Quad m_output;
        Filter m_filter;
    };

    template<typename Filter, size_t Count>
    [[gnu::noinline]] void Run(std::array<Source<Filter>, Count>& sources,
        const std::array<std::array<Quad, Count>, x_period>& inputs, size_t samples)
    {
        for (size_t n = 0; n < samples; ++n)
        {
            const auto& frame = inputs[n % x_period];
            for (size_t i = 0; i < Count; ++i)
            {
                auto& source = sources[i];
                source.m_output = source.m_filter.Process(frame[i]);
            }

            asm volatile("" : : "g"(sources.data()) : "memory");
        }
    }

    template<typename Filter, size_t Count>
    void Measure(const std::array<std::array<Quad, Count>, x_period>& inputs, int order,
        const char* implementation, int round, size_t samples, std::ostream& output)
    {
        std::array<Source<Filter>, Count> sources{};
        Run(sources, inputs, x_rate / 2);
        const double cpuStart = CPUSeconds();
        const auto wallStart = std::chrono::steady_clock::now();
        Run(sources, inputs, samples);
        const auto wallEnd = std::chrono::steady_clock::now();
        const double cpu = CPUSeconds() - cpuStart;
        const double wall = std::chrono::duration<double>(wallEnd - wallStart).count();
        for (const auto& source : sources)
        {
            for (int lane = 0; lane < 4; ++lane)
            {
                Require(std::isfinite(source.m_output[lane]), "Nonfinite Butterworth output");
            }
        }

        output << "{\"order\":" << order << ",\"implementation\":\"" << implementation
            << "\",\"round\":" << round << ",\"sources\":" << Count << ",\"samples\":" << samples
            << ",\"engine_rate\":" << x_rate << ",\"cpu_ms_per_audio_second\":" << cpu * 1000 * x_rate / samples
            << ",\"wall_ms_per_audio_second\":" << wall * 1000 * x_rate / samples << "}\n";
    }

    template<size_t Order, size_t Count>
    void Compare(int repeats, size_t samples, std::ostream& output)
    {
        using Generated = DownsamplerGeneric<float, 4, Order, x_ratio>;
        std::array<std::array<Quad, Count>, x_period> inputs;
        for (size_t n = 0; n < x_period; ++n)
        {
            for (size_t source = 0; source < Count; ++source)
            {
                for (int lane = 0; lane < 4; ++lane)
                {
                    const double phase = 2 * std::acos(-1.0) * static_cast<double>(n) / x_period;
                    inputs[n][source][lane] = static_cast<float>(0.5 + 0.3 * std::sin(phase * (lane + 1) + source)
                        + 0.1 * std::cos(phase * 83 + lane));
                }
            }
        }

        for (int round = 0; round < repeats; ++round)
        {
            if (round % 2 == 0)
            {
                Measure<LegacyDecimator<Order>>(inputs, Order, "direct_form_i", round, samples, output);
                Measure<Generated>(inputs, Order, "lean_transposed_direct_form_ii", round, samples, output);
            }
            else
            {
                Measure<Generated>(inputs, Order, "lean_transposed_direct_form_ii", round, samples, output);
                Measure<LegacyDecimator<Order>>(inputs, Order, "direct_form_i", round, samples, output);
            }
        }
    }
}

void RunButterworth(const std::vector<std::string>& arguments, std::ostream& output)
{
    Require(arguments.size() <= 2, "Butterworth: [REPEATS] [AUDIO_SECONDS_PER_CASE]");
    const int repeats = arguments.empty() ? 9 : Integer(arguments[0].c_str());
    const double seconds = arguments.size() < 2 ? 8 : PositiveSeconds(arguments[1].c_str());
    const size_t samples = static_cast<size_t>(seconds * x_rate);
    Require(repeats > 0 && samples > 0, "Butterworth repeats and sample count must be positive");

    PrepareCPU();
    Compare<2, 33>(repeats, samples, output);
    Compare<8, 8>(repeats, samples, output);
}
