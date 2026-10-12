#include "doctest.h"
#include <JuceHeader.h>
#include "ScopeComponent.hpp"
#include "QuadDelayEnvelopeVisualizerComponent.hpp"
#include <memory>

namespace
{
struct PeakedResponse : TransferFunction
{
    float m_frequency;

    explicit PeakedResponse(float frequency)
        : m_frequency(frequency)
    {
    }

    float FrequencyResponse(float frequency) const override
    {
        float distance = std::log(frequency / m_frequency) / 0.03f;
        return std::exp(-distance * distance);
    }

    std::complex<float> TransferFunctionValue(float frequency) const override
    {
        return {FrequencyResponse(frequency), 0.0f};
    }
};

template<typename Matches>
float FindPeakX(const juce::Image& image, Matches matches)
{
    for (int y = 0; y < image.getHeight(); ++y)
    {
        int sumX = 0;
        int count = 0;
        for (int x = 0; x < image.getWidth(); ++x)
        {
            if (matches(image.getPixelAt(x, y)))
            {
                sumX += x;
                ++count;
            }
        }

        if (count > 0)
        {
            return static_cast<float>(sumX) / count;
        }
    }

    DOCTEST_FAIL("No curve pixels found");
    return -1.0f;
}
}

DOCTEST_TEST_CASE("Analyser: spectrum and oversampled response share the frequency axis")
{
    bool logX = true;
    DOCTEST_SUBCASE("logarithmic")
    {
        logX = true;
    }

    DOCTEST_SUBCASE("linear")
    {
        logX = false;
    }

    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    ui->m_squiggleBoyUIState.m_activeTrack.store(1);
    auto& filter = ui->m_squiggleBoyUIState.m_voiceFilterUIState[3];
    filter.m_lpLadder.m_alpha.store(1.0f);
    filter.m_lpLadder.m_feedback.store(0.0f);
    filter.m_hp4Pole.m_g.store(0.0f);
    filter.m_hp4Pole.m_k.store(1.0f);
    auto writer = std::make_unique<ScopeWriter>(9, 1);
    int voiceOffset = 0;

    for (int bin : {4, 16, 64, 192})
    {
        DOCTEST_CAPTURE(bin);
        float frequency = static_cast<float>(bin) / BasicWaveTable::x_tableSize;
        for (size_t sample = 0; sample < BasicWaveTable::x_tableSize; ++sample)
        {
            float value = 0.05f * std::sin(2.0f * static_cast<float>(M_PI) * frequency * sample);
            writer->Write(0, 3, value);
            writer->AdvanceIndex();
        }

        writer->Publish();
        AnalyserComponent analyser(WindowedFFT(writer.get(), 0), &voiceOffset, ui.get());
        analyser.m_logX = logX;
        juce::Image spectrum(juce::Image::ARGB, 1024, 400, true);
        juce::Graphics spectrumGraphics(spectrum);
        analyser.Draw(spectrumGraphics, spectrum.getBounds());

        PeakedResponse response(frequency / SquiggleBoyVoice::x_oversample);
        PathDrawer drawer(400, 1024, 0, 0);
        drawer.m_logX = logX;
        juce::Image transfer(juce::Image::ARGB, 1024, 400, true);
        juce::Graphics transferGraphics(transfer);
        drawer.DrawFrequencyResponse(transferGraphics, juce::Colours::white, response,
            1.0f / SquiggleBoyVoice::x_oversample);

        float spectrumX = FindPeakX(spectrum, [](juce::Colour pixel)
        {
            return pixel.getGreen() > 24 && pixel.getGreen() > 2 * pixel.getRed()
                && pixel.getGreen() > 2 * pixel.getBlue();
        });
        float transferX = FindPeakX(transfer, [](juce::Colour pixel)
        {
            return pixel.getAlpha() > 24;
        });
        DOCTEST_CAPTURE(spectrumX);
        DOCTEST_CAPTURE(transferX);
        DOCTEST_CHECK(std::abs(spectrumX - transferX) <= 3.0f);
    }
}

DOCTEST_TEST_CASE("Scope: reduced point counts preserve the time axis and end marker")
{
    auto writer = std::make_unique<ScopeWriter>(1, 1);
    writer->RecordStart(0, 0);
    for (size_t sample = 0; sample < 128; ++sample)
    {
        writer->Write(0, 0, static_cast<float>(sample) / 127.0f);
        writer->AdvanceIndex();
    }

    writer->Publish();
    for (size_t points : {128, 256, 1024})
    {
        DOCTEST_CAPTURE(points);
        ScopeReader reader(writer.get(), 0, 0, points, 1);
        PathDrawer drawer(100, 200, 0, 0);
        juce::Image image(juce::Image::ARGB, 204, 104, true);
        juce::Graphics g(image);
        drawer.DrawScopePath(g, juce::Colours::white, reader, 1, 0);
        for (int x : {25, 100, 175})
        {
            int y = 100 - x / 2;
            bool painted = false;
            for (int dy = -2; dy <= 2; ++dy)
            {
                painted |= image.getPixelAt(x, y + dy).getAlpha() > 0;
            }

            DOCTEST_CHECK(painted);
        }

        drawer.DrawScopeMarker(g, juce::Colours::cyan, reader, 1, 0);
        DOCTEST_CHECK(image.getPixelAt(200, 1) == juce::Colours::cyan);
    }
}

DOCTEST_TEST_CASE("Delay waveform: full resolution retains peaks in individual source bins")
{
    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    auto& state = ui->m_squiggleBoyUIState.m_delayUIState;
    for (size_t offset = 0; offset < 4; ++offset)
    {
        DOCTEST_CAPTURE(offset);
        auto& snapshot = state.BeginSnapshot();
        snapshot = QuadDelay::UISnapshot();
        for (auto& lane : snapshot.m_delayEnvelopeState)
        {
            for (size_t firstBin : {0, 256, 1020})
            {
                lane.m_maxEnvelope[firstBin + offset] = 1;
                lane.m_minEnvelope[firstBin + offset] = -1;
            }
        }

        state.CommitSnapshot();
        QuadDelayEnvelopeVisualizerComponent visualizer(ui.get());
        juce::Image image(juce::Image::ARGB, 1024, 400, true);
        juce::Graphics g(image);
        visualizer.Draw(g, image.getBounds());
        for (int lane = 0; lane < 4; ++lane)
        {
            for (int firstBin : {0, 256, 1020})
            {
                int x = firstBin + static_cast<int>(offset);
                DOCTEST_CHECK(image.getPixelAt(x, lane * 100 + 5).getRed() > 100);
                DOCTEST_CHECK(image.getPixelAt(x, lane * 100 + 95).getRed() > 100);
            }

            DOCTEST_CHECK(image.getPixelAt(32, lane * 100 + 5) == juce::Colours::black);
        }
    }
}
