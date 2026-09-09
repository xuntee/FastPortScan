// 公共定义与界面小工具
#include "common.h"

const wchar_t K_TITLE[]       = L"FastPortScan - 极速端口扫描";
const wchar_t K_APP_NAME[]    = L"FastPortScan";
const wchar_t K_STR_SCAN[]    = L"开始扫描";
const wchar_t K_STR_STOP[]    = L"停 止";
const wchar_t K_DEF_PORTS[]   = L"21,22,23,80,135,139,443,445,1433,3306,3389,5432,6379,8080";
const wchar_t K_ERR_IP[]      = L"IP 填写不完整";
const wchar_t K_ERR_TIMEOUT[] = L"超时时间填写错误（1-15000 毫秒）";
const wchar_t K_ERR_THREADS[] = L"线程数必须在1--200之间";
const wchar_t K_ASK_CLEAR[]   = L"是否清除原有扫描结果？";
const wchar_t K_ERR_PORT[]    = L"端口号填写错误";
const wchar_t K_REPO_URL[]    = L"https://github.com/xuntee/FastPortScan";

HWND      g_hMain     = NULL;
HFONT     g_hFont     = NULL;
HFONT     g_hFontBold = NULL;
HICON     g_hLogo64   = NULL;
ULONGLONG g_tickStart = 0;

// 毫秒 → "时:分:秒"
void FormatHMS(ULONGLONG ms, wchar_t* buf, size_t cch)
{
    ULONGLONG s = ms / 1000;
    _snwprintf(buf, cch, L"%02u:%02u:%02u",
               (unsigned)(s / 3600), (unsigned)((s % 3600) / 60), (unsigned)(s % 60));
}

// 文本复制到剪贴板
void CopyTextToClipboard(HWND hOwner, const wchar_t* text)
{
    if (!OpenClipboard(hOwner)) return;
    EmptyClipboard();
    size_t bytes = (wcslen(text) + 1) * sizeof(wchar_t);
    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (hg)
    {
        void* p = GlobalLock(hg);
        if (p) { memcpy(p, text, bytes); GlobalUnlock(hg); }
        SetClipboardData(CF_UNICODETEXT, hg);
    }
    CloseClipboard();
}

// 创建子控件并套用界面字体
HWND MkChild(HWND parent, const wchar_t* cls, const wchar_t* text,
             DWORD style, DWORD ex, int x, int y, int w, int h, int id)
{
    HWND c = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style,
                             x, y, w, h, parent, (HMENU)(INT_PTR)id, NULL, NULL);
    SendMessageW(c, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    return c;
}

// 内嵌资源图标（assets/app.rc 编译进 exe 的 RT_GROUP_ICON，ID=1，多尺寸）
HICON LoadAppIcon(int side)
{
    return (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1),
                             IMAGE_ICON, side, side, LR_DEFAULTCOLOR);
}
