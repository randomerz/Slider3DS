#include "game/object_states.h"

void ObjectStates::init(const Area& area) {
    const size_t n = area.objects.size();
    self_.resize(n);
    inHierarchy_.resize(n);
    selfOwner_.assign(n, -1);
    owner_.assign(n, -1);
    selfDx_.assign(n, 0);
    selfDy_.assign(n, 0);
    offX_.assign(n, 0);
    offY_.assign(n, 0);
    flipX_.assign(n, -1);
    for (size_t i = 0; i < n; i++) self_[i] = area.objects[i].activeSelf;
    propagate(area, 0);
    activeVersion_++;
    placementVersion_++;
}

// Effective state for objects [from, end): Area lists parents before children, so one pass in
// order sees every parent's value first. Objects before `from` can't depend on it.
void ObjectStates::propagate(const Area& area, size_t from) {
    for (size_t i = from; i < area.objects.size(); i++) {
        const int parent = area.objects[i].parent;
        if (parent < 0) {
            inHierarchy_[i] = self_[i];
            owner_[i] = selfOwner_[i];
            offX_[i] = selfDx_[i];
            offY_[i] = selfDy_[i];
        } else {
            inHierarchy_[i] = self_[i] && inHierarchy_[parent];
            owner_[i] = selfOwner_[i] >= 0 ? selfOwner_[i] : owner_[parent];
            offX_[i] = selfDx_[i] + offX_[parent];
            offY_[i] = selfDy_[i] + offY_[parent];
        }
    }
}

void ObjectStates::setActive(const Area& area, int object, bool active) {
    if (self_[object] == (uint8_t)active) return;
    self_[object] = active;
    propagate(area, (size_t)object);
    activeVersion_++;
}

void ObjectStates::setPlacement(const Area& area, int object, int owner, float dx, float dy) {
    selfOwner_[object] = owner;
    selfDx_[object] = dx;
    selfDy_[object] = dy;
    propagate(area, (size_t)object);
    placementVersion_++;
}
