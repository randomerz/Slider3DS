#include "game/events/events.h"

#include <cmath>

#include "game/audio/sounds.h"
#include "game/events/conditions.h"
#include "game/village/knot_box.h"
#include "game/village/village_grid.h"
#include "game/world/world.h"

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
        if (c.target >= 0) world.objectBase(c.target, bx, by);
        else world.grid.ownerBase(t.owner, bx, by);
        teleport(world, bx + t.x + t.dx, by + t.y + t.dy, t.tpToHouse);
        break;
    }
    case M::TeleportTransform: {
        const TeleportDef& t = world.area.teleports[c.data];
        if (t.goTo >= 0) world.objectBase(t.goTo, bx, by);
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
        world.effects.fadeFromBlack(world.frame);
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
        world.npcs.typeCurrent(world, world.area.npcOn(c.target));
        break;
    case M::NpcTeleport:
        world.npcs.teleport(world, world.area.npcOn(c.target), c.objectArg);
        break;
    case M::NpcStartWalk:
        world.walks.start(world, world.area.npcOn(c.target), c.intArg);
        break;
    case M::NpcStartValidWalk:
        world.walks.startValid(world, world.area.npcOn(c.target));
        break;
    case M::SetFlipX:
        world.objects.setFlipX(c.target, c.boolArg);
        break;
    case M::AnimSetBoolTrue:
    case M::AnimSetBoolFalse:
        world.animators.setBool(world, world.area.animatorDrivenBy(c.target), c.stringArg,
                                c.method == M::AnimSetBoolTrue);
        break;
    case M::SetAnimatorController:
        world.animators.setController(world, world.area.animatorOn(c.target), c.data);
        break;
    case M::VillageActivateSlider:
        village::activateCollectible(world, "Slider " + std::to_string(c.intArg), false);
        break;
    case M::VillageActivateCollectible:
        village::activateCollectible(world, c.stringArg, true);
        break;
    case M::VillageTurnFishOn:
        village::turnFishOn(world);
        break;
    case M::VillageExplode:
        village::explode(world);
        break;
    case M::VillageWaterfallEntry:
        village::waterfallEntry(world);
        break;
    case M::VillageHouseExit:
        village::houseExit(world);
        break;
    case M::CatPickedUp:
        world.abduction.pickedUp();
        break;
    case M::VillageFillInHole:
        world.finale.fillInHole(world);
        break;
    case M::VillageShufflePuzzle:
        world.finale.shufflePuzzle(world);
        break;
    case M::SetColliderEnabled:
        world.objects.setColliderEnabled(c.data, c.boolArg);
        break;
    case M::ItemSortingOrderInc:
        world.items.sortingOrderInc(world, c.intArg);
        break;
    case M::ItemSortingOrderDec:
        world.items.sortingOrderDec(world, c.intArg);
        break;
    case M::KnotCheckParticles:
        village::knotCheckParticles(world, c.data);
        break;
    case M::KnotSetToCorrect:
        village::knotSetToCorrectPositions(world, c.data);
        break;
    case M::ChadJumpStarter:
        world.chad.jumpStarter(world, c.data);
        break;
    case M::ChadSetIsOnRock:
        world.chad.setIsOnRock(c.data);
        break;
    // Sounds (AudioManager's names). The props (game/events/sound_props): CowProp.MOO_COOLDOWN /
    // DogProp.BARK_COOLDOWN, and their emotes.
    case M::AudioPlay:
    case M::PlaySoundString:
        if (!c.stringArg.empty()) world.audio.play(c.stringArg);
        break;
    case M::UIClick: world.audio.play(sounds::UI_CLICK); break;
    case M::PlayErrorSound: world.audio.play(sounds::ARTIFACT_ERROR); break;
    case M::VillagePlayDigSound: world.audio.play(sounds::HAT_CLICK); break;
    case M::CowMoo:
    case M::DogBark: {
        const bool cow = c.method == M::CowMoo;
        if (c.target < 0 || !world.props.use(world, c.target, c.data, cow ? 0.1f : 0.2f)) break;
        float x, y;
        world.objectPosition(c.target, x, y);
        // (the dog: one of two barks, by the frame's parity for a random pick, at half volume; its
        // random pitch does nothing: the events have no "Pitch" parameter)
        if (cow) world.audio.playAt(sounds::COW_MOO, x, y);
        else world.audio.playAt(world.frame % 2 ? sounds::DOG_BARK_1 : sounds::DOG_BARK_2, x, y, 0.5f);
        break;
    }
    case M::Unsupported:
        break;
    }
}

void runCalls(World& world, const std::vector<CallDef>& calls) {
    for (const CallDef& c : calls) runCall(world, c);
}
