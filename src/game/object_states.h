// Runtime state of an Area's scene objects (Area::objects): active state as changed by
// GameObject.SetActive, and placement (owner STile + offset) for objects that move at runtime
// (NPCs walking, collectibles re-attached, carried items). Starts as authored.
//
// Like Unity: an object is active in the hierarchy only if it and every ancestor are active
// (activeSelf), so SetActive on a parent hides or shows everything under it, and showing a parent
// again leaves children that are inactive themselves hidden. Placement follows the hierarchy too:
// moving an object moves everything under it. Components (world sprites, colliders, ...) are active
// when their object is (and they are enabled), and sit at their object's placement.
#pragma once

#include <cstdint>
#include <vector>

#include "game/area.h"

class ObjectStates {
public:
    void init(const Area& area);
    // GameObject.SetActive(active) on Area::objects[object].
    void setActive(const Area& area, int object, bool active);
    // Moves `object` (and its children): `owner` replaces the exported owner STile of its components
    // (-1 = keep theirs) and (dx, dy) is added to their exported positions.
    void setPlacement(const Area& area, int object, int owner, float dx, float dy);

    bool activeInHierarchy(int object) const { return inHierarchy_[object] != 0; }
    bool active(const WorldSprite& ws) const { return activeInHierarchy(ws.object); }
    bool active(const ObjectCollider& c) const { return c.enabled && activeInHierarchy(c.object); }

    // A component's owner STile: the object's runtime owner if it has one, else the exported one.
    int owner(int object, int exportedOwner) const {
        return owner_[object] >= 0 ? owner_[object] : exportedOwner;
    }
    float offsetX(int object) const { return offX_[object]; }
    float offsetY(int object) const { return offY_[object]; }

    // Bumped by every change, so per-frame work can skip frames where nothing changed.
    uint32_t version() const { return version_; }

private:
    void propagate(const Area& area, size_t from);

    std::vector<uint8_t> self_, inHierarchy_;         // active, per object
    std::vector<int> selfOwner_, owner_;              // placement set on the object / effective; -1 = exported
    std::vector<float> selfDx_, selfDy_, offX_, offY_; // offset set on the object / effective (sum)
    uint32_t version_ = 0;
};
