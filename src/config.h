// 配置持久化、IP 段历史、网卡枚举
#pragma once
#include "common.h"

void HistoryLoad();
void HistoryAdd(DWORD sip, DWORD eip);
void SettingsLoad();
void SettingsSave();
void EnumAdapters(HWND hCombo);
wchar_t* IpToW(DWORD ip, wchar_t* buf, size_t cch);

struct HistoryItem { DWORD sip, eip; };
extern std::vector<HistoryItem> g_history;

struct AdapterInfo { DWORD ip, net, bcast; std::wstring label; };
extern std::vector<AdapterInfo> g_adapters;
