#include "Bench.hpp"
#include "TunedDecayComb.hpp"
#include "SmartGridBuildInfo.hpp"
#include <cstring>

template<size_t Size>
struct Parameters
{
    MultiChannelNumber<double, Size> m_frequency;
    MultiChannelNumber<double, Size> m_decay;
    MultiChannelNumber<double, Size> m_damp;
    MultiChannelNumber<float, Size> m_input;
};

template<size_t Size>
struct Corpus
{
    static constexpr size_t x_period = 32768;
    static constexpr double x_sampleRate = 192000;
    std::array<Parameters<Size>, x_period> m_values;

    Corpus()
    {
        for (size_t n = 0; n < x_period; ++n)
        {
            auto& p = m_values[n];
            for (size_t lane = 0; lane < Size; ++lane)
            {
                const double phase = 2 * std::acos(-1.0) * n / x_period;
                p.m_frequency[lane] = (110.0 * (lane + 1) / x_sampleRate) *
                    (1 + 0.3 * std::sin(phase + lane));
                p.m_decay[lane] = (lane % 2 == 0 ? 0.25 : 0.75) + 0.12 * std::sin(phase * 2 + lane);
                p.m_damp[lane] = 0.5 + 0.45 * std::cos(phase + lane);
                p.m_input[lane] = static_cast<float>(0.12 * std::sin(phase * (113 + 67 * lane)) +
                    0.04 * std::cos(phase * (397 + 97 * lane)));
            }
        }
    }
};

enum class Mode
{
    Process,
    Unchanged,
    One,
    All,
};

const char* ModeName(Mode mode)
{
    switch (mode)
    {
        case Mode::Process: return "process";
        case Mode::Unchanged: return "unchanged";
        case Mode::One: return "one";
        case Mode::All: return "all";
    }

    throw std::runtime_error("Unknown comb benchmark mode");
}

template<size_t Size, Mode Work>
[[gnu::noinline]] void Loop(TunedDecayCombGeneric<Size>& filter, const Corpus<Size>& corpus,
    std::vector<MultiChannelNumber<float, Size>>& outputs, size_t samples)
{
    const auto& fixed = corpus.m_values[0];
    for (size_t n = 0; n < samples; ++n)
    {
        const size_t i = n % Corpus<Size>::x_period;
        const auto& p = corpus.m_values[i];
        if constexpr (Work == Mode::Unchanged)
        {
            filter.SetParams(fixed.m_frequency, fixed.m_decay, fixed.m_damp, Corpus<Size>::x_sampleRate);
        }
        else if constexpr (Work == Mode::One)
        {
            auto frequency = fixed.m_frequency;
            auto decay = fixed.m_decay;
            auto damp = fixed.m_damp;
            frequency[0] = p.m_frequency[0];
            decay[0] = p.m_decay[0];
            damp[0] = p.m_damp[0];
            filter.SetParams(frequency, decay, damp, Corpus<Size>::x_sampleRate);
        }
        else if constexpr (Work == Mode::All)
        {
            filter.SetParams(p.m_frequency, p.m_decay, p.m_damp, Corpus<Size>::x_sampleRate);
        }

        outputs[i] = filter.Process(p.m_input);
    }

    asm volatile("" : : "g"(outputs.data()), "g"(&filter) : "memory");
}

template<size_t Size, Mode Work>
void MeasureCase(const Corpus<Size>& corpus, size_t samples, size_t round)
{
    auto filter = std::make_unique<TunedDecayCombGeneric<Size>>();
    const auto& fixed = corpus.m_values[0];
    filter->SetParams(fixed.m_frequency, fixed.m_decay, fixed.m_damp, Corpus<Size>::x_sampleRate);
    std::vector<MultiChannelNumber<float, Size>> output(Corpus<Size>::x_period);
    Loop<Size, Work>(*filter, corpus, output, Corpus<Size>::x_period);
    const double cpuStart = CPUSeconds();
    const auto wallStart = std::chrono::steady_clock::now();
    Loop<Size, Work>(*filter, corpus, output, samples);
    const auto wallEnd = std::chrono::steady_clock::now();
    const double cpu = CPUSeconds() - cpuStart;
    const double wall = std::chrono::duration<double>(wallEnd - wallStart).count();

    uint64_t hash = 14695981039346656037ULL;
    for (const auto& value : output)
    {
        for (size_t lane = 0; lane < Size; ++lane)
        {
            Require(std::isfinite(value[lane]), "Nonfinite comb benchmark output");
            uint32_t bits;
            std::memcpy(&bits, &value[lane], sizeof(bits));
            hash = (hash ^ bits) * 1099511628211ULL;
        }
    }

    std::printf("{\"implementation\":\"generated\",\"git_commit\":\"%s\",\"lanes\":%zu,\"mode\":\"%s\",\"round\":%zu,\"samples\":%zu,"
        "\"sample_rate\":%.0f,\"cpu_ms\":%.6f,\"wall_ms\":%.6f,\"ns_per_frame\":%.6f,"
        "\"ms_per_audio_second\":%.6f,\"hash\":\"%016llx\"}\n",
        SmartGridBuildInfo::x_gitCommitSha, Size, ModeName(Work), round, samples,
        Corpus<Size>::x_sampleRate, cpu * 1000, wall * 1000, cpu * 1e9 / samples,
        cpu * 1000 * Corpus<Size>::x_sampleRate / samples, static_cast<unsigned long long>(hash));
    std::fflush(stdout);
}

template<size_t Size>
void Width(const Corpus<Size>& corpus, size_t samples, size_t round)
{
    for (size_t index = 0; index < 4; ++index)
    {
        switch ((index + round) % 4)
        {
            case 0: MeasureCase<Size, Mode::Process>(corpus, samples, round); break;
            case 1: MeasureCase<Size, Mode::Unchanged>(corpus, samples, round); break;
            case 2: MeasureCase<Size, Mode::One>(corpus, samples, round); break;
            case 3: MeasureCase<Size, Mode::All>(corpus, samples, round); break;
        }
    }
}

int main(int argc, char** argv)
{
    try
    {
        Require(argc == 3, "Usage: perf_comb ROUNDS SAMPLES");
        const int rounds = Integer(argv[1]);
        const int samples = Integer(argv[2]);
        Require(rounds > 0 && rounds <= 100, "Rounds must be in 1..100");
        Require(samples >= static_cast<int>(Corpus<2>::x_period), "Samples must cover the output corpus");
        PrepareCPU();
        const auto stereo = std::make_unique<Corpus<2>>();
        const auto quad = std::make_unique<Corpus<4>>();
        for (int round = 0; round < rounds; ++round)
        {
            if (round % 2 == 0)
            {
                Width(*stereo, samples, round);
                Width(*quad, samples, round);
            }
            else
            {
                Width(*quad, samples, round);
                Width(*stereo, samples, round);
            }
        }

        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
