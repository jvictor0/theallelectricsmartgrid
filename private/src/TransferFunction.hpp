#pragma once

#include <complex>

// Shared interface for UI states that provide frequency-domain analysis.
//
struct TransferFunction
{
    virtual ~TransferFunction() = default;

    virtual float FrequencyResponse(float freq) const = 0;
    virtual std::complex<float> TransferFunctionValue(float freq) const = 0;
};
