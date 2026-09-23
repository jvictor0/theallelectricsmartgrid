// sys_save_arena.cpp — audio-thread save path is allocation-free, retries on an
// undersized arena, and the default capacity comfortably fits a real patch.
//
// Covers tasks 7.3 (footprint measurement), 8.2 (no heap allocation on the
// audio-thread build), 8.3 (undersized-arena retry yields a complete patch).

#include "doctest.h"

#include <atomic>
#include <cstdlib>
#include <new>

#include "../support/SynthRig.hpp"

// ---------------------------------------------------------------------------
// Global allocation counter. Always delegates to malloc/free; only counts while
// g_countAllocs is armed, around the precise build window under test.
// ---------------------------------------------------------------------------
namespace
{
thread_local std::atomic<long> g_allocCount{0};
thread_local std::atomic<bool> g_countAllocs{false};
}

void* operator new(std::size_t n)
{
    if (g_countAllocs.load(std::memory_order_relaxed))
    {
        g_allocCount.fetch_add(1, std::memory_order_relaxed);
    }
    void* p = std::malloc(n ? n : 1);
    if (!p)
    {
        throw std::bad_alloc();
    }
    return p;
}

void* operator new[](std::size_t n)
{
    if (g_countAllocs.load(std::memory_order_relaxed))
    {
        g_allocCount.fetch_add(1, std::memory_order_relaxed);
    }
    void* p = std::malloc(n ? n : 1);
    if (!p)
    {
        throw std::bad_alloc();
    }
    return p;
}

void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

namespace
{

// Drive the rig into a non-trivial state across a few scenes.
//
void Populate(synthrig::SynthRig& rig)
{
    rig.RunFrames(2);
    for (int scene = 0; scene < 3; ++scene)
    {
        rig.SetLeftScene(scene);
        rig.SetBlend(0.0f);
        rig.RunFrames(1);
        for (int x = 0; x < 4; ++x)
        {
            for (int y = 0; y < 4; ++y)
            {
                if (rig.EncoderConnected(x, y))
                {
                    rig.SetEncoder(x, y, 0.1f + 0.05f * (x + y));
                }
            }
        }
        rig.RunFrames(2);
    }
    rig.RunFrames(8);
}

void RequireFourSections(JSON root)
{
    DOCTEST_REQUIRE_FALSE(root.IsNull());
    DOCTEST_CHECK_FALSE(root.Get("nonagon").IsNull());
    DOCTEST_CHECK_FALSE(root.Get("squiggleBoy").IsNull());
    DOCTEST_CHECK_FALSE(root.Get("stateSaver").IsNull());
    DOCTEST_CHECK_FALSE(root.Get("configGrid").IsNull());
}

} // namespace

// ---------------------------------------------------------------------------
// 8.2: building the patch into an adequately-sized arena calls no system
//      allocator.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("sys_save_arena: audio-thread build performs no heap allocation")
{
    synthrig::SynthRig rig;
    Populate(rig);

    JsonArena& arena = rig.Internal().m_stateInterchange.SaveArena();
    arena.Reset();

    g_allocCount.store(0);
    g_countAllocs.store(true);
    JSON root = rig.Internal().ToJSON(arena);
    g_countAllocs.store(false);

    DOCTEST_CHECK_FALSE(arena.Failed());
    RequireFourSections(root);
    DOCTEST_CHECK(g_allocCount.load() == 0);
}

// ---------------------------------------------------------------------------
// 7.3: a real patch's in-arena footprint is well under the default capacity, so
//      the doubling retry effectively never fires in production.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("sys_save_arena: real patch fits comfortably in the default capacity")
{
    synthrig::SynthRig rig;
    Populate(rig);

    JsonArena& arena = rig.Internal().m_stateInterchange.SaveArena();
    arena.Reset();
    JSON root = rig.Internal().ToJSON(arena);

    DOCTEST_REQUIRE_FALSE(arena.Failed());
    RequireFourSections(root);

    size_t used = arena.m_off;
    DOCTEST_MESSAGE("in-arena patch footprint: " << used << " bytes; default capacity: "
                    << JsonArena::kDefaultCapacity << " bytes");

    // Comfortable headroom: the default holds many times a representative patch.
    //
    DOCTEST_CHECK(used < JsonArena::kDefaultCapacity / 4);
}

// ---------------------------------------------------------------------------
// 8.3: an undersized arena drives the doubling retry until the full four-section
//      patch fits.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("sys_save_arena: undersized arena retries with doubling until the patch fits")
{
    synthrig::SynthRig rig;
    Populate(rig);

    // Force the interchange's save arena far too small, then drive the real save
    // path. The audio-thread build fails, the message thread doubles the arena
    // and re-requests (RetrySaveIfFailed), until the full patch fits and is
    // serialized.
    //
    StateInterchange& si = rig.Internal().m_stateInterchange;
    si.m_saveArena.Init(4096);
    const size_t startCap = si.m_saveArena.Capacity();

    std::string json = rig.SavePatch();

    DOCTEST_REQUIRE_FALSE(json.empty());
    DOCTEST_CHECK(si.m_saveArena.Capacity() > startCap);  // it really did grow

    JsonArena parse(JsonArena::kDefaultCapacity);
    JSON root = parse.Loads(json.c_str());
    RequireFourSections(root);
}

DOCTEST_TEST_CASE("sys_save_arena: recording snapshot and all parameter types allocate no heap on audio")
{
    synthrig::SynthRig rig;
    Populate(rig);
    const auto directory = std::filesystem::temp_directory_path()
        / ("smartgrid-snapshot-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    DOCTEST_REQUIRE(rig.PrepareRecording(directory.string()));
    StreamingRecorder& recorder = rig.Internal().m_context.m_recorder;
    State* mute = rig.Internal().m_nonagon.m_stateSaver.Get("Mute", 0);
    auto* encoder = rig.Internal().m_squiggleBoy.m_encoders.m_encoderBankBank.GetEncoder(0);
    encoder->m_modulators.AddGesture(encoder, 0);
    auto* gesture = encoder->m_modulators.m_gestures[0].get();
    g_allocCount = 0;
    const auto begin = std::chrono::steady_clock::now();
    g_countAllocs = true;
    const bool started = rig.Internal().StartRecording();
    mute->Set(true);
    rig.Internal().HandleParamSet({SmartGrid::MessageIn::Mode::ParamSet14, 0, 0, 8192});
    rig.Internal().HandleParamSet({SmartGrid::MessageIn::Mode::ParamSet14, 1, 0, 4096});
    encoder->SetAndRecordValue(0.25f, 0, 0);
    gesture->SetActive(true);
    recorder.BeginFrame();
    recorder.CommitFrame();
    g_countAllocs = false;
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - begin).count();
    const long captureAllocations = g_allocCount.load();
    DOCTEST_REQUIRE(started);
    DOCTEST_CHECK_FALSE(rig.SavePatch().empty());
    DOCTEST_CHECK(recorder.m_initialPatch.Get("nonagon").Get("Mute_0").GetAt(0).IntegerValue() == 0);
    recorder.Stop();
    recorder.Shutdown();
    DOCTEST_CHECK(captureAllocations == 0);
    DOCTEST_CHECK(recorder.GetError() == StreamingRecorder::Error::None);
    DOCTEST_MESSAGE("recording snapshot and first capture: " << micros << " us; arena bytes: " << recorder.m_patchArena.m_off);
    std::filesystem::remove_all(directory);
}
