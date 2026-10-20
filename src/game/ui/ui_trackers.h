// UITrackerManager + UITracker: pins on the Artifact that follow something in the world — the
// player (Player.AddTracker: the playerFull* circles, blinking 3 s every 10 s) and, until Slider 2 is
// collected, VillageGrid.Start's pin on its collectible (circle3, blinking 3 s once). World::trackers
// (added in reloadObjects, as each scene start's; VillageGrid.RemoveSlider2Tracker removes that one);
// ArtifactUI draws them over the buttons.
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

class UiTrackers {
public:
    void clear() { trackers_.clear(); }
    // AddNewTracker (its timers as StartBlinking leaves them).
    void add(UiTracker t);
    void remove(int object); // RemoveTracker: every pin on `object`
    // UITracker.Update for `dt` s, after the manager's SetActive(target.activeInHierarchy).
    void update(const World& world, float dt);
    const std::vector<UiTracker>& trackers() const { return trackers_; }

private:
    std::vector<UiTracker> trackers_;
};
