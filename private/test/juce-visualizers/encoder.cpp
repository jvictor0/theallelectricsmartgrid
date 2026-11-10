#include "doctest.h"
#include "EncoderComponent.hpp"
#include "ModulatorVisualizers.hpp"
#include "ScopeComponent.hpp"
#include "GangedRandomLFOComponent.hpp"
#include "EncoderModulatorComponents.hpp"
#include "EncoderModulatorVisualizers.hpp"
#include "SmartGridOneEncoders.hpp"

DOCTEST_TEST_CASE("Encoder visualizers: every page group's random pair resolves its own history")
{
    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    ModulatorVisualizers visualizers;
    InitializeEncoderModulatorVisualizers(visualizers, ui.get());
    using namespace SmartGridOneModulators;
    for (size_t group = 0; group < x_numGroups; ++group)
    {
        for (size_t slot : {2, 3})
        {
            auto* view = dynamic_cast<ModulatorTimeAxisComponent*>(visualizers.Get(static_cast<Group>(group), slot));
            DOCTEST_REQUIRE(view != nullptr);
            DOCTEST_CHECK(view->GetVoiceCount() == x_groupLanes[group]);
            for (size_t lane = 0; lane < x_groupLanes[group]; ++lane)
            {
                auto expected = GetRef(x_groups[group][slot], lane);
                auto actual = view->GetSource(lane);
                DOCTEST_CHECK(actual.m_mode == expected.m_mode);
                DOCTEST_CHECK(actual.m_index == expected.m_index);
                DOCTEST_CHECK(actual.m_voice == expected.m_voice);
                DOCTEST_CHECK(GetRandomIndex(actual) != x_noRandom);
            }
        }
    }
}

namespace
{
using Mode = SmartGridOne::EncoderBankMode;
using Type = SmartGrid::EncoderType;

size_t RepresentativeGroup(Mode mode)
{
    using Group = SmartGridOneModulators::Group;
    switch (mode)
    {
        case Mode::VoiceWater: return static_cast<size_t>(Group::WaterSource);
        case Mode::VoiceFire: return static_cast<size_t>(Group::FireSource);
        case Mode::VoiceEarth: return static_cast<size_t>(Group::EarthSource);
        case Mode::Quad: return static_cast<size_t>(Group::QuadDelay);
        case Mode::Global: return static_cast<size_t>(Group::Global);
        default: return static_cast<size_t>(Group::NumGroups);
    }
}


void AdvanceControlIndex(ScopeWriter& writer)
{
    for (size_t i = 0; i < writer.m_decimation; ++i)
    {
        writer.AdvanceIndex();
    }
}

struct RepaintCounter : juce::CachedComponentImage
{
    int m_invalidations = 0;

    void paint(juce::Graphics&) override
    {
    }

    bool invalidateAll() override
    {
        ++m_invalidations;
        return true;
    }

    bool invalidate(const juce::Rectangle<int>&) override
    {
        return invalidateAll();
    }

    void releaseResources() override
    {
    }
};

struct SolidVisualizer : SmartGridOneMainVisualizerComponent
{
    juce::Colour m_color;
    int m_draws = 0;
    double m_extent = 100.0;

    double GetTimeExtent() const override
    {
        return m_extent;
    }


    explicit SolidVisualizer(juce::Colour color)
        : m_color(color)
    {
    }

    void Draw(juce::Graphics& g, juce::Rectangle<int> bounds) override
    {
        ++m_draws;
        g.setColour(m_color);
        g.setOpacity(1.0f);
        g.fillRect(bounds.expanded(20));
        g.setColour(juce::Colours::white);
        g.fillRect(0, 0, 4, 4);
    }
};

template<size_t Voices>
void CheckRandomMarkers(typename GangedRandomLFOComponent<Voices>::ColorMode mode)
{
    GangedRandomLFOUIState<Voices> state;
    state.m_sampleRate.store(1000);
    state.m_roundElapsedSamples.store(50);
    for (size_t voice = 0; voice < Voices; ++voice)
    {
        auto& value = state.m_voices[voice];
        value.m_source.store(0.2f + 0.2f * voice);
        value.m_target.store(0.2f + 0.2f * voice);
        value.m_output.store(0.2f + 0.2f * voice);
        value.m_waitingIncrement.store(0.01);
        value.m_movingIncrement.store(0.01);
    }

    const auto color = GetModulatorColor(VoiceModulator::Random1);
    GangedRandomLFOComponent<Voices> visualizer(&state, 1, nullptr, mode, color);
    juce::Image image(juce::Image::ARGB, 208, 208, true);
    juce::Graphics g(image);
    visualizer.Draw(g, image.getBounds());
    for (size_t voice = 0; voice < Voices; ++voice)
    {
        int y = 164 - 40 * static_cast<int>(voice);
        DOCTEST_CHECK(image.getPixelAt(49, y) == J(color));
    }
}
}

DOCTEST_TEST_CASE("Encoder labels: painting unchanged labels does not request another repaint")
{
    EncoderBankUIState bank;
    auto& cell = bank.m_states[0][0];
    cell.m_connected.store(true);
    cell.m_color.store(SmartGrid::Color::Pink);
    cell.m_shortName.store("TIME");
    ModulatorVisualizers visualizers;
    EncoderComponent encoder(EncoderBankUI(0, &bank, nullptr), 0, 0, false, visualizers);
    auto* counter = new RepaintCounter;
    encoder.m_segmentDisplay.setCachedComponentImage(counter);
    juce::Image image(juce::Image::ARGB, 100, 125, true);
    juce::Graphics g(image);
    encoder.paint(g);

    counter->m_invalidations = 0;
    encoder.paint(g);
    DOCTEST_CHECK(counter->m_invalidations == 0);

    cell.m_shortName.store("FDBK");
    encoder.paint(g);
    DOCTEST_CHECK(counter->m_invalidations > 0);
    counter->m_invalidations = 0;
    cell.m_color.store(SmartGrid::Color::Cyan);
    encoder.paint(g);
    DOCTEST_CHECK(counter->m_invalidations > 0);
}

DOCTEST_TEST_CASE("Modulator markers: random dots use the source color in every voice color mode")
{
    CheckRandomMarkers<3>(GangedRandomLFOComponent<3>::ColorMode::TrackVoices);
    CheckRandomMarkers<4>(GangedRandomLFOComponent<4>::ColorMode::Voices);
    CheckRandomMarkers<1>(GangedRandomLFOComponent<1>::ColorMode::Fixed);
}

DOCTEST_TEST_CASE("Modulator markers: unipolar and bipolar scopes draw dots in their source color")
{
    auto writer = std::make_unique<ScopeWriterGeneric<4, 1>>(256, 4096);
    for (size_t voice = 0; voice < 4; ++voice)
    {
        writer->RecordStart(0, voice);
    }

    for (size_t sample = 0; sample < 128; ++sample)
    {
        writer->Write(0, QuadFloat(0.2f, 0.4f, 0.6f, 0.8f));

        AdvanceControlIndex(*writer);
    }

    writer->Publish();
    writer->AcceptSamples();
    for (bool bipolar : {false, true})
    {
        auto color = GetModulatorColor(QuadModulator::LFO1);
        PolyphonicScopeComponent visualizer(writer.get(), 0, 4, color,
            bipolar ? 0.5f : 0.9f, bipolar ? 0.5f : 0.05f);
        juce::Image image(juce::Image::ARGB, 200, 200, true);
        juce::Graphics g(image);
        visualizer.Draw(g, image.getBounds());
        for (int voice = 0; voice < 4; ++voice)
        {
            int y = bipolar ? 80 - 20 * voice : 154 - 36 * voice;
            DOCTEST_CHECK(image.getPixelAt(198, y) == J(color));
        }
    }
}

