#include "game/hints/hints.h"

#include <algorithm>
#include <cstring>

#include "game/events/events.h"
#include "game/world/world.h"

namespace {

// Where the 3DS says something else than the original's text.
struct TextOverride {
    const char* name;
    const char* text; // "" = not shown
};
constexpr TextOverride OVERRIDES[] = {
    {"tab", ""}, // "Press <Tab> to open the Artifact": it's always open on the bottom screen (user's call: dropped)
    {"tiles", "Tap sliders on the bottom screen to move them"}, // "Use the mouse to move sliders"
};

// Controls.BindingDisplayString on the 3DS; null = no such button.
const char* button(const std::string& control) {
    if (control == "Move_Up") return "Up";
    if (control == "Move_Down") return "Down";
    if (control == "Move_Left") return "Left";
    if (control == "Move_Right") return "Right";
    if (control == "Action") return "A";
    if (control == "Pause") return "START";
    return nullptr;
}

bool contains(const std::vector<int>& v, int x) { return std::find(v.begin(), v.end(), x) != v.end(); }

} // namespace

std::string Hints::text(const HintDef& hint) {
    for (const TextOverride& o : OVERRIDES)
        if (hint.name == o.name) return o.text;
    // ReplacePlaceholdersWithControlBindings: each <...> takes the next bind's button.
    std::string out;
    size_t at = 0, bind = 0;
    while (true) {
        const size_t open = hint.text.find('<', at);
        if (open == std::string::npos) break;
        const size_t close = hint.text.find('>', open);
        const char* b = bind < hint.binds.size() ? button(hint.binds[bind]) : nullptr;
        if (close == std::string::npos || !b) return ""; // (a button the 3DS lacks: give it an override)
        out.append(hint.text, at, open - at).append(b);
        at = close + 1;
        bind++;
    }
    out.append(hint.text, at, std::string::npos);
    // (as the original shortens "W/A/S/D" to "WASD")
    constexpr const char* MOVE = "Up/Left/Down/Right";
    if (const size_t m = out.find(MOVE); m != std::string::npos) out.replace(m, std::strlen(MOVE), "the Circle Pad");
    return out;
}

void Hints::load(World& world) {
    const HintsDef& def = world.area.hints;
    s_.assign(def.list.size(), State());
    for (size_t i = 0; i < s_.size(); i++) {
        const HintDef& d = def.list[i];
        State& h = s_[i];
        h.canDisable = d.canDisable;
        h.left = d.delay;
        h.countdown = world.state.flag("HintCountdown " + d.name);
        h.complete = world.state.flag("HintComplete " + d.name);
        h.shown = !text(d).empty();
        // SetBools
        if (d.onLoad) h.countdown = true;
        if (h.countdown) h.canDisable = true;
    }
    list_.clear();
    removals_.clear();
    visible_ = false;
    active_ = -1;
    fade_ = Fade::None;
    boxAlpha_ = 0;
    textAlpha_ = 1;
}

void Hints::save(World& world, int hint) const {
    const std::string& name = world.area.hints.list[hint].name;
    world.state.setFlag("HintCountdown " + name, s_[hint].countdown);
    world.state.setFlag("HintComplete " + name, s_[hint].complete);
}

void Hints::trigger(World& world, int hint) {
    State& h = s_[hint];
    h.canDisable = true;
    if (!h.complete) {
        h.countdown = true;
        save(world, hint);
    }
}

void Hints::disable(World& world, int hint) {
    State& h = s_[hint];
    if (!h.canDisable) return;
    h.countdown = false;
    h.complete = true;
    save(world, hint);
    remove(world, hint);
}

void Hints::enableDisabling(int hint) { s_[hint].canDisable = true; }

void Hints::gridEvents(World& world, const std::vector<GridEvent>& events) {
    using K = GridEvent::Kind;
    for (const TileMoveHintDef& m : world.area.hints.tileMoves) {
        if (!world.objects.activeInHierarchy(m.object)) continue; // (subscribed while enabled)
        for (const GridEvent& e : events) {
            const bool tile = (e.kind == K::TileMoveStart || e.kind == K::TileMoveEnd) && e.islandId == m.stile;
            if (tile || (e.kind == K::Move && m.anyTile)) runCalls(world, m.onMove);
        }
    }
}

void Hints::update(World& world, float dt) {
    for (int i = 0; i < (int)s_.size(); i++) {
        State& h = s_[i];
        if (!h.countdown || h.complete) continue;
        h.left -= dt;
        if (h.left < 0 && !h.added && h.shown) { // Display
            h.added = true;
            add(world, i);
        }
    }
    if (fade_ == Fade::None || world.frame == fadeSince_) return;
    fadeT_ += dt;
    const float d = world.area.hints.fadeDuration;
    const float u = std::min(fadeT_ / d, 1.0f);
    switch (fade_) {
    case Fade::None: break;
    case Fade::BoxIn: boxAlpha_ = u; break;
    case Fade::BoxOut: boxAlpha_ = 1 - u; break;
    case Fade::TextOut: textAlpha_ = 1 - u; break;
    case Fade::TextIn: textAlpha_ = u; break;
    }
    if (fadeT_ < d) return;
    const Fade done = fade_;
    fade_ = Fade::None;
    if (done == Fade::TextOut) { // FadeToNextHint's second half
        active_ = list_.front();
        startFade(world, Fade::TextIn);
        return;
    }
    if (done == Fade::BoxOut) active_ = -1;
    refresh(world);
}

void Hints::add(World& world, int hint) {
    if (contains(list_, hint)) return;
    list_.push_back(hint);
    refresh(world);
}

void Hints::remove(World& world, int hint) {
    if (!contains(list_, hint)) return;
    if (fade_ != Fade::None) { // (after the fade: WaitForRemoval)
        if (!contains(removals_, hint)) removals_.push_back(hint);
        return;
    }
    list_.erase(std::find(list_.begin(), list_.end(), hint));
    startFade(world, list_.empty() ? Fade::BoxOut : Fade::TextOut);
}

void Hints::refresh(World& world) {
    if (!visible_) {
        if (!list_.empty()) {
            visible_ = true;
            active_ = list_.front();
            startFade(world, Fade::BoxIn);
        }
        return;
    }
    if (!removals_.empty() && fade_ == Fade::None) {
        const int hint = removals_.front();
        removals_.erase(removals_.begin());
        remove(world, hint);
    }
    if (fade_ != Fade::None) return;
    if (list_.empty()) {
        visible_ = false;
    } else if (active_ < 0) {
        active_ = list_.front();
        startFade(world, Fade::BoxIn);
    }
}

void Hints::startFade(World& world, Fade fade) {
    fade_ = fade;
    fadeT_ = 0;
    fadeSince_ = world.frame;
    if (fade == Fade::BoxIn) boxAlpha_ = 0;
    if (fade == Fade::TextIn) textAlpha_ = 0;
}
