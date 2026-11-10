#include "doctest.h"
#include "ScopeWriter.hpp"
#include "TimeWarpInverter.hpp"
#include <thread>

DOCTEST_TEST_CASE("Warped scope: inverse views reject unpublished endpoints and overwritten coordinates")
{
    TimeWarpInverter<16> inverse;
    inverse.m_writeHeadInverse[1].store(5.0);
    inverse.m_writeHeadInverse[2].store(100.0);
    TimeWarpView view{&inverse, 10.0, 2.0, 0.0};
    DOCTEST_CHECK(view.GetRealTime(1.0) == 5.0);
    DOCTEST_CHECK_FALSE(std::isfinite(view.GetRealTime(1.01)));
    DOCTEST_CHECK_FALSE(std::isfinite(view.GetRealTime(2.1)));
    for (size_t sample = 0; sample <= 40; ++sample)
    {
        inverse.Write(static_cast<double>(sample), static_cast<double>(sample));
    }

    view = {&inverse, 40.0, 40.0, inverse.m_oldestWarpedTime};
    DOCTEST_CHECK(view.GetRealTime(30.5) == 30.5);
    DOCTEST_CHECK_FALSE(std::isfinite(view.GetRealTime(14.5)));
    DOCTEST_CHECK(view.GetRealTime(39.5) == 39.5);
}

DOCTEST_TEST_CASE("Warped scope: nonuniform time composes current and previous cycles")
{
    double origin = 0.0;
    DOCTEST_SUBCASE("Negative warped origin")
    {
        origin = -100.0;
    }

    DOCTEST_SUBCASE("Zero warped origin")
    {
    }

    TimeWarpInverter<1024> inverse;
    ScopeWriterGeneric<1, 1> writer(1, 256);
    writer.SetTimeWarp(0, &inverse);
    for (size_t sample = 0; sample <= 80; ++sample)
    {
        double warped = sample <= 40 ? sample * 0.5 : 20.0 + (sample - 40) * 0.25;
        inverse.Write(origin + warped, static_cast<double>(sample));
        if (sample == 0 || sample == 40)
        {
            writer.RecordStart(0, 0);
        }

        writer.Write(0, static_cast<float>(sample < 40 ? warped : warped - 20.0));
        writer.AdvanceIndex();
    }

    writer.Publish();
    writer.AcceptSamples();
    ScopeReader reader(&writer, 0, 0, 16, 1);
    DOCTEST_REQUIRE_FALSE(reader.m_empty);
    DOCTEST_CHECK(reader.m_startIndex == origin + 20.0);
    DOCTEST_CHECK(reader.m_transferXSample == 8.0);
    DOCTEST_CHECK(reader.Get(4.0) == doctest::Approx(5.0));
    DOCTEST_CHECK(reader.Get(12.0) == doctest::Approx(15.0));
    DOCTEST_CHECK(reader.RealIndex(reader.Coordinate(4.0)) == 60.0);
}

DOCTEST_TEST_CASE("Warped scope: rewind undoes paired ends and starts and ignores backwards triggers")
{
    TimeWarpInverter<128> inverse;
    ScopeWriterGeneric<1, 1> writer(1, 128);
    writer.SetTimeWarp(0, &inverse);
    size_t real = 0;
    auto advance = [&](double warped, bool start, bool end)
    {
        inverse.Write(warped, static_cast<double>(real++));
        if (start)
        {
            writer.RecordStart(0, 0);
        }

        if (end)
        {
            writer.RecordEnd(0, 0);
        }

        writer.Write(0, 0.5f);
        writer.AdvanceIndex();
        writer.Publish();
        writer.AcceptSamples();
    };
    advance(0, false, false);
    advance(2, true, false);
    advance(5, false, true);
    advance(7, true, false);
    advance(9, false, true);
    advance(10, false, false);
    advance(8, false, false);
    DOCTEST_CHECK(writer.GetMarkers(0, 0).m_started);
    DOCTEST_CHECK_FALSE(writer.GetAcceptedMarkers(0, 0).m_hasEnd[1]);
    advance(6, false, false);
    DOCTEST_CHECK(writer.GetAcceptedMarkers(0, 0).m_startIndexIndex == 1);
    DOCTEST_CHECK(writer.GetAcceptedMarkers(0, 0).m_hasEnd[0]);
    advance(4, false, false);
    DOCTEST_CHECK(writer.GetMarkers(0, 0).m_started);
    DOCTEST_CHECK_FALSE(writer.GetAcceptedMarkers(0, 0).m_hasEnd[0]);
    advance(3, true, true);
    DOCTEST_CHECK(writer.GetAcceptedMarkers(0, 0).m_startIndexIndex == 1);
    DOCTEST_CHECK(writer.GetMarkers(0, 0).m_started);
    advance(1, false, false);
    DOCTEST_CHECK(writer.GetAcceptedMarkers(0, 0).m_startIndexIndex == 0);
    DOCTEST_CHECK_FALSE(writer.GetMarkers(0, 0).m_started);

    // Existing restart starts remain valid on a stationary coordinate.
    //
    advance(1, true, false);
    DOCTEST_CHECK(writer.GetAcceptedMarkers(0, 0).m_startIndexIndex == 1);
}

