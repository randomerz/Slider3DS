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
    case M::TeleportHard: {
        const TeleportDef& t = world.area.teleports[c.data];
        teleport(world, t.dx, t.dy, t.tpToHouse);
        break;
    }
    case M::TeleportRelativePlayer: {
        const TeleportDef& t = world.area.teleports[c.data];
        teleport(world, world.player.x + t.dx, world.player.y + t.dy, t.tpToHouse);
        break;
    }
    case M::TeleportRelativeMe: {
        // The changer is the call's target object (it may have moved at runtime).
        const TeleportDef& t = world.area.teleports[c.data];
        if (c.target >= 0) world.objectBase(c.target, t.owner, bx, by);
        else world.grid.ownerBase(t.owner, bx, by);
        teleport(world, bx + t.x + t.dx, by + t.y + t.dy, t.tpToHouse);
        break;
    }
    case M::TeleportTransform: {
        const TeleportDef& t = world.area.teleports[c.data];
        if (t.goTo >= 0) world.objectBase(t.goTo, t.goToOwner, bx, by);
        else world.grid.ownerBase(t.goToOwner, bx, by);
        teleport(world, bx + t.goToX + t.dx, by + t.goToY + t.dy, t.tpToHouse);
        break;
    }
    case M::ActivateSTile:
        world.setTileActive(c.intArg, true);
        break;
    case M::SetFlagOn:
        world.state.setFlag(c.stringArg, true);
        break;
    case M::SetOnWater:
        world.player.onWater = c.boolArg;
        break;
    case M::FadeFromBlack:
        world.effects.fade = 1;
        break;
    case M::CheckConditions:
        runConditionals(world, world.area.conditionCheckers[c.data].conditionals);
        break;
    case M::ChangeScenes:
        world.effects.leftArea = true;
        break;
    case M::NpcTriggerEnter:
        world.npcs.triggerEnter(world, world.area.npcOn(c.target));
        break;
    case M::NpcTriggerExit:
        world.npcs.triggerExit(world, world.area.npcOn(c.target));
        break;
    case M::NpcTypeCurrent:
        world.npcs.typeCurrentLine(world, world.area.npcOn(c.target));
        break;
    case M::NpcTeleport:
        Npcs::teleport(world, world.area.npcOn(c.target), world.area.transformArgs[c.data]);
        break;
    case M::SetFlipX:
        world.objects.setFlipX(c.target, c.boolArg);
        break;
    case M::Unsupported:
        break;
    }
}

void runCalls(World& world, const std::vector<CallDef>& calls) {
    for (const CallDef& c : calls) runCall(world, c);
}
