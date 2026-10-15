// RuinsArrow (the ruins puzzle, Slider 7) with what it drives: the rods (RuinsArrowRod: sprite,
// sparkles, runes with a white flash), the Artifact corner icons (RuinsMapIcons), the sparkle trail
// toward the buried Slider 7 (ParticleTrail) and the arrow's BobAnimation. Ported step for step from
// their scripts: the arrow listens to SGrid.OnGridMove, SGridAnimator.OnSTileMoveEnd and
// SGrid.OnSTileEnabled (Grid::events), and CheckActiveRods reads the rods as the last of those left them.
// Not yet: the "MagicChimes1" / "Hat Click" sounds.
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "game/data/area.h"
#include "game/grid/grid.h"

class World;

namespace village {

class RuinsArrows {
public:
    void reset(const Area& area); // a scene load: nothing awake or started
    // Awake / Start of what became active: a rod's Awake turns it off (SetRod(false)), an arrow's
    // Start shows the state of the grid (UpdateArrowGeneral). World calls it with Starts.
    void activate(World& world);
    // The grid's events (Grid::events), for the arrows that are active (OnEnable subscribed them).
    void gridEvents(World& world, const std::vector<GridEvent>& events);
    // Per frame: the trail's spawns and repeats, the runes' flashes, the bob, and (LateUpdate) each
    // rod's onLastFrame.
    void update(World& world, float dt);

    // RuinsArrow.CheckActiveRods on Area::ruinsArrows[arrow]: a rod was lit by the last update of the rods.
    bool rodsOn(int arrow) const { return arrow >= 0 && arrow < (int)arrows_.size() && arrows_[arrow].anyRodsOn; }
    // Whether Artifact button `islandId`'s icon `icon` shows: as authored, unless a RuinsMapIcons
    // corner is that icon (then as the arrow last set it).
    bool iconShown(const Area& area, int islandId, int icon) const;

private:
    struct Rod {
        bool awake = false;
        bool onLastFrame = false; // runes shown as of the last LateUpdate
        float flash = -1;         // s since its runes' flash started, -1 = none
    };
    struct Stagger {              // one run of the trail: a spawn every `delay`
        size_t next = 0;
        float wait = 0;
    };
    struct Arrow {
        bool started = false;
        bool shown = false;       // spriteRenderer.enabled
        int sprite = -1;          // spriteRenderer.sprite (ObjectStates::NO_SPRITE = null)
        bool flipX = false;
        bool anyRodsOn = false;
        std::vector<Rod> rods;
        std::vector<uint8_t> icons; // per mapIcons entry: shown
        bool repeating = false;   // ParticleTrail's repeat coroutine
        float repeatWait = 0;
        std::vector<std::pair<float, float>> trail; // spawn points, relative to the trail's object
        std::vector<Stagger> staggers;
        int bobPixels = 0;
    };

    void general(World& world, int a);                  // UpdateArrowGeneral
    void setArrowActive(World& world, int a, bool value, const std::vector<int>& slots);
    void updateDirection(World& world, int a, const std::vector<int>& slots);
    void updateMap(World& world, int a, const std::string& gridString);
    void updateRods(World& world, int a, const std::string& gridString);
    void setRod(World& world, int a, int rod, bool value);
    void spawnTrail(World& world, int a);               // SpawnParticleTrail(true)
    void spawnNext(World& world, int a, Stagger& s);
    bool hasSlider7(const World& world) const;

    std::vector<Arrow> arrows_;
    float time_ = 0; // s since the scene loaded (BobAnimation's Time.time)
};

} // namespace village
