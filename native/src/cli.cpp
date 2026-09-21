// Native CLI: talks to REAL BIOS/UEFI firmware, no mocking. Mirrors the
// Python CLI's command surface (same rule-based phrasing) but reads/writes
// actual firmware state via uefi_vars/tpm/vendor_bios instead of a JSON
// file. Needs to run elevated (Administrator on Windows, root on Linux)
// for anything beyond vendor-attribute reads. Shared between the Windows
// and Linux builds - narrow UTF-8 std::string throughout, no platform
// headers needed here.
#include <iostream>
#include <string>

#include "langbios/engine.hpp"
#include "langbios/rule_parser.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace {

void RunOne(const std::string& text) {
    langbios::Command cmd;
    langbios::Result result;
    if (langbios::ParseRule(text, cmd)) {
        result = langbios::Execute(cmd);
    } else {
        result = langbios::Result::Failure(
            "I didn't understand that. Try things like 'enable secure boot', "
            "'what's my fan profile', or 'list settings'.");
    }
    std::cout << result.message << std::endl;
}

const char* kBanner =
    "LangBIOS (native) - talks to REAL firmware, not a mock.\n"
    "Boot order + Secure Boot/TPM reads work on any UEFI machine (needs "
    "elevation). Other settings need this to be Dell/HP/Lenovo hardware on "
    "Windows, or any hardware whose vendor driver exposes "
    "/sys/class/firmware-attributes on Linux.\n"
    "Type things like:\n"
    "  enable secure boot\n"
    "  what's my fan profile\n"
    "  list settings\n"
    "Type 'exit' or 'quit' to leave.\n";

} // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    if (argc > 1) {
        std::string text;
        for (int i = 1; i < argc; ++i) {
            if (i > 1) text += " ";
            text += argv[i];
        }
        RunOne(text);
        return 0;
    }

    std::cout << kBanner;
    std::string line;
    while (true) {
        std::cout << "langbios-native> ";
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;
        if (line == "exit" || line == "quit") break;
        RunOne(line);
    }
    return 0;
}
