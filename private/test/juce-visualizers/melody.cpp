#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "SequencerMelodyVisualizerComponent.hpp"

#include <memory>
#include <limits>

namespace
{
using Visualizer = SequencerMelodyVisualizerComponent;
using Strategy = HarmonicSheaf::SectionChoiceStrategy;

struct MelodyRig
{
    std::unique_ptr<TheNonagonSquiggleBoyInternal::UIState> m_ui;
    int m_voiceOffset = -1;
    std::atomic<double> m_phase{9.0 / 32.0};
    Visualizer m_visualizer;

    MelodyRig()
        : m_ui(std::make_unique<TheNonagonSquiggleBoyInternal::UIState>())
        , m_visualizer(m_ui.get(), &m_voiceOffset, &m_phase)
    {
        m_ui->m_squiggleBoyUIState.m_activeTrack.store(0);
        auto& state = m_ui->m_nonagonUIState;
        auto& time = state.m_theoryOfTimeUIState;
        for (size_t loop = 0; loop < 6; ++loop)
        {
            time.m_periodTicks[loop].store(int64_t{1} << loop);
            time.m_rhythm[loop].m_size.store(2);
            time.m_rhythm[loop].m_resetLoopIndex.store(-1);
            for (size_t slot = 0; slot < 16; ++slot)
            {
                time.m_rhythm[loop].m_gate[slot].store(slot % 2 == 0);
            }
        }

        auto& harmonic = state.m_lameJuisUIState.m_harmonicSheafState;
        for (size_t slice = 0; slice < 64; ++slice)
        {
            HarmonicSheaf::Section section;
            for (size_t rank = 0; rank < 3; ++rank)
            {
                section.m_total[rank] = 1;
                section.m_high[rank] = (slice >> rank) & 1;
            }

            harmonic.m_sectionState.m_sections[slice].store(section);
        }

        harmonic.m_evaluatorState.m_coefficients[0].store(0.25f);
        harmonic.m_evaluatorState.m_coefficients[1].store(0.5f);
        harmonic.m_evaluatorState.m_coefficients[2].store(2.0f);
        for (size_t trio = 0; trio < 3; ++trio)
        {
            state.m_indexArpUIState.m_clockSelect[trio].store(0);
            state.m_indexArpUIState.m_resetSelect[trio].store(5);
        }

        for (size_t voice = 0; voice < 9; ++voice)
        {
            state.m_muted[voice].store(false);
            auto& arp = state.m_indexArpUIState.m_arpUIState[voice];
            arp.m_min.store(static_cast<float>(voice));
            arp.m_max.store(static_cast<float>(voice) + 0.75f);
            arp.m_rhythmLength.store(1);
            arp.m_rhythm[0].store(true);
            auto& chooser = harmonic.m_voiceChooserState[voice];
            chooser.m_lens.store(HarmonicSheaf::Lens(0x3c));
            chooser.m_baseStrategy.store(Strategy::None);
            chooser.m_strategy.store(Strategy::ClosestModOne);
        }
    }
};

void ConfigurePeriodRig(MelodyRig& rig, uint8_t lensBits)
{
    auto& state = rig.m_ui->m_nonagonUIState;
    for (size_t loop = 0; loop < 6; ++loop)
    {
        auto& rhythm = state.m_theoryOfTimeUIState.m_rhythm[loop];
        rhythm.m_size.store(1);
        rhythm.m_gate[0].store(true);
    }

    for (size_t voice = 0; voice < 9; ++voice)
    {
        state.m_lameJuisUIState.m_harmonicSheafState.m_voiceChooserState[voice].m_lens.store(
            HarmonicSheaf::Lens(lensBits));
    }

    state.m_indexArpUIState.m_clockSelect[0].store(-1);
    state.m_indexArpUIState.m_resetSelect[0].store(-1);
}

void SetGateRhythm(MelodyRig& rig, size_t loop, std::initializer_list<bool> gates)
{
    auto& rhythm = rig.m_ui->m_nonagonUIState.m_theoryOfTimeUIState.m_rhythm[loop];
    rhythm.m_size.store(static_cast<int64_t>(gates.size()));
    size_t slot = 0;
    for (bool gate : gates)
    {
        rhythm.m_gate[slot++].store(gate);
    }
}

void CheckPeriodWindow(MelodyRig& rig, int64_t tick, int64_t start, int64_t end)
{
    auto& state = rig.m_ui->m_nonagonUIState;
    rig.m_phase.store(static_cast<double>(tick)
        / static_cast<double>(state.m_theoryOfTimeUIState.m_periodTicks[5].load()));
    Visualizer::Frame frame;
    DOCTEST_REQUIRE(rig.m_visualizer.PrepareFrame(frame));
    DOCTEST_CHECK(frame.m_position == tick);
    DOCTEST_CHECK(frame.m_start == start);
    DOCTEST_CHECK(frame.m_end == end);
    for (size_t displayed = 0; displayed < frame.m_numVoices; ++displayed)
    {
        size_t voice = frame.m_voices[displayed];
        const auto& sequence = state.m_sequences[voice];
        DOCTEST_REQUIRE_FALSE(sequence.m_points.empty());
        DOCTEST_CHECK(sequence.StartPosition() <= start);
        DOCTEST_CHECK(sequence.EndPosition() >= end);
        if (sequence.StartPosition() <= start && sequence.EndPosition() >= end)
        {
            for (int64_t position = start; position < end; ++position)
            {
                auto actual = sequence.GetPoint(position);
                auto expected = state.GetVoicePoint(position, voice);
                DOCTEST_CHECK(actual.m_globalTickPosition == position);
                DOCTEST_CHECK(actual.m_timeSlice.m_bits == expected.m_timeSlice.m_bits);
                DOCTEST_CHECK(actual.m_choiceValue == doctest::Approx(expected.m_choiceValue));
                DOCTEST_CHECK(actual.m_pitch.m_value == doctest::Approx(expected.m_pitch.m_value));
            }
        }
    }
}

bool HasLineAt(const juce::Image& image, float pitch, int x)
{
    int y = static_cast<int>(std::round(400.0f - pitch * 100.0f));
    for (int column = x; column < x + 12; ++column)
    {
        for (int row = y - 1; row <= y + 1; ++row)
        {
            if (image.getPixelAt(column, row).getAlpha() != 0)
            {
                return true;
            }
        }
    }

    return false;
}

bool HasPixels(const juce::Image& image, juce::Rectangle<int> area)
{
    for (int x = area.getX(); x < area.getRight(); ++x)
    {
        for (int y = area.getY(); y < area.getBottom(); ++y)
        {
            if (image.getPixelAt(x, y).getAlpha() != 0)
            {
                return true;
            }
        }
    }

    return false;
}
}

