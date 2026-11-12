// GTA:SA 1.0 US (image base 0x400000).
#pragma once

#include <windows.h>
#include <d3d9.h>
#include <cstdint>

namespace game {

struct PsGlobal {
    HWND      window;
    HINSTANCE instance;
    BOOL      fullScreen;
    float     lastMousePos[2];
    DWORD     lastRenderTime;
    void*     diInterface;
    void*     diMouse;
    void*     diDevice1;
    void*     diDevice2;
};

struct RsGlobalType {
    const char* appName;
    int         maximumWidth;
    int         maximumHeight;
    int         maxFPS;
    BOOL        quit;
    PsGlobal*   ps;
};

struct VideoMode {
    UINT      width;
    UINT      height;
    UINT      refreshRate;
    D3DFORMAT format;
    DWORD     flags;
};

constexpr DWORD kVideoModeExclusive = 0x1;

namespace addr {
constexpr uintptr_t RsGlobal            = 0xC17040;
constexpr uintptr_t D3DDevice           = 0xC97C28;
constexpr uintptr_t VideoModes          = 0xC97C48;
constexpr uintptr_t PresentParams       = 0xC9C040;
constexpr uintptr_t RwWindowSize        = 0xC9BEE4;
constexpr uintptr_t GetCurrentVideoMode = 0x7F2D20;
constexpr uintptr_t VersionSignature    = 0x82457C;
constexpr uintptr_t IatPeekMessageA     = 0x8582BC;
}

namespace site {
constexpr uintptr_t DeviceDialog       = 0x746225;
constexpr uintptr_t MouseRecenter      = 0x53E9F1;
constexpr uintptr_t InitPresent        = 0x7F670A;
constexpr uintptr_t InitDevice         = 0x7F6800;
constexpr uintptr_t CameraAspect       = 0x72FCFE;
constexpr uintptr_t PauseSounds        = 0x747FA5;
constexpr uintptr_t ActivateForeground = 0x747FB0;
constexpr uintptr_t KillFocusForeground = 0x748054;
constexpr uintptr_t SetFocusMenu       = 0x748063;
constexpr uintptr_t WmSizeCameraResize = 0x747F32;
constexpr uintptr_t ClearSizeJne       = 0x7F7948;
constexpr uintptr_t ClearSizeJe        = 0x7F7955;
constexpr uintptr_t BeginSizeJne       = 0x7F91FF;
constexpr uintptr_t BeginSizeJe        = 0x7F920D;
constexpr uintptr_t VsyncBranch        = 0x7F99E5;
}

inline RsGlobalType& RsGlobal() {
    return *reinterpret_cast<RsGlobalType*>(addr::RsGlobal);
}

inline IDirect3DDevice9* D3DDevice() {
    return *reinterpret_cast<IDirect3DDevice9**>(addr::D3DDevice);
}

inline D3DPRESENT_PARAMETERS& PresentParams() {
    return *reinterpret_cast<D3DPRESENT_PARAMETERS*>(addr::PresentParams);
}

inline int GetCurrentVideoMode() {
    return reinterpret_cast<int(__cdecl*)()>(addr::GetCurrentVideoMode)();
}

inline VideoMode* CurrentVideoMode() {
    auto* list = *reinterpret_cast<VideoMode**>(addr::VideoModes);
    const int index = list ? GetCurrentVideoMode() : -1;
    return index >= 0 ? &list[index] : nullptr;
}

inline HWND Window() {
    const PsGlobal* ps = RsGlobal().ps;
    return ps ? ps->window : nullptr;
}

inline void ClearFullScreenFlag() {
    if (PsGlobal* ps = RsGlobal().ps; ps && ps->fullScreen) {
        ps->fullScreen = FALSE;
    }
}

}
