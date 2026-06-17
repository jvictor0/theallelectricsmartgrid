#include "doctest.h"

#include "SourceMeterLayout.hpp"

#include "../support/SynthRig.hpp"

namespace
{

using SourceAssignment = SquiggleBoyVoiceConfig::SourceAssignment;
using SourceChannel = SourceAssignment::Channel;

void ClearSelections(SquiggleBoyConfigGrid& grid, size_t trioIdx)
{
    for (size_t i = 0; i < SourceMixer::x_numSources; ++i)
    {
        grid.m_sourceSelected[trioIdx][i] = false;
    }
}

void SetSourceWidth(
    SquiggleBoyWithEncoderBank& squiggleBoy,
    size_t sourceIndex,
    SourceMixer::SourceWidth width)
{
    squiggleBoy.m_sourceMixerState.m_sources[sourceIndex].m_config.m_width = width;
}

void SetParamValue(
    SquiggleBoyWithEncoderBank& squiggleBoy,
    SmartGridOneEncoders::Param param,
    float value)
{
    SmartGrid::BankedEncoderCell* cell = squiggleBoy.m_encoders.m_encoderBankBank.GetEncoder(static_cast<size_t>(param));
    DOCTEST_REQUIRE(cell);

    for (size_t i = 0; i < 16; ++i)
    {
        cell->m_output[i] = value;
        cell->m_slew[i].m_output = value;
    }
}

SourceAssignment AssignmentAt(synthrig::SynthRig& rig, size_t trioIdx, size_t voiceInTrio)
{
    size_t voiceIdx = trioIdx * TheNonagonInternal::x_voicesPerTrio + voiceInTrio;
    return rig.Internal().m_squiggleBoy.m_state[voiceIdx].m_voiceConfig.m_sourceAssignment;
}

void CheckAssignment(SourceAssignment assignment, SourceChannel channel, size_t sourceIndex)
{
    DOCTEST_CHECK(assignment.m_channel == channel);
    DOCTEST_CHECK(assignment.m_sourceIndex == sourceIndex);
}

void CheckSilent(SourceAssignment assignment)
{
    DOCTEST_CHECK(assignment.m_channel == SourceChannel::Silent);
}

size_t MixerInputIndex(size_t source, size_t lane)
{
    return SquiggleBoy::x_numVoices + source * SourceMixer::x_numSourceLanes + lane;
}

SourceMixer::SourceConfig MakeSourceConfig(SourceMixer::SourceWidth width)
{
    SourceMixer::SourceConfig config;
    config.m_width = width;
    return config;
}

}

DOCTEST_TEST_CASE("SquiggleBoy config assigns stereo-only source to two voices and silences the third")
{
    synthrig::SynthRig rig;
    size_t trioIdx = static_cast<size_t>(TheNonagonInternal::Trio::Water);

    auto& grid = rig.Internal().m_configGrid;
    auto& squiggleBoy = rig.Internal().m_squiggleBoy;

    ClearSelections(grid, trioIdx);
    SetSourceWidth(squiggleBoy, 2, SourceMixer::SourceWidth::Stereo);
    grid.m_sourceSelected[trioIdx][2] = true;
    grid.PropagateSourceSelection();

    CheckAssignment(AssignmentAt(rig, trioIdx, 0), SourceChannel::Left, 2);
    CheckAssignment(AssignmentAt(rig, trioIdx, 1), SourceChannel::Right, 2);
    CheckSilent(AssignmentAt(rig, trioIdx, 2));
}

DOCTEST_TEST_CASE("SquiggleBoy config assigns mono-only source to one voice and silences the rest")
{
    synthrig::SynthRig rig;
    size_t trioIdx = static_cast<size_t>(TheNonagonInternal::Trio::Earth);

    auto& grid = rig.Internal().m_configGrid;
    auto& squiggleBoy = rig.Internal().m_squiggleBoy;

    ClearSelections(grid, trioIdx);
    SetSourceWidth(squiggleBoy, 3, SourceMixer::SourceWidth::Mono);
    grid.m_sourceSelected[trioIdx][3] = true;
    grid.PropagateSourceSelection();

    CheckAssignment(AssignmentAt(rig, trioIdx, 0), SourceChannel::Left, 3);
    CheckSilent(AssignmentAt(rig, trioIdx, 1));
    CheckSilent(AssignmentAt(rig, trioIdx, 2));
}

