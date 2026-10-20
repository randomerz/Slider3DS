#include "game/ui/artifact_ui.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {
constexpr int DRAG_THRESHOLD = 3; // px of stylus travel before a press becomes a drag (Unity's drag threshold)
constexpr int IMAGE_HALF = 16;    // button images are 32x32, centered on the slot
}

void ArtifactUI::init(const Area& area, const Artifact& artifact) {
    selectedId_ = 0;
    options_.clear();
    artifactResets_ = artifact.resets();
    autoMoveFlag_ = "forceAutoMove" + area.name;
    wasTouching_ = dragging_ = false;
    downSlotX_ = downSlotY_ = hoverX_ = hoverY_ = -1;
    int maxId = 0;
    for (const STileDef& s : area.stiles) maxId = std::max(maxId, s.islandId);
    lastSlot_.assign(maxId + 1, {-1, -1});
    nudges_.assign(maxId + 1, Nudge{});
    for (const STileDef& s : area.stiles) artifact.findButton(s.islandId, lastSlot_[s.islandId].first, lastSlot_[s.islandId].second);
}

void ArtifactUI::slotCenter(const Area& area, int x, int y, float& sx, float& sy) const {
    const int sp = area.artifact.buttonSpacing * scale;
    sx = (float)(centerX + (x - 1) * sp);
    sy = (float)(centerY - (y - 1) * sp);
}

bool ArtifactUI::slotAtPixel(const Area& area, const Artifact& artifact, int px, int py, int& x, int& y) const {
    // Each slot owns the spacing-sized square around its center (a little more generous than the
    // 32 px images, which suits a stylus).
    const float half = area.artifact.buttonSpacing * scale * 0.5f;
    for (y = 0; y < artifact.height(); y++)
        for (x = 0; x < artifact.width(); x++) {
            float cx, cy;
            slotCenter(area, x, y, cx, cy);
            if (std::fabs(px - cx) <= half && std::fabs(py - cy) <= half) return true;
        }
    x = y = -1;
    return false;
}

bool ArtifactUI::isOption(int x, int y) const {
    return std::find(options_.begin(), options_.end(), std::make_pair(x, y)) != options_.end();
}

void ArtifactUI::releaseStylus() {
    wasTouching_ = true; // (no stylus-down until a release; a release with no down slot does nothing)
    dragging_ = false;
    downSlotX_ = downSlotY_ = hoverX_ = hoverY_ = -1;
}

void ArtifactUI::deselect() {
    selectedId_ = 0;
    options_.clear();
}

void ArtifactUI::refreshOptions(const Grid& grid, const Artifact& artifact) {
    options_.clear();
    int sx, sy;
    if (selectedId_ && artifact.findButton(selectedId_, sx, sy)) options_ = artifact.moveOptions(grid, sx, sy);
}

// UIArtifact.SelectButton.
void ArtifactUI::selectButton(Grid& grid, Artifact& artifact, int x, int y, bool dragged, int playerStile, bool autoMove) {
    if (!artifact.canAddMoves) return;
    const int id = artifact.buttonAt(x, y);
    if (selectedId_) {
        if (isOption(x, y)) {
            int sx, sy;
            artifact.findButton(selectedId_, sx, sy);
            artifact.tryMove(grid, sx, sy, x, y, playerStile); // the moved tile stays selected
            refreshOptions(grid, artifact);
        } else {
            bool same = id == selectedId_;
            deselect();
            if (same) return;
        }
    }
    if (!selectedId_) {
        if (!grid.tileActive(id)) return;
        auto options = artifact.moveOptions(grid, x, y);
        if (options.empty()) return;
        selectedId_ = id;
        if (autoMove && options.size() == 1 && !dragged) {
            artifact.tryMove(grid, x, y, options[0].first, options[0].second, playerStile);
            deselect();
        } else {
            options_ = options;
        }
    }
}