DOCTEST_TEST_CASE("Encoder visualizers: catalogue supplies only supported sources and independent active trio scopes")
{
    auto state = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    auto* uiState = state.get();
    ModulatorVisualizers visualizers;
    InitializeEncoderModulatorVisualizers(visualizers, uiState);

    for (Mode mode : {Mode::VoiceWater, Mode::VoiceFire, Mode::VoiceEarth, Mode::Quad, Mode::Global})
    {
        for (int index = 0; index < 15; ++index)
        {
            DOCTEST_CAPTURE(mode);
            DOCTEST_CAPTURE(index);
            bool expected = index == 14 || (mode == Mode::Global ? index == 2 || index == 3
                : index < 4 || index == 6 || index == 7 || index == 11
                    || SmartGridOneEncoders::IsVoiceMode(mode));
            DOCTEST_CHECK((visualizers.Get(mode, index) != nullptr) == expected);
        }
    }

    for (int index : {8, 9, 10, 11, 14})
    {
        auto* source = visualizers.Get(Mode::Voice, index);
        DOCTEST_REQUIRE(source != nullptr);
        DOCTEST_CHECK(visualizers.GetSingle(Mode::Voice, 1 << index) == source);
        DOCTEST_CHECK(visualizers.GetSingle(Mode::Voice, (1 << index) | (1 << 12) | (1 << 13)) == nullptr);
        DOCTEST_CHECK(visualizers.GetSingle(Mode::Voice, (1 << index) | (1 << 6)) == nullptr);
    }

    auto* scope = dynamic_cast<ScopeComponent*>(visualizers.Get(Mode::Voice, 6));
    DOCTEST_REQUIRE(scope != nullptr);
    auto& writer = uiState->m_squiggleBoyUIState.m_voiceScopeWriter;
    DOCTEST_CHECK(scope->m_scopeWriter == &writer);
    DOCTEST_CHECK(scope->m_scopeIx == static_cast<size_t>(SmartGridOne::VoiceScopes::LFO1));
    int panelVoiceOffset = 1;
    ScopeComponent panel(scope->m_scopeIx, &writer, &panelVoiceOffset, ScopeComponent::ScopeType::Control, uiState);
    DOCTEST_CHECK(scope->m_voiceOffset != panel.m_voiceOffset);
    DOCTEST_CHECK(*scope->m_voiceOffset == -1);
    juce::Image image(juce::Image::ARGB, 100, 100, true);
    juce::Graphics g(image);
    for (size_t trio = 0; trio < 3; ++trio)
    {
        uiState->m_squiggleBoyUIState.m_activeTrack.store(trio);
        for (auto& muted : uiState->m_nonagonUIState.m_muted)
        {
            muted.store(false);
        }

        panel.Draw(g, image.getBounds());
        visualizers.Get(SmartGridOneEncoders::VoiceModeForTrio(trio), 6)->Draw(g, image.getBounds());
        DOCTEST_CHECK(panel.GetFirstVoice() == trio * 3);
        auto* trioScope = dynamic_cast<ScopeComponent*>(visualizers.Get(SmartGridOneEncoders::VoiceModeForTrio(trio), 6));
        DOCTEST_REQUIRE(trioScope);
        DOCTEST_CHECK(trioScope->GetFirstVoice() == trio * 3);
        DOCTEST_CHECK(scope->GetFirstVoice() == 0);
        auto* random = dynamic_cast<GangedRandomLFOComponent<3>*>(visualizers.Get(SmartGridOneEncoders::VoiceModeForTrio(trio), 0));
        DOCTEST_REQUIRE(random != nullptr);
        DOCTEST_CHECK(random->GetGangIndex() == trio);
    }
}

DOCTEST_TEST_CASE("Encoder visualizers: audio taps render time histories and source views follow their owning trio")
{
    using namespace SmartGridOneModulators;
    using Machine = VoiceMachine::SourceMachine;
    using Scope = SmartGridOne::VoiceScopes;
    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    auto& state = ui->m_squiggleBoyUIState;
    auto& writer = state.m_voiceScopeWriter;
    state.m_firstScopeSample.store(0);
    for (auto& muted : ui->m_nonagonUIState.m_muted)
    {
        muted.store(false);
    }

    constexpr Scope x_scopes[] = {Scope::VCO1, Scope::VCO2, Scope::Sub, Scope::PostAmp};
    constexpr size_t x_slots[] = {0, 1, 12, 13};
    constexpr VoiceModulator x_modulators[] = {VoiceModulator::SourceMod1, VoiceModulator::SourceMod2,
        VoiceModulator::Sub, VoiceModulator::Voice};
    for (size_t trio = 0; trio < 3; ++trio)
    {
        for (size_t lane = 0; lane < 3; ++lane)
        {
            writer.RecordStart(SmartGridOne::VoiceScopeIndex(static_cast<size_t>(Scope::PhysicalModelingEnvelope), trio), lane);
        }

        for (auto scope : x_scopes)
        {
            for (size_t lane = 0; lane < 3; ++lane)
            {
                writer.RecordStart(SmartGridOne::VoiceScopeIndex(static_cast<size_t>(scope), trio), lane);
            }
        }
    }

    for (size_t sample = 0; sample < 128; ++sample)
    {
        for (size_t trio = 0; trio < 3; ++trio)
        {
            for (auto scope : x_scopes)
            {
                writer.Write(SmartGridOne::VoiceScopeIndex(static_cast<size_t>(scope), trio), TrioFloat(-0.6f, -0.2f, 0.4f));
            }

            writer.Write(SmartGridOne::VoiceScopeIndex(static_cast<size_t>(Scope::PhysicalModelingEnvelope), trio), TrioFloat(0.2f, 0.4f, 0.7f));
        }

        writer.AdvanceIndex();
    }

    writer.Publish();
    writer.AcceptSamples();
    ModulatorVisualizers visualizers;
    InitializeEncoderModulatorVisualizers(visualizers, ui.get());
    for (size_t trio = 0; trio < 3; ++trio)
    {
        DOCTEST_CAPTURE(trio);
        state.m_activeTrack.store((trio + 1) % 3);
        auto group = static_cast<Group>(trio * 5);
        for (size_t voice = 0; voice < 9; ++voice)
        {
            state.m_voiceSourceUIState[voice].m_sourceMachine.store(Machine::Thru);
        }

        state.m_voiceSourceUIState[trio * 3].m_sourceMachine.store(Machine::DualWaveShapingVCO);
        for (size_t tap = 0; tap < 4; ++tap)
        {
            auto* scope = dynamic_cast<ScopeComponent*>(visualizers.Get(group, x_slots[tap]));
            DOCTEST_REQUIRE(scope != nullptr);
            DOCTEST_CHECK(scope->m_scopeIx == static_cast<size_t>(x_scopes[tap]));
            DOCTEST_CHECK(scope->GetFirstVoice() == trio * 3);
            DOCTEST_CHECK(scope->GetSource(2).m_index == static_cast<size_t>(x_modulators[tap]));
            DOCTEST_CHECK(scope->GetSource(2).m_voice == trio * 3 + 2);
            DOCTEST_CHECK(scope->GetTimeAxis(0).GetExtent() > 0.0);
            juce::Image image(juce::Image::ARGB, 100, 100, true);
            juce::Graphics g(image);
            scope->Draw(g, image.getBounds());
            constexpr int x_rows[] = {77, 59, 32};
            for (size_t lane = 0; lane < 3; ++lane)
            {
                auto pixel = image.getPixelAt(50, x_rows[lane]);
                auto color = J(TheNonagonSmartGrid::VoiceColor(trio * 3 + lane));
                DOCTEST_CHECK(pixel.getBrightness() > color.getBrightness() * 0.6f);
                DOCTEST_CHECK(pixel.getHue() == doctest::Approx(color.getHue()).epsilon(0.02));
                DOCTEST_CHECK(pixel.getSaturation() == doctest::Approx(color.getSaturation()).epsilon(0.02));
            }
        }

        auto* dualFirst = visualizers.Get(group, 0);
        auto* dualSecond = visualizers.Get(group, 1);
        auto* sub = visualizers.Get(group, 12);
        auto* voice = visualizers.Get(group, 13);
        state.m_voiceSourceUIState[trio * 3].m_sourceMachine.store(Machine::PhysicalModeling);
        DOCTEST_REQUIRE(dynamic_cast<NoiseModulatorComponent*>(visualizers.Get(group, 0)) != nullptr);
        auto* envelope = dynamic_cast<ScopeComponent*>(visualizers.Get(group, 1));
        DOCTEST_REQUIRE(envelope != nullptr);
        DOCTEST_CHECK(envelope != dualSecond);
        DOCTEST_CHECK(envelope->m_scopeIx == static_cast<size_t>(Scope::PhysicalModelingEnvelope));
        DOCTEST_CHECK(ui->GetModulatorValue(envelope->GetSource(0), 127) == doctest::Approx(0.2f));
        DOCTEST_CHECK(envelope->GetFirstVoice() == trio * 3);
        DOCTEST_CHECK(envelope->GetTimeAxis(0).GetExtent() > 0.0);
        for (auto placeholder : {Machine::Sample, Machine::Thru})
        {
            state.m_voiceSourceUIState[trio * 3].m_sourceMachine.store(placeholder);
            DOCTEST_CHECK(visualizers.Get(group, 0) == nullptr);
            DOCTEST_CHECK(visualizers.Get(group, 1) == nullptr);
            DOCTEST_CHECK(visualizers.Get(group, 12) == sub);
            DOCTEST_CHECK(visualizers.Get(group, 13) == voice);
        }

        state.m_voiceSourceUIState[trio * 3].m_sourceMachine.store(Machine::DualWaveShapingVCO);
        DOCTEST_CHECK(visualizers.Get(group, 0) == dualFirst);
        DOCTEST_CHECK(visualizers.Get(group, 1) == dualSecond);
    }
}

