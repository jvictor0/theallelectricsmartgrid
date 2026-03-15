#pragma once

#include <cmath>
#include <cstddef>

template<size_t Size>
struct PositionalBufferRecorder
{
    static_assert(0 < Size, "PositionalBufferRecorder size must be positive");

    enum class Direction
    {
        None,
        Ascending,
        Descending
    };

    static constexpr size_t x_bufferSize = Size;

    double m_buffer[x_bufferSize]{};
    double m_lastUnwrappedX{0.0};
    double m_lastY{0.0};
    size_t m_lastIndex{0};
    size_t m_maxIndex{0};
    Direction m_direction{Direction::None};
    bool m_hasPoint{false};

    double WrapPosition(double x) const
    {
        return x - std::floor(x);
    }

    double UnwrapPosition(double x) const
    {
        double wrappedX = WrapPosition(x);
        if (!m_hasPoint)
        {
            return wrappedX;
        }

        return wrappedX + std::round(m_lastUnwrappedX - wrappedX);
    }

    size_t WrapIndex(double x) const
    {
        double wrappedX = WrapPosition(x);
        return static_cast<size_t>(wrappedX * static_cast<double>(x_bufferSize)) % x_bufferSize;
    }

    double PositionForUnwrappedIndex(long long index) const
    {
        return static_cast<double>(index) / static_cast<double>(x_bufferSize);
    }

    double Lerp(double y0, double y1, double t) const
    {
        return y0 + t * (y1 - y0);
    }

    double ReadAtIndex(double index) const
    {
        double wrappedIndex = index - std::floor(index / static_cast<double>(x_bufferSize)) * static_cast<double>(x_bufferSize);
        size_t i0 = static_cast<size_t>(std::floor(wrappedIndex)) % x_bufferSize;
        size_t i1 = (i0 + 1) % x_bufferSize;
        double alpha = wrappedIndex - std::floor(wrappedIndex);

        return Lerp(m_buffer[i0], m_buffer[i1], alpha);
    }

    Direction GetDirection(double deltaX) const
    {
        if (deltaX < 0.0)
        {
            return Direction::Descending;
        }

        if (deltaX > 0.0)
        {
            return Direction::Ascending;
        }

        return Direction::None;
    }

    void WritePoint(double x, double y)
    {
        m_buffer[WrapIndex(x)] = y;
    }

    void WriteAscendingSegment(double x0, double y0, double x1, double y1)
    {
        if (x1 <= x0)
        {
            return;
        }

        long long lowIndex = static_cast<long long>(std::ceil(x0 * static_cast<double>(x_bufferSize)));
        long long highIndex = static_cast<long long>(std::floor(x1 * static_cast<double>(x_bufferSize)));
        double dx = x1 - x0;
        for (long long i = lowIndex; i <= highIndex; ++i)
        {
            double x = PositionForUnwrappedIndex(i);
            double t = (x - x0) / dx;
            double y = Lerp(y0, y1, t);
            WritePoint(x, y);
        }
    }

    void WriteDescendingSegment(double x0, double y0, double x1, double y1)
    {
        if (x0 <= x1)
        {
            return;
        }

        long long highIndex = static_cast<long long>(std::floor(x0 * static_cast<double>(x_bufferSize)));
        long long lowIndex = static_cast<long long>(std::ceil(x1 * static_cast<double>(x_bufferSize)));
        double dx = x0 - x1;
        for (long long i = highIndex; lowIndex <= i; --i)
        {
            double x = PositionForUnwrappedIndex(i);
            double t = (x0 - x) / dx;
            double y = Lerp(y0, y1, t);
            WritePoint(x, y);
        }
    }

    void WriteSegment(double x0, double y0, double x1, double y1, Direction direction)
    {
        if (direction == Direction::Ascending)
        {
            WriteAscendingSegment(x0, y0, x1, y1);
        }
        else if (direction == Direction::Descending)
        {
            WriteDescendingSegment(x0, y0, x1, y1);
        }
    }

    void Record(double x, double y)
    {
        double unwrappedX = UnwrapPosition(x);
        size_t index = WrapIndex(unwrappedX);
        WritePoint(unwrappedX, y);

        if (m_hasPoint)
        {
            Direction direction = GetDirection(unwrappedX - m_lastUnwrappedX);
            if (direction == Direction::None)
            {
                direction = m_direction;
            }
            else
            {
                m_direction = direction;
            }

            if (direction != Direction::None && index != m_lastIndex)
            {
                WriteSegment(m_lastUnwrappedX, m_lastY, unwrappedX, y, direction);
            }
        }

        m_lastUnwrappedX = unwrappedX;
        m_lastY = y;
        m_lastIndex = index;
        m_maxIndex = std::max(m_maxIndex, index);
        m_hasPoint = true;
    }

    double Get(size_t index) const
    {
        return m_buffer[index % x_bufferSize];
    }

    double Get(double x) const
    {
        double index = WrapPosition(x) * static_cast<double>(x_bufferSize);
        return ReadAtIndex(index);
    }
};