DOCTEST_TEST_CASE("Melody: pitch range follows only displayed voice bounds without sheaf padding")
{
    MelodyRig rig;
    Visualizer::Frame frame;
    DOCTEST_REQUIRE(rig.m_visualizer.PrepareFrame(frame));
    DOCTEST_CHECK(frame.m_minPitch == doctest::Approx(0.0f));
    DOCTEST_CHECK(frame.m_maxPitch == doctest::Approx(2.75f));

    rig.m_ui->m_nonagonUIState.m_muted[2].store(true);
    DOCTEST_REQUIRE(rig.m_visualizer.PrepareFrame(frame));
    DOCTEST_CHECK(frame.m_maxPitch == doctest::Approx(1.75f));

    rig.m_voiceOffset = 2;
    DOCTEST_REQUIRE(rig.m_visualizer.PrepareFrame(frame));
    DOCTEST_CHECK(frame.m_minPitch == doctest::Approx(2.0f));
    DOCTEST_CHECK(frame.m_maxPitch == doctest::Approx(2.75f));
}

DOCTEST_TEST_CASE("Melody: modulo-one candidates repeat into every visible octave")
{
    MelodyRig rig;
    Visualizer::Frame frame;
    DOCTEST_REQUIRE(rig.m_visualizer.PrepareFrame(frame));
    frame.m_start = 0;
    frame.m_end = 1;
    frame.m_minPitch = 0.0f;
    frame.m_maxPitch = 4.0f;
    juce::Image image(juce::Image::ARGB, 320, 400, true);
    juce::Graphics graphics(image);
    rig.m_visualizer.DrawSheaf(graphics, frame, image.getBounds().toFloat());
    DOCTEST_CHECK(HasLineAt(image, 0.25f, 20));
    DOCTEST_CHECK(HasLineAt(image, 1.25f, 20));
    DOCTEST_CHECK(HasLineAt(image, 2.25f, 20));
    DOCTEST_CHECK(HasLineAt(image, 3.25f, 20));
    DOCTEST_CHECK_FALSE(HasLineAt(image, 1.125f, 20));
}

DOCTEST_TEST_CASE("Melody: percentile candidates retain their register and add only requested upper octaves")
{
    MelodyRig rig;
    rig.m_voiceOffset = 0;
    auto& state = rig.m_ui->m_nonagonUIState;
    auto& arp = state.m_indexArpUIState.m_arpUIState[0];
    arp.m_max.store(1.5f);
    state.m_lameJuisUIState.m_harmonicSheafState.m_voiceChooserState[0].m_strategy.store(Strategy::Percentile);
    Visualizer::Frame frame;
    DOCTEST_REQUIRE(rig.m_visualizer.PrepareFrame(frame));
    frame.m_start = 0;
    frame.m_end = 1;
    frame.m_minPitch = 0.0f;
    frame.m_maxPitch = 4.0f;
    juce::Image image(juce::Image::ARGB, 320, 400, true);
    juce::Graphics graphics(image);
    rig.m_visualizer.DrawSheaf(graphics, frame, image.getBounds().toFloat());
    DOCTEST_CHECK_FALSE(HasLineAt(image, 0.25f, 20));
    DOCTEST_CHECK_FALSE(HasLineAt(image, 1.25f, 20));
    DOCTEST_CHECK(HasLineAt(image, 2.25f, 20));
    DOCTEST_CHECK(HasLineAt(image, 3.25f, 20));

    arp.m_max.store(0.75f);
    DOCTEST_REQUIRE(rig.m_visualizer.PrepareFrame(frame));
    frame.m_start = 0;
    frame.m_end = 1;
    frame.m_minPitch = 0.0f;
    frame.m_maxPitch = 4.0f;
    image.clear(image.getBounds());
    rig.m_visualizer.DrawSheaf(graphics, frame, image.getBounds().toFloat());
    DOCTEST_CHECK(HasLineAt(image, 2.25f, 20));
    DOCTEST_CHECK_FALSE(HasLineAt(image, 3.25f, 20));
}

