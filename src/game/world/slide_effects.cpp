#include "game/world/slide_effects.h"

#include <algorithm>
#include <utility>

#include "game/audio/sounds.h"
#include "game/world/world.h"

namespace slide_effects {

void gridEvents(World& world, const std::vector<GridEvent>& events) {
    for (const auto& [kind, name] : {std::make_pair(GridEvent::Kind::TileMoveStart, sounds::SLIDE_RUMBLE),
                                     std::make_pair(GridEvent::Kind::TileMoveEnd, sounds::SLIDE_EXPLOSION)}) {
        const bool start = kind == GridEvent::Kind::TileMoveStart;
        int n = 0;
        bool riding = false;
        float loudest = 0, sx = 0, sy = 0;
        for (const GridEvent& e : events) {
            if (e.kind != kind) continue;
            const float volume = std::clamp(e.duration, 0.1f, 1.0f);
            if (start) world.effects.shakeConstant(e.duration + 0.1f, 0.15f * volume);
            else world.effects.shake(e.duration / 2, volume);
            n++;
            loudest = std::max(loudest, volume);
            riding = riding || e.islandId == world.player.stile;
            float x, y;
            world.grid.ownerBase(e.islandId, x, y);
            sx += x;
            sy += y;
        }
        if (n == 0) continue;
        if (riding) world.audio.play(name, loudest);
        else world.audio.playAt(name, sx / n, sy / n, loudest, 0, true);
    }
}

} // namespace slide_effects
