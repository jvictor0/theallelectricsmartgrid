#include "doctest.h"
#include <JuceHeader.h>
#include "MasteringComponents.hpp"
#include "PartialMachineComponents.hpp"
#include <cmath>
#include <limits>
#include <memory>
#include <vector>

namespace
{
constexpr float x_activeOmega = 3000.0f / SampleTimer::x_hostSampleRate;
const float x_invalidFrequencies[] =
{
    -1.0f, 0.0f, -0.0f,
    std::numeric_limits<float>::quiet_NaN(),
    std::numeric_limits<float>::infinity(),
    -std::numeric_limits<float>::infinity()
};

// Observe the actual graphics boundary: a rasterizer may silently drop a NaN line.
//
struct MarkerRenderer : juce::LowLevelGraphicsSoftwareRenderer
{
    std::vector<juce::Line<float>> m_lines;
    std::vector<juce::Rectangle<float>> m_ellipses;
    size_t m_numStrokes = 0;

    explicit MarkerRenderer(const juce::Image& image)
        : juce::LowLevelGraphicsSoftwareRenderer(image)
    {
    }

    void drawLineWithThickness(const juce::Line<float>& line, float thickness) override
    {
        m_lines.push_back(line);
        DOCTEST_REQUIRE(std::isfinite(line.getStartX()));
        DOCTEST_REQUIRE(std::isfinite(line.getStartY()));
        DOCTEST_REQUIRE(std::isfinite(line.getEndX()));
        DOCTEST_REQUIRE(std::isfinite(line.getEndY()));
        juce::LowLevelGraphicsSoftwareRenderer::drawLineWithThickness(line, thickness);
    }

    void fillEllipse(const juce::Rectangle<float>& area) override
    {
        m_ellipses.push_back(area);
        DOCTEST_REQUIRE(std::isfinite(area.getX()));
        DOCTEST_REQUIRE(std::isfinite(area.getY()));
        DOCTEST_REQUIRE(std::isfinite(area.getWidth()));
        DOCTEST_REQUIRE(std::isfinite(area.getHeight()));
        juce::LowLevelGraphicsSoftwareRenderer::fillEllipse(area);
    }

    void strokePath(const juce::Path& path, const juce::PathStrokeType& stroke,
        const juce::AffineTransform& transform) override
    {
        ++m_numStrokes;
        juce::LowLevelGraphicsSoftwareRenderer::strokePath(path, stroke, transform);
    }
};

void MuteVoices(TheNonagonSquiggleBoyInternal::UIState& ui)
{
    for (auto& muted : ui.m_nonagonUIState.m_muted)
    {
        muted.store(true);
    }
}

void CheckActiveMarker(const MarkerRenderer& renderer, const juce::Image& image, juce::Colour colour)
{
    DOCTEST_REQUIRE(renderer.m_lines.size() == 1);
    const auto& line = renderer.m_lines.front();
    // 3 kHz is seven octaves above the 23.4375 Hz minimum on the twelve-octave axis.
    //
    float expectedX = image.getWidth() * 7.0f / 12.0f;
    DOCTEST_CHECK(line.getStartX() == doctest::Approx(expectedX));
    DOCTEST_CHECK(line.getEndX() == doctest::Approx(expectedX));
    DOCTEST_CHECK(line.getLength() > 10.0f);
    int y = static_cast<int>((line.getStartY() + line.getEndY()) / 2.0f);
    auto pixel = image.getPixelAt(static_cast<int>(expectedX), y);
    DOCTEST_CHECK(pixel.getBrightness() > 0.5f * colour.getBrightness());
    DOCTEST_CHECK(pixel.getHue() == doctest::Approx(colour.getHue()).epsilon(0.01));
    DOCTEST_CHECK(pixel.getSaturation() == doctest::Approx(colour.getSaturation()).epsilon(0.01));
}
}

DOCTEST_TEST_CASE("Spectral markers: initial and published unassigned voices draw no markers")
{
    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    auto vocoder = std::make_unique<DeepVocoder>();
    auto& state = ui->m_squiggleBoyUIState.m_deepVocoderUIState;
    SourceMixerFrequencyComponent component(ui.get());
    MuteVoices(*ui);
    for (bool published : {false, true})
    {
        DOCTEST_CAPTURE(published);
        if (published)
        {
            vocoder->PopulateUIState(&state);
        }

        for (size_t voice = 0; voice < DeepVocoder::x_numVoices; ++voice)
        {
            DOCTEST_CHECK(state.GetUsedOmega(voice) == (published ? -1.0f : 0.0f));
        }

        juce::Image image(juce::Image::ARGB, 1000, 200, true);
        MarkerRenderer renderer(image);
        juce::Graphics graphics(renderer);
        component.Draw(graphics, image.getBounds());
        DOCTEST_CHECK(renderer.m_lines.empty());
    }
}

