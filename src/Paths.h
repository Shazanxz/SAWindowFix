#pragma once

#include <windows.h>
#include <cstring>

namespace paths {

inline void Sibling(HMODULE module, char (&path)[MAX_PATH], const char* ext) {
    const DWORD len = GetModuleFileNameA(module, path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        path[0] = '\0';
        return;
    }
    char* name = std::strrchr(path, '\\');
    char* dot  = std::strrchr(name ? name : path, '.');
    char* end  = dot ? dot : path + len;
    strcpy_s(end, static_cast<size_t>(path + MAX_PATH - end), ext);
}

}
