#pragma once

inline float Flip(float in)
{
    if (in > 0)
    {
        return 0;
    }
    else
    {
        return 10.0;
    }
}

inline float CFlip(bool c, float in)
{
    return c ? Flip(in) : in;
}

struct Block
{
    static constexpr float x_blinkDelay = 0.01;
    static constexpr float x_blinkTime = 0.01;

    bool m_wasUp;
    float m_value;
    float m_timeToBlink;
    float m_timeBlinking;
    bool m_blink;
    bool m_flash;
    bool m_light;
    bool m_zeroOnSameVal;
    
    Block()
        : m_wasUp(false)
        , m_value(0)
        , m_timeToBlink(0)
        , m_timeBlinking(0)
        , m_blink(false)
        , m_flash(false)
        , m_light(false)
        , m_zeroOnSameVal(true)
    {
    }
    
    bool SetVal(float val)
    {
        if (val > 0)
        {
            m_timeToBlink = x_blinkDelay;
            
            if (!m_wasUp)
            {
                if (m_value == val)
                {
                    if (m_zeroOnSameVal)
                    {
                        m_value = 0;
                    }
                }
                else
                {
                    m_value = val;
                }
                
                m_wasUp = true;

                return true;
            }
        }
        else
        {
            if (m_wasUp)
            {
                m_timeToBlink = x_blinkDelay;
                m_wasUp = false;
            }
        }

        return false;
    }

    void SetValueDirectly(float val)
    {
        m_value = val;
    }

    void Blink()
    {
        m_timeBlinking = x_blinkTime;
    }
    
    void SetLight(bool on)
    {
        m_light = on;
        if (on && m_value > 0)
        {
            Blink();
        }
    }

    void SetFlash(bool flash)
    {
        m_flash = flash;
    }
    
    bool Process(float dt, float val)
    {
        bool result = SetVal(val);
        
        if (m_timeToBlink > 0)
        {
            m_blink = false;
            m_timeToBlink -= dt;
            if (m_timeToBlink < 0)
            {
                m_timeBlinking = x_blinkTime;
            }
        }
        else if (m_timeBlinking > 0)
        {
            m_blink = true;
            m_timeBlinking -= dt;
        }
        else
        {
            m_blink = false;
        }

        return result;
    }
    
    float GetLight(bool flash)
    {
        float value = m_value;
        if (value == 0 && m_light)
        {
            value = 10;
        }
        if (!m_flash || value == 0)
        {
            return CFlip(m_blink, value);
        }
        else
        {
            return CFlip(flash, value);
        }
    }
};

template<size_t x_numBlocks>
struct RadialButtonsMultiCC
{
    Block m_blocks[x_numBlocks];
    size_t m_upIx;
    
    struct Input
    {
        float m_values[x_numBlocks];
    };

    RadialButtonsMultiCC() :
        m_upIx(0)
    {
        for (size_t i = 0; i < x_numBlocks; ++i)
        {
            m_blocks[i].m_zeroOnSameVal = false;
        }

        SetUpIx(0);
    }

    bool Process(float dt, Input& inputs)
    {
        bool found = false;
        for (size_t i = 0; i < x_numBlocks; ++i)
        {
            if (m_blocks[i].Process(dt, inputs.m_values[i]))
            {
                if (!found)
                {
                    m_upIx = i;
                    found = true;
                }
            }
        }

        if (found)
        {
            for (size_t i = 0; i < x_numBlocks; ++i)
            {
                if (i != m_upIx)
                {
                    m_blocks[i].SetValueDirectly(0);
                }
            }

            return true;
        }

        return false;
    }

    void SetUpIx(size_t ix)
    {
        m_upIx = ix;
        for (size_t i = 0; i < x_numBlocks; ++i)
        {
            m_blocks[i].SetValueDirectly(i == m_upIx ? 10 : 0);
        }
    }

    Block& GetBlock(size_t ix)
    {
        return m_blocks[ix];
    }
}; 
