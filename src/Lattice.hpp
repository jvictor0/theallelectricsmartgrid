#pragma once
#include "LameJuisConstants.hpp"

namespace Lattice
{
    enum class NoteBase : int
    {
        C = 0,
        D = 1,
        E = 2,
        F = 3,
        G = 4,
        A = 5,
        B = 6
    };

    static constexpr char NoteBaseToChar(NoteBase base)
    {
        return base == NoteBase::B ? 'B' :
            base == NoteBase::A ? 'A' :
            'C' + static_cast<int>(base);
    };

    static constexpr size_t x_noteSemitones[] = {
        0,
        2,
        4,
        5,
        7,
        9,
        11
    };

    static constexpr size_t x_specificToGenericInterval[] = {
        0,
        1,
        1,
        2,
        2,
        3,
        3,
        4,
        5,
        5,
        6,
        6,
        7,
        7
    };

    struct NoteName
    {
        NoteBase m_base;
        bool m_sharp;
        uint32_t m_numSharpsOrFlats;
        
        NoteName(NoteBase base, bool sharp, uint32_t numSharpsOrFlats)
            : m_base(base)
            , m_sharp(sharp)
            , m_numSharpsOrFlats(numSharpsOrFlats)
        {
            m_buf[0] = '\0';
        }
                
        NoteName()
            : NoteName(NoteBase::C, false, 0)
        {
        }

        const char* GetNoteString()
        {
            if (m_buf[0] == '\0')
            {
                m_buf[0] = NoteBaseToChar(m_base);
                for (size_t i = 1; i < m_numSharpsOrFlats + 1; ++i)
                {
                    m_buf[i] = m_sharp ? '#' : 'b';
                }

                m_buf[m_numSharpsOrFlats + 1] = '\0';
            }

            return m_buf;
        }
        
        char m_buf[16];
    };

    struct Note
    {
        int m_pos[LameJuisConstants::x_numAccumulators];
        int m_interval[LameJuisConstants::x_numAccumulators];

        Note(int* pos, int* interval)
        {
            for (size_t i = 0; i < LameJuisConstants::x_numAccumulators; ++i)
            {
                m_pos[i] = pos[i];
                m_interval[i] = interval[i];
            }
        }
        
        NoteName ToNoteName()
        {
            using namespace LameJuisConstants;
                
            int baseInterval = 0;
            int twelveTetInterval = 0;
            for (size_t i = 0; i < x_numAccumulators; ++i)
            {
                baseInterval += m_pos[i] * x_specificToGenericInterval[m_interval[i]];
                twelveTetInterval += m_pos[i] * m_interval[i];
            }

            baseInterval %= 7;
            twelveTetInterval %= 12;
            int baseTwelveTetInterval = x_noteSemitones[baseInterval];
            int diff = (12 + twelveTetInterval - baseTwelveTetInterval) % 12;
            if (diff <= 6)
            {
                return NoteName(static_cast<NoteBase>(baseInterval), true, diff);
            }
            else
            {
                return NoteName(static_cast<NoteBase>(baseInterval), false, 12 - diff);
            }                
        }
    };
}
