#include "doctest.h"

#include "InteriorGrid.hpp"
#include "TheNonagon.hpp"
#include "TheNonagonSquiggleBoyWrldBldr.hpp"
#include "../support/GlobalEnv.hpp"
#include "../support/SynthRig.hpp"

#include <array>
#include <memory>

namespace
{
struct RhythmScene
{
    int m_size;
    int m_reset;
    uint8_t m_mask;
};

constexpr std::array<RhythmScene, 8> x_scenes =
{{
    {1, -1, 0x81},
    {2, 5, 0x42},
    {3, 4, 0x24},
    {4, 3, 0x18},
    {5, 2, 0xa5},
    {6, 1, 0x5a},
    {7, 0, 0xff},
    {8, -1, 0x00}
}};

void Tap(SmartGrid::Grid& grid, int x, int y)
{
    grid.OnPress(x, y, 100);
    grid.OnRelease(x, y);
}

struct RhythmUIRig
{
    std::unique_ptr<SmartGridOneContext> m_context;
    std::unique_ptr<TheNonagonSmartGrid> m_nonagon;

    RhythmUIRig()
    {
        GlobalEnv::Init();
        GlobalEnv::ResetPerTest();
        m_context = std::make_unique<SmartGridOneContext>();
        m_nonagon = std::make_unique<TheNonagonSmartGrid>(true, m_context.get());
    }

    TheoryOfTimeRhythm& Rhythm(size_t loop)
    {
        return m_nonagon->m_state.m_theoryOfTimeInput.m_rhythm[loop];
    }

    void SelectScene(size_t scene)
    {
        auto& sceneManager = m_context->m_sceneManager;
        bool selectRight = sceneManager.m_blendFactor == 0.0f;
        if (selectRight)
        {
            sceneManager.SetScene2(scene);
        }
        else
        {
            sceneManager.SetScene1(scene);
        }

        sceneManager.Process();
        m_nonagon->m_stateSaver.Process();
        sceneManager.SetBlendFactor(selectRight ? 1.0f : 0.0f);
        sceneManager.Process();
        m_nonagon->m_stateSaver.Process();
    }

    void SetRhythm(size_t loop, const RhythmScene& value)
    {
        auto& saver = m_nonagon->m_stateSaver;
        State* size = saver.Get("TheoryOfTimeRhythmSize", loop);
        State* reset = saver.Get("TheoryOfTimeRhythmReset", loop);
        DOCTEST_REQUIRE(size != nullptr);
        DOCTEST_REQUIRE(reset != nullptr);
        size->Set(value.m_size);
        reset->Set(value.m_reset);
        for (size_t step = 0; step < 8; ++step)
        {
            State* gate = saver.Get("TheoryOfTimeRhythm", loop, step);
            DOCTEST_REQUIRE(gate != nullptr);
            gate->Set((value.m_mask & (1 << step)) != 0);
        }
    }

    void CheckRhythm(size_t loop, const RhythmScene& value)
    {
        DOCTEST_CAPTURE(loop);
        const auto& rhythm = Rhythm(loop);
        DOCTEST_CHECK(rhythm.m_size == value.m_size);
        DOCTEST_CHECK(rhythm.m_resetLoopIndex == value.m_reset);
        for (size_t step = 0; step < 8; ++step)
        {
            DOCTEST_CAPTURE(step);
            DOCTEST_CHECK(rhythm.m_gate[step] == ((value.m_mask & (1 << step)) != 0));
        }
    }

    void PublishClock(TheoryOfTimeBase::Input& input, double phase)
    {
        auto& time = m_nonagon->m_nonagon.m_theoryOfTime;
        input.m_unmodulatedPhase = phase;
        for (size_t sample = 1; sample <= TheoryOfTimeBase::x_microBlockSize; ++sample)
        {
            time.TheoryOfTimeBase::Process(sample, input);
        }

        time.RolloverMicroblockBuffer();
    }
};

TheoryOfTimeBase::Input FlatClock()
{
    TheoryOfTimeBase::Input input;
    input.m_running = true;
    for (size_t loop = 0; loop < TheoryOfTimeBase::x_globalLoop; ++loop)
    {
        input.m_input[loop].m_parentIndex = 5;
        input.m_input[loop].m_parentMult = 1;
    }

    return input;
}

void TapWorldBuilder(TheNonagonSquiggleBoyWrldBldr& world, TheNonagonSquiggleBoyWrldBldr::Routes route, int x, int y)
{
    world.Apply(SmartGrid::MessageIn(0, static_cast<int>(route), SmartGrid::MessageIn::Mode::PadPress, x, y, 100));
    world.Apply(SmartGrid::MessageIn(0, static_cast<int>(route), SmartGrid::MessageIn::Mode::PadRelease, x, y, 0));
}
}

