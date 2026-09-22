#include "langbios/llm_fallback.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#define LB_POPEN _popen
#define LB_PCLOSE _pclose
#else
#include <csignal>
#include <unistd.h>
#define LB_POPEN popen
#define LB_PCLOSE pclose
#endif

namespace langbios {

namespace fs = std::filesystem;

namespace {

constexpr int kEmbeddingDim = 384; // bge-small-en-v1.5
constexpr int kPort = 8931;
// Empirically measured against bge-small-en-v1.5: genuine paraphrases of
// canonical intents ("can you turn secure boot on", "please mute") score
// 0.77-0.97, while greetings/chitchat/noise ("hi", "hey", "thanks") score
// 0.61-0.72 against their nearest (wrong) canonical intent - short unrelated
// text still gets a surprisingly high similarity floor with this model. 0.75
// sits in the gap between those two clusters.
constexpr float kSimilarityThreshold = 0.75f;

struct CanonicalIntent {
    Action action;
    std::string setting;
    std::string value;
    std::array<float, kEmbeddingDim> embedding;
};

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

fs::path ServerPath() {
#ifdef _WIN32
    return ExecutableDir() / "llamacpp" / "bin" / "llama-server.exe";
#else
    return ExecutableDir() / "llamacpp" / "bin" / "llama-server";
#endif
}

fs::path ModelPath() {
    return ExecutableDir() / "models" / "bge-small-en-v1.5-q8_0.gguf";
}

fs::path EmbeddingsDataPath() {
    // Shipped as native/data/canonical_embeddings.bin -> installed
    // alongside the exe at ./data/canonical_embeddings.bin (see
    // installer's [Files] section and build.ps1's copy step).
    return ExecutableDir() / "data" / "canonical_embeddings.bin";
}

std::string RunShell(const std::string& cmd) {
    FILE* pipe = LB_POPEN(cmd.c_str(), "r");
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

// Parses the first top-level JSON array of numbers found after the key
// "embedding" - avoids pulling in a JSON library for one fixed,
// fully-controlled response shape.
bool ExtractEmbeddingVector(const std::string& json, std::array<float, kEmbeddingDim>& out) {
    size_t pos = json.find("\"embedding\":[");
    if (pos == std::string::npos) return false;
    pos += std::string("\"embedding\":[").size();
    size_t end = json.find(']', pos);
    if (end == std::string::npos) return false;

    std::string arr = json.substr(pos, end - pos);
    size_t idx = 0;
    size_t start = 0;
    while (start < arr.size() && idx < kEmbeddingDim) {
        size_t comma = arr.find(',', start);
        std::string tok = (comma == std::string::npos) ? arr.substr(start) : arr.substr(start, comma - start);
        try {
            out[idx++] = std::stof(tok);
        } catch (...) {
            return false;
        }
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return idx == kEmbeddingDim;
}

bool LoadCanonicalIntents(std::vector<CanonicalIntent>& out) {
    std::ifstream f(EmbeddingsDataPath(), std::ios::binary);
    if (!f) return false;

    uint32_t count = 0;
    f.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (!f) return false;

    out.clear();
    out.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t actionCode = 0;
        f.read(reinterpret_cast<char*>(&actionCode), sizeof(actionCode));

        auto readString = [&](std::string& s) {
            uint32_t len = 0;
            f.read(reinterpret_cast<char*>(&len), sizeof(len));
            s.resize(len);
            if (len > 0) f.read(s.data(), len);
        };

        CanonicalIntent intent;
        intent.action = (actionCode == 0) ? Action::Get
                       : (actionCode == 1) ? Action::Set
                       : (actionCode == 2) ? Action::List
                       : (actionCode == 3) ? Action::Reset
                                            : Action::Unknown;
        readString(intent.setting);
        readString(intent.value);
        f.read(reinterpret_cast<char*>(intent.embedding.data()), kEmbeddingDim * sizeof(float));
        if (!f) return false;
        out.push_back(std::move(intent));
    }
    return true;
}

float CosineSimilarity(const std::array<float, kEmbeddingDim>& a, const std::array<float, kEmbeddingDim>& b) {
    double dot = 0, na = 0, nb = 0;
    for (int i = 0; i < kEmbeddingDim; ++i) {
        dot += static_cast<double>(a[i]) * b[i];
        na += static_cast<double>(a[i]) * a[i];
        nb += static_cast<double>(b[i]) * b[i];
    }
    if (na <= 0.0 || nb <= 0.0) return 0.0f;
    return static_cast<float>(dot / (std::sqrt(na) * std::sqrt(nb)));
}

// Handle to the server process, so it can be torn down after use.
#ifdef _WIN32
struct ServerHandle {
    PROCESS_INFORMATION pi{};
    bool started = false;
};
#else
struct ServerHandle {
    pid_t pid = -1;
    bool started = false;
};
#endif

bool StartServer(ServerHandle& handle) {
    std::string exe = ServerPath().string();
    std::string model = ModelPath().string();

#ifdef _WIN32
    std::string cmdLine = "\"" + exe + "\" -m \"" + model + "\" --embedding --port " +
                          std::to_string(kPort) + " --log-disable";

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    std::vector<char> mutableCmd(cmdLine.begin(), cmdLine.end());
    mutableCmd.push_back('\0');

    BOOL ok = CreateProcessA(nullptr, mutableCmd.data(), nullptr, nullptr, FALSE,
                              CREATE_NO_WINDOW, nullptr, nullptr, &si, &handle.pi);
    handle.started = ok != 0;
    return handle.started;
#else
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        execl(exe.c_str(), exe.c_str(), "-m", model.c_str(), "--embedding", "--port",
              std::to_string(kPort).c_str(), "--log-disable", (char*)nullptr);
        _exit(127);
    }
    handle.pid = pid;
    handle.started = true;
    return true;
#endif
}

void StopServer(ServerHandle& handle) {
    if (!handle.started) return;
#ifdef _WIN32
    TerminateProcess(handle.pi.hProcess, 0);
    CloseHandle(handle.pi.hProcess);
    CloseHandle(handle.pi.hThread);
#else
    kill(handle.pid, SIGTERM);
#endif
}

bool WaitForServerReady() {
    std::string healthCmd = "curl -s http://127.0.0.1:" + std::to_string(kPort) + "/health";
    for (int i = 0; i < 100; ++i) { // up to ~10s
        std::string resp = RunShell(healthCmd);
        if (resp.find("\"ok\"") != std::string::npos) return true;
#ifdef _WIN32
        Sleep(100);
#else
        usleep(100000);
#endif
    }
    return false;
}

bool GetEmbedding(const std::string& text, std::array<float, kEmbeddingDim>& out) {
    // Text goes through a temp file, not an inline argument, for the
    // same reason as the old text-generation fallback: shell quoting
    // arbitrary user text is fragile. curl reads the JSON body from
    // @<file> instead.
    fs::path bodyFile = fs::temp_directory_path() / "langbios_embed_body.json";
    {
        std::ofstream f(bodyFile, std::ios::binary | std::ios::trunc);
        f << "{\"input\":[\"";
        for (char c : text) {
            if (c == '"' || c == '\\') f << '\\';
            if (c == '\n') { f << "\\n"; continue; }
            f << c;
        }
        f << "\"]}";
    }

    std::string cmd = "curl -s http://127.0.0.1:" + std::to_string(kPort) +
                       "/v1/embeddings -H \"Content-Type: application/json\" -d @\"" +
                       bodyFile.string() + "\"";
    std::string resp = RunShell(cmd);

    std::error_code ec;
    fs::remove(bodyFile, ec);

    return ExtractEmbeddingVector(resp, out);
}

} // namespace

bool LlmFallbackAvailable() {
    std::error_code ec;
    return fs::exists(ServerPath(), ec) && fs::exists(ModelPath(), ec) &&
           fs::exists(EmbeddingsDataPath(), ec);
}

bool LlmFallbackParse(const std::string& text, Command& out) {
    if (!LlmFallbackAvailable()) return false;

    std::vector<CanonicalIntent> intents;
    if (!LoadCanonicalIntents(intents) || intents.empty()) return false;

    ServerHandle handle;
    if (!StartServer(handle)) return false;

    bool matched = false;
    if (WaitForServerReady()) {
        std::array<float, kEmbeddingDim> queryVec{};
        if (GetEmbedding(text, queryVec)) {
            float bestScore = -1.0f;
            const CanonicalIntent* best = nullptr;
            for (const auto& intent : intents) {
                float score = CosineSimilarity(queryVec, intent.embedding);
                if (score > bestScore) {
                    bestScore = score;
                    best = &intent;
                }
            }
            if (best && bestScore >= kSimilarityThreshold) {
                // List/Reset touch every setting at once - the highest
                // blast radius of any action - and a single bare word
                // ("settings", "status") tends to score deceptively high
                // against canonical examples that happen to share that
                // word ("list settings", "status report") without the
                // user actually asking for everything. Require at least
                // two words before trusting a whole-state match; a real
                // "list settings"/"show everything" request already has
                // that naturally.
                bool isBroadAction = best->action == Action::List || best->action == Action::Reset;
                bool isMultiWord = text.find(' ') != std::string::npos;
                if (!isBroadAction || isMultiWord) {
                    out.action = best->action;
                    out.setting = best->setting;
                    out.value = best->value;
                    out.rawText = text;
                    matched = true;
                }
            }
        }
    }

    StopServer(handle);
    return matched;
}

} // namespace langbios
