#pragma once

struct TimbreAndMute
{
    static constexpr size_t x_numVoices = 9;
    static constexpr size_t x_voicesPerTrio = 3;
    static constexpr size_t x_numTrios = 3;
    static constexpr size_t x_numInBits = 6;

    enum class MonoMode : int
    {
        Poly = 0,
        Trio = 1,
        Global = 2
    };
    
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
            bool m_trigIn;
            
            Input()
                : m_on(nullptr)
                , m_canPassIfOn(nullptr)
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
                m_timbre = m_preTimbre;
                m_countHigh = onAndArmed;
            }
            else
            {
                m_muted = true;
            }
        }

        void UnTrig()
        {
            m_trig = false;
            m_muted = true;
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
        MonoMode m_monoMode[x_numTrios];
        bool* m_mutes;
        
        Input(bool* mutes)
            : m_mutes(mutes)
        {
            for (size_t i = 0; i < x_numVoices; ++i)
            {
                m_input[i].m_on = m_on;
                m_input[i].m_canPassIfOn = m_canPassIfOn[i / x_voicesPerTrio];
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
                m_monoMode[i] = MonoMode::Poly;
            }
        }
    };
    
    void Process(Input& input)
    {
        bool globalTrig = false;
        for (int i = x_numTrios - 1; i >= 0; --i)
        {
            bool trioTrig = false;
            for (size_t j = 0; j < x_voicesPerTrio; ++j)
            {
                size_t ix = i * x_voicesPerTrio + j;
                if ((trioTrig && input.m_monoMode[i] == MonoMode::Trio) ||
                    (globalTrig && input.m_monoMode[i] == MonoMode::Global))
                {
                    input.m_input[ix].m_trigIn = false;
                }
                
                m_voices[ix].Process(input.m_input[ix]);
                if (m_voices[ix].m_trig && !input.m_mutes[ix])
                {
                    trioTrig = true;
                    if (input.m_monoMode[i] == MonoMode::Global)
                    {
                        globalTrig = true;
                    }
                }
            }
        }
    }
};
