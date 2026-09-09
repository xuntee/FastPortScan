#include "scan.h"

// ---------------------------------------------------------------------------
// 端口区间表 + 任务迭代器（借鉴原版扫描对象）
// ---------------------------------------------------------------------------
struct PortRange { DWORD lo, hi; PortRange* next; };

struct Scanner {
    DWORD       ipStart, ipEnd;     // 主机序（0xC0A80101 == 192.168.1.1）
    PortRange*  ranges = NULL;
    DWORD       curIP = 0;
    PortRange*  curNode = NULL;
    DWORD       curPort = 0;

    bool Parse(DWORD s, DWORD e, const char* ports)
    {
        ipStart = s; ipEnd = e; ranges = NULL;
        curIP = s; curNode = NULL; curPort = 0;
        char* buf = _strdup(ports);
        if (!buf) return false;
        PortRange** tail = &ranges;
        char* ctx = NULL;
        for (char* t = strtok_s(buf, ",", &ctx); t; t = strtok_s(NULL, ",", &ctx))
        {
            while (*t == ' ' || *t == '\t') t++;
            if (!*t) continue;
            DWORD lo, hi;
            char* dash = strchr(t, '-');
            if (dash) { *dash = 0; lo = (DWORD)strtoul(t, NULL, 10); hi = (DWORD)strtoul(dash+1, NULL, 10); }
            else      { lo = hi = (DWORD)strtoul(t, NULL, 10); }
            if (!lo || !hi || lo > 65535 || hi > 65535 || lo > hi) { free(buf); Clear(); return false; }
            PortRange* p = (PortRange*)HeapAlloc(GetProcessHeap(), 0, sizeof(PortRange));
            p->lo = lo; p->hi = hi; p->next = NULL;
            *tail = p; tail = &p->next;
        }
        free(buf);
        if (ranges) { curNode = ranges; curPort = ranges->lo - 1; }   // 迭代器就位（同原版 0x4011F1）
        return ranges != NULL;
    }
    void Clear()
    {
        PortRange* p = ranges;
        while (p) { PortRange* n = p->next; HeapFree(GetProcessHeap(), 0, p); p = n; }
        ranges = NULL; curNode = NULL; curPort = 0; curIP = ipStart;
    }
    // 顺序遍历 [ipStart..ipEnd] × 区间表；无任务返回 false
    bool Next(DWORD* ip, DWORD* port)
    {
        if (!curNode) return false;
        DWORD p = curPort + 1;
        if (p > curNode->hi)
        {
            if (curNode->next) { curNode = curNode->next; p = curNode->lo; }
            else
            {
                if (curIP >= ipEnd) { curNode = NULL; return false; }
                curIP++; curNode = ranges; p = ranges->lo;
            }
        }
        curPort = p; *ip = curIP; *port = p;
        return true;
    }
    DWORD TotalJobs() const
    {
        DWORD ports = 0;
        for (PortRange* p = ranges; p; p = p->next) ports += p->hi - p->lo + 1;
        if (ipEnd < ipStart) return 0;
        return ports * (ipEnd - ipStart + 1);
    }
};

static Scanner          g_scan;
static CRITICAL_SECTION g_scanCS;       // 任务迭代互斥
static CRITICAL_SECTION g_resultCS;     // 结果追加互斥
static INIT_ONCE        g_csOnce = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK InitCS(PINIT_ONCE, PVOID, PVOID*)
{
    InitializeCriticalSection(&g_scanCS);
    InitializeCriticalSection(&g_resultCS);
    return TRUE;
}
static void EnsureCS()                  // 必须在任何 EnterCriticalSection 之前
{
    PVOID ctx;
    InitOnceExecuteOnce(&g_csOnce, InitCS, NULL, NULL);
}
static volatile LONG    g_running = 0;  // 扫描运行标志
static volatile LONG    g_done = 0;     // 已完成任务
static volatile LONG    g_openCnt = 0;  // 开放端口计数
static DWORD            g_totalJobs = 0;
static DWORD            g_timeoutMs = 200;

bool ScanIsRunning() { return g_running != 0; }
void ScanStop()      { g_running = 0; }
DWORD ScanTimeoutMs(){ return g_timeoutMs; }

