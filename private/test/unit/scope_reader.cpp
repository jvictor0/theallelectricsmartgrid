#include "doctest.h"

#include <memory>

#include "../support/GlobalEnv.hpp"
#include "ScopeWriter.hpp"
#include "SquiggleBoy.hpp"

namespace
{
// The fractional-offset expectations below are worked out for this decimation.
//
constexpr size_t x_decimation = 8;

void WriteAt(ScopeWriterInput<1>& writer, size_t scope, size_t voice, size_t index, float value)
{
    size_t current = writer.m_index;
    writer.m_index = index;
    writer.Write(scope, MultiChannelNumber<float, 1>(value));
    writer.m_index = current;
}

void AdvanceTo(ScopeWriter& writer, size_t index)
{
    while (writer.m_index < index || !writer.IsWriteSample())
    {
        writer.AdvanceIndex();
    }
}

struct ScopeReaderFixture
{
    std::unique_ptr<ScopeWriterGeneric<1, 1>> m_writer = std::make_unique<ScopeWriterGeneric<1, 1>>(1, 4096);

    ScopeReaderFixture(float currentOffset)
    {
        for (size_t index = 0; index < 40; ++index)
        {
            float value = index < 20 ? static_cast<float>(index) - 10.0f
                                    : currentOffset + static_cast<float>(index - 20);
            WriteAt(*m_writer, 0, 0, index, value);
        }

        m_writer->RecordStart(0, 0, 10);
        m_writer->RecordStart(0, 0, 20);
        PublishThrough(24);
    }

    void PublishThrough(size_t index)
    {
        m_writer->m_index = index + 1;
        m_writer->Publish();
        m_writer->AcceptSamples();
    }
};
}

DOCTEST_TEST_CASE("ScopeReader: fractional reads interpolate on both sides of the transfer")
{
    ScopeReaderFixture fixture(100.0f);
    ScopeReader reader(fixture.m_writer.get(), 0, 0, 8, 1);

    DOCTEST_CHECK(reader.m_transferXSample == doctest::Approx(3.2));
    DOCTEST_CHECK(reader.Get(1.5) == doctest::Approx(101.875));
    DOCTEST_CHECK(reader.Get(3.1) == doctest::Approx(103.875));
    DOCTEST_CHECK(reader.Get(3.3) == doctest::Approx(4.125));
    DOCTEST_CHECK(reader.Get(6.5) == doctest::Approx(8.125));
}

DOCTEST_TEST_CASE("ScopeReader: advancing the transfer does not move a repeating waveform")
{
    ScopeReaderFixture fixture(0.0f);
    for (size_t index = 24; index <= 29; ++index)
    {
        fixture.PublishThrough(index);
        ScopeReader reader(fixture.m_writer.get(), 0, 0, 8, 1);
        DOCTEST_INFO("published sample: " << index);
        DOCTEST_CHECK(reader.Get(1) == doctest::Approx(1.25));
        DOCTEST_CHECK(reader.Get(2) == doctest::Approx(2.5));
        DOCTEST_CHECK(reader.Get(6) == doctest::Approx(7.5));
    }
}

DOCTEST_TEST_CASE("ScopeReader: fractional reads work without a previous-cycle transfer")
{
    ScopeReaderFixture fixture(100.0f);
    fixture.PublishThrough(34);
    ScopeReader reader(fixture.m_writer.get(), 0, 0, 8, 1);

    DOCTEST_CHECK(reader.Get(1.5) == doctest::Approx(102.625));
    DOCTEST_CHECK(reader.Get(6.5) == doctest::Approx(111.375));
}

