#pragma once

#include <windows.h>
#include <cstdint>
#include <cstring>

namespace mem {

inline bool Write(uintptr_t addr, const void* data, size_t size) {
    auto* dst = reinterpret_cast<void*>(addr);
    DWORD oldProtect;
    if (!VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }
    std::memcpy(dst, data, size);
    VirtualProtect(dst, size, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), dst, size);
    return true;
}

inline bool Fill(uintptr_t addr, uint8_t value, size_t size) {
    auto* dst = reinterpret_cast<void*>(addr);
    DWORD oldProtect;
    if (!VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }
    std::memset(dst, value, size);
    VirtualProtect(dst, size, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), dst, size);
    return true;
}

inline bool Nop(uintptr_t addr, size_t size) {
    return Fill(addr, 0x90, size);
}

inline bool WriteRel32(uintptr_t addr, uintptr_t target, uint8_t opcode) {
    uint8_t code[5];
    code[0] = opcode;
    const auto disp = static_cast<uint32_t>(target - (addr + sizeof(code)));
    std::memcpy(&code[1], &disp, sizeof(disp));
    return Write(addr, code, sizeof(code));
}

inline bool IsReadable(const void* ptr, size_t size) {
    constexpr DWORD kReadable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
                                PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    auto p = reinterpret_cast<uintptr_t>(ptr);
    const uintptr_t end = p + size;
    while (p < end) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery(reinterpret_cast<const void*>(p), &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT ||
            (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) || !(mbi.Protect & kReadable)) {
            return false;
        }
        p = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    }
    return true;
}

inline bool Matches(uintptr_t addr, const void* expected, size_t size) {
    return IsReadable(reinterpret_cast<const void*>(addr), size) &&
           std::memcmp(reinterpret_cast<const void*>(addr), expected, size) == 0;
}

}
