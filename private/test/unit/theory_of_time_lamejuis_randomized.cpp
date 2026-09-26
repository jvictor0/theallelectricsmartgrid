#include "doctest.h"
#include "TheNonagon.hpp"
#include "../support/GlobalEnv.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numeric>
#include <random>
#include <string>

namespace
{
using Matrix = LameJuisInternal::MatrixSwitch;
using Target = LameJuisInternal::LogicOperation::SwitchVal;
using Section = HarmonicSheaf::Section;
constexpr size_t x_bits = 6;
constexpr size_t x_lanes = 3;

// The oracle evaluates the Boolean matrix directly. It never calls GetValue,
// GetTotalAndHigh, SetBitVectors, RebuildSheaf, or Section::operator==.
//
struct ExpectedRow
{
    std::array<Matrix, x_bits> m_elements;
    std::array<bool, x_bits + 1> m_rhs;
    Target m_target = Target::Up;

    ExpectedRow()
    {
        m_elements.fill(Matrix::Muted);
        for (size_t count = 0; count <= x_bits; ++count)
        {
            m_rhs[count] = count % 2 == 1;
        }
    }

    size_t Total() const
    {
        size_t result = 0;
        for (Matrix element : m_elements)
        {
            result += element != Matrix::Muted;
        }

        return result;
    }

    size_t High(uint8_t bits) const
    {
        size_t result = 0;
        for (size_t bit = 0; bit < x_bits; ++bit)
        {
            bool high = (bits & (1u << bit)) != 0;
            result += (m_elements[bit] == Matrix::Normal && high)
                || (m_elements[bit] == Matrix::Inverted && !high);
        }

        return result;
    }

    bool Gate(uint8_t bits) const
    {
        return Total() != 0 && m_rhs[High(bits)];
    }

    size_t Accumulator() const
    {
        return m_target == Target::Up ? 0 : m_target == Target::Middle ? 1 : 2;
    }
};

bool SameSection(const Section& a, const Section& b)
{
    for (size_t accumulator = 0; accumulator < x_lanes; ++accumulator)
    {
        if (a.m_high[accumulator] != b.m_high[accumulator]
            || a.m_total[accumulator] != b.m_total[accumulator])
        {
            return false;
        }
    }

    return true;
}

int64_t Cycle(double phase, int64_t ratio)
{
    return static_cast<int64_t>(std::floor(phase * static_cast<double>(ratio)));
}

int64_t Wrap(int64_t index, int64_t length)
{
    int64_t result = index % length;
    return result < 0 ? result + length : result;
}

struct FrontendRig
{
    std::unique_ptr<SmartGridOneContext> m_context;
    std::unique_ptr<TheNonagonSmartGrid> m_frontend;
    std::array<ExpectedRow, x_bits> m_rows;
    std::array<std::array<bool, x_bits>, x_lanes> m_coMutes{};
    std::array<size_t, x_bits> m_ticks{};
    size_t m_frame = 0;
    size_t m_constantGateTicks = 0;
    size_t m_denominatorOnlyTriggers = 0;
    size_t m_backwardTicks = 0;
    std::string m_lastEdit = "initialization";

    FrontendRig()
    {
        GlobalEnv::ResetPerTest();
        m_context = std::make_unique<SmartGridOneContext>();
        m_frontend = std::make_unique<TheNonagonSmartGrid>(true, m_context.get());
        m_frontend->m_state.m_running = true;
        m_frontend->m_state.m_theoryOfTimeInput.m_freq = 1.0 / 2048.0;
        for (size_t lane = 0; lane < x_lanes; ++lane)
        {
            Set("LameJuisStrategy", lane, HarmonicSheaf::SectionChoiceStrategy::Lowest);
            Set("LameJuisInterval", lane, LameJuisInternal::Accumulator::Interval::Octave);
        }
    }

    template<typename T>
    void Set(const char* name, size_t i, T value)
    {
        State* state = m_frontend->m_stateSaver.Get(name, i);
        DOCTEST_REQUIRE(state != nullptr);
        DOCTEST_REQUIRE(state->m_len == sizeof(T));
        state->Set(value);
    }

    template<typename T>
    void Set(const char* name, size_t i, size_t j, T value)
    {
        State* state = m_frontend->m_stateSaver.Get(name, i, j);
        DOCTEST_REQUIRE(state != nullptr);
        DOCTEST_REQUIRE(state->m_len == sizeof(T));
        state->Set(value);
    }

