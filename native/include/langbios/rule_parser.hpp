#pragma once
// C++ mirror of langbios/rule_parser.py's alias tables, so the native CLI
// understands the same phrasing as the Python one. Returns false if
// nothing matched confidently.
#include <string>

namespace langbios {

enum class Action { Get, Set, List, Reset, Unknown };

struct Command {
    Action action = Action::Unknown;
    std::wstring setting;
    std::wstring value;
    std::wstring rawText;
};

bool ParseRule(const std::wstring& text, Command& out);

} // namespace langbios
