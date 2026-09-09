// 主窗口：创建控件、消息处理、入口
#include "common.h"
#include "scan.h"
#include "config.h"
#include "about.h"

// ---------------------------------------------------------------------------
// 结果框子类化：单击复制该行，双击用默认浏览器打开 ip:port
// ---------------------------------------------------------------------------
static bool GetLineUnderCursor(HWND hEdit, wchar_t* buf, size_t cch)
{
    DWORD lparam = GetMessagePos();
    POINT pt = { (short)LOWORD(lparam), (short)HIWORD(lparam) };
    ScreenToClient(hEdit, &pt);
    DWORD ch = (DWORD)SendMessageW(hEdit, EM_CHARFROMPOS, 0, (LPARAM)&pt);
    DWORD line = (DWORD)SendMessageW(hEdit, EM_LINEFROMCHAR, (WPARAM)ch, 0);
    if (cch > 0xFFFF) cch = 0xFFFF;
    *(WORD*)buf = (WORD)(cch - 1);
    DWORD n = (DWORD)SendMessageW(hEdit, EM_GETLINE, (WPARAM)line, (LPARAM)buf);
    buf[n] = 0;
    return n > 0;
}

static LRESULT CALLBACK ResultProc(HWND h, UINT m, WPARAM w, LPARAM l,
                                   UINT_PTR, DWORD_PTR)
{
    if (m == WM_LBUTTONUP)                                  // 单击：复制该行
    {
        wchar_t line[128];
        if (GetLineUnderCursor(h, line, 128))
        {
            CopyTextToClipboard(GetParent(h), line);
            wchar_t fb[160];
            _snwprintf(fb, 160, L"已复制: %s", line);
            SetDlgItemTextW(g_hMain, IDC_ST_STATUS, fb);
        }
    }
    else if (m == WM_LBUTTONDBLCLK)                         // 双击：浏览器打开
    {
        wchar_t line[128];
        if (GetLineUnderCursor(h, line, 128))
        {
            const wchar_t* colon = wcsrchr(line, L':');
            if (colon)
            {
                wchar_t host[64], url[160];
                int hostlen = (int)(colon - line);
                int ci = 0;
                for (int i = 0; i < hostlen; ++i)
                    if (line[i] != L' ') host[ci++] = line[i];
                host[ci] = 0;
                int port = _wtoi(colon + 1);
                if (ci > 0 && port > 0)
                {
                    if (port == 443)
                        _snwprintf(url, 160, L"https://%s", host);
                    else
                        _snwprintf(url, 160, L"http://%s:%d", host, port);
                    ShellExecuteW(h, L"open", url, NULL, NULL, SW_SHOWNORMAL);
                }
            }
        }
        return 0;                                           // 屏蔽默认选词
    }
    return DefSubclassProc(h, m, w, l);
}

