// The running game in one area: the loaded (read-only) Area plus every piece of runtime state,
// and the per-frame simulation. Systems take the World rather than a growing list of its parts:
// physics and AreaView read it, ArtifactUI and the update change it.
#pragma once

#include "game/area.h"
#include "game/artifact.h"
#include "game/conditions.h"
#include "game/grid.h"
#include "game/object_states.h"

// The player (Player.cs): a position point, the box that collides (on foot or in the boat), and
// the STile it stands on, which it rides while that tile slides.
struct Player {
    float x = 0, y = 0;
    bool onWater = false;
    bool facingLeft = false; // Player.Update flips the sprite by the input's x
    int stile = 0;           // islandId underneath, 0 = none

    const Hitbox& hitbox(const Area& area) const { return onWater ? area.player.boatBox : area.player.footBox; }
};

class World {
public:
    static constexpr float MOVE_SPEED = 5.0f; // world units per second (Player.moveSpeed)

    // A new game in `area`: tiles as authored, no flags or collectibles, start checks run. The
    // player is at the origin until placed.
    explicit World(const Area& area);

    // Scene objects as a fresh load with the current state would have them: authored, then the
    // start checks run again (e.g. after changing the state in a dev control).
    void reloadObjects();
    // Puts the player at a spawn point (SceneSpawns), in the boat if it says so.
    void placeAt(const Spawn& spawn);
    // Puts the player at (x, y) on foot (or the nearest spot where it fits, if any).
    void placeNear(float x, float y);

    // One frame: queued slides advance and the player rides the tile under it; start checks run
    // for checkers that became active; then the player walks along (moveX, moveY) (each -1..1,
    // normalized here) at MOVE_SPEED, colliding unless noclip.
    void update(float dt, float moveX, float moveY, bool noclip);

    const Area& area;
    Grid grid;
    Artifact artifact;
    ObjectStates objects;
    StartChecks startChecks;
    GameState state;
    Player player;
};