DOCTEST_TEST_CASE("TheoryOfTime rhythm UI toggles steps and preserves hidden steps across length edits")
{
    RhythmUIRig rig;
    auto& grid = *rig.m_nonagon->m_theoryOfTimeRhythmGrid;
    for (size_t loop = 0; loop < 6; ++loop)
    {
        DOCTEST_CAPTURE(loop);
        rig.CheckRhythm(loop, {2, -1, 0x01});
        Tap(grid, static_cast<int>(loop), 0);
        DOCTEST_CHECK_FALSE(rig.Rhythm(loop).m_gate[0]);
        Tap(grid, static_cast<int>(loop), 0);
        DOCTEST_CHECK(rig.Rhythm(loop).m_gate[0]);
        Tap(grid, static_cast<int>(loop), 7);
        DOCTEST_CHECK(rig.Rhythm(loop).m_gate[7]);
        DOCTEST_CHECK(grid.GetColor(static_cast<int>(loop), 7) == SmartGrid::Color::Off);

        rig.m_nonagon->m_state.m_shift = true;
        Tap(grid, static_cast<int>(loop), 0);
        rig.CheckRhythm(loop, {1, -1, 0x81});
        Tap(grid, static_cast<int>(loop), 7);
        rig.CheckRhythm(loop, {8, -1, 0x81});
        DOCTEST_CHECK(grid.GetColor(static_cast<int>(loop), 7) == SmartGrid::Color::Purple.Dim());
        rig.m_nonagon->m_state.m_shift = false;
    }
}

DOCTEST_TEST_CASE("TheoryOfTime rhythm UI highlights signed and wide absolute positions")
{
    struct HighlightCase
    {
        double m_phase;
        int m_step;
    };

    constexpr std::array<HighlightCase, 4> x_cases =
    {{
        {-0.25, 2},
        {4294967296.25, 1},
        {-4294967296.25, 1},
        {2147483648.25, 2}
    }};

    RhythmUIRig rig;
    rig.SetRhythm(0, {3, -1, 0x05});
    auto input = FlatClock();
    auto& grid = *rig.m_nonagon->m_theoryOfTimeRhythmGrid;
    for (const auto& example : x_cases)
    {
        DOCTEST_CAPTURE(example.m_phase);
        rig.PublishClock(input, example.m_phase);
        for (int step = 0; step < 3; ++step)
        {
            bool enabled = step != 1;
            SmartGrid::Color expected = enabled ? SmartGrid::Color::Purple.Dim() : SmartGrid::Color::Grey;
            if (step == example.m_step)
            {
                expected = enabled ? SmartGrid::Color::Purple : SmartGrid::Color::Pink;
            }

            DOCTEST_CHECK(grid.GetColor(0, step) == expected);
        }

        DOCTEST_CHECK(grid.GetColor(0, 3) == SmartGrid::Color::Off);
    }
}

DOCTEST_TEST_CASE("TheoryOfTime rhythm UI highlights steps relative to the selected reset ancestor")
{
    RhythmUIRig rig;
    rig.SetRhythm(0, {3, -1, 0x04});
    auto input = FlatClock();
    input.m_input[0].m_parentMult = 4;
    rig.PublishClock(input, 1.5);
    auto& grid = *rig.m_nonagon->m_theoryOfTimeRhythmGrid;
    DOCTEST_CHECK(grid.GetColor(0, 0) == SmartGrid::Color::Pink);
    Tap(*rig.m_nonagon->m_theoryOfTimeRhythmResetGrid, 0, 5);
    DOCTEST_CHECK(rig.Rhythm(0).m_resetLoopIndex == 5);
    DOCTEST_CHECK(grid.GetColor(0, 2) == SmartGrid::Color::Purple);
    DOCTEST_CHECK(grid.GetColor(0, 0) == SmartGrid::Color::Grey);
}

