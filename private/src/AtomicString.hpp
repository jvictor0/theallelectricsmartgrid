#pragma once

#include <atomic>
#include <cassert>
#include <cstdio>

struct ShortBuf
{
    char m_buffer[9];

    ShortBuf()
    {
        m_buffer[0] = '\0';
    }

    ShortBuf(const char* string)
    {
        SetString(string);
    }

    ShortBuf(size_t value)
    {
        SetString(value);
    }

    const char* GetString() const
    {
        return m_buffer;
    }

    void SetString(const char* string)
    {
        strncpy(m_buffer, string, sizeof(m_buffer) - 1);
        m_buffer[sizeof(m_buffer) - 1] = '\0';
    }

    void SetString(size_t value)
    {
        SetString(reinterpret_cast<const char*>(&value));
    }

    size_t GetValue() const
    {
        return *reinterpret_cast<const size_t*>(m_buffer);
    }

    void SetFromFloat(float value, const char* units)
    {
        const char* siPrefix = "";
        const char* sign = value > 0.0f ? "" : "-";
        value = std::abs(value);

        if (value >= 1e6f)
        {
            value /= 1e6f;
            siPrefix = "M";
        }
        else if (value >= 1e3f)
        {
            value /= 1e3f;
            siPrefix = "k";
        }
        else if (value >= 1.0f)
        {
            // No prefix, keep value as is.
            siPrefix = "";
        }
        else if (value >= 1e-3f)
        {
            value *= 1e3f;
            siPrefix = "m";
        }
        else if (value >= 1e-6f)
        {
            value *= 1e6f;
            siPrefix = "u";
        }
        else
        {
            // Too small, just print as is, may be zero
            siPrefix = "";
        }
     
        if (100 <= value)
        {
            snprintf(m_buffer, sizeof(m_buffer), "%s%03.0f%s%s", sign, value, siPrefix, units);
        }
        else if (10 <= value)
        {
            snprintf(m_buffer, sizeof(m_buffer), "%s%02.1f%s%s", sign, value, siPrefix, units);
        }
        else
        {
            snprintf(m_buffer, sizeof(m_buffer), "%s%0.2f%s%s", sign, value, siPrefix, units);
        } 
    }
};

struct AtomicString
{
    std::atomic<size_t> m_string;

    ShortBuf GetString() const
    {
        return ShortBuf(m_string.load());
    }

    void SetString(const char* string)
    {
        m_string.store(ShortBuf(string).GetValue());
    }
    
    void SetString(ShortBuf value)
    {
        m_string.store(value.GetValue());
    }   

    void SetFromFloat(float value, const char* units)
    {
        ShortBuf buf;
        buf.SetFromFloat(value, units);
        SetString(buf);
    }
};