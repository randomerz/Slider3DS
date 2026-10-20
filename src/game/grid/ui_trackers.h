// UITrackerManager + UITracker: pins on the Artifact that follow something in the world — the
// player (Player.AddTracker: the playerFull* circles, blinking 3 s every 10 s) and, until Slider 2 is
// collected, VillageGrid.Start's pin on its collectible (game/village: circle3, blinking 3 s once).
// World::trackers (set in reloadObjects, as each scene start's; VillageGrid.RemoveSlider2Tracker
// removes that one); `place` says where each goes, ArtifactUI draws them.
//   A pin shows while its target is active in the hierarchy (the manager's SetActive); showing again
// restarts its blinking (OnEnable). While blinking it shows its blink sprite for half of each second.
// Off the grid it shows its "off map" sprites (white), if it has them.
#pragma once

#include <vector>

class World;

constexpr float PLAYER_TRACKER_BLINK_S = 3, PLAYER_TRACKER_REPEAT_S = 10; // Player.AddTracker
constexpr float SLIDER2_TRACKER_BLINK_S = 3;                              // VillageGrid.Start

struct UiTracker {
    static constexpr int PLAYER = -2; // `object` for the player
    int object = PLAYER;              // Area::objects index, or PLAYER
    int sprite = -1, blinkSprite = -1, offMapSprite = -1, offMapBlinkSprite = -1; // Area::sprites (-1: none)
    float blinkTime = -1;      // s it blinks for (StartBlinking; -1: never)
    float repeatAfter = -1;    // s from one blink's start to the next (-1: blinks once)

    // Runtime (UITracker's).
    bool shown = true;         // its target is active
    bool blinking = false;     // this frame: the blink sprite
    float blinkLeft = 0, toSwap = 1, repeatLeft = 0;
};

// Where a pin goes (UITrackerManager.UpdateTrackerPostion) and what it shows (DetermineCurrentSprite).
struct PinPlace {
    int island = 0;   // on this tile's button (its image's center is the origin); 0: on the panel (its center)
    int left = 0, top = 0; // the sprite's top-left, whole UI px (y up, so `top` is its upper edge)
    int sprite = -1;  // Area::sprites (-1: nothing to draw)
};

class UiTrackers {
public:
    void clear() { trackers_.clear(); }
    // AddNewTracker (its timers as StartBlinking leaves them).
    void add(UiTracker t);
    void remove(int object); // RemoveTracker: every pin on `object`
    // UITracker.Update for `dt` s, after the manager's SetActive(target.activeInHierarchy).
    void update(const World& world, float dt);
    const std::vector<UiTracker>& trackers() const { return trackers_; }

    // Where `t` goes: on the button of the tile under its target, from the tile's center at 27/17
    // UI px per world unit (in a house: the button's center); else on the panel from the grid's
    // middle (17, 17) at 37/17, clamped — to +-70 x, and to `panelHalfH` (the UI px of the panel
    // the screen shows above / below its center) less half the pin, so it stays whole (port: Unity's
    // +-60 would cut it in half on the 3DS's cropped view). Snapped to whole UI px (the Artifact's
    // pixel grid). Not shown: sprite -1.
    PinPlace place(const World& world, const UiTracker& t, float panelHalfH) const;

private:
    std::vector<UiTracker> trackers_;
};