void ArtifactUI::update(const plat::Input& in, World& world, float dt) {
    clicked_ = false;
    if (nudges_.empty()) return; // (before init: no buttons yet)
    const Area& area = world.area;
    Grid& grid = world.grid;
    Artifact& artifact = world.artifact;
    const int playerStile = world.player.stile;
    if (!area.artifact.present) return;
    if (artifact.resets() != artifactResets_) { // SetGrid: DeselectSelectedButton
        artifactResets_ = artifact.resets();
        deselect();
    }
    // UIArtifact.GetAutoMoveActive: the save's "forceAutoMove<Area>" (the final puzzle sets it); or,
    // a port feature, the player's autoMove setting.
    auto autoMove = [&] { return world.settings.autoMove || world.state.flag(autoMoveFlag_); };
    const bool touching = in.touching;
    int sx = -1, sy = -1;
    const bool onSlot = touching && slotAtPixel(area, artifact, in.touchX, in.touchY, sx, sy);

    if (touching) {
        lastX_ = in.touchX;
        lastY_ = in.touchY;
    }
    if (touching && !wasTouching_) { // stylus down
        downX_ = in.touchX;
        downY_ = in.touchY;
        dragging_ = false;
        downSlotX_ = onSlot ? sx : -1;
        downSlotY_ = onSlot ? sy : -1;
    } else if (touching && !dragging_ && downSlotX_ >= 0 &&
               std::abs(in.touchX - downX_) + std::abs(in.touchY - downY_) > DRAG_THRESHOLD) {
        // ButtonDragged: a drag on a collected tile selects it (options highlight).
        dragging_ = true;
        deselect();
        if (grid.tileActive(artifact.buttonAt(downSlotX_, downSlotY_)))
            selectButton(grid, artifact, downSlotX_, downSlotY_, true, playerStile, autoMove());
    }
    hoverX_ = dragging_ && onSlot ? sx : -1;
    hoverY_ = dragging_ && onSlot ? sy : -1;

    if (!touching && wasTouching_ && downSlotX_ >= 0) { // stylus up
        int ux = -1, uy = -1;
        bool upOnSlot = slotAtPixel(area, artifact, lastX_, lastY_, ux, uy);
        if (!dragging_) {
            if (upOnSlot && ux == downSlotX_ && uy == downSlotY_) {
                clicked_ = true;
                selectButton(grid, artifact, ux, uy, false, playerStile, autoMove());
            }
        } else if (grid.tileActive(artifact.buttonAt(downSlotX_, downSlotY_)) && selectedId_) {
            // ButtonDragEnd / DoButtonDrag: drop on an option moves there; anywhere else but the
            // dragged tile cancels. Back on the dragged tile, Unity's Button also gets the click
            // (EventTrigger and Button share the object), before EndDrag: the click deselects, then
            // ButtonDragEnd's SelectButton selects it again like a tap — auto-moving a single option.
            const bool onItself = upOnSlot && ux == downSlotX_ && uy == downSlotY_;
            if (onItself) {
                clicked_ = true;
                deselect();
                selectButton(grid, artifact, ux, uy, false, playerStile, autoMove());
            } else {
                if (upOnSlot && isOption(ux, uy)) selectButton(grid, artifact, ux, uy, true, playerStile, autoMove());
                deselect();
            }
        }
        dragging_ = false;
        downSlotX_ = downSlotY_ = hoverX_ = hoverY_ = -1;
    }
    wasTouching_ = touching;

    // Keep the highlighted options current (UpdateMoveOptions runs whenever the board changes).
    if (selectedId_) refreshOptions(grid, artifact);

    // Nudge buttons whose slot changed (ArtifactTileButton.SetPosition with animateChange).
    for (const STileDef& s : area.stiles) {
        int bx, by;
        if (!artifact.findButton(s.islandId, bx, by)) continue;
        auto& last = lastSlot_[s.islandId];
        Nudge& n = nudges_[s.islandId];
        if ((bx != last.first || by != last.second) && last.first >= 0 && grid.tileActive(s.islandId)) {
            float dx = (float)(bx - last.first), dy = (float)(by - last.second);
            float len = std::sqrt(dx * dx + dy * dy);
            n.dx = -dx / len * area.artifact.nudgeDistance;
            n.dy = -dy / len * area.artifact.nudgeDistance;
            n.t = 0;
        }
        last = {bx, by};
        if (n.t < 1) n.t = std::min(1.0f, n.t + dt / area.artifact.nudgeDuration);
    }
}

bool ArtifactUI::buttonPressed(const Grid& grid, const Artifact& artifact, int islandId, int selected) const {
    // Selected, or forced down while its tile is in a running move (UpdatePushedDowns).
    return islandId == selected || (grid.tileActive(islandId) && artifact.tileInActiveMove(islandId));
}

