// Lenovo's real BIOS management interface: WMI namespace root\wmi.
// Lenovo_BiosSetting instances list "Name,Value,..." pairs;
// Lenovo_SetBiosSetting::SetBiosSetting("Name,Value") stages a change and
// Lenovo_SaveBiosSettings::SaveBiosSettings() commits it (applies at next
// boot). Reference: Lenovo BIOS/UEFI WMI interface documentation (ThinkPad/
// ThinkCentre "Client Security/Configuration" WMI classes).
#include "langbios/vendor_bios.hpp"
#include "wmi_session.hpp"
#include "win_strings.hpp"
#include <cstring>
#include <sstream>

namespace langbios {

namespace {

class LenovoBiosBackend : public IVendorBiosBackend {
public:
    std::string Name() const override { return "Lenovo"; }

    bool Supported() override {
        try {
            WmiSession session(L"root\\wmi");
            session.Query(L"SELECT * FROM Lenovo_BiosSetting");
            return true;
        } catch (...) {
            return false;
        }
    }

    Result Enumerate(std::vector<BiosAttribute>& out) override {
        try {
            WmiSession session(L"root\\wmi");
            auto rows = session.Query(L"SELECT * FROM Lenovo_BiosSetting");
            out.clear();
            for (const auto& row : rows) {
                auto it = row.find(L"CurrentSetting");
                if (it == row.end()) continue;
                // Format is "Name,Value[,...]"
                std::wstring entry = it->second;
                size_t comma = entry.find(L',');
                if (comma == std::wstring::npos) continue;
                BiosAttribute attr;
                attr.name = WideToUtf8(entry.substr(0, comma));
                attr.currentValue = WideToUtf8(entry.substr(comma + 1));
                out.push_back(std::move(attr));
            }
            return Result::Success("Enumerated " + std::to_string(out.size()) + " Lenovo BIOS attributes.");
        } catch (const std::exception& e) {
            return Result::Failure(std::string("Lenovo BIOS enumeration failed: ") + e.what());
        }
    }

    Result SetAttribute(const std::string& name, const std::string& value) override {
        try {
            WmiSession session(L"root\\wmi");
            std::wstring wname = Utf8ToWide(name);
            std::wstring wvalue = Utf8ToWide(value);

            auto setRows = session.Query(L"SELECT * FROM Lenovo_SetBiosSetting");
            if (setRows.empty()) return Result::Failure("Lenovo_SetBiosSetting instance not found.");
            std::wstring setPath = setRows.front().at(L"__PATH");

            auto setOut = session.ExecMethodOnPath(
                setPath, L"SetBiosSetting", {{L"parameter", wname + L"," + wvalue}});
            std::wstring setRv = setOut.count(L"return") ? setOut.at(L"return") : L"?";
            if (setRv != L"Success" && setRv != L"0") {
                return Result::Failure("Lenovo SetBiosSetting returned: " + WideToUtf8(setRv));
            }

            auto saveRows = session.Query(L"SELECT * FROM Lenovo_SaveBiosSettings");
            if (saveRows.empty()) return Result::Failure("Lenovo_SaveBiosSettings instance not found.");
            std::wstring savePath = saveRows.front().at(L"__PATH");

            auto saveOut = session.ExecMethodOnPath(savePath, L"SaveBiosSettings", {{L"parameter", L""}});
            std::wstring saveRv = saveOut.count(L"return") ? saveOut.at(L"return") : L"?";
            if (saveRv != L"Success" && saveRv != L"0") {
                return Result::Failure("Lenovo SaveBiosSettings returned: " + WideToUtf8(saveRv));
            }

            return Result::Success(
                "Lenovo: staged and saved " + name + " = " + value +
                " (takes effect at next boot).");
        } catch (const std::exception& e) {
            return Result::Failure(std::string("Lenovo BIOS write failed: ") + e.what());
        }
    }
};

} // namespace

std::unique_ptr<IVendorBiosBackend> CreateLenovoBackend() {
    return std::make_unique<LenovoBiosBackend>();
}

} // namespace langbios
