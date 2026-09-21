#pragma once
// Thin COM/WMI helper shared by the TPM reader and every vendor BIOS
// backend (Dell/HP/Lenovo all expose their real settings interface as WMI
// classes in a vendor-specific namespace - only the namespace/class/method
// names differ, the plumbing to get there is identical).
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WbemIdl.h>
#include <comdef.h>
#include <map>
#include <string>
#include <vector>

namespace langbios {

// A single WMI instance's properties, name -> value (already stringified;
// arrays are joined with '|').
using WmiRow = std::map<std::wstring, std::wstring>;

class WmiSession {
public:
    // Connects to \\.\root\<ns>. Throws std::runtime_error with a
    // human-readable message on any failure (COM init, DCOM auth, or the
    // namespace simply not existing - which is exactly what happens on
    // machines without that vendor's BIOS management stack installed).
    explicit WmiSession(const std::wstring& ns);
    ~WmiSession();

    WmiSession(const WmiSession&) = delete;
    WmiSession& operator=(const WmiSession&) = delete;

    // Each row also carries its own object path under the "__PATH" key,
    // needed to invoke instance/singleton methods correctly.
    std::vector<WmiRow> Query(const std::wstring& wql);

    // Executes a method against a specific object path (an instance, or a
    // singleton "*_Service" class's one instance obtained via Query's
    // "__PATH"). Returns the out-params (including "ReturnValue").
    WmiRow ExecMethodOnPath(
        const std::wstring& objectPath,
        const std::wstring& methodName,
        const std::map<std::wstring, std::wstring>& inParams);

    // Convenience for classes that are safe to address by class name
    // directly (static methods with no relevant instance state).
    WmiRow ExecMethod(
        const std::wstring& className,
        const std::wstring& methodName,
        const std::map<std::wstring, std::wstring>& inParams);

private:
    bool comInitializedHere_ = false;
    IWbemLocator* locator_ = nullptr;
    IWbemServices* services_ = nullptr;
};

// True the first time any WmiSession is constructed in this process and COM
// hadn't been initialized yet by the caller (informational only).
bool ComWasInitializedByLangbios();

} // namespace langbios