DOCTEST_TEST_CASE("SquiggleBoy config assigns mixed stereo and mono sources in order")
{
    synthrig::SynthRig rig;
    size_t trioIdx = static_cast<size_t>(TheNonagonInternal::Trio::Fire);

    auto& grid = rig.Internal().m_configGrid;
    auto& squiggleBoy = rig.Internal().m_squiggleBoy;

    ClearSelections(grid, trioIdx);
    SetSourceWidth(squiggleBoy, 0, SourceMixer::SourceWidth::Mono);
    SetSourceWidth(squiggleBoy, 1, SourceMixer::SourceWidth::Stereo);
    grid.m_sourceSelected[trioIdx][0] = true;
    grid.m_sourceSelected[trioIdx][1] = true;
    grid.PropagateSourceSelection();

    CheckAssignment(AssignmentAt(rig, trioIdx, 0), SourceChannel::Left, 0);
    CheckAssignment(AssignmentAt(rig, trioIdx, 1), SourceChannel::Left, 1);
    CheckAssignment(AssignmentAt(rig, trioIdx, 2), SourceChannel::Right, 1);
}

DOCTEST_TEST_CASE("SquiggleBoy config allows zero selected sources")
{
    synthrig::SynthRig rig;
    size_t trioIdx = static_cast<size_t>(TheNonagonInternal::Trio::Water);

    auto& grid = rig.Internal().m_configGrid;

    ClearSelections(grid, trioIdx);
    grid.PropagateSourceSelection();

    CheckSilent(AssignmentAt(rig, trioIdx, 0));
    CheckSilent(AssignmentAt(rig, trioIdx, 1));
    CheckSilent(AssignmentAt(rig, trioIdx, 2));
}

DOCTEST_TEST_CASE("SquiggleBoy config enforces channel limit after mono source becomes stereo")
{
    synthrig::SynthRig rig;
    size_t trioIdx = static_cast<size_t>(TheNonagonInternal::Trio::Earth);

    auto& grid = rig.Internal().m_configGrid;
    auto& squiggleBoy = rig.Internal().m_squiggleBoy;

    ClearSelections(grid, trioIdx);
    grid.m_sourceSelected[trioIdx][0] = true;
    grid.m_sourceSelected[trioIdx][1] = true;
    grid.m_sourceSelected[trioIdx][2] = true;

    SetSourceWidth(squiggleBoy, 1, SourceMixer::SourceWidth::Stereo);
    grid.EnforceSourceChannelLimit(trioIdx, 1);
    grid.PropagateSourceSelection();

    DOCTEST_CHECK(!grid.m_sourceSelected[trioIdx][0]);
    DOCTEST_CHECK(grid.m_sourceSelected[trioIdx][1]);
    DOCTEST_CHECK(grid.m_sourceSelected[trioIdx][2]);

    CheckAssignment(AssignmentAt(rig, trioIdx, 0), SourceChannel::Left, 1);
    CheckAssignment(AssignmentAt(rig, trioIdx, 1), SourceChannel::Right, 1);
    CheckAssignment(AssignmentAt(rig, trioIdx, 2), SourceChannel::Left, 2);
}

DOCTEST_TEST_CASE("SquiggleBoy config round-trips source widths and selections")
{
    synthrig::SynthRig rig;
    size_t trioIdx = static_cast<size_t>(TheNonagonInternal::Trio::Water);

    auto& grid = rig.Internal().m_configGrid;
    auto& squiggleBoy = rig.Internal().m_squiggleBoy;

    ClearSelections(grid, trioIdx);
    SetSourceWidth(squiggleBoy, 1, SourceMixer::SourceWidth::Stereo);
    SetSourceWidth(squiggleBoy, 3, SourceMixer::SourceWidth::Mono);
    grid.m_sourceSelected[trioIdx][1] = true;
    grid.m_sourceSelected[trioIdx][3] = true;
    grid.m_sourceMonitor[1] = false;
    grid.m_sourceMonitor[3] = true;

    JsonArena arena;
    arena.Init(JsonArena::kDefaultCapacity);
    JSON configJ = grid.ToJSON(arena);

    synthrig::SynthRig loadedRig;
    auto& loadedGrid = loadedRig.Internal().m_configGrid;
    auto& loadedSquiggleBoy = loadedRig.Internal().m_squiggleBoy;
    loadedGrid.FromJSON(configJ);
    loadedGrid.PropagateSourceSelection();

    DOCTEST_CHECK(loadedSquiggleBoy.m_sourceMixerState.m_sources[1].m_config.IsStereo());
    DOCTEST_CHECK(!loadedSquiggleBoy.m_sourceMixerState.m_sources[3].m_config.IsStereo());
    DOCTEST_CHECK(loadedGrid.m_sourceSelected[trioIdx][1]);
    DOCTEST_CHECK(loadedGrid.m_sourceSelected[trioIdx][3]);
    DOCTEST_CHECK(!loadedGrid.m_sourceMonitor[1]);
    DOCTEST_CHECK(loadedGrid.m_sourceMonitor[3]);

    CheckAssignment(AssignmentAt(loadedRig, trioIdx, 0), SourceChannel::Left, 1);
    CheckAssignment(AssignmentAt(loadedRig, trioIdx, 1), SourceChannel::Right, 1);
    CheckAssignment(AssignmentAt(loadedRig, trioIdx, 2), SourceChannel::Left, 3);
}

