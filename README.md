# FastPortScan — 极速端口扫描

参考 2006 年 `ScanPort V1.2`（`reference/端口扫描.exe`）逆向还原的扫描内核，用 C++/Win32 重写的多线程快速端口扫描器。多文件源码，静态链接，无运行库依赖。

![logo](assets/logo.png)

## 功能

- **TCP 连接扫描**：非阻塞 connect + select 毫秒级超时 + 多线程（1-200，默认 10，低于正常优先级）
- **实时进度**：进度条 + 状态栏（进度 / 已完成 / 总数 / 开放数 / 已用时间 / 预计剩余 时:分:秒），完成后显示总耗时
- **网卡一键填段**：下拉选择本机适配器，按子网前缀自动填入起止 IP（如 /24 → x.x.x.0 ~ x.x.x.255）
- **IP 段历史**：▼ 下拉最近使用的扫描范围；IP 段 / 端口 / 超时 / 线程设置持久化到 `FastPortScan.ini`
- **结果交互**：结果行**单击复制**该行，**双击用默认浏览器打开** `http://ip:port`（443 走 https）；另有"复制全部"按钮
- 端口格式：逗号分隔，支持单端口与区间（`80,139,440-445`）；超时 1-15000ms，线程 1-200

## 使用

运行 `FastPortScan.exe`（x64 静态链接）；`logo.ico` 与 exe 同目录（或 exe 旁的 `assets\logo.ico`）时作为程序图标。

## 编译

```
bash build.sh
```

核心命令（用 pip 包 ziglang 自带的 zig c++，免装 VS/MinGW）：

```
python -m ziglang c++ -target x86_64-windows-gnu -O2 -municode -static \
    -Wl,--subsystem,windows \
    src/main.cpp src/common.cpp src/scan.cpp src/config.cpp src/about.cpp \
    -o FastPortScan.exe \
    -lws2_32 -lcomctl32 -liphlpapi -lgdi32 -lshell32
```

MSVC：`cl /O2 /DUNICODE /D_UNICODE src\*.cpp ws2_32.lib comctl32.lib iphlpapi.lib gdi32.lib shell32.lib`

## CI

推送 tag（`v1.0.0` 等）自动编译并创建 GitHub Release 附上 exe：见 `.github/workflows/release.yml`。

## 目录结构

```
├── src/                      源码（main / common / scan / config / about）
├── assets/logo.png|.ico      程序图标素材
├── reference/端口扫描.exe    逆向参考原型（2006）
├── unpack/                   壳分析与脱壳脚本（NRV2B 解压器等）
├── build.sh                  编译脚本
└── .github/workflows/        tag 自动构建 Release
```

## 扫描内核（逆向自原型，实测 /24 网段 3072 项约 13 秒）

```
工作线程 × N（低于正常优先级）：
    临界区内领取下一个 IP:端口（顺序遍历 [起始IP..结束IP] × 端口区间）
    socket(AF_INET, SOCK_STREAM, 0) + ioctlsocket(FIONBIO) 非阻塞
    connect() == 0 → 开放；WSAEWOULDBLOCK → select() 写集合按毫秒超时判定
    closesocket；开放则结果框追加 "%d.%d.%d.%d: %d"
    InterlockedIncrement 计数 → 进度条 / ETA
```

## 逆向记录

原型为 UPX 变种壳（段名 BAO0/BAO1）。`unpack/unpack_nrv2b.py` 用 Python 精确模拟
壳入口的 NRV2B 解压 stub（位缓冲重填语义 + E8/E9 call-filter 还原）得到原始代码；
原始 OEP 0x402291，导入表含 ws2_32 按序号导入的 11 个 winsock 函数。关键函数：
WinMain 0x4014F0、DialogProc 0x401DD0、扫描线程 0x401930、工作线程 0x401630、
单端口探测 0x401770。
