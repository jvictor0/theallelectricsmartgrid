#include "doctest.h"
#include "EncoderComponent.hpp"
#include "ModulatorVisualizers.hpp"
#include "ScopeComponent.hpp"
#include "GangedRandomLFOComponent.hpp"
#include "EncoderModulatorComponents.hpp"
#include "EncoderModulatorVisualizers.hpp"

namespace
{
using Mode = SmartGridOne::EncoderBankMode;
using Type = SmartGrid::EncoderType;

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
    InitializeEncoderModulatorVisualizers(visualizers, uiState);

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
        visualizers.Get(SmartGridOneEncoders::VoiceModeForTrio(trio), 6)->Draw(g, image.getBounds());
        DOCTEST_CHECK(panel.GetFirstVoice() == trio * 3);
        DOCTEST_CHECK(scope->GetFirstVoice() == trio * 3);
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
    ui.SetMode(3, 3, Mode::Voice);
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
        ui.SetNumVoices(3, 3, 1);
        for (size_t index = 0; index < 15; ++index)
        {
            ui.m_states[3][3].m_modulatorWeights[0][index].store(
                (test.m_modulators & (1 << index)) != 0 ? 0.25f : 0.0f);
        }

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

DOCTEST_TEST_CASE("Encoder visualizers: adjacent voice and quad controls render their own scopes and channel counts")
{
    juce::ScopedJuceInitialiser_GUI juce;
    EncoderBankUIState ui;
    ModulatorVisualizers visualizers;
    visualizers.Add(VoiceModulator::ModulationEnvelope, [](int*)
    {
        return std::make_unique<SolidVisualizer>(juce::Colours::red);
    });
    visualizers.Add(QuadModulator::DelayLFO, [](int*)
    {
        return std::make_unique<SolidVisualizer>(juce::Colours::blue);
    });
    ui.SetMode(0, 0, Mode::VoiceEarth);
    ui.SetNumVoices(0, 0, 3);
    ui.SetMode(1, 0, Mode::Quad);
    ui.SetNumVoices(1, 0, 4);
    for (size_t x = 0; x < 2; ++x)
    {
        ui.SetConnected(x, 0, true);
        ui.SetModulatorsAffecting(x, 0, BitSet16(1 << 4));
        for (size_t channel = 0; channel < 4; ++channel)
        {
            ui.m_states[x][0].m_modulatorWeights[channel][4].store(0.25f);
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
            ui.SetIndex(x, 0, 4);
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
    state.m_firstAudioSample.store(0);
    state.m_audioScopeWriter.m_publishedIndex.store(601);
    state.m_firstControlSample.store(0);
    auto& scope = state.m_controlScopeWriter;
    for (size_t index = 0; index <= 75; ++index)
    {
        for (size_t voice = 0; voice < 9; ++voice)
        {
            if (index == 0)
            {
                scope.RecordStart(0, voice);
            }

            if (index % 50 == 0)
            {
                scope.RecordStart(1, voice);
            }

            scope.Write(0, voice, voice >= 6 ? 0.2f : 0.9f);
            scope.Write(1, voice, 0.6f);
            ui->m_nonagonUIState.m_muted[voice].store(false);
        }

        scope.AdvanceIndex();
    }

    scope.Publish();
    ModulatorVisualizers visualizers;
    InitializeEncoderModulatorVisualizers(visualizers, ui.get());
    visualizers.m_scopeVoiceOffset = 0;
    auto& encoder = state.m_encoderBankUIState.m_states[0][0];
    encoder.m_mode.store(Mode::VoiceEarth);
    encoder.m_numVoices.store(3);
    for (size_t voice = 0; voice < 3; ++voice)
    {
        encoder.m_modulatorWeights[voice][6].store(0.1f);
        encoder.m_modulatorWeights[voice][7].store(-0.3f);
    }

    auto* selected = visualizers.GetForParameter(encoder);
    DOCTEST_REQUIRE(selected == visualizers.Get(Mode::VoiceEarth, 6));
    DOCTEST_CHECK(selected->GetTimeExtent() == 600.0);
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
    DOCTEST_CHECK(mixed.getPixelAt(197, 127) == J(GetModulatorColor(VoiceModulator::LFO1)));
    DOCTEST_CHECK(mixed.getPixelAt(100, 127) != juce::Colours::black);
    DOCTEST_CHECK(mixed.getPixelAt(100, 154) == juce::Colours::black);
    selected->ClearWeightedModulators();
    juce::Image solo(juce::Image::ARGB, 200, 200, true);
    juce::Graphics soloGraphics(solo);
    selected->Draw(soloGraphics, solo.getBounds());
    DOCTEST_CHECK(solo.getPixelAt(197, 154) == J(GetModulatorColor(VoiceModulator::LFO1)));
    DOCTEST_CHECK(solo.getPixelAt(100, 127) == juce::Colours::black);

    for (size_t voice = 0; voice < 3; ++voice)
    {
        encoder.m_modulatorWeights[voice][6].store(0.0f);
    }

    selected = visualizers.GetForParameter(encoder);
    DOCTEST_REQUIRE(selected == visualizers.Get(Mode::VoiceEarth, 7));
    DOCTEST_CHECK(selected->GetTimeExtent() == 400.0);
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
        encoder.m_numVoices.store(static_cast<int>(voices));
        for (int index = 0; index < 15; ++index)
        {
            auto* source = visualizers.Get(mode, index);
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
    state.m_firstAudioSample.store(0);
    state.m_audioScopeWriter.m_publishedIndex.store(96001);
    ModulatorVisualizers visualizers;
    InitializeEncoderModulatorVisualizers(visualizers, ui.get());
    visualizers.m_scopeVoiceOffset = 1;
    auto& encoder = state.m_encoderBankUIState.m_states[0][0];
    encoder.m_mode.store(Mode::VoiceFire);
    encoder.m_numVoices.store(3);
    encoder.m_modulatorWeights[1][11].store(-0.25f);
    encoder.m_modulatorWeights[1][8].store(0.25f);
    auto* selected = visualizers.GetForParameter(encoder);
    DOCTEST_REQUIRE(selected == &visualizers.m_recentHistory);
    DOCTEST_CHECK(selected->GetTimeExtent() == SampleTimer::x_sampleRate);
    auto axis = visualizers.m_recentHistory.GetTimeAxis(1);
    DOCTEST_CHECK(axis.GetSample(0.0) == 48000.0);
    DOCTEST_CHECK(axis.GetSample(1.0) == 96000.0);
    juce::Image image(juce::Image::ARGB, 200, 200, true);
    juce::Graphics g(image);
    selected->Draw(g, image.getBounds());
    DOCTEST_CHECK(image.getPixelAt(197, 145) == J(SmartGrid::Color::White));
    DOCTEST_REQUIRE(visualizers.m_recentHistory.m_weightedModulators[1].m_count == 2);
    DOCTEST_CHECK(visualizers.m_recentHistory.m_weightedModulators[1].m_entries[1].m_weight == -0.5f);
    encoder.m_modulatorWeights[1][11].store(0.0f);
    encoder.m_modulatorWeights[1][8].store(0.0f);
    DOCTEST_CHECK(visualizers.GetForParameter(encoder) == nullptr);

    encoder.m_mode.store(Mode::Global);
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
    state.m_firstAudioSample.store(100);
    state.m_audioScopeWriter.m_publishedIndex.store(33);
    state.m_firstControlSample.store(104);
    GangedRandomLFOVoice voices[4];
    GangedRandomLFOVoiceInput inputs[4];
    for (size_t voice = 0; voice < 4; ++voice)
    {
        voices[voice].m_source = 0.2f;
        voices[voice].m_target = 0.8f;
        inputs[voice] = {0.01, 0.01, 0.0f};
    }

    state.m_quadGangedRandomLFOUIState[0].m_history.RecordRound(100, voices, inputs);
    for (size_t sample = 0; sample < 4; ++sample)
    {
        state.m_quadControlScopeWriter.Write(4, 0, 0.4f);
        state.m_quadControlScopeWriter.AdvanceIndex();
    }

    state.m_quadControlScopeWriter.Publish();
    ModulatorVisualizers visualizers;
    InitializeEncoderModulatorVisualizers(visualizers, ui.get());
    auto& encoder = state.m_encoderBankUIState.m_states[0][0];
    encoder.m_mode.store(Mode::Quad);
    encoder.m_numVoices.store(4);
    encoder.m_modulatorWeights[0][0].store(0.1f);
    encoder.m_modulatorWeights[0][6].store(0.3f);
    auto* selected = visualizers.GetForParameter(encoder);
    DOCTEST_REQUIRE(selected == visualizers.Get(Mode::Quad, 0));
    DOCTEST_CHECK(selected->GetTimeExtent() == 200.0);
    juce::Image image(juce::Image::ARGB, 208, 208, true);
    juce::Graphics g(image);
    selected->Draw(g, image.getBounds());
    DOCTEST_CHECK(image.getPixelAt(36, 134) == J(GetModulatorColor(QuadModulator::Random1)));
    bool futureCurve = false;
    for (int y = 115; y < 125; ++y)
    {
        futureCurve |= image.getPixelAt(158, y) != juce::Colours::black;
    }

    DOCTEST_CHECK(futureCurve);
}