// ---------------------------------------------------------------------------
// 主窗口过程
// ---------------------------------------------------------------------------
static LRESULT CALLBACK MainProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_CREATE:
        g_hMain = hDlg;
        HistoryLoad();
        SettingsLoad();
        return 0;

    case WM_COMMAND:
    {
        WORD id = LOWORD(wp), note = HIWORD(wp);
        if (id == IDC_BTN_SCAN)                             // 扫描 / 停止
        {
            if (ScanIsRunning())
            {
                ScanStop();
                EnableWindow(GetDlgItem(hDlg, IDC_BTN_SCAN), FALSE);
                SetDlgItemTextW(hDlg, IDC_ST_STATUS, L"正在停止...");
            }
            else ScanStart(hDlg);
            return 0;
        }
        if (id == 2) { SendMessageW(hDlg, WM_CLOSE, 0, 0); return 0; }
        if (id == 4) { ShowAbout(hDlg); return 0; }
        if (id == IDC_BTN_COPYALL)                          // 复制全部结果
        {
            HWND hRes = GetDlgItem(hDlg, IDC_ED_RESULT);
            LONG len = (LONG)GetWindowTextLengthW(hRes) + 1;
            wchar_t* txt = (wchar_t*)HeapAlloc(GetProcessHeap(), 0, len * sizeof(wchar_t));
            if (txt)
            {
                GetWindowTextW(hRes, txt, len);
                if (len > 1) CopyTextToClipboard(hDlg, txt);
                else SetDlgItemTextW(hDlg, IDC_ST_STATUS, L"结果为空，没有可复制的内容");
                HeapFree(GetProcessHeap(), 0, txt);
            }
            return 0;
        }
        if (id == IDC_IP_END && note == EN_CHANGE)          // 结束IP清空 → 复制起始IP
        {
            DWORD addr;
            if (SendDlgItemMessageW(hDlg, IDC_IP_END, IPM_GETADDRESS, 0, (LPARAM)&addr) == 0)
            {
                DWORD sip;
                if (SendDlgItemMessageW(hDlg, IDC_IP_START, IPM_GETADDRESS, 0, (LPARAM)&sip) == 4)
                    SendDlgItemMessageW(hDlg, IDC_IP_END, IPM_SETADDRESS, 0, sip);
            }
            return 0;
        }
        if (id == IDC_CB_ADAPTER && note == CBN_SELCHANGE)  // 一键填本机网段
        {
            int sel = (int)SendDlgItemMessageW(hDlg, IDC_CB_ADAPTER, CB_GETCURSEL, 0, 0);
            if (sel >= 0)
            {
                int di = (int)SendDlgItemMessageW(hDlg, IDC_CB_ADAPTER, CB_GETITEMDATA, sel, 0);
                if (di >= 0 && di < (int)g_adapters.size())
                {
                    SendDlgItemMessageW(hDlg, IDC_IP_START, IPM_SETADDRESS, 0, g_adapters[di].net);
                    SendDlgItemMessageW(hDlg, IDC_IP_END,   IPM_SETADDRESS, 0, g_adapters[di].bcast);
                }
            }
            return 0;
        }
        if (id == IDC_BTN_HISTORY && note == BN_CLICKED)    // 快速 IP 下拉
        {
            HMENU menu = CreatePopupMenu();
            wchar_t a[24], b[24], txt[64];
            for (size_t i = 0; i < g_history.size(); ++i)
            {
                _snwprintf(txt, 64, L"%s - %s",
                           IpToW(g_history[i].sip, a, 24), IpToW(g_history[i].eip, b, 24));
                AppendMenuW(menu, MF_STRING, 0x2000 + (UINT)i, txt);
            }
            if (!g_history.empty()) AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(menu, MF_STRING, 0x2FFF, L"清空历史记录");
            RECT r; GetWindowRect(GetDlgItem(hDlg, IDC_BTN_HISTORY), &r);
            UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
                                      r.left, r.bottom, 0, hDlg, NULL);
            DestroyMenu(menu);
            if (cmd == 0x2FFF) { g_history.clear(); }
            else if (cmd >= 0x2000 && cmd < 0x2000 + g_history.size())
            {
                HistoryItem& it = g_history[cmd - 0x2000];
                SendDlgItemMessageW(hDlg, IDC_IP_START, IPM_SETADDRESS, 0, it.sip);
                SendDlgItemMessageW(hDlg, IDC_IP_END,   IPM_SETADDRESS, 0, it.eip);
            }
            return 0;
        }
        return 0;
    }

    case WM_CLOSE:
        if (ScanIsRunning())
        {
            if (MessageBoxW(hDlg, L"扫描进行中，确定停止并退出？", K_APP_NAME,
                            MB_ICONQUESTION | MB_YESNO) != IDYES)
                return 0;
            ScanStop();
            Sleep(300);                                     // 给工作线程退出时间
        }
        DestroyWindow(hDlg);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hDlg, msg, wp, lp);
}

