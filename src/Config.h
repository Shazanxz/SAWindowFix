#pragma once

#include <windows.h>

struct Config {
    bool log = false;
};

namespace config {

Config Load(HMODULE self);

}