DOCTEST_TEST_CASE("TheoryOfTime rhythm reset UI only selects accepted strict ancestors")
{
    RhythmUIRig rig;
    auto input = FlatClock();
    input.m_input[0].m_parentIndex = 2;
    rig.PublishClock(input, 0.25);
    auto& grid = *rig.m_nonagon->m_theoryOfTimeRhythmResetGrid;
    DOCTEST_CHECK(grid.GetColor(0, 0) == SmartGrid::Color::Off);
    DOCTEST_CHECK(grid.GetColor(0, 1) == SmartGrid::Color::Off);
    DOCTEST_CHECK(grid.GetColor(0, 2) == SmartGrid::Color::Blue.Dim());
    DOCTEST_CHECK(grid.GetColor(0, 5) == SmartGrid::Color::Blue.Dim());
    Tap(grid, 0, 0);
    Tap(grid, 0, 1);
    DOCTEST_CHECK(rig.Rhythm(0).m_resetLoopIndex == -1);
    Tap(grid, 0, 2);
    DOCTEST_CHECK(rig.Rhythm(0).m_resetLoopIndex == 2);
    DOCTEST_CHECK(grid.GetColor(0, 2) == SmartGrid::Color::Blue);
    Tap(grid, 0, 2);
    DOCTEST_CHECK(rig.Rhythm(0).m_resetLoopIndex == -1);

    input.m_input[0].m_parentIndex = 1;
    rig.PublishClock(input, 0.75);
    DOCTEST_CHECK(grid.GetColor(0, 1) == SmartGrid::Color::Off);
    Tap(grid, 0, 1);
    DOCTEST_CHECK(rig.Rhythm(0).m_resetLoopIndex == -1);
    DOCTEST_CHECK(grid.GetColor(0, 2) == SmartGrid::Color::Blue.Dim());

    rig.PublishClock(input, 1.25);
    DOCTEST_CHECK(grid.GetColor(0, 2) == SmartGrid::Color::Off);
    Tap(grid, 0, 2);
    DOCTEST_CHECK(rig.Rhythm(0).m_resetLoopIndex == -1);
    DOCTEST_CHECK(grid.GetColor(0, 1) == SmartGrid::Color::Blue.Dim());
    Tap(grid, 0, 1);
    DOCTEST_CHECK(rig.Rhythm(0).m_resetLoopIndex == 1);
    DOCTEST_CHECK(grid.GetColor(0, 1) == SmartGrid::Color::Blue);
}

DOCTEST_TEST_CASE("TheoryOfTime rhythm state round trips every displayed step in all eight scenes")
{
    RhythmUIRig source;
    for (size_t scene = 0; scene < 8; ++scene)
    {
        source.SelectScene(scene);
        for (size_t loop = 0; loop < 6; ++loop)
        {
            source.CheckRhythm(loop, {2, -1, 0x01});
            source.SetRhythm(loop, x_scenes[(scene + loop) % x_scenes.size()]);
        }
    }

    JsonArena arena(JsonArena::kDefaultCapacity);
    JSON saved = source.m_nonagon->ToJSON(arena);
    RhythmUIRig loaded;
    loaded.m_nonagon->FromJSON(saved);
    for (size_t scene = 0; scene < 8; ++scene)
    {
        DOCTEST_CAPTURE(scene);
        source.SelectScene(scene);
        loaded.SelectScene(scene);
        for (size_t loop = 0; loop < 6; ++loop)
        {
            const auto& expected = x_scenes[(scene + loop) % x_scenes.size()];
            source.CheckRhythm(loop, expected);
            loaded.CheckRhythm(loop, expected);
        }
    }
}

DOCTEST_TEST_CASE("TheoryOfTime rhythm scene copy and reset preserve the other scenes")
{
    RhythmUIRig rig;
    rig.SetRhythm(0, {8, 5, 0x81});
    rig.m_nonagon->CopyToScene(7);
    rig.SelectScene(7);
    rig.CheckRhythm(0, {8, 5, 0x81});
    rig.SetRhythm(0, {3, 2, 0x06});
    rig.SelectScene(0);
    rig.CheckRhythm(0, {8, 5, 0x81});
    rig.SelectScene(7);
    rig.m_nonagon->RevertToDefault(false);
    rig.CheckRhythm(0, {2, -1, 0x01});
    rig.SelectScene(0);
    rig.CheckRhythm(0, {8, 5, 0x81});
    rig.m_nonagon->RevertToDefault(true);
    for (size_t scene = 0; scene < 8; ++scene)
    {
        rig.SelectScene(scene);
        for (size_t loop = 0; loop < 6; ++loop)
        {
            rig.CheckRhythm(loop, {2, -1, 0x01});
        }
    }
}

DOCTEST_TEST_CASE("TheoryOfTime rhythm missing patch keys preserve current values and fresh defaults")
{
    JsonArena arena(JsonArena::kDefaultCapacity);
    JSON oldPatch = arena.Loads(R"({"Mute_0":[1,1,1,1,1,1,1,1]})");
    RhythmUIRig fresh;
    fresh.m_nonagon->FromJSON(oldPatch);
    for (size_t scene = 0; scene < 8; ++scene)
    {
        fresh.SelectScene(scene);
        for (size_t loop = 0; loop < 6; ++loop)
        {
            fresh.CheckRhythm(loop, {2, -1, 0x01});
        }
    }

    RhythmUIRig edited;
    for (size_t scene = 0; scene < 8; ++scene)
    {
        edited.SelectScene(scene);
        edited.SetRhythm(0, x_scenes[scene]);
    }

    edited.m_nonagon->m_stateSaver.Get("Mute", 0)->Set(false);
    edited.m_nonagon->FromJSON(oldPatch);
    DOCTEST_CHECK(edited.m_nonagon->m_state.m_trigLogic.m_mute[0]);
    for (size_t scene = 0; scene < 8; ++scene)
    {
        edited.SelectScene(scene);
        edited.CheckRhythm(0, x_scenes[scene]);
    }
}

