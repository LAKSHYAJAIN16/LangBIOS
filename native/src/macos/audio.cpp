// Real system audio volume/mute via AppleScript's standard "volume
// settings" verbs - the same official, documented mechanism System
// Events and every other legitimate macOS automation tool uses. Kept
// consistent with this project's other macOS backends (uefi_vars.cpp):
// shell out to a standard Apple-provided tool rather than link
// CoreAudio/Objective-C++ directly.
//
// NOTE: written against documented `osascript`/AppleScript behavior but
// not run on real Mac hardware in this environment (Windows-only
// development machine) - please verify before trusting the write path.
#include "langbios/audio.hpp"
#include <array>
#include <cstdio>
#include <memory>
#include <string>

namespace langbios {

namespace {

std::string RunOsascript(const std::string& script) {
    std::string cmd = "osascript -e \"" + script + "\" 2>&1";
    std::array<char, 256> buf;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) return "";
    while (fgets(buf.data(), buf.size(), pipe.get()) != nullptr) result += buf.data();
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) result.pop_back();
    return result;
}

} // namespace

Result GetVolume() {
    std::string out = RunOsascript("output volume of (get volume settings)");
    if (out.empty()) {
        return Result::Failure("Could not read system volume via osascript.");
    }
    try {
        int percent = std::stoi(out);
        return Result::Success("volume = " + std::to_string(percent) + "%", std::to_string(percent));
    } catch (...) {
        return Result::Failure("Unexpected output reading volume: " + out);
    }
}

Result SetVolume(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    std::string out = RunOsascript("set volume output volume " + std::to_string(percent));
    if (!out.empty()) {
        return Result::Failure("Setting volume failed: " + out);
    }
    return Result::Success("Volume set to " + std::to_string(percent) + "%.");
}

Result GetMute() {
    std::string out = RunOsascript("output muted of (get volume settings)");
    if (out != "true" && out != "false") {
        return Result::Failure("Unexpected output reading mute state: " + out);
    }
    return Result::Success(std::string("mute = ") + (out == "true" ? "on" : "off"), out);
}

Result SetMute(bool mute) {
    std::string out = RunOsascript(std::string("set volume output muted ") + (mute ? "true" : "false"));
    if (!out.empty()) {
        return Result::Failure("Setting mute state failed: " + out);
    }
    return Result::Success(mute ? "Muted." : "Unmuted.");
}

} // namespace langbios
