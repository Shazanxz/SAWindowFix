#pragma once

#include <windows.h>
#include <cstddef>
#include <cstdint>

namespace logging {

void Init(HMODULE self, bool enabled);
bool Enabled();

// Starts a "== name ====" block; does nothing if that section is already the current one.
// `name` must be a string literal (the pointer is kept).
void Section(const char* name);

void Info(_Printf_format_string_ const char* fmt, ...);
void Warn(_Printf_format_string_ const char* fmt, ...);
void Error(_Printf_format_string_ const char* fmt, ...);

bool DescribeAddress(uintptr_t addr, char* out, size_t outSize);

}
