// VillageGrid (the Village's SGrid): the methods its UnityEvent calls and spec conditions use.
// The knot box is in game/village/knot_box. Not yet: ruins, Chad's jump, cats (their specs), FillInHole /
// ShufflePuzzle and the final 8-puzzle, the intro cutscene, trackers, particles and sounds.
#pragma once

#include <string>

#include "game/data/area.h"

class World;

namespace village {

constexpr const char* FISH_ON_FLAG = "villageFishOn";           // VillageGrid.fishOn (saved as this)
constexpr const char* INTRO_CUTSCENE_FLAG = "villageIntroCutscene";
constexpr const char* CAVE_DOOR_EXPLODED_FLAG = "caveDoorExploded";

// SGrid.ActivateCollectible / ActivateSliderCollectible: the area's collectible `name` appears
// (SpawnCollectable: its object turns on) unless it's already in the inventory. `clearDisableAtStart`
// (ActivateCollectible) also stops its Start from hiding it later.
void activateCollectible(World& world, const std::string& name, bool clearDisableAtStart);
// TurnFishOn: the fish come back (Pierre's CheckFishOn spec).
void turnFishOn(World& world);
// Explode: the cave door opens (entrance on, rocks off) for good.
void explode(World& world);
// OnWaterfallEntry: with Slider 9, the cave door counts as exploded.
void waterfallEntry(World& world);
// OnHouseExit: the first time, the intro cutscene would play (not yet: only its flag).
void houseExit(World& world);

// The spec conditions (Condition.checkBool) on VillageGrid & co., and what they read.
bool checkSpec(const World& world, const ConditionDef& c);
bool specReadsOnlyStateAndGrid(ConditionDef::Spec spec);

} // namespace village
