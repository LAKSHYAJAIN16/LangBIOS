// Real system audio volume/mute via Windows Core Audio
// (IAudioEndpointVolume on the default render/playback device) - a
// standard, documented Win32 COM API since Vista, no vendor lock-in.
#include "langbios/audio.hpp"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <string>

#pragma comment(lib, "ole32.lib")

namespace langbios {

namespace {

struct ComGuard {
    bool didInit = false;
    ComGuard() { didInit = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)); }
    ~ComGuard() { if (didInit) CoUninitialize(); }
};

// Caller must Release() a non-null result.
IAudioEndpointVolume* GetDefaultEndpointVolume() {
    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                   __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr) || !enumerator) return nullptr;

    IMMDevice* device = nullptr;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    enumerator->Release();
    if (FAILED(hr) || !device) return nullptr;

    IAudioEndpointVolume* volume = nullptr;
    hr = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
                           reinterpret_cast<void**>(&volume));
    device->Release();
    if (FAILED(hr)) return nullptr;
    return volume;
}

const char* kNoEndpointMsg =
    "Could not access the default audio playback device (none present, or it's disabled).";

} // namespace

Result GetVolume() {
    ComGuard com;
    IAudioEndpointVolume* vol = GetDefaultEndpointVolume();
    if (!vol) return Result::Failure(kNoEndpointMsg);

    float level = 0.0f;
    HRESULT hr = vol->GetMasterVolumeLevelScalar(&level);
    vol->Release();
    if (FAILED(hr)) return Result::Failure("Reading system volume failed.");

    int percent = static_cast<int>(level * 100.0f + 0.5f);
    return Result::Success("volume = " + std::to_string(percent) + "%", std::to_string(percent));
}

Result SetVolume(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    ComGuard com;
    IAudioEndpointVolume* vol = GetDefaultEndpointVolume();
    if (!vol) return Result::Failure(kNoEndpointMsg);

    HRESULT hr = vol->SetMasterVolumeLevelScalar(static_cast<float>(percent) / 100.0f, nullptr);
    vol->Release();
    if (FAILED(hr)) return Result::Failure("Setting system volume failed.");

    return Result::Success("Volume set to " + std::to_string(percent) + "%.");
}

Result GetMute() {
    ComGuard com;
    IAudioEndpointVolume* vol = GetDefaultEndpointVolume();
    if (!vol) return Result::Failure(kNoEndpointMsg);

    BOOL muted = FALSE;
    HRESULT hr = vol->GetMute(&muted);
    vol->Release();
    if (FAILED(hr)) return Result::Failure("Reading mute state failed.");

    std::string state = muted ? "true" : "false";
    return Result::Success(std::string("mute = ") + (muted ? "on" : "off"), state);
}

Result SetMute(bool mute) {
    ComGuard com;
    IAudioEndpointVolume* vol = GetDefaultEndpointVolume();
    if (!vol) return Result::Failure(kNoEndpointMsg);

    HRESULT hr = vol->SetMute(mute ? TRUE : FALSE, nullptr);
    vol->Release();
    if (FAILED(hr)) return Result::Failure("Setting mute state failed.");

    return Result::Success(mute ? "Muted." : "Unmuted.");
}

} // namespace langbios
