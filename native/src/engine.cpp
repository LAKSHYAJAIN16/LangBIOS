#include "langbios/engine.hpp"
#include "langbios/smbios.hpp"
#include "langbios/tpm.hpp"
#include "langbios/uefi_vars.hpp"
#include "langbios/vendor_bios.hpp"
#include <algorithm>
#include <cwctype>
#include <sstream>

namespace langbios {

namespace {

std::wstring ToLower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return std::towlower(c); });
    return s;
}

std::vector<std::wstring> SplitCsv(const std::wstring& s) {
    std::vector<std::wstring> parts;
    std::wstringstream ss(s);
    std::wstring item;
    while (std::getline(ss, item, L',')) {
        size_t start = item.find_first_not_of(L" \t");
        size_t end = item.find_last_not_of(L" \t");
        if (start == std::wstring::npos) continue;
        parts.push_back(item.substr(start, end - start + 1));
    }
    return parts;
}

// Real UEFI/WMI settings that don't go through the vendor attribute table.
bool IsCoreSetting(const std::wstring& s) {
    return s == L"secure_boot" || s == L"tpm" || s == L"boot_order";
}

Result GetCoreSetting(const std::wstring& s) {
    if (s == L"secure_boot") return GetSecureBootState();
    if (s == L"tpm") return GetTpmState();
    if (s == L"boot_order") return GetBootOrder();
    return Result::Failure(L"unreachable");
}

Result GetVendorSetting(const std::wstring& setting) {
    Vendor v = DetectVendor();
    auto backend = CreateVendorBackend(v);
    std::vector<BiosAttribute> attrs;
    Result r = backend->Enumerate(attrs);
    if (!r.ok) return r;

    std::wstring wanted = ToLower(setting);
    for (const auto& a : attrs) {
        std::wstring name = ToLower(a.name);
        if (name.find(wanted) != std::wstring::npos || wanted.find(name) != std::wstring::npos) {
            return Result::Success(a.name + L" = " + a.currentValue, a.currentValue);
        }
    }
    return Result::Failure(
        L"No real BIOS attribute on this " + std::wstring(VendorName(v)) +
        L" machine matches '" + setting + L"'. Use `list settings` to see the " +
        std::to_wstring(attrs.size()) + L" actual attribute names.");
}

Result SetVendorSetting(const std::wstring& setting, const std::wstring& value) {
    Vendor v = DetectVendor();
    auto backend = CreateVendorBackend(v);
    std::vector<BiosAttribute> attrs;
    Result r = backend->Enumerate(attrs);
    if (!r.ok) return r;

    std::wstring wanted = ToLower(setting);
    for (const auto& a : attrs) {
        std::wstring name = ToLower(a.name);
        if (name.find(wanted) != std::wstring::npos || wanted.find(name) != std::wstring::npos) {
            return backend->SetAttribute(a.name, value);
        }
    }
    return Result::Failure(
        L"No real BIOS attribute on this " + std::wstring(VendorName(v)) +
        L" machine matches '" + setting + L"'. Use `list settings` to see the " +
        std::to_wstring(attrs.size()) + L" actual attribute names.");
}

Result ListAll() {
    std::wstringstream ss;
    ss << GetSecureBootState().message << L"\n";
    ss << GetTpmState().message << L"\n";
    ss << GetBootOrder().message << L"\n";

    Vendor v = DetectVendor();
    auto backend = CreateVendorBackend(v);
    std::vector<BiosAttribute> attrs;
    Result r = backend->Enumerate(attrs);
    if (r.ok) {
        ss << L"Real " << VendorName(v) << L" BIOS attributes (" << attrs.size() << L"):\n";
        for (const auto& a : attrs) {
            ss << L"  " << a.name << L" = " << a.currentValue << L"\n";
        }
    } else {
        ss << r.message << L"\n";
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
                L"Resetting real BIOS settings to factory defaults isn't an "
                L"OS-writable operation on general hardware - it's done from "
                L"the physical UEFI setup menu.");

        case Action::Get: {
            if (cmd.setting.empty()) return Result::Failure(L"I didn't catch which setting you mean.");
            if (IsCoreSetting(cmd.setting)) return GetCoreSetting(cmd.setting);
            return GetVendorSetting(cmd.setting);
        }

        case Action::Set: {
            if (cmd.setting.empty()) return Result::Failure(L"I didn't catch which setting you mean.");
            if (cmd.setting == L"secure_boot" || cmd.setting == L"tpm") {
                return Result::Failure(
                    cmd.setting + L" is read-only from software: the firmware "
                    L"deliberately doesn't allow OS-level writes to it. Change it "
                    L"from the physical UEFI setup menu.");
            }
            if (cmd.setting == L"boot_order") {
                return SetBootOrderByDescriptions(SplitCsv(cmd.value));
            }
            return SetVendorSetting(cmd.setting, cmd.value);
        }

        default:
            return Result::Failure(
                L"I didn't understand that. Try things like 'enable secure boot', "
                L"'what's my fan profile', or 'list settings'.");
    }
}

} // namespace langbios
