#include "langbios/audio.hpp"
#include "langbios/engine.hpp"
#include "langbios/llm_fallback.hpp"
#include "langbios/tpm.hpp"
#include "langbios/uefi_vars.hpp"
#include "langbios/vendor_bios.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace langbios {

namespace {

std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

std::vector<std::string> SplitCsv(const std::string& s) {
    std::vector<std::string> parts;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        size_t start = item.find_first_not_of(" \t");
        size_t end = item.find_last_not_of(" \t");
        if (start == std::string::npos) continue;
        parts.push_back(item.substr(start, end - start + 1));
    }
    return parts;
}

// Settings with their own real backend (firmware or OS-level) that
// don't go through the vendor BIOS attribute table.
bool IsCoreSetting(const std::string& s) {
    return s == "secure_boot" || s == "tpm" || s == "boot_order" || s == "volume" || s == "mute";
}

Result GetCoreSetting(const std::string& s) {
    if (s == "secure_boot") return GetSecureBootState();
    if (s == "tpm") return GetTpmState();
    if (s == "boot_order") return GetBootOrder();
    if (s == "volume") return GetVolume();
    if (s == "mute") return GetMute();
    return Result::Failure("unreachable");
}

Result GetVendorSetting(const std::string& setting) {
    Vendor v = DetectVendor();
    auto backend = CreateVendorBackend(v);
    std::vector<BiosAttribute> attrs;
    Result r = backend->Enumerate(attrs);
    if (!r.ok) return r;

    std::string wanted = ToLower(setting);
    for (const auto& a : attrs) {
        std::string name = ToLower(a.name);
        if (name.find(wanted) != std::string::npos || wanted.find(name) != std::string::npos) {
            return Result::Success(a.name + " = " + a.currentValue, a.currentValue);
        }
    }
    return Result::Failure(
        "No real BIOS attribute on this " + std::string(VendorName(v)) +
        " machine matches '" + setting + "'. Use `list settings` to see the " +
        std::to_string(attrs.size()) + " actual attribute names.");
}

Result SetVendorSetting(const std::string& setting, const std::string& value) {
    Vendor v = DetectVendor();
    auto backend = CreateVendorBackend(v);
    std::vector<BiosAttribute> attrs;
    Result r = backend->Enumerate(attrs);
    if (!r.ok) return r;

    std::string wanted = ToLower(setting);
    for (const auto& a : attrs) {
        std::string name = ToLower(a.name);
        if (name.find(wanted) != std::string::npos || wanted.find(name) != std::string::npos) {
            return backend->SetAttribute(a.name, value);
        }
    }
    return Result::Failure(
        "No real BIOS attribute on this " + std::string(VendorName(v)) +
        " machine matches '" + setting + "'. Use `list settings` to see the " +
        std::to_string(attrs.size()) + " actual attribute names.");
}

Result ListAll() {
    std::stringstream ss;
    ss << GetSecureBootState().message << "\n";
    ss << GetTpmState().message << "\n";
    ss << GetBootOrder().message << "\n";
    ss << GetVolume().message << "\n";
    ss << GetMute().message << "\n";

    Vendor v = DetectVendor();
    auto backend = CreateVendorBackend(v);
    std::vector<BiosAttribute> attrs;
    Result r = backend->Enumerate(attrs);
    if (r.ok) {
        ss << "Real " << VendorName(v) << " BIOS attributes (" << attrs.size() << "):\n";
        for (const auto& a : attrs) {
            ss << "  " << a.name << " = " << a.currentValue << "\n";
        }
    } else {
        ss << r.message << "\n";
    }
    return Result::Success(ss.str());
}

} // namespace

Result Execute(const Command& cmd) {
    switch (cmd.action) {
        case Action::List:
            return ListAll();

        case Action::Reset:
            return Result::Failure(
                "Resetting real BIOS settings to factory defaults isn't an "
                "OS-writable operation on general hardware - it's done from "
                "the physical UEFI setup menu.");

        case Action::Get: {
            if (cmd.setting.empty()) return Result::Failure("I didn't catch which setting you mean.");
            if (IsCoreSetting(cmd.setting)) return GetCoreSetting(cmd.setting);
            return GetVendorSetting(cmd.setting);
        }

        case Action::Set: {
            if (cmd.setting.empty()) return Result::Failure("I didn't catch which setting you mean.");
            if (cmd.setting == "secure_boot" || cmd.setting == "tpm") {
                return Result::Failure(
                    cmd.setting + " is read-only from software: the firmware "
                    "deliberately doesn't allow OS-level writes to it. Change it "
                    "from the physical UEFI setup menu.");
            }
            if (cmd.setting == "boot_order") {
                return SetBootOrderByDescriptions(SplitCsv(cmd.value));
            }
            if (cmd.setting == "volume") {
                try {
                    return SetVolume(std::stoi(cmd.value));
                } catch (...) {
                    return Result::Failure("volume needs a number 0-100, got '" + cmd.value + "'.");
                }
            }
            if (cmd.setting == "mute") {
                return SetMute(cmd.value == "true");
            }
            return SetVendorSetting(cmd.setting, cmd.value);
        }

        default:
            return Result::Failure(
                "I didn't understand that. Try things like 'enable secure boot', "
                "'what's my fan profile', or 'list settings'.");
    }
}

Result Interpret(const std::string& text) {
    Command cmd;
    if (ParseRule(text, cmd)) {
        return Execute(cmd);
    }

    // Semantic fallback: only ever returns true with a confident match
    // above the similarity threshold (see llm_fallback.cpp), so there's
    // no separate "unknown but matched" case to handle here - either it
    // found a real canonical intent or it didn't.
    Command llmCmd;
    if (LlmFallbackParse(text, llmCmd)) {
        return Execute(llmCmd);
    }

    return Result::Failure(
        "I didn't understand that. Try things like 'enable secure boot', "
        "'what's my fan profile', or 'list settings'.");
}

} // namespace langbios
