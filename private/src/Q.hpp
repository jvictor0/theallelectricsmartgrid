#pragma once
#include <numeric>

struct Q
{
    int64_t m_num;
    int64_t m_denom;

    Q() : m_num(0), m_denom(1)
    {
    }

    Q(int64_t num, int64_t denom)
        : m_num(num)
        , m_denom(denom)
    {
        Reduce();
    }

    Q(int64_t asInt)
        : m_num(asInt)
        , m_denom(1)
    {
    }
    
    void Reduce()
    {
        int64_t gcd = std::gcd(m_num, m_denom);
        m_num /= gcd;
        m_denom /= gcd;
    }

    std::string ToString()
    {
        return std::to_string(m_num) + "/" + std::to_string(m_denom);
    }

    Q operator*(Q other)
    {
        return Q(m_num * other.m_num, m_denom * other.m_denom);
    }

    Q operator+(Q other)
    {
        return Q(m_num * other.m_denom + other.m_num * other.m_denom, m_denom * other.m_denom);
    }

    double ToDouble()
    {
        return static_cast<double>(m_num) / static_cast<double>(m_denom);
    }

    bool operator==(Q other)
    {
        return std::tie(m_num, m_denom) == std::tie(other.m_num, other.m_denom);
    }

    bool operator!=(Q other)
    {
        return !(*this == other);
    }

    bool operator<(Q other)
    {
        return m_num * other.m_denom < other.m_num * m_denom;
    }

    bool operator<=(Q other)
    {
        return m_num * other.m_denom <= other.m_num * m_denom;
    }
};