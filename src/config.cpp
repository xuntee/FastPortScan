#include "config.h"

static void IniPath(wchar_t* buf, DWORD cch)
{
    GetModuleFileNameW(NULL, buf, cch);
    wchar_t* dot = wcsrchr(buf, L'.');
    if (dot) *dot = 0;
    wcscat_s(buf, cch, L".ini");
}

std::vector<HistoryItem> g_history;
std::vector<AdapterInfo> g_adapters;

wchar_t* IpToW(DWORD ip, wchar_t* buf, size_t cch)
{
    _snwprintf(buf, cch, L"%u.%u.%u.%u", (ip>>24)&255, (ip>>16)&255, (ip>>8)&255, ip&255);
    return buf;
}

void HistoryLoad()
{
    g_history.clear();
    wchar_t ini[MAX_PATH + 16]; IniPath(ini, MAX_PATH + 16);
    int n = GetPrivateProfileIntW(L"history", L"count", 0, ini);
    if (n > 20) n = 20;
    for (int i = 0; i < n; ++i)
    {
        wchar_t key[16], val[128];
        _snwprintf(key, 16, L"h%d", i);
        if (!GetPrivateProfileStringW(L"history", key, L"", val, 128, ini)) break;
        unsigned a,b,c,d,e,f,g,h;
        if (swscanf(val, L"%u.%u.%u.%u-%u.%u.%u.%u", &a,&b,&c,&d,&e,&f,&g,&h) == 8)
        {
            HistoryItem it;
            it.sip = (a<<24)|(b<<16)|(c<<8)|d;
            it.eip = (e<<24)|(f<<16)|(g<<8)|h;
            g_history.push_back(it);
        }
    }
}

static void HistorySave()
{
    wchar_t ini[MAX_PATH + 16]; IniPath(ini, MAX_PATH + 16);
    wchar_t n[16], key[16], val[128], a[24], b[24];
    _snwprintf(n, 16, L"%d", (int)g_history.size());
    WritePrivateProfileStringW(L"history", L"count", n, ini);
    for (size_t i = 0; i < g_history.size(); ++i)
    {
        _snwprintf(key, 16, L"h%d", (int)i);
        _snwprintf(val, 128, L"%s-%s",
                   IpToW(g_history[i].sip, a, 24), IpToW(g_history[i].eip, b, 24));
        WritePrivateProfileStringW(L"history", key, val, ini);
    }
}

void HistoryAdd(DWORD sip, DWORD eip)
{
    for (size_t i = 0; i < g_history.size(); ++i)
        if (g_history[i].sip == sip && g_history[i].eip == eip)
        {
            HistoryItem it = g_history[i];
            g_history.erase(g_history.begin() + i);
            g_history.insert(g_history.begin(), it);
            HistorySave();
            return;
        }
    HistoryItem it; it.sip = sip; it.eip = eip;
    g_history.insert(g_history.begin(), it);
    if (g_history.size() > 10) g_history.resize(10);
    HistorySave();
}

void SettingsSave()
{
    wchar_t ini[MAX_PATH + 16]; IniPath(ini, MAX_PATH + 16);
    wchar_t tmp[1000], a[24], b[24];
    DWORD sip = 0, eip = 0;
    SendDlgItemMessageW(g_hMain, IDC_IP_START, IPM_GETADDRESS, 0, (LPARAM)&sip);
    SendDlgItemMessageW(g_hMain, IDC_IP_END,   IPM_GETADDRESS, 0, (LPARAM)&eip);
    _snwprintf(tmp, 100, L"%s-%s", IpToW(sip, a, 24), IpToW(eip, b, 24));
    WritePrivateProfileStringW(L"settings", L"range", tmp, ini);
    GetDlgItemTextW(g_hMain, IDC_ED_PORTS, tmp, 1000);
    WritePrivateProfileStringW(L"settings", L"ports", tmp, ini);
    _snwprintf(tmp, 100, L"%u", GetDlgItemInt(g_hMain, IDC_ED_TIMEOUT, NULL, FALSE));
    WritePrivateProfileStringW(L"settings", L"timeout", tmp, ini);
    _snwprintf(tmp, 100, L"%u", GetDlgItemInt(g_hMain, IDC_ED_THREADS, NULL, FALSE));
    WritePrivateProfileStringW(L"settings", L"threads", tmp, ini);
}

void SettingsLoad()
{
    wchar_t ini[MAX_PATH + 16]; IniPath(ini, MAX_PATH + 16);
    wchar_t tmp[1000];
    wchar_t range[128];
    if (GetPrivateProfileStringW(L"settings", L"range", L"", range, 128, ini))
    {
        unsigned a,b,c,d,e,f,g,h;
        if (swscanf(range, L"%u.%u.%u.%u-%u.%u.%u.%u", &a,&b,&c,&d,&e,&f,&g,&h) == 8)
        {
            SendDlgItemMessageW(g_hMain, IDC_IP_START, IPM_SETADDRESS, 0,
                                (a<<24)|(b<<16)|(c<<8)|d);
            SendDlgItemMessageW(g_hMain, IDC_IP_END,   IPM_SETADDRESS, 0,
                                (e<<24)|(f<<16)|(g<<8)|h);
        }
    }
    if (GetPrivateProfileStringW(L"settings", L"ports", L"", tmp, 1000, ini))
        SetDlgItemTextW(g_hMain, IDC_ED_PORTS, tmp);
    int v = GetPrivateProfileIntW(L"settings", L"timeout", 0, ini);
    if (v) SetDlgItemInt(g_hMain, IDC_ED_TIMEOUT, v, FALSE);
    v = GetPrivateProfileIntW(L"settings", L"threads", 0, ini);
    if (v) SetDlgItemInt(g_hMain, IDC_ED_THREADS, v, FALSE);
}

// 枚举本机 IPv4 适配器（下拉框一键填段用）
void EnumAdapters(HWND hCombo)
{
    g_adapters.clear();
    SendMessageW(hCombo, CB_RESETCONTENT, 0, 0);
    ULONG sz = 16 * 1024;
    std::vector<char> buf(sz);
    auto aa = (IP_ADAPTER_ADDRESSES*)&buf[0];
    if (GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, aa, &sz) == ERROR_BUFFER_OVERFLOW)
    {
        buf.resize(sz); aa = (IP_ADAPTER_ADDRESSES*)&buf[0];
        if (GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, aa, &sz) != NO_ERROR) return;
    }
    wchar_t tmp[200], a[24];
    for (auto p = aa; p; p = p->Next)
        for (auto u = p->FirstUnicastAddress; u; u = u->Next)
        {
            if (u->Address.lpSockaddr->sa_family != AF_INET) continue;
            DWORD ip = ntohl(((sockaddr_in*)u->Address.lpSockaddr)->sin_addr.s_addr);
            if (ip == 0x7F000001) continue;                    // 跳过 127.0.0.1
            int prefix = u->OnLinkPrefixLength;
            if (prefix <= 0 || prefix > 32) prefix = 24;
            DWORD mask = prefix ? (0xFFFFFFFFu << (32 - prefix)) : 0;
            AdapterInfo ai;
            ai.ip = ip; ai.net = ip & mask; ai.bcast = ai.net | (~mask);
            _snwprintf(tmp, 200, L"%s  —  %s/%d",
                       p->FriendlyName ? p->FriendlyName : L"adapter",
                       IpToW(ip, a, 24), prefix);
            ai.label = tmp;
            int idx = (int)SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)tmp);
            SendMessageW(hCombo, CB_SETITEMDATA, idx, (LPARAM)g_adapters.size());
            g_adapters.push_back(ai);
        }
}
