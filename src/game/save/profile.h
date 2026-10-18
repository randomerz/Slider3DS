// A save profile (Unity's SaveProfile, one of SaveSystem's three slots): its name, play time, the
// game state (flags, strings, collectibles), the grid (SGridData) and the player (SerializablePlayer),
// kept as a text file of its own. World::save fills one and World::load starts the scene from it.
// Not saved, as in Unity's Village: items' positions (the carried one goes back to where it started;
// returned cats come back on their spawn spots: CatTricks::load), anything mid-way (walks, cutscenes).
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "game/world/game_state.h"

struct SaveProfile {
    static constexpr int SLOTS = 3; // SaveSystem's profiles
    static std::string fileName(int slot); // its user file (plat::readUserFile)

    std::string name; // the profile's (also the cat's: GameState::newProfile)
    float playTime = 0; // seconds played (PlayTimer: while the game runs, not paused)
    GameState state;
    // The grid: tile ids in Grid::setGrid's format and the collected tiles. An empty grid has never
    // been saved (SGridData.grid null): tiles as authored.
    std::string grid;
    std::vector<int> activeTiles;
    // The player, unless never saved (then it starts at the new game's spawn): where it stands (on a
    // sliding tile: relative to where that tile ends up: Player.GetSavePosition), in the boat, in a house.
    bool hasPlayer = false;
    float playerX = 0, playerY = 0;
    bool onWater = false, inHouse = false;

    // A new profile named `profileName` (SaveProfile's constructor).
    static SaveProfile make(const std::string& profileName);

    // One line per entry: "<kind>\t<fields...>" (tabs, newlines and backslashes escaped), after a
    // header with the format's version. parse fails on a missing header or a malformed line and
    // skips kinds it doesn't know (a later version's).
    std::string serialize() const;
    static bool parse(std::string_view text, SaveProfile& out);

    enum class Read { Ok, None, Bad }; // Bad: there's a file, but it doesn't parse
    static Read load(int slot, SaveProfile& out);
    bool save(int slot) const; // true on success (the old file stays on failure)
};