DOCTEST_TEST_CASE("Warped scope: fractional boundaries retain absolute origin and decimation")
{
    TimeWarpInverter<128> inverse;
    ScopeWriterGeneric<1, 1> writer(4, 128);
    writer.SetTimeWarp(0, &inverse);
    for (size_t sample = 0; sample < 32; ++sample)
    {
        inverse.Write(2.0 * sample, 1000.0 + sample);
        if (sample == 5 || sample == 21)
        {
            writer.RecordStart(0, 0, -0.25);
        }

        writer.Write(0, static_cast<float>(sample));
        writer.AdvanceIndex();
    }

    writer.Publish();
    writer.AcceptSamples();
    const auto& markers = writer.GetAcceptedMarkers(0, 0);
    DOCTEST_CHECK(writer.m_acceptedFirstSample == 1000.0);
    DOCTEST_CHECK(markers.m_startIndices[1] == 20.75 / 4.0);
    DOCTEST_CHECK(markers.m_warpedStartIndices[1] == 41.5);
    ScopeReader reader(&writer, 0, 0, 16, 1);
    DOCTEST_CHECK(reader.RealIndex(41.5) == 20.75 / 4.0);
    DOCTEST_CHECK(reader.ReadCoordinate(41.5) == doctest::Approx(20.75));
}

DOCTEST_TEST_CASE("Warped scope: rewind reads the previous forward visit until ascent replaces it")
{
    TimeWarpInverter<128> inverse;
    ScopeWriterGeneric<1, 1> writer(1, 128);
    writer.SetTimeWarp(0, &inverse);
    for (size_t sample = 0; sample <= 10; ++sample)
    {
        inverse.Write(static_cast<double>(sample), static_cast<double>(sample));
        if (sample == 0 || sample == 5)
        {
            writer.RecordStart(0, 0);
        }

        writer.Write(0, static_cast<float>(100 + sample));
        writer.AdvanceIndex();
    }

    inverse.Write(6.0, 11.0);
    writer.Write(0, 999.0f);
    writer.AdvanceIndex();
    writer.Publish();
    writer.AcceptSamples();
    ScopeReader rewound(&writer, 0, 0, 32, 1);
    DOCTEST_CHECK(rewound.ReadCoordinate(6.0) == 106.0f);
    inverse.Write(7.0, 12.0);
    writer.Write(0, 120.0f);
    writer.AdvanceIndex();
    writer.Publish();
    writer.AcceptSamples();
    ScopeReader resumed(&writer, 0, 0, 32, 1);
    DOCTEST_CHECK(resumed.ReadCoordinate(6.0) == 999.0f);
    DOCTEST_CHECK(resumed.ReadCoordinate(5.0) == 105.0f);
}

