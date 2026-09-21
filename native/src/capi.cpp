// Plain C ABI, shared by Windows and Linux builds (see capi.h for the
// _WIN32 export-macro guard). No platform headers needed here at all now
// that Result/Command are UTF-8 std::string throughout.
#include "langbios/capi.h"
#include "langbios/engine.hpp"
#include "langbios/rule_parser.hpp"
#include <cstdio>
#include <cstring>
#include <string>

namespace {

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
    std::string text = utf8Text ? utf8Text : "";

    langbios::Command cmd;
    langbios::Result result;
    if (langbios::ParseRule(text, cmd)) {
        result = langbios::Execute(cmd);
    } else {
        result = langbios::Result::Failure(
            "I didn't understand that. Try things like 'enable secure boot', "
            "'what's my fan profile', or 'list settings'.");
    }

    std::string message = JsonEscape(result.message);
    std::string value = JsonEscape(result.value);
    std::string json = "{\"ok\":" + std::string(result.ok ? "true" : "false") +
                        ",\"message\":\"" + message + "\",\"value\":\"" + value + "\"}";

    int needed = static_cast<int>(json.size()) + 1;
    if (!outBuf || outBufLen < needed) {
        return -needed;
    }
    memcpy(outBuf, json.c_str(), needed);
    return static_cast<int>(json.size());
}
