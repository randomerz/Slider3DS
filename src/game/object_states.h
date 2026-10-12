// Runtime active state of an Area's GameObjects (world sprites and object colliders), as changed by
// GameObject.SetActive from start checks, conditions and events. Starts as authored.
//
// Like Unity's activeInHierarchy: an object is active only if it and every ancestor are active
// (activeSelf), so SetActive on a parent hides or shows everything under it, and showing a parent
// again leaves children that are inactive themselves hidden.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "game/area.h"

class ObjectStates {
public:
    void init(const Area& area);
    // GameObject.SetActive(active) on the object at hierarchy path `path`.
    void setActive(const Area& area, const std::string& path, bool active);

    bool spriteActive(size_t i) const { return sprites_[i] != 0; }   // Area::worldSprites index
    bool colliderActive(size_t i) const { return colliders_[i] != 0; } // Area::objectColliders index

private:
    // activeInHierarchy (&& enabled) of the object at `path`, from the overrides and its authored
    // inactive ancestors.
    bool compute(const std::string& path, const std::vector<std::string>& inactiveAt, bool enabled) const;

    std::vector<std::pair<std::string, bool>> overrides_; // activeSelf set at runtime, by path
    std::vector<uint8_t> sprites_, colliders_;
};
