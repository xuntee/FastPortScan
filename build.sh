#!/bin/bash
# FastPortScan 极速端口扫描 —— 用 pip 包 ziglang 的 zig c++ 编译（免装 VS/MinGW）
set -e
python -m ziglang c++ -target x86_64-windows-gnu -O2 -municode -static \
    -Wl,--subsystem,windows \
    src/main.cpp src/common.cpp src/scan.cpp src/config.cpp src/about.cpp \
    -o FastPortScan.exe \
    -lws2_32 -lcomctl32 -liphlpapi -lgdi32 -lshell32
cp -f assets/logo.png logo.png
cp -f assets/logo.ico logo.ico
echo BUILD-OK
