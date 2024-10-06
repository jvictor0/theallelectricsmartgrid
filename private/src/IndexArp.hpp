#pragma once

struct IndexArp
{
    static constexpr size_t x_rhythmLength = 8;
    
    int m_preIndex;
    int m_motiveIndex;
    int m_rhythmIndex;
    int m_lastMotiveLength;
    bool m_resetNextClock;

    int m_size;
    int m_index;

    IndexArp()
        : m_preIndex(0)
        , m_motiveIndex(0)
        , m_rhythmIndex(0)
        , m_lastMotiveLength(0)
        , m_resetNextClock(false)
        , m_size(1)
        , m_index(0)
    {
    }
    
    struct Input
    {
        bool m_clock;
        bool m_reset;
        int m_offset;
        int m_intervalSubOne;
        bool m_cycle;
        bool m_rhythm[x_rhythmLength];
        int m_rhythmLength;
        bool m_up;
        int m_motiveInterval;

        Input()
            : m_clock(false)
            , m_reset(true)
            , m_offset(0)
            , m_intervalSubOne(0)
            , m_cycle(false)
            , m_rhythmLength(x_rhythmLength)
            , m_up(true)
            , m_motiveInterval(0)
        {
            for (size_t i = 0; i < x_rhythmLength; ++i)
            {
                m_rhythm[i] = true;
            }
        }                         
    };

    Input m_state;

    void Process(Input& input)
    {
        if (input.m_reset)
        {
            m_resetNextClock = true;
        }

        if (input.m_clock)
        {
            if (m_resetNextClock)
            {
                Reset(input);
            }
            else
            {
                ++m_rhythmIndex;
                if (input.m_rhythm[m_rhythmIndex % input.m_rhythmLength])
                {
                    ++m_preIndex;
                    if (m_rhythmIndex >= input.m_rhythmLength)
                    {
                        m_rhythmIndex %= input.m_rhythmLength;
                        ++m_motiveIndex;
                        m_lastMotiveLength = m_preIndex;
                        m_preIndex = 0;
                    }
                }

                //INFO("preIndex %d rhythmIndex %d motiveIndex %d", m_preIndex, m_rhythmIndex,m_motiveIndex);
            }

            m_state = input;
        }
    }

    void Reset(Input& input)
    {
        m_preIndex = 0;
        m_rhythmIndex = 0;
        m_motiveIndex = 0;
        m_resetNextClock = false;
    }

    void Get(int size, int* ix, int* octave)
    {
        int offset = m_state.m_offset;
        if (!m_state.m_up)
        {
            offset = size - offset;
        }

        int index = m_preIndex * (m_state.m_intervalSubOne + 1);
        
        if (m_state.m_cycle)
        {
            offset += m_state.m_motiveInterval * (m_motiveIndex / 2);
            if (m_motiveIndex % 2 == 1)
            {
                index = m_lastMotiveLength - index;
            }
        }
        else
        {
            offset += m_state.m_motiveInterval * m_motiveIndex;
        }

        if (!m_state.m_up)
        {
            index = size - index;
        }

        offset %= size;
        while (index < -size)
        {
            index += size;
        }

        if (2 * size < index)
        {
            index %= 2 * size;
        }
        
        int preResult = offset + index;
        *octave = preResult / size;
        while (preResult < 0)
        {
            preResult += size;
            *octave -= 1;
        }

        *ix = preResult % size;

        m_size = size;
        m_index = *ix;

        if (*ix < 0 || *ix >= size)
        {
            INFO("preix %d motiv %d ix %d off %d ix %d oct %d size %d",
                 m_preIndex, m_motiveIndex, index, offset, *ix, *octave, size);
            assert(false);
        }
    }
};

struct NonagonIndexArp
{
    static constexpr size_t x_numClocks = 7;
    static constexpr size_t x_numTrios = 3;
    static constexpr size_t x_voicesPerTrio = 3;
    static constexpr size_t x_numVoices = 9;

    IndexArp m_arp[x_numVoices];
    IndexArp m_committedArp[x_numVoices];
    
    struct Input
    {
        IndexArp::Input m_input[x_numVoices];
        bool m_clocks[x_numClocks];
        int m_clockSelect[x_numTrios];
        int m_resetSelect[x_numTrios];
        bool m_externalReset;
        int m_commit[x_numVoices];

        Input()
        {
            for (size_t i = 0; i < x_numClocks; ++i)
            {
                m_clocks[i] = false;
            }

            for (size_t i = 0; i < x_numTrios; ++i)
            {
                m_clockSelect[i] = 0;
                m_resetSelect[i] = -1;
            }

            for (size_t i = 0; i < x_numVoices; ++i)
            {
                m_commit[i] = i;
            }

            m_externalReset = true;
        }

        void SetClocks()
        {
            for (size_t i = 0; i < x_numVoices; ++i)
            {
                size_t j = i / x_voicesPerTrio;
                m_input[i].m_reset = m_externalReset || (m_resetSelect[j] >= 0 && m_clocks[m_resetSelect[j]]);
                m_input[i].m_clock = m_clockSelect[j] >= 0 && m_clocks[m_clockSelect[j]];
            }

            m_externalReset = false;
        }
    };

    void Process(Input& input)
    {
        input.SetClocks();
        for (size_t i = 0; i < x_numVoices; ++i)
        {
            m_arp[i].Process(input.m_input[i]);
        }

        for (size_t i = 0; i < x_numVoices; ++i)
        {
            if (input.m_commit[i] != -1)
            {
                m_committedArp[i] = m_arp[input.m_commit[i]];
            }
        }
    }
};
