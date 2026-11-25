#include "../../private/src/SurveyResponseTable.hpp"

extern "C" bool SurveyKnots(const void* bytes, size_t size, const double* config, unsigned dims,
                            double* knots, unsigned bands)
{
    try
    {
        dsp_survey::ResponseTable table(bytes, size);
        if (table.m_axes != dims || table.m_bands != bands)
        {
            return false;
        }

        table.CheckConfig(config);
        for (unsigned i = 0; i < bands; ++i)
        {
            knots[i] = table.Interpolate(config, 0, 0, i);
        }

        return true;
    }
    catch (const std::invalid_argument&)
    {
        return false;
    }
}
