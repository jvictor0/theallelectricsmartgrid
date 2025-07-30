#pragma once

template<typename T>
struct BitSet
{
    BitSet() : m_bits(0) {}
    BitSet(T bits) : m_bits(bits) {}

    void Set(int index, bool value)
    {
        if (value)
        {
            m_bits |= (1 << index);
        }
        else
        {
            m_bits &= ~(1 << index);
        }
    }

    bool Get(int index) const
    {
        return (m_bits & (1 << index)) != 0;
    }
    
    BitSet Union(const BitSet& other) const
    {
        return BitSet(m_bits | other.m_bits);
    }

    BitSet Intersect(const BitSet& other) const
    {
        return BitSet(m_bits & other.m_bits);
    }

    bool IsZero() const
    {
        return m_bits == 0;
    }

    bool operator==(const BitSet& other) const
    {
        return m_bits == other.m_bits;
    }

    void Clear()
    {
        m_bits = 0;
    }

    T m_bits;
};

typedef BitSet<uint64_t> BitSet64;
typedef BitSet<uint32_t> BitSet32;
typedef BitSet<uint16_t> BitSet16;
typedef BitSet<uint8_t> BitSet8;