DOCTEST_TEST_CASE("SquiggleBoy config missing source selection JSON loads with no selected sources")
{
    synthrig::SynthRig rig;

    auto& grid = rig.Internal().m_configGrid;
    JsonArena arena;
    arena.Init(JsonArena::kDefaultCapacity);
    JSON oldConfigJ = arena.Object();

    grid.FromJSON(oldConfigJ);
    grid.PropagateSourceSelection();

    for (size_t trioIdx = 0; trioIdx < TheNonagonInternal::x_numTrios; ++trioIdx)
    {
        for (size_t source = 0; source < SourceMixer::x_numSources; ++source)
        {
            DOCTEST_CHECK(!grid.m_sourceSelected[trioIdx][source]);
        }

        CheckSilent(AssignmentAt(rig, trioIdx, 0));
        CheckSilent(AssignmentAt(rig, trioIdx, 1));
        CheckSilent(AssignmentAt(rig, trioIdx, 2));
    }
}

DOCTEST_TEST_CASE("SquiggleBoy mixer reserves separate quad mixer inputs for source lanes")
{
    synthrig::SynthRig rig;
    auto& squiggleBoy = rig.Internal().m_squiggleBoy;

    DOCTEST_CHECK(squiggleBoy.m_mixerState.m_numInputs == SquiggleBoy::x_numVoices + SourceMixer::x_numOutputChannels);

    for (size_t source = 0; source < SourceMixer::x_numSources; ++source)
    {
        for (size_t lane = 0; lane < SourceMixer::x_numSourceLanes; ++lane)
        {
            size_t inputIndex = SquiggleBoy::x_numVoices + source * SourceMixer::x_numSourceLanes + lane;
            DOCTEST_CHECK(inputIndex < squiggleBoy.m_mixerState.m_numInputs);
        }
    }
}

DOCTEST_TEST_CASE("SquiggleBoy mixer centers mono source lanes and pans stereo lanes from source input bits plus one")
{
    synthrig::SynthRig rig;
    auto& squiggleBoy = rig.Internal().m_squiggleBoy;

    for (size_t source = 0; source < SourceMixer::x_numSources; ++source)
    {
        SetSourceWidth(squiggleBoy, source, SourceMixer::SourceWidth::Mono);
    }

    rig.RunSamples(1);

    for (size_t source = 0; source < SourceMixer::x_numSources; ++source)
    {
        for (size_t lane = 0; lane < SourceMixer::x_numSourceLanes; ++lane)
        {
            DOCTEST_CHECK(squiggleBoy.m_mixerState.m_x[MixerInputIndex(source, lane)] == 0.5f);
            DOCTEST_CHECK(squiggleBoy.m_mixerState.m_y[MixerInputIndex(source, lane)] == 0.5f);
        }
    }

    for (size_t source = 0; source < SourceMixer::x_numSources; ++source)
    {
        SetSourceWidth(squiggleBoy, source, SourceMixer::SourceWidth::Stereo);
    }

    rig.RunSamples(1);

    for (size_t source = 0; source < SourceMixer::x_numSources; ++source)
    {
        for (size_t lane = 0; lane < SourceMixer::x_numSourceLanes; ++lane)
        {
            size_t sourceInputIndex = source * SourceMixer::x_numSourceLanes + lane;
            size_t panIndex = sourceInputIndex + 1;
            float expectedX = (panIndex & 1) ? 1.0f : 0.0f;
            float expectedY = (panIndex & 2) ? 1.0f : 0.0f;

            DOCTEST_CHECK(squiggleBoy.m_mixerState.m_x[MixerInputIndex(source, lane)] == expectedX);
            DOCTEST_CHECK(squiggleBoy.m_mixerState.m_y[MixerInputIndex(source, lane)] == expectedY);
        }

        float leftX = squiggleBoy.m_mixerState.m_x[MixerInputIndex(source, 0)];
        float leftY = squiggleBoy.m_mixerState.m_y[MixerInputIndex(source, 0)];
        float rightX = squiggleBoy.m_mixerState.m_x[MixerInputIndex(source, 1)];
        float rightY = squiggleBoy.m_mixerState.m_y[MixerInputIndex(source, 1)];

        DOCTEST_CHECK(leftX + rightX == 1.0f);
        DOCTEST_CHECK(leftY + rightY == 1.0f);
    }
}