DOCTEST_TEST_CASE("Encoder visualizers: typed associations retain independent source views")
{
    ModulatorVisualizers visualizers;
    int created = 0;
    auto factory = [&](int*)
    {
        ++created;
        return std::make_unique<SolidVisualizer>(juce::Colours::red);
    };
    visualizers.Add(VoiceModulator::Random3, factory);
    visualizers.Add(QuadModulator::Random3, factory);
    visualizers.Add(GlobalModulator::Random1, factory);
    visualizers.Add(std::nullopt, factory);
    DOCTEST_CHECK(created == 5);
    auto* voice = visualizers.Get(Mode::VoiceWater, 2);
    DOCTEST_REQUIRE(voice != nullptr);
    DOCTEST_CHECK(visualizers.Get(Mode::VoiceFire, 2) != voice);
    DOCTEST_CHECK(visualizers.Get(Mode::VoiceEarth, 2) != voice);
    auto* quad = visualizers.Get(Mode::Quad, 2);
    auto* global = visualizers.Get(Mode::Global, 2);
    DOCTEST_REQUIRE(quad != nullptr);
    DOCTEST_REQUIRE(global != nullptr);
    DOCTEST_CHECK(voice != quad);
    DOCTEST_CHECK(voice != global);
    DOCTEST_CHECK(quad != global);
    DOCTEST_CHECK(visualizers.Get(Mode::Voice, 14) == nullptr);
    DOCTEST_CHECK(visualizers.Get(Mode::Voice, -1) == nullptr);
    DOCTEST_CHECK(visualizers.Get(Mode::Voice, 15) == nullptr);
    DOCTEST_CHECK(visualizers.Get(Mode::NumModes, 2) == nullptr);
}

DOCTEST_TEST_CASE("Encoder visualizers: dial background is clipped translated and translucent beneath foreground")
{
    juce::ScopedJuceInitialiser_GUI juce;
    EncoderBankUIState ui;
    ModulatorVisualizers visualizers;
    visualizers.Add(VoiceModulator::LFO1, [](int*)
    {
        return std::make_unique<SolidVisualizer>(juce::Colours::red);
    });
    ui.SetConnected(3, 3, true);
    ui.SetEncoderType(3, 3, Type::ModulatorAmount);
    ui.SetIndex(3, 3, 6);
    ui.SetMode(3, 3, Mode::Voice);
    ui.m_states[3][3].m_modulatorGroup.store(RepresentativeGroup(Mode::Voice));
    ui.SetNumVoices(3, 3, 1);
    ui.SetIndicatorColor(3, 3, 0, SmartGrid::Color::White);
    for (bool badgesAbove : {false, true})
    {
        EncoderComponent encoder(EncoderBankUI(0, &ui, nullptr), 3, 3, badgesAbove, visualizers);
        encoder.SetSize(100);
        juce::Image image(juce::Image::ARGB, 100, 125, true);
        juce::Graphics g(image);
        encoder.paint(g);
        const int dialY = badgesAbove ? 25 : 0;
        const int badgeY = badgesAbove ? 12 : 112;
        DOCTEST_CHECK(image.getPixelAt(50, badgeY).getAlpha() == 0);
        DOCTEST_CHECK(image.getPixelAt(1, dialY + 1).getAlpha() == 128);
        DOCTEST_CHECK(image.getPixelAt(1, dialY + 1).getRed() == 255);
        DOCTEST_CHECK(image.getPixelAt(1, dialY + 1).getGreen() == 255);
        DOCTEST_CHECK(image.getPixelAt(50, dialY + 50).getAlpha() == 128);
        DOCTEST_CHECK(image.getPixelAt(50, dialY + 50).getRed() == 255);
        DOCTEST_CHECK(image.getPixelAt(50, dialY + 50).getGreen() == 0);
        DOCTEST_CHECK(image.getPixelAt(50, dialY + 5).getAlpha() == 255);
        DOCTEST_CHECK(image.getPixelAt(50, dialY + 5).getGreen() == SmartGrid::Color::White.m_green);
    }
}

DOCTEST_TEST_CASE("Encoder visualizers: only connected associated depths draw including zero depths")
{
    juce::ScopedJuceInitialiser_GUI juce;
    EncoderBankUIState ui;
    ModulatorVisualizers visualizers;
    visualizers.Add(GlobalModulator::Random1, [](int*)
    {
        return std::make_unique<SolidVisualizer>(juce::Colours::red);
    });
    auto* visualizer = static_cast<SolidVisualizer*>(visualizers.Get(Mode::Global, 2));
    EncoderComponent encoder(EncoderBankUI(0, &ui, nullptr), 3, 3, false, visualizers);
    ui.SetMode(3, 3, Mode::Global);
    ui.m_states[3][3].m_modulatorGroup.store(RepresentativeGroup(Mode::Global));
    ui.SetIndex(3, 3, 2);
    ui.SetEncoderType(3, 3, Type::ModulatorAmount);
    juce::Image image(juce::Image::ARGB, 100, 125, true);
    juce::Graphics g(image);
    encoder.paint(g);
    DOCTEST_CHECK(visualizer->m_draws == 0);
    ui.SetConnected(3, 3, true);
    ui.SetBrightness(3, 3, 0);
    encoder.paint(g);
    DOCTEST_CHECK(visualizer->m_draws == 1);
    for (Type type : {Type::BaseParam, Type::GestureParam})
    {
        ui.SetEncoderType(3, 3, type);
        encoder.paint(g);
        DOCTEST_CHECK(visualizer->m_draws == 1);
    }

    ui.SetEncoderType(3, 3, Type::ModulatorAmount);
    ui.SetIndex(3, 3, 14);
    encoder.paint(g);
    DOCTEST_CHECK(visualizer->m_draws == 1);
    ui.SetIndex(3, 3, 2);
    ui.SetMode(3, 3, Mode::Voice);
    ui.m_states[3][3].m_modulatorGroup.store(RepresentativeGroup(Mode::Voice));
    encoder.paint(g);
    DOCTEST_CHECK(visualizer->m_draws == 1);
}