void ArtifactUI::draw(const World& world, const SpriteSheets& sheets) const {
    if (nudges_.empty()) return; // (before init: no buttons yet)
    const Area& area = world.area;
    const Grid& grid = world.grid;
    const Artifact& artifact = world.artifact;
    const ArtifactDef& def = area.artifact;
    if (!def.present) return;
    // UI rect (min corner + size, y up, UI px) relative to a screen-pixel origin -> screen top-left.
    auto drawPart = [&](int sprite, int x, int y, int h, float originX, float originY) {
        sheets.draw(area, sprite, originX + x * scale, originY - (y + h) * scale, 0, scale);
    };
    // A setGrid since update() (World::update runs after it): the selection is already gone.
    const bool stale = artifact.resets() != artifactResets_;
    const int selected = stale ? 0 : selectedId_;
    drawPart(def.frame.sprite, def.frame.x, def.frame.y, def.frame.h, (float)centerX, (float)centerY);
    drawPart(def.background.sprite, def.background.x, def.background.y, def.background.h, (float)centerX,
             (float)centerY);
    for (const UiPart& u : def.underlays) drawPart(u.sprite, u.x, u.y, u.h, (float)centerX, (float)centerY);

    // A button's screen center this frame (its slot, its nudge toward it, pressed: dropped); its
    // image (and the pins on it) sit at (cx, cy + drop).
    auto buttonCenter = [&](int islandId, int bx, int by, float& cx, float& cy, float& drop) {
        slotCenter(area, bx, by, cx, cy);
        const Nudge& n = nudges_[islandId];
        if (n.t < 1) { // ease out: the curve starts with slope 2
            float k = 1 - (1 - n.t) * (1 - n.t);
            cx += n.dx * (1 - k) * scale;
            cy -= n.dy * (1 - k) * scale;
        }
        drop = buttonPressed(grid, artifact, islandId, selected) ? (float)(def.pressedDrop * scale) : 0.0f;
    };
    // The pins (UITrackerManager.UpdateTrackerPostion: game/grid/ui_trackers), each drawn at its
    // place's whole UI px from an origin on the screen's 2x grid.
    std::vector<PinPlace> pins;
    for (const UiTracker& t : world.trackers.trackers())
        if (const PinPlace pin = world.trackers.place(world, t, centerY / (float)scale); pin.sprite >= 0) pins.push_back(pin);
    auto drawPin = [&](const PinPlace& pin, float originX, float originY) {
        sheets.draw(area, pin.sprite, std::round(originX) + pin.left * scale, std::round(originY) - pin.top * scale, 0, scale);
    };
    for (const ArtifactButtonDef& b : def.buttons) { // Button1..9 sibling order
        int bx, by;
        if (!artifact.findButton(b.islandId, bx, by)) continue;
        float cx, cy, drop;
        buttonCenter(b.islandId, bx, by, cx, cy, drop);
        const bool active = grid.tileActive(b.islandId);
        const bool pressed = buttonPressed(grid, artifact, b.islandId, selected);
        const bool option = !stale && isOption(bx, by);
        const bool hovered = bx == hoverX_ && by == hoverY_ && option;

        const bool complete = world.artifactButtons.complete(b.islandId) && b.completed >= 0;
        const int image = hovered ? def.hoverIcon : !active ? def.empty : complete ? b.completed : b.island;
        drawPart(image, -IMAGE_HALF, -IMAGE_HALF, 2 * IMAGE_HALF, cx, cy + drop);
        if (active)
            for (int i = 0; i < (int)b.icons.size(); i++)
                if (world.artifactButtons.iconShown(b.islandId, i))
                    drawPart(b.icons[i].sprite, b.icons[i].x, b.icons[i].y, b.icons[i].h, cx, cy + drop);
        // Its pins: children of its Image, under its highlight and pressed frames (ButtonSprites:
        // Image, Border, Shadow), moving with the image.
        for (const PinPlace& pin : pins)
            if (pin.island == b.islandId) drawPin(pin, cx, cy + drop);
        if (option && !hovered)
            drawPart(def.highlight.sprite, def.highlight.x, def.highlight.y, def.highlight.h, cx, cy + drop);
        if (pressed) drawPart(def.pressed.sprite, def.pressed.x, def.pressed.y, def.pressed.h, cx, cy);
    }

    // Pins off the grid: children of the panel, after the buttons.
    for (const PinPlace& pin : pins)
        if (pin.island == 0) drawPin(pin, (float)centerX, (float)centerY);
}
