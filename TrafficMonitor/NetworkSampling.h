#pragma once
// Windows IP Helper API wrappers. No partially filled table is ever published.
#include "TrafficRateMath.h"
#include <iphlpapi.h>
#include <netioapi.h>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <vector>
#include <unordered_set>

namespace NetworkSampling
{
    inline bool RefreshLegacyTable(MIB_IFTABLE*& table, DWORD& capacity)
    {
        // Retain the last complete snapshot on failure; never expose a half-filled buffer.
        if (table == nullptr)
        {
            capacity = sizeof(MIB_IFTABLE);
            table = static_cast<MIB_IFTABLE*>(std::malloc(capacity));
            if (!table) { capacity = 0; return false; }
        }
        for (int attempt = 0; attempt < 4; ++attempt)
        {
            DWORD size = capacity;
            const DWORD result = ::GetIfTable(table, &size, FALSE);
            if (result == NO_ERROR)
                return true;
            if (result != ERROR_INSUFFICIENT_BUFFER || size <= capacity ||
                size > 8 * 1024 * 1024)
                return false;
            void* resized = std::malloc(size);
            if (!resized)
                return false;
            std::free(table);
            table = static_cast<MIB_IFTABLE*>(resized);
            capacity = size;
        }
        return false;
    }

    struct Counters
    {
        std::uint64_t received = 0;
        std::uint64_t sent = 0;
    };

    inline bool Read64BitCounters(const std::vector<NET_IFINDEX>& interfaceIndices,
                                  Counters& counters)
    {
        if (interfaceIndices.empty())
            return false;
        PMIB_IF_TABLE2 raw = nullptr;
        if (::GetIfTable2(&raw) != NO_ERROR || !raw)
            return false;
        std::unique_ptr<MIB_IF_TABLE2, decltype(&::FreeMibTable)> table(raw, &::FreeMibTable);
        std::unordered_set<NET_IFINDEX> wanted;
        for (NET_IFINDEX interfaceIndex : interfaceIndices)
        {
            if (interfaceIndex != 0)
                wanted.insert(interfaceIndex);
        }
        if (wanted.empty())
            return false;
        std::size_t matches = 0;
        for (ULONG i = 0; i < table->NumEntries; ++i)
        {
            const auto& entry = table->Table[i];
            if (wanted.erase(entry.InterfaceIndex))
            {
                counters.received += entry.InOctets;
                counters.sent += entry.OutOctets;
                ++matches;
            }
        }
        // Missing interfaces mean adapter enumeration is stale. Do not publish
        // a partial aggregate and introduce artificial counter resets.
        return matches != 0 && wanted.empty();
    }

}
