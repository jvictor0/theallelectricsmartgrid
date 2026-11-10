#include "doctest.h"
#include "ScopeWriter.hpp"
#include "SmartGridOneScopeEnums.hpp"
#include "TrioUtils.hpp"
#include <cmath>
#include <type_traits>

DOCTEST_TEST_CASE("Scope LOD: vector histories preserve original coordinates through every level and wrap")
{
    ScopeWriterGeneric<3, 2> writer(1, 128);
    for (size_t sample = 0; sample < 150000; ++sample)
    {
        float ramp = static_cast<float>(sample) / 100000.0f;
        writer.Write(0, MultiChannelNumber<float, 3>(ramp, 2.0f * ramp, -ramp));
        writer.Write(1, MultiChannelNumber<float, 3>::Splat(0.25f));
        writer.AdvanceIndex();
        if (sample % 64 == 63)
        {
            writer.Publish();
            writer.AcceptSamples();
        }
    }

    writer.Publish();
    writer.AcceptSamples();
    for (size_t lod = 0; lod < ScopeWriter::x_numLevels; ++lod)
    {
        DOCTEST_CAPTURE(lod);
        double position = writer.LatestIndex(lod) - 2.5 * ScopeWriter::Stride(lod);
        float value = 0.0f;
        DOCTEST_REQUIRE(writer.ReadHistory(0, 0, position, lod, value));
        DOCTEST_CHECK(value == doctest::Approx(position / 100000.0).epsilon(0.00001));
        DOCTEST_REQUIRE(writer.ReadHistory(0, 1, position, lod, value));
        DOCTEST_CHECK(value == doctest::Approx(2.0 * position / 100000.0).epsilon(0.00001));
        DOCTEST_REQUIRE(writer.ReadHistory(0, 2, position, lod, value));
        DOCTEST_CHECK(value == doctest::Approx(-position / 100000.0).epsilon(0.00001));
        DOCTEST_REQUIRE(writer.ReadHistory(1, 2, position, lod, value));
        DOCTEST_CHECK(value == doctest::Approx(0.25f));
        DOCTEST_CHECK_FALSE(writer.ReadHistory(0, 0, writer.OldestIndex(lod) - 0.1, lod, value));
        DOCTEST_CHECK_FALSE(writer.ReadHistory(0, 0, writer.LatestIndex(lod) + 0.1, lod, value));
        DOCTEST_CHECK(writer.GetLod(writer.LatestIndex(0) - position) <= lod);
    }

    DOCTEST_CHECK(writer.GetLod(200000.0) == ScopeWriter::x_numLevels);
    DOCTEST_CHECK(writer.SampleStorageBytes() == 2 * 6 * 128 * 4 * sizeof(float));
}

DOCTEST_TEST_CASE("Scope LOD: publication excludes unfinished and delayed samples")
{
    ScopeWriterGeneric<1, 1> writer(4, 64);
    float value = 0.0f;
    for (size_t sample = 0; sample < 512; ++sample)
    {
        writer.Write(0, 0.5f);
        writer.AdvanceIndex();
    }

    DOCTEST_CHECK_FALSE(writer.ReadHistory(0, 0, 100.0, 0, value));
    DOCTEST_CHECK_FALSE(writer.ReadHistory(0, 0, 64.0, 1, value));
    writer.Publish();
    writer.AcceptSamples();
    DOCTEST_CHECK(writer.AcceptedCount(0) == 128);
    DOCTEST_CHECK(writer.AcceptedCount(1) == 32);
    DOCTEST_REQUIRE(writer.ReadHistory(0, 0, 64.5, 1, value));
    DOCTEST_CHECK(value == doctest::Approx(0.5f));
    DOCTEST_CHECK_FALSE(writer.ReadHistory(0, 0, 127.0, 1, value));
    DOCTEST_CHECK(writer.ReadDisplay(0, 0, 127.0, 1) == doctest::Approx(0.5f));
}

