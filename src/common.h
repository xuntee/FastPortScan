// ============================================================================
//  FastPortScan（极速端口扫描）—— 公共定义
//
//  扫描内核借鉴自 ScanPort V1.2 (2006) 逆向还原的算法：
//    1. socket + FIONBIO 非阻塞 connect()，绝不在 connect 上阻塞等待
//    2. connect 返回 WSAEWOULDBLOCK 后用 select() 写集合按毫秒级超时判定开放
//    3. N 个工作线程（低于正常优先级）在临界区内领取任务，
//       顺序遍历 [起始IP..结束IP] × 端口区间表
//    4. InterlockedIncrement 计数，进度条按 完成*100/总数 刷新
// ============================================================================
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <commctrl.h>
#include <iphlpapi.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// 控件 ID
// ---------------------------------------------------------------------------
#define IDC_IP_START    1001        // SysIPAddress32 起始IP
#define IDC_IP_END      1002        // SysIPAddress32 结束IP
#define IDC_ED_PORTS    1003        // 端口号
#define IDC_ED_TIMEOUT  1004        // 超时(ms)
#define IDC_ED_RESULT   1005        // 扫描结果
#define IDC_ED_THREADS  1006        // 线程数
#define IDC_PROGRESS    1007        // 进度条
#define IDC_CB_ADAPTER  1008        // 网卡下拉
#define IDC_ST_STATUS   1009        // 状态栏
#define IDC_BTN_HISTORY 1010        // ▼ 快速IP下拉
#define IDC_BTN_SCAN    1011        // 扫描/停止
#define IDC_BTN_COPYALL 1012        // 复制全部

// ---------------------------------------------------------------------------
// 常量
// ---------------------------------------------------------------------------
#define TIMEOUT_MIN     1
#define TIMEOUT_MAX     15000       // 原版上限
#define THREADS_MIN     1
#define THREADS_MAX     200         // 原版上限

#define K_VERSION L"v1.0.5"   // 发版时与 assets/app.rc 的 FileVersion 同步改
extern const wchar_t K_TITLE[];
extern const wchar_t K_APP_NAME[];
extern const wchar_t K_STR_SCAN[];
extern const wchar_t K_STR_STOP[];
extern const wchar_t K_DEF_PORTS[];
extern const wchar_t K_ERR_IP[];
extern const wchar_t K_ERR_TIMEOUT[];
extern const wchar_t K_ERR_THREADS[];
extern const wchar_t K_ASK_CLEAR[];
extern const wchar_t K_ERR_PORT[];
extern const wchar_t K_REPO_URL[];

// ---------------------------------------------------------------------------
// 共享全局（定义在 common.cpp）
// ---------------------------------------------------------------------------
extern HWND      g_hMain;           // 主窗口
extern HFONT     g_hFont;           // 界面字体
extern HFONT     g_hFontBold;       // 标题字体
extern HICON     g_hLogo64;         // 64px logo（关于窗口用）
extern ULONGLONG g_tickStart;       // 本次扫描开始时刻

// 小工具
void   FormatHMS(ULONGLONG ms, wchar_t* buf, size_t cch);        // 毫秒 → "时:分:秒"
void   CopyTextToClipboard(HWND hOwner, const wchar_t* text);
void   InstallCrashLogger();   // 崩溃现场写入 exe 旁 FastPortScan-crash.log
HWND   MkChild(HWND parent, const wchar_t* cls, const wchar_t* text,
               DWORD style, DWORD ex, int x, int y, int w, int h, int id);

// 图标（内嵌资源：assets/app.rc → RT_GROUP_ICON ID=1，多尺寸）
HICON  LoadAppIcon(int side);

// ---------------------------------------------------------------------------
// 扫描（scan.h/scan.cpp）
// ---------------------------------------------------------------------------
bool    ScanStart(HWND hMain);                      // 校验并启动扫描线程
bool    ScanIsRunning();
void    ScanStop();                                 // 请求停止（异步）
DWORD   ScanTimeoutMs();

// ---------------------------------------------------------------------------
// 配置 / 历史 / 网卡（config.cpp）
// ---------------------------------------------------------------------------
// 配置 / 历史 / 网卡见 config.h（config.cpp）


void HistoryLoad();
void HistoryAdd(DWORD sip, DWORD eip);
void SettingsLoad();
void SettingsSave();
void EnumAdapters(HWND hCombo);

// ---------------------------------------------------------------------------
// 关于窗口（about.cpp）
// ---------------------------------------------------------------------------
void ShowAbout(HWND hOwner);
