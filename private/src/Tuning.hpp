#pragma once

#include "SampleTimer.hpp"
#include <cmath>

struct Tuning
{
    static constexpr float x_middleCHz = 261.6256f;
    static constexpr double x_nyquistHz = SampleTimer::x_hostSampleRate / 2.0;
    static constexpr double x_minMasterTuneHz = x_nyquistHz / 128.0;
    static constexpr double x_maxMasterTuneHz = x_nyquistHz / 64.0;
    static constexpr double x_minTempoHz = x_nyquistHz / 2097152.0;
    static constexpr double x_maxTempoHz = x_nyquistHz / 8192.0;
    inline static const float x_defaultMasterTune = static_cast<float>(
        std::log2(x_middleCHz / x_minMasterTuneHz));

    static_assert(x_minMasterTuneHz < x_middleCHz && x_middleCHz < x_maxMasterTuneHz,
        "Master tuning must bracket middle C with adjacent Nyquist suboctaves");
};
