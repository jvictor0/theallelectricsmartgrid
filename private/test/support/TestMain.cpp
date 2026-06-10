// Custom doctest main for the standalone smartgrid test binary.
//
// We own main() so that the global test environment (SampleTimer, RNG seeding,
// logger) is initialized exactly once before any test runs.

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

#include "GlobalEnv.hpp"

int main(int argc, char** argv)
{
    GlobalEnv::Init();

    doctest::Context context;
    context.applyCommandLine(argc, argv);

    int res = context.run();

    if (context.shouldExit())
    {
        return res;
    }

    return res;
}
