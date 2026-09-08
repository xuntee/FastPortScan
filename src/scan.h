// 扫描引擎与线程编排（内核借鉴原版逆向结论）
#pragma once
#include "common.h"

// 校验参数并启动扫描；参数非法时弹原版同款提示并返回 false
bool ScanStart(HWND hMain);
bool ScanIsRunning();
void ScanStop();
DWORD ScanTimeoutMs();
