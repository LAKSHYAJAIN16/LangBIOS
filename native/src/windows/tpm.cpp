#include "langbios/tpm.hpp"
#include "wmi_session.hpp"
#include "win_strings.hpp"
#include <cstring>
#include <sstream>

namespace langbios {

Result GetTpmState() {
    try {
        WmiSession session(L"root\\CIMV2\\Security\\MicrosoftTpm");
        auto rows = session.Query(L"SELECT * FROM Win32_Tpm");
        if (rows.empty()) {
            return Result::Failure("No TPM reported by WMI on this machine.");
        }
        const auto& row = rows.front();

        auto get = [&](const wchar_t* key) -> std::wstring {
            auto it = row.find(key);
            return it != row.end() ? it->second : L"(unknown)";
        };

        std::string enabled = WideToUtf8(get(L"IsEnabled_InitialValue"));
        std::string activated = WideToUtf8(get(L"IsActivated_InitialValue"));
        std::string specVersion = WideToUtf8(get(L"SpecVersion"));
        std::string manufacturerVersion = WideToUtf8(get(L"ManufacturerVersion"));

        std::stringstream ss;
        ss << "tpm = present, enabled=" << enabled << ", activated=" << activated
           << ", spec_version=" << specVersion << ", manufacturer_version=" << manufacturerVersion
           << " (read-only: TPM enable/disable is a firmware setup action, not "
              "an OS-writable setting on general hardware)";
        return Result::Success(ss.str(), enabled);
    } catch (const std::exception& e) {
        return Result::Failure(std::string("Could not query TPM via WMI: ") + e.what());
    }
}

} // namespace langbios
