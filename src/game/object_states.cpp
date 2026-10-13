#include "game/object_states.h"

namespace {

// activeInHierarchy for objects [from, end): Area lists parents before children, so one pass
// in order sees every parent's value first.
void propagate(const Area& area, const std::vector<uint8_t>& self, std::vector<uint8_t>& inHierarchy, size_t from) {
    for (size_t i = from; i < area.objects.size(); i++) {
        const int parent = area.objects[i].parent;
        inHierarchy[i] = self[i] && (parent < 0 || inHierarchy[parent]);
    }
}

} // namespace

void ObjectStates::init(const Area& area) {
    self_.resize(area.objects.size());
    inHierarchy_.resize(area.objects.size());
    for (size_t i = 0; i < area.objects.size(); i++) self_[i] = area.objects[i].activeSelf;
    propagate(area, self_, inHierarchy_, 0);
}

void ObjectStates::setActive(const Area& area, int object, bool active) {
    if (self_[object] == (uint8_t)active) return;
    self_[object] = active;
    // Descendants come after the object; objects before it can't change.
    propagate(area, self_, inHierarchy_, (size_t)object);
}
