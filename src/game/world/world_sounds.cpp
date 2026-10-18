#include "game/world/world_sounds.h"

#include <cmath>
#include <limits>

#include "game/world/world.h"

namespace {

constexpr float PARAMETER_STEP = 0.01f; // (a change smaller than this isn't sent)

} // namespace

const std::string& WorldSounds::surfaceAt(const World& world, float x, float y) {
    // Player.GetCurrentMaterialTilemap: the STile underneath's (its house's while inside), else the world's.
    const Area& area = world.area;
    const int owner = world.player.stile;
    const bool house = owner != 0 && world.player.inHouse;
    for (const MaterialLayer& m : area.materials) {
        if (m.owner != owner || m.house != house) continue;
        float bx, by;
        world.grid.ownerBase(owner, bx, by);
        const int cx = (int)std::floor(x - (bx + m.originX + m.minX)), cy = (int)std::floor(y - (by + m.originY + m.minY));
        if (cx < 0 || cy < 0 || cx >= m.w || cy >= m.h) break;
        const int k = m.cell(cx, cy);
        if (k >= 0) return area.footsteps[k];
        break;
    }
    return area.footstepFallback;
}

void WorldSounds::start(World& world) {
    const Area& area = world.area;
    sent_.assign(area.ambiences.size(), -1);
    started_ = true;
    for (const Area::AmbienceDef& a : area.ambiences)
        if (world.objects.activeInHierarchy(a.object)) world.audio.playAmbience(a.name);
    sendParameters(world);
}

void WorldSounds::update(World& world) {
    for (const Animators::Fired& f : world.animators.fired())
        if (f.animator == world.area.player.animator && *f.function == "Step" && !world.playerLocked())
            world.audio.playAt("Footstep " + surfaceAt(world, world.player.x, world.player.y), world.player.x, world.player.y);
    if (started_) sendParameters(world);
}

void WorldSounds::sendParameters(World& world) {
    const Area& area = world.area;
    for (size_t i = 0; i < area.ambiences.size(); i++) {
        const Area::AmbienceDef& a = area.ambiences[i];
        if (!a.enabled || !world.objects.activeInHierarchy(a.object)) continue;
        // The nearest active node among every ambience of this name (nameToMinDist is shared).
        float nearest = std::numeric_limits<float>::infinity();
        for (const Area::AmbienceDef& b : area.ambiences) {
            if (b.name != a.name || !b.enabled) continue;
            for (int node : b.nodes) {
                if (!world.objects.activeInHierarchy(node)) continue;
                float nx, ny;
                world.objectPosition(node, nx, ny);
                nearest = std::fmin(nearest, std::hypot(nx - world.player.x, ny - world.player.y));
            }
        }
        if (std::isinf(nearest) || (sent_[i] >= 0 && std::fabs(nearest - sent_[i]) < PARAMETER_STEP)) continue;
        sent_[i] = nearest;
        world.audio.setParameter(a.parameter, nearest);
    }
}

void WorldSounds::end(World& world) {
    if (!started_) return;
    for (const Area::AmbienceDef& a : world.area.ambiences) {
        world.audio.stopAmbience(a.name);
        world.audio.setParameter(a.parameter, a.defaultValue);
    }
    started_ = false;
}
