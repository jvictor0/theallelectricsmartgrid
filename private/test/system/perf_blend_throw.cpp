// Reproduces the CPU spike from throwing the scene blend fader. Skipped by default;
// run with: smartgrid_tests --test-case='perf: blend*' --no-skip
//
#include "doctest.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <libproc.h>
#include <sys/resource.h>
#include <unistd.h>

#include "../support/SynthRig.hpp"

namespace
{

std::string ReadPatchFixture()
{
    std::ifstream file(std::string(SMARTGRID_REPO_ROOT) + "/private/test/fixtures_espace_etale_2026-06-24T10-40-13.json");
    std::ostringstream out;
    out << file.rdbuf();
    return out.str();
}

void SetStateTree(SmartGrid::BankedEncoderCell* cell)
{
    cell->SetState();
    for (size_t i = 0; i < cell->m_modulators.m_numActiveModulators; ++i)
    {
        SetStateTree(cell->GetModulator(i));
    }

    for (size_t i = 0; i < cell->m_modulators.m_numActiveGestures; ++i)
    {
        SetStateTree(cell->m_modulators.GetGesture(i));
    }
}

constexpr size_t x_hostBlock = 128;
constexpr size_t x_messageInterval = 48;
constexpr size_t x_throwSamples = 48000;
constexpr double x_blockBudgetUs = 1.0e6 * x_hostBlock / 48000.0;

struct BlockStats
{
    double m_meanUs;
    double m_maxUs;
    size_t m_changedSamples;
};

// Runs x_throwSamples in host-sized blocks. When throwing, the blend fader is swept
// 0 -> 1 -> 0 with a ParamSet14 every x_messageInterval samples (1 kHz, like a
// 14-bit MIDI fader moved fast).
//
BlockStats RunThrow(synthrig::SynthRig& rig, bool throwFader)
{
    using Clock = std::chrono::steady_clock;
    double totalUs = 0;
    double maxUs = 0;
    size_t blocks = 0;
    size_t changedSamples = 0;
    for (size_t start = 0; start < x_throwSamples; start += x_hostBlock)
    {
        Clock::time_point t0 = Clock::now();
        for (size_t offset = 0; offset < x_hostBlock; ++offset)
        {
            size_t sample = start + offset;
            if (throwFader && sample % x_messageInterval == 0)
            {
                float phase = static_cast<float>(sample) / static_cast<float>(x_throwSamples);
                rig.SetBlend(phase < 0.5f ? 2.0f * phase : 2.0f - 2.0f * phase);
            }

            rig.RunSamples(1);
            changedSamples += rig.Internal().m_context.m_sceneManager.m_changed ? 1 : 0;
        }

        double us = std::chrono::duration<double, std::micro>(Clock::now() - t0).count();
        totalUs += us;
        maxUs = std::max(maxUs, us);
        ++blocks;
        rig.ClearOutput();
    }

    return BlockStats{totalUs / static_cast<double>(blocks), maxUs, changedSamples};
}

void Report(const char* name, const BlockStats& stats)
{
    std::printf(
        "%-28s mean %7.1f us/block (%5.1f%%)  max %7.1f us/block (%5.1f%%)  changed samples %zu\n",
        name,
        stats.m_meanUs,
        100.0 * stats.m_meanUs / x_blockBudgetUs,
        stats.m_maxUs,
        100.0 * stats.m_maxUs / x_blockBudgetUs,
        stats.m_changedSamples);
}

enum class SceneChange
{
    None,
    Full,
    BlendOnly
};

double TimeEncoderProcessUs(synthrig::SynthRig& rig, SceneChange sceneChange)
{
    using Clock = std::chrono::steady_clock;
    constexpr size_t x_iterations = 4800;
    auto& bankBank = rig.Internal().m_squiggleBoy.m_encoders.m_encoderBankBank;
    auto& sceneManager = rig.Internal().m_context.m_sceneManager;
    Clock::time_point t0 = Clock::now();
    for (size_t i = 0; i < x_iterations; ++i)
    {
        if (sceneChange == SceneChange::Full)
        {
            bankBank.HandleChangedSceneManager();
        }

        sceneManager.m_changed = sceneChange == SceneChange::BlendOnly;
        bankBank.Process();
    }

    sceneManager.m_changed = false;
    double us = std::chrono::duration<double, std::micro>(Clock::now() - t0).count();
    return us / static_cast<double>(x_iterations);
}

// Cost of re-reading every scene value in the tree, with no recompute: the part a
// cached base + blend * delta would replace.
//
double TimeSetStateTreeUs(synthrig::SynthRig& rig)
{
    using Clock = std::chrono::steady_clock;
    constexpr size_t x_iterations = 4800;
    auto& bankBank = rig.Internal().m_squiggleBoy.m_encoders.m_encoderBankBank;
    Clock::time_point t0 = Clock::now();
    for (size_t i = 0; i < x_iterations; ++i)
    {
        bankBank.ForEachNamedEncoder(
            [](size_t, SmartGrid::BankedEncoderCell* cell)
            {
                SetStateTree(cell);
            });
    }

    double us = std::chrono::duration<double, std::micro>(Clock::now() - t0).count();
    return us / static_cast<double>(x_iterations);
}

size_t CountTreeCells(SmartGrid::BankedEncoderCell* cell);

void ReportEncoderCosts(synthrig::SynthRig& rig)
{
    std::printf("encoder Process, no scene change:         %.2f us/sample\n", TimeEncoderProcessUs(rig, SceneChange::None));
    std::printf("encoder Process, full scene refresh:      %.2f us/sample\n", TimeEncoderProcessUs(rig, SceneChange::Full));
    std::printf("encoder Process, blend-only change:       %.2f us/sample\n", TimeEncoderProcessUs(rig, SceneChange::BlendOnly));
    std::printf("SetState over the whole tree only:        %.2f us/sample\n", TimeSetStateTreeUs(rig));

    size_t numEncoders = 0;
    size_t numCells = 0;
    rig.Internal().m_squiggleBoy.m_encoders.m_encoderBankBank.ForEachNamedEncoder(
        [&numEncoders, &numCells](size_t, SmartGrid::BankedEncoderCell* cell)
        {
            ++numEncoders;
            numCells += CountTreeCells(cell);
        });

    std::printf("named encoders: %zu, cells in tree: %zu, sizeof(BankedEncoderCell) %zu\n",
        numEncoders,
        numCells,
        sizeof(SmartGrid::BankedEncoderCell));
}

size_t CountTreeCells(SmartGrid::BankedEncoderCell* cell)
{
    size_t count = 1;
    for (size_t i = 0; i < cell->m_modulators.m_numActiveModulators; ++i)
    {
        count += CountTreeCells(cell->GetModulator(i));
    }

    for (size_t i = 0; i < cell->m_modulators.m_numActiveGestures; ++i)
    {
        count += CountTreeCells(cell->m_modulators.GetGesture(i));
    }

    return count;
}

}

