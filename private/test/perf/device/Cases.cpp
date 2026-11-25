#include "Cases.hpp"
#include "../DensityCase.hpp"
#include "../ResponsesCase.hpp"
#include "../ResynthesisWorkload.hpp"
#include "../ReverbWorkload.hpp"
#include <random>
#include <sstream>
#include <stdexcept>

void RunButterworth(const std::vector<std::string>& arguments, std::ostream& output);

void RunInverter(const std::vector<std::string>& arguments, std::ostream& output)
{
    std::vector<std::string> words{"inverter"};
    if (arguments.empty())
    {
        words.insert(words.end(), {"current-performance", "0", "16", "3"});
    }
    else
    {
        words.insert(words.end(), arguments.begin(), arguments.end());
    }

    if (words[1] != "current-performance" && words[1] != "tape-performance"
        && words[1] != "write-performance" && words[1] != "self-test")
    {
        throw std::runtime_error("Inverter: current-performance|tape-performance|write-performance "
            "SETTING PERIOD_BITS [REPEATS] [CYCLES], or self-test");
    }

    std::vector<char*> argv;
    for (auto& word : words)
    {
        argv.push_back(word.data());
    }

    std::ostringstream errors;
    if (RunDensity(static_cast<int>(argv.size()), argv.data(), output, errors) != 0)
    {
        throw std::runtime_error(errors.str());
    }
}

void RunTransferResponses(const std::vector<std::string>& arguments, std::ostream& output)
{
    std::vector<std::string> words{"responses"};
    words.insert(words.end(), arguments.begin(), arguments.end());
    std::vector<char*> argv;
    for (auto& word : words)
    {
        argv.push_back(word.data());
    }

    std::ostringstream errors;
    if (RunResponses(static_cast<int>(argv.size()), argv.data(), output, errors) != 0)
    {
        throw std::runtime_error(errors.str());
    }
}

void RunQuadReverb(const std::vector<std::string>& arguments, std::ostream& output)
{
    Require(arguments.size() <= 2, "Reverb: [REPEATS] [SECONDS_PER_CASE]");
    const int repeats = arguments.empty() ? 7 : Integer(arguments[0].c_str());
    const double seconds = arguments.size() < 2 ? 12 : PositiveSeconds(arguments[1].c_str());
    Require(repeats > 0, "Reverb repeats must be positive");

    using Run = void (*)(double, const char*, std::ostream&);
    const std::array<Run, 12> cases{
        MeasureReverb<0, ReverbImplementation::Legacy>,
        MeasureReverb<0, ReverbImplementation::WanderingEverySample>,
        MeasureReverb<0, ReverbImplementation::Wandering>,
        MeasureReverb<1, ReverbImplementation::Legacy>,
        MeasureReverb<1, ReverbImplementation::WanderingEverySample>,
        MeasureReverb<1, ReverbImplementation::Wandering>,
        MeasureReverb<2, ReverbImplementation::Legacy>,
        MeasureReverb<2, ReverbImplementation::WanderingEverySample>,
        MeasureReverb<2, ReverbImplementation::Wandering>,
        MeasureReverb<3, ReverbImplementation::Legacy>,
        MeasureReverb<3, ReverbImplementation::WanderingEverySample>,
        MeasureReverb<3, ReverbImplementation::Wandering>};
    std::vector<Run> jobs;
    for (int repeat = 0; repeat < repeats; ++repeat)
    {
        jobs.insert(jobs.end(), cases.begin(), cases.end());
    }

    std::mt19937 random(1000);
    std::shuffle(jobs.begin(), jobs.end(), random);

    PrepareCPU();
    SampleTimer::Init(512);
    for (Run run : jobs)
    {
        run(seconds, nullptr, output);
    }
}

void RunResynthesis(const std::vector<std::string>& arguments, std::ostream& output)
{
    Require(arguments.size() <= 2, "Resynthesis: [REPEATS] [AUDIO_SECONDS_PER_CASE]");
    const int repeats = arguments.empty() ? 5 : Integer(arguments[0].c_str());
    const double seconds = arguments.size() < 2 ? 8 : PositiveSeconds(arguments[1].c_str());
    Require(repeats > 0, "Resynthesis repeats must be positive");

    PrepareCPU();
    for (int repeat = 0; repeat < repeats; ++repeat)
    {
        MeasureResynthesis<1>(seconds, output);
        MeasureResynthesis<3>(seconds, output);
        MeasureResynthesis<4>(seconds, output);
    }
}

const std::vector<BenchmarkCase>& BenchmarkCases()
{
    static const std::vector<BenchmarkCase> x_cases
    {
        {"inverter", "Isolate inverse write/read cost across warp settings. Writes at 192 kHz; "
            "four reads per 1024-host-frame hop. Excludes trajectory generation and allocation; "
            "discards the first half-second. Metrics are CPU ms/audio second and per-hop times. "
            "tape-performance compares the original plain-double tape at 192 kHz against current; "
            "write-performance compares the pre-optimization inverse against current. "
            "These timings do not establish equivalent correctness.", RunInverter},
        {"responses", "Sampled transfer response evaluation at 321, 641 and 1024 frequency columns. "
            "32 captured states, all lanes, cached frequency preparation and production percentile selection. "
            "Compares magnitude overrides with complex magnitude; reports CPU/wall ms per whole plot. "
            "Excludes JUCE path drawing, FFT, audio and concurrent publication. Arguments: [REPEATS] [SECONDS_PER_CASE].",
            RunTransferResponses},
        {"reverb", "Legacy, sample-rate and framed wandering reverbs at 192 kHz with shared traces. "
            "Reports thread CPU ms per audio second for all four control modes after a two-second warmup. "
            "Arguments: [REPEATS] [SECONDS_PER_CASE].", RunQuadReverb},
        {"butterworth", "Direct form I and Lean transposed direct form II at the engine rate. "
            "Times 33 quad second-order decimators and eight quad eighth-order decimators on identical inputs. "
            "Arguments: [REPEATS] [AUDIO_SECONDS_PER_CASE].", RunButterworth},
        {"resynthesis", "Process Hann-windowed 48 kHz grains through the full phase vocoder, including packed FFTs, "
            "pitch shift, unison, and the engine-rate synthesis frame. Reports CPU and wall timing for one, three, and four lanes. "
            "Arguments: [REPEATS] [AUDIO_SECONDS_PER_CASE].", RunResynthesis}
    };
    return x_cases;
}