// ---------------------------------------------------------------------------
// 核心探测：非阻塞 connect + select 毫秒超时（借鉴原版，这是速度的来源）
// ---------------------------------------------------------------------------
static int TestPort(DWORD ipHost, DWORD port, DWORD timeoutMs)
{
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) return 0;

    u_long nb = 1;
    ioctlsocket(s, FIONBIO, &nb);                       // 非阻塞，connect 不等待

    sockaddr_in sa; memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons((u_short)port);
    sa.sin_addr.s_addr = htonl(ipHost);

    int open = 0;
    int r = connect(s, (sockaddr*)&sa, sizeof(sa));
    if (r == 0)
        open = 1;                                       // 本机/同网段大多立即成功
    else if (WSAGetLastError() == WSAEWOULDBLOCK)
    {
        fd_set wset; FD_ZERO(&wset); FD_SET(s, &wset);
        timeval tv; tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;
        if (select(0, NULL, &wset, NULL, &tv) > 0) open = 1;
    }
    closesocket(s);
    return open;
}

// 结果追加到多行编辑框（原版 0x401890 同款：GETTEXTLENGTH + SETSEL + REPLACESEL）
static void AppendResult(HWND hEdit, const wchar_t* text)
{
    EnterCriticalSection(&g_resultCS);
    LONG len = (LONG)SendMessageW(hEdit, WM_GETTEXTLENGTH, 0, 0);
    SendMessageW(hEdit, EM_SETSEL, len, len);
    SendMessageW(hEdit, EM_REPLACESEL, 0, (LPARAM)text);
    SendMessageW(hEdit, EM_SCROLLCARET, 0, 0);
    LeaveCriticalSection(&g_resultCS);
}

// 状态栏 + 进度条刷新（百分比变化才写控件，避免高频 SendMessage 拖慢扫描）
static void UpdateStatus(bool force)
{
    static int s_lastPct = -1;
    DWORD done = (DWORD)g_done;
    int pct = g_totalJobs ? (int)(done * 100 / g_totalJobs) : 0;
    SendMessageW(GetDlgItem(g_hMain, IDC_PROGRESS), PBM_SETPOS, pct, 0);
    if (pct == s_lastPct && !force) return;
    s_lastPct = pct;
    ULONGLONG elapsed = GetTickCount64() - g_tickStart;
    wchar_t used[16], left[16];
    FormatHMS(elapsed, used, 16);
    if (done > 0)
        FormatHMS(elapsed * (g_totalJobs - done) / done, left, 16);   // 线性估算剩余
    else
        _snwprintf(left, 16, L"--:--:--");
    wchar_t buf[220];
    _snwprintf(buf, 220, L"进度 %d%%  ·  已完成 %u / %u  ·  开放 %u  ·  已用 %s  ·  剩余约 %s",
               pct, done, g_totalJobs, (unsigned)g_openCnt, used, left);
    SetDlgItemTextW(g_hMain, IDC_ST_STATUS, buf);
}

// ---------------------------------------------------------------------------
// 工作线程：领任务 → 探测 → 回填
// ---------------------------------------------------------------------------
static DWORD WINAPI ScanWorker(LPVOID)
{
    HWND hResult = GetDlgItem(g_hMain, IDC_ED_RESULT);
    DWORD ip, port;
    while (g_running)
    {
        EnterCriticalSection(&g_scanCS);
        bool has = g_scan.Next(&ip, &port);
        LeaveCriticalSection(&g_scanCS);
        if (!has) break;

        if (TestPort(ip, port, g_timeoutMs))
        {
            wchar_t line[64];
            _snwprintf(line, 64, L"%u.%u.%u.%u: %u\r\n",
                       (ip >> 24) & 255, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255, port);
            AppendResult(hResult, line);
            InterlockedIncrement(&g_openCnt);
        }
        InterlockedIncrement(&g_done);
        UpdateStatus(false);
    }
    return 0;
}

// 分批等待全部线程结束（WaitForMultipleObjects 单次最多 64 句柄）
static void WaitAll(std::vector<HANDLE>& hs)
{
    DWORD off = 0;
    while (off < hs.size())
    {
        DWORD cnt = (DWORD)hs.size() - off;
        if (cnt > 64) cnt = 64;
        WaitForMultipleObjects(cnt, &hs[off], TRUE, INFINITE);
        off += cnt;
    }
}

