#include "game/grid/artifact_buttons.h"

void ArtifactButtons::reset(const Area& area) {
    ids_.clear();
    icons_.clear();
    for (const ArtifactButtonDef& b : area.artifact.buttons) {
        ids_.push_back(b.islandId);
        icons_.push_back(b.iconsEnabled);
    }
    complete_.assign(ids_.size(), 0);
}

int ArtifactButtons::index(int islandId) const {
    for (size_t b = 0; b < ids_.size(); b++)
        if (ids_[b] == islandId) return (int)b;
    return -1;
}

bool ArtifactButtons::iconShown(int islandId, int icon) const {
    const int b = index(islandId);
    return b >= 0 && icon >= 0 && icon < (int)icons_[b].size() && icons_[b][icon];
}

void ArtifactButtons::setIcon(int islandId, int icon, bool shown) {
    const int b = index(islandId);
    if (b >= 0 && icon >= 0 && icon < (int)icons_[b].size()) icons_[b][icon] = shown ? 1 : 0;
}

bool ArtifactButtons::complete(int islandId) const {
    const int b = index(islandId);
    return b >= 0 && complete_[b];
}

void ArtifactButtons::setComplete(int islandId, bool complete) {
    const int b = index(islandId);
    if (b >= 0) complete_[b] = complete ? 1 : 0;
}
