#include "langbios/rule_parser.hpp"
#include <algorithm>
#include <cwctype>
#include <map>
#include <regex>
#include <vector>

namespace langbios {

namespace {

std::wstring ToLower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return std::towlower(c); });
    return s;
}

const std::wregex kOnWords(LR"(\b(?:turn on|enable|enabled|activate|on)\b)");
const std::wregex kOffWords(LR"(\b(?:turn off|disable|disabled|deactivate|off)\b)");
const std::wregex kQueryWords(LR"(\b(?:what's|what is|check|status of|is)\b)");
const std::wregex kListWords(LR"(\b(?:list|show|dump)\b.*\bsettings?\b)");
const std::wregex kResetWords(LR"(\breset\b.*\b(?:default|factory)\w*)");
const std::wregex kBootOrderValue(LR"((?:to|:)\s*([a-z0-9, ]+)$)");

const std::map<std::wstring, std::vector<std::wstring>> kAliases = {
    {L"secure_boot", {L"secure boot", L"secureboot"}},
    {L"virtualization", {L"virtualization", L"virtualisation", L"vt-x", L"vtx", L"svm", L"amd-v"}},
    {L"tpm", {L"tpm", L"trusted platform module"}},
    {L"fast_boot", {L"fast boot", L"fastboot", L"quick boot"}},
    {L"xmp", {L"xmp", L"docp", L"memory profile", L"ram overclock"}},
    {L"cpu_turbo", {L"turbo", L"cpu turbo", L"boost clock", L"turbo boost"}},
    {L"power_profile", {L"power profile", L"power plan", L"power mode"}},
    {L"fan_profile", {L"fan profile", L"fan curve", L"fan speed", L"fan mode"}},
    {L"boot_order", {L"boot order", L"boot priority", L"boot sequence"}},
};

const std::vector<std::wstring> kBoolSettings = {
    L"secure_boot", L"virtualization", L"tpm", L"fast_boot", L"xmp", L"cpu_turbo"};

const std::map<std::wstring, std::vector<std::wstring>> kEnumValues = {
    {L"power_profile", {L"power_saver", L"power saver", L"balanced", L"performance"}},
    {L"fan_profile", {L"silent", L"standard", L"performance", L"full speed", L"full_speed"}},
};

bool Contains(const std::vector<std::wstring>& v, const std::wstring& x) {
    return std::find(v.begin(), v.end(), x) != v.end();
}

std::wstring FindSetting(const std::wstring& t) {
    for (const auto& kv : kAliases) {
        for (const auto& phrase : kv.second) {
            if (t.find(phrase) != std::wstring::npos) return kv.first;
        }
    }
    return L"";
}

} // namespace

bool ParseRule(const std::wstring& text, Command& out) {
    std::wstring t = ToLower(text);

    if (std::regex_search(t, kListWords) || t == L"list" || t == L"settings" || t == L"status") {
        out = Command{Action::List, L"", L"", text};
        return true;
    }
    if (std::regex_search(t, kResetWords)) {
        out = Command{Action::Reset, L"", L"", text};
        return true;
    }

    std::wstring setting = FindSetting(t);
    if (setting.empty()) return false;

    if (Contains(kBoolSettings, setting)) {
        if (std::regex_search(t, kOnWords)) {
            out = Command{Action::Set, setting, L"true", text};
            return true;
        }
        if (std::regex_search(t, kOffWords)) {
            out = Command{Action::Set, setting, L"false", text};
            return true;
        }
        if (std::regex_search(t, kQueryWords)) {
            out = Command{Action::Get, setting, L"", text};
            return true;
        }
        return false;
    }

    auto enumIt = kEnumValues.find(setting);
    if (enumIt != kEnumValues.end()) {
        for (const auto& value : enumIt->second) {
            if (t.find(value) != std::wstring::npos) {
                out = Command{Action::Set, setting, value, text};
                return true;
            }
        }
        if (std::regex_search(t, kQueryWords)) {
            out = Command{Action::Get, setting, L"", text};
            return true;
        }
        return false;
    }

    if (setting == L"boot_order") {
        std::wsmatch m;
        if (std::regex_search(t, m, kBootOrderValue)) {
            out = Command{Action::Set, setting, m[1].str(), text};
            return true;
        }
        if (std::regex_search(t, std::wregex(LR"(\b(?:what's|what is|show)\b)"))) {
            out = Command{Action::Get, setting, L"", text};
            return true;
        }
        return false;
    }

    return false;
}

} // namespace langbios
