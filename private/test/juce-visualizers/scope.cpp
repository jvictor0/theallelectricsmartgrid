#include "doctest.h"
#include <JuceHeader.h>
#include "ScopeComponent.hpp"
#include "QuadDelayEnvelopeVisualizerComponent.hpp"
#include "PartialMachineComponents.hpp"
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

struct FlatSampledResponse : FilterResponseSnapshot
{
    float m_gain = 1;

    std::complex<float> ComplexResponse(const ResponseFrequency&, size_t) const override
    {
        return {m_gain, 0};
    }
};

// Advances the writer to its next stored sample, so each write is stored.
//
void AdvanceStoredSample(ScopeWriter& writer)
{
    for (size_t i = 0; i < writer.m_decimation; ++i)
    {
        writer.AdvanceIndex();
    }
}

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

DOCTEST_TEST_CASE("Spectrum axis: fixed low frequency through engine Nyquist with host-rate atom alignment")
{
    PathDrawer drawer;
    DOCTEST_CHECK(drawer.m_bucketExpX[0] * SampleTimer::x_sampleRate == doctest::Approx(23.4375));
    DOCTEST_CHECK(drawer.m_bucketExpX[PathDrawer::x_numPoints - 1] == doctest::Approx(0.5f));
    for (size_t point : {0, 256, 512, 1023})
    {
        DOCTEST_CHECK(PathDrawer::LinearToLog(drawer.m_bucketExpX[point])
            == doctest::Approx(static_cast<float>(point) / (PathDrawer::x_numPoints - 1)));
    }

    float engineFrequency = 3000.0f / SampleTimer::x_sampleRate;
    float hostFrequency = 3000.0f / SampleTimer::x_hostSampleRate;
    DOCTEST_CHECK(PartialMachineComponentUtils::ScreenXForOmega(hostFrequency, 1000.0f)
        == doctest::Approx(1000.0f * PathDrawer::LinearToLog(engineFrequency)));
    DOCTEST_CHECK(engineFrequency * QuadAnalyserComponent::GetTransferFunctionFrequencyScale(
        QuadAnalyserComponent::Type::PartialMachine) == doctest::Approx(hostFrequency));
    DOCTEST_CHECK(engineFrequency * QuadAnalyserComponent::GetTransferFunctionFrequencyScale(
        QuadAnalyserComponent::Type::Delay) == doctest::Approx(engineFrequency));
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
    TrioSquiggleBoyShape::Snapshot filterSnapshot;
    filterSnapshot.m_ladder.m_lp.m_alpha = 1.0f;
    filterSnapshot.m_ladder.m_lp.m_feedback = 0.0f;
    filterSnapshot.m_ladder.m_hp.m_stage.m_g = 0.0f;
    filterSnapshot.m_ladder.m_hp.m_stage.m_k = 1.0f;
    filterSnapshot.m_ladder.m_hp.m_stage.m_weights = {0, 0, 1};
    filter.m_history.Push(filterSnapshot);
    auto writer = std::make_unique<ScopeWriterGeneric<3, 3>>(1, 4096);
    int voiceOffset = 0;

    for (int bin : {4, 64, 768, 1800})
    {
        DOCTEST_CAPTURE(bin);
        float frequency = static_cast<float>(bin) / WindowedFFT::Transform::x_tableSize;
        for (size_t sample = 0; sample < WindowedFFT::Transform::x_tableSize; ++sample)
        {
            float value = 0.05f * std::sin(2.0f * static_cast<float>(M_PI) * frequency * sample);
            writer->Write(1, TrioFloat(value, 0.0f, 0.0f));
            writer->AdvanceIndex();
        }

        writer->Publish();
        writer->AcceptSamples();
        AnalyserComponent analyser(TrioWindowedFFT(writer.get(), 0), &voiceOffset, ui.get());
        analyser.m_logX = logX;
        juce::Image spectrum(juce::Image::ARGB, 1024, 400, true);
        juce::Graphics spectrumGraphics(spectrum);
        analyser.Draw(spectrumGraphics, spectrum.getBounds());

        PeakedResponse response(frequency);
        PathDrawer drawer(400, 1024, 0, 0);
        drawer.m_logX = logX;
        juce::Image transfer(juce::Image::ARGB, 1024, 400, true);
        juce::Graphics transferGraphics(transfer);
        drawer.DrawFrequencyResponse(transferGraphics, juce::Colours::white, response,
            1.0f);

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

DOCTEST_TEST_CASE("Analyser: sub spectrum follows the trio and ignores the main signal and filter")
{
    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    auto& writer = ui->m_squiggleBoyUIState.m_voiceScopeWriter;
    const size_t subScope = static_cast<size_t>(SmartGridOne::VoiceScopes::Sub);
    const size_t mainScope = static_cast<size_t>(SmartGridOne::VoiceScopes::PostAmp);
    for (size_t sample = 0; sample < WindowedFFT::Transform::x_tableSize; ++sample)
    {
        for (size_t trio = 0; trio < SquiggleBoy::x_numTracks; ++trio)
        {
            const float phase = 2.0f * static_cast<float>(M_PI) * sample / WindowedFFT::Transform::x_tableSize;
            writer.Write(SmartGridOne::VoiceScopeIndex(subScope, trio), TrioFloat(0.5f * std::sin((8 + 8 * trio) * phase), 0.0f, 0.0f));
            writer.Write(SmartGridOne::VoiceScopeIndex(mainScope, trio), TrioFloat(0.9f * std::sin(64 * phase), 0.0f, 0.0f));
        }

        AdvanceStoredSample(writer);
    }

    writer.Publish();
    writer.AcceptSamples();
    int voiceOffset = 0;
    for (size_t trio = 0; trio < SquiggleBoy::x_numTracks; ++trio)
    {
        ui->m_squiggleBoyUIState.m_activeTrack.store(trio);
        AnalyserComponent analyser(TrioWindowedFFT(&writer, subScope), &voiceOffset, ui.get(),
            AnalyserComponent::FilterOverlay::None);
        juce::Image spectrum(juce::Image::ARGB, 512, 200, true);
        juce::Graphics graphics(spectrum);
        analyser.Draw(graphics, spectrum.getBounds());
        const auto& components = analyser.m_windowedFFT.m_dft.m_components;
        size_t peak = 0;
        for (size_t bin = 1; bin < WindowedFFT::Transform::x_maxComponents; ++bin)
        {
            if (std::abs(components[bin][0]) > std::abs(components[peak][0]))
            {
                peak = bin;
            }
        }

        DOCTEST_CHECK(peak == 8 + 8 * trio);
        auto& filter = ui->m_squiggleBoyUIState.m_voiceFilterUIState[trio * 3];
        TrioSquiggleBoyShape::Snapshot filterSnapshot;
        filterSnapshot.m_ladder.m_lp.m_alpha = 0.01f;
        filterSnapshot.m_ladder.m_lp.m_feedback = 3.0f;
        filter.m_history.Push(filterSnapshot);
        AnalyserComponent changedFilter(TrioWindowedFFT(&writer, subScope), &voiceOffset, ui.get(),
            AnalyserComponent::FilterOverlay::None);
        juce::Image unchangedSpectrum(juce::Image::ARGB, 512, 200, true);
        juce::Graphics changedGraphics(unchangedSpectrum);
        changedFilter.Draw(changedGraphics, unchangedSpectrum.getBounds());
        bool equal = true;
        for (int y = 0; y < spectrum.getHeight(); ++y)
        {
            for (int x = 0; x < spectrum.getWidth(); ++x)
            {
                equal = equal && spectrum.getPixelAt(x, y) == unchangedSpectrum.getPixelAt(x, y);
            }
        }

        DOCTEST_CHECK(equal);
    }
}

DOCTEST_TEST_CASE("Quad analyser: mix draws five separate colored spectra without transfer functions")
{
    using Scope = SmartGridOne::QuadScopes;
    using Type = QuadAnalyserComponent::Type;
    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    auto& writer = ui->m_squiggleBoyUIState.m_quadScopeWriter;
    for (size_t sample = 0; sample < WindowedFFT::Transform::x_tableSize; ++sample)
    {
        const float phase = 2.0f * static_cast<float>(M_PI) * sample / WindowedFFT::Transform::x_tableSize;
        for (size_t scope = 0; scope < static_cast<size_t>(Scope::NumScopes); ++scope)
        {
            QuadFloat values;
            for (size_t speaker = 0; speaker < 4; ++speaker)
            {
                values[speaker] = 0.5f * std::sin((8 + 16 * scope + 2 * speaker) * phase);
            }

            writer.Write(scope, values);
        }

        AdvanceStoredSample(writer);
    }

    writer.Publish();
    writer.AcceptSamples();
    auto analyser = std::make_unique<QuadAnalyserComponent>(ui.get(), Type::Mix);
    analyser->m_drawAll = false;
    juce::Image spectrum(juce::Image::ARGB, 1024, 512, true);
    juce::Graphics graphics(spectrum);
    analyser->Draw(graphics, spectrum.getBounds());
    const Scope x_scopes[] = {Scope::Delay, Scope::Reverb, Scope::PartialMachine, Scope::Dry, Scope::Sub};
    const SmartGrid::Color x_colors[] =
    {
        SmartGrid::Color::Pink, SmartGrid::Color::Fuscia, SmartGrid::Color::Cyan,
        SmartGrid::Color::Yellow, SmartGrid::Color::White
    };
    for (size_t source = 0; source < 5; ++source)
    {
        DOCTEST_CAPTURE(source);
        for (size_t speaker = 0; speaker < 4; ++speaker)
        {
            const auto& components = analyser->m_quadWindowedFFT[source].m_dft.m_components;
            size_t peak = 0;
            for (size_t bin = 1; bin < WindowedFFT::Transform::x_maxComponents; ++bin)
            {
                if (std::abs(components[bin][speaker]) > std::abs(components[peak][speaker]))
                {
                    peak = bin;
                }
            }

            DOCTEST_CHECK(peak == 8 + 16 * static_cast<size_t>(x_scopes[source]) + 2 * speaker);
            for (Type type : {Type::Delay, Type::Reverb, Type::PartialMachine})
            {
                DOCTEST_CHECK(analyser->GetTransferFunctionForSpeaker(type, speaker) == nullptr);
            }
        }

        const auto color = x_colors[source];
        const int maxColor = std::max({color.m_red, color.m_green, color.m_blue});
        size_t pixels[4] = {};
        for (int y = 0; y < spectrum.getHeight(); ++y)
        {
            for (int x = 0; x < spectrum.getWidth(); ++x)
            {
                const auto pixel = spectrum.getPixelAt(x, y);
                const int maxPixel = std::max({pixel.getRed(), pixel.getGreen(), pixel.getBlue()});
                if (maxPixel > 32
                    && std::abs(pixel.getRed() * maxColor - color.m_red * maxPixel) <= 3 * maxColor
                    && std::abs(pixel.getGreen() * maxColor - color.m_green * maxPixel) <= 3 * maxColor
                    && std::abs(pixel.getBlue() * maxColor - color.m_blue * maxPixel) <= 3 * maxColor)
                {
                    ++pixels[(y >= spectrum.getHeight() / 2 ? 2 : 0) + (x >= spectrum.getWidth() / 2 ? 1 : 0)];
                }
            }
        }

        for (size_t count : pixels)
        {
            DOCTEST_CHECK(count > 5);
        }
    }
}

DOCTEST_TEST_CASE("Scope: reduced point counts preserve the time axis and end marker")
{
    auto writer = std::make_unique<ScopeWriterGeneric<1, 1>>(256, 4096);
    writer->RecordStart(0, 0);
    for (size_t sample = 0; sample < 128; ++sample)
    {
        writer->Write(0, static_cast<float>(sample) / 127.0f);
        AdvanceStoredSample(*writer);
    }

    writer->Publish();
    writer->AcceptSamples();
    for (size_t points : {128, 256, 1024})
    {
        DOCTEST_CAPTURE(points);
        ScopeReader reader(writer.get(), 0, 0, points);
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

DOCTEST_TEST_CASE("Delay waveform: the shared magnitude scope draws a symmetric decibel envelope")
{
    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    auto& state = ui->m_squiggleBoyUIState.m_delayUIState;
    TimeWarpInverter<8192> inverse(1024.0);
    state.m_scopeWriter.SetTimeWarp(0, &inverse);
    for (size_t sample = 0; sample <= 2304; ++sample)
    {
        inverse.Write(static_cast<double>(sample) / 1024.0, static_cast<double>(sample));
        if (sample % 1024 == 0)
        {
            for (size_t channel = 0; channel < 4; ++channel)
            {
                state.m_scopeWriter.SetPeriodic(0, channel, {0.0, 1.0});
            }
        }

        const float levels[] = {1.0f, 0.1f, 0.01f, 0.0f};
        state.m_scopeWriter.Write(0, QuadFloat::Splat(levels[(sample % 1024) / 256]));
        state.m_scopeWriter.AdvanceIndex();
    }

    state.m_scopeWriter.Publish();
    state.m_scopeWriter.AcceptSamples();
    QuadDelayEnvelopeVisualizerComponent visualizer(ui.get());
    juce::Image image(juce::Image::ARGB, 1024, 400, true);
    juce::Graphics graphics(image);
    visualizer.Draw(graphics, image.getBounds());
    for (int channel = 0; channel < 4; ++channel)
    {
        int top = channel * 100;
        DOCTEST_CHECK(image.getPixelAt(128, top + 5).getRed() > 100);
        DOCTEST_CHECK(image.getPixelAt(384, top + 5) == juce::Colours::black);
        DOCTEST_CHECK(image.getPixelAt(384, top + 20).getRed() > 100);
        DOCTEST_CHECK(image.getPixelAt(384, top + 50).getRed() > 100);
        DOCTEST_CHECK(image.getPixelAt(640, top + 20) == juce::Colours::black);
        DOCTEST_CHECK(image.getPixelAt(640, top + 50).getRed() > 100);
        DOCTEST_CHECK(image.getPixelAt(640, top + 85) == juce::Colours::black);
        DOCTEST_CHECK(image.getPixelAt(896, top + 95) == juce::Colours::black);
        for (int x : {128, 384, 640, 896})
        {
            for (int y = 0; y < 50; ++y)
            {
                DOCTEST_CHECK(image.getPixelAt(x, top + y) == image.getPixelAt(x, top + 99 - y));
            }
        }
    }
}

DOCTEST_TEST_CASE("Time warp scope: unwarped periodic axis, cursor, and disconnected vertical wraps")
{
    for (bool reverse : {false, true})
    {
        DOCTEST_CAPTURE(reverse);
        auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
        ui->m_squiggleBoyUIState.m_activeTrack.store(0);
        ui->m_nonagonUIState.m_loopMultiplier[0].store(1);
        ui->m_tempoHz.store(static_cast<double>(SampleTimer::x_sampleRate) / 32768.0);
        auto& writer = ui->m_squiggleBoyUIState.m_monoScopeWriter;
        size_t scope = static_cast<size_t>(SmartGridOne::MonoScopes::TheoryOfTime);
        TimeWarpInverter<8192> inverse(4096.0);
        writer.SetTimeWarp(scope, &inverse);
        writer.SetPeriodic(scope, 0, {0.0, 1.0});
        for (size_t sample = 0; sample <= 40960; ++sample)
        {
            double phase = static_cast<double>(sample) / 32768.0;
            inverse.Write(phase, static_cast<double>(sample));
            float displacement = reverse ? -0.25 - 2.0 * (phase - std::floor(phase)) : -0.5;
            writer.Write(scope, displacement);
            writer.AdvanceIndex();
        }

        writer.Publish();
        writer.AcceptSamples();
        TheoryOfTimeScopeComponent component(ui.get());
        juce::Image image(juce::Image::ARGB, 1024, 200, true);
        juce::Graphics graphics(image);
        component.Draw(graphics, image.getBounds());
        int wrapX = reverse ? 768 : 512;
        for (int y = 10; y < 190; ++y)
        {
            DOCTEST_CHECK(image.getPixelAt(wrapX, y) == juce::Colours::black);
        }

        int markerY = reverse ? 100 : 50;
        DOCTEST_CHECK(image.getPixelAt(255, markerY - 2).getRed() > 200);
        DOCTEST_CHECK(image.getPixelAt(640, 175).getRed() > 50);
        ui->m_tempoHz.store(20.0);
        component.Draw(graphics, image.getBounds());
        DOCTEST_CHECK(image.getPixelAt(255, markerY - 2) == juce::Colours::black);
    }
}

DOCTEST_TEST_CASE("Scope gaps: missing history draws neither zero nor the time-warp diagonal")
{
    struct CountingInverse : TimeWarpInverter<8192>
    {
        mutable size_t m_reads = 0;

        CountingInverse() : TimeWarpInverter(1024.0)
        {
        }

        double ReadInverse(double phase, double cutoff) const override
        {
            ++m_reads;
            return TimeWarpInverter::ReadInverse(phase, cutoff);
        }
    } inverse;
    auto ui = std::make_unique<TheNonagonSquiggleBoyInternal::UIState>();
    auto& mono = ui->m_squiggleBoyUIState.m_monoScopeWriter;
    auto& delay = ui->m_squiggleBoyUIState.m_delayUIState.m_scopeWriter;
    size_t scope = static_cast<size_t>(SmartGridOne::MonoScopes::TheoryOfTime);
    ui->m_nonagonUIState.m_loopMultiplier[0].store(1);
    mono.SetTimeWarp(scope, &inverse);
    mono.SetPeriodic(scope, 0, {0.0, 1.0});
    delay.SetTimeWarp(0, &inverse);
    for (size_t lane = 0; lane < 4; ++lane)
    {
        delay.SetPeriodic(0, lane, {0.0, 1.0});
    }

    for (size_t sample = 0; sample <= 1280; ++sample)
    {
        inverse.Write(static_cast<double>(sample) / 1024.0, static_cast<double>(sample));
        mono.Write(scope, -0.25f);
        delay.Write(0, QuadFloat::Splat(0.1f));
        mono.AdvanceIndex();
        delay.AdvanceIndex();
    }

    mono.Publish();
    mono.AcceptSamples();
    delay.Publish();
    delay.AcceptSamples();
    ScopeReaderFactory factory(&mono, 0, scope, 256);
    TheoryOfTimeScopeComponent::DrawFn fn(&factory);
    inverse.m_reads = 0;
    DOCTEST_CHECK(fn.PhaseAt(192.0) == 0.5);
    DOCTEST_CHECK(inverse.m_reads == 1);
    for (size_t cell = 450; cell <= 575; ++cell)
    {
        inverse.ClearCell(cell);
    }

    DOCTEST_CHECK_FALSE(std::isfinite(fn.PhaseAt(128.0)));
    juce::Image warpImage(juce::Image::ARGB, 512, 200, true);
    juce::Graphics warpGraphics(warpImage);
    TheoryOfTimeScopeComponent warp(ui.get());
    warp.Draw(warpGraphics, warpImage.getBounds());
    for (int y = 5; y < 195; ++y)
    {
        DOCTEST_CHECK(warpImage.getPixelAt(256, y) == juce::Colours::black);
    }

    juce::Image delayImage(juce::Image::ARGB, 512, 200, true);
    juce::Graphics delayGraphics(delayImage);
    QuadDelayEnvelopeVisualizerComponent waveform(ui.get());
    waveform.Draw(delayGraphics, delayImage.getBounds());
    for (int y = 0; y < 200; ++y)
    {
        DOCTEST_CHECK(delayImage.getPixelAt(256, y) == juce::Colours::black);
    }

    ScopeReader reader(&mono, 0, scope, 256);
    juce::Image scopeImage(juce::Image::ARGB, 512, 200, true);
    juce::Graphics scopeGraphics(scopeImage);
    PathDrawer drawer(200, 512, 0, 0);
    drawer.DrawScopePath(scopeGraphics, juce::Colours::white, reader, 1.0f, 1.0f);
    DOCTEST_CHECK(scopeImage.getPixelAt(200, 50).getAlpha() > 0);
    for (int y = 0; y < 200; ++y)
    {
        DOCTEST_CHECK(scopeImage.getPixelAt(256, y).getAlpha() == 0);
    }
}

DOCTEST_TEST_CASE("Sampled response rendering: percentile motion creates a feathered band around the median")
{
    SampledTransferFunction::Config config;
    config.m_capacity = 64;
    config.m_numSamples = 32;
    TransferFunctionGeneric<FlatSampledResponse> stationary(config);
    TransferFunctionGeneric<FlatSampledResponse> moving(config);
    for (size_t i = 0; i < 32; ++i)
    {
        FlatSampledResponse snapshot;
        snapshot.m_gain = 0.4f;
        stationary.m_history.Push(snapshot);
        snapshot.m_gain = i < 8 ? 0.1f : i < 24 ? 0.4f : 1.0f;
        moving.m_history.Push(snapshot);
    }

    juce::Image still(juce::Image::ARGB, 320, 200, true);
    juce::Image blur(juce::Image::ARGB, 320, 200, true);
    juce::Graphics stillGraphics(still);
    juce::Graphics blurGraphics(blur);
    PathDrawer drawer(200, 320, 0, 0);
    drawer.DrawSampledFrequencyResponse(stillGraphics, juce::Colours::cyan, stationary);
    drawer.DrawSampledFrequencyResponse(blurGraphics, juce::Colours::cyan, moving);
    int stillRows = 0;
    int blurRows = 0;
    for (int y = 0; y < 200; ++y)
    {
        stillRows += still.getPixelAt(160, y).getAlpha() > 0 ? 1 : 0;
        blurRows += blur.getPixelAt(160, y).getAlpha() > 0 ? 1 : 0;
    }

    DOCTEST_CHECK(stillRows <= 3);
    DOCTEST_CHECK(blurRows >= 30);
    DOCTEST_CHECK(FindPeakX(still, [](juce::Colour colour) { return colour.getAlpha() > 128; })
        == doctest::Approx(FindPeakX(blur, [](juce::Colour colour) { return colour.getAlpha() > 128; })).epsilon(0.01));

    if (const char* outputPath = std::getenv("SMARTGRID_RESPONSE_PREVIEW"))
    {
        juce::Image preview(juce::Image::ARGB, 640, 400, true);
        juce::Graphics graphics(preview);
        graphics.fillAll(juce::Colours::black);
        graphics.drawImageAt(still, 0, 0);
        graphics.drawImageAt(blur, 320, 0);
        OPLowPassFilter::UIState cutoff(config);
        OPLowPassFilter lowPass;
        for (size_t i = 0; i < 32; ++i)
        {
            float hz = 2000.0f * std::exp2(2 * std::sin(2 * juce::MathConstants<float>::pi * i / 32));
            lowPass.SetAlphaFromNatFreq(hz / SampleTimer::x_sampleRate);
            cutoff.m_history.Push(lowPass.MakeSnapshot(0));
        }

        PathDrawer responseDrawer(200, 640, 0, 200);
        responseDrawer.DrawSampledFrequencyResponse(graphics, juce::Colours::orange, cutoff);
        juce::File file(outputPath);
        auto stream = file.createOutputStream();
        DOCTEST_REQUIRE(stream != nullptr);
        juce::PNGImageFormat png;
        DOCTEST_CHECK(png.writeImageToStream(preview, *stream));
    }
}
