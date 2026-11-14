#pragma once

#include <windows.h>

namespace window {

RECT MonitorRect();

void ApplyBorderless();

void InstallWndProcHook();

void HideCursor();

bool InstallMouseScaling();

bool InstallMouseFocusGuard();

}