DOCTEST_TEST_CASE("Melody: percentile choice interpolates on each time slice's pitch axis")
{
    MelodyRig rig;
    rig.m_voiceOffset = 0;
    auto& state = rig.m_ui->m_nonagonUIState;
    auto& arp = state.m_indexArpUIState.m_arpUIState[0];
    arp.m_max.store(1.0f);
    arp.m_offset.store(0.125f);
    state.m_lameJuisUIState.m_harmonicSheafState.m_voiceChooserState[0].m_strategy.store(Strategy::Percentile);
    Visualizer::Frame frame;
    DOCTEST_REQUIRE(rig.m_visualizer.PrepareFrame(frame));
    frame.m_start = 0;
    frame.m_end = 8;
    frame.m_minPitch = 0.0f;
    frame.m_maxPitch = 4.0f;
    juce::Image image(juce::Image::ARGB, 320, 400, true);
    juce::Graphics graphics(image);
    rig.m_visualizer.DrawVoice(graphics, frame, image.getBounds().toFloat(), 0);
    DOCTEST_CHECK(HasLineAt(image, 2.125f, 10));
    DOCTEST_CHECK(HasLineAt(image, 0.125f, 170));
    DOCTEST_CHECK_FALSE(HasLineAt(image, 3.0f, 10));
    DOCTEST_CHECK_FALSE(HasLineAt(image, 1.0f, 170));
    DOCTEST_CHECK_FALSE(HasLineAt(image, 1.125f, 10));
}

DOCTEST_TEST_CASE("Melody: percentile bounds convert to pitch across the displayed time slices")
{
    MelodyRig rig;
    rig.m_voiceOffset = 0;
    auto& state = rig.m_ui->m_nonagonUIState;
    auto& arp = state.m_indexArpUIState.m_arpUIState[0];
    arp.m_min.store(0.125f);
    arp.m_max.store(1.125f);
    state.m_lameJuisUIState.m_harmonicSheafState.m_voiceChooserState[0].m_strategy.store(Strategy::Percentile);
    Visualizer::Frame frame;
    DOCTEST_REQUIRE(rig.m_visualizer.PrepareFrame(frame));
    DOCTEST_CHECK(frame.m_minPitch == doctest::Approx(0.125f));
    DOCTEST_CHECK(frame.m_maxPitch == doctest::Approx(3.125f));
}

DOCTEST_TEST_CASE("Melody: a collapsed arp range places its line at the vertical center")
{
    MelodyRig rig;
    rig.m_voiceOffset = 0;
    auto& arp = rig.m_ui->m_nonagonUIState.m_indexArpUIState.m_arpUIState[0];
    arp.m_min.store(0.25f);
    arp.m_max.store(0.25f);
    Visualizer::Frame frame;
    DOCTEST_REQUIRE(rig.m_visualizer.PrepareFrame(frame));
    DOCTEST_CHECK(frame.m_minPitch == doctest::Approx(0.25f));
    DOCTEST_CHECK(frame.m_maxPitch == doctest::Approx(0.25f));
    DOCTEST_CHECK(Visualizer::PitchY(0.25f, frame, juce::Rectangle<float>(0, 0, 320, 400))
        == doctest::Approx(200.0f));
    DOCTEST_CHECK(Visualizer::PitchY(0.24f, frame, juce::Rectangle<float>(0, 0, 320, 400)) > 400.0f);
    DOCTEST_CHECK(Visualizer::PitchY(0.26f, frame, juce::Rectangle<float>(0, 0, 320, 400)) < 0.0f);
}

DOCTEST_TEST_CASE("Melody: percentile interpolation preserves duplicate ranks and signed octave shifts")
{
    MelodyRig rig;
    auto& harmonic = rig.m_ui->m_nonagonUIState.m_lameJuisUIState.m_harmonicSheafState;
    harmonic.m_evaluatorState.m_coefficients[0].store(0.0f);
    Visualizer::Frame frame;
    DOCTEST_REQUIRE(rig.m_visualizer.PrepareFrame(frame));
    const auto& pitches = frame.m_pitchSlices[60];
    DOCTEST_CHECK(pitches.InterpolatePercentile(0.125f) == doctest::Approx(2.0f));
    DOCTEST_CHECK(pitches.InterpolatePercentile(0.375f) == doctest::Approx(2.25f));
    DOCTEST_CHECK(pitches.InterpolatePercentile(0.875f) == doctest::Approx(2.5f));
    DOCTEST_CHECK(pitches.InterpolatePercentile(1.375f) == doctest::Approx(3.25f));
    DOCTEST_CHECK(pitches.InterpolatePercentile(-0.625f) == doctest::Approx(1.25f));
}