DOCTEST_TEST_CASE("Source meter layout keeps mono meter and reduction in offset slots")
{
    SourceMeterLayout::Slots slots = SourceMeterLayout::Make(10.0f, 12.0f, MakeSourceConfig(SourceMixer::SourceWidth::Mono));

    DOCTEST_CHECK(slots.m_leftReductionX0 == 11.0f);
    DOCTEST_CHECK(slots.m_leftReductionX1 == 12.0f);
    DOCTEST_CHECK(slots.m_leftMeterX0 == 10.0f);
    DOCTEST_CHECK(slots.m_leftMeterX1 == 11.0f);
    DOCTEST_CHECK(slots.m_rightMeterX0 == 10.0f);
    DOCTEST_CHECK(slots.m_rightMeterX1 == 11.0f);
    DOCTEST_CHECK(slots.m_rightReductionX0 == 11.0f);
    DOCTEST_CHECK(slots.m_rightReductionX1 == 12.0f);
}

DOCTEST_TEST_CASE("Source meter layout splits stereo into left reduction, left meter, right meter, right reduction")
{
    SourceMeterLayout::Slots slots = SourceMeterLayout::Make(10.0f, 12.0f, MakeSourceConfig(SourceMixer::SourceWidth::Stereo));

    DOCTEST_CHECK(slots.m_leftReductionX0 == 10.0f);
    DOCTEST_CHECK(slots.m_leftReductionX1 == 10.5f);
    DOCTEST_CHECK(slots.m_leftMeterX0 == 10.5f);
    DOCTEST_CHECK(slots.m_leftMeterX1 == 11.0f);
    DOCTEST_CHECK(slots.m_rightMeterX0 == 11.0f);
    DOCTEST_CHECK(slots.m_rightMeterX1 == 11.5f);
    DOCTEST_CHECK(slots.m_rightReductionX0 == 11.5f);
    DOCTEST_CHECK(slots.m_rightReductionX1 == 12.0f);
}

DOCTEST_TEST_CASE("SquiggleBoy mixer copies source monitor state without using a source lane as storage")
{
    synthrig::SynthRig rig;
    auto& grid = rig.Internal().m_configGrid;
    auto& squiggleBoy = rig.Internal().m_squiggleBoy;

    grid.m_sourceMonitor[1] = true;
    squiggleBoy.m_mixerState.m_monitor[MixerInputIndex(1, 0)] = false;
    squiggleBoy.m_mixerState.m_monitor[MixerInputIndex(1, 1)] = false;

    rig.RunSamples(1);

    DOCTEST_CHECK(squiggleBoy.m_mixerState.m_monitor[MixerInputIndex(1, 0)]);
    DOCTEST_CHECK(squiggleBoy.m_mixerState.m_monitor[MixerInputIndex(1, 1)]);
}

DOCTEST_TEST_CASE("SquiggleBoy writes source trims to paired physical K-Mix input channels")
{
    synthrig::SynthRig rig;
    auto& squiggleBoy = rig.Internal().m_squiggleBoy;
    KMixMidi kMixMidi;

    DOCTEST_REQUIRE(KMixMidi::x_numChannels == SourceMixer::x_numPhysicalInputChannels);

    SetSourceWidth(squiggleBoy, 1, SourceMixer::SourceWidth::Mono);
    SetSourceWidth(squiggleBoy, 2, SourceMixer::SourceWidth::Stereo);
    SetParamValue(squiggleBoy, SmartGridOneEncoders::Param::Source2Gain, 0.75f);
    SetParamValue(squiggleBoy, SmartGridOneEncoders::Param::Source3Gain, 1.0f);

    squiggleBoy.WriteKMixMidi(&kMixMidi);

    DOCTEST_CHECK(kMixMidi.m_trims[2] == 63);
    DOCTEST_CHECK(kMixMidi.m_trims[3] == 0);
    DOCTEST_CHECK(kMixMidi.m_trims[4] == 127);
    DOCTEST_CHECK(kMixMidi.m_trims[5] == 127);
}
