#!/bin/bash
# FastPortScan 极速端口扫描 —— 用 pip 包 ziglang 的 zig c++ 编译（免装 VS/MinGW）
# 图标/版本信息通过 assets/app.rc 编译成资源嵌入 exe（zig >= 0.14 的 zig rc）
set -e
python -m ziglang rc -c 65001 assets/app.rc app.res
python -m ziglang c++ -target x86_64-windows-gnu -O2 -municode -static \
    -Wl,--subsystem,windows \
    src/main.cpp src/common.cpp src/scan.cpp src/config.cpp src/about.cpp \
    app.res -o FastPortScan.exe \
    -lws2_32 -lcomctl32 -liphlpapi -lgdi32 -lshell32
echo BUILD-OK
