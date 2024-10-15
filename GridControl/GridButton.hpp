

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
