#pragma once
#include <string>

namespace langbios {

// Result of any operation against real firmware/BIOS management interfaces.
// `ok` distinguishes success from a clearly-explained failure (unsupported
// setting, missing privilege, no vendor interface, etc.) - callers should
// always be able to show `message` to a human as-is.
struct Result {
    bool ok = false;
    std::wstring message;
    std::wstring value; // populated for get-style operations

    static Result Success(std::wstring msg, std::wstring val = L"") {
        return Result{true, std::move(msg), std::move(val)};
    }
    static Result Failure(std::wstring msg) {
        return Result{false, std::move(msg), L""};
    }
};

} // namespace langbios
