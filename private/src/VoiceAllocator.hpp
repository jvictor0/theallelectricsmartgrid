
template <typename T>
struct VoiceAllocator
{
    static constexpr size_t x_maxVoices = 16;

    size_t m_maxPolyphony = 1;
    int64_t m_ix = 0;
    size_t m_numActive = 0;
    T m_voices[x_maxVoices];

    void Allocate(T voice)
    {
        if (m_numActive == m_maxPolyphony)
        {
            DeAllocate(m_ix);
        }

        assert(m_numActive < m_maxPolyphony);
        
        while (m_voices[m_ix].m_gate && *m_voices[m_ix].m_gate)
        {
            m_ix = (m_ix + 1) % m_maxPolyphony;
        }

        m_voices[m_ix] = voice;        
        m_ix = (m_ix + 1) % m_maxPolyphony;
        *voice.m_gate = true;
        ++m_numActive;
    }

    void DeAllocate(int64_t index)
    {
        if (m_voices[index].m_gate && *m_voices[index].m_gate)
        {
            *m_voices[index].m_gate = false;
            --m_numActive;
        }
    }

    void DeAllocate(T voice)
    {
        for (size_t i = 0; i < m_maxPolyphony; ++i)
        {
            if (m_voices[i] == voice)
            {
                DeAllocate(i);
            }
        }
    }

    void Clear()
    {
        for (size_t i = 0; i < x_maxVoices; ++i)
        {
            if (m_voices[i].m_gate)
            {
                *m_voices[i].m_gate = false;
            }
        }

        m_ix = 0;
        m_numActive = 0;
    }

    void SetPolyphony(size_t polyphony)
    {
        m_maxPolyphony = polyphony;
        Clear();
    }

    struct iterator
    {
        VoiceAllocator<T>* m_allocator;
        int64_t m_index;

        iterator(VoiceAllocator<T>* allocator, int64_t index)
        {
            m_allocator = allocator;
            m_index = index;
        }

        iterator& operator++()
        {
            m_index++;
            while (*this != m_allocator->end() && !(**this).m_gate && !*(**this).m_gate)
            {
                m_index++;
            }
            
            return *this;
        }

        T& operator*()
        {
            return m_allocator->m_voices[m_index];
        }

        bool operator!=(const iterator& other)
        {
            return m_index != other.m_index;
        }
    };

    iterator begin()
    {
        return iterator(this, 0);
    }

    iterator end()
    {
        return iterator(this, m_maxPolyphony);
    }
};

struct CellVoice
{
    bool* m_gate;
    int m_x;
    int m_y;

    CellVoice()
    {
        m_gate = nullptr;
        m_x = 0;
        m_y = 0;
    }

    CellVoice(bool* gate, int x, int y)
    {
        m_gate = gate;
        m_x = x;
        m_y = y;
    }

    bool operator==(const CellVoice& other)
    {
        return m_x == other.m_x && m_y == other.m_y;
    }
};
