#include "game/save/profile.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <iterator>

#include "game/village/village_grid.h"
#include "platform/platform.h"

namespace {

constexpr std::string_view HEADER = "slider3ds-profile 1";

using Fields = std::vector<std::string>; // a line's fields; [0] is its kind

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

std::string number(double v) {
    char buf[40];
    snprintf(buf, sizeof buf, "%.17g", v); // (reads back exactly, as a float or a double)
    return buf;
}

// A line's tab-separated fields, unescaped; false on a bad escape.
bool split(std::string_view text, Fields& out) {
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

template <class T>
bool toReal(const std::string& s, T& v) {
    char* rest = nullptr;
    v = (T)std::strtod(s.c_str(), &rest);
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

bool readIds(const std::string& text, std::vector<int>& out) {
    out.clear();
    std::string_view ids = text;
    while (!ids.empty()) {
        const size_t sp = std::min(ids.find(' '), ids.size());
        int id;
        if (!toInt(std::string(ids.substr(0, sp)), id)) return false;
        out.push_back(id);
        ids.remove_prefix(std::min(sp + 1, ids.size()));
    }
    return true;
}

// Each line kind: its name, how many fields follow it, and how it's read (false: malformed).
struct Kind {
    std::string_view name;
    size_t fields;
    bool (*read)(const Fields& f, SaveProfile& p);
};
constexpr Kind KINDS[] = {
    {"name", 1, [](const Fields& f, SaveProfile& p) { p.name = f[1]; return true; }},
    {"playTime", 1, [](const Fields& f, SaveProfile& p) { return toReal(f[1], p.playTime) && p.playTime >= 0; }},
    {"lastSaved", 1,
     [](const Fields& f, SaveProfile& p) {
         char* rest = nullptr;
         p.lastSaved = std::strtoll(f[1].c_str(), &rest, 10);
         return !f[1].empty() && *rest == '\0';
     }},
    {"flag", 1, [](const Fields& f, SaveProfile& p) { p.state.setFlag(f[1], true); return true; }},
    {"string", 2, [](const Fields& f, SaveProfile& p) { p.state.setStr(f[1], f[2]); return true; }},
    {"collectible", 2, [](const Fields& f, SaveProfile& p) { p.state.setCollectible(f[1], f[2], true); return true; }},
    {"grid", 1, [](const Fields& f, SaveProfile& p) { p.grid = f[1]; return !p.grid.empty(); }},
    {"activeTiles", 1, [](const Fields& f, SaveProfile& p) { return readIds(f[1], p.activeTiles); }},
    {"player", 4,
     [](const Fields& f, SaveProfile& p) {
         p.hasPlayer = true;
         return toReal(f[1], p.playerX) && toReal(f[2], p.playerY) && toBool(f[3], p.onWater) && toBool(f[4], p.inHouse);
     }},
};

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
    line(out, "lastSaved", {std::to_string(lastSaved)});
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
    Fields f;
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
        if (!split(ln, f)) return false;
        const Kind* kind = std::find_if(std::begin(KINDS), std::end(KINDS), [&](const Kind& k) { return k.name == f[0]; });
        if (kind == std::end(KINDS)) continue; // (a later version's)
        if (f.size() - 1 != kind->fields || !kind->read(f, out)) return false;
    }
    return !header;
}

SaveProfile::Read SaveProfile::load(int slot, SaveProfile& out) {
    std::string text;
    if (!plat::readUserFile(fileName(slot).c_str(), text)) return Read::None;
    return parse(text, out) ? Read::Ok : Read::Bad;
}

int SaveProfile::areasCompleted() const { return state.hasCollectible(village::FINAL_SLIDER, "Village") ? 1 : 0; }

bool SaveProfile::remove(int slot) { return plat::removeUserFile(fileName(slot).c_str()); }

bool SaveProfile::save(int slot) const { return plat::writeUserFile(fileName(slot).c_str(), serialize()); }
