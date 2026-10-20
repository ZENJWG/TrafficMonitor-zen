#pragma once
#include <cstdint>
#include <limits>

namespace TrafficRateMath
{
    // Non-monotonic counters (adapter reset/removal) contribute no traffic.
    constexpr std::uint64_t Delta(std::uint64_t previous,
                                  std::uint64_t current) noexcept
    {
        return current < previous ? 0 : current - previous;
    }

    constexpr double BytesPerSecond(std::uint64_t delta, double seconds) noexcept
    {
        return seconds > 0.0 ? static_cast<double>(delta) / seconds : 0.0;
    }

    constexpr std::uint64_t SafeRate(std::uint64_t delta, double seconds) noexcept
    {
        const double rate = BytesPerSecond(delta, seconds);
        if (rate <= 0.0) return 0;
        if (rate >= static_cast<double>((std::numeric_limits<std::uint64_t>::max)()))
            return (std::numeric_limits<std::uint64_t>::max)();
        return static_cast<std::uint64_t>(rate);
    }
}
