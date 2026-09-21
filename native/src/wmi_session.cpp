#include "langbios/wmi_session.hpp"
#include <Wbemidl.h>
#include <stdexcept>
#include <atomic>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace langbios {

namespace {

std::atomic<bool> g_comInitByUs{false};
std::atomic<bool> g_securityInitDone{false};

std::string NarrowForThrow(const std::wstring& w) {
    if (w.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), len, nullptr, nullptr);
    return s;
}

std::wstring VariantToWString(const VARIANT& v) {
    if (v.vt == VT_BSTR) {
        return v.bstrVal ? std::wstring(v.bstrVal, SysStringLen(v.bstrVal)) : L"";
    }
    if (v.vt == VT_BOOL) {
        return v.boolVal ? L"true" : L"false";
    }
    if (v.vt == VT_I4 || v.vt == VT_UI4 || v.vt == VT_I2 || v.vt == VT_UI2) {
        return std::to_wstring(v.lVal);
    }
    if ((v.vt & VT_ARRAY) && (v.vt & VT_BSTR)) {
        SAFEARRAY* sa = v.parray;
        if (!sa) return L"";
        long lBound = 0, uBound = -1;
        SafeArrayGetLBound(sa, 1, &lBound);
        SafeArrayGetUBound(sa, 1, &uBound);
        std::wstring joined;
        for (long i = lBound; i <= uBound; ++i) {
            BSTR item = nullptr;
            SafeArrayGetElement(sa, &i, &item);
            if (!joined.empty()) joined += L"|";
            if (item) joined += std::wstring(item, SysStringLen(item));
        }
        return joined;
    }
    if (v.vt == VT_NULL || v.vt == VT_EMPTY) {
        return L"";
    }
    return L"";
}

} // namespace

bool ComWasInitializedByLangbios() { return g_comInitByUs.load(); }

WmiSession::WmiSession(const std::wstring& ns) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (hr == S_OK) {
        comInitializedHere_ = true;
        g_comInitByUs.store(true);
    } else if (hr != S_FALSE && hr != RPC_E_CHANGED_MODE) {
        throw std::runtime_error("CoInitializeEx failed");
    }

    bool expected = false;
    if (g_securityInitDone.compare_exchange_strong(expected, true)) {
        HRESULT secHr = CoInitializeSecurity(
            nullptr, -1, nullptr, nullptr,
            RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
            nullptr, EOAC_NONE, nullptr);
        // RPC_E_TOO_LATE means some other component (or a previous
        // WmiSession in this process) already set security - that's fine.
        if (FAILED(secHr) && secHr != RPC_E_TOO_LATE) {
            if (comInitializedHere_) CoUninitialize();
            throw std::runtime_error("CoInitializeSecurity failed");
        }
    }

    HRESULT hrLoc = CoCreateInstance(
        CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, reinterpret_cast<LPVOID*>(&locator_));
    if (FAILED(hrLoc)) {
        if (comInitializedHere_) CoUninitialize();
        throw std::runtime_error("Failed to create IWbemLocator (WMI unavailable)");
    }

    _bstr_t nsBstr(ns.c_str());
    HRESULT hrConn = locator_->ConnectServer(
        nsBstr, nullptr, nullptr, nullptr, 0, nullptr, nullptr, &services_);
    if (FAILED(hrConn)) {
        locator_->Release();
        locator_ = nullptr;
        if (comInitializedHere_) CoUninitialize();
        throw std::runtime_error(
            "Could not connect to WMI namespace " + NarrowForThrow(ns) +
            " (this vendor's BIOS management stack is likely not present on this machine)");
    }

    HRESULT hrProxy = CoSetProxyBlanket(
        services_, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
    if (FAILED(hrProxy)) {
        services_->Release();
        services_ = nullptr;
        locator_->Release();
        locator_ = nullptr;
        if (comInitializedHere_) CoUninitialize();
        throw std::runtime_error("CoSetProxyBlanket failed");
    }
}

WmiSession::~WmiSession() {
    if (services_) services_->Release();
    if (locator_) locator_->Release();
    if (comInitializedHere_) CoUninitialize();
}

