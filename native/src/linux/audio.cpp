// Real system audio volume/mute via pactl (PulseAudio, or PipeWire's
// pulse-compatible shim - the de facto standard on modern Linux
// desktops), targeting the default sink. Same shell-out style as this
// project's other Linux backends.
//
// Verified on Linux against a real PulseAudio server (null sink): get/set
// volume (including clamping), mute/unmute and reading mute state.
#include "langbios/audio.hpp"
#include <array>
#include <cstdio>
#include <memory>
#include <regex>

namespace langbios {

namespace {

std::string RunCommand(const std::string& cmd) {
    std::array<char, 256> buf;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen((cmd + " 2>&1").c_str(), "r"), pclose);
    if (!pipe) return "";
    while (fgets(buf.data(), buf.size(), pipe.get()) != nullptr) result += buf.data();
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) result.pop_back();
    return result;
}

bool LooksLikeError(const std::string& out) {
    return out.find("No such") != std::string::npos || out.find("not found") != std::string::npos ||
           out.find("Connection refused") != std::string::npos;
}

} // namespace

Result GetVolume() {
    std::string out = RunCommand("pactl get-sink-volume @DEFAULT_SINK@");
    if (LooksLikeError(out)) {
        return Result::Failure("Could not read volume via pactl - is PulseAudio/PipeWire-Pulse running? (" + out + ")");
    }
    std::smatch m;
    std::regex re(R"((\d{1,3})%)");
    if (std::regex_search(out, m, re)) {
        return Result::Success("volume = " + m[1].str() + "%", m[1].str());
    }
    return Result::Failure("Could not parse pactl output: " + out);
}

Result SetVolume(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    std::string out = RunCommand("pactl set-sink-volume @DEFAULT_SINK@ " + std::to_string(percent) + "%");
    if (LooksLikeError(out)) {
        return Result::Failure("Setting volume via pactl failed: " + out);
    }
    return Result::Success("Volume set to " + std::to_string(percent) + "%.");
}

Result GetMute() {
    std::string out = RunCommand("pactl get-sink-mute @DEFAULT_SINK@");
    if (LooksLikeError(out)) {
        return Result::Failure("Could not read mute state via pactl - is PulseAudio/PipeWire-Pulse running? (" + out + ")");
    }
    bool muted = out.find("yes") != std::string::npos;
    return Result::Success(std::string("mute = ") + (muted ? "on" : "off"), muted ? "true" : "false");
}

Result SetMute(bool mute) {
    std::string out = RunCommand(std::string("pactl set-sink-mute @DEFAULT_SINK@ ") + (mute ? "1" : "0"));
    if (LooksLikeError(out)) {
        return Result::Failure("Setting mute via pactl failed: " + out);
    }
    return Result::Success(mute ? "Muted." : "Unmuted.");
}

} // namespace langbios
