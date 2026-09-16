#include "doctest.h"

#include "FrequencyDependentParameter.hpp"

DOCTEST_TEST_CASE("FrequencyDependentParameter visits all four lanes and interpolates back to the first")
{
    FrequencyDependentParameter::Input input;
    input.m_linearFreqs = FrequencyDependentParameter::Parameter(1.0f);
    FrequencyDependentParameter::Parameter parameter;
    parameter.m_parameters[0] = 1.0f;
    parameter.m_parameters[1] = 2.0f;
    parameter.m_parameters[2] = 4.0f;
    parameter.m_parameters[3] = 8.0f;

    struct Case
    {
        float m_position;
        float m_linear;
        float m_geometric;
    };

    const Case cases[] =
    {
        {0.0f, 1.0f, 1.0f},
        {0.25f, 2.0f, 2.0f},
        {0.5f, 4.0f, 4.0f},
        {0.75f, 8.0f, 8.0f},
        {0.875f, 4.5f, 2.828427125f},
        {1.0f, 1.0f, 1.0f},
        {-0.125f, 4.5f, 2.828427125f},
        {-0.25f, 8.0f, 8.0f},
        {-1.0f, 1.0f, 1.0f}
    };

    for (const auto& example : cases)
    {
        DOCTEST_CAPTURE(example.m_position);
        auto index = FrequencyDependentParameter::GetIndexForLogFrequency(example.m_position, input);
        DOCTEST_CHECK(parameter.ProcessLinear(index) == doctest::Approx(example.m_linear));
        DOCTEST_CHECK(parameter.Process(index) == doctest::Approx(example.m_geometric));
    }
}

DOCTEST_TEST_CASE("FrequencyDependentParameter is continuous at positive and negative segment boundaries")
{
    FrequencyDependentParameter::Input input;
    input.m_linearFreqs = FrequencyDependentParameter::Parameter(1.0f);
    FrequencyDependentParameter::Parameter parameter;
    parameter.m_parameters[0] = 1.0f;
    parameter.m_parameters[1] = 2.0f;
    parameter.m_parameters[2] = 4.0f;
    parameter.m_parameters[3] = 8.0f;

    for (int boundary = -8; boundary <= 8; ++boundary)
    {
        float position = boundary * 0.25f;
        auto left = FrequencyDependentParameter::GetIndexForLogFrequency(position - 1e-6f, input);
        auto center = FrequencyDependentParameter::GetIndexForLogFrequency(position, input);
        auto right = FrequencyDependentParameter::GetIndexForLogFrequency(position + 1e-6f, input);
        DOCTEST_CAPTURE(position);
        DOCTEST_CHECK(parameter.ProcessLinear(left) == doctest::Approx(parameter.ProcessLinear(center)).epsilon(1e-4));
        DOCTEST_CHECK(parameter.ProcessLinear(right) == doctest::Approx(parameter.ProcessLinear(center)).epsilon(1e-4));
        DOCTEST_CHECK(parameter.Process(left) == doctest::Approx(parameter.Process(center)).epsilon(1e-4));
        DOCTEST_CHECK(parameter.Process(right) == doctest::Approx(parameter.Process(center)).epsilon(1e-4));
    }
}
