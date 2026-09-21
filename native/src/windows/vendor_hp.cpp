// HP's real BIOS management interface: WMI namespace root\hp\instrumentedBIOS.
// HP_BIOSEnumeration lists settings; HP_BIOSSettingInterface::SetBIOSSetting
// is the real write path. Reference: HP Client Management Script Library /
// HP BIOS Configuration Utility WMI documentation.
#include "langbios/vendor_bios.hpp"
#include "wmi_session.hpp"
#include "win_strings.hpp"
#include <cstring>
#include <sstream>

namespace langbios {

namespace {

class HpBiosBackend : public IVendorBiosBackend {
public:
    std::string Name() const override { return "HP"; }

    bool Supported() override {
        try {
            WmiSession session(L"root\\hp\\instrumentedBIOS");
            session.Query(L"SELECT * FROM HP_BIOSEnumeration");
            return true;
        } catch (...) {
            return false;
        }
    }

    Result Enumerate(std::vector<BiosAttribute>& out) override {
        try {
            WmiSession session(L"root\\hp\\instrumentedBIOS");
            auto rows = session.Query(L"SELECT * FROM HP_BIOSEnumeration");
            out.clear();
            for (const auto& row : rows) {
                BiosAttribute attr;
                auto nameIt = row.find(L"Name");
                auto valIt = row.find(L"Value");
                auto possIt = row.find(L"PossibleValues");
                if (nameIt == row.end()) continue;
                attr.name = WideToUtf8(nameIt->second);
                attr.currentValue = valIt != row.end() ? WideToUtf8(valIt->second) : "";
                if (possIt != row.end()) {
                    std::wstringstream ss(possIt->second);
                    std::wstring item;
                    while (std::getline(ss, item, L'|')) attr.possibleValues.push_back(WideToUtf8(item));
                }
                out.push_back(std::move(attr));
            }
            return Result::Success("Enumerated " + std::to_string(out.size()) + " HP BIOS attributes.");
        } catch (const std::exception& e) {
            return Result::Failure(std::string("HP BIOS enumeration failed: ") + e.what());
        }
    }

    Result SetAttribute(const std::string& name, const std::string& value) override {
        try {
            WmiSession session(L"root\\hp\\instrumentedBIOS");
            auto rows = session.Query(L"SELECT * FROM HP_BIOSSettingInterface");
            if (rows.empty()) {
                return Result::Failure("HP_BIOSSettingInterface instance not found.");
            }
            std::wstring path = rows.front().at(L"__PATH");
            std::wstring wname = Utf8ToWide(name);
            std::wstring wvalue = Utf8ToWide(value);

            // Empty password ("") is standard for machines with no BIOS
            // admin password set; if one is set, this call will fail and
            // that's surfaced below rather than silently no-op'd.
            auto out = session.ExecMethodOnPath(
                path, L"SetBIOSSetting",
                {{L"Name", wname}, {L"Value", wvalue}, {L"Password", L""}});

            std::wstring rv = out.count(L"Return") ? out.at(L"Return") : L"?";
            if (rv == L"0") {
                return Result::Success("HP: set " + name + " = " + value + " (Return=0).");
            }
            return Result::Failure(
                "HP SetBIOSSetting returned code " + WideToUtf8(rv) + " (non-zero = failure; a "
                "BIOS admin password may be required).");
        } catch (const std::exception& e) {
            return Result::Failure(std::string("HP BIOS write failed: ") + e.what());
        }
    }
};

} // namespace

std::unique_ptr<IVendorBiosBackend> CreateHpBackend() {
    return std::make_unique<HpBiosBackend>();
}

} // namespace langbios
