// The Artifact's tile screen on the bottom screen: draws the frame and the 3x3 tile buttons, and
// turns stylus taps and drags into moves, like UIArtifact.SelectButton / ButtonDragged /
// ButtonDragEnd and ArtifactTileButton(Animator):
//   - tap a collected tile with somewhere to go: it's selected (pressed) and its move options
//     highlight; tap an option to slide it there (it stays selected), anything else deselects
//     (tapping another collected tile selects that one instead);
//   - drag a tile onto an option to slide it there; releasing it back on itself selects it,
//     anywhere else cancels;
//   - tiles in a running move show pressed; a button whose slot changes nudges toward it.
// Positions are UI pixels from the Area's ArtifactDef, drawn `scale`x around (centerX, centerY).
// At 2x the outer frame runs off the 320x240 screen; the buttons and the area art fit.
#pragma once

#include <utility>
#include <vector>

#include "game/data/area.h"
#include "game/grid/artifact.h"
#include "game/grid/grid.h"
#include "game/render/sprite_sheets.h"
#include "game/world/world.h"
#include "platform/platform.h"

class ArtifactUI {
public:
    void init(const Area& area, const Artifact& artifact); // clears selection and animations

    // One frame of stylus input and animation. Moves go through world.artifact.tryMove (the
    // player's tile keeps its walls while it slides). Call before world.update().
    void update(const plat::Input& in, World& world, float dt);
    void draw(const World& world, const SpriteSheets& sheets) const;

    int selected() const { return selectedId_; } // islandId of the selected tile, 0 = none
    bool isOption(int x, int y) const;           // slot highlighted as a move option
    // Screen pixel center of slot (x, y), without nudge (for tests and touch mapping).
    void slotCenter(const Area& area, int x, int y, float& sx, float& sy) const;

    int centerX = plat::BOTTOM_W / 2, centerY = plat::BOTTOM_H / 2;
    int scale = 2; // whole-pixel magnification of the UI art

private:
    struct Nudge {
        float dx = 0, dy = 0, t = 1; // start offset (UI px, y up); t >= 1 = at rest
    };

    bool slotAtPixel(const Area& area, const Artifact& artifact, int px, int py, int& x, int& y) const;
    void selectButton(Grid& grid, Artifact& artifact, int x, int y, bool dragged, int playerStile, bool autoMove);
    void deselect();
    void refreshOptions(const Grid& grid, const Artifact& artifact);
    bool buttonPressed(const Grid& grid, const Artifact& artifact, int islandId) const;

    int selectedId_ = 0;
    int artifactResets_ = -1; // Artifact::resets() last seen
    std::vector<std::pair<int, int>> options_;
    // Stylus state.
    bool wasTouching_ = false, dragging_ = false;
    int downX_ = 0, downY_ = 0;       // touch-down pixel
    int lastX_ = 0, lastY_ = 0;       // last touched pixel (the 3DS reports none on release)
    int downSlotX_ = -1, downSlotY_ = -1; // slot touched down on (-1 = none)
    int hoverX_ = -1, hoverY_ = -1;   // slot under the stylus while dragging
    // Per islandId: last slot seen (to detect moves) and its nudge animation.
    std::vector<std::pair<int, int>> lastSlot_;
    std::vector<Nudge> nudges_;
};