DOCTEST_TEST_CASE("Melody: percentile range includes extrema between bounds at octave jumps")
{
    MelodyRig rig;
    rig.m_voiceOffset = 0;
    auto& state = rig.m_ui->m_nonagonUIState;
    auto& harmonic = state.m_lameJuisUIState.m_harmonicSheafState;
    harmonic.m_evaluatorState.m_coefficients[1].store(4.0f);
    harmonic.m_voiceChooserState[0].m_strategy.store(Strategy::Percentile);
    auto& arp = state.m_indexArpUIState.m_arpUIState[0];
    arp.m_min.store(0.8f);
    arp.m_max.store(1.1f);
    Visualizer::Frame frame;
    DOCTEST_REQUIRE(rig.m_visualizer.PrepareFrame(frame));
    DOCTEST_CHECK(frame.m_minPitch == doctest::Approx(1.0f));
    DOCTEST_CHECK(frame.m_maxPitch == doctest::Approx(6.25f));
}

DOCTEST_TEST_CASE("Melody: choice eases toward the next step only during the preceding half step")
{
    MelodyRig rig;
    rig.m_voiceOffset = 0;
    auto& arp = rig.m_ui->m_nonagonUIState.m_indexArpUIState.m_arpUIState[0];
    arp.m_min.store(0.0f);
    arp.m_max.store(4.0f);
    arp.m_offset.store(0.125f);
    arp.m_interval.store(0.25f);
    arp.m_rhythmLength.store(4);
    Visualizer::Frame frame;
    DOCTEST_REQUIRE(rig.m_visualizer.PrepareFrame(frame));
    frame.m_start = 0;
    frame.m_end = 2;
    juce::Image image(juce::Image::ARGB, 320, 400, true);
    juce::Graphics graphics(image);
    rig.m_visualizer.DrawVoice(graphics, frame, image.getBounds().toFloat(), 0);
    DOCTEST_CHECK(HasPixels(image, juce::Rectangle<int>(110, 270, 30, 50)));
    DOCTEST_CHECK_FALSE(HasPixels(image, juce::Rectangle<int>(20, 270, 50, 50)));
    DOCTEST_CHECK_FALSE(HasPixels(image, juce::Rectangle<int>(159, 280, 3, 40)));
}

DOCTEST_TEST_CASE("Melody: choice paths do not connect across a motive boundary")
{
    MelodyRig rig;
    rig.m_voiceOffset = 0;
    auto& arp = rig.m_ui->m_nonagonUIState.m_indexArpUIState.m_arpUIState[0];
    arp.m_min.store(0.0f);
    arp.m_max.store(4.0f);
    arp.m_offset.store(0.125f);
    arp.m_interval.store(0.25f);
    arp.m_rhythmLength.store(4);
    Visualizer::Frame frame;
    DOCTEST_REQUIRE(rig.m_visualizer.PrepareFrame(frame));
    frame.m_start = 0;
    frame.m_end = 8;
    juce::Image image(juce::Image::ARGB, 320, 400, true);
    juce::Graphics graphics(image);
    rig.m_visualizer.DrawVoice(graphics, frame, image.getBounds().toFloat(), 0);
    DOCTEST_CHECK_FALSE(HasPixels(image, juce::Rectangle<int>(158, 190, 4, 20)));
    DOCTEST_CHECK(HasLineAt(image, 3.5f, 130));
    DOCTEST_CHECK(HasLineAt(image, 0.5f, 170));
}

DOCTEST_TEST_CASE("Melody: slice separators follow read gates and ignore co-muted gate changes")
{
    MelodyRig rig;
    auto& time = rig.m_ui->m_nonagonUIState.m_theoryOfTimeUIState;
    for (size_t loop = 0; loop < 6; ++loop)
    {
        for (size_t slot = 0; slot < 16; ++slot)
        {
            time.m_rhythm[loop].m_gate[slot].store(true);
        }
    }

    time.m_rhythm[0].m_gate[1].store(false);
    time.m_rhythm[2].m_gate[1].store(false);
    Visualizer::Frame frame;
    DOCTEST_REQUIRE(rig.m_visualizer.PrepareFrame(frame));
    frame.m_start = 0;
    frame.m_end = 8;
    frame.m_minPitch = 0.0f;
    frame.m_maxPitch = 4.0f;
    juce::Image image(juce::Image::ARGB, 320, 400, true);
    juce::Graphics graphics(image);
    rig.m_visualizer.DrawSheaf(graphics, frame, image.getBounds().toFloat());
    DOCTEST_CHECK(HasPixels(image, juce::Rectangle<int>(159, 235, 2, 5)));
    DOCTEST_CHECK_FALSE(HasPixels(image, juce::Rectangle<int>(79, 235, 2, 5)));
    DOCTEST_CHECK_FALSE(HasPixels(image, juce::Rectangle<int>(239, 235, 2, 5)));

    image.clear(image.getBounds());
    frame.m_end = 32;
    rig.m_visualizer.DrawSheaf(graphics, frame, juce::Rectangle<float>(0, 0, 16, 400));
    DOCTEST_CHECK_FALSE(HasPixels(image, juce::Rectangle<int>(0, 235, 16, 5)));
}