    Section Evaluate(uint8_t bits) const
    {
        Section result;
        for (const ExpectedRow& row : m_rows)
        {
            if (row.Total() > 0)
            {
                ++result.m_total[row.Accumulator()];
                result.m_high[row.Accumulator()] += row.Gate(bits);
            }
        }

        return result;
    }

    void CheckClock(const TheoryOfTimeBase::Sample& previous, size_t sampleIndex)
    {
        const auto& request = m_frontend->m_state.m_theoryOfTimeInput;
        const auto& sample = m_frontend->m_nonagon.m_theoryOfTime.m_samples[sampleIndex];
        bool started = !previous.m_running;
        std::array<bool, x_bits> ticks{};
        std::array<int64_t, x_bits> ratios{};
        ratios[5] = 1;
        for (size_t bit = 0; bit < x_bits; ++bit)
        {
            int64_t ratio = previous.m_loops[bit].m_cycleRatio;
            ticks[bit] = started || Cycle(previous.m_modulatedPhase, ratio) != Cycle(sample.m_modulatedPhase, ratio);
        }

        for (int bit = 4; bit >= 0; --bit)
        {
            const auto& old = previous.m_loops[bit].m_input;
            const auto& wanted = request.m_input[bit];
            bool accept = started || (ticks[old.m_parentIndex] && ticks[wanted.m_parentIndex]);
            const auto& expected = accept ? wanted : old;
            const auto& actual = sample.m_loops[bit].m_input;
            DOCTEST_CAPTURE(bit);
            DOCTEST_REQUIRE(actual.m_parentIndex == expected.m_parentIndex);
            DOCTEST_REQUIRE(actual.m_parentMult == expected.m_parentMult);
            ratios[bit] = ratios[expected.m_parentIndex] * expected.m_parentMult;
        }

        int64_t common = 1;
        for (int64_t ratio : ratios)
        {
            common = std::lcm(common, ratio);
        }

        for (size_t bit = 0; bit < x_bits; ++bit)
        {
            DOCTEST_CAPTURE(sampleIndex);
            DOCTEST_CAPTURE(bit);
            const auto& loop = sample.m_loops[bit];
            const auto& rhythm = request.m_rhythm[bit];
            DOCTEST_REQUIRE(loop.m_cycleRatio == ratios[bit]);
            DOCTEST_REQUIRE(loop.m_periodTicks == common / ratios[bit]);
            DOCTEST_REQUIRE(static_cast<bool>(loop.m_modulatedCycleCrossed) == ticks[bit]);
            bool expectedGate = previous.m_loops[bit].m_gate;
            if (ticks[bit])
            {
                int64_t index = Cycle(sample.m_modulatedPhase, ratios[bit]);
                int ancestor = static_cast<int>(bit);
                while (ancestor < static_cast<int>(x_bits))
                {
                    if (ancestor == rhythm.m_resetLoopIndex)
                    {
                        index = Wrap(index, ratios[bit] / ratios[ancestor]);
                        break;
                    }

                    ancestor = sample.m_loops[ancestor].m_input.m_parentIndex;
                }

                expectedGate = rhythm.m_gate[Wrap(index, rhythm.m_size)];
                m_constantGateTicks += expectedGate == previous.m_loops[bit].m_gate;
                m_backwardTicks += sample.m_modulatedPhase < previous.m_modulatedPhase;
            }

            DOCTEST_REQUIRE(loop.m_gate == expectedGate);
        }
    }

