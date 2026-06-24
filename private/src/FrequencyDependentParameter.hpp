#pragma once

#include "Math.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <tuple>

struct FrequencyDependentParameter
{
    static constexpr int x_numParameters = 4;
    static constexpr float m_anchors[] = { 0.0f, 2.0f, 4.0f, 6.0f };

    static float FrequencyToLinear(float frequency)
    {
        return std::log2f(frequency) + 10.0f;
    }

    struct Index
    {
        float m_interp;
        int m_index;

        Index()
            : m_interp(0.0f)
            , m_index(0)
        {
        }

        Index(float interp, int index)
            : m_interp(interp)
            , m_index(index)
        {
        }
    };

    struct Parameter
    {
        float m_parameters[x_numParameters];

        Parameter()
        {
            for (int i = 0; i < x_numParameters; ++i)
            {
                m_parameters[i] = 0.0f;
            }
        }

        Parameter(float value)
        {
            for (int i = 0; i < x_numParameters; ++i)
            {
                m_parameters[i] = value;
            }
        }

        float Process(Index index) const
        {
            double left = m_parameters[index.m_index % x_numParameters];
            double right = m_parameters[(index.m_index + 1) % x_numParameters];
            left = std::max(1e-12, left);
            right = std::max(1e-12, right);
            return static_cast<float>(left * std::pow(right / left, index.m_interp));
        }

        float ProcessLinear(Index index) const
        {
            float left = m_parameters[index.m_index % x_numParameters];
            float right = m_parameters[(index.m_index + 1) % x_numParameters];
            return left + (right - left) * index.m_interp;
        }
    };

    struct AtomicParameter
    {
        std::atomic<float> m_parameters[x_numParameters];

        AtomicParameter()
        {
            for (int i = 0; i < x_numParameters; ++i)
            {
                m_parameters[i].store(0.0f);
            }
        }

        float Process(Index index) const
        {
            return ToParameter().Process(index);
        }

        Parameter ToParameter() const
        {
            Parameter result;
            for (int i = 0; i < x_numParameters; ++i)
            {
                result.m_parameters[i] = m_parameters[i].load();
            }

            return result;
        }

        void FromParameter(const Parameter& parameter)
        {
            for (int i = 0; i < x_numParameters; ++i)
            {
                m_parameters[i].store(parameter.m_parameters[i]);
            }
        }
    };

    struct Input
    {
        Parameter m_linearFreqs;

        Input()
        {
        }
    };

    struct UIState
    {
        AtomicParameter m_linearFreqs;

        Input ToInput() const
        {
            Input result;
            result.m_linearFreqs = m_linearFreqs.ToParameter();
            return result;
        }

        void FromInput(const Input& input)
        {
            m_linearFreqs.FromParameter(input.m_linearFreqs);
        }

        Index GetIndexForFrequency(float frequency) const
        {
            return FrequencyDependentParameter::GetIndexForFrequency(frequency, ToInput());
        }

        Index GetIndexForLogFrequency(float logFrequency) const
        {
            return FrequencyDependentParameter::GetIndexForLogFrequency(logFrequency, ToInput());
        }
    };

    static Index GetIndexForFrequency(float frequency, const Input& parameterInput)
    {
        return GetIndexForLogFrequency(FrequencyToLinear(frequency), parameterInput);
    }

    static Index GetIndexForLogFrequency(float logFrequency, const Input& parameterInput)
    {
        int before = std::lower_bound(m_anchors, m_anchors + x_numParameters, logFrequency) - m_anchors;
        float linearF;
        if (before == 0)
        {
            linearF = parameterInput.m_linearFreqs.Process(Index(0.0f, 0));            
        }
        else if (before == x_numParameters)
        {
            linearF = parameterInput.m_linearFreqs.Process(Index(1.0f, x_numParameters - 2));
        }
        else
        {
            float t = (logFrequency - m_anchors[before - 1]) / (m_anchors[before] - m_anchors[before - 1]);
            Index intermediateIndex(t, before - 1);
            linearF = parameterInput.m_linearFreqs.Process(intermediateIndex);
        }

        float linearT = logFrequency * linearF;

        float segmentPosition = linearT * x_numParameters;
        float segmentFloor = std::floor(segmentPosition);
        int segmentIndex = static_cast<int>(segmentFloor);
        int numSegments = x_numParameters - 1;
        int index = ((segmentIndex % numSegments) + numSegments) % numSegments;
        return Index(segmentPosition - segmentFloor, index);
    }
};

struct ScalarParameter
{
    struct Input
    {
        Input()
        {
        }
    };

    struct Index
    {
        Index()
        {
        }
    };

    struct Parameter
    {
        float m_value;

        Parameter()
            : m_value(0.0f)
        {
        }

        Parameter(float value)
            : m_value(value)
        {
        }

        float Process(Index index)
        {
            std::ignore = index;
            return m_value;
        }
    };

    void Process(const Input& parameterInput)
    {
        std::ignore = parameterInput;
    }

    static float FrequencyToLinear(float frequency)
    {
        return frequency;
    }

    static Index GetIndexForFrequency(float frequency, const Input& parameterInput)
    {
        return GetIndexForLogFrequency(FrequencyToLinear(frequency), parameterInput);
    }

    static Index GetIndexForLogFrequency(float logFrequency, const Input& parameterInput)
    {
        std::ignore = logFrequency;
        std::ignore = parameterInput;
        return Index();
    }

};
