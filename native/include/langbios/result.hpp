#pragma once
#include <string>

namespace langbios {

// Result of any operation against real firmware/BIOS management interfaces.
// UTF-8 std::string throughout (not std::wstring) so this type - and
// everything built on it (Command, engine, capi) - is shared as-is
// between the Windows and Linux backends; only the low-level
// Windows-specific files (WMI, Win32 firmware APIs) need to convert to
///from wide strings internally, right at their own boundary.
//
// `ok` distinguishes success from a clearly-explained failure (unsupported
// setting, missing privilege, no vendor interface, etc.) - callers should
// always be able to show `message` to a human as-is.
struct Result {
    bool ok = false;
    std::string message;
    std::string value; // populated for get-style operations

    static Result Success(std::string msg, std::string val = "") {
        return Result{true, std::move(msg), std::move(val)};
    }
    static Result Failure(std::string msg) {
        return Result{false, std::move(msg), ""};
    }
};

} // namespace langbios