DOCTEST_TEST_CASE("Encoder visualizers: base parameters combine eligible sources while depths retain their own")
{
    juce::ScopedJuceInitialiser_GUI juce;
    EncoderBankUIState ui;
    ModulatorVisualizers visualizers;
    visualizers.Add(VoiceModulator::Random1, [](int*)
    {
        return std::make_unique<SolidVisualizer>(juce::Colours::red);
    });
    visualizers.Add(VoiceModulator::LFO1, [](int*)
    {
        return std::make_unique<SolidVisualizer>(juce::Colours::blue);
    });
    visualizers.Add(QuadModulator::Random1, [](int*)
    {
        return std::make_unique<SolidVisualizer>(juce::Colours::lime);
    });
    EncoderComponent encoder(EncoderBankUI(0, &ui, nullptr), 3, 3, false, visualizers);
    ui.SetConnected(3, 3, true);
    struct Case
    {
        Mode m_mode;
        Type m_type;
        int m_index;
        uint16_t m_modulators;
        uint16_t m_gestures;
        juce::Colour m_background;
    };

    const Case cases[] =
    {
        {Mode::Voice, Type::BaseParam, 6, 0, 0, juce::Colours::transparentBlack},
        {Mode::Voice, Type::BaseParam, 6, 1, 0, juce::Colours::red},
        {Mode::VoiceFire, Type::BaseParam, 6, 1, 0xffff, juce::Colours::red},
        {Mode::VoiceEarth, Type::BaseParam, 0, 64, 0, juce::Colours::blue},
        {Mode::Voice, Type::BaseParam, 0, 65, 0, juce::Colours::red},
        {Mode::Voice, Type::BaseParam, 0, 0x4001, 0, juce::Colours::red},
        {Mode::Voice, Type::BaseParam, 0, 0x4000, 0, juce::Colours::transparentBlack},
        {Mode::Voice, Type::BaseParam, 0, 0, 1, juce::Colours::transparentBlack},
        {Mode::Quad, Type::BaseParam, 6, 1, 0, juce::Colours::lime},
        {Mode::Global, Type::BaseParam, 0, 1, 0, juce::Colours::transparentBlack},
        {Mode::Voice, Type::ModulatorAmount, 6, 1, 0, juce::Colours::blue},
        {Mode::Voice, Type::ModulatorAmount, 14, 1, 0, juce::Colours::transparentBlack},
        {Mode::Voice, Type::GestureParam, 0, 1, 0, juce::Colours::transparentBlack},
        {Mode::Voice, Type::BaseParam, 6, 1, 0, juce::Colours::red},
        {Mode::Voice, Type::BaseParam, 6, 0, 0, juce::Colours::transparentBlack}
    };

    for (auto& test : cases)
    {
        DOCTEST_CAPTURE(test.m_mode);
        DOCTEST_CAPTURE(test.m_type);
        DOCTEST_CAPTURE(test.m_modulators);
        ui.SetMode(3, 3, test.m_mode);
        ui.m_states[3][3].m_modulatorGroup.store(RepresentativeGroup(test.m_mode));
        ui.SetNumVoices(3, 3, 1);
        uint16_t slots = test.m_modulators;
        if (test.m_mode != Mode::Global)
        {
            slots = (slots & ~1u) | ((slots & 1u) << 2);
        }

        for (size_t index = 0; index < 15; ++index)
        {
            ui.m_states[3][3].m_modulatorWeights[0][index].store(
                (slots & (1 << index)) != 0 ? 0.25f : 0.0f);
        }

        ui.SetEncoderType(3, 3, test.m_type);
        ui.SetIndex(3, 3, test.m_index);
        ui.SetModulatorsAffecting(3, 3, BitSet16(slots));
        ui.SetGesturesAffecting(3, 3, BitSet16(test.m_gestures));
        juce::Image image(juce::Image::ARGB, 100, 125, true);
        juce::Graphics g(image);
        encoder.paint(g);
        auto expected = test.m_background.isTransparent() ? test.m_background : test.m_background.withAlpha(0.5f);
        DOCTEST_CHECK(image.getPixelAt(50, 50) == expected);
    }
}

DOCTEST_TEST_CASE("Encoder visualizers: adjacent voice and quad controls render their own scopes and channel counts")
{
    juce::ScopedJuceInitialiser_GUI juce;
    EncoderBankUIState ui;
    ModulatorVisualizers visualizers;
    visualizers.Add(VoiceModulator::ModulationEnvelope, [](int*)
    {
        return std::make_unique<SolidVisualizer>(juce::Colours::red);
    });
    visualizers.Add(QuadModulator::Random1, [](int*)
    {
        return std::make_unique<SolidVisualizer>(juce::Colours::blue);
    });
    ui.SetMode(0, 0, Mode::VoiceEarth);
    ui.m_states[0][0].m_modulatorGroup.store(RepresentativeGroup(Mode::VoiceEarth));
    ui.SetNumVoices(0, 0, 3);
    ui.SetMode(1, 0, Mode::Quad);
    ui.m_states[1][0].m_modulatorGroup.store(RepresentativeGroup(Mode::Quad));
    ui.SetNumVoices(1, 0, 4);
    for (size_t x = 0; x < 2; ++x)
    {
        ui.SetConnected(x, 0, true);
        ui.SetModulatorsAffecting(x, 0, BitSet16(1 << (x == 0 ? 4 : 2)));
        for (size_t channel = 0; channel < 4; ++channel)
        {
            ui.m_states[x][0].m_modulatorWeights[channel][x == 0 ? 4 : 2].store(0.25f);
            ui.SetValue(x, 0, channel, 0.0f);
            ui.SetMinValue(x, 0, channel, 0.0f);
            ui.SetMaxValue(x, 0, channel, 0.0f);
            ui.SetIndicatorColor(x, 0, channel, SmartGrid::Color::White);
        }
    }

    for (Type type : {Type::BaseParam, Type::ModulatorAmount})
    {
        for (int x = 0; x < 2; ++x)
        {
            DOCTEST_CAPTURE(type);
            DOCTEST_CAPTURE(x);
            ui.SetEncoderType(x, 0, type);
            ui.SetIndex(x, 0, x == 0 ? 4 : 2);
            EncoderComponent encoder(EncoderBankUI(0, &ui, nullptr), x, 0, false, visualizers);
            juce::Image image(juce::Image::ARGB, 100, 125, true);
            juce::Graphics g(image);
            encoder.paint(g);
            auto background = x == 0 ? juce::Colours::red : juce::Colours::blue;
            DOCTEST_CHECK(image.getPixelAt(50, 50) == background.withAlpha(0.5f));
            DOCTEST_CHECK(image.getPixelAt(50, 20).getAlpha() == (x == 0 ? 128 : 255));
        }
    }
}

