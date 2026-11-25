#include "ResponsesCase.hpp"
#include <iostream>

int main(int argc, char** argv)
{
    return RunResponses(argc, argv, SMARTGRID_LADDER_RESPONSE, std::cout, std::cerr);
}