DOCTEST_TEST_CASE("Scope LOD: cycle transfer selects one level for both cycle portions")
{
    ScopeWriterGeneric<1, 1> writer(1, 128);
    for (size_t sample = 0; sample < 3000; ++sample)
    {
        if (sample == 1500 || sample == 2500)
        {
            writer.RecordStart(0, 0, 0.25);
        }

        writer.Write(0, static_cast<float>(sample) / 10000.0f);
        writer.AdvanceIndex();
        if (sample % 64 == 63)
        {
            writer.Publish();
            writer.AcceptSamples();
        }
    }

    writer.Publish();
    writer.AcceptSamples();
    ScopeReader reader(&writer, 0, 0, 256);
    DOCTEST_REQUIRE_FALSE(reader.m_empty);
    DOCTEST_CHECK(reader.m_lod == 2);
    DOCTEST_CHECK(reader.m_startIndex == 2500.25);
    DOCTEST_CHECK(reader.m_transferXSample == doctest::Approx(0.49875 * 256));
    for (double pixel : {0.0, 64.0, 192.0, 255.0})
    {
        double sample = pixel < reader.m_transferXSample
            ? reader.m_startIndex + pixel / reader.m_transferXSample * (reader.m_transferIndex - reader.m_startIndex)
            : reader.m_postTransferIndex + (pixel - reader.m_transferXSample) / (256.0 - reader.m_transferXSample)
                * (reader.m_startIndex - reader.m_postTransferIndex);
        sample = std::min(sample, writer.LatestIndex(reader.m_lod));
        DOCTEST_CHECK(reader.Get(pixel) == doctest::Approx(sample / 10000.0).epsilon(0.00001));
    }
}

DOCTEST_TEST_CASE("Scope LOD: logical scopes and trios occupy distinct vector entries")
{
    ScopeWriterGeneric<3, 15> writer(1, 16);
    for (size_t scope = 0; scope < 5; ++scope)
    {
        for (size_t trio = 0; trio < 3; ++trio)
        {
            float base = 100.0f * scope + 10.0f * trio;
            writer.Write(SmartGridOne::VoiceScopeIndex(scope, trio),
                MultiChannelNumber<float, 3>(base, base + 1.0f, base + 2.0f));
        }
    }

    writer.AdvanceIndex();
    writer.Publish();
    writer.AcceptSamples();
    for (size_t scope = 0; scope < 5; ++scope)
    {
        for (size_t voice = 0; voice < 9; ++voice)
        {
            float value = 0.0f;
            DOCTEST_REQUIRE(writer.ReadHistory(SmartGridOne::VoiceScopeIndex(scope, voice / 3),
                voice % 3, 0.0, 0, value));
            DOCTEST_CHECK(value == 100.0f * scope + 10.0f * (voice / 3) + voice % 3);
        }
    }
}

DOCTEST_TEST_CASE("Scope LOD: selection retains the captured history interval as acceptance advances")
{
    ScopeWriterGeneric<1, 1> writer(1, 128);
    for (size_t sample = 0; sample < 1000; ++sample)
    {
        writer.Write(0, 0.5f);
        writer.AdvanceIndex();
    }

    writer.Publish();
    writer.AcceptSamples();
    size_t published = writer.m_publishedIndex.load();
    double lookback = 99.0;
    DOCTEST_REQUIRE(writer.GetLod(lookback, published) == 0);
    for (size_t sample = 0; sample < 64; ++sample)
    {
        writer.Write(0, 0.5f);
        writer.AdvanceIndex();
    }

    writer.Publish();
    writer.AcceptSamples();
    DOCTEST_CHECK(writer.GetLod(lookback) == 0);
    DOCTEST_CHECK(writer.GetLod(lookback, published) == 1);
}

DOCTEST_TEST_CASE("Scope LOD: FFT startup fills unavailable history with zero")
{
    ScopeWriterGeneric<1, 1> writer(1, 2048);
    WindowedFFT spectrum(&writer, 0);
    spectrum.Compute(0);
    for (const auto& component : spectrum.m_dft.m_components)
    {
        DOCTEST_CHECK(std::abs(component[0]) == 0.0f);
    }

    writer.Write(0, 1.0f);
    writer.AdvanceIndex();
    writer.Publish();
    writer.AcceptSamples();
    spectrum.Compute(0);
    for (const auto& component : spectrum.m_dft.m_components)
    {
        DOCTEST_CHECK(std::isfinite(std::abs(component[0])));
    }
}

DOCTEST_TEST_CASE("Scope FFT: engine spectra preserve bin spacing and capture ultrasonic tones")
{
    constexpr size_t x_size = WindowedFFT::Transform::x_tableSize;
    constexpr size_t x_highBin = 3 * x_size / 8;
    ScopeWriterGeneric<1, 1> writer(1, x_size);
    for (size_t sample = 0; sample < x_size; ++sample)
    {
        double phase = 2.0 * M_PI * sample / x_size;
        writer.Write(0, 0.5f * std::sin(8 * phase) + 0.25f * std::sin(x_highBin * phase));
        writer.AdvanceIndex();
    }

    writer.Publish();
    writer.AcceptSamples();
    WindowedFFT spectrum(&writer, 0);
    spectrum.Compute(0);
    DOCTEST_CHECK(static_cast<double>(SampleTimer::x_sampleRate) / x_size == 46.875);
    DOCTEST_CHECK(std::abs(spectrum.m_dft.m_components[8][0]) == doctest::Approx(0.125).epsilon(0.002));
    DOCTEST_CHECK(std::abs(spectrum.m_dft.m_components[x_highBin][0]) == doctest::Approx(0.0625).epsilon(0.002));
    DOCTEST_CHECK(std::isfinite(spectrum.GetMagDb(0.5f)));
    DOCTEST_CHECK(spectrum.GetMagDb(0.6f) == spectrum.GetMagDb(0.5f));
    DOCTEST_CHECK(spectrum.GetPeakMagDb(0.374f, 0.376f) >= spectrum.GetMagDb(0.375f));
}

