// Regression coverage for a saved patch that drove PartialMachine non-finite.
//

#include "doctest.h"

#include <cmath>
#include <fstream>
#include <sstream>
#include <string>

#include "../support/SynthRig.hpp"

namespace
{

std::string ReadFixture(const char* relativePath)
{
    std::ifstream file(std::string(SMARTGRID_REPO_ROOT) + "/" + relativePath);
    std::ostringstream out;
    out << file.rdbuf();
    return out.str();
}

} // namespace

DOCTEST_TEST_CASE("PartialMachine: espace etale patch remains finite after load")
{
    std::string patch = ReadFixture("private/test/fixtures_espace_etale_2026-06-24T10-40-13.json");
    DOCTEST_REQUIRE_FALSE(patch.empty());

    synthrig::SynthRig rig;
    rig.RunFrames(2);
    DOCTEST_REQUIRE(rig.LoadPatch(patch));

    rig.ClearNaN();
    rig.ClearOutput();
    rig.RunSeconds(4.0);

    const auto& output = rig.Output();
    for (std::size_t i = 0; i < output.size(); ++i)
    {
        const float* values = &output[i].m_quad[0];
        for (int channel = 0; channel < 7; ++channel)
        {
            if (!std::isfinite(values[channel]))
            {
                DOCTEST_INFO("first non-finite output sample=" << i);
                DOCTEST_INFO("first non-finite channel=" << channel);
                DOCTEST_INFO("first non-finite value=" << values[channel]);
                DOCTEST_FAIL_CHECK("patch produced non-finite output");
                return;
            }
        }
    }

    DOCTEST_CHECK_FALSE(rig.SawNaN());
}
