// CatTrick (Cathy's cats, Village): each frame, a cat item whose collider overlaps the detection
// zone counts as returned for good. Its collider is on unless it's carried, so a cat counts once
// it's dropped in the zone. The returned cats are the save's flags (villageHasMittens / Autumn /
// Tofu), which the specs CheckMittens / CheckAutumn / CheckTofu read and Cathy's conditionals
// test. While 1 or 2 are back, UpdateText shows "n/3" (its text objects on; AreaView draws it).
// Not yet: Load's moving returned cats to their spawn spots (with the save profile).
#pragma once

#include "game/data/area.h"

class World;

namespace village {

// The save keys, in CatTrickDef::cats order (mittens, autumn, tofu).
inline constexpr const char* CAT_FLAGS[CatTrickDef::CATS] = {"villageHasMittens", "villageHasAutumn", "villageHasTofu"};

class CatTricks {
public:
    // CatTrick.Update: UpdateCollection, then UpdateText. In the scripts' Update.
    void update(World& world);
    // How many cats are back (the flags), and the "n/3" text UpdateText shows.
    static int count(const World& world);
    static const char* text(int count);
};

} // namespace village