std::vector<WmiRow> WmiSession::Query(const std::wstring& wql) {
    std::vector<WmiRow> rows;
    IEnumWbemClassObject* enumerator = nullptr;
    HRESULT hr = services_->ExecQuery(
        _bstr_t(L"WQL"), _bstr_t(wql.c_str()),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATE,
        nullptr, &enumerator);
    if (FAILED(hr) || !enumerator) {
        throw std::runtime_error("WMI query failed");
    }

    IWbemClassObject* obj = nullptr;
    ULONG returned = 0;
    while (enumerator->Next(WBEM_INFINITE, 1, &obj, &returned) == S_OK) {
        WmiRow row;
        SAFEARRAY* names = nullptr;
        if (SUCCEEDED(obj->GetNames(nullptr, WBEM_FLAG_ALWAYS, nullptr, &names)) && names) {
            long lBound = 0, uBound = -1;
            SafeArrayGetLBound(names, 1, &lBound);
            SafeArrayGetUBound(names, 1, &uBound);
            for (long i = lBound; i <= uBound; ++i) {
                BSTR propName = nullptr;
                SafeArrayGetElement(names, &i, &propName);
                if (!propName) continue;
                VARIANT val;
                VariantInit(&val);
                if (SUCCEEDED(obj->Get(propName, 0, &val, nullptr, nullptr))) {
                    row[std::wstring(propName, SysStringLen(propName))] = VariantToWString(val);
                }
                VariantClear(&val);
            }
            SafeArrayDestroy(names);
        }
        obj->Release();
        rows.push_back(std::move(row));
    }
    enumerator->Release();
    return rows;
}

WmiRow WmiSession::ExecMethod(
    const std::wstring& className,
    const std::wstring& methodName,
    const std::map<std::wstring, std::wstring>& inParams) {
    IWbemClassObject* classObj = nullptr;
    HRESULT hr = services_->GetObject(_bstr_t(className.c_str()), 0, nullptr, &classObj, nullptr);
    if (FAILED(hr) || !classObj) {
        throw std::runtime_error("Could not get WMI class definition for method call");
    }

    IWbemClassObject* inSignature = nullptr;
    classObj->GetMethod(methodName.c_str(), 0, &inSignature, nullptr);

    IWbemClassObject* inParamsInstance = nullptr;
    if (inSignature) {
        inSignature->SpawnInstance(0, &inParamsInstance);
        for (const auto& kv : inParams) {
            VARIANT v;
            VariantInit(&v);
            v.vt = VT_BSTR;
            v.bstrVal = SysAllocString(kv.second.c_str());
            inParamsInstance->Put(kv.first.c_str(), 0, &v, 0);
            VariantClear(&v);
        }
        inSignature->Release();
    }

    IWbemClassObject* outParams = nullptr;
    HRESULT hrExec = services_->ExecMethod(
        _bstr_t(className.c_str()), _bstr_t(methodName.c_str()), 0, nullptr,
        inParamsInstance, &outParams, nullptr);

    if (inParamsInstance) inParamsInstance->Release();
    classObj->Release();

    if (FAILED(hrExec)) {
        if (outParams) outParams->Release();
        throw std::runtime_error("WMI method call failed");
    }

    WmiRow result;
    if (outParams) {
        SAFEARRAY* names = nullptr;
        if (SUCCEEDED(outParams->GetNames(nullptr, WBEM_FLAG_ALWAYS, nullptr, &names)) && names) {
            long lBound = 0, uBound = -1;
            SafeArrayGetLBound(names, 1, &lBound);
            SafeArrayGetUBound(names, 1, &uBound);
            for (long i = lBound; i <= uBound; ++i) {
                BSTR propName = nullptr;
                SafeArrayGetElement(names, &i, &propName);
                if (!propName) continue;
                VARIANT val;
                VariantInit(&val);
                if (SUCCEEDED(outParams->Get(propName, 0, &val, nullptr, nullptr))) {
                    result[std::wstring(propName, SysStringLen(propName))] = VariantToWString(val);
                }
                VariantClear(&val);
            }
            SafeArrayDestroy(names);
        }
        outParams->Release();
    }
    return result;
}

} // namespace langbios
