#include "Window.h"

#include "Device.h"
#include "Game.h"
#include "Log.h"

namespace window {
namespace {

constexpr LONG kStyleRemove   = WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU |
                                WS_BORDER | WS_DLGFRAME;
constexpr LONG kStyleAdd      = WS_POPUP | WS_VISIBLE | WS_CLIPSIBLINGS;
constexpr LONG kExStyleRemove = WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE;

WNDPROC g_wndProcOri = nullptr;

LRESULT CALLBACK WndProcHook(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    game::ClearFullScreenFlag();
    return CallWindowProcA(g_wndProcOri, wnd, msg, wParam, lParam);
}

// Rescaling in the game's PeekMessageA import fixes the coordinates for every WndProc, SA-MP included.
using PeekMessageA_t = BOOL(WINAPI*)(LPMSG, HWND, UINT, UINT, UINT);
PeekMessageA_t g_peekMessageOri = nullptr;

void ScaleMouseMessage(MSG* msg) {
    if (msg->message < WM_MOUSEFIRST || msg->message > WM_MOUSELAST || msg->message == WM_MOUSEWHEEL ||
        msg->message == WM_MOUSEHWHEEL) {
        return;
    }
    const POINT bb = device::BackBufferSize();
    if (bb.x <= 0 || bb.y <= 0 || msg->hwnd != game::Window()) {
        return;
    }
    RECT client;
    if (!GetClientRect(msg->hwnd, &client) || client.right <= 0 || client.bottom <= 0 ||
        (client.right == bb.x && client.bottom == bb.y)) {
        return;
    }
    const int x = static_cast<short>(LOWORD(msg->lParam));
    const int y = static_cast<short>(HIWORD(msg->lParam));
    msg->lParam = MAKELPARAM(MulDiv(x, bb.x, client.right), MulDiv(y, bb.y, client.bottom));
}

BOOL WINAPI PeekMessageHook(LPMSG msg, HWND wnd, UINT filterMin, UINT filterMax, UINT removeMsg) {
    const BOOL got = g_peekMessageOri(msg, wnd, filterMin, filterMax, removeMsg);
    if (got && msg) {
        ScaleMouseMessage(msg);
    }
    return got;
}

}

RECT MonitorRect() {
    HWND wnd = game::Window();
    HMONITOR mon = wnd ? MonitorFromWindow(wnd, MONITOR_DEFAULTTOPRIMARY)
                       : MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = { sizeof(mi) };
    if (!GetMonitorInfoA(mon, &mi)) {
        return { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
    }
    return mi.rcMonitor;
}

void ApplyBorderless() {
    HWND wnd = game::Window();
    if (!wnd || IsIconic(wnd)) {
        return;
    }

    const LONG style      = GetWindowLongA(wnd, GWL_STYLE);
    const LONG exStyle    = GetWindowLongA(wnd, GWL_EXSTYLE);
    const LONG newStyle   = (style & ~kStyleRemove) | kStyleAdd;
    const LONG newExStyle = exStyle & ~kExStyleRemove;
    const RECT mon        = MonitorRect();
    RECT current          = {};
    GetWindowRect(wnd, &current);

    if (style == newStyle && exStyle == newExStyle && EqualRect(&current, &mon)) {
        return;
    }

    if (style != newStyle) {
        SetWindowLongA(wnd, GWL_STYLE, newStyle);
    }
    if (exStyle != newExStyle) {
        SetWindowLongA(wnd, GWL_EXSTYLE, newExStyle);
    }
    // No activation or z-order change: this also runs while another app is in the foreground.
    SetWindowPos(wnd, nullptr, mon.left, mon.top, mon.right - mon.left, mon.bottom - mon.top,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    static unsigned applied = 0;
    if (++applied <= 50) {
        logging::Info("window    borderless %ldx%ld at (%ld,%ld)", mon.right - mon.left, mon.bottom - mon.top,
                      mon.left, mon.top);
        logging::Info("          style %08lX -> %08lX, exstyle %08lX -> %08lX", style, newStyle, exStyle,
                      newExStyle);
    } else if (applied == 51) {
        logging::Warn("borderless window re-applied 50 times; something keeps changing the window, not logging further");
    }
}

void InstallWndProcHook() {
    HWND wnd = game::Window();
    if (g_wndProcOri || !wnd) {
        return;
    }
    g_wndProcOri = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrA(wnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WndProcHook)));
    if (g_wndProcOri) {
        logging::Info("hooks     window procedure");
    } else {
        logging::Warn("could not hook the window procedure (error %lu)", GetLastError());
    }
}

void HideCursor() {
    static bool done = false;
    HWND wnd = game::Window();
    if (done || !wnd) {
        return;
    }
    done = true;
    SetClassLongPtrA(wnd, GCLP_HCURSOR, 0);
    SetCursor(nullptr);
}

bool InstallMouseScaling() {
    auto* slot = reinterpret_cast<PeekMessageA_t*>(game::addr::IatPeekMessageA);
    char where[MAX_PATH + 32];
    if (!*slot || !logging::DescribeAddress(reinterpret_cast<uintptr_t>(*slot), where, sizeof(where))) {
        logging::Warn("PeekMessageA import slot does not point into a module; mouse scaling disabled");
        return false;
    }

    DWORD oldProtect;
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &oldProtect)) {
        logging::Warn("could not unprotect the PeekMessageA import slot (error %lu)", GetLastError());
        return false;
    }
    g_peekMessageOri = *slot;
    *slot = &PeekMessageHook;
    VirtualProtect(slot, sizeof(*slot), oldProtect, &oldProtect);
    logging::Info("mouse scaling installed (PeekMessageA was %s)", where);
    return true;
}

}