DOCTEST_TEST_CASE("Encoder weighted curves: longest axis hosts the normalized mixture on the parameter's voice track")
{
    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    auto& state = ui->m_squiggleBoyUIState;
    state.m_firstScopeSample.store(0);
    auto& scope = state.m_voiceScopeWriter;
    for (size_t index = 0; index <= 75; ++index)
    {
        for (size_t voice = 0; voice < 9; ++voice)
        {
            if (index == 0)
            {
                scope.RecordStart(SmartGridOne::VoiceScopeIndex(static_cast<size_t>(SmartGridOne::VoiceScopes::LFO1), voice / 3), voice % 3);
            }

            if (index % 50 == 0)
            {
                scope.RecordStart(SmartGridOne::VoiceScopeIndex(static_cast<size_t>(SmartGridOne::VoiceScopes::LFO2), voice / 3), voice % 3);
            }

            if (voice % 3 == 0)
            {
                scope.Write(SmartGridOne::VoiceScopeIndex(static_cast<size_t>(SmartGridOne::VoiceScopes::LFO1), voice / 3), TrioFloat::Splat(voice >= 6 ? 0.2f : 0.9f));
                scope.Write(SmartGridOne::VoiceScopeIndex(static_cast<size_t>(SmartGridOne::VoiceScopes::LFO2), voice / 3), TrioFloat::Splat(0.6f));
            }

            ui->m_nonagonUIState.m_muted[voice].store(false);
        }

        AdvanceControlIndex(scope);
    }

    scope.Publish();
    scope.AcceptSamples();
    ModulatorVisualizers visualizers;
    InitializeEncoderModulatorVisualizers(visualizers, ui.get());
    visualizers.m_scopeVoiceOffset = 0;
    auto& encoder = state.m_encoderBankUIState.m_states[0][0];
    encoder.m_mode.store(Mode::VoiceEarth);
    encoder.m_modulatorGroup.store(RepresentativeGroup(Mode::VoiceEarth));
    encoder.m_numVoices.store(3);
    for (size_t voice = 0; voice < 3; ++voice)
    {
        encoder.m_modulatorWeights[voice][6].store(0.1f);
        encoder.m_modulatorWeights[voice][7].store(-0.3f);
    }

    auto* selected = visualizers.GetForParameter(encoder);
    DOCTEST_REQUIRE(selected == visualizers.Get(Mode::VoiceEarth, 6));
    DOCTEST_CHECK(selected->GetTimeExtent() == 75.0);
    auto* timeView = dynamic_cast<ModulatorTimeAxisComponent*>(selected);
    DOCTEST_REQUIRE(timeView != nullptr);
    DOCTEST_CHECK(timeView->GetFirstVoice() == 6);
    for (size_t voice = 0; voice < 3; ++voice)
    {
        const auto& weights = timeView->m_weightedModulators[voice];
        DOCTEST_REQUIRE(weights.m_count == 2);
        DOCTEST_CHECK(weights.m_entries[0].m_weight == doctest::Approx(0.25f));
        DOCTEST_CHECK(weights.m_entries[1].m_weight == doctest::Approx(-0.75f));
        DOCTEST_CHECK(weights.m_entries[0].m_modulator.m_voice == 6 + voice);
    }

    juce::Image mixed(juce::Image::ARGB, 200, 200, true);
    juce::Graphics mixedGraphics(mixed);
    selected->Draw(mixedGraphics, mixed.getBounds());
    DOCTEST_CHECK(mixed.getPixelAt(197, 125) == juce::Colours::black);
    DOCTEST_CHECK(mixed.getPixelAt(100, 127) != juce::Colours::black);
    DOCTEST_CHECK(mixed.getPixelAt(100, 154) == juce::Colours::black);
    selected->ClearWeightedModulators();
    juce::Image solo(juce::Image::ARGB, 200, 200, true);
    juce::Graphics soloGraphics(solo);
    selected->Draw(soloGraphics, solo.getBounds());
    DOCTEST_CHECK(solo.getPixelAt(197, 152) == juce::Colours::black);
    DOCTEST_CHECK(solo.getPixelAt(100, 154) != juce::Colours::black);
    DOCTEST_CHECK(solo.getPixelAt(100, 127) == juce::Colours::black);

    for (size_t voice = 0; voice < 3; ++voice)
    {
        encoder.m_modulatorWeights[voice][6].store(0.0f);
    }

    selected = visualizers.GetForParameter(encoder);
    DOCTEST_REQUIRE(selected == visualizers.Get(Mode::VoiceEarth, 7));
    DOCTEST_CHECK(selected->GetTimeExtent() == 50.0);
    DOCTEST_CHECK_FALSE(dynamic_cast<ModulatorTimeAxisComponent*>(selected)->m_hasWeights);
}

DOCTEST_TEST_CASE("Encoder visualizers: a single source keeps its own graphic regardless of time extent or voice count")
{
    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    ModulatorVisualizers visualizers;
    InitializeEncoderModulatorVisualizers(visualizers, ui.get());
    EncoderUIState encoder;
    for (Mode mode : {Mode::VoiceWater, Mode::VoiceFire, Mode::VoiceEarth, Mode::Quad, Mode::Global})
    {
        size_t voices = mode == Mode::Global ? 1 : mode == Mode::Quad ? 4 : 3;
        encoder.m_mode.store(mode);
        encoder.m_modulatorGroup.store(RepresentativeGroup(mode));
        encoder.m_numVoices.store(static_cast<int>(voices));
        for (int index = 0; index < 15; ++index)
        {
            auto* source = visualizers.Get(static_cast<SmartGridOneModulators::Group>(RepresentativeGroup(mode)), index);
            if (!source)
            {
                continue;
            }

            DOCTEST_CAPTURE(mode);
            DOCTEST_CAPTURE(index);
            for (size_t voice = 0; voice < voices; ++voice)
            {
                encoder.m_modulatorWeights[voice][index].store(voice == 0 ? -0.25f : 0.5f);
            }

            WeightedModulators staleMixture;
            staleMixture.Add(ModulatorRef(GlobalModulator::Noise), 1.0f);
            source->SetWeightedModulators(0, staleMixture);
            DOCTEST_CHECK(visualizers.GetForParameter(encoder) == source);
            if (auto* timeView = dynamic_cast<ModulatorTimeAxisComponent*>(source))
            {
                DOCTEST_CHECK_FALSE(timeView->m_hasWeights);
            }

            for (size_t voice = 0; voice < voices; ++voice)
            {
                encoder.m_modulatorWeights[voice][index].store(0.0f);
            }
        }
    }
}

DOCTEST_TEST_CASE("Encoder weighted curves: multiple sources without time axes use the last second and empty weights draw nothing")
{
    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    auto& state = ui->m_squiggleBoyUIState;
    state.m_firstScopeSample.store(0);
    state.m_voiceScopeWriter.m_acceptedIndex = 1 + 2 * SampleTimer::x_sampleRate;
    ModulatorVisualizers visualizers;
    InitializeEncoderModulatorVisualizers(visualizers, ui.get());
    visualizers.m_scopeVoiceOffset = 1;
    auto& encoder = state.m_encoderBankUIState.m_states[0][0];
    encoder.m_mode.store(Mode::VoiceFire);
    encoder.m_modulatorGroup.store(RepresentativeGroup(Mode::VoiceFire));
    encoder.m_numVoices.store(3);
    encoder.m_modulatorWeights[1][11].store(-0.25f);
    encoder.m_modulatorWeights[1][8].store(0.25f);
    auto* selected = visualizers.GetForParameter(encoder);
    DOCTEST_REQUIRE(selected == &visualizers.m_recentHistory);
    DOCTEST_CHECK(selected->GetTimeExtent() == SampleTimer::x_sampleRate);
    auto axis = visualizers.m_recentHistory.GetTimeAxis(1);
    DOCTEST_CHECK(axis.GetSample(0.0) == static_cast<double>(SampleTimer::x_sampleRate));
    DOCTEST_CHECK(axis.GetSample(1.0) == static_cast<double>(2 * SampleTimer::x_sampleRate));
    juce::Image image(juce::Image::ARGB, 200, 200, true);
    juce::Graphics g(image);
    selected->Draw(g, image.getBounds());
    DOCTEST_CHECK(image.getPixelAt(197, 145) == juce::Colours::black);
    auto notes = std::make_unique<NonagonNoteWriter>();
    ui->m_nonagonUIState.m_noteWriter = notes.get();
    notes->m_timbreHistory[visualizers.m_recentHistory.GetFirstVoice() + 1].Record({0, {0.0, 0.0, 0.0}});
    selected->Draw(g, image.getBounds());
    DOCTEST_CHECK(image.getPixelAt(197, 145) == J(SmartGrid::Color::White));
    DOCTEST_REQUIRE(visualizers.m_recentHistory.m_weightedModulators[1].m_count == 2);
    DOCTEST_CHECK(visualizers.m_recentHistory.m_weightedModulators[1].m_entries[1].m_weight == -0.5f);
    encoder.m_modulatorWeights[1][11].store(0.0f);
    encoder.m_modulatorWeights[1][8].store(0.0f);
    DOCTEST_CHECK(visualizers.GetForParameter(encoder) == nullptr);

    encoder.m_mode.store(Mode::Global);
    encoder.m_modulatorGroup.store(RepresentativeGroup(Mode::Global));
    encoder.m_numVoices.store(1);
    encoder.m_modulatorWeights[0][14].store(0.5f);
    encoder.m_modulatorWeights[0][2].store(0.5f);
    DOCTEST_REQUIRE(visualizers.GetForParameter(encoder) == &visualizers.m_recentHistory);
    DOCTEST_CHECK(visualizers.m_recentHistory.GetVoiceCount() == 1);
    DOCTEST_CHECK(visualizers.m_recentHistory.m_weightedModulators[0].m_entries[0].m_modulator.m_mode
        == SmartGridOneLayout::ParamMode::Global);
}