DOCTEST_TEST_CASE("ScopeReader: the unfinished trace uses the previous cycle before its gap")
{
    for (size_t gap : {0, 8, 64})
    {
        for (double fraction : {0.0, 0.25})
        {
            DOCTEST_CAPTURE(gap);
            DOCTEST_CAPTURE(fraction);
            ScopeWriterGeneric<1, 1> writer(1, 4096);
            size_t currentStart = 26 + gap;
            for (size_t index = 0; index <= currentStart + 8; ++index)
            {
                if (index == 10)
                {
                    writer.RecordStart(0, 0, fraction);
                }

                if (index == 26)
                {
                    writer.RecordEnd(0, 0);
                }

                if (index == currentStart)
                {
                    writer.RecordStart(0, 0, fraction);
                }

                float value = index < 26 ? static_cast<float>(index) - 10.0f : -1000.0f;
                if (index >= currentStart)
                {
                    value = 100.0f + static_cast<float>(index - currentStart);
                }

                writer.Write(0, value);
                writer.AdvanceIndex();
            }

            writer.Publish();
            for (size_t elapsed : {1, 4, 8})
            {
                DOCTEST_CAPTURE(elapsed);
                writer.AcceptSamples(currentStart + elapsed + 1);
                ScopeReader reader(&writer, 0, 0, 8, 1);
                DOCTEST_REQUIRE_FALSE(reader.m_empty);
                double previousLength = 16.0 - fraction;
                DOCTEST_CHECK(reader.m_transferXSample
                    == doctest::Approx((elapsed - fraction) * 8.0 / previousLength));
                for (double sample : {0.0, 1.5, 3.3, 6.5, 7.0})
                {
                    DOCTEST_CAPTURE(sample);
                    double offset = fraction + sample * previousLength / 8.0;
                    double expected = sample < reader.m_transferXSample ? 100.0 + offset : offset;
                    DOCTEST_CHECK(reader.Get(sample) == doctest::Approx(expected));
                }
            }
        }
    }
}

DOCTEST_TEST_CASE("ScopeReader: an ended short cycle borrows the previous cycle without its gap")
{
    ScopeWriterGeneric<1, 1> writer(1, 4096);
    for (size_t index = 0; index < 80; ++index)
    {
        if (index == 10 || index == 58)
        {
            writer.RecordStart(0, 0);
        }

        if (index == 26 || index == 62)
        {
            writer.RecordEnd(0, 0);
        }

        float value = index < 26 ? static_cast<float>(index) - 10.0f : -1000.0f;
        if (index >= 58 && index < 62)
        {
            value = 100.0f + static_cast<float>(index - 58);
        }

        writer.Write(0, value);
        writer.AdvanceIndex();
    }

    writer.Publish();
    writer.AcceptSamples();
    ScopeReader reader(&writer, 0, 0, 8, 1);
    DOCTEST_REQUIRE_FALSE(reader.m_empty);
    DOCTEST_CHECK(reader.m_transferIndex == 61.0);
    DOCTEST_CHECK(reader.Get(1.0) == doctest::Approx(102.0));
    DOCTEST_CHECK(reader.Get(2.0) == doctest::Approx(4.0));
    DOCTEST_CHECK(reader.Get(6.5) == doctest::Approx(13.0));
    DOCTEST_CHECK(reader.Get(7.0) == doctest::Approx(14.0));
}

DOCTEST_TEST_CASE("ScopeReader: the right endpoint stays finite at a full transfer")
{
    ScopeReaderFixture fixture(100.0f);
    fixture.PublishThrough(30);
    ScopeReader reader(fixture.m_writer.get(), 0, 0, 8, 1);

    DOCTEST_CHECK(reader.Get(8.0) == doctest::Approx(110.0));
}

DOCTEST_TEST_CASE("ScopeReader: one published sample has a finite display")
{
    auto writer = std::make_unique<ScopeWriterGeneric<1, 1>>(1, 4096);
    writer->Write(0, 0.5f);
    writer->RecordStart(0, 0);
    writer->AdvanceIndex();
    writer->Publish();
    writer->AcceptSamples();
    ScopeReader reader(writer.get(), 0, 0, 8, 1);

    DOCTEST_CHECK(reader.Get(0.0) == doctest::Approx(0.5));
    DOCTEST_CHECK(reader.Get(4.5) == doctest::Approx(0.5));
    DOCTEST_CHECK(reader.Get(8.0) == doctest::Approx(0.5));
}

