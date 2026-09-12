#pragma once

#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>

namespace SmartGrid
{
    struct MidiOutputSchedule
    {
        static constexpr std::uint64_t x_latencyUs = 20000;
        static inline thread_local std::uint64_t s_deadline = 0;
        static inline std::atomic<std::uint64_t> s_scheduled{0};
        static inline std::atomic<std::uint64_t> s_immediate{0};
        static inline std::atomic<std::uint64_t> s_errors{0};
        static inline std::atomic<std::uint64_t> s_missingOutput{0};
        static inline std::atomic<std::uint64_t> s_disconnected{0};
        static inline std::atomic<std::uint64_t> s_late{0};
        static inline std::atomic<std::uint64_t> s_minLeadTicks{UINT64_MAX};
        static inline std::atomic<std::uint64_t> s_maxLeadTicks{0};

        struct Result
        {
            bool m_drop = false;
            std::uint64_t m_hostTicks = 0;
            double m_leadUs = 0;
        };

        static Result Plan(std::uint64_t timestampUs, double nowUs, double ticksPerSecond)
        {
            if (timestampUs == 0)
            {
                return {};
            }

            if (timestampUs > UINT64_MAX - x_latencyUs || !std::isfinite(nowUs)
                || !std::isfinite(ticksPerSecond) || ticksPerSecond <= 0)
            {
                return {true, 0, 0};
            }

            const double targetUs = static_cast<double>(timestampUs + x_latencyUs);
            const double ticks = targetUs * ticksPerSecond / 1000000.0;
            if (targetUs <= nowUs || !std::isfinite(ticks)
                || ticks >= static_cast<double>(UINT64_MAX))
            {
                return {true, 0, 0};
            }

            return {false, static_cast<std::uint64_t>(ticks), targetUs - nowUs};
        }

        struct Scope
        {
            std::uint64_t m_previous;

            explicit Scope(std::uint64_t deadline)
                : m_previous(s_deadline)
            {
                s_deadline = deadline;
            }

            ~Scope()
            {
                s_deadline = m_previous;
            }
        };

        static std::uint64_t Deadline()
        {
            return s_deadline;
        }

        static std::uint64_t Timestamp(std::uint64_t now)
        {
            return s_deadline == 0 ? now : s_deadline;
        }

        static bool DropLate(std::uint64_t now)
        {
            if (s_deadline == 0 || s_deadline > now)
            {
                return false;
            }

            s_late.fetch_add(1, std::memory_order_relaxed);
            return true;
        }

        static void RecordSubmit(std::int32_t status, std::uint64_t submittedAt)
        {
            if (status != 0)
            {
                s_errors.fetch_add(1, std::memory_order_relaxed);
            }

            if (s_deadline == 0)
            {
                s_immediate.fetch_add(1, std::memory_order_relaxed);
                return;
            }

            s_scheduled.fetch_add(1, std::memory_order_relaxed);
            const auto lead = s_deadline > submittedAt ? s_deadline - submittedAt : 0;
            auto minimum = s_minLeadTicks.load(std::memory_order_relaxed);
            while (lead < minimum && !s_minLeadTicks.compare_exchange_weak(minimum, lead, std::memory_order_relaxed))
            {
            }

            auto maximum = s_maxLeadTicks.load(std::memory_order_relaxed);
            while (lead > maximum && !s_maxLeadTicks.compare_exchange_weak(maximum, lead, std::memory_order_relaxed))
            {
            }
        }
    };
}
