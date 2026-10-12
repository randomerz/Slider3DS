#include "game/object_states.h"

#include <algorithm>

namespace {

// `path` is `ancestor` or lies under it.
bool isUnder(const std::string& path, const std::string& ancestor) {
    return path.compare(0, ancestor.size(), ancestor) == 0 && (path.size() == ancestor.size() || path[ancestor.size()] == '/');
}

} // namespace

void ObjectStates::init(const Area& area) {
    overrides_.clear();
    sprites_.assign(area.worldSprites.size(), 0);
    colliders_.assign(area.objectColliders.size(), 0);
    for (size_t i = 0; i < sprites_.size(); i++) sprites_[i] = area.worldSprites[i].active;
    for (size_t i = 0; i < colliders_.size(); i++) colliders_[i] = area.objectColliders[i].active;
}

bool ObjectStates::compute(const std::string& path, const std::vector<std::string>& inactiveAt, bool enabled) const {
    if (!enabled) return false;
    // Every prefix of the path ending at a '/' (or the end) is an ancestor-or-self.
    for (size_t end = path.find('/'); ; end = path.find('/', end + 1)) {
        const size_t len = end == std::string::npos ? path.size() : end;
        bool active = true, overridden = false;
        for (const auto& o : overrides_)
            if (o.first.size() == len && path.compare(0, len, o.first) == 0) {
                active = o.second;
                overridden = true;
            }
        if (!overridden)
            for (const std::string& p : inactiveAt)
                if (p.size() == len && path.compare(0, len, p) == 0) active = false;
        if (!active) return false;
        if (end == std::string::npos) return true;
    }
}

void ObjectStates::setActive(const Area& area, const std::string& path, bool active) {
    auto it = std::find_if(overrides_.begin(), overrides_.end(), [&](const auto& o) { return o.first == path; });
    if (it != overrides_.end()) it->second = active;
    else overrides_.push_back({path, active});
    for (size_t i = 0; i < sprites_.size(); i++) {
        const WorldSprite& ws = area.worldSprites[i];
        if (isUnder(ws.path, path)) sprites_[i] = compute(ws.path, ws.inactiveAt, true);
    }
    for (size_t i = 0; i < colliders_.size(); i++) {
        const ObjectCollider& c = area.objectColliders[i];
        if (isUnder(c.path, path)) colliders_[i] = compute(c.path, c.inactiveAt, c.enabled);
    }
}
