// infra_sample_timer.cpp -- SampleTimer rate domains and conversions, plus a
// guard against hand-written sample-rate literals in private/src.
//

#include "doctest.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "../support/GlobalEnv.hpp"
#include "SampleTimer.hpp"

#ifndef SMARTGRID_REPO_ROOT
#define SMARTGRID_REPO_ROOT "."
#endif

static_assert(SampleTimer::x_sampleRate == SampleTimer::x_hostSampleRate * SampleTimer::x_oversample);
static_assert(SampleTimer::HzToCycPerSamp(static_cast<double>(SampleTimer::x_sampleRate)) == 1.0);
static_assert(SampleTimer::HzToCycPerHostSamp(static_cast<double>(SampleTimer::x_hostSampleRate)) == 1.0);
static_assert(SampleTimer::CycPerSampToHz(1.0) == static_cast<double>(SampleTimer::x_sampleRate));
static_assert(SampleTimer::CycPerHostSampToHz(1.0) == static_cast<double>(SampleTimer::x_hostSampleRate));
static_assert(SampleTimer::SecondsToSamples(1.0) == static_cast<double>(SampleTimer::x_sampleRate));
static_assert(SampleTimer::SecondsToHostSamples(1.0) == static_cast<double>(SampleTimer::x_hostSampleRate));
static_assert(SampleTimer::SecondsToCycPerSamp(1.0) == SampleTimer::x_samplePeriod);
static_assert(SampleTimer::SecondsToCycPerHostSamp(1.0) == SampleTimer::x_hostSamplePeriod);

DOCTEST_TEST_CASE("SampleTimer: host frames group x_oversample engine samples")
{
    GlobalEnv::ResetPerTest();

    for (size_t i = 0; i < 64; ++i)
    {
        size_t sample = SampleTimer::GetSample();
        DOCTEST_CHECK(SampleTimer::GetHostSample() == sample / SampleTimer::x_oversample);
        DOCTEST_CHECK(SampleTimer::GetSubSampleIndex() == sample % SampleTimer::x_oversample);
        DOCTEST_CHECK(SampleTimer::IsHostFrame() == (sample % SampleTimer::x_oversample == 0));
        SampleTimer::IncrementSample();
    }
}

DOCTEST_TEST_CASE("SampleTimer: no hand-written 48 kHz literals in private/src")
{
    // Rate-dependent constants must go through the SampleTimer helpers so the
    // engine rate can change in one place.
    //
    std::filesystem::path sourceDir = std::filesystem::path(SMARTGRID_REPO_ROOT) / "private" / "src";
    DOCTEST_REQUIRE(std::filesystem::is_directory(sourceDir));

    size_t violations = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(sourceDir))
    {
        if (!entry.is_regular_file() || entry.path().filename() == "SampleTimer.hpp")
        {
            continue;
        }

        std::string extension = entry.path().extension().string();
        if (extension != ".hpp" && extension != ".cpp" && extension != ".h")
        {
            continue;
        }

        std::ifstream file(entry.path());
        std::string line;
        size_t lineNumber = 0;
        while (std::getline(file, line))
        {
            ++lineNumber;
            if (line.find("48000") != std::string::npos)
            {
                DOCTEST_INFO(entry.path().string() << ":" << lineNumber << ": " << line);
                DOCTEST_CHECK(false);
                ++violations;
            }
        }
    }

    DOCTEST_CHECK(violations == 0);
}
