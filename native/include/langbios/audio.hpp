#pragma once
// Real system audio volume/mute control - not a BIOS/firmware setting,
// but a genuinely real, well-documented OS-level API on both platforms:
// Windows Core Audio (IAudioEndpointVolume) and Linux PulseAudio/
// PipeWire-Pulse (via pactl). First concrete step in expanding LangBIOS
// beyond firmware into other real hardware-adjacent settings.
#include "langbios/result.hpp"

namespace langbios {

Result GetVolume();          // value is "0".."100" (percent)
Result SetVolume(int percent);
Result GetMute();            // value is "true"/"false"
Result SetMute(bool mute);

} // namespace langbios
