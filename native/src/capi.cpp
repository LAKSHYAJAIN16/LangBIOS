#include "langbios/capi.h"
#include "langbios/engine.hpp"
#include "langbios/rule_parser.hpp"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

std::wstring Utf8ToWide(const char* s) {
    if (!s || !*s) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    if (len <= 0) return L"";
    std::wstring w(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w.data(), len);
    return w;
}

std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), len, nullptr, nullptr);
    return s;
}

std::string JsonEscape(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 8);
    for (unsigned char c : in) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

} // namespace

int langbios_execute(const char* utf8Text, char* outBuf, int outBufLen) {
    std::wstring text = Utf8ToWide(utf8Text);

    langbios::Command cmd;
    langbios::Result result;
    if (langbios::ParseRule(text, cmd)) {
        result = langbios::Execute(cmd);
    } else {
        result = langbios::Result::Failure(
            L"I didn't understand that. Try things like 'enable secure boot', "
            L"'what's my fan profile', or 'list settings'.");
    }

    std::string message = JsonEscape(WideToUtf8(result.message));
    std::string value = JsonEscape(WideToUtf8(result.value));
    std::string json = "{\"ok\":" + std::string(result.ok ? "true" : "false") +
                        ",\"message\":\"" + message + "\",\"value\":\"" + value + "\"}";

    int needed = static_cast<int>(json.size()) + 1;
    if (!outBuf || outBufLen < needed) {
        return -needed;
    }
    memcpy(outBuf, json.c_str(), needed);
    return static_cast<int>(json.size());
}