DOCTEST_TEST_CASE("ScopeReader: oscillator tops retain their subsample phase crossing")
{
    GlobalEnv::ResetPerTest();
    AdaptiveWaveTable table;
    for (size_t i = 0; i < BasicWaveTable::x_tableSize; ++i)
    {
        table.m_waveTable.m_table[i] = 0.25f;
    }

    table.Generate();
    for (size_t topOversample = 0; topOversample < 4; ++topOversample)
    {
        auto writer = std::make_unique<ScopeWriterGeneric<1, 3>>(1, 4096);
        for (size_t scope = 0; scope < 3; ++scope)
        {
            for (size_t i = 0; i < 100; ++i)
            {
                WriteAt(*writer, scope, 0, i, -0.25f);
            }
        }

        writer->m_index = 100;
        auto sourceStorage = std::make_unique<SquiggleBoySource>();
        auto& source = *sourceStorage;
        auto& oscillator = source.m_dualWaveShapingVCO;
        SquiggleBoySource::Input sourceInput;
        sourceInput.m_dualWaveShapingVCOInput.m_baseFreq = 0.0625f;
        for (size_t i = 0; i < 2; ++i)
        {
            oscillator.m_vco[i].m_phase = 0.9921875f - static_cast<float>(topOversample) / 64.0f;
            oscillator.m_vco[i].SetLeft(&table);
            oscillator.m_vco[i].SetRight(&table);
            oscillator.m_scopeWriter[i] = ScopeWriterHolder(writer.get(), i);
        }

        SquiggleBoyShape filter;
        SquiggleBoyShape::Input filterInput;
        filter.m_scopeWriter = ScopeWriterHolder(writer.get(), 2);
        for (size_t i = 0; i < 8; ++i)
        {
            filter.Process(filterInput, source.Process(sourceInput));
            filter.WriteScopes(source.m_top);
            writer->AdvanceIndex();
        }

        writer->Publish();
        writer->AcceptSamples();
        constexpr double x_expectedStarts[] = {99.125, 99.375, 99.625, 99.875};
        for (size_t scope = 0; scope < 3; ++scope)
        {
            ScopeReader reader(writer.get(), 0, scope, 1024, 1);
            DOCTEST_CHECK(reader.m_startIndex == doctest::Approx(x_expectedStarts[topOversample]));
        }
    }
}

DOCTEST_TEST_CASE("ScopeReader: interpolated cycle length and reads survive buffer wrap")
{
    auto writer = std::make_unique<ScopeWriterGeneric<1, 1>>(1, 4096);
    writer->m_index = writer->MaxIndexes() - 16;
    for (size_t i = 0; i < 25; ++i)
    {
        WriteAt(*writer, 0, 0, writer->m_index + i, static_cast<float>(i));
    }

    writer->RecordStart(0, 0, 9.75);
    writer->RecordStart(0, 0, 20.25);
    writer->m_index += 25;
    writer->Publish();
    writer->AcceptSamples();
    ScopeReader reader(writer.get(), 0, 0, 8, 1);

    DOCTEST_CHECK(reader.m_startIndex == doctest::Approx(static_cast<double>(writer->MaxIndexes()) + 4.25));
    DOCTEST_CHECK(reader.Get(1.5) == doctest::Approx(22.21875));
    DOCTEST_CHECK(reader.Get(4.5) == doctest::Approx(15.65625));
    DOCTEST_CHECK(reader.Get(6.5) == doctest::Approx(18.28125));
}

DOCTEST_TEST_CASE("ScopeReader: a top beyond the accepted boundary keeps the previous cycle visible")
{
    ScopeReaderFixture fixture(100.0f);
    fixture.m_writer->RecordStart(0, 0);
    fixture.m_writer->Publish();
    fixture.m_writer->AcceptSamples();
    ScopeReader reader(fixture.m_writer.get(), 0, 0, 8, 1);

    DOCTEST_CHECK_FALSE(reader.m_empty);
    DOCTEST_CHECK(reader.m_startIndex == 20.0);
    DOCTEST_CHECK(reader.m_transferIndex == 24.0);
}

