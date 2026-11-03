#pragma once

#include "AdaptiveWaveTable.hpp"

template<typename Number, size_t Size>
struct MorphingWaveTableGeneric
{
    using Vector = MultiChannelNumber<Number, Size>;
    using Table = AdaptiveWaveTableGeneric<Number, 1>;
    using Evaluation = AdaptiveWaveTableGeneric<Number, Size>;

    Table* m_left[Size]{};
    Table* m_right[Size]{};

    Table* SetLeft(size_t channel, Table* left)
    {
        Table* old = m_left[channel];
        m_left[channel] = left;
        return old;
    }

    Table* SetRight(size_t channel, Table* right)
    {
        Table* old = m_right[channel];
        m_right[channel] = right;
        return old;
    }

    Table* SetLeft(Table* left)
    {
        static_assert(Size == 1, "SetLeft without a channel requires one channel");
        return SetLeft(0, left);
    }

    Table* SetRight(Table* right)
    {
        static_assert(Size == 1, "SetRight without a channel requires one channel");
        return SetRight(0, right);
    }

    Vector Evaluate(const Vector& phase, const Vector& freq, const Vector& maxFreq, const Vector& blend) const
    {
        typename Evaluation::EvalSite site;
        Evaluation::EvaluateSite(freq, maxFreq, site);
        typename Evaluation::Lookup lookup(phase);
        Vector left = Evaluation::Evaluate(lookup, site, m_left);
        Vector right = Evaluation::Evaluate(lookup, site, m_right);
        return left * (Vector::Splat(1) - blend) + right * blend;
    }

    Vector StartValue(const Vector& blend, const Vector& freq, const Vector& maxFreq) const
    {
        return Evaluate(Vector(), freq, maxFreq, blend);
    }

    Vector CenterValue(const Vector& blend, const Vector& freq, const Vector& maxFreq) const
    {
        return Evaluate(Vector::Splat(static_cast<Number>(0.5)), freq, maxFreq, blend);
    }
};

using MorphingWaveTable = MorphingWaveTableGeneric<float, 1>;
