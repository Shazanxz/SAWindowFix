#include "Config.h"

#include "Paths.h"

#include <cstdio>
#include <share.h>

namespace config {
namespace {

constexpr const char* kSection = "SAWindowFix";

constexpr const char* kDefaultIni =
    "[SAWindowFix]\n"
    "; Write SAWindowFix.log next to the .asi (0 = off, 1 = on)\n"
    "Log=0\n";

}

Config Load(HMODULE self) {
    Config cfg;
    char path[MAX_PATH];
    paths::Sibling(self, path, ".ini");
    if (!path[0]) {
        return cfg;
    }

    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) {
        if (FILE* f = _fsopen(path, "w", _SH_DENYNO)) {
            std::fputs(kDefaultIni, f);
            std::fclose(f);
        }
    }

    cfg.log = GetPrivateProfileIntA(kSection, "Log", 0, path) != 0;
    return cfg;
}

}
