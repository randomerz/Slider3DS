// SGridAnimator.EffectOnMoveStart / Finish, fed the grid's events: a tile starting to slide shakes
// the camera steadily and rumbles, one landing shakes it once and explodes, both scaled by the move's
// duration (clamped to 0.1-1: the "volume"). The sounds of one frame play as one (LateUpdate's audio
// queue: the loudest volume, placed between the tiles — at the listener if the player rides one),
// always heard as outdoors.
#pragma once

#include <vector>

#include "game/grid/grid.h"

class World;

namespace slide_effects {

void gridEvents(World& world, const std::vector<GridEvent>& events);

} // namespace slide_effects
