#include "Log.h"

#include "Paths.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <share.h>

namespace logging {
namespace {

bool             g_enabled = false;
FILE*            g_file    = nullptr;
DWORD            g_startTick = 0;
CRITICAL_SECTION g_lock;
char             g_line[2048];

enum class Level { Info, Warn, Error };

const char* Tag(Level level) {
    switch (level) {
    case Level::Warn:  return "WARN ";
    case Level::Error: return "ERROR";
    default:           return "INFO ";
    }
}

void VWrite(Level level, const char* fmt, va_list args) {
    if (!g_enabled) {
        return;
    }
    EnterCriticalSection(&g_lock);

    SYSTEMTIME t;
    GetLocalTime(&t);
    const DWORD elapsed = GetTickCount() - g_startTick;

    constexpr size_t kMax = sizeof(g_line) - 2;
    size_t len = 0;
    int n = std::snprintf(g_line, kMax, "%02u:%02u:%02u.%03u  %5lu.%03lus  %s  ", t.wHour, t.wMinute, t.wSecond,
                          t.wMilliseconds, elapsed / 1000, elapsed % 1000, Tag(level));
    if (n > 0) {
        len = std::min(static_cast<size_t>(n), kMax - 1);
    }
    n = std::vsnprintf(g_line + len, kMax - len, fmt, args);
    if (n > 0) {
        len = std::min(len + static_cast<size_t>(n), kMax - 1);
    }
    g_line[len++] = '\n';
    g_line[len]   = '\0';

    OutputDebugStringA(g_line);
    if (g_file) {
        std::fputs(g_line, g_file);
        std::fflush(g_file);
    }

    LeaveCriticalSection(&g_lock);
}

}

void Init(HMODULE self, bool enabled) {
    if (!enabled || g_enabled) {
        return;
    }
    InitializeCriticalSection(&g_lock);
    g_startTick = GetTickCount();

    char path[MAX_PATH];
    paths::Sibling(self, path, ".log");
    if (path[0]) {
        g_file = _fsopen(path, "w", _SH_DENYNO);
    }
    g_enabled = true;
}

bool Enabled() {
    return g_enabled;
}

void Info(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    VWrite(Level::Info, fmt, args);
    va_end(args);
}

void Warn(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    VWrite(Level::Warn, fmt, args);
    va_end(args);
}

void Error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    VWrite(Level::Error, fmt, args);
    va_end(args);
}

bool DescribeAddress(uintptr_t addr, char* out, size_t outSize) {
    HMODULE mod = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCSTR>(addr), &mod)) {
        return false;
    }
    char path[MAX_PATH] = "?";
    GetModuleFileNameA(mod, path, MAX_PATH);
    const char* slash = std::strrchr(path, '\\');
    std::snprintf(out, outSize, "%s+0x%X", slash ? slash + 1 : path,
                  static_cast<unsigned>(addr - reinterpret_cast<uintptr_t>(mod)));
    return true;
}

}
