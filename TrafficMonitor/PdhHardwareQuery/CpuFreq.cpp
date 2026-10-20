#include "stdafx.h"
#include "CpuFreq.h"
#include <powerbase.h>
#include <sysinfoapi.h>
#include <climits>


///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////

typedef struct _PROCESSOR_POWER_INFORMATION {
    ULONG Number;
    ULONG MaxMhz;
    ULONG CurrentMhz;
    ULONG MhzLimit;
    ULONG MaxIdleState;
    ULONG CurrentIdleState;
} PROCESSOR_POWER_INFORMATION, * PPROCESSOR_POWER_INFORMATION;

CPdhCpuFreq::CPdhCpuFreq()
    : CPdhQuery(_T("\\Processor Information(_Total)\\% Processor Performance"))
{
    //获取max_cpu_freq
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    auto ppInfo = std::vector<PROCESSOR_POWER_INFORMATION>(si.dwNumberOfProcessors);
    if (ppInfo.empty())
        return;
    const size_t bytes = sizeof(PROCESSOR_POWER_INFORMATION) * ppInfo.size();
    if (bytes > static_cast<size_t>(ULONG_MAX))
        return;
    const auto status = CallNtPowerInformation(POWER_INFORMATION_LEVEL::ProcessorInformation,
        nullptr, 0, ppInfo.data(), static_cast<ULONG>(bytes));
    if (status != 0)
        return;
    for (size_t i = 0; i < ppInfo.size(); i++)
    {
        max_cpu_freq = max(max_cpu_freq, ppInfo[i].MaxMhz / 1000.f);
    }
}

bool CPdhCpuFreq::GetCpuFreq(float& freq)
{
    double value{};
    if (QueryValue(value))
    {
        freq = static_cast<float>(value / 100.0 * max_cpu_freq);
        return true;
    }
    return false;
}
