#include "../TrafficMonitor/TrafficRateMath.h"
#include <cassert>
#include <cstdint>
#include <limits>
#include <iostream>

int main()
{
    using namespace TrafficRateMath;
    static_assert(Delta(100, 300) == 200);
    static_assert(Delta(300, 100) == 0);
    static_assert(Delta(0, 0) == 0);
    static_assert(Delta(4294967290ULL, 4294967390ULL) == 100);
    assert(SafeRate(1024, 1.0) == 1024);
    assert(SafeRate(1024, 0.5) == 2048);
    assert(SafeRate(1024, 0.0) == 0);
    assert(SafeRate(1024, -10.0) == 0);
    assert(SafeRate(std::numeric_limits<std::uint64_t>::max(), 0.01)
        == std::numeric_limits<std::uint64_t>::max());
    std::cout << "TrafficRateMath tests passed\n";
}
