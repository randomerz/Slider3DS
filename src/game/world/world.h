// The running game in one area: the loaded (read-only) Area plus every piece of runtime state,
// and the per-frame simulation. Systems take the World rather than a growing list of its parts:
// physics and AreaView read it, ArtifactUI, events and the update change it.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "game/anim/animators.h"
#include "game/audio/audio_cues.h"
#include "game/events/sound_props.h"
#include "game/cutscene/camera_dolly.h"
#include "game/data/area.h"
#include "game/fx/item_pickup.h"
#include "game/hints/hints.h"
#include "game/fx/particles.h"
#include "game/fx/screen_effects.h"
#include "game/grid/artifact.h"
#include "game/grid/artifact_buttons.h"
#include "game/grid/ui_trackers.h"
#include "game/world/game_state.h"
#include "game/grid/grid.h"
#include "game/items/items.h"
#include "game/npc/npc_walks.h"
#include "game/npc/npcs.h"
#include "game/world/object_states.h"
#include "game/save/settings.h"
#include "game/world/starts.h"
#include "game/world/world_sounds.h"
#include "game/events/triggers.h"
#include "game/village/knot_box.h"
#include "game/village/cat_trick.h"
#include "game/village/chad_jump.h"
#include "game/village/final_puzzle.h"
#include "game/village/ruins_arrow.h"
#include "game/village/cat_abduction.h"
#include "game/village/village_intro.h"

struct SaveProfile;

// The player (Player.cs): a position point, the box that collides (on foot or in the boat), and
// the STile it stands on, which it rides while that tile slides.
// Who holds the player (Player.SetCanMove(false) + a PauseManager restriction): while any does, it
// can't move, act or pause, and the Artifact stays shut.
enum PlayerLock : uint8_t { LOCK_DOLLY = 1, LOCK_SCENE = 2, LOCK_PICKUP = 4 };

struct Player {
    static constexpr float HOUSE_Y = -75; // Player.houseYThreshold: below it, the player is in a house
    float x = 0, y = 0;
    bool onWater = false;
    bool facingLeft = false; // Player.Update flips the sprite by the input's x
    float lastMoveX = 0, lastMoveY = 0; // Player.lastMoveDir: the last non-zero input (item drops go that way)
    bool inHouse = false;    // Player.SetIsInHouse (house teleports and placing the player set it)
    int stile = 0;           // islandId underneath, 0 = none
    uint8_t locks = 0;       // PlayerLock bits held now

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

inline const Settings DEFAULT_SETTINGS{}; // SettingsManager's defaults

// The player's input for one frame (from the frontend): where it walks (each -1..1) and whether the
// action button is down (held-button behaviours: holding to skip a dolly). Presses go to action().
struct FrameInput {
    float moveX = 0, moveY = 0;
    bool actionHeld = false;
};

// World-space text in the dialogue style that isn't an NPC's typed line (CatTrick's count): drawn
// fully shown in `box`, placed with objectBase(object), sorted like its canvas by `sortObject`'s y.
struct WorldText {
    const TextBoxDef* box;
    int object, sortObject;
    const char* text; // (static strings: AreaView caches layouts by pointer)
};

class World {
public:
    static constexpr float MOVE_SPEED = 5.0f; // world units per second (Player.moveSpeed)

    // A new game in `area`: tiles as authored, no flags or collectibles, Starts run. The player is
    // at the origin until placed. `settings` (the player's, owned by main) must outlive the World
    // (tests: DEFAULT_SETTINGS, or their own).
    World(const Area& area, const Settings& settings);

    // The scene loaded from `profile` (SaveProfile.Load, then the scene's Starts): its state, the
    // grid (unless never saved: as authored), the player (unless never saved: placeAtStart), play time.
    void load(const SaveProfile& profile);
    // Profile.Save: writes what the profile keeps (all but its name) from the game as it is now.
    void save(SaveProfile& profile) const;

    // Scene objects as a fresh load with the current state would have them (e.g. after changing
    // the state in a dev control): authored, uncollected tiles' objects off (STile.Awake), then
    // SGrid.Start (collected collectibles hidden), then every component's Start. Trigger contacts
    // reset too, so the next update re-enters the triggers the player stands in, as after a load.
    void reloadObjects();
    // STile.SetTileActive: the tile joins (or leaves) the grid, with its objects and tilemaps (whose
    // components then Start).
    void setTileActive(int islandId, bool active);
    // SGrid.SetGrid: the tiles jump to `ids` (Grid::setGrid's format) and the Artifact resets to
    // match; the player rides along with its tile (it's parented to it). Use it, not
    // Artifact::setGrid, whenever the player matters. Unlike Unity, queued and running moves are
    // dropped (Unity keeps both, which would slide tiles from where they no longer are). False if
    // malformed.
    bool setGrid(const std::string& ids);
    // SceneChanger.ChangeScenes (`changer`: its object): the player is held while the screen fades
    // to black at double speed, then the area is left (effects.leftArea: the port's ending, as no
    // area follows the Village yet) and the game is saved (Unity saves before the fade) with the
    // player on the spawn nearest the exit — where coming back through it puts them — so Continue
    // doesn't start inside the exit (also if the app closes mid-fade: save writes that spot). Once
    // per scene load. Relies on the fade's callback: nothing else may start a black fade meanwhile
    // (none can: the held player can't open doors, and dollies hold the player themselves).
    void leaveArea(int changer);
    // The spawn leaving through `changer` saves the player on: on the exit's own STile, nearest to
    // it (each exit has its way-back spawn beside it). Its Area::spawns index, -1 = none.
    int exitSpawn(int changer) const;
    // Puts the player at a spawn point (SceneSpawns), in the boat if it says so.
    void placeAt(const Spawn& spawn);
    void placeAt(const Spawn& spawn, Player& p) const; // (`p` instead of the player)
    // The new game's start: the "Default" spawn (inside tile 1's house). Its Area::spawns index, -1
    // if the area has none (the player stays put).
    int placeAtStart();
    int startSpawn() const; // that spawn's index, -1 = none
    // Puts the player at (x, y) on foot (or the nearest spot where it fits, if any).
    void placeNear(float x, float y);

