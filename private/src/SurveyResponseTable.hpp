#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace dsp_survey
{
// A borrowed view of the little-endian response table. The caller owns the bytes.
//
struct ResponseTable
{
    const unsigned char* m_bytes;
    unsigned m_axes;
    unsigned m_bands;
    size_t m_frequencyOffset;
    size_t m_powerOffset;
    bool m_spectral = false;
    size_t m_mesh[3] = {};
    size_t m_offsets = 0;
    size_t m_center = 0;
    size_t m_basis = 0;
    size_t m_scores = 0;

    static double Lerp(double a, double b, double t)
    {
        if ((a <= 0 && b >= 0) || (a >= 0 && b <= 0))
        {
            return t * b + (1 - t) * a;
        }

        if (t == 1)
        {
            return b;
        }

        const double x = a + t * (b - a);
        return (t > 1) == (b > a) ? std::max(b, x) : std::min(b, x);
    }

    template<typename T>
    T Read(size_t offset) const
    {
        T value;
        std::memcpy(&value, m_bytes + offset, sizeof(value));
        return value;
    }

    ResponseTable(const void* bytes, size_t length)
        : m_bytes(static_cast<const unsigned char*>(bytes)), m_axes(0), m_bands(0),
          m_frequencyOffset(0), m_powerOffset(0)
    {
        const uint16_t endian = 1;
        m_spectral = m_bytes && length >= 4 && !std::memcmp(bytes, "DSB1", 4);
        if (!m_bytes || length < 20 || (!m_spectral && std::memcmp(bytes, "DSR1", 4) && std::memcmp(bytes, "DSR2", 4)) ||
            *reinterpret_cast<const unsigned char*>(&endian) != 1)
        {
            throw std::invalid_argument("Invalid response table header");
        }

        m_axes = Read<uint32_t>(4);
        m_bands = Read<uint32_t>(8);
        m_frequencyOffset = 20 + 24ULL * m_axes;
        m_powerOffset = m_frequencyOffset + 8ULL * m_bands;
        if (!m_axes || !m_bands || (m_spectral && m_axes != 3) || m_powerOffset > length || !std::isfinite(Read<double>(12)))
        {
            throw std::invalid_argument("Invalid table dimensions");
        }

        bool relative = std::memcmp(bytes, "DSR2", 4) == 0;
        unsigned relativeAxes = 0;
        size_t entries = m_bands;
        for (unsigned axis = 0; axis < m_axes; ++axis)
        {
            size_t offset = 20 + 24ULL * axis;
            unsigned count = Read<uint32_t>(offset);
            double low = Read<double>(offset + 8);
            double high = Read<double>(offset + 16);
            unsigned flags = Read<uint32_t>(offset + 4);
            relativeAxes += (flags & 2u) != 0;
            if (count < 2 || flags > 3 || !std::isfinite(low) || !std::isfinite(high) ||
                !(low < high) || (flags && low <= 0) || (!m_spectral && entries > (length - m_powerOffset) / 2 / count))
            {
                throw std::invalid_argument("Invalid table axis");
            }

            if (!m_spectral)
            {
                entries *= count;
            }
        }

        if (m_spectral)
        {
            ReadMesh(length);
        }

        if (relativeAxes != (relative ? 1u : 0u) || (!m_spectral && entries * 2 != length - m_powerOffset))
        {
            throw std::invalid_argument("Invalid table length");
        }

        double last = 0;
        for (unsigned band = 0; band < m_bands; ++band)
        {
            double hz = Frequency(band);
            if (!std::isfinite(hz) || hz <= last)
            {
                throw std::invalid_argument("Invalid frequency grid");
            }

            last = hz;
        }
    }

    void ReadMesh(size_t length)
    {
        size_t next = m_powerOffset;
        auto take = [&](size_t count, size_t stride)
        {
            if (count > (length - next) / stride)
            {
                throw std::invalid_argument("Truncated spectral table");
            }

            size_t start = next;
            next += count * stride;
            return start;
        };

        for (unsigned axis = 0; axis < 3; ++axis)
        {
            unsigned n = Read<uint32_t>(20 + axis * 24);
            m_mesh[axis] = take(n, sizeof(double));
            double last = -1;
            for (unsigned i = 0; i < n; ++i)
            {
                double x = Read<double>(m_mesh[axis] + i * sizeof(double));
                if (!(x > last && x <= 1) || (i == 0 && x != 0))
                {
                    throw std::invalid_argument("Invalid spectral control mesh");
                }

                last = x;
            }

            if (last != 1)
            {
                throw std::invalid_argument("Incomplete spectral control mesh");
            }
        }

        size_t slices = Read<uint32_t>(68);
        m_offsets = take(slices + 1, sizeof(uint32_t));
        unsigned modes = 0;
        for (size_t i = 0; i <= slices; ++i)
        {
            unsigned end = Read<uint32_t>(m_offsets + i * sizeof(uint32_t));
            if (end < modes || (i == 0 && end != 0))
            {
                throw std::invalid_argument("Invalid spectral basis offsets");
            }

            modes = end;
        }

        m_center = take(slices, sizeof(int16_t) * m_bands);
        m_basis = take(modes, sizeof(int16_t) * m_bands);
        m_scores = take(modes, size_t(Read<uint32_t>(20)) * Read<uint32_t>(44));
        if (next != length)
        {
            throw std::invalid_argument("Invalid spectral table length");
        }
    }

    double Frequency(unsigned band) const
    {
        return Read<double>(m_frequencyOffset + 8ULL * band);
    }

    struct FrequencyPosition
    {
        unsigned m_lower;
        double m_fraction;
    };

    void ReadSpectralKnots(const double* config, unsigned firstBand, unsigned bands,
        double* result, double* scratch) const
    {
        FrequencyPosition pos[3];
        unsigned counts[3];
        for (unsigned axis = 0; axis < 3; ++axis)
        {
            size_t header = 20 + axis * 24;
            counts[axis] = Read<uint32_t>(header);
            double low = Read<double>(header + 8);
            double high = Read<double>(header + 16);
            double x = std::clamp(config[axis], low, high);
            double u = Read<uint32_t>(header + 4) ? std::log(x / low) / std::log(high / low) : (x - low) / (high - low);
            auto knot = [&](unsigned i) { return Read<double>(m_mesh[axis] + i * sizeof(double)); };
            unsigned lo = 0;
            unsigned hi = counts[axis] - 1;
            while (hi - lo > 1)
            {
                unsigned mid = lo + (hi - lo) / 2;
                if (knot(mid) <= u)
                {
                    lo = mid;
                }
                else
                {
                    hi = mid;
                }
            }

            pos[axis] = {lo, std::clamp((u - knot(lo)) / (knot(hi) - knot(lo)), 0.0, 1.0)};
        }

        double* values[2] = {result, scratch};
        for (unsigned side = 0; side < 2; ++side)
        {
            auto* value = values[side];
            std::fill_n(value, bands, 0.0);
            size_t slice = pos[2].m_lower + side;
            unsigned begin = Read<uint32_t>(m_offsets + slice * sizeof(uint32_t));
            unsigned end = Read<uint32_t>(m_offsets + (slice + 1) * sizeof(uint32_t));
            for (size_t mode = begin; mode < end; ++mode)
            {
                size_t score = m_scores + (mode * counts[0] + pos[0].m_lower) * counts[1] + pos[1].m_lower;
                auto row = [&](size_t i)
                {
                    return Lerp(double(Read<int8_t>(i)), double(Read<int8_t>(i + 1)), pos[1].m_fraction);
                };

                double weight = Lerp(row(score), row(score + counts[1]), pos[0].m_fraction);
                size_t basis = m_basis + (mode * m_bands + firstBand) * sizeof(int16_t);
                for (unsigned band = 0; band < bands; ++band)
                {
                    value[band] += weight * Read<int16_t>(basis + band * sizeof(int16_t));
                }
            }

            size_t center = m_center + (slice * m_bands + firstBand) * sizeof(int16_t);
            for (unsigned band = 0; band < bands; ++band)
            {
                value[band] = (Read<int16_t>(center + band * sizeof(int16_t)) + value[band] / 127) / 128;
            }
        }

        for (unsigned band = 0; band < bands; ++band)
        {
            result[band] = Lerp(result[band], scratch[band], pos[2].m_fraction);
        }
    }

    double SpectralKnot(const double* config, unsigned band) const
    {
        double result;
        double scratch;
        ReadSpectralKnots(config, band, 1, &result, &scratch);
        return result;
    }

    FrequencyPosition LocateFrequency(double hz) const
    {
        if (hz <= Frequency(0))
        {
            return {0, 0};
        }

        if (hz >= Frequency(m_bands - 1))
        {
            return {m_bands - 1, 0};
        }

        unsigned lo = 0;
        unsigned hi = m_bands - 1;
        while (hi > lo + 1)
        {
            unsigned mid = lo + (hi - lo) / 2;
            if (Frequency(mid) <= hz)
            {
                lo = mid;
            }
            else
            {
                hi = mid;
            }
        }

        double fraction = std::log(hz / Frequency(lo)) / std::log(Frequency(hi) / Frequency(lo));
        return {lo, std::clamp(fraction, 0.0, 1.0)};
    }

    double Interpolate(const double* config, unsigned axis, size_t vertex, FrequencyPosition band) const
    {
        if (axis == m_axes)
        {
            size_t offset = m_powerOffset + 2 * (vertex * m_bands + band.m_lower);
            double lower = Read<int16_t>(offset) / 64.0;
            if (band.m_fraction == 0)
            {
                return lower;
            }

            return lower + band.m_fraction * (Read<int16_t>(offset + 2) / 64.0 - lower);
        }

        size_t offset = 20 + 24ULL * axis;
        unsigned count = Read<uint32_t>(offset);
        unsigned flags = Read<uint32_t>(offset + 4);
        double low = Read<double>(offset + 8);
        double high = Read<double>(offset + 16);
        double x = std::clamp(config[axis], low, high);
        double t = (flags & 1u) ? std::log(x / low) / std::log(high / low) : (x - low) / (high - low);
        t = std::clamp(t, 0.0, 1.0) * (count - 1);
        unsigned i = std::min(static_cast<unsigned>(t), count - 2);

        FrequencyPosition lower = band;
        FrequencyPosition upper = band;
        if (flags & 2u)
        {
            auto shifted = [&](unsigned knot)
            {
                double position = static_cast<double>(knot) / (count - 1);
                double logKnot = (flags & 1u) ? Lerp(std::log(low), std::log(high), position)
                                              : std::log(Lerp(low, high, position));
                double logShift = logKnot - std::log(x);
                return LocateFrequency(std::exp(std::log(Frequency(band.m_lower)) + logShift));
            };

            lower = shifted(i);
            upper = shifted(i + 1);
        }

        double a = Interpolate(config, axis + 1, vertex * count + i, lower);
        double b = Interpolate(config, axis + 1, vertex * count + i + 1, upper);
        return a + (t - i) * (b - a);
    }

    double Interpolate(const double* config, unsigned axis, size_t vertex, unsigned band) const
    {
        if (m_spectral)
        {
            return SpectralKnot(config, band);
        }

        return Interpolate(config, axis, vertex, FrequencyPosition{band, 0});
    }

    void CheckConfig(const double* config) const
    {
        for (unsigned axis = 0; axis < m_axes; ++axis)
        {
            if (!std::isfinite(config[axis]))
            {
                throw std::invalid_argument("Nonfinite control");
            }
        }
    }

    double DisplayDb(const double* config, double hz) const
    {
        CheckConfig(config);
        if (!std::isfinite(hz) || hz <= 0)
        {
            throw std::invalid_argument("Invalid frequency");
        }

        FrequencyPosition p = LocateFrequency(hz);
        double a = Interpolate(config, 0, 0, p.m_lower);
        if (p.m_fraction != 0)
        {
            a += p.m_fraction * (Interpolate(config, 0, 0, p.m_lower + 1) - a);
        }

        return std::max(Read<double>(12), a);
    }

    double DisplayDbAt(const double* knots, FrequencyPosition p) const
    {
        const double a = knots[p.m_lower];
        const double db = p.m_fraction == 0 ? a : a + p.m_fraction * (knots[p.m_lower + 1] - a);
        return std::max(Read<double>(12), db);
    }
};
}
