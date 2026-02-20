#pragma once

#include "BasicMidi.hpp"

struct KMixMidi
{
    static constexpr size_t x_numChannels = 4;

    static SmartGrid::BasicMidi Trim(int channel, int trim)
    {
        return SmartGrid::BasicMidi::CC(0, -1, channel, 29, trim);
    }

    struct Iterator
    {
        KMixMidi* m_owner;
        size_t m_channel;

        bool IsDone() const
        {
            return x_numChannels <= m_channel;
        }

        void Advance()
        {
            ++m_channel;
        }

        bool NeedsToSend() const
        {
            if (m_owner->m_trims[m_channel] != m_owner->m_trimsSent[m_channel])
            {
                return true;
            }

            return false;
        }

        Iterator& operator++()
        {
            Advance();
            while (!IsDone() && !NeedsToSend())
            {
                Advance();
            }

            return *this;
        }

        SmartGrid::BasicMidi operator*() const
        {
            m_owner->m_trimsSent[m_channel] = m_owner->m_trims[m_channel];
            return Trim(m_channel, m_owner->m_trims[m_channel]);
        }

        Iterator(KMixMidi* owner)
            : m_owner(owner)
            , m_channel(0)
        {
            while (!IsDone() && !NeedsToSend())
            {
                Advance();
            }
        }

        Iterator()
            : m_owner(nullptr)
            , m_channel(x_numChannels)
        {
        }

        bool operator==(const Iterator& other) const
        {
            return m_channel == other.m_channel;
        }

        bool operator!=(const Iterator& other) const
        {
            return !(*this == other);
        }
    };

    Iterator begin()
    {
        return Iterator(this);
    }

    Iterator end()
    {
        return Iterator();
    }

    void SetTrim(size_t channel, float trim)
    {
        m_trims[channel] = static_cast<uint8_t>(trim * 127.0f);
    }

    void Reset()
    {
        for (size_t i = 0; i < x_numChannels; ++i)
        {
            m_trims[i] = 255;
            m_trimsSent[i] = 255;
        }
    }

    KMixMidi()
        : m_trims{}
        , m_trimsSent{}
    {
        for (size_t i = 0; i < x_numChannels; ++i)
        {
            m_trims[i] = 255;
            m_trimsSent[i] = 255;
        }
    }

    uint8_t m_trims[4];
    uint8_t m_trimsSent[4];
};