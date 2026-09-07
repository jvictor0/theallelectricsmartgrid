// Reachable countHigh in a trio sheaf fiber: interval vs brute-force enumeration.
//
// NOTE: the test target defines DOCTEST_CONFIG_NO_SHORT_MACRO_NAMES, hence the
// DOCTEST_-prefixed macros.

#include "doctest.h"

#include "LameJuis.hpp"

namespace
{

using MatrixSwitch = LameJuisInternal::MatrixSwitch;

void SetElements(
    LameJuisInternal::LogicOperation& operation,
    MatrixSwitch e0,
    MatrixSwitch e1,
    MatrixSwitch e2,
    MatrixSwitch e3,
    MatrixSwitch e4,
    MatrixSwitch e5)
{
    operation.m_elements[0] = e0;
    operation.m_elements[1] = e1;
    operation.m_elements[2] = e2;
    operation.m_elements[3] = e3;
    operation.m_elements[4] = e4;
    operation.m_elements[5] = e5;
    operation.SetBitVectors();
}

bool BruteCountHighReachable(
    LameJuisInternal::LogicOperation& operation,
    HarmonicSheaf::BitVector inputVector,
    HarmonicSheaf::BitVector coMuteMask,
    size_t count)
{
    uint8_t readMask = static_cast<uint8_t>(~coMuteMask.m_bits);
    for (uint8_t bits = 0; bits < HarmonicSheaf::x_numBasePoints; ++bits)
    {
        HarmonicSheaf::BitVector candidate(bits);
        if (((candidate.m_bits ^ inputVector.m_bits) & readMask) != 0)
        {
            continue;
        }

        size_t countTotal;
        size_t countHigh;
        operation.GetTotalAndHigh(candidate, &countTotal, &countHigh);
        if (countHigh == count)
        {
            return true;
        }
    }

    return false;
}

void CheckRangeMatchesBrute(
    LameJuisInternal::LogicOperation& operation,
    HarmonicSheaf::BitVector inputVector,
    HarmonicSheaf::BitVector coMuteMask)
{
    size_t countMin;
    size_t countMax;
    operation.GetReachableCountHighRange(inputVector, coMuteMask, &countMin, &countMax);
    for (size_t count = 0; count <= LameJuisInternal::x_numInputs; ++count)
    {
        bool interval = operation.CountHighReachable(inputVector, coMuteMask, count);
        bool brute = BruteCountHighReachable(operation, inputVector, coMuteMask, count);
        DOCTEST_CHECK(interval == brute);
        if (count < countMin || count > countMax)
        {
            DOCTEST_CHECK(interval == false);
        }
        else
        {
            DOCTEST_CHECK(interval == true);
        }
    }
}

}

DOCTEST_TEST_CASE("countHigh fiber: no co-mutes lights the current count only")
{
    LameJuisInternal juis;
    LameJuisInternal::LogicOperation& operation = juis.m_operations[0];
    SetElements(
        operation,
        MatrixSwitch::Normal,
        MatrixSwitch::Normal,
        MatrixSwitch::Muted,
        MatrixSwitch::Muted,
        MatrixSwitch::Muted,
        MatrixSwitch::Muted);

    HarmonicSheaf::BitVector input(0b000011);
    HarmonicSheaf::BitVector noCoMutes(0);
    size_t countMin;
    size_t countMax;
    operation.GetReachableCountHighRange(input, noCoMutes, &countMin, &countMax);
    DOCTEST_CHECK(countMin == 2);
    DOCTEST_CHECK(countMax == 2);
    CheckRangeMatchesBrute(operation, input, noCoMutes);
}

DOCTEST_TEST_CASE("countHigh fiber: one co-muted active bit lights a contiguous pair")
{
    LameJuisInternal juis;
    LameJuisInternal::LogicOperation& operation = juis.m_operations[0];
    SetElements(
        operation,
        MatrixSwitch::Normal,
        MatrixSwitch::Normal,
        MatrixSwitch::Muted,
        MatrixSwitch::Muted,
        MatrixSwitch::Muted,
        MatrixSwitch::Muted);

    // Bit 0 is high and read; bit 1 is co-muted, so countHigh is 1 or 2.
    //
    HarmonicSheaf::BitVector input(0b000001);
    HarmonicSheaf::BitVector coMuteBit1(0b000010);
    size_t countMin;
    size_t countMax;
    operation.GetReachableCountHighRange(input, coMuteBit1, &countMin, &countMax);
    DOCTEST_CHECK(countMin == 1);
    DOCTEST_CHECK(countMax == 2);
    CheckRangeMatchesBrute(operation, input, coMuteBit1);
}

DOCTEST_TEST_CASE("countHigh fiber: co-muting a muted bit does not add counts")
{
    LameJuisInternal juis;
    LameJuisInternal::LogicOperation& operation = juis.m_operations[0];
    SetElements(
        operation,
        MatrixSwitch::Normal,
        MatrixSwitch::Muted,
        MatrixSwitch::Muted,
        MatrixSwitch::Muted,
        MatrixSwitch::Muted,
        MatrixSwitch::Muted);

    HarmonicSheaf::BitVector input(0b000001);
    HarmonicSheaf::BitVector coMuteBit1(0b000010);
    size_t countMin;
    size_t countMax;
    operation.GetReachableCountHighRange(input, coMuteBit1, &countMin, &countMax);
    DOCTEST_CHECK(countMin == 1);
    DOCTEST_CHECK(countMax == 1);
    CheckRangeMatchesBrute(operation, input, coMuteBit1);
}

DOCTEST_TEST_CASE("countHigh fiber: inverted read bit contributes to the base")
{
    LameJuisInternal juis;
    LameJuisInternal::LogicOperation& operation = juis.m_operations[0];
    SetElements(
        operation,
        MatrixSwitch::Inverted,
        MatrixSwitch::Normal,
        MatrixSwitch::Muted,
        MatrixSwitch::Muted,
        MatrixSwitch::Muted,
        MatrixSwitch::Muted);

    // Bit 0 is high and inverted, so it contributes 0. Bit 1 is co-muted.
    //
    HarmonicSheaf::BitVector input(0b000001);
    HarmonicSheaf::BitVector coMuteBit1(0b000010);
    size_t countMin;
    size_t countMax;
    operation.GetReachableCountHighRange(input, coMuteBit1, &countMin, &countMax);
    DOCTEST_CHECK(countMin == 0);
    DOCTEST_CHECK(countMax == 1);
    CheckRangeMatchesBrute(operation, input, coMuteBit1);
}

DOCTEST_TEST_CASE("countHigh fiber: all active bits co-muted lights every possible count")
{
    LameJuisInternal juis;
    LameJuisInternal::LogicOperation& operation = juis.m_operations[0];
    SetElements(
        operation,
        MatrixSwitch::Normal,
        MatrixSwitch::Normal,
        MatrixSwitch::Normal,
        MatrixSwitch::Inverted,
        MatrixSwitch::Normal,
        MatrixSwitch::Normal);

    HarmonicSheaf::BitVector input(0b010101);
    HarmonicSheaf::BitVector allCoMuted(0b111111);
    size_t countMin;
    size_t countMax;
    operation.GetReachableCountHighRange(input, allCoMuted, &countMin, &countMax);
    DOCTEST_CHECK(countMin == 0);
    DOCTEST_CHECK(countMax == 6);
    CheckRangeMatchesBrute(operation, input, allCoMuted);
}
