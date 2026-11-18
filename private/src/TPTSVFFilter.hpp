#pragma once

#include "TPTSVFGeneric.hpp"
#include "TPTSVF4PoleHighPassGeneric.hpp"
#include "MorphingTPTSVFGeneric.hpp"
#include "PhaseUtils.hpp"

// Preserve the app's natural-frequency cutoff and exponential resonance knob.
// At resonance zero Q is 0.5, giving -6.02 dB at cutoff for LP and HP.
//
template<typename Number, size_t Size>
struct TPTSVFControlsGeneric
{
    using Vector = MultiChannelNumber<Number, Size>;
    static constexpr Number x_maxCutoff = static_cast<Number>(0.499f);
    static constexpr Number x_qMin = static_cast<Number>(0.5);
    static constexpr Number x_qMax = static_cast<Number>(5.5);
    PhaseUtils::ExpParamGeneric<Number, Size> m_resonance{x_qMin, x_qMax};

    static Vector ClampCutoff(const Vector& cutoff)
    {
        assert(Vector::IfLess(Vector(), cutoff, Vector::Splat(1), Vector()) == Vector::Splat(1));
        return Vector::Min(Vector::Splat(x_maxCutoff), cutoff);
    }
};

// The generated LP/HP view supplies its own composable snapshot and UI state.
// Processing and coefficient preparation stay in the generated filter.
//
template<typename Generated>
struct TPTSVFAppFilter : Generated
{
    using Vector = typename Generated::Vector;
    TPTSVFControlsGeneric<typename Generated::NumberType, Generated::x_numChannels> m_controls;

    TPTSVFAppFilter()
        : Generated{}
    {
        SetCutoffAndResonance(Vector::Splat(0.1), Vector());
    }

    void SetCutoffAndResonance(const Vector& cutoff, const Vector& resonance)
    {
        SetCutoffAndQ(cutoff, m_controls.m_resonance.Update(resonance));
    }

    void SetCutoffAndQ(const Vector& cutoff, const Vector& q)
    {
        assert(Vector::IfLess(Vector(), q, Vector::Splat(1), Vector()) == Vector::Splat(1));
        this->ComputeCoefficients({m_controls.ClampCutoff(cutoff), q});
    }

    void Process(const Vector& sample)
    {
        typename Generated::Input input{sample};
        Generated::Process(input);
    }

    Vector GetLowPass() const
    {
        return this->m_state.m_lowPass;
    }

    Vector GetHighPass() const
    {
        return this->m_state.m_highPass;
    }

    Vector GetBandPass() const
    {
        return this->m_state.m_bandPass;
    }

    void Reset()
    {
        this->m_state.m_ic1 = Vector();
        this->m_state.m_ic2 = Vector();
        this->m_state.m_lowPass = Vector();
        this->m_state.m_bandPass = Vector();
        this->m_state.m_highPass = Vector();
        this->m_state.m_notch = Vector();
        this->m_state.m_allPass = Vector();
    }
};

template<typename Number, size_t Size>
using TPTSVFLowPassFilterGeneric = TPTSVFAppFilter<TPTSVFLowPassGeneric<Number, Size>>;

template<typename Number, size_t Size>
using TPTSVFHighPassFilterGeneric = TPTSVFAppFilter<TPTSVFHighPassGeneric<Number, Size>>;

template<typename Number, size_t Size>
using TPTSVFBandPassFilterGeneric = TPTSVFAppFilter<TPTSVFBandPassGeneric<Number, Size>>;

template<typename Number, size_t Size>
struct TPTSVF4PoleHighPassFilterGeneric : GeneratedFilterView<TPTSVF4PoleHighPassGeneric<Number, Size>, &TPTSVF4PoleHighPassGeneric<Number, Size>::MagnitudeSquared>
{
    using Filter = GeneratedFilterView<TPTSVF4PoleHighPassGeneric<Number, Size>, &TPTSVF4PoleHighPassGeneric<Number, Size>::MagnitudeSquared>;
    using Vector = typename Filter::Vector;
    TPTSVFControlsGeneric<Number, Size> m_controls;

    TPTSVF4PoleHighPassFilterGeneric()
        : Filter{}
    {
        SetCutoffAndResonance(Vector::Splat(0.1), Vector());
    }

    void SetCutoffAndResonance(const Vector& cutoff, const Vector& resonance)
    {
        const auto frequency = m_controls.ClampCutoff(cutoff);
        const auto q = m_controls.m_resonance.Update(resonance);
        this->ComputeCoefficients({{frequency, q}, {frequency, q}});
    }

    void Process(const Vector& sample)
    {
        typename Filter::Input input{sample};
        Filter::Process(input);
    }

    Vector GetOutput() const
    {
        return this->m_state.m_stage2.m_highPass;
    }
};

template<typename Number, size_t Size>
struct MorphingTPTSVFFilterGeneric : GeneratedFilterView<MorphingTPTSVFGeneric<Number, Size>, &MorphingTPTSVFGeneric<Number, Size>::MagnitudeSquared>
{
    using Filter = GeneratedFilterView<MorphingTPTSVFGeneric<Number, Size>, &MorphingTPTSVFGeneric<Number, Size>::MagnitudeSquared>;
    using Vector = typename Filter::Vector;
    TPTSVFControlsGeneric<Number, Size> m_controls;

    MorphingTPTSVFFilterGeneric()
        : Filter{}
    {
        SetCutoffResonanceAndMorph(Vector::Splat(0.1), Vector(), Vector::Splat(0.5));
    }

    void SetCutoffResonanceAndMorph(const Vector& cutoff, const Vector& resonance, const Vector& morph)
    {
        this->ComputeCoefficients({{m_controls.ClampCutoff(cutoff), m_controls.m_resonance.Update(resonance)},
            Vector::Min(Vector::Splat(1), Vector::Max(Vector(), morph))});
    }

    void Process(const Vector& sample)
    {
        typename Filter::Input input{sample};
        Filter::Process(input);
    }

    Vector GetOutput() const
    {
        return this->m_state.m_output;
    }
};
