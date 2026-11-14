#include "Cases.hpp"
#include "../DensityCase.hpp"
#include "../ResponsesCase.hpp"
#include <sstream>
#include <stdexcept>

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
            RunTransferResponses}
    };
    return x_cases;
}
