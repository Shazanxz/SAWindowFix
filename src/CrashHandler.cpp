#include "CrashHandler.h"

#include "Log.h"
#include "Memory.h"

#include <windows.h>
#include <cstdint>

namespace crash {
namespace {

constexpr LONG kMaxReports  = 8;
constexpr int  kStackDump   = 16;
constexpr int  kStackScan   = 64;

volatile LONG g_reports = 0;
volatile LONG g_busy    = 0;

const char* CodeName(DWORD code) {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:      return "ACCESS_VIOLATION";
    case EXCEPTION_ILLEGAL_INSTRUCTION:   return "ILLEGAL_INSTRUCTION";
    case EXCEPTION_PRIV_INSTRUCTION:      return "PRIV_INSTRUCTION";
    case EXCEPTION_STACK_OVERFLOW:        return "STACK_OVERFLOW";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:    return "INT_DIVIDE_BY_ZERO";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "ARRAY_BOUNDS_EXCEEDED";
    default:                              return nullptr;
    }
}

const char* AccessKind(ULONG_PTR kind) {
    switch (kind) {
    case 0:  return "read from";
    case 1:  return "write to";
    case 8:  return "execute";
    default: return "access";
    }
}

void Report(const EXCEPTION_POINTERS* ep, const char* name) {
    static char where[MAX_PATH + 32];
    static char desc[MAX_PATH + 32];

    const EXCEPTION_RECORD* rec = ep->ExceptionRecord;
    const CONTEXT*          ctx = ep->ContextRecord;
    const auto addr = reinterpret_cast<uintptr_t>(rec->ExceptionAddress);
    if (!logging::DescribeAddress(addr, where, sizeof(where))) {
        lstrcpyA(where, "no module");
    }

    logging::Section("crash");
    logging::Error("%s (0x%08lX) at 0x%08X [%s], thread %lu (first-chance, may still be handled)", name,
                   rec->ExceptionCode, addr, where, GetCurrentThreadId());
    if (rec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && rec->NumberParameters >= 2) {
        logging::Error("  attempted to %s 0x%08X", AccessKind(rec->ExceptionInformation[0]),
                       static_cast<unsigned>(rec->ExceptionInformation[1]));
    }
    logging::Error("  eax=%08lX ebx=%08lX ecx=%08lX edx=%08lX esi=%08lX edi=%08lX ebp=%08lX esp=%08lX eip=%08lX",
                   ctx->Eax, ctx->Ebx, ctx->Ecx, ctx->Edx, ctx->Esi, ctx->Edi, ctx->Ebp, ctx->Esp, ctx->Eip);

    if (rec->ExceptionCode == EXCEPTION_STACK_OVERFLOW) {
        return;
    }

    const auto* stack = reinterpret_cast<const uint32_t*>(ctx->Esp);
    if (mem::IsReadable(stack, kStackDump * sizeof(uint32_t))) {
        for (int i = 0; i < kStackDump; i += 8) {
            logging::Error("  [esp+%02X] %08X %08X %08X %08X %08X %08X %08X %08X", i * 4, stack[i], stack[i + 1],
                           stack[i + 2], stack[i + 3], stack[i + 4], stack[i + 5], stack[i + 6], stack[i + 7]);
        }
    }
    if (mem::IsReadable(stack, kStackScan * sizeof(uint32_t))) {
        for (int i = 0; i < kStackScan; ++i) {
            if (stack[i] > 0x10000 && logging::DescribeAddress(stack[i], desc, sizeof(desc))) {
                logging::Error("  [esp+%03X] %08X = %s", i * 4, stack[i], desc);
            }
        }
    }
}

LONG CALLBACK Handler(EXCEPTION_POINTERS* ep) {
    const char* name = CodeName(ep->ExceptionRecord->ExceptionCode);
    if (!name || g_reports >= kMaxReports) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (InterlockedCompareExchange(&g_busy, 1, 0) != 0) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (InterlockedIncrement(&g_reports) <= kMaxReports) {
        Report(ep, name);
    }
    InterlockedExchange(&g_busy, 0);
    return EXCEPTION_CONTINUE_SEARCH;
}

}

void Install() {
    AddVectoredExceptionHandler(0, Handler);
}

}