DOCTEST_TEST_CASE("Warped scope: one rewind removes all retained retriggers after marker wrap")
{
    TimeWarpInverter<1024> inverse;
    ScopeWriterGeneric<1, 1> writer(1, 1024);
    writer.SetTimeWarp(0, &inverse);
    for (size_t sample = 0; sample < 300; ++sample)
    {
        inverse.Write(2.0 * sample, static_cast<double>(sample));
        writer.RecordEnd(0, 0);
        writer.RecordStart(0, 0);
        writer.AdvanceIndex();
    }

    writer.Publish();
    writer.AcceptSamples();
    DOCTEST_CHECK(writer.GetAcceptedMarkers(0, 0).m_oldestStartIndex == 44);
    DOCTEST_CHECK(writer.GetAcceptedMarkers(0, 0).m_startIndexIndex == 300);
    inverse.Write(80.0, 300.0);
    writer.AdvanceIndex();
    writer.Publish();
    writer.AcceptSamples();
    const auto& markers = writer.GetAcceptedMarkers(0, 0);
    DOCTEST_CHECK(markers.m_startIndexIndex == markers.m_oldestStartIndex);
    DOCTEST_CHECK_FALSE(writer.GetMarkers(0, 0).m_started);
    DOCTEST_CHECK(ScopeReader(&writer, 0, 0, 32, 1).m_empty);

    inverse.Write(81.0, 301.0);
    writer.RecordStart(0, 0);
    writer.RecordEnd(0, 0);
    writer.RecordStart(0, 0);
    writer.AdvanceIndex();
    writer.Publish();
    writer.AcceptSamples();
    const auto& retriggers = writer.GetAcceptedMarkers(0, 0);
    DOCTEST_CHECK(retriggers.m_startIndexIndex - retriggers.m_oldestStartIndex == 2);
    DOCTEST_CHECK(retriggers.m_hasEnd[44]);
    DOCTEST_CHECK_FALSE(retriggers.m_hasEnd[45]);
    inverse.Write(80.5, 302.0);
    writer.AdvanceIndex();
    DOCTEST_CHECK_FALSE(writer.GetMarkers(0, 0).m_started);
}

DOCTEST_TEST_CASE("Warped scope: concurrent publication keeps marker pairs and inverse cutoffs coherent")
{
    TimeWarpInverter<4096> inverse;
    ScopeWriterGeneric<1, 1> writer(1, 4096);
    writer.SetTimeWarp(0, &inverse);
    std::atomic<bool> finished{false};
    std::thread producer([&]
    {
        for (size_t sample = 0; sample < 65536; ++sample)
        {
            inverse.Write(0.5 * sample, 1000.0 + sample);
            if (sample % 32 == 0)
            {
                writer.RecordStart(0, 0);
            }

            if (sample % 32 == 16)
            {
                writer.RecordEnd(0, 0);
            }

            writer.Write(0, static_cast<float>(sample));
            writer.AdvanceIndex();
            if (sample % 16 == 15)
            {
                writer.Publish();
            }
        }

        finished.store(true);
    });
    bool coherent = true;
    do
    {
        writer.AcceptSamples();
        const auto& markers = writer.GetAcceptedMarkers(0, 0);
        coherent &= markers.m_startIndexIndex >= markers.m_oldestStartIndex
            && markers.m_startIndexIndex - markers.m_oldestStartIndex <= ScopeWriter::x_numStartIndices;
        for (size_t marker = markers.m_oldestStartIndex; marker < markers.m_startIndexIndex; ++marker)
        {
            size_t slot = marker % ScopeWriter::x_numStartIndices;
            coherent &= markers.m_startIndices[slot] == marker * 32.0
                && markers.m_warpedStartIndices[slot] == marker * 16.0;
            if (markers.m_hasEnd[slot])
            {
                coherent &= markers.m_endIndices[slot] == marker * 32 + 16
                    && markers.m_warpedEndIndices[slot] == marker * 16.0 + 8.0;
            }
        }

        const auto& view = writer.m_acceptedTimeWarp;
        double warped = view.m_warpedTime - 1.5;
        double real = view.GetRealTime(warped);
        coherent &= !std::isfinite(real) || (real <= view.m_realTime && real == 1000.0 + 2.0 * warped);
    }
    while (!finished.load());
    producer.join();
    writer.AcceptSamples();
    DOCTEST_CHECK(coherent);
    DOCTEST_CHECK(writer.m_acceptedIndex == 65536);
}