// ---------------------------------------------------------------------------
// 主扫描线程：换按钮 → 拉起工作线程 → 等待 → 恢复 UI
// ---------------------------------------------------------------------------
static DWORD WINAPI ScanMain(LPVOID)
{
    g_running = 1;
    g_tickStart = GetTickCount64();
    SetDlgItemTextW(g_hMain, IDC_BTN_SCAN, K_STR_STOP);

    SendMessageW(GetDlgItem(g_hMain, IDC_PROGRESS), PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageW(GetDlgItem(g_hMain, IDC_PROGRESS), PBM_SETPOS, 0, 0);

    EnterCriticalSection(&g_scanCS);
    g_totalJobs = g_scan.TotalJobs();
    LeaveCriticalSection(&g_scanCS);
    g_done = 0; g_openCnt = 0;
    UpdateStatus(true);

    DWORD nThreads = GetDlgItemInt(g_hMain, IDC_ED_THREADS, NULL, FALSE);
    if (nThreads < THREADS_MIN || nThreads > THREADS_MAX) nThreads = 10;

    std::vector<HANDLE> hs;
    for (DWORD i = 0; i < nThreads; ++i)
    {
        DWORD id;
        HANDLE h = CreateThread(NULL, 0, ScanWorker, NULL, 0, &id);
        if (!h) break;
        SetThreadPriority(h, THREAD_PRIORITY_BELOW_NORMAL);     // 不抢 UI（同原版）
        hs.push_back(h);
    }
    if (!hs.empty()) WaitAll(hs);
    for (HANDLE h : hs) CloseHandle(h);

    SendMessageW(GetDlgItem(g_hMain, IDC_PROGRESS), PBM_SETPOS, 100, 0);
    SetDlgItemTextW(g_hMain, IDC_BTN_SCAN, K_STR_SCAN);
    g_running = 0;

    ULONGLONG total = GetTickCount64() - g_tickStart;
    wchar_t used[16];
    FormatHMS(total, used, 16);
    wchar_t buf[220];
    _snwprintf(buf, 220, L"扫描完成  ·  共 %u 项  ·  开放 %u  ·  总耗时 %s",
               g_totalJobs, (unsigned)g_openCnt, used);
    SetDlgItemTextW(g_hMain, IDC_ST_STATUS, buf);
    return 0;
}

// ---------------------------------------------------------------------------
// 开始扫描：校验（沿用原版提示文案）→ 清屏询问 → 起线程
// ---------------------------------------------------------------------------
bool ScanStart(HWND hDlg)
{
    EnsureCS();
    DWORD sip, eip;
    if (SendDlgItemMessageW(hDlg, IDC_IP_START, IPM_GETADDRESS, 0, (LPARAM)&sip) != 4 ||
        SendDlgItemMessageW(hDlg, IDC_IP_END,   IPM_GETADDRESS, 0, (LPARAM)&eip) != 4)
    {
        MessageBoxW(hDlg, K_ERR_IP, K_APP_NAME, MB_ICONWARNING);
        return false;
    }
    if (eip < sip) { DWORD t = sip; sip = eip; eip = t; }        // 自动交换大小

    wchar_t portsW[1000];
    GetDlgItemTextW(hDlg, IDC_ED_PORTS, portsW, 1000);
    char portsA[2000];
    WideCharToMultiByte(CP_ACP, 0, portsW, -1, portsA, 2000, NULL, NULL);

    if (!g_scan.Parse(sip, eip, portsA))
    {
        MessageBoxW(hDlg, K_ERR_PORT, K_APP_NAME, MB_ICONWARNING);
        return false;
    }
    DWORD timeout = GetDlgItemInt(hDlg, IDC_ED_TIMEOUT, NULL, FALSE);
    if (!timeout || timeout > TIMEOUT_MAX)
    {
        MessageBoxW(hDlg, K_ERR_TIMEOUT, K_APP_NAME, MB_ICONWARNING);
        return false;
    }
    g_timeoutMs = timeout;
    DWORD threads = GetDlgItemInt(hDlg, IDC_ED_THREADS, NULL, FALSE);
    if ((LONG)threads <= 0 || threads > THREADS_MAX)
    {
        MessageBoxW(hDlg, K_ERR_THREADS, K_APP_NAME, MB_ICONWARNING);
        return false;
    }

    HistoryAdd(sip, eip);
    SettingsSave();

    HWND hResult = GetDlgItem(hDlg, IDC_ED_RESULT);
    if (GetWindowTextLengthW(hResult) > 0 &&
        MessageBoxW(hDlg, K_ASK_CLEAR, K_APP_NAME, MB_ICONQUESTION | MB_YESNO) == IDYES)
        SetWindowTextW(hResult, L"");

    DWORD id;
    HANDLE h = CreateThread(NULL, 0, ScanMain, NULL, 0, &id);
    if (h) { CloseHandle(h); return true; }
    return false;
}