    void Step()
    {
        DOCTEST_CAPTURE(m_frame);
        DOCTEST_CAPTURE(m_lastEdit);
        auto& engine = m_frontend->m_nonagon;
        auto& request = m_frontend->m_state;
        auto previous = engine.m_theoryOfTime.m_samples[8];
        std::array<bool, x_bits> ticks{};
        bool process = false;
        for (size_t sample = 1; sample <= 8; ++sample)
        {
            process = process || engine.m_theoryOfTime.m_samples[sample].m_anyChange;
            for (size_t bit = 0; bit < x_bits; ++bit)
            {
                ticks[bit] = ticks[bit] || engine.m_theoryOfTime.m_samples[sample].m_loops[bit].m_modulatedCycleCrossed;
            }
        }

        uint8_t bits = 0;
        for (size_t bit = 0; bit < x_bits; ++bit)
        {
            bits |= static_cast<uint8_t>(previous.m_loops[bit].m_gate ? 1u << bit : 0);
            m_ticks[bit] += ticks[bit];
        }

        auto oldCoMutes = m_coMutes;
        std::array<int64_t, x_lanes> expectedArpIndex;
        std::array<std::array<HarmonicSheaf::SectionWithValue, 3>, 3> oldNotes;
        for (size_t lane = 0; lane < x_lanes; ++lane)
        {
            expectedArpIndex[lane] = request.m_arpInput.m_totalIndex[lane];
            int clock = request.m_arpInput.m_clockSelect[lane];
            if (process && clock < 0)
            {
                expectedArpIndex[lane] = 0;
            }
            else if (process && ticks[clock])
            {
                int64_t ratio = previous.m_loops[clock].m_cycleRatio;
                int64_t index = Cycle(previous.m_modulatedPhase, ratio);
                int ancestor = clock;
                while (ancestor < static_cast<int>(x_bits))
                {
                    if (ancestor == request.m_arpInput.m_resetSelect[lane])
                    {
                        index = Wrap(index, ratio / previous.m_loops[ancestor].m_cycleRatio);
                        break;
                    }

                    ancestor = previous.m_loops[ancestor].m_input.m_parentIndex;
                }

                expectedArpIndex[lane] = index;
            }

            for (size_t voice = 0; voice < 3; ++voice)
            {
                oldNotes[lane][voice] = engine.m_lameJuis.m_lanes[lane].m_pitch[voice];
            }
        }

        if (process)
        {
            for (size_t row = 0; row < x_bits; ++row)
            {
                auto& expected = m_rows[row];
                const auto& wanted = request.m_lameJuisInput.m_operationInput[row];
                bool readRow = false;
                for (size_t bit = 0; bit < x_bits; ++bit)
                {
                    if (ticks[bit])
                    {
                        readRow = readRow || expected.m_elements[bit] != Matrix::Muted || wanted.m_elements[bit] != Matrix::Muted;
                        expected.m_elements[bit] = wanted.m_elements[bit];
                    }
                }

                if (readRow)
                {
                    std::copy(std::begin(wanted.m_rhs), std::end(wanted.m_rhs), expected.m_rhs.begin());
                    expected.m_target = wanted.m_switch;
                }
            }

            for (size_t lane = 0; lane < x_lanes; ++lane)
            {
                for (size_t bit = 0; bit < x_bits; ++bit)
                {
                    if (ticks[bit])
                    {
                        m_coMutes[lane][bit] = request.m_lameJuisInput.m_laneInput[lane].m_coMuteInput.m_coMutes[bit];
                    }
                }
            }
        }

        SampleTimer::s_instance->m_sample = m_frame * 8;
        m_frontend->ProcessSample(1.0f / 48000.0f);
        ++m_frame;
        for (size_t sample = 1; sample <= 8; ++sample)
        {
            CheckClock(previous, sample);
            previous = engine.m_theoryOfTime.m_samples[sample];
        }

        for (size_t row = 0; row < x_bits; ++row)
        {
            DOCTEST_CAPTURE(row);
            const auto& actual = engine.m_lameJuis.m_operations[row];
            const auto& expected = m_rows[row];
            DOCTEST_REQUIRE(actual.m_countTotal == expected.Total());
            DOCTEST_REQUIRE(actual.m_switch == expected.m_target);
            for (size_t bit = 0; bit < x_bits; ++bit)
            {
                DOCTEST_REQUIRE(actual.m_elements[bit] == expected.m_elements[bit]);
                DOCTEST_REQUIRE(((actual.m_active.m_bits & (1u << bit)) != 0) == (expected.m_elements[bit] != Matrix::Muted));
                DOCTEST_REQUIRE(((actual.m_inverted.m_bits & (1u << bit)) != 0) == (expected.m_elements[bit] == Matrix::Inverted));
            }

            for (size_t count = 0; count <= x_bits; ++count)
            {
                DOCTEST_REQUIRE(actual.m_rhs[count] == expected.m_rhs[count]);
            }

            if (process)
            {
                DOCTEST_REQUIRE(actual.m_countHigh == expected.High(bits));
                DOCTEST_REQUIRE(actual.m_gate == expected.Gate(bits));
            }
        }

        LameJuisInternal::UIState ui;
        engine.m_lameJuis.PopulateUIState(&ui);
        for (uint8_t point = 0; point < 64; ++point)
        {
            DOCTEST_CAPTURE(point);
            const auto& actual = engine.m_lameJuis.m_sheaf.m_sections[point];
            Section expected = Evaluate(point);
            for (size_t accumulator = 0; accumulator < x_lanes; ++accumulator)
            {
                DOCTEST_REQUIRE(actual.m_total[accumulator] == expected.m_total[accumulator]);
                DOCTEST_REQUIRE(actual.m_high[accumulator] == expected.m_high[accumulator]);
                DOCTEST_REQUIRE(ui.m_dimensions[accumulator].load() == expected.m_total[accumulator]);
            }
        }

        for (size_t lane = 0; lane < x_lanes; ++lane)
        {
            DOCTEST_CAPTURE(lane);
            DOCTEST_REQUIRE(request.m_arpInput.m_totalIndex[lane] == expectedArpIndex[lane]);
            bool read = false;
            uint8_t fixedMask = 0;
            for (size_t bit = 0; bit < x_bits; ++bit)
            {
                read = read || (ticks[bit] && !oldCoMutes[lane][bit]);
                fixedMask |= static_cast<uint8_t>(m_coMutes[lane][bit] ? 0 : 1u << bit);
                DOCTEST_REQUIRE(engine.m_lameJuis.m_lanes[lane].m_coMuteState.m_coMutes[bit] == m_coMutes[lane][bit]);
            }

            int clock = request.m_arpInput.m_clockSelect[lane];
            read = process && (read || (clock >= 0 && ticks[clock]));
            Section selected;
            int minimum = 100;
            for (uint8_t point = 0; point < 64; ++point)
            {
                if (((point ^ bits) & fixedMask) == 0)
                {
                    Section candidate = Evaluate(point);
                    int value = candidate.m_high[0] + candidate.m_high[1] + candidate.m_high[2];
                    if (value < minimum)
                    {
                        minimum = value;
                        selected = candidate;
                    }
                }
            }

            for (size_t voice = 0; voice < 3; ++voice)
            {
                DOCTEST_CAPTURE(voice);
                const auto& old = oldNotes[lane][voice];
                const auto& actual = engine.m_lameJuis.m_lanes[lane].m_pitch[voice];
                if (read)
                {
                    DOCTEST_REQUIRE(SameSection(actual.m_section, selected));
                    DOCTEST_REQUIRE(actual.m_value == static_cast<float>(minimum));
                    bool changed = !SameSection(old.m_section, selected) || old.m_value != minimum;
                    DOCTEST_REQUIRE(engine.m_lameJuis.m_lanes[lane].m_trigger[voice] == changed);
                    size_t voiceIndex = lane * 3 + voice;
                    DOCTEST_REQUIRE(engine.m_multiPhasorGate.m_ahdControl[voiceIndex].m_trig == changed);
                    if (changed)
                    {
                        for (size_t accumulator = 0; accumulator < x_lanes; ++accumulator)
                        {
                            float timbre = selected.m_total[accumulator] == 0 ? 0.0f
                                : static_cast<float>(selected.m_high[accumulator]) / selected.m_total[accumulator];
                            DOCTEST_REQUIRE(engine.m_output.m_extraTimbre[voiceIndex][accumulator] == doctest::Approx(timbre));
                        }
                    }

                    m_denominatorOnlyTriggers += changed && old.m_value == minimum
                        && std::equal(std::begin(old.m_section.m_high), std::end(old.m_section.m_high), std::begin(selected.m_high));
                }
                else if (previous.m_running && m_frame > 1)
                {
                    DOCTEST_REQUIRE(SameSection(actual.m_section, old.m_section));
                    DOCTEST_REQUIRE(actual.m_value == old.m_value);
                    DOCTEST_REQUIRE_FALSE(engine.m_multiPhasorGate.m_ahdControl[lane * 3 + voice].m_trig);
                    if (process)
                    {
                        DOCTEST_REQUIRE_FALSE(engine.m_lameJuis.m_lanes[lane].m_trigger[voice]);
                    }
                }
            }
        }
    }

