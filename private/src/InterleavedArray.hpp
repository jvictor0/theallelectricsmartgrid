#pragma once

template<typename T, size_t N, size_t Fold>
struct InterleavedArray
{
    T m_data[N * Fold];

    T& Get(size_t foldIx, size_t index)
    {
        return m_data[index * Fold + foldIx];
    }
};

template<typename T, size_t N, size_t Fold>
struct InterleavedArrayHolder
{
    InterleavedArray<T, N, Fold>* m_array;
    size_t m_foldIx;

    InterleavedArrayHolder()
        : m_array(nullptr)
        ,m_foldIx(0)
    {
    }

    InterleavedArrayHolder(InterleavedArray<T, N, Fold>* array, size_t foldIx)
        : m_array(array)
        , m_foldIx(foldIx)
    {
    }

    T& operator[](size_t index)
    {
        return m_array->Get(m_foldIx, index);
    }
};