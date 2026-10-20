#pragma once
#include "Common.h"

//����һ������������Ϣ
struct NetWorkConection
{
	int index{ -1 };
	DWORD interface_index{};			//��������MIB_IFTABLE�е�����
	string description;		//������������ȡ��GetAdapterInfo��
	string description_2;	//������������ȡ��GetIfTable��
	unsigned int in_bytes{};	//��ʼʱ�ѽ����ֽ���
	unsigned int out_bytes{};	//��ʼʱ�ѷ����ֽ���
	wstring ip_address{ L"-.-.-.-" };	//IP��ַ
	wstring subnet_mask{ L"-.-.-.-" };	//��������
	wstring default_gateway{ L"-.-.-.-" };	//Ĭ������
};

class CAdapterCommon
{
public:
	CAdapterCommon();
	~CAdapterCommon();

	//��ȡ���������б����������������IP��ַ���������롢Ĭ��������Ϣ
	static void GetAdapterInfo(vector<NetWorkConection>& adapters);

	//ˢ�����������б��е�IP��ַ���������롢Ĭ��������Ϣ
	static void RefreshIpAddress(vector<NetWorkConection>& adapters);

	//��ȡ�����б���ÿ���������ӵ�MIB_IFTABLE�е���������ʼʱ�ѽ���/�����ֽ�������Ϣ
	static void GetIfTableInfo(vector<NetWorkConection>& adapters, MIB_IFTABLE* pIfTable);

	//ֱ�ӽ�MIB_IFTABLE�е������������ӵ�adapters������
	static void GetAllIfTableInfo(vector<NetWorkConection>& adapters, MIB_IFTABLE* pIfTable);
private:
	//����һ���������������ж��Ƿ���IfTable�б�������������Ҳ����򷵻�-1
	static int FindConnectionInIfTable(string connection, MIB_IFTABLE* pIfTable);

	//����һ���������������ж��Ƿ���IfTable���б�������������Ҳ����򷵻�-1��ֻ��Ҫ����ƥ��
	static int FindConnectionInIfTableFuzzy(string connection, MIB_IFTABLE* pIfTable);
};