DOCTEST_TEST_CASE("ScopeReader: a first top before sample zero has no readable history")
{
    auto writer = std::make_unique<ScopeWriterGeneric<1, 1>>(1, 4096);
    writer->Write(0, 0.5f);
    writer->RecordStart(0, 0, -0.25);
    writer->AdvanceIndex();
    writer->Publish();
    writer->AcceptSamples();
    ScopeReader reader(writer.get(), 0, 0, 8, 1);

    DOCTEST_CHECK(reader.m_empty);
    DOCTEST_CHECK(reader.Get(0.0) == 0.0f);
}

DOCTEST_TEST_CASE("ScopeReader: PolyXFader top reaches its control-rate scope between samples")
{
    GlobalEnv::ResetPerTest();
    TheoryOfTimeBase time;
    TheoryOfTimeBase::Input clock;
    clock.m_running = true;
    for (size_t i = 0; i < TheoryOfTimeBase::x_globalLoop; ++i)
    {
        clock.m_input[i].m_parentIndex = TheoryOfTimeBase::x_globalLoop;
        clock.m_input[i].m_parentMult = 1;
    }

    clock.m_unmodulatedPhase = 0.875;
    time.Process(clock);
    clock.m_unmodulatedPhase = 1.125;
    time.Process(clock);

    auto writer = std::make_unique<ScopeWriterGeneric<1, 1>>(x_decimation, 4096);
    for (size_t i = 0; i < 12; ++i)
    {
        WriteAt(*writer, 0, 0, i, 0.5f);
    }

    writer->m_index = 9;
    writer->m_subIndex = 2;
    SquiggleLFOGeneric<1> lfo;
    SquiggleLFOGeneric<1>::Input input;
    input.m_polyXFaderInput.m_theoryOfTime = &time;
    input.m_polyXFaderInput.m_size = 1;
    lfo.m_scopeWriter = ScopeWriterHolder(writer.get(), 0);
    lfo.Process(input);
    AdvanceTo(*writer, 11);
    writer->Publish();
    writer->AcceptSamples();
    ScopeReader reader(writer.get(), 0, 0, 1024, 1);

    DOCTEST_CHECK(reader.m_startIndex == doctest::Approx(9.1875));
}

DOCTEST_TEST_CASE("ScopeReader: Theory of Time captures one phase per control sample")
{
    GlobalEnv::ResetPerTest();
    TheoryOfTime time;
    TheoryOfTime::Input input;
    input.m_running = true;
    input.m_freq = 1.0 / 32.0;
    input.m_modIndex.m_expParam = 0.0;

    auto writer = std::make_unique<ScopeWriterGeneric<1, 1>>(x_decimation, 4096);
    for (size_t index = 99; index < 113; ++index)
    {
        WriteAt(*writer, 0, 0, index, -1.0f);
    }

    writer->m_index = 100;
    time.SetupMonoScopeWriter(writer.get());
    constexpr float x_phases[] = {0.03125f, 0.28125f, 0.53125f, 0.78125f, 0.03125f};
    for (size_t block = 0; block < 5; ++block)
    {
        DOCTEST_CAPTURE(block);
        for (size_t sample = 0; sample < x_decimation; ++sample)
        {
            time.Process(input);
            writer->AdvanceIndex();
        }

        DOCTEST_CHECK(writer->ReadSample(0, 0, 99) == -1.0f);
        for (size_t previous = 0; previous <= block; ++previous)
        {
            DOCTEST_CHECK(writer->ReadSample(0, 0, 100 + previous) == doctest::Approx(x_phases[previous]));
        }

        for (size_t future = 101 + block; future < 113; ++future)
        {
            DOCTEST_CHECK(writer->ReadSample(0, 0, future) == -1.0f);
        }
    }
}

