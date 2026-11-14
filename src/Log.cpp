#include "Log.h"

#include "Paths.h"
#include "Version.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <share.h>

namespace logging {
namespace {

constexpr int kRuleWidth = 77;

bool             g_enabled = false;
FILE*            g_file    = nullptr;
DWORD            g_startTick = 0;
CRITICAL_SECTION g_lock;
char             g_line[2048];
const char*      g_section = nullptr;

enum class Level { Info, Warn, Error };

const char* Tag(Level level) {
    switch (level) {
    case Level::Warn:  return "WARN ";
    case Level::Error: return "ERROR";
    default:           return "     ";
    }
}

// Caller holds g_lock.
void Emit(const char* text) {
    OutputDebugStringA(text);
    if (g_file) {
        std::fputs(text, g_file);
        std::fflush(g_file);
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

    char uptime[24];
    std::snprintf(uptime, sizeof(uptime), "+%lu.%03lus", elapsed / 1000, elapsed % 1000);

    constexpr size_t kMax = sizeof(g_line) - 2;
    size_t len = 0;
    int n = std::snprintf(g_line, kMax, "%02u:%02u:%02u.%03u %9s  %s ", t.wHour, t.wMinute, t.wSecond,
                          t.wMilliseconds, uptime, Tag(level));
    if (n > 0) {
        len = std::min(static_cast<size_t>(n), kMax - 1);
    }
    n = std::vsnprintf(g_line + len, kMax - len, fmt, args);
    if (n > 0) {
        len = std::min(len + static_cast<size_t>(n), kMax - 1);
    }
    g_line[len++] = '\n';
    g_line[len]   = '\0';
    Emit(g_line);

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

    SYSTEMTIME t;
    GetLocalTime(&t);
    EnterCriticalSection(&g_lock);
    std::snprintf(g_line, sizeof(g_line), "%s %s (commit %s, built %s)\nlog started %04u-%02u-%02u %02u:%02u:%02u.%03u\n",
                  MOD_NAME, MOD_VERSION, MOD_COMMIT, MOD_BUILD_TIME, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute,
                  t.wSecond, t.wMilliseconds);
    Emit(g_line);
    LeaveCriticalSection(&g_lock);
}

bool Enabled() {
    return g_enabled;
}

void Section(const char* name) {
    if (!g_enabled) {
        return;
    }
    EnterCriticalSection(&g_lock);
    if (g_section == nullptr || std::strcmp(g_section, name) != 0) {
        g_section = name;
        int n = std::snprintf(g_line, sizeof(g_line) - 2, "\n== %s ", name);
        n = std::max(0, std::min(n, static_cast<int>(sizeof(g_line)) - 3));
        while (n < kRuleWidth) {
            g_line[n++] = '=';
        }
        g_line[n++] = '\n';
        g_line[n]   = '\0';
        Emit(g_line);
    }
    LeaveCriticalSection(&g_lock);
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
