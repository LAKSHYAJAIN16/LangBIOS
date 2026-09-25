#include "langbios/rule_parser.hpp"
#include <algorithm>
#include <cctype>
#include <map>
#include <regex>
#include <vector>

namespace langbios {

namespace {

std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

const std::regex kOnWords(R"(\b(?:turn on|enable|enabled|activate|on)\b)");
const std::regex kOffWords(R"(\b(?:turn off|disable|disabled|deactivate|off)\b)");
const std::regex kQueryWords(R"(\b(?:what's|what is|check|status of|is)\b)");
// A sentence *opening* as a question is a read even when it contains an
// on/off word or a value ("is tpm enabled", "is my fan profile silent") -
// checking kOnWords first used to turn those into writes. Anchored at the
// start so "can you enable secure boot?" still stays a write.
const std::regex kQuestionStart(
    R"(^\s*(?:is|are|was|does|do i have|what's|whats|what is|what|check|status of|tell me)\b)");
const std::regex kListWords(R"(\b(?:list|show|dump)\b.*\bsettings?\b)");
const std::regex kResetWords(R"(\breset\b.*\b(?:default|factory)\w*)");
const std::regex kBootOrderValue(R"((?:to|:)\s*([a-z0-9, ]+)$)");
const std::regex kBootOrderQuery(R"(\b(?:what's|what is|show)\b)");
const std::regex kVolumeNumber(R"((\d{1,3}))");

const std::map<std::string, std::vector<std::string>> kAliases = {
    {"secure_boot", {"secure boot", "secureboot"}},
    {"virtualization", {"virtualization", "virtualisation", "vt-x", "vtx", "svm", "amd-v"}},
    {"tpm", {"tpm", "trusted platform module"}},
    {"fast_boot", {"fast boot", "fastboot", "quick boot"}},
    {"xmp", {"xmp", "docp", "memory profile", "ram overclock"}},
    {"cpu_turbo", {"turbo", "cpu turbo", "boost clock", "turbo boost"}},
    {"power_profile", {"power profile", "power plan", "power mode"}},
    {"fan_profile", {"fan profile", "fan curve", "fan speed", "fan mode"}},
    {"boot_order", {"boot order", "boot priority", "boot sequence"}},
    {"volume", {"volume", "audio volume", "sound volume"}},
    {"mute", {"unmute", "mute", "audio mute", "sound mute"}},
};

const std::vector<std::string> kBoolSettings = {
    "secure_boot", "virtualization", "tpm", "fast_boot", "xmp", "cpu_turbo"};

const std::map<std::string, std::vector<std::string>> kEnumValues = {
    {"power_profile", {"power_saver", "power saver", "balanced", "performance"}},
    {"fan_profile", {"silent", "standard", "performance", "full speed", "full_speed"}},
};

bool Contains(const std::vector<std::string>& v, const std::string& x) {
    return std::find(v.begin(), v.end(), x) != v.end();
}

std::string FindSetting(const std::string& t) {
    for (const auto& kv : kAliases) {
        for (const auto& phrase : kv.second) {
            if (t.find(phrase) != std::string::npos) return kv.first;
        }
    }
    return "";
}

} // namespace

bool ParseRule(const std::string& text, Command& out) {
    std::string t = ToLower(text);

    // Only fires on an unambiguous, explicit request for everything -
    // bare "settings" or "status" alone used to trigger this too, which
    // meant any input that happened to be just that one word (not
    // necessarily meaning "list everything") dumped the full state
    // instead of being treated as unclear.
    if (std::regex_search(t, kListWords) || t == "list") {
        out = Command{Action::List, "", "", text};
        return true;
    }
    if (std::regex_search(t, kResetWords)) {
        out = Command{Action::Reset, "", "", text};
        return true;
    }

    std::string setting = FindSetting(t);
    if (setting.empty()) return false;

    if (std::regex_search(t, kQuestionStart)) {
        out = Command{Action::Get, setting, "", text};
        return true;
    }

    if (Contains(kBoolSettings, setting)) {
        if (std::regex_search(t, kOnWords)) {
            out = Command{Action::Set, setting, "true", text};
            return true;
        }
        if (std::regex_search(t, kOffWords)) {
            out = Command{Action::Set, setting, "false", text};
            return true;
        }
        if (std::regex_search(t, kQueryWords)) {
            out = Command{Action::Get, setting, "", text};
            return true;
        }
        return false;
    }

    auto enumIt = kEnumValues.find(setting);
    if (enumIt != kEnumValues.end()) {
        for (const auto& value : enumIt->second) {
            if (t.find(value) != std::string::npos) {
                out = Command{Action::Set, setting, value, text};
                return true;
            }
        }
        if (std::regex_search(t, kQueryWords)) {
            out = Command{Action::Get, setting, "", text};
            return true;
        }
        return false;
    }

    if (setting == "boot_order") {
        std::smatch m;
        if (std::regex_search(t, m, kBootOrderValue)) {
            out = Command{Action::Set, setting, m[1].str(), text};
            return true;
        }
        if (std::regex_search(t, kBootOrderQuery)) {
            out = Command{Action::Get, setting, "", text};
            return true;
        }
        return false;
    }

    if (setting == "volume") {
        std::smatch m;
        if (std::regex_search(t, m, kVolumeNumber)) {
            out = Command{Action::Set, setting, m[1].str(), text};
            return true;
        }
        if (std::regex_search(t, kQueryWords)) {
            out = Command{Action::Get, setting, "", text};
            return true;
        }
        return false;
    }

    if (setting == "mute") {
        // "mute"/"unmute" are themselves the on/off verbs here (nobody
        // says "turn mute on") - kQueryWords is checked first since
        // "is" would otherwise never fire (both mute/unmute phrases
        // already matched via FindSetting above).
        if (std::regex_search(t, kQueryWords)) {
            out = Command{Action::Get, setting, "", text};
            return true;
        }
        bool wantsUnmute = t.find("unmute") != std::string::npos;
        out = Command{Action::Set, setting, wantsUnmute ? "false" : "true", text};
        return true;
    }

    return false;
}

} // namespace langbios