    void Advance(size_t frames)
    {
        for (size_t frame = 0; frame < frames; ++frame)
        {
            Step();
        }
    }

    void CheckSettled()
    {
        const auto& input = m_frontend->m_state;
        const auto& time = m_frontend->m_nonagon.m_theoryOfTime;
        for (size_t bit = 0; bit < x_bits; ++bit)
        {
            DOCTEST_CAPTURE(bit);
            if (bit < 5)
            {
                DOCTEST_REQUIRE(time.GetLoop(bit, 8).m_input.m_parentIndex == input.m_theoryOfTimeInput.m_input[bit].m_parentIndex);
                DOCTEST_REQUIRE(time.GetLoop(bit, 8).m_input.m_parentMult == input.m_theoryOfTimeInput.m_input[bit].m_parentMult);
            }

            for (size_t lane = 0; lane < x_lanes; ++lane)
            {
                DOCTEST_REQUIRE(m_coMutes[lane][bit] == input.m_lameJuisInput.m_laneInput[lane].m_coMuteInput.m_coMutes[bit]);
            }

            for (size_t row = 0; row < x_bits; ++row)
            {
                DOCTEST_REQUIRE(m_rows[row].m_elements[bit] == input.m_lameJuisInput.m_operationInput[row].m_elements[bit]);
            }
        }

        for (size_t row = 0; row < x_bits; ++row)
        {
            if (m_rows[row].Total() > 0)
            {
                DOCTEST_REQUIRE(m_rows[row].m_target == input.m_lameJuisInput.m_operationInput[row].m_switch);
                for (size_t count = 0; count <= x_bits; ++count)
                {
                    DOCTEST_REQUIRE(m_rows[row].m_rhs[count] == input.m_lameJuisInput.m_operationInput[row].m_rhs[count]);
                }
            }
        }
    }

