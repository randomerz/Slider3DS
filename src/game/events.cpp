#include "game/events.h"

#include "game/conditions.h"
#include "game/world.h"

namespace {

// Player.SetPosition + SetIsInHouse.
void teleport(World& world, float x, float y, bool inHouse) {
    world.player.x = x;
    world.player.y = y;
    world.player.inHouse = inHouse;
    world.player.stile = world.grid.stileUnder(x, y);
}

} // namespace

void runCall(World& world, const CallDef& c) {
    using M = CallDef::Method;
    float bx = 0, by = 0;
    switch (c.method) {
    case M::SetActive:
        world.objects.setActive(world.area, c.target, c.boolArg);
        break;
    case M::TeleportHard:
        teleport(world, c.dx, c.dy, c.tpToHouse);
        break;
    case M::TeleportRelativePlayer:
        teleport(world, world.player.x + c.dx, world.player.y + c.dy, c.tpToHouse);
        break;
    case M::TeleportRelativeMe:
        world.grid.ownerBase(c.owner, bx, by);
        teleport(world, bx + c.x + c.dx, by + c.y + c.dy, c.tpToHouse);
        break;
    case M::TeleportTransform:
        world.grid.ownerBase(c.goToOwner, bx, by);
        teleport(world, bx + c.goToX + c.dx, by + c.goToY + c.dy, c.tpToHouse);
        break;
    case M::ActivateSTile:
        world.grid.setTileActive(c.intArg, true);
        break;
    case M::SetFlagOn:
        world.state.setFlag(c.stringArg, true);
        break;
    case M::SetOnWater:
        world.player.onWater = c.boolArg;
        break;
    case M::FadeFromBlack:
        world.fade = 1;
        break;
    case M::CheckConditions:
        runConditionals(world, world.area.conditionCheckers[c.checker].conditionals);
        break;
    case M::ChangeScenes:
        world.leftArea = true;
        break;
    case M::Unsupported:
        break;
    }
}

void runCalls(World& world, const std::vector<CallDef>& calls) {
    for (const CallDef& c : calls) runCall(world, c);
}