DOCTEST_TEST_CASE("Encoder weighted curves: random time axes keep other sources at their latest value in the future")
{
    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    auto& state = ui->m_squiggleBoyUIState;
    state.m_firstScopeSample.store(100);
    state.m_voiceScopeWriter.m_acceptedIndex = 1 + 32;
    GangedRandomLFOVoice voices[4];
    GangedRandomLFOVoiceInput inputs[4];
    for (size_t voice = 0; voice < 4; ++voice)
    {
        voices[voice].m_source = 0.2f;
        voices[voice].m_target = 0.8f;
        inputs[voice] = {0.01, 0.01, 0.0f};
    }

    state.m_quadGangedRandomLFOUIState[0].m_history.RecordRound(100, voices, inputs);
    const size_t controlSamples = 33;
    for (size_t sample = 0; sample < controlSamples; ++sample)
    {
        state.m_quadScopeWriter.Write(static_cast<size_t>(SmartGridOne::QuadScopes::QuadLFO1), QuadFloat::Splat(0.4f));
        AdvanceControlIndex(state.m_quadScopeWriter);
    }

    state.m_quadScopeWriter.Publish();
    state.m_quadScopeWriter.AcceptSamples();
    ModulatorVisualizers visualizers;
    InitializeEncoderModulatorVisualizers(visualizers, ui.get());
    auto& encoder = state.m_encoderBankUIState.m_states[0][0];
    encoder.m_mode.store(Mode::Quad);
    encoder.m_modulatorGroup.store(RepresentativeGroup(Mode::Quad));
    encoder.m_numVoices.store(4);
    encoder.m_modulatorWeights[0][2].store(0.1f);
    encoder.m_modulatorWeights[0][6].store(0.3f);
    auto* selected = visualizers.GetForParameter(encoder);
    DOCTEST_REQUIRE(selected == visualizers.Get(Mode::Quad, 0));
    DOCTEST_CHECK(selected->GetTimeExtent() == 224.0);
    juce::Image image(juce::Image::ARGB, 208, 208, true);
    juce::Graphics g(image);
    selected->Draw(g, image.getBounds());
    DOCTEST_CHECK(image.getPixelAt(33, 132) == juce::Colours::black);
    bool futureCurve = false;
    for (int y = 115; y < 125; ++y)
    {
        futureCurve |= image.getPixelAt(140, y) != juce::Colours::black;
    }

    DOCTEST_CHECK(futureCurve);
}

DOCTEST_TEST_CASE("Encoder motion: curved blur grows continuously and stays inside the knob domain")
{
    auto still = EncoderGeometry::GetMotionIndicator(45.0f, 0.5f, 0.0f);
    auto slow = EncoderGeometry::GetMotionIndicator(45.0f, 0.5f, 0.02f);
    auto fast = EncoderGeometry::GetMotionIndicator(45.0f, 0.5f, 0.2f);
    DOCTEST_CHECK(still.m_end - still.m_start < 0.01f);
    DOCTEST_CHECK(slow.m_end - slow.m_start > still.m_end - still.m_start);
    DOCTEST_CHECK(fast.m_end - fast.m_start > slow.m_end - slow.m_start);
    DOCTEST_CHECK(fast.m_outerWidth > still.m_outerWidth);
    DOCTEST_CHECK(fast.m_outlineAlpha < slow.m_outlineAlpha);
    for (float value : {0.0f, 0.5f, 1.0f})
    {
        auto wide = EncoderGeometry::GetMotionIndicator(45.0f, value, 0.5f);
        DOCTEST_CHECK(wide.m_start >= 0.0f);
        DOCTEST_CHECK(wide.m_end <= 1.0f);
        DOCTEST_CHECK(wide.m_start <= value);
        DOCTEST_CHECK(wide.m_end >= value);
    }
}

DOCTEST_TEST_CASE("Encoder motion: real painting consumes voice spreads and leaves switches sharp")
{
    juce::ScopedJuceInitialiser_GUI juce;
    EncoderBankUIState ui;
    ModulatorVisualizers visualizers;
    ui.SetConnected(0, 0, true);
    ui.SetColor(0, 0, SmartGrid::Color::Red);
    ui.SetBrightness(0, 0, 1.0f);
    ui.SetNumVoices(0, 0, 1);
    ui.SetIndicatorColor(0, 0, 0, SmartGrid::Color::Red);
    ui.SetValue(0, 0, 0, 0.5f);
    ui.SetMinValue(0, 0, 0, 0.2f);
    ui.SetMaxValue(0, 0, 0, 0.8f);
    ui.SetShortName(0, 0, "MOVE");
    EncoderComponent encoder(EncoderBankUI(0, &ui, nullptr), 0, 0, false, visualizers);
    encoder.SetSize(160);
    auto render = [&]()
    {
        juce::Image image(juce::Image::ARGB, 160, 200, true);
        juce::Graphics g(image);
        encoder.paintEntireComponent(g, false);
        return image;
    };
    auto differences = [](const juce::Image& left, const juce::Image& right)
    {
        size_t count = 0;
        for (int y = 0; y < left.getHeight(); ++y)
        {
            for (int x = 0; x < left.getWidth(); ++x)
            {
                count += left.getPixelAt(x, y) != right.getPixelAt(x, y);
            }
        }

        return count;
    };
    auto still = render();
    ui.SetSpread(0, 0, 0, 0.02f);
    auto slow = render();
    ui.SetSpread(0, 0, 0, 0.15f);
    auto fast = render();
    DOCTEST_CHECK(differences(still, fast) > differences(still, slow));
    DOCTEST_CHECK(differences(still, slow) > 100);

    ui.SetBipolar(0, 0, true);
    auto bipolar = render();
    // Except for the zero marker, the indicator uses the same normalized
    // center and spread without halving either a second time.
    //
    for (int y = 0; y < 80; ++y)
    {
        for (int x = 0; x < 160; ++x)
        {
            if (std::abs(x - 80) > 4 || std::abs(y - 8) > 4)
            {
                DOCTEST_CHECK(bipolar.getPixelAt(x, y) == fast.getPixelAt(x, y));
            }
        }
    }

    ui.SetSwitchValues(0, 0, 5);
    auto switchBlur = render();
    ui.SetSpread(0, 0, 0, 0.0f);
    auto switchStill = render();
    DOCTEST_CHECK(differences(switchBlur, switchStill) == 0);

    ui.SetBipolar(0, 0, false);
    ui.SetSwitchValues(0, 0, 0);
    ui.SetNumVoices(0, 0, 4);
    for (size_t voice = 0; voice < 4; ++voice)
    {
        const SmartGrid::Color colors[] = {SmartGrid::Color::Red, SmartGrid::Color::Cyan,
            SmartGrid::Color::Pink, SmartGrid::Color::White};
        ui.SetIndicatorColor(0, 0, voice, colors[voice]);
        ui.SetValue(0, 0, voice, 0.3f + voice * 0.12f);
        ui.SetSpread(0, 0, voice, 0.03f + voice * 0.025f);
    }

    auto polyphonic = render();
    DOCTEST_CHECK(differences(fast, polyphonic) > 1000);
    if (const char* path = std::getenv("SMARTGRID_ENCODER_MOTION_PREVIEW"))
    {
        juce::Image preview(juce::Image::ARGB, 800, 230, true);
        juce::Graphics g(preview);
        g.fillAll(juce::Colour(24, 24, 28));
        const juce::Image images[] = {still, slow, fast, polyphonic, switchStill};
        const char* labels[] = {"Still", "Slow", "Audio rate", "Four voices", "Switch"};
        for (int i = 0; i < 5; ++i)
        {
            g.drawImageAt(images[i], i * 160, 0);
            g.setColour(juce::Colours::white);
            g.drawText(labels[i], i * 160, 200, 160, 24, juce::Justification::centred);
        }

        juce::FileOutputStream output{juce::File(path)};
        DOCTEST_REQUIRE(output.openedOk());
        DOCTEST_CHECK(juce::PNGImageFormat().writeImageToStream(preview, output));
    }
}

