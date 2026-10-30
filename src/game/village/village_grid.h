// VillageGrid (the Village's SGrid): the methods its UnityEvent calls and spec conditions use.
// The knot box is in game/village/knot_box, the ruins arrow in game/village/ruins_arrow, Chad in
// game/village/chad_jump, FillInHole / ShufflePuzzle and the final 8-puzzle in game/village/final_puzzle.
// The intro cutscene is game/village/village_intro, the cats game/village/cat_trick, the fish
// game/village/village_fish.
#pragma once

#include <string>

#include "game/data/area.h"

class World;

namespace village {

constexpr const char* FISH_ON_FLAG = "villageFishOn";           // VillageGrid.fishOn (saved as this)
constexpr const char* INTRO_CUTSCENE_FLAG = "villageIntroCutscene";
constexpr const char* CAVE_DOOR_EXPLODED_FLAG = "caveDoorExploded";
constexpr const char* VILLAGE_COMPLETION_FLAG = "villageCompletion"; // VillageGrid.checkCompletion (saved as this)
constexpr const char* AUTO_MOVE_FLAG = "forceAutoMoveVillage";        // UIArtifact.GetAutoMoveActive
constexpr const char* PENDING_COMPLETE_FLAG = "villagePendingComplete";
constexpr const char* FINAL_SLIDER = "Slider 9"; // PlaceTile9 hands it out: the Village is complete

// SGrid.ActivateCollectible / ActivateSliderCollectible: the area's collectible `name` appears
// (SpawnCollectable: its object turns on) unless it's already in the inventory. `clearDisableAtStart`
// (ActivateCollectible) also stops its Start from hiding it later.
void activateCollectible(World& world, const std::string& name, bool clearDisableAtStart);
// TurnFishOn: the fish come back (game/village/village_fish; Pierre's CheckFishOn spec).
void turnFishOn(World& world);
// Explode: the cave door opens (entrance on, rocks off) for good.
void explode(World& world);
// OnWaterfallEntry: with Slider 9, the cave door counts as exploded.
void waterfallEntry(World& world);
// OnHouseExit: the first time, the intro cutscene plays (VillageIntro).
void houseExit(World& world);
// VillageGrid.Start's Artifact pin on the Slider 2 collectible, while it isn't collected
// (RemoveSlider2Tracker drops it as it's collected).
void addSlider2Tracker(World& world);

// The spec conditions (Condition.checkBool) on VillageGrid & co., and what they read.
bool checkSpec(const World& world, const ConditionDef& c);
bool specReadsOnlyStateAndGrid(ConditionDef::Spec spec);

} // namespace village
