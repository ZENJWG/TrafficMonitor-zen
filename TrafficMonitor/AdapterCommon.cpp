#include "stdafx.h"
#include "AdapterCommon.h"
#include <vector>
#include <utility>
#include <algorithm>
#include <unordered_map>


CAdapterCommon::CAdapterCommon()
{
}


CAdapterCommon::~CAdapterCommon()
{
}

void CAdapterCommon::GetAdapterInfo(vector<NetWorkConection>& adapters)
{
    adapters.clear();
    ULONG bytes = 0;
    // GetAdaptersInfo is retained for compatibility with the existing
    // description matching and IPv4 display. Manage buffers with RAII.
    DWORD result = ::GetAdaptersInfo(nullptr, &bytes);
    if (result == ERROR_BUFFER_OVERFLOW && bytes > 0 && bytes < 8 * 1024 * 1024)
    {
        std::vector<BYTE> buffer(bytes);
        result = ::GetAdaptersInfo(
            reinterpret_cast<PIP_ADAPTER_INFO>(buffer.data()), &bytes);
        if (result == NO_ERROR)
        {
            for (auto* current = reinterpret_cast<PIP_ADAPTER_INFO>(buffer.data());
                 current != nullptr; current = current->Next)
            {
                NetWorkConection connection{};
                connection.description = current->Description;
                connection.ip_address = CCommon::StrToUnicode(current->IpAddressList.IpAddress.String);
                connection.subnet_mask = CCommon::StrToUnicode(current->IpAddressList.IpMask.String);
                connection.default_gateway = CCommon::StrToUnicode(current->GatewayList.IpAddress.String);
                adapters.push_back(std::move(connection));
            }
        }
    }
    if (adapters.empty())
    {
        NetWorkConection connection{};
        connection.description = CCommon::UnicodeToStr(CCommon::LoadText(L"<", IDS_NO_CONNECTION, L">"));
        adapters.push_back(connection);
    }
}

void CAdapterCommon::RefreshIpAddress(vector<NetWorkConection>& adapters)
{
    vector<NetWorkConection> refreshed;
    GetAdapterInfo(refreshed);
    // Descriptions are kept as compatibility keys for persisted settings.
    // Last match wins, matching the previous nested-loop behavior.
    std::unordered_map<string, const NetWorkConection*> byDescription;
    for (const auto& item : refreshed)
        byDescription[item.description] = &item;

    for (auto& adapter : adapters)
    {
        auto match = byDescription.find(adapter.description);
        if (match == byDescription.end())
            continue;
        adapter.ip_address = match->second->ip_address;
        adapter.subnet_mask = match->second->subnet_mask;
        adapter.default_gateway = match->second->default_gateway;
    }
}

void CAdapterCommon::GetIfTableInfo(vector<NetWorkConection>& adapters, MIB_IFTABLE* pIfTable)
{
	//依次在IfTable里查找每个连接
	if (!pIfTable) return;
	for (size_t i{}; i < adapters.size(); i++)
	{
		adapters[i].index = -1;
		if (adapters[i].description.empty())
			continue;
		int index;
		index = FindConnectionInIfTable(adapters[i].description, pIfTable);
		if (index == -1)		//如果使用精确匹配的方式没有找到，则采用模糊匹配的方式再查找一次
			index = FindConnectionInIfTableFuzzy(adapters[i].description, pIfTable);
		if (index < 0 || static_cast<DWORD>(index) >= pIfTable->dwNumEntries)
			continue;
		adapters[i].index = index;
        adapters[i].interface_index = pIfTable->table[index].dwIndex;
		adapters[i].in_bytes = pIfTable->table[index].dwInOctets;
		adapters[i].out_bytes = pIfTable->table[index].dwOutOctets;
		adapters[i].description_2 = std::string(
            reinterpret_cast<const char*>(pIfTable->table[index].bDescr),
            std::min<DWORD>(pIfTable->table[index].dwDescrLen, MAXLEN_IFDESCR));
	}
}

void CAdapterCommon::GetAllIfTableInfo(vector<NetWorkConection>& adapters, MIB_IFTABLE * pIfTable)
{
	vector<NetWorkConection> adapters_tmp;
	GetAdapterInfo(adapters_tmp);		//获取IP地址
	adapters.clear();
	if (!pIfTable) return;
	for (size_t i{}; i < pIfTable->dwNumEntries; i++)
	{
		NetWorkConection connection;
		connection.description = connection.description_2 = std::string(
            reinterpret_cast<const char*>(pIfTable->table[i].bDescr),
            std::min<DWORD>(pIfTable->table[i].dwDescrLen, MAXLEN_IFDESCR));
		connection.index = static_cast<int>(i);
        connection.interface_index = pIfTable->table[i].dwIndex;
		connection.in_bytes = pIfTable->table[i].dwInOctets;
		connection.out_bytes = pIfTable->table[i].dwOutOctets;
		for (size_t j{}; j < adapters_tmp.size(); j++)
		{
			if (connection.description.find(adapters_tmp[j].description) != string::npos)
			{
				connection.ip_address = adapters_tmp[j].ip_address;
				connection.subnet_mask = adapters_tmp[j].subnet_mask;
				connection.default_gateway = adapters_tmp[j].default_gateway;
				break;
			}
		}
		adapters.push_back(connection);
	}
}

int CAdapterCommon::FindConnectionInIfTable(string connection, MIB_IFTABLE* pIfTable)
{
	if (!pIfTable) return -1;
	for (size_t i{}; i < pIfTable->dwNumEntries; i++)
	{
		string descr(reinterpret_cast<const char*>(pIfTable->table[i].bDescr),
            std::min<DWORD>(pIfTable->table[i].dwDescrLen, MAXLEN_IFDESCR));
		if (descr == connection && i <= static_cast<size_t>(INT_MAX))
			return static_cast<int>(i);
	}
	return -1;
}

int CAdapterCommon::FindConnectionInIfTableFuzzy(string connection, MIB_IFTABLE* pIfTable)
{
	if (!pIfTable) return -1;
	for (size_t i{}; i < pIfTable->dwNumEntries; i++)
	{
		string descr(reinterpret_cast<const char*>(pIfTable->table[i].bDescr),
            std::min<DWORD>(pIfTable->table[i].dwDescrLen, MAXLEN_IFDESCR));
		size_t index;
		//在较长的字符串里查找较短的字符串
		if (descr.size() >= connection.size())
			index = descr.find(connection);
		else
			index = connection.find(descr);
		if (index != string::npos && i <= static_cast<size_t>(INT_MAX))
			return static_cast<int>(i);
	}
	//如果还是没有找到，则使用字符串匹配算法查找
	double max_degree{};
	int best_index{ -1 };
	if (!pIfTable) return -1;
	for (size_t i{}; i < pIfTable->dwNumEntries; i++)
	{
		string descr(reinterpret_cast<const char*>(pIfTable->table[i].bDescr),
            std::min<DWORD>(pIfTable->table[i].dwDescrLen, MAXLEN_IFDESCR));
		double degree = CCommon::StringSimilarDegree_LD(descr, connection);
		if (degree > max_degree)
		{
			max_degree = degree;
			best_index = i <= static_cast<size_t>(INT_MAX) ? static_cast<int>(i) : -1;
		}
	}
	return best_index;
}
