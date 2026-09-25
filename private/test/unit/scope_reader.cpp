#include "doctest.h"

#include <memory>

#include "../support/GlobalEnv.hpp"
#include "QuadLFO.hpp"
#include "ScopeWriter.hpp"
#include "SquiggleBoy.hpp"

namespace
{
struct ScopeReaderFixture
{
    std::unique_ptr<ScopeWriter> m_writer = std::make_unique<ScopeWriter>(1, 1);

    ScopeReaderFixture(float currentOffset)
    {
        for (size_t index = 0; index < 40; ++index)
        {
            float value = index < 20 ? static_cast<float>(index) - 10.0f
                                    : currentOffset + static_cast<float>(index - 20);
            m_writer->Write(0, 0, index, value);
        }

        m_writer->RecordStart(0, 0, 10);
        m_writer->RecordStart(0, 0, 20);
        PublishThrough(24);
    }

    void PublishThrough(size_t index)
    {
        m_writer->m_index = index + 1;
        m_writer->Publish();
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
    for (size_t index = 21; index <= 29; ++index)
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

DOCTEST_TEST_CASE("ScopeReader: the right endpoint stays finite at a full transfer")
{
    ScopeReaderFixture fixture(100.0f);
    fixture.PublishThrough(30);
    ScopeReader reader(fixture.m_writer.get(), 0, 0, 8, 1);

    DOCTEST_CHECK(reader.Get(8.0) == doctest::Approx(110.0));
}

DOCTEST_TEST_CASE("ScopeReader: one published sample has a finite display")
{
    auto writer = std::make_unique<ScopeWriter>(1, 1);
    writer->Write(0, 0, 0.5f);
    writer->RecordStart(0, 0);
    writer->AdvanceIndex();
    writer->Publish();
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
        auto writer = std::make_unique<ScopeWriter>(1, 3);
        for (size_t scope = 0; scope < 3; ++scope)
        {
            for (size_t i = 0; i < 100; ++i)
            {
                writer->Write(scope, 0, i, -0.25f);
            }
        }

        writer->m_index = 100;
        auto sourceStorage = std::make_unique<SquiggleBoySource>();
        auto& source = *sourceStorage;
        auto& oscillator = source.m_dualWaveShapingVCO;
        SquiggleBoySource::Input sourceInput;
        auto& input = sourceInput.m_dualWaveShapingVCOInput;
        input.m_baseFreq = 0.0625f;
        input.m_detune.m_expParam = 1.0f;
        input.m_offsetFreqFactor.m_expParam = 1.0f;
        oscillator.m_baseFreqSlew.m_filter.m_output = input.m_baseFreq;
        oscillator.m_detuneSlew.m_filter.m_output = 1.0f;
        oscillator.m_offsetFreqFactorSlew.m_filter.m_output = 1.0f;
        for (size_t i = 0; i < 2; ++i)
        {
            oscillator.m_vco[i].m_phase = 0.9921875f - static_cast<float>(topOversample) / 64.0f;
            oscillator.m_vco[i].m_morphingWaveTable.SetLeft(&table);
            oscillator.m_vco[i].m_morphingWaveTable.SetRight(&table);
            oscillator.m_scopeWriter[i] = ScopeWriterHolder(writer.get(), 0, i);
        }

        source.ProcessUBlock(sourceInput);
        SquiggleBoyVoiceConfig config;
        SquiggleBoyVoice::FilterSection filter;
        SquiggleBoyVoice::FilterSection::Input filterInput;
        filterInput.m_voiceConfig = &config;
        filter.m_scopeWriter = ScopeWriterHolder(writer.get(), 0, 2);
        filter.ProcessUBlock(filterInput, source.m_uBlockOutput, source.m_uBlockTop);
        writer->m_index += SampleTimer::x_controlFrameRate;
        writer->Publish();
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
    auto writer = std::make_unique<ScopeWriter>(1, 1);
    writer->m_index = writer->MaxIndexes() - 16;
    for (size_t i = 0; i < 25; ++i)
    {
        writer->Write(0, 0, i, static_cast<float>(i));
    }

    writer->RecordStart(0, 0, 9.75);
    writer->RecordStart(0, 0, 20.25);
    writer->m_index += 25;
    writer->Publish();
    ScopeReader reader(writer.get(), 0, 0, 8, 1);

    DOCTEST_CHECK(reader.m_startIndex == doctest::Approx(static_cast<double>(writer->MaxIndexes()) + 4.25));
    DOCTEST_CHECK(reader.Get(1.5) == doctest::Approx(22.21875));
    DOCTEST_CHECK(reader.Get(4.5) == doctest::Approx(15.65625));
    DOCTEST_CHECK(reader.Get(6.5) == doctest::Approx(18.28125));
}

DOCTEST_TEST_CASE("ScopeReader: a top with no published sample waits for its data")
{
    ScopeReaderFixture fixture(100.0f);
    fixture.m_writer->RecordStart(0, 0);
    fixture.m_writer->Publish();
    ScopeReader reader(fixture.m_writer.get(), 0, 0, 8, 1);

    DOCTEST_CHECK(reader.m_empty);
    DOCTEST_CHECK(reader.Get(reader.m_transferXSample) == 0.0f);
}

DOCTEST_TEST_CASE("ScopeReader: a first top before sample zero has no readable history")
{
    auto writer = std::make_unique<ScopeWriter>(1, 1);
    writer->Write(0, 0, 0.5f);
    writer->RecordStart(0, 0, -0.25);
    writer->AdvanceIndex();
    writer->Publish();
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
    time.Process(1, clock);
    clock.m_unmodulatedPhase = 1.125;
    time.Process(2, clock);

    auto writer = std::make_unique<ScopeWriter>(1, 1);
    for (size_t i = 0; i < 12; ++i)
    {
        writer->Write(0, 0, i, 0.5f);
    }

    writer->m_index = 10;
    SquiggleBoyVoice::SquiggleLFO lfo;
    SquiggleBoyVoice::SquiggleLFO::Input input;
    input.m_polyXFaderInput.m_theoryOfTime = &time;
    input.m_polyXFaderInput.m_size = 1;
    lfo.m_scopeWriter = ScopeWriterHolder(writer.get(), 0, 0);
    SampleTimer::s_instance->m_sample = 2;
    lfo.Process(input);
    writer->AdvanceIndex();
    writer->Publish();
    ScopeReader reader(writer.get(), 0, 0, 1024, 1);

    DOCTEST_CHECK(reader.m_startIndex == doctest::Approx(9.1875));
}

DOCTEST_TEST_CASE("ScopeReader: Theory of Time adds fractional timing to its existing start index")
{
    GlobalEnv::ResetPerTest();
    TheoryOfTime time;
    TheoryOfTime::Input input;
    input.m_running = true;
    input.m_unmodulatedPhase = 0.8125;
    input.m_freq = 0.125;
    input.m_modIndex.m_expParam = 0.0;
    for (size_t i = 0; i < TheoryOfTimeBase::x_globalLoop; ++i)
    {
        input.m_input[i].m_parentIndex = TheoryOfTimeBase::x_globalLoop;
        input.m_input[i].m_parentMult = 1;
    }

    auto writer = std::make_unique<ScopeWriter>(1, 1);
    for (size_t i = 0; i < 106; ++i)
    {
        writer->Write(0, 0, i, 0.5f);
    }

    writer->m_index = 100;
    time.SetupMonoScopeWriter(writer.get());
    for (size_t i = 1; i <= 4; ++i)
    {
        time.Process(i, input);
    }

    writer->m_index = 105;
    writer->Publish();
    ScopeReader reader(writer.get(), 0, 0, 1024, 1);

    DOCTEST_CHECK(reader.m_startIndex == doctest::Approx(102.5));
}

DOCTEST_TEST_CASE("ScopeReader: a PolyXFader top in slot zero keeps the preceding fractional time")
{
    GlobalEnv::ResetPerTest();
    TheoryOfTimeBase time;
    TheoryOfTimeBase::Input clock;
    clock.m_running = true;
    clock.m_input[0].m_parentIndex = 5;
    clock.m_input[0].m_parentMult = 1;
    clock.m_unmodulatedPhase = 0.875;
    time.Process(7, clock);
    clock.m_unmodulatedPhase = 1.125;
    time.Process(8, clock);
    time.RolloverMicroblockBuffer();

    auto writer = std::make_unique<ScopeWriter>(1, 1);
    for (size_t i = 0; i < 21; ++i)
    {
        writer->Write(0, 0, i, 0.5f);
    }

    writer->m_index = 20;
    SquiggleBoyVoice::SquiggleLFO lfo;
    SquiggleBoyVoice::SquiggleLFO::Input input;
    input.m_polyXFaderInput.m_theoryOfTime = &time;
    input.m_polyXFaderInput.m_size = 1;
    lfo.m_scopeWriter = ScopeWriterHolder(writer.get(), 0, 0);
    SampleTimer::s_instance->m_sample = 8;
    lfo.Process(input);
    writer->AdvanceIndex();
    writer->Publish();
    ScopeReader reader(writer.get(), 0, 0, 1024, 1);

    DOCTEST_CHECK(reader.m_startIndex == doctest::Approx(19.9375));
}

DOCTEST_TEST_CASE("ScopeReader: QuadLFO top accounts for the advanced control writer")
{
    GlobalEnv::ResetPerTest();
    auto writer = std::make_unique<ScopeWriter>(4, 1);
    for (size_t voice = 0; voice < 4; ++voice)
    {
        for (size_t i = 0; i < 11; ++i)
        {
            writer->Write(0, voice, i, 0.5f);
        }
    }

    writer->m_index = 10;
    QuadLFO lfo;
    QuadLFO::Input input;
    input.m_freq = QuadFloat(0.25f, 0.125f, 0.0625f, 0.03125f);
    lfo.m_phase[0] = 0.875f;
    lfo.ConfigureScopeWriter(writer.get(), 0);
    SampleTimer::s_instance->m_sample = 7;
    lfo.Process(input);
    writer->AdvanceIndex();
    writer->Publish();
    ScopeReader reader(writer.get(), 0, 0, 1024, 1);

    DOCTEST_CHECK(reader.m_startIndex == doctest::Approx(9.8125));
}
