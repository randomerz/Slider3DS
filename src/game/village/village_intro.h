// The Village's intro (VillageGrid.OnHouseExit + StartVillageCutscene): the first time the player
// leaves the starting house, the intro camera dolly rolls (game/cutscene/camera_dolly) while Romeo,
// his message ping hidden, looks right, left and right again, walks (his walk 0) and gets his ping
// back 5 s in. Its save key (villageIntroCutscene) is set as it starts.
// Not yet: the dolly's end starting the cat abduction (CatAbductionController).
#pragma once

#include "game/world/wait.h"

class World;

namespace village {

class VillageIntro {
public:
    void reset() { *this = VillageIntro(); }
    void houseExit(World& world); // VillageGrid.OnHouseExit
    // StartVillageCutscene's waits (coroutine slot).
    void coroutines(World& world, float dt);

private:
    int step_ = -1; // the next step after wait_, -1 = not running
    Wait wait_;
};

} // namespace village
