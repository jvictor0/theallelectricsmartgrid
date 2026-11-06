#include "doctest.h"
#include "TuningVisualizerComponent.hpp"
#include "SmartGridOneParamVisualizers.hpp"
#include <cstdlib>
#include <limits>
#include <memory>

namespace
{
using Visualizer = TuningVisualizerComponent;

struct TuningRig
{
    std::unique_ptr<TheNonagonSquiggleBoyInternal::UIState> m_ui;
    Visualizer m_visualizer;

    TuningRig()
        : m_ui(std::make_unique<TheNonagonSquiggleBoyInternal::UIState>())
        , m_visualizer(m_ui.get())
    {
        m_ui->m_masterTuneHz.store(187.5f);
        double tempo = 187.5 * std::exp2(25.0 / 1200.0);
        m_ui->m_tempoHz.store(tempo / 512.0);
        for (size_t voice = 0; voice < SquiggleBoy::x_numVoices; ++voice)
        {
            double cents = -40.0 + 10.0 * static_cast<double>(voice);
            m_ui->m_voiceFrequencyHz[voice].store(static_cast<float>(tempo * std::exp2(cents / 1200.0)));
            m_ui->m_nonagonUIState.m_muted[voice].store(false);
        }
    }
};

bool HasFraction(const std::vector<Visualizer::RuleMark>& marks, int numerator, int denominator, double cents)
{
    for (const auto& mark : marks)
    {
        if (mark.m_fraction.m_numerator == numerator && mark.m_fraction.m_denominator == denominator)
        {
            return std::abs(mark.m_cents - cents) < 1.0e-6;
        }
    }

    return false;
}

bool HasColor(const juce::Image& image, juce::Colour color)
{
    for (int y = 0; y < image.getHeight(); ++y)
    {
        for (int x = 0; x < image.getWidth(); ++x)
        {
            auto pixel = image.getPixelAt(x, y);
            if (pixel.getFloatAlpha() > 0.25f && pixel.getBrightness() > 0.25f
                && std::abs(pixel.getHue() - color.getHue()) < 0.015f
                && std::abs(pixel.getSaturation() - color.getSaturation()) < 0.015f)
            {
                return true;
            }
        }
    }

    return false;
}
}

DOCTEST_TEST_CASE("Tuning visualizer: octave folding and reciprocal fraction rulers follow the knob")
{
    DOCTEST_CHECK(Visualizer::OctavePosition(187.5) == doctest::Approx(0.0));
    DOCTEST_CHECK(Visualizer::OctavePosition(375.0) == doctest::Approx(0.0));
    DOCTEST_CHECK(Visualizer::OctavePosition(187.5 * std::sqrt(2.0)) == doctest::Approx(0.5));
    DOCTEST_CHECK(Visualizer::RelativeCents(375.0 * std::exp2(-25.0 / 1200.0), 187.5)
        == doctest::Approx(-25.0));
    double tempo = 187.5 * 1.5 / 512.0;
    DOCTEST_CHECK(HasFraction(Visualizer::MakeRuler(tempo, 187.5), 3, 2, 0.0));
    DOCTEST_CHECK(HasFraction(Visualizer::MakeRuler(187.5, tempo), 4, 3, 0.0));
    for (int octave = -8; octave <= 8; ++octave)
    {
        DOCTEST_CHECK(HasFraction(Visualizer::MakeRuler(tempo, std::ldexp(187.5, octave)), 3, 2, 0.0));
    }

    DOCTEST_CHECK(HasFraction(Visualizer::MakeRuler(tempo * std::exp2(25.0 / 1200.0), 187.5), 3, 2, -25.0));
    DOCTEST_CHECK(HasFraction(Visualizer::MakeRuler(tempo * std::exp2(50.0 / 1200.0), 187.5), 3, 2, -50.0));
    DOCTEST_CHECK_FALSE(HasFraction(Visualizer::MakeRuler(tempo * std::exp2(51.0 / 1200.0), 187.5), 3, 2, -51.0));
    DOCTEST_CHECK(Visualizer::MakeRuler(0.0, 187.5).empty());
    DOCTEST_CHECK(Visualizer::MakeRuler(1.0, std::numeric_limits<double>::infinity()).empty());
}