    void CheckGrid()
    {
        for (size_t lane = 0; lane < x_lanes; ++lane)
        {
            DOCTEST_CAPTURE(lane);
            auto& view = m_frontend->m_nonagon.m_lameJuis.m_lanes[lane].m_gridSheafView;
            uint8_t fixedMask = 0;
            for (size_t bit = 0; bit < x_bits; ++bit)
            {
                fixedMask |= static_cast<uint8_t>(m_coMutes[lane][bit] ? 0 : 1u << bit);
            }

            uint64_t cells = 0;
            for (uint8_t x = 0; x < 8; ++x)
            {
                for (uint8_t y = 0; y < 8; ++y)
                {
                    uint8_t point = view.m_cellBaseTimeSlices[x][y].m_bits;
                    DOCTEST_REQUIRE((cells & (uint64_t{1} << point)) == 0);
                    cells |= uint64_t{1} << point;
                    for (uint8_t bit = 0; bit < x_bits; ++bit)
                    {
                        uint8_t other = static_cast<uint8_t>(point ^ (1u << bit));
                        bool sameSlice = ((point ^ other) & fixedMask) == 0;
                        bool actualSameSlice = view.LensEquivalent(HarmonicSheaf::BitVector(point), HarmonicSheaf::BitVector(other));
                        DOCTEST_CHECK(actualSameSlice == sameSlice);
                        if (actualSameSlice != sameSlice)
                        {
                            return;
                        }
                    }
                }
            }

            DOCTEST_REQUIRE(cells == UINT64_MAX);
        }

    }

    void Settle()
    {
        // Keep the final topology, modulation and rhythm running. Never reset
        // the engine or flatten the clocks to make pending edits disappear.
        // Four global cycles exceed the bounded modulation excursion and give
        // every old/requested parent and every input repeated acceptance ticks.
        //
        auto before = m_ticks;
        size_t frames = static_cast<size_t>(std::ceil(4.0 / (8.0 * m_frontend->m_state.m_theoryOfTimeInput.m_freq)));
        Advance(frames);
        CheckSettled();
        for (size_t bit = 0; bit < x_bits; ++bit)
        {
            DOCTEST_REQUIRE(m_ticks[bit] >= before[bit] + 2);
        }

        Advance(frames);
        CheckSettled();
    }

