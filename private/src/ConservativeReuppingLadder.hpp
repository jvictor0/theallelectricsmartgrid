#pragma once

#include "ConservativeReuppingLadderCoreGeneric.hpp"

// App controls around the generated fixed-feedback core. The cached response
// describes its small-signal behavior at the current cutoff, resonance and drive.
//
template<typename Number, size_t Size>
struct ConservativeReuppingLadderGeneric : GeneratedFilterView<ConservativeReuppingLadderCoreGeneric<Number, Size>,
    &ConservativeReuppingLadderCoreGeneric<Number, Size>::MagnitudeSquared>
{
    using Core = ConservativeReuppingLadderCoreGeneric<Number, Size>;
    using Base = GeneratedFilterView<Core, &Core::MagnitudeSquared>;
    using Vector = typename Core::Vector;
    static constexpr Number x_maxCutoff = static_cast<Number>(0.5);
    static constexpr Number x_halfPowerRatio = static_cast<Number>(0.43497944204608224);
    typename Core::LogicalSpec m_spec{};

    ConservativeReuppingLadderGeneric()
        : Base{}
    {
        SetCutoffResonanceAndGain(Vector::Splat(static_cast<Number>(0.1)), Vector(),
            Vector::Splat(static_cast<Number>(0.5)));
    }

    void SetCutoffResonanceAndGain(const Vector& cutoff, const Vector& resonance, const Vector& gain)
    {
        assert(Vector::IfLess(Vector(), cutoff, Vector::Splat(1), Vector()) == Vector::Splat(1));
        assert(Vector::IfLess(Vector(), gain, Vector::Splat(1), Vector()) == Vector::Splat(1));
        assert(Vector::Max(Vector(), Vector::Min(Vector::Splat(1), resonance)) == resonance);
        const auto clamped = Vector::Min(Vector::Splat(x_maxCutoff), cutoff);
        if (clamped != m_spec.m_cutoff || resonance != m_spec.m_resonance || gain != m_spec.m_gain)
        {
            m_spec = {clamped, resonance, gain};
            this->ComputeCoefficients(m_spec);
        }
    }

    void SetCutoffAndResonance(const Vector& cutoff, const Vector& resonance)
    {
        SetCutoffResonanceAndGain(cutoff, resonance, m_spec.m_gain);
    }

    void SetSaturationGain(const Vector& gain)
    {
        SetCutoffResonanceAndGain(m_spec.m_cutoff, m_spec.m_resonance, gain);
    }

    Vector Process(const Vector& sample)
    {
        typename Core::Input input{sample};
        Core::Process(input);
        return this->m_state.m_output;
    }
};

using ConservativeReuppingLadder = ConservativeReuppingLadderGeneric<float, 1>;
using TrioConservativeReuppingLadder = ConservativeReuppingLadderGeneric<float, 3>;
