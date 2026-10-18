#include "game/village/ruins_arrow.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "game/audio/sounds.h"
#include "game/world/world.h"
#include "platform/platform.h"

namespace village {

namespace {

constexpr const char* SLIDER_7 = "Slider 7";

std::vector<int> currentSlots(const Grid& grid) {
    std::vector<int> slots(grid.width() * grid.height());
    for (int y = 0; y < grid.height(); y++)
        for (int x = 0; x < grid.width(); x++) slots[y * grid.width() + x] = grid.idAt(x, y);
    return slots;
}

bool assembled(const std::string& gridString) { return CheckGrid::contains(gridString, "31..62"); } // AreRuinsAssembled

// The pairs of rods each assembled piece of the ruins lights (UpdateMap / UpdateRods), by island.
struct Pair {
    const char* pattern;
    int a, b;
};
constexpr Pair PAIRS[] = {{"31", 1, 3}, {"62", 2, 6}, {"3...6", 3, 6}, {"1...2", 1, 2}};

} // namespace

void RuinsArrows::reset(const Area& area) {
    arrows_.assign(area.ruinsArrows.size(), Arrow());
    for (size_t a = 0; a < area.ruinsArrows.size(); a++) {
        const RuinsArrowDef& d = area.ruinsArrows[a];
        Arrow& s = arrows_[a];
        s.rods.assign(d.rods.size(), Rod());
        for (const WorldSprite& ws : area.worldSprites)
            if (ws.object == d.renderer) {
                s.exportedSprite = ws.place.sprite;
            }
    }
    time_ = 0;
}

bool RuinsArrows::hasSlider7(const World& world) const { return world.state.hasCollectible(SLIDER_7, world.area.name); }

void RuinsArrows::activate(World& world) {
    const Area& area = world.area;
    for (int a = 0; a < (int)arrows_.size(); a++) {
        const RuinsArrowDef& d = area.ruinsArrows[a];
        Arrow& s = arrows_[a];
        // Awake runs as an object first turns on, before anyone's Start.
        for (int r = 0; r < (int)d.rods.size(); r++)
            if (!s.rods[r].awake && world.objects.activeInHierarchy(d.rods[r].object)) {
                s.rods[r].awake = true;
                setRod(world, a, r, false);
            }
        if (!s.started && world.objects.activeInHierarchy(d.object)) {
            s.started = true;
            general(world, a, currentSlots(world.grid), world.grid.gridString());
        }
    }
}

void RuinsArrows::gridEvents(World& world, const std::vector<GridEvent>& events) {
    for (int a = 0; a < (int)arrows_.size(); a++) {
        for (const GridEvent& e : events) {
            if (!world.objects.activeInHierarchy(world.area.ruinsArrows[a].object)) break; // (OnDisable unsubscribed)
            switch (e.kind) {
            case GridEvent::Kind::TileMoveStart: break;
            case GridEvent::Kind::Move: // UpdateArrowOnStart
                setArrowActive(world, a, assembled(e.oldGridString) && assembled(e.gridString), e.oldSlots);
                updateMap(world, a, e.gridString);
                break;
            case GridEvent::Kind::TileMoveEnd: // UpdateArrowOnEnd
                setArrowActive(world, a, assembled(e.gridString), e.slots);
                updateRods(world, a, e.gridString);
                break;
            case GridEvent::Kind::TileEnabled: // UpdateArrowOnCollect
                general(world, a, e.slots, e.gridString);
                if (e.islandId == 7) {
                    arrows_[a].repeating = false; // particleTrail.StopRepeating
                    // Port choice (not in RuinsArrow.cs): the arrow's gone (arrowComplete is null), so its
                    // sparkles stop too; Unity leaves them looping until the ruins come apart.
                    world.particles.stop(world.area.ruinsArrows[a].particles);
                }
                break;
            }
        }
    }
}

void RuinsArrows::general(World& world, int a, const std::vector<int>& slots, const std::string& gridString) {
    setArrowActive(world, a, assembled(gridString), slots);
    updateMap(world, a, gridString);
    updateRods(world, a, gridString);
}

void RuinsArrows::setArrowActive(World& world, int a, bool value, const std::vector<int>& slots) {
    const RuinsArrowDef& d = world.area.ruinsArrows[a];
    Arrow& s = arrows_[a];
    if (value) {
        if (!world.objects.rendererEnabled(d.renderer) && !hasSlider7(world)) { // (it just appeared)
            world.particles.play(world, d.particles);
            world.objects.setActive(world.area, d.buriedSlider, true);
            float x, y;
            world.objectPosition(d.object, x, y);
            world.audio.playAt(sounds::MAGIC_CHIMES_1, x, y);
        }
        world.objects.setRendererEnabled(d.renderer, true);
        updateDirection(world, a, slots);
        if (!hasSlider7(world)) {
            s.repeating = false;
            spawnTrail(world, a);
        }
    } else {
        world.objects.setRendererEnabled(d.renderer, false);
        world.particles.stop(d.particles);
        s.repeating = false;
    }
}

void RuinsArrows::updateDirection(World& world, int a, const std::vector<int>& slots) {
    const RuinsArrowDef& d = world.area.ruinsArrows[a];
    const Arrow& s = arrows_[a];
    ObjectStates& objects = world.objects;
    if (hasSlider7(world)) {
        objects.setSprite(d.renderer, d.complete >= 0 ? d.complete : ObjectStates::NO_SPRITE);
        return;
    }
    // From tile 2 to tile 5 (where Slider 7 is buried), in grid slots.
    int x2 = 0, y2 = 0, x5 = 0, y5 = 0;
    for (int i = 0; i < (int)slots.size(); i++) {
        const int x = i % world.grid.width(), y = i / world.grid.width();
        if (slots[i] == 2) x2 = x, y2 = y;
        else if (slots[i] == 5) x5 = x, y5 = y;
    }
    const int dx = x5 - x2, dy = y5 - y2;
    const int old = objects.sprite(d.renderer) != -1 ? objects.sprite(d.renderer) : s.exportedSprite;
    const bool oldFlip = objects.flipX(d.renderer);
    int k = -1;
    if (dy == 2) k = (dx == 0 || dx == -1) ? 0 : (dx == 1 || dx == -2) ? 1 : -1; // NNE, NE
    else if (dy == 1) k = 2;                                                       // ENE
    else if (dy == 0) k = 3;                                                       // ESE
    else if (dy == -1) k = (dx == 1 || dx == -2) ? 4 : (dx == 0 || dx == -1) ? 5 : -1; // SE, SSE
    const int sprite = k >= 0 ? d.arrows[k] : old;
    const bool flip = dx < 0;
    objects.setSprite(d.renderer, sprite);
    objects.setFlipX(d.renderer, flip);
    if (old != sprite || oldFlip != flip) world.particles.play(world, d.particles);
}

void RuinsArrows::updateMap(World& world, int a, const std::string& gridString) {
    for (const RuinsArrowDef::MapIcon& m : world.area.ruinsArrows[a].mapIcons) {
        bool on = false;
        for (const Pair& p : PAIRS)
            if ((m.corner == p.a || m.corner == p.b) && CheckGrid::contains(gridString, p.pattern)) on = true;
        world.artifactButtons.setIcon(m.button, m.icon, on);
    }
}

void RuinsArrows::updateRods(World& world, int a, const std::string& gridString) {
    const RuinsArrowDef& d = world.area.ruinsArrows[a];
    Arrow& s = arrows_[a];
    // ResetRods, then light each assembled pair's (so a lit rod goes off and on again: its sparkles restart).
    for (int r = 0; r < (int)d.rods.size(); r++) setRod(world, a, r, false);
    s.anyRodsOn = false;
    for (const Pair& p : PAIRS) {
        if (!CheckGrid::contains(gridString, p.pattern)) continue;
        for (int r = 0; r < (int)d.rods.size(); r++)
            if (d.rods[r].island == p.a || d.rods[r].island == p.b) setRod(world, a, r, true);
        s.anyRodsOn = true;
    }
    updateMap(world, a, gridString); // (the same icons, as UpdateRods sets them too)
}

void RuinsArrows::setRod(World& world, int a, int r, bool value) {
    const RuinsRodDef& d = world.area.ruinsArrows[a].rods[r];
    Rod& rod = arrows_[a].rods[r];
    world.objects.setSprite(d.object, value ? d.on : d.off);
    if (value) {
        world.particles.play(world, d.particles);
        // runesFlash.Flash(1) if they were off last frame (a coroutine: only on an active object).
        if (!rod.onLastFrame && world.objects.activeInHierarchy(d.runes)) rod.flash = 0;
    } else {
        world.particles.stop(d.particles);
    }
    world.objects.setRendererEnabled(d.runes, value);
}

void RuinsArrows::spawnTrail(World& world, int a) {
    const RuinsArrowDef::Trail& t = world.area.ruinsArrows[a].trail;
    Arrow& s = arrows_[a];
    float sx, sy, tx, ty, ox, oy;
    world.objectPosition(t.start, sx, sy);
    world.objectPosition(t.target, tx, ty);
    world.objectPosition(t.object, ox, oy);
    const float len = std::sqrt((tx - sx) * (tx - sx) + (ty - sy) * (ty - sy));
    const float ux = len > 0 ? (tx - sx) / len : 0, uy = len > 0 ? (ty - sy) / len : 0;
    const int n = (int)(len / t.distance) + 1;
    s.repeatPoints.clear();
    for (int i = 0; i < n; i++) s.repeatPoints.push_back({sx + i * t.distance * ux - ox, sy + i * t.distance * uy - oy});
    // RepeatTrailSpawns: a run now (its first spawn at once), then one every `repeat` seconds.
    s.repeating = true;
    s.repeatWait = t.repeat;
    startRun(world, a, s.repeatPoints);
}

void RuinsArrows::startRun(World& world, int a, const Points& points) {
    Stagger st;
    st.points = points;
    spawnNext(world, a, st);
    arrows_[a].staggers.push_back(std::move(st));
}

void RuinsArrows::spawnNext(World& world, int a, Stagger& st) {
    const RuinsArrowDef::Trail& t = world.area.ruinsArrows[a].trail;
    const std::pair<float, float>& p = st.points[st.next++];
    world.particles.spawn(world, t.particles, p.first, p.second);
    world.audio.play(sounds::HAT_CLICK, 0.5f); // (ParticleTrail.doAudio: on for the Village's)
    st.wait = t.delay;
}

void RuinsArrows::update(World& world, float dt) {
    const Area& area = world.area;
    time_ += dt;
    for (int a = 0; a < (int)arrows_.size(); a++) {
        const RuinsArrowDef& d = area.ruinsArrows[a];
        Arrow& s = arrows_[a];
        // The trail (coroutines on the trail's object: only while it's active).
        if (world.objects.activeInHierarchy(d.trail.object)) {
            for (Stagger& st : s.staggers) {
                st.wait -= dt;
                if (st.wait <= 0 && st.next < st.points.size()) spawnNext(world, a, st);
            }
            s.staggers.erase(std::remove_if(s.staggers.begin(), s.staggers.end(),
                                            [](const Stagger& st) { return st.next >= st.points.size() && st.wait <= 0; }),
                             s.staggers.end());
            if (s.repeating) {
                s.repeatWait -= dt;
                if (s.repeatWait <= 0) {
                    s.repeatWait += d.trail.repeat;
                    startRun(world, a, s.repeatPoints);
                }
            }
        } else {
            s.staggers.clear();
            s.repeating = false;
        }
        // Runes flash white for flashTime, then show normally for flashTime.
        for (int r = 0; r < (int)d.rods.size(); r++) {
            Rod& rod = s.rods[r];
            if (rod.flash < 0) continue;
            rod.flash += dt;
            if (rod.flash >= 2 * d.rods[r].flashTime) rod.flash = -1;
            world.objects.setFill(d.rods[r].runes, rod.flash >= 0 && rod.flash < d.rods[r].flashTime ? d.flashColor : 0);
        }
        // BobAnimation: down a pixel, back, up a pixel (unless firstUp is -1), back, each cycle.
        if (d.bob && world.objects.activeInHierarchy(d.renderer)) {
            const float t = std::fmod(time_ + d.bobOffset, d.bobLength);
            const int px = d.bobUp >= 0 && t >= d.bobUp ? 1 : t >= d.bobMiddle ? 0 : t >= d.bobDown ? -1 : 0;
            if (px != s.bobPixels) {
                s.bobPixels = px;
                world.objects.setPlacement(area, d.renderer, -1, 0, px / (float)area.ppu);
            }
        }
        // LateUpdate: whether each rod's runes showed this frame.
        for (int r = 0; r < (int)d.rods.size(); r++)
            if (world.objects.activeInHierarchy(d.rods[r].object))
                s.rods[r].onLastFrame = world.objects.rendererEnabled(d.rods[r].runes);
    }
}

} // namespace village
