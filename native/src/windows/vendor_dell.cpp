// Dell's real BIOS management interface: WMI namespace root\dcim\sysman.
// DCIM_BIOSEnumeration lists every toggle-style setting (AttributeName,
// CurrentValue, PossibleValues); DCIM_BIOSService::SetBIOSAttribute is the
// real write path. Reference: Dell Command | Configure / Dell Client
// Command Suite WMI documentation.
#include "langbios/vendor_bios.hpp"
#include "wmi_session.hpp"
#include "win_strings.hpp"
#include <cstring>
#include <sstream>

namespace langbios {

namespace {

class DellBiosBackend : public IVendorBiosBackend {
public:
    std::string Name() const override { return "Dell"; }

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
                attr.name = WideToUtf8(nameIt->second);
                attr.currentValue = valIt != row.end() ? WideToUtf8(valIt->second) : "";
                if (possIt != row.end()) {
                    std::wstringstream ss(possIt->second);
                    std::wstring item;
                    while (std::getline(ss, item, L'|')) attr.possibleValues.push_back(WideToUtf8(item));
                }
                out.push_back(std::move(attr));
            }
            return Result::Success("Enumerated " + std::to_string(out.size()) + " Dell BIOS attributes.");
        } catch (const std::exception& e) {
            return Result::Failure(std::string("Dell BIOS enumeration failed: ") + e.what());
        }
    }

    Result SetAttribute(const std::string& name, const std::string& value) override {
        try {
            WmiSession session(L"root\\dcim\\sysman");
            auto rows = session.Query(L"SELECT * FROM DCIM_BIOSService");
            if (rows.empty()) {
                return Result::Failure("DCIM_BIOSService instance not found.");
            }
            std::wstring path = rows.front().at(L"__PATH");
            std::wstring wname = Utf8ToWide(name);
            std::wstring wvalue = Utf8ToWide(value);

            auto out = session.ExecMethodOnPath(
                path, L"SetBIOSAttribute",
                {{L"AttributeName", wname}, {L"AttributeValue", wvalue}});

            std::wstring rv = out.count(L"ReturnValue") ? out.at(L"ReturnValue") : L"?";
            if (rv == L"0") {
                return Result::Success(
                    "Dell: staged " + name + " = " + value + " (ReturnValue=0). "
                    "Dell requires a pending-changes commit + reboot for BIOS "
                    "attribute changes to actually take effect - this call only "
                    "stages it.");
            }
            return Result::Failure("Dell SetBIOSAttribute returned code " + WideToUtf8(rv) + " (non-zero = failure).");
        } catch (const std::exception& e) {
            return Result::Failure(std::string("Dell BIOS write failed: ") + e.what());
        }
    }
};

} // namespace

std::unique_ptr<IVendorBiosBackend> CreateDellBackend() {
    return std::make_unique<DellBiosBackend>();
}

} // namespace langbios