DOCTEST_TEST_CASE("Melody: diminished grid ranks octaves above tritones and off minor thirds")
{
    MelodyRig rig;
    rig.m_voiceOffset = 0;
    auto& state = rig.m_ui->m_nonagonUIState;
    auto& harmonic = state.m_lameJuisUIState.m_harmonicSheafState;
    for (auto& coefficient : harmonic.m_evaluatorState.m_coefficients)
    {
        coefficient.store(0.0f);
    }

    harmonic.m_voiceChooserState[0].m_strategy.store(Strategy::None);
    auto& arp = state.m_indexArpUIState.m_arpUIState[0];
    for (float offset : {0.0f, -3.0f})
    {
        DOCTEST_CAPTURE(offset);
        arp.m_min.store(0.25f + offset);
        arp.m_max.store(2.25f + offset);
        juce::Image image(juce::Image::ARGB, 340, 500, true);
        juce::Graphics graphics(image);
        rig.m_visualizer.Draw(graphics, image.getBounds());

        // Sum adjacent pixels to compare line strength across subpixel positions.
        // The plot spans y=8..404, with 198 pixels per octave.
        //
        auto LineStrength = [&](int row)
        {
            float strength = 0.0f;
            for (int y = row - 1; y <= row + 1; ++y)
            {
                strength += image.getPixelAt(200, y).getBrightness();
            }

            return strength;
        };

        DOCTEST_CHECK(LineStrength(57) > LineStrength(156));
        DOCTEST_CHECK(LineStrength(255) > LineStrength(354));
        DOCTEST_CHECK(LineStrength(156) > LineStrength(107));
        DOCTEST_CHECK(LineStrength(354) > LineStrength(305));
        DOCTEST_CHECK(LineStrength(107) > LineStrength(111));
        DOCTEST_CHECK(LineStrength(206) > LineStrength(210));
        DOCTEST_CHECK(LineStrength(305) > LineStrength(309));
    }
}

DOCTEST_TEST_CASE("Melody: motive identity follows held notes across rests and signed reset boundaries")
{
    MelodyRig rig;
    auto& state = rig.m_ui->m_nonagonUIState;
    auto& arp = state.m_indexArpUIState.m_arpUIState[0];
    arp.m_rhythmLength.store(4);
    for (size_t slot = 0; slot < 4; ++slot)
    {
        arp.m_rhythm[slot].store(slot % 2 == 1);
    }

    state.Snapshot();
    auto MotiveAt = [&](int64_t tick)
    {
        return rig.m_visualizer.GetMotive(state.GetVoicePoint(tick, 0), 0);
    };

    DOCTEST_CHECK(MotiveAt(3) == MotiveAt(4));
    DOCTEST_CHECK(MotiveAt(4) != MotiveAt(5));
    DOCTEST_CHECK(MotiveAt(31) == MotiveAt(32));
    DOCTEST_CHECK(MotiveAt(32) != MotiveAt(33));
    DOCTEST_CHECK(MotiveAt(-1) == MotiveAt(0));
    DOCTEST_CHECK(MotiveAt(0) != MotiveAt(1));

    state.m_indexArpUIState.m_resetSelect[0].store(1);
    arp.m_rhythm[0].store(true);
    state.Snapshot();
    DOCTEST_CHECK(MotiveAt(0) == MotiveAt(1));
    DOCTEST_CHECK(MotiveAt(1) != MotiveAt(2));
}

DOCTEST_TEST_CASE("Melody: playhead follows published continuous phase within ticks and across loop boundaries")
{
    MelodyRig rig;
    rig.m_voiceOffset = 0;
    auto& time = rig.m_ui->m_nonagonUIState.m_theoryOfTimeUIState;
    Visualizer visualizer(rig.m_ui.get(), &rig.m_voiceOffset, &time.m_globalPhase);
    TheoryOfTimeBase clock;
    TheoryOfTimeBase::Input input;
    input.m_running = true;
    input.m_phaseOffset = 0.125;
    for (auto& rhythm : input.m_rhythm)
    {
        rhythm.m_resetLoopIndex = 5;
    }

    struct Example
    {
        double m_phase;
        int m_pixelX;
    };

    const Example examples[] =
    {
        {9.5 / 32.0, 104},
        {9.75 / 32.0, 106},
        {-0.25 / 32.0, 329},
        {31.75 / 32.0, 329},
        {32.25 / 32.0, 10},
        {(4294967296.0 + 9.5) / 32.0, 104}
    };
    for (const auto& example : examples)
    {
        DOCTEST_CAPTURE(example.m_phase);
        input.m_unmodulatedPhase = example.m_phase - input.m_phaseOffset;
        for (size_t sample = 1; sample <= TheoryOfTimeBase::x_microBlockSize; ++sample)
        {
            clock.Process(sample, input);
        }

        clock.RolloverMicroblockBuffer();
        clock.PopulateUIState(time, input);
        juce::Image image(juce::Image::ARGB, 340, 500, true);
        juce::Graphics graphics(image);
        visualizer.Draw(graphics, image.getBounds());

        DOCTEST_REQUIRE(time.GetGlobalPeriodTicks() == 32);
        auto pixel = image.getPixelAt(example.m_pixelX, 100);
        DOCTEST_CHECK(pixel.getFloatRed() > 0.2f);
        DOCTEST_CHECK(pixel.getRed() == pixel.getGreen());
        DOCTEST_CHECK(pixel.getGreen() == pixel.getBlue());
    }
}

