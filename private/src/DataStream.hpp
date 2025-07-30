#pragma once

#include <atomic>

template <size_t Size, size_t Fold>
struct DataBuffer
{
    float m_data[Size * Fold];

    float* GetData(size_t foldIx)
    {
        return m_data + foldIx * Size;
    }    
};

