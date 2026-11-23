// Read-only, parallel FNV validation for immutable survey records.
//
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>

extern "C" int64_t LadderVerifyRecords(const unsigned char* data, uint64_t count, uint64_t recordBytes)
{
    if (recordBytes != 3472)
    {
        return -2;
    }

    std::atomic<uint64_t> next{0};
    std::atomic<int64_t> bad{-1};
    std::vector<std::thread> workers;
    for (int worker = 0; worker < 8; ++worker)
    {
        workers.emplace_back([&]
        {
            while (true)
            {
                const uint64_t begin = next.fetch_add(256);
                if (begin >= count)
                {
                    break;
                }

                for (uint64_t i = begin; i < std::min(count,begin+256); ++i)
                {
                    const auto* record = data+i*recordBytes;
                    uint64_t hash = 14695981039346656037ULL;
                    for (uint64_t b = 0; b < recordBytes-8; ++b)
                    {
                        hash = (hash^record[b])*1099511628211ULL;
                    }

                    uint64_t expected;
                    std::memcpy(&expected,record+recordBytes-8,sizeof(expected));
                    if (hash != expected)
                    {
                        bad.store(static_cast<int64_t>(i));
                    }
                }
            }
        });
    }

    for (auto& worker : workers)
    {
        worker.join();
    }

    return bad.load();
}
