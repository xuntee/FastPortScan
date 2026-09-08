#include "about.h"

static bool    g_aboutClosed = false;
static HBRUSH  g_hAboutBg = NULL;

// ID 分配：1=确定  2=仓库链接  3=标题  4=版本行
static LRESULT CALLBACK AboutProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_CTLCOLORSTATIC:
    {
        HDC dc = (HDC)wp;
        SetBkMode(dc, TRANSPARENT);
        if ((HWND)lp == GetDlgItem(h, 2))
            SetTextColor(dc, RGB(0, 102, 217));             // 仓库链接：蓝
        else if ((HWND)lp == GetDlgItem(h, 3) || (HWND)lp == GetDlgItem(h, 4))
            SetTextColor(dc, RGB(96, 96, 96));              // 次要文字：灰
        else
            SetTextColor(dc, RGB(30, 30, 30));
        return (LRESULT)g_hAboutBg;
    }
    case WM_SETCURSOR:                                      // 链接上手型光标
    {
        if (LOWORD(lp) == HTCLIENT)
        {
            POINT pt; GetCursorPos(&pt);
            RECT r; GetWindowRect(GetDlgItem(h, 2), &r);
            if (PtInRect(&r, pt)) { SetCursor(LoadCursorW(NULL, IDC_HAND)); return TRUE; }
        }
        break;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == 2)                                // 点击仓库链接 → 默认浏览器
        {
            ShellExecuteW(h, L"open", K_REPO_URL, NULL, NULL, SW_SHOWNORMAL);
            return 0;
        }
        if (LOWORD(wp) == 1 || LOWORD(wp) == IDCANCEL)
        {
            DestroyWindow(h);
            return 0;
        }
        break;
    case WM_DESTROY:
        g_aboutClosed = true;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void ShowAbout(HWND hOwner)
{
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = AboutProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hbrBackground = g_hAboutBg = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"FastPortScanAbout";
    RegisterClassW(&wc);

    RECT ra; SystemParametersInfoW(SPI_GETWORKAREA, 0, &ra, 0);
    int ww = 380, wh = 336;
    int wx = ra.left + ((ra.right - ra.left) - ww) / 2;
    int wy = ra.top + ((ra.bottom - ra.top) - wh) / 2;

    HWND h = CreateWindowExW(WS_EX_DLGMODALFRAME, L"FastPortScanAbout",
                             L"关于 FastPortScan",
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                             wx, wy, ww, wh, hOwner, NULL, wc.hInstance, NULL);

    // logo（64px）
    wchar_t logoPath[MAX_PATH * 2];
    if (IconPath(logoPath, MAX_PATH * 2))
    {
        HICON h64 = (HICON)LoadImageW(NULL, logoPath, IMAGE_ICON, 64, 64, LR_LOADFROMFILE);
        if (h64)
        {
            HWND hIcon = MkChild(h, L"STATIC", L"", SS_ICON, 0, ww/2-32, 16, 64, 64, 1);
            SendMessageW(hIcon, STM_SETICON, (WPARAM)h64, 0);
        }
    }

    // 标题 + 版本
    HWND hTitle = MkChild(h, L"STATIC", L"FastPortScan 极速端口扫描", SS_CENTER, 0,
                          10, 90, ww-20, 24, 3);
    SendMessageW(hTitle, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);
    MkChild(h, L"STATIC", L"v1.0  ·  多线程 TCP 快速端口扫描器", SS_CENTER, 0,
            10, 118, ww-20, 18, 4);

    // 开发者信息
    MkChild(h, L"STATIC", L"开 发 者： xuntee", SS_CENTER, 0,
            10, 154, ww-20, 18, 0);
    MkChild(h, L"STATIC", L"开发工具： ZCode · 智谱 GLM", SS_CENTER, 0,
            10, 176, ww-20, 18, 0);
    MkChild(h, L"STATIC", L"支持国产优质团队", SS_CENTER, 0,
            10, 198, ww-20, 18, 0);

    // 仓库地址（可点击）
    MkChild(h, L"STATIC", K_REPO_URL, SS_CENTER | SS_NOTIFY, 0,
            10, 230, ww-20, 18, 2);

    HWND hOk = MkChild(h, L"BUTTON", L"确定", BS_DEFPUSHBUTTON | WS_TABSTOP, 0,
                       ww/2-45, 262, 90, 28, 1);
    SendMessageW(hOk, WM_SETFONT, (WPARAM)g_hFont, TRUE);

    // 模态消息循环
    EnableWindow(hOwner, FALSE);
    ShowWindow(h, SW_SHOW);
    UpdateWindow(h);

    MSG m;
    g_aboutClosed = false;
    while (!g_aboutClosed && GetMessageW(&m, NULL, 0, 0) > 0)
    {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    EnableWindow(hOwner, TRUE);
    SetForegroundWindow(hOwner);
}
