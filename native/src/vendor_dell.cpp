// Dell's real BIOS management interface: WMI namespace root\dcim\sysman.
// DCIM_BIOSEnumeration lists every toggle-style setting (AttributeName,
// CurrentValue, PossibleValues); DCIM_BIOSService::SetBIOSAttribute is the
// real write path. Reference: Dell Command | Configure / Dell Client
// Command Suite WMI documentation.
#include "langbios/vendor_bios.hpp"
#include "langbios/wmi_session.hpp"
#include <cstring>
#include <sstream>

namespace langbios {

namespace {

class DellBiosBackend : public IVendorBiosBackend {
public:
    std::wstring Name() const override { return L"Dell"; }

    bool Supported() override {
        try {
            WmiSession session(L"root\\dcim\\sysman");
            session.Query(L"SELECT * FROM DCIM_BIOSEnumeration");
            return true;
        } catch (...) {
            return false;
        }
    }

    Result Enumerate(std::vector<BiosAttribute>& out) override {
        try {
            WmiSession session(L"root\\dcim\\sysman");
            auto rows = session.Query(L"SELECT * FROM DCIM_BIOSEnumeration");
            out.clear();
            for (const auto& row : rows) {
                BiosAttribute attr;
                auto nameIt = row.find(L"AttributeName");
                auto valIt = row.find(L"CurrentValue");
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
            return Result::Success(L"Enumerated " + std::to_wstring(out.size()) + L" Dell BIOS attributes.");
        } catch (const std::exception& e) {
            return Result::Failure(std::wstring(L"Dell BIOS enumeration failed: ") +
                                    std::wstring(e.what(), e.what() + strlen(e.what())));
        }
    }

    Result SetAttribute(const std::wstring& name, const std::wstring& value) override {
        try {
            WmiSession session(L"root\\dcim\\sysman");
            auto rows = session.Query(L"SELECT * FROM DCIM_BIOSService");
            if (rows.empty()) {
                return Result::Failure(L"DCIM_BIOSService instance not found.");
            }
            std::wstring path = rows.front().at(L"__PATH");

            auto out = session.ExecMethodOnPath(
                path, L"SetBIOSAttribute",
                {{L"AttributeName", name}, {L"AttributeValue", value}});

            std::wstring rv = out.count(L"ReturnValue") ? out.at(L"ReturnValue") : L"?";
            if (rv == L"0") {
                return Result::Success(
                    L"Dell: staged " + name + L" = " + value + L" (ReturnValue=0). "
                    L"Dell requires a pending-changes commit + reboot for BIOS "
                    L"attribute changes to actually take effect - this call only "
                    L"stages it.");
            }
            return Result::Failure(L"Dell SetBIOSAttribute returned code " + rv + L" (non-zero = failure).");
        } catch (const std::exception& e) {
            return Result::Failure(std::wstring(L"Dell BIOS write failed: ") +
                                    std::wstring(e.what(), e.what() + strlen(e.what())));
        }
    }
};

} // namespace

std::unique_ptr<IVendorBiosBackend> CreateDellBackend() {
    return std::make_unique<DellBiosBackend>();
}

} // namespace langbios
