// sys_json_fixture.cpp — round-trip the new JSON library against real on-disk
// files: a large real-world JSON document (the parser/serializer stress test)
// and a nonagon patch written to and read back from disk.
//
// Covers task 8.1 (existing on-disk files load and re-emit equivalently).

#include "doctest.h"

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>

#include "Json.hpp"
#include "../support/SynthRig.hpp"

#ifndef SMARTGRID_REPO_ROOT
#define SMARTGRID_REPO_ROOT "."
#endif

namespace
{

std::string ReadFile(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
    {
        return std::string();
    }
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

bool WriteFile(const std::string& path, const std::string& content)
{
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f)
    {
        return false;
    }
    f << content;
    return f.good();
}

} // namespace

// ---------------------------------------------------------------------------
// A large real-world JSON file parses and re-emits stably (parse -> dump ->
// parse -> dump is byte-identical).
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("sys_json_fixture: large on-disk JSON round-trips byte-stably")
{
    const std::string path = std::string(SMARTGRID_REPO_ROOT) + "/patches/WRLD.BLDR.json";
    const std::string text = ReadFile(path);
    if (text.empty())
    {
        DOCTEST_WARN_MESSAGE(false, "fixture not found, skipping: " << path);
        return;
    }

    JsonArena a(64u * 1024u * 1024u);
    JSON root = a.Loads(text.c_str());
    DOCTEST_REQUIRE_FALSE(root.IsNull());
    DOCTEST_REQUIRE_FALSE(a.Failed());

    char* dumped = root.Dumps(0);
    DOCTEST_REQUIRE(dumped != nullptr);
    std::string d1(dumped);
    free(dumped);

    JsonArena b(64u * 1024u * 1024u);
    JSON root2 = b.Loads(d1.c_str());
    DOCTEST_REQUIRE_FALSE(root2.IsNull());

    char* dumped2 = root2.Dumps(0);
    DOCTEST_REQUIRE(dumped2 != nullptr);
    std::string d2(dumped2);
    free(dumped2);

    DOCTEST_CHECK(d1 == d2);
}

// ---------------------------------------------------------------------------
// A nonagon patch survives an actual disk write/read cycle: the file bytes
// match the in-memory save, and re-loading restores a NaN-clean system.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("sys_json_fixture: nonagon patch survives a disk write/read cycle")
{
    synthrig::SynthRig rig;
    rig.RunFrames(2);

    auto encoders = std::vector<std::pair<int,int>>();
    for (int x = 0; x < 4; ++x)
    {
        for (int y = 0; y < 4; ++y)
        {
            if (rig.EncoderConnected(x, y))
            {
                rig.SetEncoder(x, y, 0.2f + 0.03f * (x + y));
                encoders.emplace_back(x, y);
            }
        }
    }
    rig.RunFrames(8);

    const std::string jsonA = rig.SavePatch();
    DOCTEST_REQUIRE_FALSE(jsonA.empty());

    const std::string tmp = std::string(SMARTGRID_REPO_ROOT) + "/private/test/build/_roundtrip_patch.json";
    DOCTEST_REQUIRE(WriteFile(tmp, jsonA));

    const std::string back = ReadFile(tmp);
    DOCTEST_CHECK(back == jsonA);

    DOCTEST_CHECK(rig.LoadPatch(back));
    rig.RunFrames(8);
    DOCTEST_CHECK_FALSE(rig.SawNaN());

    std::remove(tmp.c_str());
}
