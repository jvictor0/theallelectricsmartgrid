#include "TimeWarpInverter.hpp"
#include "reference/LandedTimeWarpInverter.hpp"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

namespace
{
double Decode(uint64_t bits)
{
    double value;
    static_assert(sizeof(value) == sizeof(bits));
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::string Encode(double value)
{
    if (std::isnan(value))
    {
        return "missing";
    }

    uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return std::to_string(bits);
}

template<size_t Capacity>
int Compare(double resolution)
{
    TimeWarpInverter<Capacity> generated(resolution);
    LandedTimeWarpInverter<Capacity> reference(resolution);
    char operation;
    uint64_t a;
    uint64_t b;
    size_t row = 0;
    while (std::cin >> operation >> a >> b)
    {
        ++row;
        const double phase = Decode(a);
        const double time = Decode(b);
        if (operation == 'W')
        {
            generated.Write(phase, time);
            reference.Write(phase, time);
            if (generated.m_oldestWarpedTime != reference.m_oldestWarpedTime
                || generated.m_islandReassignments != reference.m_islandReassignments)
            {
                std::cerr << "Writer state mismatch at row " << row << '\n';
                return 1;
            }
        }
        else if (operation == 'R')
        {
            const auto actual = Encode(generated.ReadInverse(phase, time));
            const auto expected = Encode(reference.ReadInverse(phase, time));
            if (actual != expected)
            {
                std::cerr << "C++ lookup mismatch at row " << row << '\n';
                return 1;
            }

            std::cout << actual << '\n';
        }
        else
        {
            return 1;
        }
    }

    return std::cin.eof() ? 0 : 1;
}
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        return 1;
    }

    const double resolution = Decode(std::stoull(argv[2]));
    switch (std::stoul(argv[1]))
    {
        case 1: return Compare<1>(resolution);
        case 4: return Compare<4>(resolution);
        case 37: return Compare<37>(resolution);
        case 1024: return Compare<1024>(resolution);
    }

    return 1;
}
