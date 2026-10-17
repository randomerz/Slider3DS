#include "game/save/settings.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iterator>

#include "platform/platform.h"

namespace {

// Every setting: its key in the file (SettingsManager's names) and its member.
struct Field {
    const char* key;
    bool Settings::*flag;
    float Settings::*value;
};
constexpr Field FIELDS[] = {
    {"autoMove", &Settings::autoMove, nullptr},
    {"screenShake", nullptr, &Settings::screenShake},
    {"screenVisualEffects", nullptr, &Settings::screenEffects},
    {"bigTextEnabled", &Settings::bigText, nullptr},
    {"masterVolume", nullptr, &Settings::masterVolume},
    {"sfxVolume", nullptr, &Settings::sfxVolume},
    {"musicVolume", nullptr, &Settings::musicVolume},
    {"ambienceVolume", nullptr, &Settings::ambienceVolume},
};
// Every member is in FIELDS: binding Settings' members stops compiling when one is added (add it to
// FIELDS, then a name here).
constexpr size_t memberCount() {
    Settings s;
    [[maybe_unused]] auto& [a, b, c, d, e, f, g, h] = s;
    return 8;
}
static_assert(memberCount() == std::size(FIELDS), "a Settings member missing from FIELDS");

} // namespace

float Settings::snap(float value) { return std::lround(std::clamp(value, 0.0f, 1.0f) * STEPS) / (float)STEPS; }

bool Settings::operator==(const Settings& o) const {
    for (const Field& f : FIELDS)
        if (f.flag ? this->*f.flag != o.*f.flag : this->*f.value != o.*f.value) return false;
    return true;
}

std::string Settings::serialize() const {
    std::string out;
    char line[64];
    for (const Field& f : FIELDS) {
        if (f.flag) snprintf(line, sizeof line, "%s=%d\n", f.key, this->*f.flag ? 1 : 0);
        else snprintf(line, sizeof line, "%s=%.2f\n", f.key, this->*f.value);
        out += line;
    }
    return out;
}

Settings Settings::parse(std::string_view text) {
    Settings s;
    while (!text.empty()) {
        const size_t end = std::min(text.find('\n'), text.size());
        std::string_view line = text.substr(0, end);
        text.remove_prefix(std::min(end + 1, text.size()));
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        const size_t eq = line.find('=');
        if (eq == std::string_view::npos) continue;
        const std::string_view key = line.substr(0, eq);
        const std::string value(line.substr(eq + 1));
        char* rest = nullptr;
        const float v = std::strtof(value.c_str(), &rest);
        if (value.empty() || *rest != '\0' || !std::isfinite(v)) continue;
        for (const Field& f : FIELDS)
            if (key == f.key) {
                if (f.flag) s.*f.flag = v != 0;
                else s.*f.value = snap(v);
            }
    }
    return s;
}

Settings Settings::load() {
    std::string text;
    return plat::readUserFile(FILE_NAME, text) ? parse(text) : Settings{};
}

bool Settings::save() const { return plat::writeUserFile(FILE_NAME, serialize()); }