DOCTEST_TEST_CASE("Scope acceptance: only UI acceptance advances FIRs, bounds, and cycle markers")
{
    ScopeWriterGeneric<3, 1> writer(1, 128);
    for (size_t sample = 0; sample < 48; ++sample)
    {
        if (sample % 24 == 0)
        {
            writer.RecordStart(0, 0);
        }

        writer.Write(0, TrioFloat::Splat(static_cast<float>(sample)));
        writer.AdvanceIndex();
    }

    writer.Publish();
    DOCTEST_CHECK(writer.m_publishedIndex.load() == 48);
    DOCTEST_CHECK(writer.m_acceptedIndex == 0);
    for (auto& filter : writer.m_storage[0].m_downsamplers)
    {
        DOCTEST_CHECK(filter.m_index == 0);
        DOCTEST_CHECK(filter.m_phase == 0);
        DOCTEST_CHECK(filter.GetOutput()[0] == 0.0f);
    }

    float value = 0.0f;
    DOCTEST_CHECK_FALSE(writer.ReadHistory(0, 0, 10.0, 0, value));
    writer.AcceptSamples(24);
    DOCTEST_CHECK(writer.m_acceptedIndex == 24);
    DOCTEST_CHECK(writer.AcceptedCount(1) == 6);
    DOCTEST_REQUIRE(writer.ReadHistory(0, 0, 15.0, 1, value));
    DOCTEST_CHECK(value == doctest::Approx(15.0f));
    DOCTEST_CHECK_FALSE(writer.ReadHistory(0, 0, 24.0, 0, value));
    ScopeReader first(&writer, 0, 0, 256);
    DOCTEST_CHECK(first.m_startIndex == 0.0);
    DOCTEST_CHECK(first.m_transferIndex == 23.0);
    auto filterIndex = writer.m_storage[0].m_downsamplers[0].m_index;
    writer.AcceptSamples(24);
    DOCTEST_CHECK(writer.m_storage[0].m_downsamplers[0].m_index == filterIndex);

    writer.RecordEnd(0, 0);
    writer.Publish();
    ScopeReader stillFirst(&writer, 0, 0, 256);
    DOCTEST_CHECK(stillFirst.m_startIndex == first.m_startIndex);
    DOCTEST_CHECK(stillFirst.m_transferIndex == first.m_transferIndex);
    writer.AcceptSamples();
    ScopeReader second(&writer, 0, 0, 256);
    DOCTEST_CHECK(second.m_startIndex == 24.0);
    DOCTEST_CHECK(second.m_transferIndex == 47.0);
}

DOCTEST_TEST_CASE("Scope acceptance: fractional starts beyond the captured last sample wait for the next frame")
{
    ScopeWriterGeneric<3, 1> writer(1, 128);
    for (size_t sample = 0; sample < 24; ++sample)
    {
        if (sample % 12 == 0)
        {
            writer.RecordStart(0, 0);
        }

        if (sample == 23)
        {
            writer.RecordStart(0, 1);
        }

        writer.Write(0, TrioFloat::Splat(static_cast<float>(sample)));
        writer.AdvanceIndex();
    }

    writer.Publish();
    size_t capturedEnd = writer.m_publishedIndex.load();

    writer.RecordStart(0, 0, -0.25);
    writer.Write(0, TrioFloat::Splat(24.0f));
    writer.AdvanceIndex();
    writer.Publish();

    writer.AcceptSamples(capturedEnd);
    ScopeReader previous(&writer, 0, 0, 256);
    DOCTEST_REQUIRE_FALSE(previous.m_empty);
    DOCTEST_CHECK(previous.m_startIndex == 12.0);
    DOCTEST_CHECK(previous.m_transferIndex == 23.0);
    DOCTEST_CHECK(previous.Get(0) == doctest::Approx(12.0f));

    ScopeReader exactBoundary(&writer, 1, 0, 256);
    DOCTEST_REQUIRE_FALSE(exactBoundary.m_empty);
    DOCTEST_CHECK(exactBoundary.m_startIndex == 23.0);

    writer.AcceptSamples();
    ScopeReader next(&writer, 0, 0, 256);
    DOCTEST_REQUIRE_FALSE(next.m_empty);
    DOCTEST_CHECK(next.m_startIndex == 23.75);
    DOCTEST_CHECK(next.m_transferIndex == 24.0);
    DOCTEST_CHECK(next.Get(0) == doctest::Approx(23.75f));
}

