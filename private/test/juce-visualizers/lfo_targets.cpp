#include "doctest.h"
#include "LFOTargetVisualizers.hpp"
#include <initializer_list>

namespace
{
using Targets = LFOTargetVisualizers;
using Param = SmartGridOneLayout::Param;
using Visualizer = SmartGridOneParamVisualizers::Visualizer;

template<typename Modulator>
void Publish(ModulationMatrixUIState& matrix, Param param, size_t trio,
    std::initializer_list<Modulator> modulators)
{
    BitSet16 bits;
    for (auto modulator : modulators)
    {
        bits.Set(static_cast<int>(modulator), true);
    }

    matrix.SetModulatorsAffecting(ModulationMatrixUIState::GetParamId(param, trio), bits);
}

void CheckTargets(const Targets& targets, std::initializer_list<Visualizer> expected)
{
    DOCTEST_REQUIRE(targets.m_count == expected.size());
    size_t index = 0;
    for (auto visualizer : expected)
    {
        DOCTEST_CHECK(targets.m_visualizers[index++] == visualizer);
    }
}
}

DOCTEST_TEST_CASE("LFO targets: combine both LFOs, deduplicate, cap four and update after removal")
{
    ModulationMatrixUIState matrix;
    CheckTargets(Targets::Find(matrix, Targets::Page::Voice, 1), {});
    Publish(matrix, Param::Harmonics1, 1, {VoiceModulator::LFO1});
    Publish(matrix, Param::VPSV1, 1, {VoiceModulator::LFO2});
    Publish(matrix, Param::OscillatorMix, 1, {VoiceModulator::LFO2});
    Publish(matrix, Param::FilterDrive, 1, {VoiceModulator::LFO1, VoiceModulator::LFO2});
    Publish(matrix, Param::LPCutoff, 1, {VoiceModulator::LFO2});
    Publish(matrix, Param::AmpAHDDecay, 1, {VoiceModulator::LFO1});
    Publish(matrix, Param::PanRadius, 1, {VoiceModulator::LFO2});

    CheckTargets(Targets::Find(matrix, Targets::Page::Voice, 1),
        {Visualizer::VCO1Scope, Visualizer::PostFilterScope,
            Visualizer::FilterAnalyzer, Visualizer::ShapeAmpEnvelopeScope});

    Publish<VoiceModulator>(matrix, Param::Harmonics1, 1, {});
    CheckTargets(Targets::Find(matrix, Targets::Page::Voice, 1),
        {Visualizer::VCO1Scope, Visualizer::PostFilterScope,
            Visualizer::FilterAnalyzer, Visualizer::ShapeAmpEnvelopeScope});
    Publish<VoiceModulator>(matrix, Param::VPSV1, 1, {});
    CheckTargets(Targets::Find(matrix, Targets::Page::Voice, 1),
        {Visualizer::PostFilterScope, Visualizer::FilterAnalyzer,
            Visualizer::ShapeAmpEnvelopeScope, Visualizer::SoundStage});

    for (auto param : {Param::OscillatorMix, Param::FilterDrive, Param::LPCutoff,
        Param::AmpAHDDecay, Param::PanRadius})
    {
        Publish<VoiceModulator>(matrix, param, 1, {});
    }

    CheckTargets(Targets::Find(matrix, Targets::Page::Voice, 1), {});
}

DOCTEST_TEST_CASE("LFO targets: voice tracks and quad sources remain independent")
{
    ModulationMatrixUIState matrix;
    Publish(matrix, Param::Harmonics1, 0, {VoiceModulator::LFO1});
    Publish(matrix, Param::Harmonics2, 1, {VoiceModulator::LFO2});
    Publish(matrix, Param::SubOscillator, 2, {VoiceModulator::LFO1});
    Publish(matrix, Param::SampleStart, 2, {VoiceModulator::Random1});
    Publish(matrix, Param::DelayTime, 0, {QuadModulator::LFO1});
    Publish(matrix, Param::DelayReadSpeed, 0, {QuadModulator::LFO2});
    Publish(matrix, Param::ReverbTime, 0, {QuadModulator::LFO2});
    Publish(matrix, Param::PartialMachineAttack, 0, {QuadModulator::DelayLFO});
    Publish(matrix, Param::MasterVolume, 0, {GlobalModulator::Random1});

    CheckTargets(Targets::Find(matrix, Targets::Page::Voice, 0), {Visualizer::VCO1Scope});
    CheckTargets(Targets::Find(matrix, Targets::Page::Voice, 1), {Visualizer::VCO2Scope});
    CheckTargets(Targets::Find(matrix, Targets::Page::Voice, 2), {Visualizer::SubAnalyzer});
    for (size_t trio = 0; trio < SmartGridOneLayout::x_numTrios; ++trio)
    {
        CheckTargets(Targets::Find(matrix, Targets::Page::Quad, trio),
            {Visualizer::QuadDelayEnvelope, Visualizer::ReverbAnalyzer});
    }
}

