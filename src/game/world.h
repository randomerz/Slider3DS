// The running game in one area: the loaded (read-only) Area plus every piece of runtime state,
// and the per-frame simulation. Systems take the World rather than a growing list of its parts:
// physics and AreaView read it, ArtifactUI, events and the update change it.
#pragma once

#include <cstdint>
#include <vector>

#include "game/area.h"
#include "game/artifact.h"
#include "game/game_state.h"
#include "game/grid.h"
#include "game/object_states.h"
#include "game/starts.h"
#include "game/triggers.h"

// The player (Player.cs): a position point, the box that collides (on foot or in the boat), and
// the STile it stands on, which it rides while that tile slides.
struct Player {
    float x = 0, y = 0;
    bool onWater = false;
    bool facingLeft = false; // Player.Update flips the sprite by the input's x
    bool inHouse = false;    // Player.SetIsInHouse (house teleports set it)
    int stile = 0;           // islandId underneath, 0 = none

    const Hitbox& hitbox(const Area& area) const { return onWater ? area.player.boatBox : area.player.footBox; }
};

// Something the action button can use (Unity's IInteractable in PlayerAction.availableInteractables).
// Only PlayerConditionals so far; NPCs and items join as more kinds. Its priority and prompt are
// asked for when needed (World::priority / showsPrompt): an NPC's change with its dialogue.
struct Interactable {
    enum class Kind : uint8_t { Interaction }; // Interaction: Area::interactions[index]
    Kind kind;
    int index;

    bool operator==(const Interactable& o) const { return kind == o.kind && index == o.index; }
};

// Screen-level effects events ask for (UIEffects, SceneChanger), for the frontend to show.
struct ScreenEffects {
    float fade = 0;        // FadeFromBlack: black overlay alpha, falls to 0 over area.fadeDuration
    bool leftArea = false; // SceneChanger.ChangeScenes ran (the port ends at the Village)
};

class World {
public:
    static constexpr float MOVE_SPEED = 5.0f; // world units per second (Player.moveSpeed)

    // A new game in `area`: tiles as authored, no flags or collectibles, Starts run. The player is
    // at the origin until placed.
    explicit World(const Area& area);

    // Scene objects as a fresh load with the current state would have them (e.g. after changing
    // the state in a dev control): authored, uncollected tiles' objects off (STile.Awake), then
    // SGrid.Start (collected collectibles hidden), then every component's Start. Trigger contacts
    // reset too, so the next update re-enters the triggers the player stands in, as after a load.
    void reloadObjects();
    // STile.SetTileActive: the tile joins (or leaves) the grid, with its objects and tilemaps (whose
    // components then Start).
    void setTileActive(int islandId, bool active);
    // Puts the player at a spawn point (SceneSpawns), in the boat if it says so.
    void placeAt(const Spawn& spawn);
    // Puts the player at (x, y) on foot (or the nearest spot where it fits, if any).
    void placeNear(float x, float y);

    // One frame: queued slides advance and the player rides the tile under it; the player walks
    // along (moveX, moveY) (each -1..1, normalized here) at MOVE_SPEED, colliding unless noclip
    // (and is pushed out of anything it overlaps); triggers react to what it touches; then Starts
    // run for components that became active.
    void update(float dt, float moveX, float moveY, bool noclip);

    // PlayerAction: offer / withdraw an interactable, and the action button: runs the offered
    // interactables with the highest priority.
    void offer(const Interactable& it);
    void withdraw(const Interactable& it);
    void interact();
    int priority(const Interactable& it) const;     // IInteractable.InteractionPriority
    bool showsPrompt(const Interactable& it) const; // IInteractable.DisplayInteractionPrompt
    bool promptShown() const; // any offered interactable shows the prompt (the indicator)

    // World position a component of `object` is placed relative to: its owner STile's center (the
    // object's runtime owner, else `exportedOwner`) plus the object's runtime offset.
    void objectBase(int object, int exportedOwner, float& x, float& y) const;

    const Area& area;
    Grid grid;
    Artifact artifact;
    ObjectStates objects;
    Starts starts;
    TriggerTracker triggers;
    GameState state;
    Player player;
    std::vector<Interactable> interactables; // on offer now
    ScreenEffects effects;
};
