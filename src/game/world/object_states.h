// Runtime state of an Area's scene objects (Area::objects): the hierarchy (parents can change at
// runtime: NPC.Teleport reparents), active state as changed by GameObject.SetActive, placement
// (owner STile + offset) for objects that move at runtime (NPCs, carried items), their SpriteRenderers'
// state as scripts change it (flipX, enabled, sprite, a flash fill, sorting order), and colliders' enabled flags
// (Collider2D.enabled). Starts as authored.
//
// Like Unity: an object is active in the hierarchy only if it and every ancestor are active
// (activeSelf), so SetActive on a parent hides or shows everything under it, and showing a parent
// again leaves children that are inactive themselves hidden. Placement follows the hierarchy too:
// moving an object moves everything under it, and an object belongs to the STile its parent does
// (an STile's own object: that tile). Components (world sprites, colliders, ...) are active when
// their object is (and they are enabled), and sit at their object's placement.
#pragma once

#include <climits>
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
    // transform.SetParent(parent) (-1 = the scene root), keeping the offset set on the object (callers
    // then place it: World::placeObject). Ignored if `parent` is `object` or under it.
    void setParent(const Area& area, int object, int parent);

    int parent(int object) const { return parent_[object]; }
    bool activeInHierarchy(int object) const { return inHierarchy_[object] != 0; }
    bool active(const WorldSprite& ws) const { return activeInHierarchy(ws.object); }
    bool active(const ObjectCollider& c) const { return colliderEnabled_[c.slot] && activeInHierarchy(c.object); }
    // Collider2D.enabled of the collider with that ObjectCollider::slot.
    void setColliderEnabled(int slot, bool enabled) { colliderEnabled_[slot] = enabled ? 1 : 0; }

    // The STile (islandId, 0 = world) an object's components are placed relative to, and their
    // offset from their exported positions.
    int owner(int object) const { return owner_[object]; }
    float offsetX(int object) const { return offX_[object]; }
    float offsetY(int object) const { return offY_[object]; }
    // The offset set on the object itself (setPlacement's dx, dy; not inherited).
    float selfOffsetX(int object) const { return selfDx_[object]; }
    float selfOffsetY(int object) const { return selfDy_[object]; }

    // SpriteRenderer.flipX of the sprite on `object` (not inherited): as exported (world sprites'
    // flip) until a script sets it.
    void setFlipX(int object, bool flip) { flipX_[object] = flip ? 1 : 0; }
    bool flipX(int object) const { return flipX_[object] != 0; }
    // SpriteRenderer.enabled of the sprite on `object` (not inherited; starts as exported).
    void setRendererEnabled(int object, bool on) { rendererOn_[object] = on ? 1 : 0; }
    bool rendererEnabled(int object) const { return rendererOn_[object] != 0; }
    // SpriteRenderer.sprite set by a script: an Area::sprites index, NO_SPRITE (null: draws nothing),
    // or -1 = as exported (or animated).
    static constexpr int NO_SPRITE = -2;
    void setSprite(int object, int sprite) { sprite_[object] = sprite; }
    int sprite(int object) const { return sprite_[object]; }
    // A solid color over the sprite's shape (FlashWhiteSprite's white material), RGBA; 0 = none.
    void setFill(int object, uint32_t rgba) { fill_[object] = rgba; }
    uint32_t fill(int object) const { return fill_[object]; }
    // SpriteRenderer.sortingOrder set by a script, or EXPORTED_ORDER.
    static constexpr int EXPORTED_ORDER = INT_MIN;
    void setSortingOrder(int object, int order) { sortingOrder_[object] = order; }
    int sortingOrder(int object) const { return sortingOrder_[object]; }

    // Objects not where they were exported (another owner, or an offset): physics checks their
    // colliders on their own, as the per-owner broadphase groups place colliders as exported.
    // (Kept up to date as placements change, in object index order.)
    const std::vector<int>& moved() const { return moved_; }
    bool isMoved(const Area& area, int object) const { return owner_[object] != area.objects[object].owner || offX_[object] != 0 || offY_[object] != 0; }

    // Bumped when active state / placement changes, so per-frame work can skip frames where what
    // it depends on didn't change (Starts watch activity; placement has no reader yet: for walks).
    uint32_t activeVersion() const { return activeVersion_; }
    uint32_t placementVersion() const { return placementVersion_; }

private:
    void buildOrder();                         // order_ etc. from parent_
    void propagate(const Area& area, int object); // effective state of `object` and everything under it
    void updateMoved(const Area& area, int object); // moved_ for `object` and everything under it

    std::vector<int> parent_;              // runtime parent, -1 = root
    std::vector<int> order_, at_, end_;    // depth-first order; object's index in it; end of its subtree
    std::vector<uint8_t> self_, inHierarchy_; // active, per object
    std::vector<int> selfOwner_, owner_;   // owner set on the object (-1 = none) / effective
    std::vector<float> selfDx_, selfDy_, offX_, offY_; // offset set on the object / effective (sum)
    std::vector<uint8_t> flipX_;
    std::vector<uint8_t> rendererOn_;      // per object (objects without a sprite: 1)
    std::vector<int> sprite_;
    std::vector<uint32_t> fill_;
    std::vector<int> sortingOrder_;
    std::vector<uint8_t> colliderEnabled_; // per ObjectCollider::slot
    uint32_t activeVersion_ = 0, placementVersion_ = 0;
    std::vector<int> moved_;               // moved(), sorted
};
