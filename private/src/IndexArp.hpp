#pragma once

struct IndexArp
{
    static constexpr size_t x_rhythmLength = 8;
    
    int m_index;
    int m_motiveIndex;
    int m_rhythmIndex;
    int m_lastMotiveLength;
    bool m_resetNextClock;
    float m_output;

    IndexArp()
        : m_index(0)
        , m_motiveIndex(0)
        , m_rhythmIndex(0)
        , m_lastMotiveLength(0)
        , m_resetNextClock(false)
        , m_output(0.0f)
    {
    }
    
    struct Input
    {
        bool m_clock;
        bool m_reset;

        float m_offset;
        float m_interval;
        float m_min;
        float m_max;
        bool m_invert;
        bool m_retro;
        bool m_cycle;
        float m_pageInterval;
        bool m_rhythm[x_rhythmLength];
        int m_rhythmLength;

        Input()
            : m_clock(false)
            , m_reset(true)
            , m_offset(0)
            , m_interval(0)
            , m_min(0)
            , m_max(0)
            , m_invert(false)
            , m_retro(false)
            , m_cycle(false)
            , m_pageInterval(0)
            , m_rhythmLength(x_rhythmLength)
        {
            for (size_t i = 0; i < x_rhythmLength; ++i)
            {
                m_rhythm[i] = true;
            }
        }      

        float GetOutput(int index, int pageIndex)
        {
            index = GetPhysicalIndex(index);
            float result = m_offset + index * m_interval + pageIndex * m_pageInterval;
            if (m_cycle)
            {
                result = result - 2 * std::floor(result);
                if (result > 1)
                {
                    result = 2 - result;
                }
            }
            else
            {
                result = result - std::floor(result);
            }

            if (m_invert)
            {
                result = 1 - result;
            }

            result = m_min + result * (m_max - m_min);
            return result;
        }                 

        int GetPhysicalIndex(int index)
        {
            if (m_retro)
            {
                int numNotes = NumNotes();
                return numNotes - index;
            }
            else
            {
                return index;
            }
        }

        int NumNotes() const
        {
            int result = 0;
            for (size_t i = 0; i < x_rhythmLength; ++i)
            {
                if (m_rhythm[i])
                {
                    ++result;
                }
            }
            return result;
        }
    };

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

            ++m_rhythmIndex;

            if (m_rhythmIndex >= input.m_rhythmLength)
            {
                m_rhythmIndex %= input.m_rhythmLength;
                ++m_motiveIndex;
                m_lastMotiveLength = m_index;
                m_index = -1;
            }    

            if (input.m_rhythm[m_rhythmIndex])
            {
                ++m_index;
                 m_output = input.GetOutput(m_index, m_motiveIndex);
            }
        }
    }

    void Reset(Input& input)
    {
        m_index = -1;
        m_rhythmIndex = -1;
        m_motiveIndex = 0;
        m_resetNextClock = false;
    }
};

struct NonagonIndexArp
{
    static constexpr size_t x_numClocks = 7;
    static constexpr size_t x_numTrios = 3;
    static constexpr size_t x_voicesPerTrio = 3;
    static constexpr size_t x_numVoices = 9;

    IndexArp m_arp[x_numVoices];
    
    struct Input
    {
        IndexArp::Input m_input[x_numVoices];
        bool m_clocks[x_numClocks];
        int m_clockSelect[x_numTrios];
        int m_resetSelect[x_numTrios];
        bool m_externalReset;

        float m_zoneHeight[x_numTrios];
        float m_zoneOverlap[x_numTrios];
        float m_offset[x_numTrios];
        float m_interval[x_numTrios];
        float m_pageInterval[x_numTrios];
        float m_offsetSpread[x_numTrios];
        float m_intervalSpread[x_numTrios];
        float m_pageIntervalSpread[x_numTrios];
        
        bool m_invert[x_numTrios];
        bool m_retro[x_numTrios];
        bool m_cycle[x_numTrios];        

        void SetTrioInputs()
        {
            for (size_t i = 0; i < x_numTrios; ++i)
            {                
                for (size_t j = 0; j < x_voicesPerTrio; ++j)
                {
                    if (j == 0)
                    {
                        m_input[i * x_voicesPerTrio + j].m_min = 0;
                    }
                    else 
                    {
                        m_input[i * x_voicesPerTrio + j].m_min = m_input[i * x_voicesPerTrio + j - 1].m_max - m_zoneHeight[i] * m_zoneOverlap[i];
                    }
                    
                    m_input[i * x_voicesPerTrio + j].m_max = m_input[i * x_voicesPerTrio + j].m_min + m_zoneHeight[i];

                    m_input[i * x_voicesPerTrio + j].m_offset = m_offset[i] + j * m_offsetSpread[i] / x_voicesPerTrio;
                    m_input[i * x_voicesPerTrio + j].m_interval = m_interval[i] + j * m_intervalSpread[i] / x_voicesPerTrio;
                    m_input[i * x_voicesPerTrio + j].m_pageInterval = m_pageInterval[i] + j * m_pageIntervalSpread[i] / x_voicesPerTrio;
                    m_input[i * x_voicesPerTrio + j].m_invert = m_invert[i];
                    m_input[i * x_voicesPerTrio + j].m_retro = m_retro[i];
                    m_input[i * x_voicesPerTrio + j].m_cycle = m_cycle[i];
                }
            }
        }

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
        input.SetTrioInputs();
        for (size_t i = 0; i < x_numVoices; ++i)
        {
            m_arp[i].Process(input.m_input[i]);
        }
    }
};
