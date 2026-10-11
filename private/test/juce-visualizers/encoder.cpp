#include "doctest.h"
#include "EncoderComponent.hpp"
#include "ModulatorVisualizers.hpp"
#include "ScopeComponent.hpp"
#include "GangedRandomLFOComponent.hpp"
#include "EncoderModulatorComponents.hpp"

namespace
{
using Mode = SmartGridOne::EncoderBankMode;
using Type = SmartGrid::EncoderType;

struct SolidVisualizer : SmartGridOneMainVisualizerComponent
{
    juce::Colour m_color;
    int m_draws = 0;

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
    state.m_sampleRate.store(48000);
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
        DOCTEST_CHECK(image.getPixelAt(54, y) == J(color));
    }
}
}

DOCTEST_TEST_CASE("Modulator markers: random dots use the source color in every voice color mode")
{
    CheckRandomMarkers<3>(GangedRandomLFOComponent<3>::ColorMode::TrackVoices);
    CheckRandomMarkers<4>(GangedRandomLFOComponent<4>::ColorMode::Voices);
    CheckRandomMarkers<1>(GangedRandomLFOComponent<1>::ColorMode::Fixed);
}

DOCTEST_TEST_CASE("Modulator markers: both quad LFO families draw dots in their source color")
{
    auto writer = std::make_unique<ScopeWriter>(4, 1);
    for (size_t voice = 0; voice < 4; ++voice)
    {
        writer->RecordStart(0, voice);
    }

    for (size_t sample = 0; sample < 2; ++sample)
    {
        for (size_t voice = 0; voice < 4; ++voice)
        {
            writer->Write(0, voice, 0.2f + 0.2f * voice);
        }

        writer->AdvanceIndex();
    }

    writer->Publish();
    for (bool bipolar : {false, true})
    {
        auto color = GetModulatorColor(bipolar ? QuadModulator::DelayLFO : QuadModulator::LFO1);
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
#define H(modulator, ctor) \
    visualizers.Add(modulator, [&]([[maybe_unused]] int* scopeVoiceOffset) \
    { \
        return ctor; \
    });
#define G(name, slot, ctor, mode, color, modulator) H(modulator, ctor)
#include "ForEachModulationVisualizer.hpp"
#undef G
#undef H

    for (Mode mode : {Mode::VoiceWater, Mode::VoiceFire, Mode::VoiceEarth, Mode::Quad, Mode::Global})
    {
        for (int index = 0; index < 15; ++index)
        {
            DOCTEST_CAPTURE(mode);
            DOCTEST_CAPTURE(index);
            bool expected = index == 14 || (mode == Mode::Global ? index == 2 || index == 3
                : index < 8 || index == 11 || (SmartGridOneEncoders::IsVoiceMode(mode) && index < 11));
            DOCTEST_CHECK((visualizers.Get(mode, index) != nullptr) == expected);
        }
    }

    for (int index : {8, 9, 10, 11, 14})
    {
        auto* source = visualizers.Get(Mode::Voice, index);
        DOCTEST_REQUIRE(source != nullptr);
        DOCTEST_CHECK(visualizers.GetSingle(Mode::Voice, 1 << index) == source);
        DOCTEST_CHECK(visualizers.GetSingle(Mode::Voice, (1 << index) | (1 << 12) | (1 << 13)) == source);
        DOCTEST_CHECK(visualizers.GetSingle(Mode::Voice, (1 << index) | (1 << 6)) == nullptr);
    }

    auto* scope = dynamic_cast<ScopeComponent*>(visualizers.Get(Mode::Voice, 6));
    DOCTEST_REQUIRE(scope != nullptr);
    auto& writer = uiState->m_squiggleBoyUIState.m_controlScopeWriter;
    DOCTEST_CHECK(scope->m_scopeWriter == &writer);
    DOCTEST_CHECK(scope->m_scopeIx == static_cast<size_t>(SmartGridOne::ControlScopes::LFO1));
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
        scope->Draw(g, image.getBounds());
        DOCTEST_CHECK(panel.m_scopeReaderFactory.m_voiceIx == trio * 3 + 1);
        DOCTEST_CHECK(scope->m_scopeReaderFactory.m_voiceIx == trio * 3 + 2);
        auto* random = dynamic_cast<GangedRandomLFOComponent<3>*>(visualizers.Get(Mode::Voice, 0));
        DOCTEST_REQUIRE(random != nullptr);
        DOCTEST_CHECK(random->GetGangIndex() == trio);
    }
}

DOCTEST_TEST_CASE("Encoder visualizers: typed associations share voice views and separate modes")
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
    DOCTEST_CHECK(created == 3);
    auto* voice = visualizers.Get(Mode::VoiceWater, 2);
    DOCTEST_REQUIRE(voice != nullptr);
    DOCTEST_CHECK(visualizers.Get(Mode::VoiceFire, 2) == voice);
    DOCTEST_CHECK(visualizers.Get(Mode::VoiceEarth, 2) == voice);
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
    ui.SetIndicatorColor(0, SmartGrid::Color::White);
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
    ui.SetMode(Mode::Global);
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
    ui.SetMode(Mode::Voice);
    encoder.paint(g);
    DOCTEST_CHECK(visualizer->m_draws == 1);
}

DOCTEST_TEST_CASE("Encoder visualizers: base parameters follow one eligible source while depths retain their own")
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
        {Mode::Voice, Type::BaseParam, 0, 65, 0, juce::Colours::transparentBlack},
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
        ui.SetMode(test.m_mode);
        ui.SetEncoderType(3, 3, test.m_type);
        ui.SetIndex(3, 3, test.m_index);
        ui.SetModulatorsAffecting(3, 3, BitSet16(test.m_modulators));
        ui.SetGesturesAffecting(3, 3, BitSet16(test.m_gestures));
        juce::Image image(juce::Image::ARGB, 100, 125, true);
        juce::Graphics g(image);
        encoder.paint(g);
        auto expected = test.m_background.isTransparent() ? test.m_background : test.m_background.withAlpha(0.5f);
        DOCTEST_CHECK(image.getPixelAt(50, 50) == expected);
    }
}