// ---------------------------------------------------------------------------
// 入口：InitCommonControlsEx → WSAStartup → 主窗口消息循环 → WSACleanup
// ---------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nCmdShow)
{
    InstallCrashLogger();

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_INTERNET_CLASSES | ICC_PROGRESS_CLASS |
                                          ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

    g_hFont = CreateFontW(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, GB2312_CHARSET,
                          0, 0, CLEARTYPE_QUALITY, 0, L"Microsoft YaHei UI");
    g_hFontBold = CreateFontW(-16, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, GB2312_CHARSET,
                              0, 0, CLEARTYPE_QUALITY, 0, L"Microsoft YaHei UI");

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = MainProc;
    wc.hInstance = hInst;
    wc.hIcon = LoadAppIcon(GetSystemMetrics(SM_CXICON));
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"FastPortScanMain";
    RegisterClassW(&wc);

    RECT wa; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int ww = 660, wh = 448;     // 424 时状态栏 y=388 被客户区截掉，加高露出进度+状态两行
    int wx = wa.left + ((wa.right - wa.left) - ww) / 2;
    int wy = wa.top + ((wa.bottom - wa.top) - wh) / 2;

    HWND mainWnd = CreateWindowExW(0, L"FastPortScanMain", K_TITLE,
                                   WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                   wx, wy, ww, wh, NULL, NULL, hInst, NULL);

    // ---- 左侧：信息设置 ----
    MkChild(mainWnd, L"BUTTON", L"信息设置", BS_GROUPBOX, 0, 12, 10, 260, 300, 0);
    MkChild(mainWnd, L"STATIC", L"网卡:",  SS_RIGHT, 0, 26, 48, 52, 18, 0);
    HWND hCombo = MkChild(mainWnd, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
                          0, 82, 44, 178, 200, IDC_CB_ADAPTER);
    MkChild(mainWnd, L"STATIC", L"起始IP:", SS_RIGHT, 0, 26, 92, 52, 18, 0);
    MkChild(mainWnd, L"SysIPAddress32", L"", WS_TABSTOP, WS_EX_CLIENTEDGE,
            82, 88, 138, 24, IDC_IP_START);
    MkChild(mainWnd, L"BUTTON", L"▼", BS_PUSHBUTTON | WS_TABSTOP, 0,
            226, 88, 30, 24, IDC_BTN_HISTORY);
    MkChild(mainWnd, L"STATIC", L"结束IP:", SS_RIGHT, 0, 26, 132, 52, 18, 0);
    MkChild(mainWnd, L"SysIPAddress32", L"", WS_TABSTOP, WS_EX_CLIENTEDGE,
            82, 128, 138, 24, IDC_IP_END);
    MkChild(mainWnd, L"STATIC", L"端口号:", SS_RIGHT, 0, 26, 172, 52, 18, 0);
    MkChild(mainWnd, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE,
            82, 168, 174, 24, IDC_ED_PORTS);
    MkChild(mainWnd, L"STATIC", L"超　时:", SS_RIGHT, 0, 26, 212, 52, 18, 0);
    MkChild(mainWnd, L"EDIT", L"", ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE,
            82, 208, 80, 24, IDC_ED_TIMEOUT);
    MkChild(mainWnd, L"STATIC", L"毫秒", SS_LEFT, 0, 168, 212, 40, 18, 0);
    MkChild(mainWnd, L"STATIC", L"线程数:", SS_RIGHT, 0, 26, 252, 52, 18, 0);
    MkChild(mainWnd, L"EDIT", L"", ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE,
            82, 248, 80, 24, IDC_ED_THREADS);
    MkChild(mainWnd, L"STATIC", L"(1-200)", SS_LEFT, 0, 168, 252, 60, 18, 0);

    // ---- 右侧：扫描结果 ----
    MkChild(mainWnd, L"BUTTON", L"扫描结果", BS_GROUPBOX, 0, 282, 10, 362, 300, 0);
    HWND hResult = MkChild(mainWnd, L"EDIT", L"",
            ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY |
            WS_VSCROLL | WS_HSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE,
            296, 28, 334, 268, IDC_ED_RESULT);
    SetWindowSubclass(hResult, ResultProc, 1, 0);

    // ---- 底部：按钮 / 进度 / 状态 ----
    MkChild(mainWnd, L"BUTTON", K_STR_SCAN, BS_DEFPUSHBUTTON | WS_TABSTOP, 0,
            282, 320, 100, 30, IDC_BTN_SCAN);
    MkChild(mainWnd, L"BUTTON", L"复制全部", BS_PUSHBUTTON | WS_TABSTOP, 0,
            388, 320, 70, 30, IDC_BTN_COPYALL);
    MkChild(mainWnd, L"BUTTON", L"关于", BS_PUSHBUTTON | WS_TABSTOP, 0,
            464, 320, 56, 30, 4);
    MkChild(mainWnd, L"BUTTON", L"退出", BS_PUSHBUTTON | WS_TABSTOP, 0,
            526, 320, 56, 30, 2);
    MkChild(mainWnd, L"msctls_progress32", L"", WS_BORDER, 0,
            12, 360, 632, 20, IDC_PROGRESS);
    MkChild(mainWnd, L"STATIC", L"就绪", SS_LEFT, 0, 14, 388, 630, 18, IDC_ST_STATUS);

    // ---- 默认值（ini 里已保存的会在 SettingsLoad 中覆盖）----
    SendDlgItemMessageW(mainWnd, IDC_IP_START, IPM_SETADDRESS, 0, 0xC0A80101);  // 192.168.1.1
    SendDlgItemMessageW(mainWnd, IDC_IP_END,   IPM_SETADDRESS, 0, 0xC0A801FE);  // 192.168.1.254
    SetDlgItemTextW(mainWnd, IDC_ED_PORTS, K_DEF_PORTS);
    SetDlgItemInt(mainWnd, IDC_ED_TIMEOUT, 200, FALSE);
    SetDlgItemInt(mainWnd, IDC_ED_THREADS, 200, FALSE);
    EnumAdapters(hCombo);
    if ((int)SendMessageW(hCombo, CB_GETCOUNT, 0, 0) > 0)
        SendMessageW(hCombo, CB_SETCURSEL, 0, 0);
    SettingsLoad();

    // 窗口图标：assets/logo.ico（多尺寸；找不到则用类默认图标）
    HICON hSmall = LoadAppIcon(GetSystemMetrics(SM_CXSMICON));
    if (hSmall) SendMessageW(mainWnd, WM_SETICON, ICON_SMALL, (LPARAM)hSmall);
    HICON hBig = LoadAppIcon(GetSystemMetrics(SM_CXICON));
    if (hBig) SendMessageW(mainWnd, WM_SETICON, ICON_BIG, (LPARAM)hBig);

    ShowWindow(mainWnd, nCmdShow ? nCmdShow : SW_SHOW);
    UpdateWindow(mainWnd);

    MSG m;
    while (GetMessageW(&m, NULL, 0, 0) > 0)
    {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }

    WSACleanup();
    if (g_hFont) DeleteObject(g_hFont);
    if (g_hFontBold) DeleteObject(g_hFontBold);
    return 0;
}