    // One frame: queued slides advance and the player rides the tile under it; the player walks
    // along (moveX, moveY) (each -1..1, normalized here) at MOVE_SPEED, colliding unless `noclip`
    // or noClip (and is pushed out of anything it overlaps; `noclip`: tests that walk through walls
    // but keep triggers); triggers react to what it touches; NPCs update; Starts run for components
    // that became active; Animators and particles advance.
    void update(float dt, const FrameInput& in, bool noclip = false);
    void update(float dt, float moveX, float moveY, bool noclip) { update(dt, FrameInput{moveX, moveY, false}, noclip); }

    // PlayerAction.Action (the action button): pick up an item if one is in reach, use the offered
    // interactables; if neither did anything, drop the carried item. While the player is locked it
    // can't act: the press goes to what holds it (a dolly: the hold to skip).
    void action();
    // PlayerAction: offer / withdraw an interactable, and run the offered ones with the highest
    // priority; true if one did something (IInteractable.Interact).
    void offer(const Interactable& it);
    void withdraw(const Interactable& it);
    bool interact();
    int priority(const Interactable& it) const;     // IInteractable.InteractionPriority
    bool showsPrompt(const Interactable& it) const; // IInteractable.DisplayInteractionPrompt
    bool promptShown() const; // any offered interactable shows the prompt (the indicator)

    // World position a component of `object` is placed relative to (its exported position is added):
    // its owner STile's center (as of now: ObjectStates) plus the object's runtime offset.
    void objectBase(int object, float& x, float& y) const;
    // Where `object` itself is now (its transform position).
    void objectPosition(int object, float& x, float& y) const;
    // The world texts showing now (`out` is cleared first).
    void worldTexts(std::vector<WorldText>& out) const;
    // Where the top screen's camera is: on a running camera dolly, else following the player (shaken).
    void camera(float& x, float& y) const;
    // What the camera follows (the dolly, else the player), without its shake: AudioManager's listener.
    void cameraTarget(float& x, float& y) const;
    // Something holds the player (PlayerLock): no moving, acting, pausing or Artifact.
    bool playerLocked() const { return player.locks != 0; }
    void lockPlayer(PlayerLock owner, bool locked) {
        player.locks = locked ? (uint8_t)(player.locks | owner) : (uint8_t)(player.locks & ~owner);
    }
    // Moves `object` (and what's under it) so it sits at world (x, y), on STile `owner` (0 = the world).
    void placeObject(int object, int owner, float x, float y);
    // A script's item drop has landed: runs its callback (Items::drop's DropDone).
    void dropLanded(DropDone done, int arg);
    // A screen effect's callback is due (ScreenEffects: a fade to black ended, a flash peaked).
    void effectDone(EffectDone done);

    const Area& area;
    Grid grid;
    Artifact artifact;
    ArtifactButtons artifactButtons; // what scripts set on the Artifact's buttons (icons, completion)
    UiTrackers trackers;             // the Artifact's pins (the player, Slider 2's collectible)
    ObjectStates objects;
    Starts starts;
    TriggerTracker triggers;
    Npcs npcs;
    NpcWalks walks; // NPCs walking their paths
    Items items;
    Animators animators;
    Particles particles;
    village::KnotBoxes knots; // the knot puzzle's lines (Village)
    village::RuinsArrows ruins; // the ruins arrow, its rods and trail (Village)
    village::ChadJumps chad;    // Chad's jump onto the rock and fall (Village)
    village::CatTricks cats;    // Cathy's cats returned (Village)
    village::FinalPuzzle finale; // the fragment in the hole, the shuffle, the final 8-puzzle (Village)
    village::VillageIntro intro; // the first house exit's cutscene (Village)
    village::CatAbduction abduction; // the cat floating off to the beach after the intro (Village)
    CameraDollies dollies;       // cutscene cameras on a path
    GameState state;
    Player player;
    std::vector<Interactable> interactables; // on offer now
    ScreenEffects effects;
    ItemPickup pickup; // ItemPickupEffect: a collectible's pick-up cutscene
    Hints hints;       // PlayerActionHints + UIHints: the tutorial hints
    const Settings& settings; // the player's preferences (not the save profile)
    // The coroutine clock (Wait / WaitUntil in game/world/wait.h): advanced at the end of update().
    uint32_t frame = 0;
    double playTime = 0; // seconds played in this profile (PlayTimer: update() adds each frame)
    // A script asked for the game to be saved (SaveSystem.SaveGame: a collectible's pick-up); the
    // frontend saves and clears it.
    bool saveRequested = false;
    // Player.noClipEnabled (the Cheats page's NoClip, Player.toggleCollision): the player's colliders
    // are off — it walks through everything and touches no trigger (leaving those it was in;
    // turned back on, it enters them again). Set by the frontend each frame.
    bool noClip = false;
    // What scripts asked AudioManager for; the frontend passes it on each frame.
    AudioCues audio;
    SoundProps props; // CowProp / DogProp: cooldowns and emotes
    WorldSounds sfx;  // footsteps, ambiences

private:
    // Objects may have become active: their Animators enable (rebind), Awake, then their components Start.
    void runStarts();
    // Scripts listening to the grid's events (SGrid / SGridAnimator) hear what happened since last time.
    void sendGridEvents();

    int exitSpawn_ = -1; // leaveArea: the Area::spawns index the save puts the player on, -1 = none
};
