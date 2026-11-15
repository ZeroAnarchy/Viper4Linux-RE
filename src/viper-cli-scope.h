#pragma once
// Accept scoped Linux-style CLI syntax and translate it into the established
// legacy command stream. The existing handlers and validation remain authoritative.
// No IPC or settings access occurs during normalization.
#include <string>
#include <vector>

namespace viperbass::cli_scope {
struct EffectName { const char* name; const char* title; };
inline constexpr EffectName effects[] = {
    {"agc", "Playback Gain Control"}, {"analogx", "AnalogX"},
    {"bass", "ViPER Bass"}, {"clarity", "ViPER Clarity"},
    {"convolver", "Convolver"}, {"crossfeed", "Crossfeed"},
    {"ddc", "ViPER-DDC"}, {"deq", "Dynamic EQ"},
    {"diff", "Differential Surround"}, {"dynamic", "Dynamic System"},
    {"eq", "Graphic Equalizer"}, {"fet", "FET Compressor"},
    {"field", "Field Surround"}, {"headphone", "Headphone Surround+"},
    {"imager", "Stereo Imager"}, {"limiter", "Master Limiter"},
    {"lufs", "LUFS Targeting"}, {"mb", "Multiband Compressor"},
    {"mono", "ViPER Bass Mono"}, {"psycho", "Psychoacoustic Bass"},
    {"reverb", "Reverberation"}, {"speaker", "Speaker Correction"},
    {"spectrum", "Spectrum Extension"}, {"tube", "Tube Simulator"},
};
inline const EffectName* findEffect(const std::string& name) noexcept {
    for (const auto& entry : effects)
        if (name == entry.name) return &entry;
    return nullptr;
}
inline const char* effectNote(const std::string& name) noexcept {
    if (name == "deq") return "Use bands=COUNT, band=INDEX:FIELD:VALUE, or spread.";
    if (name == "mb") return "Use bands=COUNT, crossover=INDEX:HZ, or band=INDEX:FIELD:VALUE.";
    if (name == "ddc") return "Use file=PATH or import=PATH for VDC profiles, or file=none; a supported rate is required.";
    if (name == "convolver") return "Use file=PATH or import=PATH for IRS/WAV responses, or file=none.";
    if (name == "eq") return "Quote preset names with spaces. save=NAME and delete=NAME are standalone operations.";
    if (name == "limiter") return "The limiter has no independent on/off; use master --on or --off.";
    return nullptr;
}
} // namespace viperbass::cli_scope
