// Runtime active state of an Area's scene objects (Area::objects), as changed by
// GameObject.SetActive from start checks, conditions and events. Starts as authored.
//
// Like Unity: an object is active in the hierarchy only if it and every ancestor are active
// (activeSelf), so SetActive on a parent hides or shows everything under it, and showing a parent
// again leaves children that are inactive themselves hidden. Components (world sprites,
// colliders, ...) are active when their object is, and they are enabled.
#pragma once

#include <cstdint>
#include <vector>

#include "game/area.h"

class ObjectStates {
public:
    void init(const Area& area);
    // GameObject.SetActive(active) on Area::objects[object].
    void setActive(const Area& area, int object, bool active);

    bool activeInHierarchy(int object) const { return inHierarchy_[object] != 0; }
    bool active(const WorldSprite& ws) const { return activeInHierarchy(ws.object); }
    bool active(const ObjectCollider& c) const { return c.enabled && activeInHierarchy(c.object); }

private:
    std::vector<uint8_t> self_, inHierarchy_; // per Area::objects index
};