DOCTEST_TEST_CASE("TheoryOfTime WorldBuilder aux rhythm selector routes both pages and retains other modes")
{
    using WorldBuilder = TheNonagonSquiggleBoyWrldBldr;
    using Mode = WorldBuilder::GridsMode;
    using Route = WorldBuilder::Routes;
    synthrig::SynthRig rig;
    auto world = std::make_unique<WorldBuilder>(&rig.Internal());
    auto& nonagon = rig.Internal().m_nonagon;
    world->m_uiState.m_displayMode.store(WorldBuilder::DisplayMode::Visualizer);
    TapWorldBuilder(*world, Route::AuxGrid, 1, 1);
    DOCTEST_CHECK(world->m_gridsMode == Mode::TheoryOfTimeRhythm);
    DOCTEST_CHECK(world->m_uiState.m_displayMode.load() == WorldBuilder::DisplayMode::Controller);
    DOCTEST_CHECK(rig.Internal().m_uiState.m_gridsMode.load() == static_cast<uint8_t>(Mode::TheoryOfTimeRhythm));
    DOCTEST_CHECK(world->m_leftGrid.m_grid == nonagon.m_theoryOfTimeRhythmGrid);
    DOCTEST_CHECK(world->m_rightGrid.m_grid == nonagon.m_theoryOfTimeRhythmResetGrid);
    TapWorldBuilder(*world, Route::LeftGrid, 0, 1);
    DOCTEST_CHECK(nonagon.m_state.m_theoryOfTimeInput.m_rhythm[0].m_gate[1]);
    TapWorldBuilder(*world, Route::RightGrid, 0, 5);
    DOCTEST_CHECK(nonagon.m_state.m_theoryOfTimeInput.m_rhythm[0].m_resetLoopIndex == 5);

    struct ModeCase
    {
        int m_selector;
        Mode m_mode;
        SmartGrid::Grid* m_left;
        SmartGrid::Grid* m_right;
    };

    const std::array<ModeCase, 5> x_modes =
    {{
        {0, Mode::ComuteAndTheory, nonagon.m_lameJuisCoMuteGrid, nonagon.m_theoryOfTimeTopologyGrid},
        {2, Mode::Matrix, nonagon.m_lameJuisMatrixGrid, nonagon.m_lameJuisRHSGrid},
        {3, Mode::Intervals, nonagon.m_lameJuisCoMuteGrid, nonagon.m_lameJuisIntervalGrid},
        {4, Mode::SubSequencer, nonagon.m_sheafViewGridWaterGrid, nonagon.m_indexArpWaterGrid},
        {5, Mode::Config, nonagon.m_lameJuisCoMuteGrid, &rig.Internal().m_configGrid}
    }};

    for (const auto& mode : x_modes)
    {
        TapWorldBuilder(*world, Route::AuxGrid, mode.m_selector, 1);
        DOCTEST_CHECK(world->m_gridsMode == mode.m_mode);
        DOCTEST_CHECK(world->m_leftGrid.m_grid == mode.m_left);
        DOCTEST_CHECK(world->m_rightGrid.m_grid == mode.m_right);
    }

    TapWorldBuilder(*world, Route::AuxGrid, 4, 1);
    TapWorldBuilder(*world, Route::AuxGrid, 7, 0);
    DOCTEST_CHECK(world->m_leftGrid.m_grid == nonagon.m_sheafViewGridFireGrid);
    DOCTEST_CHECK(world->m_rightGrid.m_grid == nonagon.m_indexArpFireGrid);
    TapWorldBuilder(*world, Route::AuxGrid, 6, 0);
    DOCTEST_CHECK(world->m_leftGrid.m_grid == nonagon.m_sheafViewGridEarthGrid);
    DOCTEST_CHECK(world->m_rightGrid.m_grid == nonagon.m_indexArpEarthGrid);
    TapWorldBuilder(*world, Route::AuxGrid, 5, 0);
    DOCTEST_CHECK(world->m_leftGrid.m_grid == nonagon.m_sheafViewGridWaterGrid);
    DOCTEST_CHECK(world->m_rightGrid.m_grid == nonagon.m_indexArpWaterGrid);
}
