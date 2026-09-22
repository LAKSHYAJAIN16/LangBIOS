#include "langbios/llm_fallback.hpp"
#include <array>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#define LB_POPEN _popen
#define LB_PCLOSE _pclose
#else
#include <unistd.h>
#define LB_POPEN popen
#define LB_PCLOSE pclose
#endif

namespace langbios {

namespace fs = std::filesystem;

namespace {

fs::path ExecutableDir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return fs::path(std::wstring(buf, len)).parent_path();
#else
    char buf[4096];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len <= 0) return fs::current_path();
    buf[len] = '\0';
    return fs::path(buf).parent_path();
#endif
}

fs::path LlamaCliPath() {
#ifdef _WIN32
    return ExecutableDir() / "llamacpp" / "bin" / "llama-cli.exe";
#else
    return ExecutableDir() / "llamacpp" / "bin" / "llama-cli";
#endif
}

fs::path ModelPath() {
    return ExecutableDir() / "models" / "qwen2.5-0.5b-instruct-q4_k_m.gguf";
}

const char* kSystemPrompt =
    "You translate a user's natural-language request about their computer's "
    "BIOS/UEFI settings into a single strict JSON object, and nothing else.\n"
    "\n"
    "Available settings:\n"
    "- secure_boot (bool): UEFI Secure Boot\n"
    "- virtualization (bool): CPU virtualization (VT-x/AMD-V)\n"
    "- tpm (bool): Trusted Platform Module\n"
    "- fast_boot (bool): Fast Boot\n"
    "- xmp (bool): Memory XMP/DOCP overclock profile\n"
    "- cpu_turbo (bool): CPU turbo/boost clocks\n"
    "- power_profile (enum: power_saver, balanced, performance)\n"
    "- fan_profile (enum: silent, standard, performance, full_speed)\n"
    "- boot_order (a comma-separated list of real boot entry descriptions)\n"
    "\n"
    "Respond with ONLY a JSON object of this shape:\n"
    "{\"action\": \"get\"|\"set\"|\"list\"|\"reset\"|\"unknown\", \"setting\": \"<name or null>\", \"value\": <value or null>}\n"
    "\n"
    "Rules:\n"
    "- action \"list\" means the user wants to see all current settings.\n"
    "- action \"reset\" means the user wants factory defaults restored.\n"
    "- action \"get\" means the user is asking the current value of one setting.\n"
    "- action \"set\" means the user wants to change one setting; include \"value\".\n"
    "- For bool settings use JSON true/false.\n"
    "- If you cannot confidently map the request, respond with action \"unknown\".\n"
    "- Output JSON only. No explanation, no markdown fences.\n";

// Extracts the first balanced {...} object anywhere in `text` - the
// CLI's stdout includes an ASCII banner, ANSI color codes, and an
// echoed prompt before the actual generated JSON, and this is far more
// robust than trying to line-delimit around that noise.
bool ExtractJsonObject(const std::string& text, std::string& out) {
    size_t start = text.find('{');
    while (start != std::string::npos) {
        int depth = 0;
        for (size_t i = start; i < text.size(); ++i) {
            if (text[i] == '{') {
                depth++;
            } else if (text[i] == '}') {
                depth--;
                if (depth == 0) {
                    out = text.substr(start, i - start + 1);
                    return true;
                }
            }
        }
        start = text.find('{', start + 1);
    }
    return false;
}

// Minimal field extraction for our specific known {action,setting,value}
// schema - not a general JSON parser, but sufficient for one small,
// fully-controlled response shape without pulling in a JSON library.
std::string JsonStringField(const std::string& json, const std::string& key) {
    std::string pattern = "\"" + key + "\"";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + pattern.size());
    if (pos == std::string::npos) return "";
    pos++;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) pos++;
    if (pos >= json.size()) return "";
    if (json[pos] == 'n') return ""; // null
    if (json[pos] == '"') {
        size_t i = pos + 1;
        std::string value;
        while (i < json.size() && json[i] != '"') {
            if (json[i] == '\\' && i + 1 < json.size()) {
                value += json[i + 1];
                i += 2;
                continue;
            }
            value += json[i];
            i++;
        }
        return value;
    }
    size_t end = pos;
    while (end < json.size() && json[end] != ',' && json[end] != '}') end++;
    std::string value = json.substr(pos, end - pos);
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
    return value;
}

std::string ReadAllStdout(const std::string& command) {
    FILE* pipe = LB_POPEN(command.c_str(), "r");
    if (!pipe) return "";
    std::string output;
    std::array<char, 512> buf;
    size_t n;
    while ((n = fread(buf.data(), 1, buf.size(), pipe)) > 0) {
        output.append(buf.data(), n);
    }
    LB_PCLOSE(pipe);
    return output;
}

} // namespace

bool LlmFallbackAvailable() {
    std::error_code ec;
    return fs::exists(LlamaCliPath(), ec) && fs::exists(ModelPath(), ec);
}

bool LlmFallbackParse(const std::string& text, Command& out) {
    if (!LlmFallbackAvailable()) return false;

    // Prompts go through temp files, not inline command-line arguments:
    // cmd.exe's quoting rules for _popen() are notoriously fragile for
    // arbitrary text (embedded quotes, &, %, |), and this sidesteps that
    // entirely - only the (self-generated, quote-free) file paths need
    // to go on the command line at all.
    fs::path tmpDir = fs::temp_directory_path();
    fs::path sysFile = tmpDir / "langbios_llm_system.txt";
    fs::path promptFile = tmpDir / "langbios_llm_prompt.txt";

    {
        std::ofstream sys(sysFile, std::ios::binary | std::ios::trunc);
        sys << kSystemPrompt;
    }
    {
        std::ofstream prompt(promptFile, std::ios::binary | std::ios::trunc);
        prompt << text;
    }

    std::ostringstream cmd;
    cmd << "\"" << LlamaCliPath().string() << "\""
        << " -m \"" << ModelPath().string() << "\""
        << " -sysf \"" << sysFile.string() << "\""
        << " -f \"" << promptFile.string() << "\""
        << " -n 80 --temp 0 --single-turn --simple-io --log-disable"
#ifdef _WIN32
        << " 2>NUL";
#else
        << " 2>/dev/null";
#endif

    std::string output = ReadAllStdout(cmd.str());

    std::error_code ec;
    fs::remove(sysFile, ec);
    fs::remove(promptFile, ec);

    std::string json;
    if (!ExtractJsonObject(output, json)) return false;

    std::string action = JsonStringField(json, "action");
    if (action != "get" && action != "set" && action != "list" && action != "reset") {
        action = "unknown";
    }

    out.action = (action == "get")   ? Action::Get
               : (action == "set")   ? Action::Set
               : (action == "list")  ? Action::List
               : (action == "reset") ? Action::Reset
                                      : Action::Unknown;
    out.setting = JsonStringField(json, "setting");
    out.value = JsonStringField(json, "value");
    out.rawText = text;
    return true;
}

} // namespace langbios