DOCTEST_TEST_CASE("LFO targets: LFO controls never consume a target slot")
{
    ModulationMatrixUIState matrix;
    for (const auto& member : SmartGridOneLayout::x_pageMembers)
    {
        if (member.m_page == SmartGridOneLayout::Page::VoiceLFOs)
        {
            Publish(matrix, member.m_param, 0, {VoiceModulator::LFO1, VoiceModulator::LFO2});
        }
        else if (member.m_page == SmartGridOneLayout::Page::QuadLFOs)
        {
            Publish(matrix, member.m_param, 0, {QuadModulator::LFO1, QuadModulator::LFO2});
        }
    }

    for (auto param : {Param::DelayModSpeed, Param::DelayModPhaseShift,
        Param::ReverbModDepth, Param::ReverbModSpeed, Param::ReverbModPhaseShift})
    {
        Publish(matrix, param, 0, {QuadModulator::LFO1});
    }

    CheckTargets(Targets::Find(matrix, Targets::Page::Voice, 0), {});
    CheckTargets(Targets::Find(matrix, Targets::Page::Quad, 0), {});
    Publish(matrix, Param::Portamento, 0, {VoiceModulator::LFO2});
    Publish(matrix, Param::PartialMachineAttack, 0, {QuadModulator::LFO2});
    CheckTargets(Targets::Find(matrix, Targets::Page::Voice, 0), {Visualizer::MelodyRoll});
    CheckTargets(Targets::Find(matrix, Targets::Page::Quad, 0), {Visualizer::PartialMachineInputSpectrum});
}

DOCTEST_TEST_CASE("LFO targets: one through four fill the slot with the requested layout")
{
    ModulationMatrixUIState matrix;
    constexpr std::array<Param, 4> x_params =
        {Param::Harmonics1, Param::Harmonics2, Param::OscillatorMix, Param::FilterDrive};
    const juce::Rectangle<int> bounds(17, 29, 337, 339);
    const std::array<std::array<juce::Rectangle<int>, 4>, 4> expected =
    {{
        {bounds},
        {juce::Rectangle<int>(17, 29, 337, 169), juce::Rectangle<int>(17, 198, 337, 170)},
        {juce::Rectangle<int>(17, 29, 337, 169), juce::Rectangle<int>(17, 198, 168, 170),
            juce::Rectangle<int>(185, 198, 169, 170)},
        {juce::Rectangle<int>(17, 29, 168, 169), juce::Rectangle<int>(185, 29, 169, 169),
            juce::Rectangle<int>(17, 198, 168, 170), juce::Rectangle<int>(185, 198, 169, 170)}
    }};
    for (size_t count = 1; count <= x_params.size(); ++count)
    {
        Publish(matrix, x_params[count - 1], 0, {VoiceModulator::LFO1});
        auto targets = Targets::Find(matrix, Targets::Page::Voice, 0);
        DOCTEST_REQUIRE(targets.m_count == count);
        int area = 0;
        for (size_t i = 0; i < targets.m_count; ++i)
        {
            auto region = GetVisualizerRegionBounds(bounds, targets.GetRegion(i));
            DOCTEST_CHECK(region == expected[count - 1][i]);
            area += region.getWidth() * region.getHeight();
            for (size_t j = 0; j < i; ++j)
            {
                DOCTEST_CHECK(region.getIntersection(
                    GetVisualizerRegionBounds(bounds, targets.GetRegion(j))).isEmpty());
            }
        }

        DOCTEST_CHECK(area == bounds.getWidth() * bounds.getHeight());
    }
}
