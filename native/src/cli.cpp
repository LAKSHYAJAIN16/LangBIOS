// Native CLI: talks to REAL BIOS/UEFI firmware, no mocking. Mirrors the
// Python CLI's command surface (same rule-based phrasing) but reads/writes
// actual firmware state via uefi_vars/tpm/vendor_bios instead of a JSON
// file. Needs to run elevated for anything beyond vendor-attribute reads.
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <iostream>
#include <string>

#include "langbios/engine.hpp"
#include "langbios/rule_parser.hpp"

namespace {

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), len);
    return w;
}

// Printing wide strings straight to a redirected/piped stdout via
// _O_U16TEXT is unreliable outside a real console; converting to UTF-8
// and writing narrow bytes works everywhere (console, pipes, files).
std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), len, nullptr, nullptr);
    return s;
}

void RunOne(const std::wstring& text) {
    langbios::Command cmd;
    langbios::Result result;
    if (langbios::ParseRule(text, cmd)) {
        result = langbios::Execute(cmd);
    } else {
        result = langbios::Result::Failure(
            L"I didn't understand that. Try things like 'enable secure boot', "
            L"'what's my fan profile', or 'list settings'.");
    }
    std::cout << WideToUtf8(result.message) << std::endl;
}

const char* kBanner =
    "LangBIOS (native) - talks to REAL firmware, not a mock.\n"
    "Boot order + Secure Boot/TPM reads work on any UEFI machine (needs "
    "elevation). Other settings need this to be Dell/HP/Lenovo hardware.\n"
    "Type things like:\n"
    "  enable secure boot\n"
    "  what's my fan profile\n"
    "  list settings\n"
    "Type 'exit' or 'quit' to leave.\n";

} // namespace

int wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);

    if (argc > 1) {
        std::wstring text;
        for (int i = 1; i < argc; ++i) {
            if (i > 1) text += L" ";
            text += argv[i];
        }
        RunOne(text);
        return 0;
    }

    std::cout << kBanner;
    std::string narrowLine;
    while (true) {
        std::cout << "langbios-native> ";
        if (!std::getline(std::cin, narrowLine)) break;
        if (narrowLine.empty()) continue;
        if (narrowLine == "exit" || narrowLine == "quit") break;
        RunOne(Utf8ToWide(narrowLine));
    }
    return 0;
}