DOCTEST_TEST_CASE("Scope acceptance: irregular UI batches match incremental filtering through all levels")
{
    ScopeWriterGeneric<3, 2> incremental(1, 128);
    ScopeWriterGeneric<3, 2> batched(1, 128);
    for (size_t sample = 0; sample < 8192; ++sample)
    {
        TrioFloat value(std::sin(sample * 0.03), static_cast<float>(sample) / 8192.0f,
            sample % 43 == 0 ? 1.0f : 0.0f);
        for (auto* writer : {&incremental, &batched})
        {
            writer->Write(0, value);
            if (sample % 3 != 0)
            {
                writer->Write(1, value * 0.5f);
            }

            writer->AdvanceIndex();
            writer->Publish();
        }

        incremental.AcceptSamples();
        if (sample % 37 == 36)
        {
            batched.AcceptSamples();
        }
    }

    batched.AcceptSamples();
    for (size_t lod = 0; lod < ScopeWriter::x_numLevels; ++lod)
    {
        DOCTEST_CHECK(batched.AcceptedCount(lod) == incremental.AcceptedCount(lod));
        for (size_t scope = 0; scope < 2; ++scope)
        {
            for (size_t index = batched.OldestStoredIndex(lod); index < batched.AcceptedCount(lod); ++index)
            {
                for (size_t lane = 0; lane < 3; ++lane)
                {
                    DOCTEST_CHECK(batched.ReadStored(scope, lane, index, lod)
                        == incremental.ReadStored(scope, lane, index, lod));
                }
            }
        }
    }
}

DOCTEST_TEST_CASE_TEMPLATE("Scope FFT: SIMD transforms match independent scalar spectra in every lane", Channels,
    std::integral_constant<size_t, 1>, std::integral_constant<size_t, 3>, std::integral_constant<size_t, 4>)
{
    constexpr size_t x_size = Channels::value;
    using FFT = WindowedFFTGeneric<x_size>;
    constexpr size_t x_samples = FFT::Transform::x_tableSize;
    ScopeWriterGeneric<x_size, 1> writer(1, x_samples);
    typename FFT::Table input;
    for (size_t sample = 0; sample < x_samples; ++sample)
    {
        typename FFT::Vector value;
        for (size_t lane = 0; lane < x_size; ++lane)
        {
            value[lane] = std::sin(2.0 * M_PI * (7 + 503 * lane) * sample / x_samples)
                + 0.2f * std::cos(sample * (lane + 1) * 0.713)
                + (sample % (17 + lane) == 0 ? 0.125f : 0.0f);
        }

        input.m_table[sample] = value;
        writer.Write(0, value);
        writer.AdvanceIndex();
    }

    writer.Publish();
    writer.AcceptSamples();
    FFT spectrum(&writer, 0);
    spectrum.Compute();
    for (size_t lane = 0; lane < x_size; ++lane)
    {
        WindowedFFT::Table windowed;
        for (size_t sample = 0; sample < x_samples; ++sample)
        {
            float phase = 2.0f * static_cast<float>(M_PI) * sample / (x_samples - 1);
            windowed.m_table[sample] = input.m_table[sample][lane] * (0.5f - 0.5f * std::cos(phase));
        }

        WindowedFFT::Transform reference;
        reference.Transform(windowed);
        for (size_t bin = 0; bin < FFT::Transform::x_maxComponents; ++bin)
        {
            DOCTEST_CHECK(std::abs(spectrum.m_dft.m_components[bin][lane] - reference.m_components[bin][0]) < 0.000002f);
        }
    }

    auto previous = spectrum.m_dft.m_components[7];
    writer.Write(0, typename FFT::Vector());
    writer.AdvanceIndex();
    writer.Publish();
    spectrum.Compute();
    for (size_t lane = 0; lane < x_size; ++lane)
    {
        DOCTEST_CHECK(std::abs(spectrum.m_dft.m_components[7][lane] - previous[lane]) < 0.000002f);
    }
}
