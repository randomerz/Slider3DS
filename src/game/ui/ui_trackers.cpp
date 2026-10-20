#include "game/ui/ui_trackers.h"

#include <algorithm>

#include "game/world/world.h"

namespace {

// StartBlinking / OnEnable: a blink from the start, the repeat counting down with it.
void restartBlink(UiTracker& t) {
    t.blinkLeft = t.blinkTime;
    t.toSwap = 1;
    t.repeatLeft = t.repeatAfter;
    t.blinking = false;
}

} // namespace

void UiTrackers::add(UiTracker t) {
    t.shown = true; // (made active; the next update hides it if its target is off)
    restartBlink(t);
    trackers_.push_back(t);
}

void UiTrackers::remove(int object) {
    trackers_.erase(std::remove_if(trackers_.begin(), trackers_.end(), [&](const UiTracker& t) { return t.object == object; }),
                    trackers_.end());
}

void UiTrackers::update(const World& world, float dt) {
    for (UiTracker& t : trackers_) {
        const bool active = t.object == UiTracker::PLAYER || world.objects.activeInHierarchy(t.object);
        if (active && !t.shown) restartBlink(t); // (OnEnable)
        t.shown = active;
        if (!active || t.blinkTime == -1) continue;
        // UITracker.Update, step for step.
        if (t.blinkLeft > 0) {
            t.blinking = !(t.toSwap > 0.5f || t.toSwap < 0);
        } else {
            if (t.repeatLeft < 0 && t.repeatAfter != -1) { // (a repeat blink)
                t.blinkLeft = t.blinkTime;
                t.toSwap = 1;
                t.repeatLeft = t.repeatAfter;
            }
            t.blinking = false;
        }
        t.toSwap -= dt;
        t.blinkLeft -= dt;
        t.repeatLeft -= dt;
        if (t.toSwap < 0) t.toSwap = 1;
    }
}
