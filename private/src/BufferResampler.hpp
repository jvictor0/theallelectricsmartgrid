#pragma once

#include "ButterworthFilter.hpp"

#include <cmath>
#include <cstddef>
#include <cstring>
#include <memory>

// Offline mono float resampling. No std::vector: callers pass pointers and capacities.
// Each of input and output is bounded by x_maxBufferBytes of float data (sixteen MiB per side).
//
struct BufferResampler
{
    static constexpr size_t x_maxBufferBytes = size_t{16} * size_t{1024} * size_t{1024};
    static constexpr size_t x_maxSampleFrames = x_maxBufferBytes / sizeof(float);

    static constexpr double x_rateMatchEpsilon = 1.0e-3;

    // Output length for ResampleToRate, or 0 if parameters are invalid.
    //
    static size_t OutputFrameCount(size_t sourceCount, double sourceRate, double targetRate)
    {
        if (sourceCount == 0 || sourceRate <= 0.0 || targetRate <= 0.0)
        {
            return 0;
        }

        const double rel = std::abs(sourceRate - targetRate) / targetRate;
        if (rel < x_rateMatchEpsilon)
        {
            return sourceCount;
        }

        const double outLen = static_cast<double>(sourceCount) * targetRate / sourceRate;
        return std::max<size_t>(1, static_cast<size_t>(std::lround(outLen)));
    }

    // Lowpass at ~0.45 * target Nyquist expressed in source-rate cycles per sample.
    //
    static void AntiAliasLowpassInPlace(float* samples, size_t sampleCount, double sourceRate, double targetRate)
    {
        if (sampleCount == 0)
        {
            return;
        }

        if (sourceRate <= targetRate * (1.0 + x_rateMatchEpsilon))
        {
            return;
        }

        float cutoffCycles = 0.45f * static_cast<float>(targetRate / sourceRate);
        if (cutoffCycles < ButterworthFilter::x_minCyclesPerSample)
        {
            cutoffCycles = ButterworthFilter::x_minCyclesPerSample;
        }
        else if (ButterworthFilter::x_maxCyclesPerSample < cutoffCycles)
        {
            cutoffCycles = ButterworthFilter::x_maxCyclesPerSample;
        }

        ButterworthFilter filter;
        filter.SetCyclesPerSample(cutoffCycles);
        filter.Reset();

        for (size_t i = 0; i < sampleCount; ++i)
        {
            samples[i] = filter.Process(samples[i]);
        }
    }

    // Writes resampled audio into out. sourceRate and targetRate are in Hz.
    // outCapacity must be at least OutputFrameCount(sourceCount, sourceRate, targetRate).
    //
    static bool ResampleToRate(
        const float* source,
        size_t sourceCount,
        double sourceRate,
        double targetRate,
        float* out,
        size_t outCapacity,
        size_t* outCountOut)
    {
        if (outCountOut)
        {
            *outCountOut = 0;
        }

        if (!source || !out || !outCountOut)
        {
            return false;
        }

        if (sourceCount == 0 || sourceRate <= 0.0 || targetRate <= 0.0)
        {
            return false;
        }

        if (x_maxSampleFrames < sourceCount)
        {
            return false;
        }

        const double rel = std::abs(sourceRate - targetRate) / targetRate;
        if (rel < x_rateMatchEpsilon)
        {
            if (outCapacity < sourceCount)
            {
                return false;
            }

            std::memcpy(out, source, sourceCount * sizeof(float));
            *outCountOut = sourceCount;
            return true;
        }

        const size_t outCount = OutputFrameCount(sourceCount, sourceRate, targetRate);
        if (outCount == 0 || outCapacity < outCount || x_maxSampleFrames < outCount)
        {
            return false;
        }

        const float* readData = source;
        std::unique_ptr<float[]> work;
        if (sourceRate > targetRate * (1.0 + x_rateMatchEpsilon))
        {
            work.reset(new float[sourceCount]);
            std::memcpy(work.get(), source, sourceCount * sizeof(float));
            AntiAliasLowpassInPlace(work.get(), sourceCount, sourceRate, targetRate);
            readData = work.get();
        }

        if (sourceCount == 1)
        {
            for (size_t j = 0; j < outCount; ++j)
            {
                out[j] = readData[0];
            }

            *outCountOut = outCount;
            return true;
        }

        const double scale = sourceRate / targetRate;
        const double maxIndex = static_cast<double>(sourceCount - 1);

        for (size_t j = 0; j < outCount; ++j)
        {
            const double srcIndex = static_cast<double>(j) * scale;

            if (srcIndex <= 0.0)
            {
                out[j] = readData[0];
                continue;
            }

            if (maxIndex <= srcIndex)
            {
                out[j] = readData[sourceCount - 1];
                continue;
            }

            const size_t i0 = static_cast<size_t>(std::floor(srcIndex));
            const size_t i1 = i0 + 1;
            const float alpha = static_cast<float>(srcIndex - std::floor(srcIndex));
            const float y0 = readData[i0];
            const float y1 = readData[i1];
            out[j] = y0 + alpha * (y1 - y0);
        }

        *outCountOut = outCount;
        return true;
    }
};
