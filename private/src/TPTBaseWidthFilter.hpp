#pragma once

#include "TPTBaseWidthGeneric.hpp"
#include "SampleTimer.hpp"

// App controls retain their base/width mapping and cutoff ceiling. The Lean
// filter owns coefficient preparation, processing and response snapshots.
//
struct QuadTPTBaseWidthFilter : TPTBaseWidthGeneric<float, 4>
{
    using Filter = TPTBaseWidthGeneric<float, 4>;
    static constexpr double x_minBaseHz = SampleTimer::CycPerHostSampToHz(1.0 / 2048.0);
    static constexpr double x_maxBaseHz = SampleTimer::CycPerHostSampToHz(0.5);
    static constexpr float x_maxCutoff = 0.499f;

    QuadTPTBaseWidthFilter()
        : Filter{}
    {
    }

    void SetBaseWidth(const Vector& base, const Vector& width)
    {
        const auto highPass = Vector::Min(Vector::Splat(x_maxCutoff), base);
        const auto lowPass = Vector::Min(Vector::Splat(x_maxCutoff), base * width);
        assert(Vector::IfLess(Vector(), highPass, Vector::Splat(1), Vector()) == Vector::Splat(1));
        assert(Vector::IfLess(Vector(), lowPass, Vector::Splat(1), Vector()) == Vector::Splat(1));
        ComputeCoefficients({{highPass}, {lowPass}});
    }

    Vector Process(const Vector& sample)
    {
        Input input{sample};
        Filter::Process(input);
        return m_state.m_lowPass.m_y;
    }
};