DOCTEST_TEST_CASE("perf: blend fader throw" * doctest::skip())
{
    synthrig::SynthRig rig;
    rig.StartSequencer();
    rig.RunSeconds(0.5);

    BlockStats idle = RunThrow(rig, false);
    BlockStats glide = RunThrow(rig, true);

    rig.Internal().m_blendSlew.m_filter.m_alpha = 1.0f;
    BlockStats noGlide = RunThrow(rig, true);
    rig.Internal().m_blendSlew.SetAlphaFromNatFreq(TheNonagonSquiggleBoyInternal::x_inputSlewCutoff);

    std::printf("host block %zu samples, budget %.1f us\n", x_hostBlock, x_blockBudgetUs);
    Report("idle", idle);
    Report("throw, 500 Hz glide", glide);
    Report("throw, no glide", noGlide);

    ReportEncoderCosts(rig);

    DOCTEST_CHECK(idle.m_changedSamples == 0);
}

DOCTEST_TEST_CASE("perf: blend fader throw, espace etale patch" * doctest::skip())
{
    std::string patch = ReadPatchFixture();
    DOCTEST_REQUIRE_FALSE(patch.empty());

    synthrig::SynthRig rig;
    rig.RunFrames(2);
    DOCTEST_REQUIRE(rig.LoadPatch(patch));
    rig.StartSequencer();
    rig.RunSeconds(0.5);

    BlockStats idle = RunThrow(rig, false);
    BlockStats glide = RunThrow(rig, true);

    std::printf("espace etale, host block %zu samples, budget %.1f us\n", x_hostBlock, x_blockBudgetUs);
    Report("idle", idle);
    Report("throw, 500 Hz glide", glide);

    rig.Internal().SetBlendFactor(0.5f);
    rig.RunSamples(x_hostBlock);
    ReportEncoderCosts(rig);
}

// Spins the blend-only encoder pass on the espace etale patch so a sampling profiler
// can attach. Seconds come from SMARTGRID_PROFILE_SECONDS (default 10).
//
DOCTEST_TEST_CASE("perf: blend-only encoder pass profile loop" * doctest::skip())
{
    std::string patch = ReadPatchFixture();
    DOCTEST_REQUIRE_FALSE(patch.empty());

    synthrig::SynthRig rig;
    rig.RunFrames(2);
    DOCTEST_REQUIRE(rig.LoadPatch(patch));
    rig.Internal().SetBlendFactor(0.5f);
    rig.RunSamples(x_hostBlock);

    const char* secondsEnv = std::getenv("SMARTGRID_PROFILE_SECONDS");
    double seconds = secondsEnv ? std::atof(secondsEnv) : 10.0;
    auto& bankBank = rig.Internal().m_squiggleBoy.m_encoders.m_encoderBankBank;
    auto& sceneManager = rig.Internal().m_context.m_sceneManager;
    using Clock = std::chrono::steady_clock;
    Clock::time_point end = Clock::now() + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(seconds));
    size_t passes = 0;
    while (Clock::now() < end)
    {
        for (size_t i = 0; i < 4800; ++i)
        {
            sceneManager.m_blendFactor = 0.25f + 0.5f * static_cast<float>(i) / 4800.0f;
            sceneManager.m_changed = true;
            bankBank.Process();
        }

        passes += 4800;
    }

    sceneManager.m_changed = false;
    std::printf("profile loop: %zu passes, %.2f us/pass\n", passes, seconds * 1.0e6 / static_cast<double>(passes));
}

