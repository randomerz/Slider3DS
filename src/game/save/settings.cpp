#include "game/save/settings.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "platform/platform.h"

std::string Settings::serialize() const {
    char buf[64];
    snprintf(buf, sizeof buf, "autoMove=%d\nscreenShake=%.2f\n", autoMove ? 1 : 0, screenShake);
    return buf;
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
        if (key == "autoMove") s.autoMove = v != 0;
        else if (key == "screenShake") s.setScreenShake(v);
    }
    return s;
}

void Settings::setScreenShake(float value) {
    screenShake = std::lround(std::clamp(value, 0.0f, 1.0f) * SHAKE_STEPS) / (float)SHAKE_STEPS;
}

int Settings::screenShakeStep() const { return (int)std::lround(screenShake * SHAKE_STEPS); }

Settings Settings::load() {
    std::string text;
    return plat::readUserFile(FILE_NAME, text) ? parse(text) : Settings{};
}

bool Settings::save() const { return plat::writeUserFile(FILE_NAME, serialize()); }
