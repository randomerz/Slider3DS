#include "game/save/profile.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

#include "platform/platform.h"

namespace {

constexpr std::string_view HEADER = "slider3ds-profile 1";

// The line kinds and how many fields follow each.
struct Kind {
    std::string_view name;
    size_t fields;
};
constexpr Kind KINDS[] = {{"name", 1},   {"playTime", 1}, {"flag", 1},        {"string", 2},
                          {"collectible", 2}, {"grid", 1}, {"activeTiles", 1}, {"player", 4}};

void escapeInto(std::string& out, std::string_view field) {
    for (char c : field) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '\t': out += "\\t"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        default: out += c;
        }
    }
}

// Adds one line: `kind`, then each field (escaped), tab-separated.
void line(std::string& out, const char* kind, std::initializer_list<std::string_view> fields) {
    out += kind;
    for (std::string_view f : fields) {
        out += '\t';
        escapeInto(out, f);
    }
    out += '\n';
}

std::string number(float v) {
    char buf[32];
    snprintf(buf, sizeof buf, "%.9g", v); // (reads back exactly)
    return buf;
}

// A line's tab-separated fields, unescaped; false on a bad escape.
bool fields(std::string_view text, std::vector<std::string>& out) {
    out.assign(1, std::string());
    for (size_t i = 0; i < text.size(); i++) {
        const char c = text[i];
        if (c == '\t') {
            out.emplace_back();
        } else if (c == '\\') {
            if (++i == text.size()) return false;
            switch (text[i]) {
            case '\\': out.back() += '\\'; break;
            case 't': out.back() += '\t'; break;
            case 'n': out.back() += '\n'; break;
            case 'r': out.back() += '\r'; break;
            default: return false;
            }
        } else {
            out.back() += c;
        }
    }
    return true;
}

bool toFloat(const std::string& s, float& v) {
    char* rest = nullptr;
    v = std::strtof(s.c_str(), &rest);
    return !s.empty() && *rest == '\0' && std::isfinite(v);
}

bool toInt(const std::string& s, int& v) {
    char* rest = nullptr;
    const long n = std::strtol(s.c_str(), &rest, 10);
    v = (int)n;
    return !s.empty() && *rest == '\0' && n == v;
}

bool toBool(const std::string& s, bool& v) {
    v = s == "1";
    return s == "0" || s == "1";
}

} // namespace

std::string SaveProfile::fileName(int slot) { return "profile" + std::to_string(slot) + ".txt"; }

SaveProfile SaveProfile::make(const std::string& profileName) {
    SaveProfile p;
    p.name = profileName;
    p.state.newProfile(profileName);
    return p;
}

std::string SaveProfile::serialize() const {
    std::string out(HEADER);
    out += '\n';
    line(out, "name", {name});
    line(out, "playTime", {number(playTime)});
    for (const std::string& f : state.flags()) line(out, "flag", {f});
    for (const auto& [key, value] : state.strings()) line(out, "string", {key, value});
    for (const GameState::Collectible& c : state.collectibles()) line(out, "collectible", {c.name, c.area});
    if (!grid.empty()) {
        line(out, "grid", {grid});
        std::string ids;
        for (int id : activeTiles) ids += (ids.empty() ? "" : " ") + std::to_string(id);
        line(out, "activeTiles", {ids});
    }
    if (hasPlayer)
        line(out, "player", {number(playerX), number(playerY), onWater ? "1" : "0", inHouse ? "1" : "0"});
    return out;
}

bool SaveProfile::parse(std::string_view text, SaveProfile& out) {
    out = SaveProfile();
    bool header = true;
    std::vector<std::string> f;
    while (!text.empty()) {
        const size_t end = std::min(text.find('\n'), text.size());
        std::string_view ln = text.substr(0, end);
        text.remove_prefix(std::min(end + 1, text.size()));
        if (!ln.empty() && ln.back() == '\r') ln.remove_suffix(1);
        if (header) {
            if (ln != HEADER) return false;
            header = false;
            continue;
        }
        if (ln.empty()) continue;
        if (!fields(ln, f)) return false;
        const std::string& kind = f[0];
        const Kind* known = std::find_if(std::begin(KINDS), std::end(KINDS), [&](const Kind& k) { return k.name == kind; });
        if (known == std::end(KINDS)) continue; // (a later version's)
        if (f.size() - 1 != known->fields) return false;
        if (kind == "name") {
            out.name = f[1];
        } else if (kind == "playTime") {
            if (!toFloat(f[1], out.playTime) || out.playTime < 0) return false;
        } else if (kind == "flag") {
            out.state.setFlag(f[1], true);
        } else if (kind == "string") {
            out.state.setStr(f[1], f[2]);
        } else if (kind == "collectible") {
            out.state.setCollectible(f[1], f[2], true);
        } else if (kind == "grid") {
            if (f[1].empty()) return false;
            out.grid = f[1];
        } else if (kind == "activeTiles") {
            out.activeTiles.clear();
            std::string_view ids = f[1];
            while (!ids.empty()) {
                const size_t sp = std::min(ids.find(' '), ids.size());
                int id;
                if (!toInt(std::string(ids.substr(0, sp)), id)) return false;
                out.activeTiles.push_back(id);
                ids.remove_prefix(std::min(sp + 1, ids.size()));
            }
        } else { // player
            if (!toFloat(f[1], out.playerX) || !toFloat(f[2], out.playerY) || !toBool(f[3], out.onWater) ||
                !toBool(f[4], out.inHouse))
                return false;
            out.hasPlayer = true;
        }
    }
    return !header;
}

SaveProfile::Read SaveProfile::load(int slot, SaveProfile& out) {
    std::string text;
    if (!plat::readUserFile(fileName(slot).c_str(), text)) return Read::None;
    return parse(text, out) ? Read::Ok : Read::Bad;
}

bool SaveProfile::save(int slot) const { return plat::writeUserFile(fileName(slot).c_str(), serialize()); }
