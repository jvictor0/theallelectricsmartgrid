#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unordered_map>

struct TwoByte
{
    uint16_t m_value;
    static constexpr uint16_t x_maxValue = 1 << 14;

    TwoByte()
    {
        m_value = 0x8080;
    }

    TwoByte(uint16_t value)
    {
        Set(value);
    }

    void Set(int b, uint16_t a)
    {
        Clear(b);
        m_value = m_value | (a << (8 * b));
    }

    void Set(uint16_t a)
    {
        Set(0, a & 0x7F);
        Set(1, (a >> 7) & 0x7F);
    }

    void Clear(int b)
    {
        m_value = m_value & ~(0xFF << (8 * b));
    }

    uint16_t Get(int b)
    {
        return (m_value >> (8 * b)) & 0xFF;
    }

    bool IsSet(int b)
    {
        return Get(b) != 0x80;
    }

    int16_t Get()
    {
        uint16_t value = 0;
        for (int i = 0; i < 2; i++)
        {
            if (IsSet(i))
            {
                value |= Get(i) << (7 * i);
            }
        }

        return value;
    }

    void Clear()
    {
        m_value = 0x8080;
    }

    float GetFloat()
    {
        return static_cast<float>(Get()) / x_maxValue;
    }

    void SetFloat(float value)
    {
        Set(static_cast<uint16_t>(value * x_maxValue));
    }
};

struct NRPNInterfaceSingleChannel
{
    int m_chan;
    TwoByte m_key;
    TwoByte m_value;

    NRPNInterfaceSingleChannel()
        : m_chan(0)
    {
    }

    bool UpdateFromMidi(int cc, int value)
    {
        if (cc == 0x63 && !m_key.IsSet(0))
        {
            m_key.Set(0, value);
            return true;
        }
        else if (cc == 0x62 && !m_key.IsSet(1))
        {
            m_key.Set(1, value);
            return true;
        }
        else if (cc == 0x06 && !m_value.IsSet(0))
        {
            m_value.Set(0, value);
            return true;
        }
        else if (cc == 0x26 && !m_value.IsSet(1))
        {
            m_value.Set(1, value);
            return true;
        }

        return false;
    }

    bool IsComplete()
    {
        return m_key.IsSet(0) && m_key.IsSet(1) && m_value.IsSet(0);
    }

    bool PopulateMidi(int i, uint8_t& cc, uint8_t& value)
    {
        if (i == 0)
        {
            cc = 0x63;
            value = m_key.Get(0);
            return true;
        }
        else if (i == 1)
        {
            cc = 0x62;
            value = m_key.Get(1);
            return true;
        }
        else if (i == 2)
        {
            cc = 0x06;
            value = m_value.Get(0);
            return true;
        }
        else if (i == 3 && m_value.IsSet(1))
        {
            cc = 0x26;
            value = m_value.Get(1);
            return true;
        }

        return false;
    }
};

struct NRPNInterface
{
    static constexpr int x_maxChannels = 16;
    NRPNInterfaceSingleChannel m_channels[x_maxChannels];

    bool UpdateFromMidi(int chan, int cc, int value)
    {
        if (chan < 0 || chan >= x_maxChannels)
        {
            return false;
        }

        return m_channels[chan].UpdateFromMidi(cc, value);
    }

    void Clear(int chan)
    {
        if (chan < 0 || chan >= x_maxChannels)
        {
            return;
        }

        m_channels[chan] = NRPNInterfaceSingleChannel();
    }

    void Clear()
    {
        for (int i = 0; i < x_maxChannels; i++)
        {
            Clear(i);
        }
    }
};
    
struct NRPN
{
    uint8_t m_chan;
    uint16_t m_key;
    float m_value;

    NRPN()
        : m_chan(0)
        , m_key(0)
        , m_value(0)
    {
    }

    NRPN(NRPNSingleChannel& channel)
        : m_chan(channel.m_chan)
        , m_key(channel.m_key.Get())
        , m_value(channel.m_value.GetFloat())
    {
    }

    NRPN(int chan, uint16_t key, float value)
        : m_chan(chan)
        , m_key(key)
        , m_value(value)
    {
    }

    NRPN(uint32_t key, float value)
        : m_chan(key >> 16)
        , m_key(key & 0xFFFF)
        , m_value(value)
    {
    }

    uint32_t GetKey()
    {
        return (m_chan << 16) | m_key;
    }

    void GetInterface(NRPNInterfaceSingleChannel& interface)
    {
        interface.m_chan = m_chan;
        interface.m_key.Set(m_key);
        interface.m_value.SetFloat(m_value);
    }

    NRPN Interpolate(NRPN& other, float t)
    {
        return NRPN(m_chan, m_key, m_value + (other.m_value - m_value) * t);
    }
};
    
struct NPRNManager
{
    static constexpr int x_numBanks = 8;
    
    struct Entry
    {
        float m_value;
        float m_max;
        float m_min;
        float m_bankedValues[x_numBanks];

        Entry(float value)
            : m_value(value)
            , m_max(value)
            , m_min(value)
        {
            for (int i = 0; i < x_numBanks; i++)
            {
                m_bankedValues[i] = value;
            }
        }

