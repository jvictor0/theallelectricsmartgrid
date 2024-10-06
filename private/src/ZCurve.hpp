namespace SmartGrid
{

size_t ZEncode(size_t r, size_t g, size_t b)
{
    size_t result = 0;
    for (size_t i = 0; i < 8; ++i)
    {
        result |= static_cast<size_t>((r >> i) & 1) << (3 * i);
        result |= static_cast<size_t>((g >> i) & 1) << (3 * i + 1);
        result |= static_cast<size_t>((b >> i) & 1) << (3 * i + 2);
    }

    return result;
}

void ZDecode(size_t x, uint8_t* r, uint8_t* g, uint8_t* b)
{
    for (size_t i = 0; i < 8; ++i)
    {
        *r |= ((x >> (3 * i)) & 1) << i;
        *g |= ((x >> (3 * i + 1)) & 1) << i;
        *b |= ((x >> (3 * i + 2)) & 1) << i;
    }
}

}