namespace
{

struct CpuCounters
{
    uint64_t m_instructions;
    uint64_t m_cycles;
};

CpuCounters ReadCpuCounters()
{
    rusage_info_v4 info{};
    proc_pid_rusage(getpid(), RUSAGE_INFO_V4, reinterpret_cast<rusage_info_t*>(&info));
    return CpuCounters{info.ri_instructions, info.ri_cycles};
}

void ReportCounters(const char* name, synthrig::SynthRig& rig, SceneChange sceneChange, size_t numCells)
{
    constexpr size_t x_passes = 48000;
    auto& bankBank = rig.Internal().m_squiggleBoy.m_encoders.m_encoderBankBank;
    auto& sceneManager = rig.Internal().m_context.m_sceneManager;
    CpuCounters before = ReadCpuCounters();
    for (size_t i = 0; i < x_passes; ++i)
    {
        if (sceneChange == SceneChange::Full)
        {
            bankBank.HandleChangedSceneManager();
        }

        sceneManager.m_changed = sceneChange == SceneChange::BlendOnly;
        bankBank.Process();
    }

    sceneManager.m_changed = false;
    CpuCounters after = ReadCpuCounters();
    double instructions = static_cast<double>(after.m_instructions - before.m_instructions) / x_passes;
    double cycles = static_cast<double>(after.m_cycles - before.m_cycles) / x_passes;
    std::printf(
        "%-12s %8.0f instr/pass %8.0f cycles/pass  IPC %.2f  per cell: %5.1f instr %5.1f cycles\n",
        name,
        instructions,
        cycles,
        instructions / cycles,
        instructions / static_cast<double>(numCells),
        cycles / static_cast<double>(numCells));
}

// Times one blend-only pass with the caches hot, and again after sweeping a buffer
// larger than L2 and the system level cache, so every cell load starts cold.
//
void ReportColdCache(synthrig::SynthRig& rig)
{
    using Clock = std::chrono::steady_clock;
    constexpr size_t x_passes = 2000;
    constexpr size_t x_evictBytes = 64 * 1024 * 1024;
    std::vector<uint8_t> evict(x_evictBytes, 1);
    auto& bankBank = rig.Internal().m_squiggleBoy.m_encoders.m_encoderBankBank;
    auto& sceneManager = rig.Internal().m_context.m_sceneManager;
    double hotUs = 0;
    double coldUs = 0;
    uint64_t sink = 0;
    for (size_t i = 0; i < x_passes; ++i)
    {
        sceneManager.m_changed = true;
        Clock::time_point t0 = Clock::now();
        bankBank.Process();
        hotUs += std::chrono::duration<double, std::micro>(Clock::now() - t0).count();

        for (size_t j = 0; j < x_evictBytes; j += 64)
        {
            evict[j] = static_cast<uint8_t>(evict[j] + 1);
            sink += evict[j];
        }

        sceneManager.m_changed = true;
        t0 = Clock::now();
        bankBank.Process();
        coldUs += std::chrono::duration<double, std::micro>(Clock::now() - t0).count();
    }

    sceneManager.m_changed = false;
    std::printf(
        "blend-only pass: hot %.2f us, cold cache %.2f us (sink %llu)\n",
        hotUs / x_passes,
        coldUs / x_passes,
        static_cast<unsigned long long>(sink & 1));
}

}

// Instructions, cycles and IPC per encoder pass from proc_pid_rusage, plus a hot
// versus cold cache comparison, to tell whether the tree walk is memory bound.
//
DOCTEST_TEST_CASE("perf: blend-only encoder pass counters" * doctest::skip())
{
    std::string patch = ReadPatchFixture();
    DOCTEST_REQUIRE_FALSE(patch.empty());

    synthrig::SynthRig rig;
    rig.RunFrames(2);
    DOCTEST_REQUIRE(rig.LoadPatch(patch));
    rig.Internal().SetBlendFactor(0.5f);
    rig.RunSamples(x_hostBlock);

    size_t numCells = 0;
    rig.Internal().m_squiggleBoy.m_encoders.m_encoderBankBank.ForEachNamedEncoder(
        [&numCells](size_t, SmartGrid::BankedEncoderCell* cell)
        {
            numCells += CountTreeCells(cell);
        });

    std::printf("cells in tree: %zu, sizeof(BankedEncoderCell) %zu\n", numCells, sizeof(SmartGrid::BankedEncoderCell));
    ReportCounters("none", rig, SceneChange::None, numCells);
    ReportCounters("blend-only", rig, SceneChange::BlendOnly, numCells);
    ReportCounters("full", rig, SceneChange::Full, numCells);
    ReportColdCache(rig);
}