DOCTEST_TEST_CASE("Melody: observable constant gates have period one and repeated gate blocks use their primitive period")
{
    MelodyRig rig;
    ConfigurePeriodRig(rig, 0x3f);
    CheckPeriodWindow(rig, 9, 9, 10);
    SetGateRhythm(rig, 5, {false, false, false});
    CheckPeriodWindow(rig, 9, 9, 10);
    SetGateRhythm(rig, 2, {true, false, true, false});
    CheckPeriodWindow(rig, 9, 8, 16);
    CheckPeriodWindow(rig, -1, -8, 0);
    CheckPeriodWindow(rig, -8, -8, 0);
    auto& sequence = rig.m_ui->m_nonagonUIState.m_sequences[0];
    DOCTEST_CHECK((sequence.GetPoint(-8).m_timeSlice.m_bits & 4) != 0);
    DOCTEST_CHECK((sequence.GetPoint(-4).m_timeSlice.m_bits & 4) == 0);
}

DOCTEST_TEST_CASE("Melody: gate resets repeat the whole truncated cyclic prefix")
{
    MelodyRig rig;
    ConfigurePeriodRig(rig, 1);
    SetGateRhythm(rig, 0, {true, false, false});
    rig.m_ui->m_nonagonUIState.m_theoryOfTimeUIState.m_rhythm[0].m_resetLoopIndex.store(2);
    CheckPeriodWindow(rig, 5, 4, 8);
    auto& sequence = rig.m_ui->m_nonagonUIState.m_sequences[0];
    DOCTEST_CHECK((sequence.GetPoint(4).m_timeSlice.m_bits & 1) != 0);
    DOCTEST_CHECK((sequence.GetPoint(5).m_timeSlice.m_bits & 1) == 0);
    DOCTEST_CHECK((sequence.GetPoint(7).m_timeSlice.m_bits & 1) != 0);
    SetGateRhythm(rig, 0, {true, false, true, false, false});
    CheckPeriodWindow(rig, 5, 4, 6);
}

DOCTEST_TEST_CASE("Melody: invalid gate reset is ignored and unread loops do not affect the window")
{
    MelodyRig rig;
    ConfigurePeriodRig(rig, 4);
    auto& time = rig.m_ui->m_nonagonUIState.m_theoryOfTimeUIState;
    time.m_periodTicks[2].store(3);
    time.m_periodTicks[3].store(8);
    SetGateRhythm(rig, 2, {true, false});
    time.m_rhythm[2].m_resetLoopIndex.store(3);
    SetGateRhythm(rig, 5, {true, false, false, false, false});
    CheckPeriodWindow(rig, 7, 6, 12);
}

DOCTEST_TEST_CASE("Melody: arp periods combine all displayed voices and refresh after mute or selection")
{
    MelodyRig rig;
    ConfigurePeriodRig(rig, 0);
    auto& state = rig.m_ui->m_nonagonUIState;
    state.m_indexArpUIState.m_clockSelect[0].store(0);
    state.m_indexArpUIState.m_arpUIState[0].m_rhythmLength.store(3);
    state.m_indexArpUIState.m_arpUIState[1].m_rhythmLength.store(4);
    state.m_indexArpUIState.m_arpUIState[2].m_rhythmLength.store(5);
    CheckPeriodWindow(rig, 61, 60, 120);
    state.m_muted[2].store(true);
    CheckPeriodWindow(rig, 61, 60, 72);
    rig.m_voiceOffset = 2;
    CheckPeriodWindow(rig, 61, 60, 65);
    state.m_indexArpUIState.m_clockSelect[0].store(-1);
    CheckPeriodWindow(rig, 61, 61, 62);
}

DOCTEST_TEST_CASE("Melody: arp reset preserves natural repeat only for complete motives")
{
    MelodyRig rig;
    ConfigurePeriodRig(rig, 0);
    rig.m_voiceOffset = 0;
    auto& state = rig.m_ui->m_nonagonUIState;
    state.m_indexArpUIState.m_clockSelect[0].store(0);
    state.m_indexArpUIState.m_resetSelect[0].store(3);
    auto& arp = state.m_indexArpUIState.m_arpUIState[0];
    arp.m_rhythmLength.store(3);
    arp.m_interval.store(0.125f);
    CheckPeriodWindow(rig, 9, 8, 16);
    state.m_theoryOfTimeUIState.m_periodTicks[3].store(12);
    CheckPeriodWindow(rig, 9, 9, 12);
    DOCTEST_CHECK(state.m_sequences[0].GetPoint(10).m_choiceValue == doctest::Approx(0.09375f));
}

DOCTEST_TEST_CASE("Melody: every nonzero page interval uses reset or omits arp without an effective reset")
{
    for (float pageInterval : {0.5f, 0.499f, std::numeric_limits<float>::denorm_min()})
    {
        DOCTEST_CAPTURE(pageInterval);
        MelodyRig rig;
        ConfigurePeriodRig(rig, 1);
        SetGateRhythm(rig, 0, {true, false});
        rig.m_voiceOffset = 0;
        auto& state = rig.m_ui->m_nonagonUIState;
        state.m_indexArpUIState.m_clockSelect[0].store(0);
        state.m_indexArpUIState.m_arpUIState[0].m_rhythmLength.store(3);
        state.m_indexArpUIState.m_arpUIState[0].m_pageInterval.store(pageInterval);
        CheckPeriodWindow(rig, 9, 8, 10);
        state.m_indexArpUIState.m_resetSelect[0].store(3);
        CheckPeriodWindow(rig, 9, 8, 16);
        state.m_theoryOfTimeUIState.m_periodTicks[0].store(3);
        CheckPeriodWindow(rig, 9, 6, 12);
    }
}

