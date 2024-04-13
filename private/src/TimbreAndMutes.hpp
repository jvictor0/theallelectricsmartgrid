#pragma once

struct TimbreAndMute
{
    static constexpr size_t x_numVoices = 9;
    static constexpr size_t x_voicesPerTrio = 3;
    static constexpr size_t x_numTrios = 3;
    static constexpr size_t x_numInBits = 6;
    
    struct TimbreAndMuteSingleVoice
    {
        TimbreAndMuteSingleVoice()
            : m_preTimbre(0)
            , m_timbre(0)
            , m_trig(false)
            , m_muted(false)
        {
            for (size_t i = 0; i < x_numInBits; ++i)
            {
                m_armed[i] = false;
                m_on[i] = false;
            }
        }
        
        struct Input
        {
            bool m_armed[x_numInBits];
            bool* m_on;
            bool* m_canPassIfOn;
            float* m_timbreMult;
            bool m_trigIn;
            
            Input()
                : m_on(nullptr)
                , m_canPassIfOn(nullptr)
                , m_timbreMult(nullptr)
                , m_trigIn(false)
            {
                for (size_t i = 0; i < x_numInBits; ++i)
                {
                    m_armed[i] = false;
                }
            }
        };
        
        void Process(Input& input)
        {
            m_trig = false;
            if (!input.m_trigIn)
            {
                return;
            }
            
            size_t armed = 0;
            size_t onAndArmed = 0;
            for (size_t i = 0; i < x_numInBits; ++i)
            {            
                m_armed[i] = input.m_armed[i];
                if (m_armed[i])
                {
                    ++armed;
                    if (input.m_on[i])
                    {
                        ++onAndArmed;
                    }
                }
            }

            if (input.m_canPassIfOn[onAndArmed])
            {
                m_trig = true;
                m_muted = false;
                for (size_t i = 0; i < x_numInBits; ++i)
                {            
                    m_on[i] = input.m_on[i];
                }

                m_preTimbre = static_cast<float>(onAndArmed) / static_cast<float>(armed);
                m_timbre = (*input.m_timbreMult) * m_preTimbre;
                m_countHigh = onAndArmed;
            }
            else
            {
                m_muted = true;
            }
        }
        
        bool m_armed[x_numInBits];
        bool m_on[x_numInBits];
        size_t m_countHigh;
        float m_preTimbre;
        float m_timbre;
        bool m_trig;
        bool m_muted;
    };
    
    TimbreAndMuteSingleVoice m_voices[x_numVoices];
    
    struct Input
    {
        TimbreAndMuteSingleVoice::Input m_input[x_numVoices];
        bool m_on[x_numInBits];
        bool m_canPassIfOn[x_numTrios][x_numInBits + 1];
        float m_timbreMult[x_numTrios];
        
        Input()
        {
            for (size_t i = 0; i < x_numVoices; ++i)
            {
                m_input[i].m_on = m_on;
                m_input[i].m_canPassIfOn = m_canPassIfOn[i / x_voicesPerTrio];
                m_input[i].m_timbreMult = &m_timbreMult[i / x_voicesPerTrio];
            }

            for (size_t i = 0; i < x_numInBits; ++i)
            {
                m_on[i] = false;
            }

            for (size_t i = 0; i < x_numInBits + 1; ++i)
            {
                for (size_t j = 0; j < x_numTrios; ++j)
                {
                    m_canPassIfOn[j][i] = true;
                }
            }

            for (size_t i = 0; i < x_numTrios; ++i)
            {
                m_timbreMult[i] = 0;
            }
        }
    };
    
    void Process(Input& input)
    {
        for (size_t i = 0; i < x_numVoices; ++i)
        {
            m_voices[i].Process(input.m_input[i]);
        }
    }
};
