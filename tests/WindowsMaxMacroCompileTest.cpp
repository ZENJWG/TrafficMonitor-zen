
#include <cstdint>
#include <limits>
#define max(a,b) DO_NOT_CALL_WINDOWS_MAX_MACRO
#include "../TrafficMonitor/TrafficRateMath.h"
static_assert(TrafficRateMath::Delta(5, 9) == 4);
static_assert(TrafficRateMath::Delta(9, 5) == 0);
int main(){return TrafficRateMath::SafeRate(10, 2.0) == 5 ? 0 : 1;}