DOCTEST_TEST_CASE("Melody: period exactly 128 aligns and caches beyond the nominal global period")
{
    MelodyRig rig;
    ConfigurePeriodRig(rig, 8);
    SetGateRhythm(rig, 3, {true, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false});
    CheckPeriodWindow(rig, 127, 0, 128);
    CheckPeriodWindow(rig, 128, 128, 256);
    CheckPeriodWindow(rig, -1, -128, 0);
}

DOCTEST_TEST_CASE("Melody: period exactly 256 fills every displayed voice at signed boundaries and seeks")
{
    MelodyRig rig;
    ConfigurePeriodRig(rig, 16);
    SetGateRhythm(rig, 4, {true, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false});
    CheckPeriodWindow(rig, 255, 0, 256);
    CheckPeriodWindow(rig, 256, 256, 512);
    CheckPeriodWindow(rig, -1, -256, 0);
    CheckPeriodWindow(rig, 0, 0, 256);
    CheckPeriodWindow(rig, 512, 512, 768);
    CheckPeriodWindow(rig, -512, -512, -256);

    auto& state = rig.m_ui->m_nonagonUIState;
    for (auto& muted : state.m_muted)
    {
        muted.store(true);
    }

    CheckPeriodWindow(rig, 1023, 768, 1024);
    DOCTEST_CHECK(state.m_sequences[0].StartPosition() <= 768);
    DOCTEST_CHECK(state.m_sequences[0].EndPosition() >= 1024);
}

DOCTEST_TEST_CASE("Melody: combined arp and gate periods between 128 and 256 remain visible")
{
    MelodyRig rig;
    ConfigurePeriodRig(rig, 4);
    SetGateRhythm(rig, 2, {true, false, false});
    auto& state = rig.m_ui->m_nonagonUIState;
    state.m_indexArpUIState.m_clockSelect[0].store(4);
    state.m_indexArpUIState.m_arpUIState[0].m_rhythmLength.store(5);
    CheckPeriodWindow(rig, 25, 0, 240);
}

DOCTEST_TEST_CASE("Melody: oversized arp is omitted before gate rhythm contributions")
{
    MelodyRig rig;
    ConfigurePeriodRig(rig, 4);
    SetGateRhythm(rig, 2, {true, false, false});
    auto& state = rig.m_ui->m_nonagonUIState;
    state.m_indexArpUIState.m_clockSelect[0].store(5);
    state.m_indexArpUIState.m_arpUIState[0].m_rhythmLength.store(5);
    CheckPeriodWindow(rig, 25, 24, 36);
}

DOCTEST_TEST_CASE("Melody: oversized gate rhythms are neglected in ascending loop order and stop when fitting")
{
    MelodyRig rig;
    ConfigurePeriodRig(rig, 3);
    SetGateRhythm(rig, 0, {true, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false});
    SetGateRhythm(rig, 1, {true, false, false, false, false, false, false});
    rig.m_ui->m_nonagonUIState.m_theoryOfTimeUIState.m_periodTicks[1].store(8);
    CheckPeriodWindow(rig, 25, 0, 56);
    auto& sequence = rig.m_ui->m_nonagonUIState.m_sequences[0];
    DOCTEST_CHECK((sequence.GetPoint(15).m_timeSlice.m_bits & 1) != 0);
    DOCTEST_CHECK((sequence.GetPoint(16).m_timeSlice.m_bits & 1) == 0);
}

DOCTEST_TEST_CASE("Melody: residual oversized clock periods scroll in centered 256 tick windows")
{
    MelodyRig rig;
    ConfigurePeriodRig(rig, 1);
    rig.m_ui->m_nonagonUIState.m_theoryOfTimeUIState.m_periodTicks[0].store(257);
    SetGateRhythm(rig, 0, {true, false});
    CheckPeriodWindow(rig, 9, -119, 137);
    CheckPeriodWindow(rig, -9, -137, 119);
    CheckPeriodWindow(rig, 0, -128, 128);
    CheckPeriodWindow(rig, 256, 128, 384);
    CheckPeriodWindow(rig, 257, 129, 385);
    CheckPeriodWindow(rig, -1, -129, 127);
}

DOCTEST_TEST_CASE("Melody: a nonreference displayed page interval controls the trio arp repeat")
{
    MelodyRig rig;
    ConfigurePeriodRig(rig, 0);
    auto& state = rig.m_ui->m_nonagonUIState;
    state.m_indexArpUIState.m_clockSelect[0].store(0);
    state.m_indexArpUIState.m_resetSelect[0].store(3);
    for (size_t voice = 0; voice < 3; ++voice)
    {
        state.m_indexArpUIState.m_arpUIState[voice].m_rhythmLength.store(2);
    }

    CheckPeriodWindow(rig, 9, 8, 10);
    state.m_indexArpUIState.m_arpUIState[1].m_pageInterval.store(0.5f);
    CheckPeriodWindow(rig, 9, 8, 16);
    state.m_muted[1].store(true);
    CheckPeriodWindow(rig, 9, 8, 10);
}

