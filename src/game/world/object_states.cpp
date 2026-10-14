#include "game/world/object_states.h"

void ObjectStates::init(const Area& area) {
    const size_t n = area.objects.size();
    parent_.resize(n);
    self_.resize(n);
    inHierarchy_.resize(n);
    selfOwner_.assign(n, -1);
    owner_.assign(n, 0);
    selfDx_.assign(n, 0);
    selfDy_.assign(n, 0);
    offX_.assign(n, 0);
    offY_.assign(n, 0);
    flipX_.assign(n, -1);
    for (size_t i = 0; i < n; i++) {
        parent_[i] = area.objects[i].parent;
        self_[i] = area.objects[i].activeSelf;
    }
    buildOrder();
    for (int root = 0; root < (int)n; root++)
        if (parent_[root] < 0) propagate(area, root);
    activeVersion_++;
    placementVersion_++;
}

void ObjectStates::buildOrder() {
    const int n = (int)parent_.size();
    // Children lists (in index order), then an iterative depth-first walk from each root.
    std::vector<int> first(n, -1), next(n, -1), last(n, -1);
    for (int i = 0; i < n; i++) {
        const int p = parent_[i];
        if (p < 0) continue;
        if (last[p] < 0) first[p] = i;
        else next[last[p]] = i;
        last[p] = i;
    }
    order_.clear();
    at_.assign(n, 0);
    end_.assign(n, 0);
    std::vector<int> stack;
    for (int root = 0; root < n; root++) {
        if (parent_[root] >= 0) continue;
        stack.push_back(root);
        while (!stack.empty()) {
            const int o = stack.back();
            if (o >= 0) {
                at_[o] = (int)order_.size();
                order_.push_back(o);
                stack.back() = ~o; // revisit after the children: mark the subtree's end
                // Push children in reverse so they're visited in index order.
                std::vector<int> kids;
                for (int c = first[o]; c >= 0; c = next[c]) kids.push_back(c);
                for (auto it = kids.rbegin(); it != kids.rend(); ++it) stack.push_back(*it);
            } else {
                end_[~o] = (int)order_.size();
                stack.pop_back();
            }
        }
    }
}

// Effective state of `object`'s subtree: in depth-first order every parent comes before its children.
void ObjectStates::propagate(const Area& area, int object) {
    for (int k = at_[object]; k < end_[object]; k++) {
        const int i = order_[k];
        const int p = parent_[i];
        if (p < 0) {
            inHierarchy_[i] = self_[i];
            owner_[i] = selfOwner_[i] >= 0 ? selfOwner_[i] : area.objects[i].owner;
            offX_[i] = selfDx_[i];
            offY_[i] = selfDy_[i];
            continue;
        }
        inHierarchy_[i] = self_[i] && inHierarchy_[p];
        // Under its exported parent an object belongs to its own STile (differs only for an STile's
        // own object); moved under another, it belongs to wherever that one is.
        const bool ownTile = p == area.objects[i].parent && area.objects[i].owner != area.objects[p].owner;
        owner_[i] = selfOwner_[i] >= 0 ? selfOwner_[i] : ownTile ? area.objects[i].owner : owner_[p];
        offX_[i] = selfDx_[i] + offX_[p];
        offY_[i] = selfDy_[i] + offY_[p];
    }
}

void ObjectStates::setActive(const Area& area, int object, bool active) {
    if (self_[object] == (uint8_t)active) return;
    self_[object] = active;
    propagate(area, object);
    activeVersion_++;
}

void ObjectStates::setPlacement(const Area& area, int object, int owner, float dx, float dy) {
    selfOwner_[object] = owner;
    selfDx_[object] = dx;
    selfDy_[object] = dy;
    propagate(area, object);
    placementVersion_++;
}

void ObjectStates::moveOnto(const Area& area, int object, int target) {
    if (at_[target] >= at_[object] && at_[target] < end_[object]) return; // itself or its own child
    // Its components' exported positions are relative to its exported spot; the target's spot
    // (exported position + its offset, on its owner) is where they go.
    parent_[object] = target;
    selfOwner_[object] = -1;
    selfDx_[object] = area.objects[target].x - area.objects[object].x;
    selfDy_[object] = area.objects[target].y - area.objects[object].y;
    buildOrder();
    propagate(area, object);
    activeVersion_++;
    placementVersion_++;
}
