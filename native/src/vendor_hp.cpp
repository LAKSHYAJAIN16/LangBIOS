// HP's real BIOS management interface: WMI namespace root\hp\instrumentedBIOS.
// HP_BIOSEnumeration lists settings; HP_BIOSSettingInterface::SetBIOSSetting
// is the real write path. Reference: HP Client Management Script Library /
// HP BIOS Configuration Utility WMI documentation.
#include "langbios/vendor_bios.hpp"
#include "langbios/wmi_session.hpp"
#include <cstring>
#include <sstream>

namespace langbios {

namespace {

class HpBiosBackend : public IVendorBiosBackend {
public:
    std::wstring Name() const override { return L"HP"; }

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
                attr.name = nameIt->second;
                attr.currentValue = valIt != row.end() ? valIt->second : L"";
                if (possIt != row.end()) {
                    std::wstringstream ss(possIt->second);
                    std::wstring item;
                    while (std::getline(ss, item, L'|')) attr.possibleValues.push_back(item);
                }
                out.push_back(std::move(attr));
            }
            return Result::Success(L"Enumerated " + std::to_wstring(out.size()) + L" HP BIOS attributes.");
        } catch (const std::exception& e) {
            return Result::Failure(std::wstring(L"HP BIOS enumeration failed: ") +
                                    std::wstring(e.what(), e.what() + strlen(e.what())));
        }
    }

    Result SetAttribute(const std::wstring& name, const std::wstring& value) override {
        try {
            WmiSession session(L"root\\hp\\instrumentedBIOS");
            auto rows = session.Query(L"SELECT * FROM HP_BIOSSettingInterface");
            if (rows.empty()) {
                return Result::Failure(L"HP_BIOSSettingInterface instance not found.");
            }
            std::wstring path = rows.front().at(L"__PATH");

            // Empty password ("") is standard for machines with no BIOS
            // admin password set; if one is set, this call will fail and
            // that's surfaced below rather than silently no-op'd.
            auto out = session.ExecMethodOnPath(
                path, L"SetBIOSSetting",
                {{L"Name", name}, {L"Value", value}, {L"Password", L""}});

            std::wstring rv = out.count(L"Return") ? out.at(L"Return") : L"?";
            if (rv == L"0") {
                return Result::Success(L"HP: set " + name + L" = " + value + L" (Return=0).");
            }
            return Result::Failure(
                L"HP SetBIOSSetting returned code " + rv + L" (non-zero = failure; a "
                L"BIOS admin password may be required).");
        } catch (const std::exception& e) {
            return Result::Failure(std::wstring(L"HP BIOS write failed: ") +
                                    std::wstring(e.what(), e.what() + strlen(e.what())));
        }
    }
};

} // namespace

std::unique_ptr<IVendorBiosBackend> CreateHpBackend() {
    return std::make_unique<HpBiosBackend>();
}

} // namespace langbios