DOCTEST_TEST_CASE("Encoder motion: full-scale audio sine covers the entire dial through matrix and UI publication")
{
    juce::ScopedJuceInitialiser_GUI juce;
    SampleTimer timer;
    timer.m_samplesPerFrame = SampleTimer::x_samplesPerProcessFrame;
    juce::ScopedValueSetter<SampleTimer*> clock(SampleTimer::s_instance, &timer);
    for (float frequency : {100.0f, 1000.0f, 10000.0f})
    {
        DOCTEST_CAPTURE(frequency);
        SmartGridOneContext context;
        EncoderBankBank banks(1, 1, 1, 15, 1, &context);
        banks.InitMode(0, 1);
        banks.InitBank(0, 0, SmartGrid::Color::Red);
        banks.CreateEncoder(0, 0, 0, 0.0f, "Sine", "SINE", SmartGrid::Color::Red, 0, true);
        banks.PlaceEncoder(0, 0, 0, 0);
        banks.SelectGrid(0);
        auto& sources = banks.GetModulatorValues(0);
        sources.SetModulatorSkin(0, SmartGrid::Color::Red, "SIN");
        auto* cell = banks.GetEncoder(0);
        cell->FillModulators(&context);
        cell->m_modulators.m_modulators[0]->SetToValue(1.0f);
        SmartGridOneEncoders::VisibleMotion motion;
        EncoderBankUIState ui;
        float minValue = 1.0f;
        float maxValue = 0.0f;
        float minCenter = 1.0f;
        float maxCenter = 0.0f;
        float minSpread = 1.0f;
        float maxSpread = 0.0f;
        float largestArcStart = 0.0f;
        float smallestArcEnd = 1.0f;
        for (size_t sample = 0; sample < SampleTimer::x_hostSampleRate; ++sample)
        {
            timer.m_sample = sample * SampleTimer::x_oversample;
            float sine = std::sin(2.0 * juce::MathConstants<double>::pi * frequency * sample
                / SampleTimer::x_hostSampleRate);
            // A 0 dBFS sine has peaks at -1 and +1. The matrix consumes the
            // corresponding unipolar coordinate, not a clipped bipolar input.
            //
            sources.m_hostValue[0] = QuadFloat::Splat(0.5f + 0.5f * sine);
            context.m_sceneManager.Process();
            banks.ProcessHostFrame();
            motion.Process(banks);
            if (sample < SampleTimer::x_hostSampleRate / 4)
            {
                continue;
            }

            banks.PopulateUIState(&ui);
            motion.PopulateUIState(banks, ui);
            float center = ui.GetDisplayValue(0, 0, 0);
            float spread = ui.GetSpread(0, 0, 0);
            auto geometry = EncoderGeometry::GetMotionIndicator(144.0f, center, spread);
            minValue = std::min(minValue, ui.GetValue(0, 0, 0));
            maxValue = std::max(maxValue, ui.GetValue(0, 0, 0));
            minCenter = std::min(minCenter, center);
            maxCenter = std::max(maxCenter, center);
            minSpread = std::min(minSpread, spread);
            maxSpread = std::max(maxSpread, spread);
            largestArcStart = std::max(largestArcStart, geometry.m_start);
            smallestArcEnd = std::min(smallestArcEnd, geometry.m_end);
        }

        DOCTEST_REQUIRE(minValue == doctest::Approx(0.0f));
        DOCTEST_REQUIRE(maxValue == doctest::Approx(1.0f));
        DOCTEST_CHECK(largestArcStart == 0.0f);
        DOCTEST_CHECK(smallestArcEnd == 1.0f);
        DOCTEST_MESSAGE(frequency << " Hz: value=" << minValue << ".." << maxValue
            << ", center=" << minCenter << ".." << maxCenter
            << ", spread=" << minSpread << ".." << maxSpread
            << ", worst-case arc=" << largestArcStart << ".." << smallestArcEnd);
        if (frequency == 1000.0f)
        {
            ui.SetIndicatorColor(0, 0, 0, SmartGrid::Color::Red);
            ModulatorVisualizers visualizers;
            EncoderComponent encoder(EncoderBankUI(0, &ui, nullptr), 0, 0, false, visualizers);
            encoder.SetSize(320);
            juce::Image image(juce::Image::ARGB, 320, 400, true);
            juce::Graphics g(image);
            g.fillAll(juce::Colour(24, 24, 28));
            encoder.paintEntireComponent(g, false);
            // Probe outside the thin range arc at both endpoints to verify
            // that the thick motion indicator itself reaches each end.
            //
            for (float value : {0.0f, 1.0f})
            {
                float angle = EncoderGeometry::ValueToArcAngle(value);
                int x = static_cast<int>(std::round(160.0f + 150.0f * std::sin(angle)));
                int y = static_cast<int>(std::round(160.0f - 150.0f * std::cos(angle)));
                auto pixel = image.getPixelAt(x, y);
                DOCTEST_CHECK(pixel.getRed() > 200);
                DOCTEST_CHECK(pixel.getGreen() < 50);
            }

            if (const char* path = std::getenv("SMARTGRID_ENCODER_SINE_PREVIEW"))
            {
                g.setColour(juce::Colours::white);
                g.drawText("1 kHz sine / 0 dBFS / full depth", 0, 340, 320, 24, juce::Justification::centred);
                juce::FileOutputStream output{juce::File(path)};
                DOCTEST_REQUIRE(output.openedOk());
                DOCTEST_CHECK(juce::PNGImageFormat().writeImageToStream(image, output));
            }
        }
    }
}

DOCTEST_TEST_CASE("Encoder visualizers: group identity selects source track independently of parameter and active track")
{
    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    ui->m_squiggleBoyUIState.m_activeTrack.store(2);
    ModulatorVisualizers visualizers;
    InitializeEncoderModulatorVisualizers(visualizers, ui.get());
    EncoderUIState encoder;
    encoder.m_mode.store(Mode::VoiceWater);
    encoder.m_modulatorGroup.store(static_cast<size_t>(SmartGridOneModulators::Group::FireSource));
    encoder.m_numVoices.store(3);
    encoder.m_modulatorWeights[0][2].store(1.0f);
    auto* source = visualizers.GetForParameter(encoder);
    DOCTEST_REQUIRE(source == visualizers.Get(Mode::VoiceFire, 0));
    auto* random = dynamic_cast<GangedRandomLFOComponent<3>*>(source);
    DOCTEST_REQUIRE(random);
    DOCTEST_CHECK(random->GetGangIndex() == 1);
    DOCTEST_CHECK(random->GetFirstVoice() == 3);
    auto* sheaf = dynamic_cast<SheafyModulatorDialComponent*>(visualizers.Get(SmartGridOneModulators::Group::FireSource, 8));
    DOCTEST_REQUIRE(sheaf);
    DOCTEST_CHECK(sheaf->m_trackOverride == 1);
}

