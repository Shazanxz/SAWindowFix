#include "Device.h"

#include "Game.h"
#include "Log.h"
#include "Memory.h"
#include "Window.h"

#include <algorithm>
#include <cstdio>
#include <intrin.h>

namespace device {
namespace {

using Reset_t   = HRESULT(__stdcall*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
using Present_t = HRESULT(__stdcall*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);

constexpr size_t kVtblReset   = 16;
constexpr size_t kVtblPresent = 17;

Reset_t   g_resetOri   = nullptr;
Present_t g_presentOri = nullptr;
POINT     g_backBuffer = { 0, 0 };

// RW rescales render targets by BackBufferWidth / this snapshot after a Reset; keep them equal.
void SyncRwWindowSize() {
    if (g_backBuffer.x > 0 && g_backBuffer.y > 0) {
        auto* size = reinterpret_cast<int*>(game::addr::RwWindowSize);
        size[0] = g_backBuffer.x;
        size[1] = g_backBuffer.y;
    }
}

void ForcePresentParams() {
    auto& rs = game::RsGlobal();
    auto& pp = game::PresentParams();
    game::VideoMode* mode = game::CurrentVideoMode();

    const RECT mon = window::MonitorRect();
    g_backBuffer = { mon.right - mon.left, mon.bottom - mon.top };
    if (mode && mode->width > 0 && mode->height > 0) {
        g_backBuffer = { static_cast<LONG>(mode->width), static_cast<LONG>(mode->height) };
    }

    pp.Windowed                   = TRUE;
    pp.BackBufferWidth            = static_cast<UINT>(g_backBuffer.x);
    pp.BackBufferHeight           = static_cast<UINT>(g_backBuffer.y);
    pp.BackBufferFormat           = D3DFMT_X8R8G8B8;
    pp.SwapEffect                 = D3DSWAPEFFECT_DISCARD;
    pp.FullScreen_RefreshRateInHz = 0;
    pp.PresentationInterval       = D3DPRESENT_INTERVAL_IMMEDIATE;
    if (HWND wnd = game::Window()) {
        pp.hDeviceWindow = wnd;
    }

    rs.maximumWidth  = g_backBuffer.x;
    rs.maximumHeight = g_backBuffer.y;
    if (rs.ps) {
        rs.ps->fullScreen = FALSE;
    }

    if (mode) {
        mode->format      = pp.BackBufferFormat;
        mode->refreshRate = 0;
        mode->flags      &= ~game::kVideoModeExclusive;
    }

    logging::Info("present   windowed, backbuffer %ldx%ld (%s), monitor %ldx%ld, vsync off", g_backBuffer.x,
                  g_backBuffer.y, mode ? "menu resolution" : "no video mode, using monitor size",
                  mon.right - mon.left, mon.bottom - mon.top);
}

const char* GameResetReason(uintptr_t ret) {
    switch (ret) {
    case 0x7F8192: case 0x7F8332: case 0x7F971B: case 0x7F9B4E:
        return "device lost recovery";
    case 0x7F86CD: case 0x7F8B84:
        return "video mode change";
    case 0x7F9A21:
        return "vsync toggle";
    case 0x7F799B: case 0x7F7AAF: case 0x7F9253: case 0x7F9367:
        return "window size check";
    default:
        return nullptr;
    }
}

bool LooksLikeReturnAddress(uintptr_t ret) {
    const auto* p = reinterpret_cast<const uint8_t*>(ret);
    if (ret < 0x10000 || !mem::IsReadable(p - 6, 6)) {
        return false;
    }
    return p[-5] == 0xE8 || p[-6] == 0xFF || p[-3] == 0xFF || p[-2] == 0xFF;
}

void DescribeCallers(const void* stackTop, char* out, size_t outSize, const char** reason) {
    constexpr int kScan = 256, kMaxFrames = 6;
    out[0] = '\0';
    *reason = nullptr;
    const auto* stack = static_cast<const uintptr_t*>(stackTop);
    if (!mem::IsReadable(stack, kScan * sizeof(uintptr_t))) {
        return;
    }
    size_t len = 0;
    int frames = 0;
    char desc[MAX_PATH + 32];
    for (int i = 0; i < kScan && frames < kMaxFrames && len + 2 < outSize; ++i) {
        const uintptr_t ret = stack[i];
        if (!LooksLikeReturnAddress(ret) || !logging::DescribeAddress(ret, desc, sizeof(desc))) {
            continue;
        }
        const int n = std::snprintf(out + len, outSize - len, "%s%s", frames ? " <- " : "", desc);
        if (n < 0) {
            break;
        }
        len = std::min(len + static_cast<size_t>(n), outSize - 1);
        ++frames;
        if (const char* r = GameResetReason(ret)) {
            *reason = r;
            break;
        }
    }
}

const char* CooperativeLevelName(HRESULT hr) {
    switch (hr) {
    case D3D_OK:                 return "ok";
    case D3DERR_DEVICELOST:      return "DEVICELOST";
    case D3DERR_DEVICENOTRESET:  return "DEVICENOTRESET";
    case D3DERR_DRIVERINTERNALERROR: return "DRIVERINTERNALERROR";
    default:                     return "?";
    }
}

DWORD g_resetTick = 0;

HRESULT __stdcall ResetHook(IDirect3DDevice9* self, D3DPRESENT_PARAMETERS* pp) {
    static char callers[1024];
    const char* reason = nullptr;
    DescribeCallers(_AddressOfReturnAddress(), callers, sizeof(callers), &reason);
    const HRESULT coop = self->TestCooperativeLevel();
    const UINT requestedW = pp->BackBufferWidth, requestedH = pp->BackBufferHeight;

    pp->Windowed                   = TRUE; // a menu mode change requests exclusive fullscreen
    pp->FullScreen_RefreshRateInHz = 0;
    pp->BackBufferFormat           = D3DFMT_X8R8G8B8;
    if ((pp->BackBufferWidth == 0 || pp->BackBufferHeight == 0) && g_backBuffer.x > 0) {
        pp->BackBufferWidth  = static_cast<UINT>(g_backBuffer.x);
        pp->BackBufferHeight = static_cast<UINT>(g_backBuffer.y);
    }

    const DWORD start = GetTickCount();
    const HRESULT hr = g_resetOri(self, pp);
    const DWORD took = GetTickCount() - start;

    static unsigned resets = 0;
    if (++resets <= 100) {
        logging::Section("device reset");
        char label[16];
        std::snprintf(label, sizeof(label), "#%u", resets);
        if (SUCCEEDED(hr)) {
            logging::Info("%-9s %s, %ux%u (requested %ux%u), cooperative level %s, took %lums", label,
                          reason ? reason : "unknown reason", pp->BackBufferWidth, pp->BackBufferHeight,
                          requestedW, requestedH, CooperativeLevelName(coop), took);
        } else {
            logging::Error("%-9s %s, %ux%u failed with hr=0x%08lX, cooperative level %s", label,
                           reason ? reason : "unknown reason", pp->BackBufferWidth, pp->BackBufferHeight, hr,
                           CooperativeLevelName(coop));
        }
        logging::Info("          called from %s", callers[0] ? callers : "?");
    }

    if (SUCCEEDED(hr)) {
        g_backBuffer = { static_cast<LONG>(pp->BackBufferWidth), static_cast<LONG>(pp->BackBufferHeight) };
        g_resetTick  = GetTickCount();
        window::ApplyBorderless();
        SyncRwWindowSize();
    }
    return hr;
}

void TrackFrameRate() {
    if (!logging::Enabled()) {
        return;
    }
    constexpr DWORD kInterval = 10000;
    static DWORD    start  = GetTickCount();
    static unsigned frames = 0;

    ++frames;
    const DWORD now     = GetTickCount();
    const DWORD elapsed = now - start;
    if (elapsed >= kInterval) {
        const double fps     = frames * 1000.0 / elapsed;
        const double seconds = elapsed / 1000.0;
        const char*  focus   = GetForegroundWindow() == game::Window() ? "foreground" : "background";
        logging::Section("fps");
        if (elapsed >= kInterval * 3 / 2) {
            logging::Warn("%5.1f fps  %5.1fs   %s   (interval took %.1fs: game stalled)", fps, seconds, focus,
                          seconds);
        } else {
            logging::Info("%5.1f fps  %5.1fs   %s", fps, seconds, focus);
        }
        frames = 0;
        start  = now;
    }
}

HRESULT __stdcall PresentHook(IDirect3DDevice9* self, const RECT* src, const RECT* dst, HWND wnd,
                              const RGNDATA* dirty) {
    game::ClearFullScreenFlag(); // the ESC menu keeps setting it back to TRUE
    if (g_resetTick) {
        logging::Info("          first frame %lums after the reset", GetTickCount() - g_resetTick);
        g_resetTick = 0;
    }
    window::ApplyBorderless();
    window::HideCursor();
    TrackFrameRate();
    return g_presentOri(self, src, dst, wnd, dirty);
}

void InstallHooks() {
    IDirect3DDevice9* dev = game::D3DDevice();
    if (!dev) {
        logging::Error("no D3D9 device after CreateDevice; device hooks not installed");
        return;
    }

    auto* vtbl = *reinterpret_cast<uintptr_t**>(dev);
    if (vtbl[kVtblReset] == reinterpret_cast<uintptr_t>(&ResetHook)) {
        return;
    }

    DWORD oldProtect;
    if (!VirtualProtect(&vtbl[kVtblReset], 2 * sizeof(uintptr_t), PAGE_READWRITE, &oldProtect)) {
        logging::Error("could not unprotect the D3D9 device vtable (error %lu)", GetLastError());
        return;
    }
    g_resetOri          = reinterpret_cast<Reset_t>(vtbl[kVtblReset]);
    g_presentOri        = reinterpret_cast<Present_t>(vtbl[kVtblPresent]);
    vtbl[kVtblReset]    = reinterpret_cast<uintptr_t>(&ResetHook);
    vtbl[kVtblPresent]  = reinterpret_cast<uintptr_t>(&PresentHook);
    VirtualProtect(&vtbl[kVtblReset], 2 * sizeof(uintptr_t), oldProtect, &oldProtect);

    char reset[MAX_PATH + 32]   = "?";
    char present[MAX_PATH + 32] = "?";
    logging::DescribeAddress(reinterpret_cast<uintptr_t>(g_resetOri), reset, sizeof(reset));
    logging::DescribeAddress(reinterpret_cast<uintptr_t>(g_presentOri), present, sizeof(present));
    logging::Info("hooks     device %p, Reset %s, Present %s", static_cast<void*>(dev), reset, present);
}

void __cdecl OnInitPresent() {
    logging::Section("d3d device");
    window::ApplyBorderless();
    ForcePresentParams();
}

void __cdecl OnInitDevice() {
    InstallHooks();
    window::InstallWndProcHook();
    window::ApplyBorderless();
    SyncRwWindowSize();
}

}

POINT BackBufferSize() {
    return g_backBuffer;
}

// The stubs return to site + 6 (game::site::InitPresent / InitDevice).
__declspec(naked) void InitPresentStub() {
    __asm {
        pushad
        pushfd
        call OnInitPresent
        popfd
        popad
        mov ecx, dword ptr ds:[0C97C4Ch]
        push 07F6710h
        ret
    }
}

__declspec(naked) void InitDeviceStub() {
    __asm {
        mov dword ptr ds:[0C9808Ch], ebp
        pushad
        pushfd
        call OnInitDevice
        popfd
        popad
        push 07F6806h
        ret
    }
}

}