DOCTEST_TEST_CASE("Spectral markers: invalid voice frequencies leave the active marker intact")
{
    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    auto& state = ui->m_squiggleBoyUIState.m_deepVocoderUIState;
    SourceMixerFrequencyComponent component(ui.get());
    MuteVoices(*ui);
    for (float invalid : x_invalidFrequencies)
    {
        DOCTEST_CAPTURE(invalid);
        for (auto& voice : state.m_voiceUIState)
        {
            voice.m_usedOmega.store(invalid);
            voice.m_pitchCenter.store(x_activeOmega);
            voice.m_threshold.store(0.01f);
            voice.m_atomMagnitude.store(0.25f);
        }

        state.m_voiceUIState[4].m_usedOmega.store(x_activeOmega);
        juce::Image image(juce::Image::ARGB, 1000, 200, true);
        MarkerRenderer renderer(image);
        juce::Graphics graphics(renderer);
        component.Draw(graphics, image.getBounds());
        CheckActiveMarker(renderer, image, J(TheNonagonSmartGrid::VoiceColor(4)));
    }
}

DOCTEST_TEST_CASE("Spectral markers: invalid vocoder atoms leave the active atom intact")
{
    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    auto& state = ui->m_squiggleBoyUIState.m_deepVocoderUIState;
    SourceMixerFrequencyComponent component(ui.get());
    MuteVoices(*ui);
    for (float invalid : x_invalidFrequencies)
    {
        DOCTEST_CAPTURE(invalid);
        auto& snapshot = state.BeginSnapshot();
        snapshot.m_numFrequencies = 2;
        snapshot.m_frequencies[0] = invalid;
        snapshot.m_frequencies[1] = x_activeOmega;
        snapshot.m_magnitudes[0] = snapshot.m_magnitudes[1] = 0.25f;
        state.CommitSnapshot();
        juce::Image image(juce::Image::ARGB, 1000, 200, true);
        MarkerRenderer renderer(image);
        juce::Graphics graphics(renderer);
        component.Draw(graphics, image.getBounds());
        CheckActiveMarker(renderer, image, juce::Colours::white);
    }
}

DOCTEST_TEST_CASE("Spectral markers: pitch curves require a finite positive center")
{
    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    SourceMixer::Source source;
    for (auto& state : ui->m_squiggleBoyUIState.m_sourceMixerUIState.m_sources)
    {
        state.m_history.Push(source.MakeSnapshot());
    }

    auto& voice = ui->m_squiggleBoyUIState.m_deepVocoderUIState.m_voiceUIState[0];
    SourceMixerFrequencyComponent component(ui.get());
    MuteVoices(*ui);
    ui->m_nonagonUIState.m_muted[0].store(false);
    voice.m_threshold.store(0.01f);
    for (float invalid : x_invalidFrequencies)
    {
        DOCTEST_CAPTURE(invalid);
        voice.m_pitchCenter.store(invalid);
        juce::Image image(juce::Image::ARGB, 1000, 200, true);
        MarkerRenderer renderer(image);
        juce::Graphics graphics(renderer);
        component.Draw(graphics, image.getBounds());
        size_t baseStrokes = renderer.m_numStrokes;
        DOCTEST_CHECK(baseStrokes == 3 * SourceMixer::x_numSources);
        voice.m_pitchCenter.store(x_activeOmega);
        component.Draw(graphics, image.getBounds());
        DOCTEST_CHECK(renderer.m_numStrokes == 2 * baseStrokes + 1);
    }
}

DOCTEST_TEST_CASE("Spectral markers: partial machine skips invalid atoms before spectral and spatial calculations")
{
    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    auto& state = ui->m_squiggleBoyUIState.m_partialMachineUIState;
    PartialMachine::Input input;
    input.m_synthesisContextInput.m_volume = PartialMachine::Parameter(1.0f);
    input.m_synthesisContextInput.m_bwBaseFrequency = PartialMachine::Parameter(x_activeOmega);
    input.m_synthesisContextInput.m_bwWidth = PartialMachine::Parameter(4.0f);
    input.m_synthesisContextInput.m_bassCutoff = PartialMachine::Parameter(x_activeOmega / 8.0f);
    state.FromInput(input);
    PartialMachineInputSpectrumComponent spectrum(ui.get());
    PartialMachineSpatialComponent spatial(ui.get());
    for (float invalid : x_invalidFrequencies)
    {
        DOCTEST_CAPTURE(invalid);
        auto& snapshot = state.BeginSnapshot();
        snapshot.m_numAtoms = 2;
        snapshot.m_atoms[0].m_synthesisOmega = invalid;
        snapshot.m_atoms[1].m_synthesisOmega = x_activeOmega;
        snapshot.m_atoms[0].m_synthesisMagnitude = snapshot.m_atoms[1].m_synthesisMagnitude = 0.25f;
        state.CommitSnapshot();
        juce::Image image(juce::Image::ARGB, 1000, 200, true);
        MarkerRenderer renderer(image);
        juce::Graphics graphics(renderer);
        spectrum.Draw(graphics, image.getBounds());
        CheckActiveMarker(renderer, image,
            PartialMachineComponentUtils::ColourFromIndex(snapshot.m_atoms[1].m_index));
        spatial.Draw(graphics, image.getBounds());
        DOCTEST_CHECK(renderer.m_ellipses.size() == 1);
    }
}
