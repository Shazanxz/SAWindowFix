#include "Mod.h"

#include <windows.h>

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        mod::Initialize(module);
    }
    return TRUE;
}
