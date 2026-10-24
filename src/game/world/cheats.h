// CheatsControlPanel: the pause menu's Cheats page (game/ui/pause_menu) changes this state in place,
// the Frontend owns it and acts on it. Every cheat needs Settings::cheats (a port addition: the
// original's panel is always there). Using one marks the save profile (UsedCheats).
#pragma once

#include <string>
#include <vector>

class World;

struct Cheats {
    std::vector<std::string> collectibles; // Give's list (SGrid.GetCollectibles): the area's, by index
    std::vector<int> toGive; // the indices marked on the page, given when the menu closes (CheckCheatsOnPauseClosed)
    bool noclip = false;     // DoNoClip: the player's colliders off (Player::noClip; until the next game starts)
    bool respawn = false;    // DoRespawnPlayer was pressed: the Frontend runs it and clears this
};

namespace cheats {

constexpr const char* USED_CHEATS_FLAG = "UsedCheats"; // SetCheated's save key

void markUsed(World& world);
// SGrid.GivePlayerTheCollectible: the collectible appears (unless it's already in the inventory)
// at the player's feet, unparented, so the player picks it up.
void giveCollectible(World& world, int index);
// DoRespawnPlayer: back to the scene's spawn (the Default one: no scene changes lead here yet).
void respawnPlayer(World& world);

} // namespace cheats
