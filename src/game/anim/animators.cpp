#include "game/anim/animators.h"

#include <cassert>
#include <cmath>

#include "game/world/world.h"

void Animators::reset(const Area& area) {
    s_.assign(area.animators.size(), State());
    for (size_t i = 0; i < s_.size(); i++) {
        s_[i].controller = area.animators[i].controller;
        for (const AnimatorDef::Target& t : area.animators[i].targets) {
            s_[i].targetX.push_back(t.localX);
            s_[i].targetY.push_back(t.localY);
        }
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

void Animators::update(World& world, float dt) {
    const Area& area = world.area;
    enable(world);
    fired_.clear();
    for (size_t i = 0; i < s_.size(); i++) {
        State& st = s_[i];
        if (!st.enabled) continue;
        const AnimatorControllerDef& c = area.animatorControllers[st.controller];
        if (c.states.empty()) continue;
        const AnimatorStateDef& s = c.states[st.state];
        const float before = s.length > 0 ? st.time / s.length : 1;
        const float from = st.time;
        st.time += dt * s.speed;
        // Clip events between the last time and now (each loop's, for a looping clip; one at the
        // state's very start counts once it begins).
        for (const auto& [at, function] : s.events) {
            if (s.length <= 0) break;
            float t = s.loop ? at + std::ceil((from - at) / s.length) * s.length : at; // the first pass from `from` on
            for (; t <= st.time; t += s.length) {
                if (t > from || (t == 0 && from == 0)) fired_.push_back({(int)i, &function});
                if (!s.loop) break;
            }
        }
        // Any State transitions first, then the state's own, in order; one per frame.
        const float normalized = s.length > 0 ? st.time / s.length : 1;
        // An exit time of 1 or more holds from then on; below 1 only on the frame the normalized time
        // passes it in a loop (k + exitTime, k = 0, 1, ...; never the 0 a state starts at). Measured in
        // Unity (Slider3DSSceneSurvey.SurveyExitTimesBatch): Cat1's 0.8 s WakeUp (exit 0) plays all 48
        // frames, the bubble's formBubble waits for its 5 s idle loop's end.
        auto exitReached = [&](float exitTime) {
            if (exitTime >= 1 || s.length <= 0) return normalized >= exitTime;
            const float passed = std::ceil(normalized - exitTime) - 1 + exitTime; // the last k + exitTime below now
            return passed > 0 && passed >= before;
        };
        auto fires = [&](const AnimatorTransitionDef& t) {
            if (t.exitTime >= 0 && !exitReached(t.exitTime)) return false;
            for (const AnimatorConditionDef& k : t.conds)
                if (!k.holds(st.params[k.param])) return false;
            return t.exitTime >= 0 || !t.conds.empty(); // Unity needs one or the other
        };
        const AnimatorTransitionDef* taken = nullptr;
        for (const AnimatorTransitionDef& t : c.anyState)
            if (!taken && t.to != st.state && fires(t)) taken = &t;
        for (const AnimatorTransitionDef& t : s.transitions)
            if (!taken && fires(t)) taken = &t;
        if (taken) {
            for (const AnimatorConditionDef& k : taken->conds)
                if (c.paramTriggers[k.param]) st.params[k.param] = 0; // (a trigger is used up)
            st.state = taken->to;
            st.time = 0;
        }
        refreshSprite(area, st);
        applyMoves(world, (int)i);
    }
}

void Animators::applyMoves(World& world, int animator) {
    const Area& area = world.area;
    const AnimatorDef& def = area.animators[animator];
    if (def.targets.empty()) return;
    State& st = s_[animator];
    const AnimatorStateDef& s = area.animatorControllers[st.controller].states[st.state];
    const float t = s.length <= 0 ? 0 : s.loop ? std::fmod(st.time, s.length) : std::min(st.time, s.length);
    // What the state doesn't move: back to the default (writeDefaults) or left as it is.
    scratchX_ = st.targetX;
    scratchY_ = st.targetY;
    if (s.writeDefaults)
        for (size_t k = 0; k < def.targets.size(); k++) {
            scratchX_[k] = def.targets[k].localX;
            scratchY_[k] = def.targets[k].localY;
        }
    for (const MoveCurveDef& c : s.moves) {
        const int k = def.curveTarget[c.id];
        if (k < 0) continue;
        if (!c.x.empty()) scratchX_[k] = evalKeys(c.x, t);
        if (!c.y.empty()) scratchY_[k] = evalKeys(c.y, t);
    }
    for (size_t k = 0; k < def.targets.size(); k++) {
        const AnimatorDef::Target& target = def.targets[k];
        const float x = scratchX_[k], y = scratchY_[k];
        if (x == st.targetX[k] && y == st.targetY[k]) continue;
        // The Animator owns its targets' placement: nothing else may have moved them since.
        assert(target.object < 0 || (world.objects.selfOffsetX(target.object) == st.targetX[k] - target.localX &&
                                     world.objects.selfOffsetY(target.object) == st.targetY[k] - target.localY));
        st.targetX[k] = x;
        st.targetY[k] = y;
        if (target.object >= 0) world.objects.setPlacement(area, target.object, -1, x - target.localX, y - target.localY);
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
    setFloat(world, animator, param, value ? 1.0f : 0.0f);
}

void Animators::setFloat(const World& world, int animator, const std::string& param, float value) {
    setParam(world, animator, world.area.animatorControllers[s_[animator].controller].paramIndex(param), value);
}

void Animators::setParam(const World& world, int animator, int param, float value) {
    syncEnabled(world, animator);
    if (param >= 0) s_[animator].params[param] = value;
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

float Animators::paramValue(const Area& area, int animator, const std::string& name) const {
    const State& st = s_[animator];
    const int p = area.animatorControllers[st.controller].paramIndex(name);
    return p >= 0 ? st.params[p] : 0;
}
