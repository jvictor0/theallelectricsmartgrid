#pragma once

struct WaveTable
{
    static const int x_tableSize = 1024;
    float m_table[x_tableSize + 2];
    bool m_isGenerated;

    WaveTable()
    {
        m_isGenerated = false;
    }
    
    float Evaluate(float phase) const
    {
        int index = static_cast<int>(phase * x_tableSize);
        assert(index >= 0 && index <= x_tableSize);
        float remainder = phase * x_tableSize - index;
        return m_table[index] * (1.0f - remainder) + m_table[index + 1] * remainder;
    }

    void GenerateCosine()
    {
        if (m_isGenerated)
        {
            return;
        }
        
        for (int i = 0; i < x_tableSize; ++i)
        {
            m_table[i] = cosf(2.0f * M_PI * i / x_tableSize);
        }

        m_table[x_tableSize] = m_table[0];
        m_table[x_tableSize + 1] = m_table[1];
        m_isGenerated = true;
    }

    void GenerateSine()
    {
        if (m_isGenerated)
        {
            return;
        }
        
        for (int i = 0; i < x_tableSize; ++i)
        {
            m_table[i] = sinf(2.0f * M_PI * i / x_tableSize);
        }

        m_table[x_tableSize] = m_table[0];
        m_table[x_tableSize + 1] = m_table[1];
        m_isGenerated = true;
    }

    static WaveTable s_cosine;
    static WaveTable s_sine;

    static const WaveTable& GetCosine()
    {
        s_cosine.GenerateCosine();
        return s_cosine;
    }

    static const WaveTable& GetSine()
    {
        s_sine.GenerateSine();
        return s_sine;
    }
};

WaveTable WaveTable::s_cosine;
WaveTable WaveTable::s_sine;