DOCTEST_TEST_CASE("Melody: effective gate reset can make a nonconstant stored rhythm constant")
{
    MelodyRig rig;
    ConfigurePeriodRig(rig, 4);
    SetGateRhythm(rig, 2, {true, true, false});
    rig.m_ui->m_nonagonUIState.m_theoryOfTimeUIState.m_rhythm[2].m_resetLoopIndex.store(3);
    CheckPeriodWindow(rig, 9, 9, 10);
    CheckPeriodWindow(rig, -1, -1, 0);
    DOCTEST_CHECK((rig.m_ui->m_nonagonUIState.m_sequences[0].GetPoint(-1).m_timeSlice.m_bits & 4) != 0);
}

DOCTEST_TEST_CASE("Melody: huge constant gate clocks do not force scrolling")
{
    MelodyRig rig;
    ConfigurePeriodRig(rig, 1);
    rig.m_ui->m_nonagonUIState.m_theoryOfTimeUIState.m_periodTicks[0].store(
        std::numeric_limits<int64_t>::max());
    SetGateRhythm(rig, 0, {false, false, false});
    CheckPeriodWindow(rig, 9, 9, 10);
    SetGateRhythm(rig, 0, {true, false});
    CheckPeriodWindow(rig, 9, -119, 137);
}

DOCTEST_TEST_CASE("Melody: huge effective gate resets preserve primitive periods without scanning the reset prefix")
{
    MelodyRig rig;
    ConfigurePeriodRig(rig, 1);
    auto& time = rig.m_ui->m_nonagonUIState.m_theoryOfTimeUIState;
    time.m_periodTicks[4].store(std::numeric_limits<int64_t>::max() - 1);
    time.m_rhythm[0].m_resetLoopIndex.store(4);
    SetGateRhythm(rig, 0, {true, false});
    CheckPeriodWindow(rig, 9, 8, 10);
    time.m_periodTicks[4].store(std::numeric_limits<int64_t>::max());
    CheckPeriodWindow(rig, 9, 9, 10);
}

DOCTEST_TEST_CASE("Melody: huge arp periods saturate safely and are omitted before gate repeats")
{
    MelodyRig rig;
    ConfigurePeriodRig(rig, 1);
    SetGateRhythm(rig, 0, {true, false});
    auto& state = rig.m_ui->m_nonagonUIState;
    state.m_theoryOfTimeUIState.m_periodTicks[4].store(std::numeric_limits<int64_t>::max());
    state.m_indexArpUIState.m_clockSelect[0].store(4);
    state.m_indexArpUIState.m_arpUIState[0].m_rhythmLength.store(8);
    CheckPeriodWindow(rig, 9, 8, 10);
    state.m_indexArpUIState.m_clockSelect[0].store(0);
    state.m_indexArpUIState.m_resetSelect[0].store(4);
    state.m_indexArpUIState.m_arpUIState[0].m_pageInterval.store(0.5f);
    CheckPeriodWindow(rig, 9, 8, 10);
}

DOCTEST_TEST_CASE("Melody: explicit cache view period consumes the frozen configuration snapshot")
{
    MelodyRig rig;
    ConfigurePeriodRig(rig, 0);
    auto& state = rig.m_ui->m_nonagonUIState;
    state.m_indexArpUIState.m_clockSelect[0].store(0);
    auto& arp = state.m_indexArpUIState.m_arpUIState[0];
    arp.m_rhythmLength.store(3);
    arp.m_interval.store(0.125f);
    arp.m_max.store(1.0f);
    state.Snapshot();
    arp.m_interval.store(0.25f);
    state.PreProcess(0, 3);
    state.Process(0, 0, 3);
    DOCTEST_REQUIRE(state.m_sequences[0].HasPoint(1));
    DOCTEST_CHECK(state.m_sequences[0].GetPoint(1).m_choiceValue == doctest::Approx(0.125f));
    DOCTEST_CHECK(state.Changed());
}

DOCTEST_TEST_CASE("Melody: scrolling seeks fill the visible window despite overlapping cache buffers")
{
    MelodyRig rig;
    ConfigurePeriodRig(rig, 1);
    rig.m_ui->m_nonagonUIState.m_theoryOfTimeUIState.m_periodTicks[0].store(257);
    SetGateRhythm(rig, 0, {true, false});
    CheckPeriodWindow(rig, 0, -128, 128);
    CheckPeriodWindow(rig, 400, 272, 528);
    CheckPeriodWindow(rig, -400, -528, -272);
}

DOCTEST_TEST_CASE("Melody: aligned seeks fill the visible window despite overlapping cache buffers")
{
    MelodyRig rig;
    ConfigurePeriodRig(rig, 8);
    SetGateRhythm(rig, 3, {true, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false});
    CheckPeriodWindow(rig, 0, 0, 128);
    CheckPeriodWindow(rig, 256, 256, 384);
    CheckPeriodWindow(rig, -256, -256, -128);
}
