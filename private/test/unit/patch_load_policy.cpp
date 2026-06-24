#include "doctest.h"

#include "PatchLoadPolicy.hpp"

DOCTEST_TEST_CASE("PatchLoadPolicy restores faders only for standalone open loads")
{
    DOCTEST_CHECK(PatchLoadPolicy::ShouldRestoreFaders(false, false));
    DOCTEST_CHECK_FALSE(PatchLoadPolicy::ShouldRestoreFaders(true, false));
    DOCTEST_CHECK_FALSE(PatchLoadPolicy::ShouldRestoreFaders(false, true));
    DOCTEST_CHECK_FALSE(PatchLoadPolicy::ShouldRestoreFaders(true, true));
}