DOCTEST_TEST_CASE("Encoder visualizers: mixer audio uses a trailing window and envelopes use independent follower VUs")
{
    using namespace SmartGridOneModulators;
    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    auto& state = ui->m_squiggleBoyUIState;
    state.m_firstScopeSample.store(0);
    ModulatorVisualizers visualizers;
    InitializeEncoderModulatorVisualizers(visualizers, ui.get());
    constexpr QuadModulator x_envelopes[] =
    {
        QuadModulator::DelayDryEnvelope, QuadModulator::ReverbDryEnvelope, QuadModulator::PartialMachineDryEnvelope,
        QuadModulator::DelayWetEnvelope, QuadModulator::ReverbWetEnvelope, QuadModulator::PartialMachineWetEnvelope,
        QuadModulator::OutputEnvelope
    };
    constexpr QuadModulator x_audio[] =
    {
        QuadModulator::DelayDryAudio, QuadModulator::ReverbDryAudio, QuadModulator::PartialMachineDryAudio,
        QuadModulator::DelayWetAudio, QuadModulator::ReverbWetAudio, QuadModulator::PartialMachineWetAudio,
        QuadModulator::OutputAudio
    };
    const QuadFloat audio(-0.8f, -0.4f, 0.2f, 0.6f);
    const QuadFloat envelope(0.001f, 0.1f, 0.5f, 2.0f);
    const size_t count = static_cast<size_t>(SampleTimer::SecondsToSamples(0.125));
    for (size_t sample = 0; sample < count; ++sample)
    {
        for (size_t tap = 0; tap < 7; ++tap)
        {
            state.m_quadScopeWriter.Write(static_cast<size_t>(GetMixerScope(x_audio[tap])), audio);
            state.m_quadScopeWriter.Write(static_cast<size_t>(GetMixerScope(x_envelopes[tap])), envelope);
        }

        state.AdvanceScopeIndices();
    }

    state.m_voiceScopeWriter.Publish();
    state.m_quadScopeWriter.Publish();
    state.AcceptScopeSamples();
    for (size_t tap = 0; tap < 7; ++tap)
    {
        DOCTEST_CAPTURE(tap);
        auto* scope = dynamic_cast<RecentScopeComponent*>(visualizers.Get(SmartGridOne::EncoderBankMode::Quad, static_cast<size_t>(x_audio[tap])));
        auto* vu = dynamic_cast<EnvelopeFollowerComponent*>(visualizers.Get(SmartGridOne::EncoderBankMode::Quad, static_cast<size_t>(x_envelopes[tap])));
        DOCTEST_REQUIRE(scope);
        DOCTEST_REQUIRE(vu);
        auto axis = scope->GetTimeAxis(0);
        DOCTEST_CHECK(axis.m_end == count - 1);
        DOCTEST_CHECK(axis.GetExtent() == doctest::Approx(SampleTimer::SecondsToSamples(0.1)));
        DOCTEST_CHECK(scope->GetVoiceCount() == 4);
        for (size_t lane = 0; lane < 4; ++lane)
        {
            DOCTEST_CHECK(vu->GetEnvelope(lane) == envelope[lane]);
            DOCTEST_CHECK(ui->GetModulatorValue(scope->GetSource(lane), count - 1) == doctest::Approx(0.5f + 0.5f * audio[lane]));
        }

        juce::Image image(juce::Image::ARGB, 100, 100, true);
        juce::Graphics g(image);
        scope->Draw(g, image.getBounds());
        constexpr int x_rows[] = {86, 68, 41, 23};
        auto color = J(x_skins[static_cast<size_t>(SmartGridOneLayout::ParamMode::Quad)][static_cast<size_t>(x_audio[tap])].m_color);
        for (int row : x_rows)
        {
            auto pixel = image.getPixelAt(50, row);
            DOCTEST_CHECK(pixel.getBrightness() > color.getBrightness() * 0.6f);
            DOCTEST_CHECK(pixel.getHue() == doctest::Approx(color.getHue()).epsilon(0.02));
        }

        // All four RMS needles share the full dial's pivot at (100, 144).
        // Sample each colored ray away from the scale and the other needles.
        //
        juce::Image actual(juce::Image::ARGB, 200, 160, true);
        juce::Graphics actualGraphics(actual);
        vu->Draw(actualGraphics, actual.getBounds());
        const juce::Point<int> needlePoints[] = {{55, 99}, {78, 84}, {116, 83}, {145, 99}};
        for (size_t lane = 0; lane < 4; ++lane)
        {
            DOCTEST_CAPTURE(lane);
            auto needleColor = J(TheNonagonSmartGrid::VoiceColor(lane));
            bool found = false;
            for (int y = -2; y <= 2; ++y)
            {
                for (int x = -2; x <= 2; ++x)
                {
                    auto point = needlePoints[lane] + juce::Point<int>(x, y);
                    auto pixel = actual.getPixelAt(point.x, point.y);
                    found |= pixel.getBrightness() > needleColor.getBrightness() * 0.6f
                        && std::abs(pixel.getHue() - needleColor.getHue()) < 0.02f;
                }
            }

            DOCTEST_CHECK(found);
        }

        if (tap == 0)
        {
            if (const char* path = std::getenv("SMARTGRID_ENVELOPE_PREVIEW"))
            {
                juce::FileOutputStream output{juce::File(path)};
                DOCTEST_REQUIRE(output.openedOk());
                DOCTEST_CHECK(juce::PNGImageFormat().writeImageToStream(actual, output));
            }
        }

        state.m_quadScopeWriter.Write(static_cast<size_t>(GetMixerScope(x_envelopes[tap])), QuadFloat::Splat(99.0f));
        DOCTEST_CHECK(vu->GetEnvelope(0) == envelope[0]);
    }

    auto* scope = dynamic_cast<RecentScopeComponent*>(visualizers.Get(Group::QuadDelay, 1));
    DOCTEST_REQUIRE(scope);
    scope->m_windowMilliseconds = 25.0;
    DOCTEST_CHECK(scope->GetTimeAxis(0).GetExtent() == doctest::Approx(SampleTimer::SecondsToSamples(0.025)));
    state.AdvanceScopeIndices();
    state.m_voiceScopeWriter.Publish();
    state.m_quadScopeWriter.Publish();
    state.AcceptScopeSamples();
    DOCTEST_CHECK(scope->GetTimeAxis(0).m_end == count);
    auto* vu = dynamic_cast<EnvelopeFollowerComponent*>(visualizers.Get(Group::QuadDelay, 0));
    DOCTEST_REQUIRE(vu);
    DOCTEST_CHECK(vu->GetEnvelope(0) == 99.0f);
    for (Group group : {Group::QuadDelay, Group::QuadReverb, Group::QuadPartialMachine, Group::QuadLFO})
    {
        DOCTEST_CHECK(visualizers.Get(group, 12) == visualizers.Get(Group::QuadDelay, 12));
        DOCTEST_CHECK(visualizers.Get(group, 13) == visualizers.Get(Group::QuadDelay, 13));
    }

    for (size_t slot : {0, 1, 4, 5})
    {
        DOCTEST_CHECK(visualizers.Get(Group::QuadLFO, slot) == nullptr);
    }
}
