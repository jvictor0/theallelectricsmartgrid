#pragma once

#include <vector>
#include <cassert>
#include <utility>

template <typename T, size_t N>
struct FixedAllocator
{
    T m_data[N];
    bool m_allocated[N];
    size_t m_index;

    int m_freeQueue[N];
    int m_freeQueueHead;
    int m_freeQueueTail;

    FixedAllocator()
        : m_data{}
        , m_allocated{}
        , m_index(0)
        , m_freeQueue{}
        , m_freeQueueHead(N)
        , m_freeQueueTail(0)
    {
        m_freeQueueHead = N;
        m_freeQueueTail = 0;

        for (size_t i = 0; i < N; ++i)
        {
            m_freeQueue[i] = i;
            m_allocated[i] = false;
        }
    }

    size_t Available() const
    {
        return m_freeQueueHead - m_freeQueueTail;
    }

    T* Allocate()
    {
        if (m_freeQueueHead == m_freeQueueTail)
        {
            return nullptr;
        }

        int index = m_freeQueue[m_freeQueueTail % N];
        m_freeQueueTail++;
        m_allocated[index] = true;
        return &m_data[index];
    }

    void Free(T* ptr)
    {
        assert(Available() < N);        

        int index = IndexOf(ptr);
        m_freeQueue[m_freeQueueHead % N] = index;
        m_freeQueueHead++;
        m_allocated[index] = false;
    }

    int IndexOf(T* ptr) const
    {
        return static_cast<int>(ptr - m_data);
    }

    bool ComesFrom(T* ptr) const
    {
        int index = IndexOf(ptr);
        return index >= 0 && index < static_cast<int>(N);
    }

    bool IsAllocated(T* ptr) const
    {
        int index = IndexOf(ptr);
        return m_allocated[index];
    }
};

template <typename T, size_t N>
struct SegmentedAllocator
{
    std::vector<FixedAllocator<T, N>*> m_segments;

    SegmentedAllocator()
    {
        for (size_t i = 0; i < 4; ++i)
        {
            m_segments.push_back(new FixedAllocator<T, N>());
        }
    }

    ~SegmentedAllocator()
    {
        for (auto* seg : m_segments)
        {
            delete seg;
        }
    }

    T* Allocate()
    {
        for (auto* seg : m_segments)
        {
            if (seg->Available() > 0)
            {
                return seg->Allocate();
            }
        }

        // No available slots, create new segment
        //
        auto* newSeg = new FixedAllocator<T, N>();
        m_segments.push_back(newSeg);
        return newSeg->Allocate();
    }

    void Free(T* ptr)
    {
        for (auto* seg : m_segments)
        {
            if (seg->ComesFrom(ptr))
            {
                seg->Free(ptr);
                return;
            }
        }

        assert(false && "Pointer not from this allocator");
    }
};

// PoolPtr: unique ownership pointer that uses a SegmentedAllocator
//
template <typename T, size_t N>
struct PoolPtr
{
    T* m_ptr;
    SegmentedAllocator<T, N>* m_allocator;

    PoolPtr()
        : m_ptr(nullptr)
        , m_allocator(nullptr)
    {
    }

    PoolPtr(T* ptr, SegmentedAllocator<T, N>* allocator)
        : m_ptr(ptr)
        , m_allocator(allocator)
    {
    }

    ~PoolPtr()
    {
        Reset();
    }

    // No copy
    //
    PoolPtr(const PoolPtr&) = delete;
    PoolPtr& operator=(const PoolPtr&) = delete;

    // Move
    //
    PoolPtr(PoolPtr&& other) noexcept
        : m_ptr(other.m_ptr)
        , m_allocator(other.m_allocator)
    {
        other.m_ptr = nullptr;
        other.m_allocator = nullptr;
    }

    PoolPtr& operator=(PoolPtr&& other) noexcept
    {
        if (this != &other)
        {
            Reset();
            m_ptr = other.m_ptr;
            m_allocator = other.m_allocator;
            other.m_ptr = nullptr;
            other.m_allocator = nullptr;
        }

        return *this;
    }

    PoolPtr& operator=(std::nullptr_t)
    {
        Reset();
        return *this;
    }

    void Reset()
    {
        if (m_ptr && m_allocator)
        {
            m_ptr->~T();
            new (m_ptr) T();
            m_allocator->Free(m_ptr);
        }

        m_ptr = nullptr;
        m_allocator = nullptr;
    }

    T* get() const
    {
        return m_ptr;
    }

    T* operator->() const
    {
        return m_ptr;
    }

    T& operator*() const
    {
        return *m_ptr;
    }

    explicit operator bool() const
    {
        return m_ptr != nullptr;
    }
};

// Helper to create PoolPtr with in-place construction
// Destructs the default-constructed object before placement new
//
template <typename T, size_t N, typename... Args>
PoolPtr<T, N> MakePooled(SegmentedAllocator<T, N>& allocator, Args&&... args)
{
    T* ptr = allocator.Allocate();
    ptr->~T();
    new (ptr) T(std::forward<Args>(args)...);
    return PoolPtr<T, N>(ptr, &allocator);
}