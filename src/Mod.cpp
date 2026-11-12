#include "Mod.h"

#include "Config.h"
#include "CrashHandler.h"
#include "Device.h"
#include "Game.h"
#include "Log.h"
#include "Memory.h"
#include "Version.h"
#include "Window.h"

#include <cstdint>
#include <cstring>

namespace mod {
namespace {

// Calls are redirected here instead of NOPed so other mods hooking the same call can still chain.
// Every replaced callee takes no stack args or is __cdecl, and its return value is unused.
__declspec(naked) void NullSub() {
    __asm ret
}

enum class Op {
    JumpShort,
    CallNullSub,
    Detour,
    Nop,
    JeToJmp,
    StoreOne,
};

struct Patch {
    const char*    name;
    uintptr_t      addr;
    const uint8_t* expected;
    size_t         size;
    Op             op;
    void         (*target)();
};

constexpr uint8_t kJle[]          = { 0x7E };
constexpr uint8_t kCall[]         = { 0xE8 };
constexpr uint8_t kInitPresent[]  = { 0x8B, 0x0D, 0x4C, 0x7C, 0xC9, 0x00 };
constexpr uint8_t kInitDevice[]   = { 0x89, 0x2D, 0x8C, 0x80, 0xC9, 0x00 };
constexpr uint8_t kCameraAspect[] = { 0x74, 0x16 };
constexpr uint8_t kPauseSounds[]  = { 0xE8, 0x86, 0xF4, 0xDB, 0xFF };
// Only opcode + address: SA-MP anti-pause patches often rewrite the imm32 already.
constexpr uint8_t kSetFgApp[]     = { 0xC7, 0x05, 0x1C, 0x62, 0x8D, 0x00 };
constexpr uint8_t kOpenMenu[]     = { 0xE8, 0xF8, 0x3B, 0xDF, 0xFF };
constexpr uint8_t kCameraResize[] = { 0xE8, 0x29, 0x1C, 0xED, 0xFF };
constexpr uint8_t kClearSizeJne[] = { 0x75, 0x11 };
constexpr uint8_t kBeginSizeJne[] = { 0x75, 0x12 };
constexpr uint8_t kSizeJe[]       = { 0x0F, 0x84, 0xE6, 0x02, 0x00, 0x00 };
constexpr uint8_t kVsyncBranch[]  = { 0x74, 0x13 };

#define EXPECT(bytes) bytes, sizeof(bytes)

namespace site = game::site;

const Patch kPatches[] = {
    { "skip device-selection dialog",   site::DeviceDialog,        EXPECT(kJle),          Op::JumpShort,   nullptr },
    { "no cursor recentering",          site::MouseRecenter,       EXPECT(kCall),         Op::CallNullSub, nullptr },
    { "windowed present params",        site::InitPresent,         EXPECT(kInitPresent),  Op::Detour,      &device::InitPresentStub },
    { "device hooks",                   site::InitDevice,          EXPECT(kInitDevice),   Op::Detour,      &device::InitDeviceStub },
    { "fixed 4:3 view window",          site::CameraAspect,        EXPECT(kCameraAspect), Op::Nop,         nullptr },
    { "keep audio on focus loss",       site::PauseSounds,         EXPECT(kPauseSounds),  Op::CallNullSub, nullptr },
    { "keep running on WM_ACTIVATE",    site::ActivateForeground,  EXPECT(kSetFgApp),     Op::StoreOne,    nullptr },
    { "keep running on WM_KILLFOCUS",   site::KillFocusForeground, EXPECT(kSetFgApp),     Op::StoreOne,    nullptr },
    { "no pause menu on WM_SETFOCUS",   site::SetFocusMenu,        EXPECT(kOpenMenu),     Op::CallNullSub, nullptr },
    { "no camera resize on WM_SIZE",    site::WmSizeCameraResize,  EXPECT(kCameraResize), Op::CallNullSub, nullptr },
    { "fixed backbuffer (clear, w)",    site::ClearSizeJne,        EXPECT(kClearSizeJne), Op::Nop,         nullptr },
    { "fixed backbuffer (clear, h)",    site::ClearSizeJe,         EXPECT(kSizeJe),       Op::JeToJmp,     nullptr },
    { "fixed backbuffer (begin, w)",    site::BeginSizeJne,        EXPECT(kBeginSizeJne), Op::Nop,         nullptr },
    { "fixed backbuffer (begin, h)",    site::BeginSizeJe,         EXPECT(kSizeJe),       Op::JeToJmp,     nullptr },
    { "vsync off",                      site::VsyncBranch,         EXPECT(kVsyncBranch),  Op::JumpShort,   nullptr },
};

#undef EXPECT

bool Apply(const Patch& p) {
    switch (p.op) {
    case Op::JumpShort: {
        const uint8_t jmp = 0xEB;
        return mem::Write(p.addr, &jmp, sizeof(jmp));
    }
    case Op::CallNullSub:
        return mem::WriteRel32(p.addr, reinterpret_cast<uintptr_t>(&NullSub), 0xE8);
    case Op::Detour:
        return mem::Nop(p.addr, p.size) && mem::WriteRel32(p.addr, reinterpret_cast<uintptr_t>(p.target), 0xE9);
    case Op::Nop:
        return mem::Nop(p.addr, p.size);
    case Op::JeToJmp: {
        uint8_t code[6];
        std::memcpy(code, reinterpret_cast<const void*>(p.addr), sizeof(code));
        code[0] = 0x90;
        code[1] = 0xE9;
        return mem::Write(p.addr, code, sizeof(code));
    }
    case Op::StoreOne: {
        const uint32_t one = 1;
        return mem::Write(p.addr + p.size, &one, sizeof(one));
    }
    }
    return false;
}

bool IsGameVersion10US() {
    const auto* sig = reinterpret_cast<const uint32_t*>(game::addr::VersionSignature);
    return mem::IsReadable(sig, sizeof(*sig)) && *sig == 0x94BF;
}

void LogEnvironment() {
    logging::Info("%s %s (commit %s, built %s)", MOD_NAME, MOD_VERSION, MOD_COMMIT, MOD_BUILD_TIME);

    char exe[MAX_PATH] = "?";
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    logging::Info("game: %s", exe);

    using RtlGetVersion_t = LONG(WINAPI*)(OSVERSIONINFOW*);
    if (HMODULE ntdll = GetModuleHandleA("ntdll.dll")) {
        auto rtlGetVersion = reinterpret_cast<RtlGetVersion_t>(GetProcAddress(ntdll, "RtlGetVersion"));
        OSVERSIONINFOW os = { sizeof(os) };
        if (rtlGetVersion && rtlGetVersion(&os) == 0) {
            logging::Info("windows: %lu.%lu build %lu", os.dwMajorVersion, os.dwMinorVersion, os.dwBuildNumber);
        }
    }

    const RECT mon = window::MonitorRect();
    logging::Info("primary monitor: %ldx%ld", mon.right - mon.left, mon.bottom - mon.top);
}

}

bool Initialize(HMODULE self) {
    const Config cfg = config::Load(self);
    logging::Init(self, cfg.log);
    if (cfg.log) {
        LogEnvironment();
        crash::Install();
    }

    if (!IsGameVersion10US()) {
        logging::Error("unsupported gta_sa.exe (only 1.0 US is supported); nothing patched");
        return false;
    }

    for (const Patch& p : kPatches) {
        if (!mem::Matches(p.addr, p.expected, p.size)) {
            logging::Error("patch site 0x%06X (%s) was already modified, probably by another windowed/no-pause "
                           "mod; nothing patched", p.addr, p.name);
            return false;
        }
    }

    for (const Patch& p : kPatches) {
        if (!Apply(p)) {
            logging::Error("failed to write patch at 0x%06X (%s), error %lu", p.addr, p.name, GetLastError());
            return false;
        }
        logging::Info("patched 0x%06X  %s", p.addr, p.name);
    }

    window::InstallMouseScaling();
    logging::Info("initialized, waiting for the D3D device");
    return true;
}

}
