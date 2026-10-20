#include "game/grid/ui_trackers.h"

#include <algorithm>
#include <cmath>

#include "game/world/world.h"

namespace {

// UITrackerManager's (code constants) and UITracker's.
constexpr float TILE_SCALE = 27.0f / 17.0f;   // UI px per world unit from the tile's center
constexpr float CENTER = 17;                  // off the grid: from world (17, 17), the grid's middle,
constexpr float CENTER_SCALE = 37.0f / 17.0f; // at this scale,
constexpr float CLAMP_X = 70, CLAMP_Y = 60;   // clamped to the panel
constexpr float HOUSE_Y = -75;                // below it: in a house (UITracker.minY)

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

PinPlace UiTrackers::place(const World& world, const UiTracker& t, float panelHalfH) const {
    PinPlace p;
    if (!t.shown) return p;
    float x, y;
    if (t.object == UiTracker::PLAYER) {
        x = world.player.x;
        y = world.player.y;
    } else {
        world.objectPosition(t.object, x, y);
    }
    float cx = 0, cy = 0; // its center, UI px from the origin
    int bx, by;
    p.island = world.grid.stileUnder(x, y);
    if (p.island > 0 && world.artifact.findButton(p.island, bx, by)) { // (CalculateOffset)
        if (y >= HOUSE_Y) { // (in a house: the button's center; trackHousingAccurately isn't ported: refused at load)
            float tx, ty;
            world.grid.ownerBase(p.island, tx, ty);
            cx = (x - tx) * TILE_SCALE;
            cy = (y - ty) * TILE_SCALE;
        }
        p.sprite = t.blinking ? t.blinkSprite : t.sprite;
    } else { // (CalculateOffsetNullTile; DetermineCurrentSprite's off-map sprites)
        p.island = 0;
        p.sprite = t.offMapSprite >= 0 ? (t.blinking ? t.offMapBlinkSprite : t.offMapSprite)
                                       : (t.blinking ? t.blinkSprite : t.sprite);
        const float halfH = p.sprite >= 0 ? world.area.sprites[p.sprite].h * 0.5f : 0.0f;
        const float clampY = std::min(CLAMP_Y, panelHalfH - halfH);
        cx = std::clamp((x - CENTER) * CENTER_SCALE, -CLAMP_X, CLAMP_X);
        cy = std::clamp((y - CENTER) * CENTER_SCALE, -clampY, clampY);
    }
    if (p.sprite < 0) return p;
    const SpriteDef& S = world.area.sprites[p.sprite];
    // (its pivot is its center: the corner snapped, so odd-sized pins keep their half-pixel offset)
    p.left = (int)std::lround(cx - S.w * 0.5f);
    p.top = (int)std::lround(cy + S.h * 0.5f);
    return p;
}
