#include "langbios/tpm.hpp"
#include "langbios/wmi_session.hpp"
#include <cstring>
#include <sstream>

namespace langbios {

Result GetTpmState() {
    try {
        WmiSession session(L"root\\CIMV2\\Security\\MicrosoftTpm");
        auto rows = session.Query(L"SELECT * FROM Win32_Tpm");
        if (rows.empty()) {
            return Result::Failure(L"No TPM reported by WMI on this machine.");
        }
        const auto& row = rows.front();

        auto get = [&](const wchar_t* key) -> std::wstring {
            auto it = row.find(key);
            return it != row.end() ? it->second : L"(unknown)";
        };

        bool present = true;
        std::wstring enabled = get(L"IsEnabled_InitialValue");
        std::wstring activated = get(L"IsActivated_InitialValue");
        std::wstring specVersion = get(L"SpecVersion");
        std::wstring manufacturerVersion = get(L"ManufacturerVersion");

        std::wstringstream ss;
        ss << L"tpm = present, enabled=" << enabled << L", activated=" << activated
           << L", spec_version=" << specVersion << L", manufacturer_version=" << manufacturerVersion
           << L" (read-only: TPM enable/disable is a firmware setup action, not "
              L"an OS-writable setting on general hardware)";
        (void)present;
        return Result::Success(ss.str(), enabled);
    } catch (const std::exception& e) {
        return Result::Failure(std::wstring(L"Could not query TPM via WMI: ") +
                                std::wstring(e.what(), e.what() + strlen(e.what())));
    }
}

} // namespace langbios
