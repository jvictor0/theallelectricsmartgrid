#include "Bench.hpp"
#include "NaiveReuppingLadder.hpp"
#include "ConservativeReuppingLadder.hpp"
#include <cstring>

using Vector = MultiChannelNumber<float, 3>;

struct Parameters
{
    Vector m_cutoff;
    Vector m_resonance;
    Vector m_gain;
    Vector m_input;
};

struct Corpus
{
    static constexpr size_t x_period = 32768;
    std::array<Parameters, x_period> m_values;

    Corpus()
    {
        for (size_t i = 0; i < x_period; ++i)
        {
            for (size_t lane = 0; lane < 3; ++lane)
            {
                const float phase = static_cast<float>(i) * static_cast<float>(lane + 1) * 0.0003f;
                auto& p = m_values[i];
                p.m_cutoff[lane] = 0.014f * static_cast<float>(lane + 1) * (1 + 0.5f * std::sin(phase));
                p.m_resonance[lane] = 0.2f + 0.2f * static_cast<float>(lane) + 0.12f * std::sin(phase);
                p.m_gain[lane] = 0.25f + 4.75f * (0.5f + 0.5f * std::sin(phase * 0.3f));
                p.m_input[lane] = 0.3f * std::sin(static_cast<float>(i * (lane + 1)) * 0.017f);
            }
        }
    }
};

enum class Mode
{
    Process,
    Unchanged,
    Drive,
    All,
    Coefficients,
};

const char* ModeName(Mode mode)
{
    switch (mode)
    {
        case Mode::Process: return "process";
        case Mode::Unchanged: return "unchanged";
        case Mode::Drive: return "drive";
        case Mode::All: return "all";
        case Mode::Coefficients: return "coefficients";
    }

    throw std::runtime_error("Unknown benchmark mode");
}

template<bool Conservative>
struct Filter
{
    using Core = std::conditional_t<Conservative, TrioConservativeReuppingLadder, TrioNaiveReuppingLadder>;
    Core m_core{};

    void Tune(const Vector& cutoff, const Vector& resonance, const Vector& gain)
    {
        m_core.SetCutoffResonanceAndGain(cutoff, resonance, gain);
    }

    Vector Process(const Vector& x)
    {
        return m_core.Process(x);
    }
};

template<bool Conservative, Mode Work>
[[gnu::noinline]] void Loop(Filter<Conservative>& filter, const Corpus& corpus,
    std::vector<Vector>& outputs, size_t samples)
{
    const auto& fixed = corpus.m_values[0];
    for (size_t n = 0; n < samples; ++n)
    {
        const size_t i = n % Corpus::x_period;
        const auto& p = corpus.m_values[i];
        if constexpr (Work == Mode::Unchanged)
        {
            filter.Tune(fixed.m_cutoff, fixed.m_resonance, fixed.m_gain);
        }
        else if constexpr (Work == Mode::Drive)
        {
            filter.Tune(fixed.m_cutoff, fixed.m_resonance, p.m_gain);
        }
        else if constexpr (Work == Mode::All || Work == Mode::Coefficients)
        {
            filter.Tune(p.m_cutoff, p.m_resonance, p.m_gain);
        }

        if constexpr (Work == Mode::Coefficients)
        {
            // Every prepared field remains observable; don't let the compiler
            // remove computations just because this case omits audio processing.
            //
            asm volatile("" : : "g"(&filter.m_core) : "memory");
        }
        else
        {
            outputs[i] = filter.Process(p.m_input);
        }
    }

    asm volatile("" : : "g"(outputs.data()), "g"(&filter.m_core) : "memory");
}

template<bool Conservative, Mode Work>
uint64_t MeasureCase(const Corpus& corpus, size_t samples, size_t round)
{
    Filter<Conservative> filter;
    const auto& fixed = corpus.m_values[0];
    filter.Tune(fixed.m_cutoff, fixed.m_resonance, fixed.m_gain);
    std::vector<Vector> output(Corpus::x_period);
    Loop<Conservative, Work>(filter, corpus, output, Corpus::x_period);
    const double cpuStart = CPUSeconds();
    const auto wallStart = std::chrono::steady_clock::now();
    Loop<Conservative, Work>(filter, corpus, output, samples);
    const auto wallEnd = std::chrono::steady_clock::now();
    const double cpu = CPUSeconds() - cpuStart;
    const double wall = std::chrono::duration<double>(wallEnd - wallStart).count();

    uint64_t hash = 14695981039346656037ULL;
    for (const auto& value : output)
    {
        for (size_t lane = 0; lane < 3; ++lane)
        {
            Require(std::isfinite(value[lane]), "Nonfinite ladder benchmark output");
            uint32_t bits;
            std::memcpy(&bits, &value[lane], sizeof(bits));
            hash = (hash ^ bits) * 1099511628211ULL;
        }
    }

    std::printf("{\"implementation\":\"%s\",\"mode\":\"%s\",\"round\":%zu,\"samples\":%zu,"
        "\"cpu_ms\":%.6f,\"wall_ms\":%.6f,\"hash\":\"%016llx\"}\n",
        Conservative ? "conservative" : "naive", ModeName(Work), round, samples,
        cpu * 1000, wall * 1000, static_cast<unsigned long long>(hash));
    std::fflush(stdout);
    return hash;
}

template<Mode Work>
void Pair(const Corpus& corpus, size_t samples, size_t round)
{
    if (round % 2 == 0)
    {
        MeasureCase<false, Work>(corpus, samples, round);
        MeasureCase<true, Work>(corpus, samples, round);
    }
    else
    {
        MeasureCase<true, Work>(corpus, samples, round);
        MeasureCase<false, Work>(corpus, samples, round);
    }
}

int main(int argc, char** argv)
{
    try
    {
        Require(argc == 3, "Usage: perf_ladder ROUNDS SAMPLES");
        const int rounds = Integer(argv[1]);
        const int samples = Integer(argv[2]);
        Require(rounds > 0 && rounds <= 100, "Rounds must be in 1..100");
        Require(samples >= static_cast<int>(Corpus::x_period), "Samples must cover the output corpus");
        PrepareCPU();
        const auto corpus = std::make_unique<Corpus>();
        for (int round = 0; round < rounds; ++round)
        {
            for (int index = 0; index < 5; ++index)
            {
                switch ((index + round) % 5)
                {
                    case 0: Pair<Mode::Process>(*corpus, samples, round); break;
                    case 1: Pair<Mode::Unchanged>(*corpus, samples, round); break;
                    case 2: Pair<Mode::Drive>(*corpus, samples, round); break;
                    case 3: Pair<Mode::All>(*corpus, samples, round); break;
                    case 4: Pair<Mode::Coefficients>(*corpus, samples, round); break;
                }
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
