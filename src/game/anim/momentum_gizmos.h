// MomentumGizmo: trees and bushes (Area::momentumGizmos) sway with their tile, through their Animators
// (the Tree / SimpleDynamicGizmo controllers). Each OnEnable sets the Animator's float random
// (Random.Range(0, 1)); its tile's moves (STile.onChangeMove, the grid's TileMoveStart / TileMoveEnd)
// set the ints dx / dy to where the tile goes, then 0 / 0 and the trigger finishedMoving. A gizmo
// that's off hears nothing (it unsubscribes in OnDisable).
#pragma once

#include <cstdint>
#include <vector>

#include "game/data/area.h"
#include "game/grid/grid.h"

class World;

class MomentumGizmos {
public:
    void reset(const Area& area); // none enabled yet (a scene load)
    // OnEnable for the gizmos whose object turned on since the last call (after Animators::enable,
    // which rebinds their Animators first).
    void enable(World& world);
    void gridEvents(World& world, const std::vector<GridEvent>& events);

private:
    float random(); // [0, 1)

    std::vector<uint8_t> enabled_; // per gizmo: its object was active at the last enable()
    uint32_t rng_ = 0x2545f491u;
};