        void Serialize(std::vector<uint8_t>& buffer)
        {
            buffer.insert(buffer.end(), static_cast<uint8_t*>(&m_value), static_cast<uint8_t*>(&m_value) + sizeof(float));
            buffer.insert(buffer.end(), static_cast<uint8_t*>(&m_max), static_cast<uint8_t*>(&m_max) + sizeof(float));
            buffer.insert(buffer.end(), static_cast<uint8_t*>(&m_min), static_cast<uint8_t*>(&m_min) + sizeof(float));
            buffer.insert(buffer.end(), static_cast<uint8_t*>(m_bankedValues), static_cast<uint8_t*>(m_bankedValues) + sizeof(float) * x_numBanks);
        }

        void Deserialize(std::vector<uint8_t>& buffer, int& offset)
        {
            m_value = *reinterpret_cast<float*>(&buffer[offset]);
            offset += sizeof(float);
            m_max = *reinterpret_cast<float*>(&buffer[offset]);
            offset += sizeof(float);
            m_min = *reinterpret_cast<float*>(&buffer[offset]);
            offset += sizeof(float);
            memcpy(m_bankedValues, &buffer[offset], sizeof(float) * x_numBanks);
            offset += sizeof(float) * x_numBanks;
        }
        
        void SetValue(float value)
        {
            m_value = value;
        }

        void Set(float value, int b1, int b2, float t)
        {
            m_max = std::max(m_max, value);
            m_min = std::min(m_min, value);
            float delta = value - m_value;
            m_value = value;
            float newBV1 = m_bankedValues[b1] + delta * (1 - t);
            float newBV2 = m_bankedValues[b2] + delta * t;
            if (newBV1 < m_min || newBV1 > m_max)
            {
                m_bankedValues[b1] = newBV1 < m_min ? m_min : m_max;
                m_bankedValues[b2] = (value - m_bankedValues[b1] * (1 - t)) / t;
            }
            else if (newBV2 < m_min || newBV2 > m_max)
            {
                m_bankedValues[b2] = newBV2 < m_min ? m_min : m_max;
                m_bankedValues[b1] = (value - m_bankedValues[b2] * t) / (1 - t);
            }
            else
            {
                m_bankedValues[b1] = newBV1;
                m_bankedValues[b2] = newBV2;
            }
        }
            
        float Interpolate(int b1, int b2, float t)
        {
            return (1 - t) * m_bankedValues[b1] + t * m_bankedValues[b2];
        }
    };

    std::unordered_map<uint32_t, Entry> m_entries;
    int m_bank1;
    int m_bank2;
    float m_t;

    void Process(NRPN& nrpn)
    {
        if (m_entries.find(nrpn.GetKey()) == m_entries.end())
        {
            m_entries[nrpn.GetKey()] = Entry(nrpn.m_value);
        }

        m_entries[nrpn.GetKey()].Set(nrpn.m_value, m_bank1, m_bank2, m_t);
    }
    
    struct Iterator
    {
        NPRNManager& m_manager;
        std::unordered_map<uint32_t, Entry>::iterator m_iter;

        Iterator(NPRNManager& manager, bool end)
            : m_manager(manager)
        {
            if (end)
            {
                m_iter = m_manager.m_entries.end();
            }
            else
            {
                m_iter = m_manager.m_entries.begin();
            }
        }

        bool operator!=(const Iterator& other)
        {
            return m_iter != other.m_iter;
        }

        Iterator& operator++()
        {
            m_iter++;
            return *this;
        }

        NRPN operator*()
        {
            return NRPN(m_iter->first, m_iter->second.Interpolate(m_manager.m_bank1, m_manager.m_bank2, m_manager.m_t));
        }
    };

    Iterator begin()
    {
        return Iterator(*this, false);
    }

    Iterator end()
    {
        return Iterator(*this, true);
    }

    void Serialize(std::vector<uint8_t>& buffer)
    {
        buffer.insert(buffer.end(), static_cast<uint8_t*>(&m_bank1), static_cast<uint8_t*>(&m_bank1) + sizeof(int));
        buffer.insert(buffer.end(), static_cast<uint8_t*>(&m_bank2), static_cast<uint8_t*>(&m_bank2) + sizeof(int));
        buffer.insert(buffer.end(), static_cast<uint8_t*>(&m_t), static_cast<uint8_t*>(&m_t) + sizeof(float));
        for (auto& entry : m_entries)
        {
            buffer.insert(buffer.end(), static_cast<uint8_t*>(&entry.first), static_cast<uint8_t*>(&entry.first) + sizeof(uint32_t));
            entry.second.Serialize(buffer);
        }
    }

    void Deserialize(std::vector<uint8_t>& buffer)
    {
        int offset = 0;
        m_bank1 = *reinterpret_cast<int*>(&buffer[offset]);
        offset += sizeof(int);
        m_bank2 = *reinterpret_cast<int*>(&buffer[offset]);
        offset += sizeof(int);
        m_t = *reinterpret_cast<float*>(&buffer[offset]);
        offset += sizeof(float);
        while (offset < buffer.size())
        {
            uint32_t key = *reinterpret_cast<uint32_t*>(&buffer[offset]);
            offset += sizeof(uint32_t);
            m_entries[key] = Entry(0);
            m_entries[key].Deserialize(buffer, offset);
        }
    }
};
