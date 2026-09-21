#pragma once
// Windows-only wide<->UTF-8 conversion helpers. Every cross-platform type
// (Result, Command, BiosAttribute, BootEntry) is narrow UTF-8 std::string;
// these helpers exist purely so the Windows-specific files (which must
// call wide Win32/WMI APIs) can convert right at their own boundary.
#include <string>

namespace langbios {

std::string WideToUtf8(const std::wstring& w);
std::wstring Utf8ToWide(const std::string& s);

} // namespace langbios
