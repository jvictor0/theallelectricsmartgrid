

struct GridButton
{
    static constexpr size_t x_gridX = 20;
    static constexpr size_t x_gridY = 8;
    
    static uint8_t ToIndex(uint8_t x, uint8_t y)
    {
        return x + y * x_gridX;
    }
    
    static uint8_t ToX(uint8_t index)
    {
        return index % x_gridX;
    }

    static uint8_t ToY(uint8_t index)
    {
        return index / x_gridX;
    }

    static std::pair<uint8_t, uint8_t> ToXY(uint8_t index)
    {
        return std::make_pair(ToX(index), ToY(index));
    }
};

struct ColorRemember
{
    struct Color
    {
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint8_t empty;

        Color(uint8_t r, uint8_t g, uint8_t b)
            : r(r), g(g), b(b), empty(0)
        {
        }

        Color()
            : r(0), g(0), b(0), empty(1)
        {
        }

        bool operator==(const Color& other) const
        {
            return r == other.r && g == other.g && b == other.b && empty == other.empty;
        }
    };

    Color m_colors[GridButton::x_gridX][GridButton::x_gridY];

    void Clear()
    {
        for (size_t x = 0; x < GridButton::x_gridX; ++x)
        {
            for (size_t y = 0; y < GridButton::x_gridY; ++y)
            {
                m_colors[x][y] = Color();
            }
        }
    }

    bool Remember(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b)
    {
        Color color(r, g, b);

        if (m_colors[x][y] == color)
        {
            return false;
        }

        m_colors[x][y] = color;

        return true;
    }
};