DOCTEST_TEST_CASE("ScopeReader: Theory of Time maps fractional tops into decimated control samples across blocks")
{
    struct Case
    {
        double m_initialPhase;
        double m_startIndex;
        size_t m_firstBlockStarts;
    };

    // At 1/8 cycle per audio sample, these crossings land 1.5, 6.5, 7, 7.5
    // and 8 samples after the first stored sample. The last two fall after
    // the first block's final sample, so the second block records them.
    //
    constexpr Case x_cases[] =
    {
        {0.8125, 100.1875, 2},
        {0.1875, 100.8125, 2},
        {0.125, 100.875, 2},
        {0.0625, 100.9375, 1},
        {0.0, 101.0, 1},
    };

    for (const Case& item : x_cases)
    {
        DOCTEST_CAPTURE(item.m_initialPhase);
        GlobalEnv::ResetPerTest();
        TheoryOfTime time;
        TheoryOfTime::Input input;
        input.m_running = true;
        input.m_unmodulatedPhase = item.m_initialPhase;
        input.m_freq = 0.125;
        input.m_modIndex.m_expParam = 0.0;
        for (size_t i = 0; i < TheoryOfTimeBase::x_globalLoop; ++i)
        {
            input.m_input[i].m_parentIndex = TheoryOfTimeBase::x_globalLoop;
            input.m_input[i].m_parentMult = 1;
        }

        auto writer = std::make_unique<ScopeWriterGeneric<1, 1>>(x_decimation, 4096);
        writer->m_index = 100;
        time.SetupMonoScopeWriter(writer.get());
        for (size_t block = 0; block < 2; ++block)
        {
            for (size_t sample = 0; sample < x_decimation; ++sample)
            {
                time.Process(input);
                writer->AdvanceIndex();
            }

            writer->Publish();
            writer->AcceptSamples();
            DOCTEST_CHECK(writer->GetMarkers(0, 0).m_startIndexIndex.load() == (block == 0 ? item.m_firstBlockStarts : 2));

            // Keep the second block below the next crossing while making its
            // first-sample phase available for the reader's fractional lookup.
            //
            input.m_freq = 1.0 / 256.0;
        }

        DOCTEST_CHECK(writer->GetMarkers(0, 0).m_startIndices[0] == doctest::Approx(100.125));
        ScopeReader reader(writer.get(), 0, 0, 1024, 1);
        DOCTEST_CHECK_FALSE(reader.m_empty);
        DOCTEST_CHECK(reader.m_startIndex == doctest::Approx(item.m_startIndex));
    }
}

DOCTEST_TEST_CASE("ScopeReader: a PolyXFader top on a stored sample keeps the preceding fractional time")
{
    GlobalEnv::ResetPerTest();
    TheoryOfTimeBase time;
    TheoryOfTimeBase::Input clock;
    clock.m_running = true;
    clock.m_input[0].m_parentIndex = 5;
    clock.m_input[0].m_parentMult = 1;
    clock.m_unmodulatedPhase = 0.875;
    time.Process(clock);
    clock.m_unmodulatedPhase = 1.125;
    time.Process(clock);

    auto writer = std::make_unique<ScopeWriterGeneric<1, 1>>(x_decimation, 4096);
    for (size_t i = 0; i < 21; ++i)
    {
        WriteAt(*writer, 0, 0, i, 0.5f);
    }

    writer->m_index = 20;
    SquiggleLFOGeneric<1> lfo;
    SquiggleLFOGeneric<1>::Input input;
    input.m_polyXFaderInput.m_theoryOfTime = &time;
    input.m_polyXFaderInput.m_size = 1;
    lfo.m_scopeWriter = ScopeWriterHolder(writer.get(), 0);
    lfo.Process(input);
    AdvanceTo(*writer, 21);
    writer->Publish();
    writer->AcceptSamples();
    ScopeReader reader(writer.get(), 0, 0, 1024, 1);

    DOCTEST_CHECK(reader.m_startIndex == doctest::Approx(19.9375));
}
