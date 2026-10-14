// Runtime state of an Area's scene objects (Area::objects): the hierarchy (parents can change at
// runtime: NPC.Teleport reparents), active state as changed by GameObject.SetActive, placement
// (owner STile + offset) for objects that move at runtime (NPCs, carried items) and sprite flips set
// by events. Starts as authored.
//
// Like Unity: an object is active in the hierarchy only if it and every ancestor are active
// (activeSelf), so SetActive on a parent hides or shows everything under it, and showing a parent
// again leaves children that are inactive themselves hidden. Placement follows the hierarchy too:
// moving an object moves everything under it, and an object belongs to the STile its parent does
// (an STile's own object: that tile). Components (world sprites, colliders, ...) are active when
// their object is (and they are enabled), and sit at their object's placement.
#pragma once

#include <cstdint>
#include <vector>

#include "game/data/area.h"

class ObjectStates {
public:
    void init(const Area& area);
    // GameObject.SetActive(active) on Area::objects[object].
    void setActive(const Area& area, int object, bool active);
    // Moves `object` (and its children): `owner` replaces the STile its components are placed
    // relative to (-1 = the one it belongs to) and (dx, dy) is added to their exported positions.
    void setPlacement(const Area& area, int object, int owner, float dx, float dy);
    // transform.position = target.position; transform.parent = target (NPC.Teleport): `object`
    // moves onto `target` and from then on follows it, in placement and activity. Ignored if
    // `target` is `object` or under it.
    void moveOnto(const Area& area, int object, int target);

    int parent(int object) const { return parent_[object]; }
    bool activeInHierarchy(int object) const { return inHierarchy_[object] != 0; }
    bool active(const WorldSprite& ws) const { return activeInHierarchy(ws.object); }
    bool active(const ObjectCollider& c) const { return c.enabled && activeInHierarchy(c.object); }

    // The STile (islandId, 0 = world) an object's components are placed relative to, and their
    // offset from their exported positions.
    int owner(int object) const { return owner_[object]; }
    float offsetX(int object) const { return offX_[object]; }
    float offsetY(int object) const { return offY_[object]; }

    // SpriteRenderer.flipX of the sprite on `object` (not inherited): 1 / 0, or -1 = as exported.
    void setFlipX(int object, bool flip) { flipX_[object] = flip ? 1 : 0; }
    int flipX(int object) const { return flipX_[object]; }

    // Bumped when active state / placement changes, so per-frame work can skip frames where what
    // it depends on didn't change (Starts watch activity; walking NPCs change placement every frame).
    uint32_t activeVersion() const { return activeVersion_; }
    uint32_t placementVersion() const { return placementVersion_; }

private:
    void buildOrder();                         // order_ etc. from parent_
    void propagate(const Area& area, int object); // effective state of `object` and everything under it

    std::vector<int> parent_;              // runtime parent, -1 = root
    std::vector<int> order_, at_, end_;    // depth-first order; object's index in it; end of its subtree
    std::vector<uint8_t> self_, inHierarchy_; // active, per object
    std::vector<int> selfOwner_, owner_;   // owner set on the object (-1 = none) / effective
    std::vector<float> selfDx_, selfDy_, offX_, offY_; // offset set on the object / effective (sum)
    std::vector<int8_t> flipX_;
    uint32_t activeVersion_ = 0, placementVersion_ = 0;
};
