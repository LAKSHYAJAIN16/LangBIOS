#pragma once
// Mirror of langbios/rule_parser.py's alias tables, so every CLI
// (Python, native Windows, native Linux) understands the same phrasing.
// Returns false if nothing matched confidently. Plain UTF-8 std::string -
// the whole vocabulary is ASCII English, so no wide-char handling is
// needed here regardless of platform.
#include <string>

namespace langbios {

enum class Action { Get, Set, List, Reset, Unknown };

struct Command {
    Action action = Action::Unknown;
    std::string setting;
    std::string value;
    std::string rawText;
};

bool ParseRule(const std::string& text, Command& out);

} // namespace langbios