    void Edit(std::mt19937& random, size_t kind)
    {
        size_t bit = random() % 6;
        size_t row = random() % 6;
        size_t lane = random() % 3;
        m_lastEdit = "kind=" + std::to_string(kind) + " bit=" + std::to_string(bit)
            + " row=" + std::to_string(row) + " lane=" + std::to_string(lane);
        switch (kind)
        {
            case 0:
                Set("LameJuisMatrixSwitch", bit, row, static_cast<Matrix>(random() % 3));
                break;
            case 1:
                Set("LameJuisRHS", row, random() % 7, (random() & 1) != 0);
                break;
            case 2:
                Set("LameJuisCoMute", bit, lane, (random() & 1) != 0);
                break;
            case 3:
                Set("LameJuisEquationOutputSwitch", row, static_cast<Target>(random() % 3));
                break;
            case 4:
                Set("TheoryOfTimeMult", random() % 5, static_cast<int>(1 + random() % 5));
                break;
            case 5:
            {
                size_t child = random() % 4;
                Set("TheoryOfTimeParentIx", child, static_cast<int>(child + 1 + random() % 2));
                break;
            }

            case 6:
            {
                Set("TheoryOfTimeRhythmSize", bit, static_cast<int>(1 + random() % 8));
                uint32_t mask = random() % 4 == 0 ? 0 : random() % 4 == 0 ? 255 : random() % 256;
                for (size_t step = 0; step < 8; ++step)
                {
                    Set("TheoryOfTimeRhythm", bit, step, (mask & (1u << step)) != 0);
                }

                break;
            }

            case 7:
                Set("TheoryOfTimeRhythmReset", bit, static_cast<int>(random() % 7) - 1);
                break;
            case 8:
                for (size_t input = 0; input < 6; ++input)
                {
                    Set("LameJuisMatrixSwitch", input, row, input == bit && (random() & 1) ? Matrix::Normal : Matrix::Muted);
                }

                break;
            case 9:
                Set("IndexArpClockSelect", lane, static_cast<int>(random() % 7) - 1);
                Set("IndexArpResetSelect", lane, static_cast<int>(random() % 7) - 1);
                break;
            case 10:
                m_frontend->m_state.m_theoryOfTimeInput.m_modIndex.Update(static_cast<float>(random() % 9) / 8.0f);
                break;
            case 11:
                m_frontend->m_state.m_theoryOfTimeInput.m_freq = 1.0 / static_cast<double>(1024u << (random() % 3));
                break;
        }
    }
};
}

DOCTEST_TEST_CASE("Frontend convergence: seeded clock and matrix edits latch on their ticks and settle")
{
    for (uint32_t seed : {0x5eed1234u, 0x12345678u, 0xdeadbeefu, 0x10203040u,
                          0xc001d00du, 0x707a1234u, 0x31415926u, 0x27182818u})
    {
        DOCTEST_SUBCASE(std::to_string(seed).c_str())
        {
            DOCTEST_CAPTURE(seed);
            FrontendRig rig;
            std::mt19937 random(seed);
            for (size_t epoch = 0; epoch < 3; ++epoch)
            {
                DOCTEST_CAPTURE(epoch);
                for (size_t edit = 0; edit < 144; ++edit)
                {
                    DOCTEST_CAPTURE(edit);
                    rig.Edit(random, edit % 12);
                    rig.Advance(random() % 8);
                }

                rig.Settle();
            }

            DOCTEST_REQUIRE(rig.m_constantGateTicks > 0);
            DOCTEST_REQUIRE(rig.m_denominatorOnlyTriggers > 0);
            DOCTEST_REQUIRE(rig.m_backwardTicks > 0);
        }
    }
}

DOCTEST_TEST_CASE("Frontend convergence: untouched co-mute lanes publish the settled grid")
{
    for (uint32_t seed : {0x10203040u, 0x10293847u, 0x87654321u})
    {
        DOCTEST_SUBCASE(std::to_string(seed).c_str())
        {
            DOCTEST_CAPTURE(seed);
            FrontendRig rig;
            std::mt19937 random(seed);
            // Leave co-mutes at their initial all-readable setting while every
            // other frontend control changes. A cached view must still agree
            // with that setting without requiring a sacrificial co-mute edit.
            //
            for (size_t edit = 0; edit < 144; ++edit)
            {
                size_t kind = edit % 12;
                if (kind != 2)
                {
                    rig.Edit(random, kind);
                }

                rig.Advance(random() % 8);
            }

            rig.Settle();
            rig.CheckGrid();

            // Reach the same all-readable setting via an accepted edit in
            // every lane. Both histories must produce a valid grid view.
            //
            for (size_t lane = 0; lane < x_lanes; ++lane)
            {
                rig.Set("LameJuisCoMute", size_t{5}, lane, true);
            }

            rig.Settle();
            for (size_t lane = 0; lane < x_lanes; ++lane)
            {
                rig.Set("LameJuisCoMute", size_t{5}, lane, false);
            }

            rig.Settle();
            rig.CheckGrid();
        }
    }
}