DOCTEST_TEST_CASE("Tuning visualizer: all unmuted voices appear independently of the selected trio")
{
    TuningRig rig;
    Visualizer::Frame frame;
    rig.m_ui->m_squiggleBoyUIState.m_activeTrack.store(2);
    DOCTEST_REQUIRE(rig.m_visualizer.PrepareFrame(frame));
    DOCTEST_CHECK(frame.m_numVoices == 9);
    rig.m_ui->m_nonagonUIState.m_muted[0].store(true);
    rig.m_ui->m_nonagonUIState.m_muted[4].store(true);
    rig.m_ui->m_voiceFrequencyHz[8].store(std::numeric_limits<float>::quiet_NaN());
    DOCTEST_REQUIRE(rig.m_visualizer.PrepareFrame(frame));
    DOCTEST_CHECK(frame.m_numVoices == 6);
    for (size_t i = 0; i < frame.m_numVoices; ++i)
    {
        DOCTEST_CHECK(frame.m_voices[i].m_index != 0);
        DOCTEST_CHECK(frame.m_voices[i].m_index != 4);
        DOCTEST_CHECK(frame.m_voices[i].m_index != 8);
    }

    rig.m_ui->m_tempoHz.store(0.0);
    DOCTEST_CHECK_FALSE(rig.m_visualizer.PrepareFrame(frame));
}

DOCTEST_TEST_CASE("Tuning visualizer: only the tempo dial shows voice colors")
{
    juce::ScopedJuceInitialiser_GUI gui;
    TuningRig rig;
    Visualizer::Frame frame;
    DOCTEST_REQUIRE(rig.m_visualizer.PrepareFrame(frame));
    for (bool showVoices : {true, false})
    {
        juce::Image image(juce::Image::ARGB, 300, 160, true);
        juce::Graphics graphics(image);
        Visualizer::DrawDial(graphics, {150.0f, 100.0f}, 180.0f, frame.m_tempoHz,
            frame.m_tuneHz, juce::Colours::yellow, juce::Colours::white, frame, showVoices);
        DOCTEST_CHECK(HasColor(image, J(TheNonagonSmartGrid::VoiceColor(1))) == showVoices);
        DOCTEST_CHECK(HasColor(image, J(TheNonagonSmartGrid::VoiceColor(7))) == showVoices);
    }
}

DOCTEST_TEST_CASE("Tuning visualizer: renders the circle inside its slot at narrow and full sizes")
{
    juce::ScopedJuceInitialiser_GUI gui;
    TuningRig rig;
    for (int size : {168, 336, 672})
    {
        DOCTEST_CAPTURE(size);
        juce::Image image(juce::Image::ARGB, size + 40, size + 40, true);
        juce::Graphics graphics(image);
        rig.m_visualizer.Draw(graphics, {20, 20, size, size});
        DOCTEST_CHECK(image.getPixelAt(0, 0).getAlpha() == 0);
        DOCTEST_CHECK(image.getPixelAt(size + 20, size + 20).getAlpha() == 0);
        DOCTEST_CHECK(HasColor(image, juce::Colours::yellow));
        DOCTEST_CHECK(HasColor(image, juce::Colours::white));
        DOCTEST_CHECK(HasColor(image, J(TheNonagonSmartGrid::VoiceColor(1))));
        if (size == 672)
        {
            if (const char* path = std::getenv("SMARTGRID_TUNING_PREVIEW"))
            {
                juce::FileOutputStream stream{juce::File(path)};
                DOCTEST_REQUIRE(stream.openedOk());
                DOCTEST_CHECK(juce::PNGImageFormat().writeImageToStream(image, stream));
            }
        }
    }

    using Mapping = SmartGridOneParamVisualizers;
    DOCTEST_CHECK(Mapping::Get(Mapping::Param::MasterTune) == Mapping::Visualizer::TuningRelations);
    DOCTEST_CHECK(Mapping::Get(Mapping::Param::Tempo) == Mapping::Visualizer::TuningRelations);
}

DOCTEST_TEST_CASE("Tuning visualizer: coincident octave unisons retain all voice colors on the circle")
{
    juce::ScopedJuceInitialiser_GUI gui;
    TuningRig rig;
    rig.m_ui->m_tempoHz.store(187.5 / 512.0);
    for (size_t voice = 0; voice < SquiggleBoy::x_numVoices; ++voice)
    {
        rig.m_ui->m_voiceFrequencyHz[voice].store(std::ldexp(187.5f, static_cast<int>(voice)));
    }

    juce::Image image(juce::Image::ARGB, 672, 672, true);
    juce::Graphics graphics(image);
    rig.m_visualizer.Draw(graphics, image.getBounds());
    auto spokes = image.getClippedImage({320, 30, 32, 65});
    for (size_t voice = 0; voice < SquiggleBoy::x_numVoices; ++voice)
    {
        DOCTEST_CAPTURE(voice);
        DOCTEST_CHECK(HasColor(spokes, J(TheNonagonSmartGrid::VoiceColor(voice))));
    }
}
