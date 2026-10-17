// What scripts set on the Artifact's buttons (UIArtifact / ArtifactTileButton), part of World:
// each button's icons' Image.enabled (RuinsMapIcons) and whether it shows complete
// (UIArtifact.SetButtonComplete: its completed sprite). ArtifactUI draws them.
#pragma once

#include <cstdint>
#include <vector>

#include "game/data/area.h"

class ArtifactButtons {
public:
    void reset(const Area& area); // a scene load: icons as exported, nothing complete

    // Icon `icon` of button `islandId` shown; false for a button or icon that doesn't exist.
    bool iconShown(int islandId, int icon) const;
    void setIcon(int islandId, int icon, bool shown);
    bool complete(int islandId) const;
    void setComplete(int islandId, bool complete);

private:
    int index(int islandId) const; // in Area::artifact.buttons, -1 = none

    std::vector<int> ids_;                   // per button: its islandId
    std::vector<std::vector<uint8_t>> icons_; // per button, per icon: shown
    std::vector<uint8_t> complete_;
};
