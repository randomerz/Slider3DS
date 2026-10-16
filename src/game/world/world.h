// The running game in one area: the loaded (read-only) Area plus every piece of runtime state,
// and the per-frame simulation. Systems take the World rather than a growing list of its parts:
// physics and AreaView read it, ArtifactUI, events and the update change it.
#pragma once

#include <cstdint>
#include <vector>

#include "game/anim/animators.h"
#include "game/data/area.h"
#include "game/fx/particles.h"
#include "game/grid/artifact.h"
#include "game/world/game_state.h"
#include "game/grid/grid.h"
#include "game/items/items.h"
#include "game/npc/npcs.h"
#include "game/world/object_states.h"
#include "game/world/starts.h"
#include "game/events/triggers.h"
#include "game/village/knot_box.h"
#include "game/village/chad_jump.h"
#include "game/village/ruins_arrow.h"

// The player (Player.cs): a position point, the box that collides (on foot or in the boat), and
// the STile it stands on, which it rides while that tile slides.
struct Player {
    float x = 0, y = 0;
    bool onWater = false;
    bool facingLeft = false; // Player.Update flips the sprite by the input's x
    float lastMoveX = 0, lastMoveY = 0; // Player.lastMoveDir: the last non-zero input (item drops go that way)
    bool inHouse = false;    // Player.SetIsInHouse (house teleports set it)
    int stile = 0;           // islandId underneath, 0 = none

    const Hitbox& hitbox(const Area& area) const { return onWater ? area.player.boatBox : area.player.footBox; }
};

// Something the action button can use (Unity's IInteractable in PlayerAction.availableInteractables):
// PlayerConditionals and NPCs so far; items join later. Its priority and prompt are asked for when
// needed (World::priority / showsPrompt): an NPC's change with its dialogue.
struct Interactable {
    enum class Kind : uint8_t { Interaction, Npc }; // Area::interactions / Area::npcs [index]
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
    // (and is pushed out of anything it overlaps); triggers react to what it touches; NPCs update;
    // Starts run for components that became active; Animators and particles advance.
    void update(float dt, float moveX, float moveY, bool noclip);

    // PlayerAction.Action (the action button): pick up an item if one is in reach, use the offered
    // interactables; if neither did anything, drop the carried item.
    void action();
    // PlayerAction: offer / withdraw an interactable, and run the offered ones with the highest
    // priority; true if one did something (IInteractable.Interact).
    void offer(const Interactable& it);
    void withdraw(const Interactable& it);
    bool interact();
    int priority(const Interactable& it) const;     // IInteractable.InteractionPriority
    bool showsPrompt(const Interactable& it) const; // IInteractable.DisplayInteractionPrompt
    bool promptShown() const; // any offered interactable shows the prompt (the indicator)
    // Image.enabled of icon `icon` on Artifact button `islandId` (as exported until a script sets it,
    // e.g. RuinsMapIcons); false for a button or icon that doesn't exist.
    bool artifactIconShown(int islandId, int icon) const;
    void setArtifactIcon(int islandId, int icon, bool shown);

    // World position a component of `object` is placed relative to (its exported position is added):
    // its owner STile's center (as of now: ObjectStates) plus the object's runtime offset.
    void objectBase(int object, float& x, float& y) const;
    // Where `object` itself is now (its transform position).
    void objectPosition(int object, float& x, float& y) const;
    // Moves `object` (and what's under it) so it sits at world (x, y), on STile `owner` (0 = the world).
    void placeObject(int object, int owner, float x, float y);

    const Area& area;
    Grid grid;
    Artifact artifact;
    ObjectStates objects;
    Starts starts;
    TriggerTracker triggers;
    Npcs npcs;
    Items items;
    Animators animators;
    Particles particles;
    village::KnotBoxes knots; // the knot puzzle's lines (Village)
    village::RuinsArrows ruins; // the ruins arrow, its rods and trail (Village)
    village::ChadJumps chad;    // Chad's jump onto the rock and fall (Village)
    GameState state;
    Player player;
    std::vector<Interactable> interactables; // on offer now
    ScreenEffects effects;

private:
    std::vector<std::vector<uint8_t>> artifactIcons_; // per Area::artifact.buttons entry, per icon: shown

    // Objects may have become active: their Animators enable (rebind), Awake, then their components Start.
    void runStarts();
    // Scripts listening to the grid's events (SGrid / SGridAnimator) hear what happened since last time.
    void sendGridEvents();
};
