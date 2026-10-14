#include "game/anim/animators.h"

#include "game/world/world.h"

void Animators::reset(const Area& area) {
    s_.assign(area.animators.size(), State());
    for (size_t i = 0; i < s_.size(); i++) {
        s_[i].controller = area.animators[i].controller;
        rebind(area, s_[i]);
    }
}

void Animators::rebind(const Area& area, State& st) {
    const AnimatorControllerDef& c = area.animatorControllers[st.controller];
    st.state = c.defaultState;
    st.time = 0;
    st.params = c.paramDefaults;
    refreshSprite(area, st);
}

void Animators::refreshSprite(const Area& area, State& st) {
    const AnimatorControllerDef& c = area.animatorControllers[st.controller];
    if (c.states.empty()) return;
    const int anim = c.states[st.state].anim;
    if (anim >= 0) st.sprite = area.anims[anim].frameAt(st.time);
}

void Animators::syncEnabled(const World& world, int animator) {
    const Area& area = world.area;
    State& st = s_[animator];
    const int object = area.animators[animator].object;
    const bool active = object < 0 || world.objects.activeInHierarchy(object); // (-1: the player's)
    if (active && !st.enabled && !area.animators[animator].keepState) rebind(area, st);
    st.enabled = active;
}

void Animators::enable(const World& world) {
    for (int i = 0; i < (int)s_.size(); i++) syncEnabled(world, i);
}

void Animators::update(const World& world, float dt) {
    const Area& area = world.area;
    enable(world);
    for (size_t i = 0; i < s_.size(); i++) {
        State& st = s_[i];
        if (!st.enabled) continue;
        const AnimatorControllerDef& c = area.animatorControllers[st.controller];
        if (c.states.empty()) continue;
        const AnimatorStateDef& s = c.states[st.state];
        st.time += dt * s.speed;
        // Any State transitions first, then the state's own, in order; one per frame.
        const int anim = s.anim;
        const float length = anim >= 0 ? area.anims[anim].length : 0;
        const float normalized = length > 0 ? st.time / length : 1;
        auto fires = [&](const AnimatorTransitionDef& t) {
            if (t.exitTime >= 0 && normalized < t.exitTime) return false;
            for (const auto& k : t.conds)
                if ((st.params[k.first] != 0) != k.second) return false;
            return t.exitTime >= 0 || !t.conds.empty(); // Unity needs one or the other
        };
        const AnimatorTransitionDef* taken = nullptr;
        for (const AnimatorTransitionDef& t : c.anyState)
            if (!taken && t.to != st.state && fires(t)) taken = &t;
        for (const AnimatorTransitionDef& t : s.transitions)
            if (!taken && fires(t)) taken = &t;
        if (taken) {
            st.state = taken->to;
            st.time = 0;
        }
        refreshSprite(area, st);
    }
}

void Animators::play(const World& world, int animator, const std::string& state, bool restart) {
    syncEnabled(world, animator);
    const Area& area = world.area;
    State& st = s_[animator];
    const int to = area.animatorControllers[st.controller].stateIndex(state);
    if (to < 0 || (to == st.state && !restart)) return;
    st.state = to;
    st.time = 0;
    refreshSprite(area, st);
}

void Animators::setBool(const World& world, int animator, const std::string& param, bool value) {
    syncEnabled(world, animator);
    const Area& area = world.area;
    State& st = s_[animator];
    const int p = area.animatorControllers[st.controller].paramIndex(param);
    if (p >= 0) st.params[p] = value ? 1 : 0;
}

void Animators::setController(const World& world, int animator, int controller) {
    syncEnabled(world, animator);
    State& st = s_[animator];
    st.controller = controller;
    rebind(world.area, st);
}

const std::string& Animators::stateName(const Area& area, int animator) const {
    const State& st = s_[animator];
    static const std::string none;
    const AnimatorControllerDef& c = area.animatorControllers[st.controller];
    return c.states.empty() ? none : c.states[st.state].name;
}

bool Animators::param(const Area& area, int animator, const std::string& name) const {
    const State& st = s_[animator];
    const int p = area.animatorControllers[st.controller].paramIndex(name);
    return p >= 0 && st.params[p] != 0;
}
