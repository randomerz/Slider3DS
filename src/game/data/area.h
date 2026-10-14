// Area data loaded from the Unity export (assets/<area>/<area>.json; format documented in the
// exporter, Slider repo: Assets/Scripts/Editor/Export3DS/Slider3DSExporter.cs).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct SpriteDef {
    int sheet;
    int x, y, w, h;       // pixel rect in the sheet, top-left origin
    float pivotX, pivotY; // pixels from the sprite's bottom-left
    float ppu;
};

struct TileDef {
    std::vector<int> frames; // SpriteDef indices; empty = occupies the cell but draws nothing
    float fps = 0.0f;
};

struct TileLayer {
    std::string path;
    int owner = 0; // STile islandId, 0 = world
    bool active = true;
    float originX = 0, originY = 0; // world units; relative to the owner STile's position
    float anchorX = 0.5f, anchorY = 0.5f;
    int sortLayer = 0, order = 0;
    int minX = 0, minY = 0, w = 0, h = 0;
    std::vector<int16_t> cells; // TileDef index, -1 empty; row-major from (minX, minY), y up
    std::vector<uint8_t> xform; // plat::DrawFlags per cell; empty = none

    int cell(int cx, int cy) const { return cells[cy * w + cx]; }
};

// Collision tilemap: cells index Area::shapes. Only solid (non-trigger, enabled) maps are kept.
enum class CollisionMode { Always, OnFoot, OnWater }; // WaterLandColliderManager toggles the latter two

struct CollisionLayer {
    std::string path;
    int owner = 0;
    bool active = true; // authored state; ignored for OnFoot/OnWater
    CollisionMode mode = CollisionMode::Always;
    float originX = 0, originY = 0;
    int minX = 0, minY = 0, w = 0, h = 0;
    std::vector<int16_t> cells; // shape index, -1 = none

    int cell(int cx, int cy) const { return cells[cy * w + cx]; }
};

// One polygon, flat x0,y0,x1,y1,... in cell units with (0,0) at the cell's min corner.
using Polygon = std::vector<float>;

struct Spawn {
    std::string name;
    int owner; // STile islandId (position relative to its center), 0 = world
    float x, y;
    bool inBoat;
};

// A sprite animation (an AnimationClip's sprite keys): frame i shows from times[i] until the next key.
struct Anim {
    std::string name;
    std::vector<int> frames;  // Area::sprites indices
    std::vector<float> times; // seconds, ascending, parallel to frames
    float length = 0;         // clip length (s)
    bool loop = true;

    // The sprite shown `t` seconds into the clip.
    int frameAt(float t) const;
};

// A scene object (GameObject): what components belong to and GameObject.SetActive acts on. Area
// lists them parents before children; runtime state (active, placement, parent) is in ObjectStates.
struct SceneObject {
    std::string path;      // hierarchy path, e.g. "Map/World Grid/Alcove/Cave Door"
    int parent = -1;       // index in Area::objects, -1 = root
    bool activeSelf = true; // authored
    int owner = 0;         // the STile it's under (islandId; an STile's own object: itself), 0 = world
    float x = 0, y = 0;    // position, relative to its owner's center (its components' "owner" is the same)
};

// A sprite as placed in the world: its pivot lands on (x, y) (relative to some base), and it sorts
// among the tile layers by (sortLayer, order, higher sortY first), like Unity's transparency sort.
struct SpritePlacement {
    int sprite = -1;    // Area::sprites index (the authored frame; anim overrides it)
    int anim = -1;      // Area::anims index, -1 = static
    float x = 0, y = 0; // transform position
    float sortY = 0;    // sort point y (pivot or sprite center)
    int sortLayer = 0, order = 0;
    uint32_t flags = 0; // plat::DrawFlags (FLIP_X / FLIP_Y)
};

// A world SpriteRenderer (prop, tree, NPC, item, empty-slot frame).
struct WorldSprite {
    int object = 0;        // Area::objects index
    int owner = 0;         // STile islandId (placement relative to its center), 0 = world
    SpritePlacement place;
    float animStart = 0, animRandom = 0; // normalized start time, or a random one in [0, animRandom)
};

// A non-tilemap collider: solid (Area::objectColliders, blocks the player) or a trigger
// (Area::triggerColliders). World units relative to its owner's center (plus the object's runtime
// offset, ObjectStates).
struct ObjectCollider {
    enum class Kind { Poly, Circle, Edge };
    int object = 0;      // Area::objects index
    int owner = 0;
    bool enabled = true; // the component itself (active also needs its object active in hierarchy)
    Kind kind = Kind::Poly;
    std::vector<Polygon> paths;       // Poly: closed polygons; Edge: one open polyline
    float cx = 0, cy = 0, radius = 0; // Circle
    float minX = 0, minY = 0, maxX = 0, maxY = 0; // bounds, for culling
};

// One Condition (Condition.cs) as exported.
struct ConditionDef {
    enum class Type { Item, Grid, GridStationary, NoGrid, Flag, NoFlag, PlayerCarryingItem, Unsupported };
    Type type = Type::Unsupported;
    std::string name;     // Item: collectible name; Flag/NoFlag: flag; PlayerCarryingItem: item name
    std::string itemArea; // Item: the collectible's area ("Village")
    std::string pattern;  // grid conditions: CheckGrid.contains pattern
    std::vector<int> stationaryTiles; // GridStationary: these tiles must not be moving
};

// Whether a condition depends only on GameState and Grid (whose version() counters then tell when
// it may have changed); a new condition kind must say which it is.
bool conditionReadsOnlyStateAndGrid(ConditionDef::Type type);

// The UnityEvent methods the port runs, as X(enum name, exported "<TargetType>.<Method>"):
//   SetActive               GameObject.SetActive(boolArg) on target
//   TeleportHard            PlayerPositionChanger.UPPHard: player to dPos
//   TeleportRelativePlayer  UPPRelativePlayer: player += dPos
//   TeleportRelativeMe      UPPRelativeMe: player to the changer's position + dPos
//   TeleportTransform       UPPTransform: player to goTo's position + dPos
//   ActivateSTile           Collectible.ActivateSTile(intArg): SGrid.CollectSTile
//   SetFlagOn               VarManager.SetBoolOn(stringArg)
//   SetOnWater              WaterLandColliderManager.SetOnWater(boolArg)
//   FadeFromBlack           UIEffectApplier.FadeFromBlack
//   CheckConditions         ConditionChecker.CheckConditions on conditionCheckers[data]
//   ChangeScenes            SceneChanger.ChangeScenes (leaves the Village)
//   NpcTriggerEnter/Exit    NPC.OnDialogueTriggerEnter / Exit on the target's NPC
//   NpcTypeCurrent          NPC.TypeCurrentDialogue: (re)types its current line
//   NpcTeleport             NPC.Teleport(objectArg): the NPC moves onto that object (and is parented to it)
//   SetFlipX                SpriteRenderer.set_flipX(boolArg) on the target's sprite
// To add one: a line here, its case in events.cpp's runCall (the compiler flags a missing case),
// and its name in the exporter's PortMethods (area_test pins the names still unsupported).
#define SLIDER_CALL_METHODS(X)                                         \
    X(SetActive, "GameObject.SetActive")                               \
    X(TeleportHard, "PlayerPositionChanger.UPPHard")                   \
    X(TeleportRelativePlayer, "PlayerPositionChanger.UPPRelativePlayer") \
    X(TeleportRelativeMe, "PlayerPositionChanger.UPPRelativeMe")       \
    X(TeleportTransform, "PlayerPositionChanger.UPPTransform")         \
    X(ActivateSTile, "Collectible.ActivateSTile")                      \
    X(SetFlagOn, "VarManager.SetBoolOn")                               \
    X(SetOnWater, "WaterLandColliderManager.SetOnWater")               \
    X(FadeFromBlack, "UIEffectApplier.FadeFromBlack")                  \
    X(CheckConditions, "ConditionChecker.CheckConditions")             \
    X(ChangeScenes, "SceneChanger.ChangeScenes")                       \
    X(NpcTriggerEnter, "NPC.OnDialogueTriggerEnter")                   \
    X(NpcTriggerExit, "NPC.OnDialogueTriggerExit")                     \
    X(NpcTypeCurrent, "NPC.TypeCurrentDialogue")                       \
    X(NpcTeleport, "NPC.Teleport")                                     \
    X(SetFlipX, "SpriteRenderer.set_flipX")

// One UnityEvent persistent call, as exported. Methods the port doesn't run load as Unsupported
// (and do nothing); the exporter's report lists them by name.
struct CallDef {
    enum class Method {
#define SLIDER_CALL_ENUM(id, name) id,
        SLIDER_CALL_METHODS(SLIDER_CALL_ENUM)
#undef SLIDER_CALL_ENUM
        Unsupported,
    };
    Method method = Method::Unsupported;
    std::string name; // "<TargetType>.<Method>", for logs and tests
    int target = -1;  // Area::objects index of the target, -1 = none
    int objectArg = -1; // a scene-object argument (Transform / GameObject), Area::objects index
    bool boolArg = false;
    int intArg = 0;
    std::string stringArg;
    int data = -1; // Teleport*: Area::teleports index; CheckConditions: Area::conditionCheckers index
};

// What a PlayerPositionChanger call needs of its component.
struct TeleportDef {
    int owner = 0;      // the changer's STile (its position is relative to it), 0 = world
    float x = 0, y = 0; // the changer's position
    float dx = 0, dy = 0; // dPos
    bool tpToHouse = false;
    bool hasGoTo = false; // goTo set (UPPTransform needs it)
    int goTo = -1;        // goTo's Area::objects index (it may move at runtime)
    int goToOwner = 0;
    float goToX = 0, goToY = 0;
};

// Conditionals: all conditions true -> onSuccess, else onFail.
struct ConditionalsDef {
    std::vector<ConditionDef> conditions;
    std::vector<CallDef> onSuccess, onFail;
};

// A ConditionChecker: its conditionals run from events (CheckConditions), and with checkOnStart
// also when its object first becomes active (Unity's Start).
struct ConditionCheckerDef {
    int object = 0;      // Area::objects index
    bool enabled = true; // the component itself
    bool checkOnStart = false;
    ConditionalsDef conditionals;
};

// PlayerConditionals: with addToOnAction, offered while the player is in its trigger and run by the
// action button; otherwise run when the player enters the trigger.
struct InteractionDef {
    int object = 0;
    bool addToOnAction = false;
    bool excludeBoat = false, excludeLand = false; // not offered on water / on land
    bool isCarryingItem = false; // needs the player to carry an item (named itemName, if set)
    std::string itemName;
    int priority = 0; // IInteractable.InteractionPriority (PlayerConditionals doesn't override it: 0)
    std::vector<CallDef> onSuccess;
};

// OnTriggerEnter: calls when the player enters / leaves the object's trigger.
struct TriggerEventDef {
    int object = 0;
    bool onPlayerEnter = true;
    std::vector<CallDef> onEnter, onExit;
};

// A Collectible: picked up when the player touches it (PlayerInventory.AddCollectible), hiding it.
struct CollectibleDef {
    int object = 0;
    std::string name, area;     // CollectibleData
    bool disableAtStart = false; // Start hides it (shown later by an event)
    int sprite = -1;
    std::vector<CallDef> onCollect;
};

// One line of an NPC's dialogue chain (DialogueData).
struct DialogueDef {
    std::string text;            // English, with TMPSpecialText tags (<var>, <wavy>, ...)
    float delay = 0.5f;          // delayAfterFinishedTyping: then the chain advances (0 = it doesn't)
    bool waitForAction = false;  // waitUntilPlayerAction: the action button advances it
    bool manual = false;         // advanceDialogueManually: only an event advances it
    bool noRepeatEvents = false; // doNotRepeatEvents: onStart / onEnd run once
    bool dontInterrupt = false;  // leaving the trigger doesn't end it
    uint8_t emoteOnStart = 0, emoteOnLeave = 0; // NPCEmotes.Emotes shown when it starts / the box closes
    std::vector<CallDef> onStart, onEnd;
};

// NPCConditionals: when its conditions hold (all, or any with anyOf) it can be the NPC's current
// one; the highest in the list wins.
struct NpcCondDef {
    std::vector<ConditionDef> conditions;
    bool anyOf = false;
    bool alwaysStartFromBeginning = false;
    uint8_t emote = 0;                          // emoteOnEnter (NPCEmotes.Emotes)
    std::vector<DialogueDef> dialogue;          // the dialogue chain
    std::vector<CallDef> onEnter, onExhausted;  // onConditionalEnter, onDialogueChainExhausted
};

// Where and how an NPC's DialogueDisplay types: TMP text in a box (owner-relative world units,
// inside its margins), centered, bottom-aligned, word-wrapped, in the dialogue font.
struct NpcTextDef {
    float minX = 0, minY = 0, maxX = 0, maxY = 0;
    float lineAdvance = 0.43f, wordSpacing = 0, charSpacing = 0; // world units
    int sortLayer = 0, order = 0; // its canvas
};

// NPCEmotes: a small animated sprite beside an NPC's head showing one of NPCEmotes.Emotes (index:
// 0 None, 1 Sparkle, 2 Exclamation, 3 Question, 4 Annoyed, 5 Sweat): its Animator state's clip, then
// (once that ends) the clip of the state it moves on to. It sits on the side the NPC faces.
struct NpcEmoteDef {
    enum : uint8_t { None, Sparkle, Exclamation, Question, Annoyed, Sweat, Count };
    int object = -1;       // the NPCEmotes object, -1 = the NPC has none
    SpritePlacement place; // owner-relative, at its exported local x; sprite may be -1 (the clips key it)
    float localX = 0;      // its exported local x; it's mirrored to +-|localX| by the facing
    bool facesRight = false; // npcDefaultFacesRight
    int npcSprite = -1;      // object of the NPC's SpriteRenderer, whose flipX decides the side
    bool npcSpriteFlipped = false; // that sprite's exported flipX
    int intro[Count] = {-1, -1, -1, -1, -1, -1}; // per emote: Area::anims index, -1 = none
    int loop[Count] = {-1, -1, -1, -1, -1, -1};  // what plays once intro's clip has ended, -1 = none
};

// An NPC (NPC.cs): conditionals with dialogue chains, typed above it while the player is in its
// dialogue trigger (whose OnTriggerEnter calls NpcTriggerEnter / Exit). Its position and owner are
// its object's.
struct NpcDef {
    int object = 0;
    std::string name;   // characterName
    bool facingLeft = false; // spriteDefaultFacingLeft (for walks: not read yet)
    NpcTextDef text;
    int pingObject = -1; // the message ping (a world sprite; shown while the chain isn't exhausted), -1 = none
    NpcEmoteDef emote;
    // Some conditional reads more than the game state and the grid (PlayerCarryingItem, spec / not yet
    // supported kinds), so its pick must be re-checked every frame (derived at load).
    bool pollsEveryFrame = false;
    std::vector<NpcCondDef> conds;
};

// A behaviour component on a scene object (an entry in one of the Area's per-kind vectors), for
// handling things by object: what reacts when the player touches an object's trigger, which
// components have a Start. Built at load.
struct ComponentRef {
    enum class Kind : uint8_t { TriggerEvent, Interaction, Collectible, ConditionChecker, Npc };
    Kind kind;
    int index; // into triggerEvents / interactions / collectibles / conditionCheckers / npcs
};

struct ComponentRange {
    const ComponentRef *first, *last;
    const ComponentRef* begin() const { return first; }
    const ComponentRef* end() const { return last; }
};

// A box collider relative to the player position: center offset and full size (edge radius included).
struct Hitbox {
    float offX = 0, offY = 0, w = 0, h = 0;
};

// How the player is drawn (relative to the player position; the boat only on water, the action
// indicator only while an interaction is on offer) and collides.
struct PlayerDef {
    SpritePlacement body, boat, actionIndicator;
    Hitbox footBox, boatBox; // the player's collider on foot / on water
};

// A sprite placed in UI pixels: rect min corner (x, y) and size, y up, relative to some origin.
struct UiPart {
    int sprite = -1; // index in Area::sprites; -1 = none
    int x = 0, y = 0, w = 0, h = 0;
};

struct ArtifactButtonDef {
    int islandId = 0;
    int island = -1, completed = -1; // sprites
    std::vector<UiPart> icons;      // overlays, relative to the button center
};

// The Artifact's tile screen (UIArtifact / ArtifactTileButton). UI pixels, y up, origin at the
// panel center; button slot (x, y) is centered at ((x - 1), (y - 1)) * buttonSpacing.
struct ArtifactDef {
    bool present = false;
    int buttonSpacing = 37;
    UiPart frame, background;          // relative to the panel center
    std::vector<UiPart> underlays;     // area art over the background, under the buttons (panel center)
    UiPart pressed, highlight, hover;  // relative to a button center
    int empty = -1, blank = -1, hoverIcon = -1; // sprites replacing the island image
    int pressedDrop = 1;               // px the image drops while pressed
    float nudgeDistance = 2, nudgeDuration = 0.2f;
    std::vector<ArtifactButtonDef> buttons;

    const ArtifactButtonDef* button(int islandId) const {
        for (const ArtifactButtonDef& b : buttons)
            if (b.islandId == islandId) return &b;
        return nullptr;
    }
};

// An STile as exported (new-game state). Its runtime slot/position lives in Grid.
struct STileDef {
    int islandId;
    std::string name;
    int x, y;  // starting grid slot; the tile is centered on (x * width, y * width)
    int width; // STILE_WIDTH (17)
    bool active; // collected at the start
    // Area::objects indices STile.SetTileActive switches with the tile (its "objects" and
    // "allTileMaps"), -1 = none.
    int objectsObject = -1, tileMapsObject = -1;
};

// Loaded, read-only area data. Runtime state (tile slots, slides, fences) is in Grid.
struct Area {
    int ppu = 16;
    int gridW = 0, gridH = 0;
    std::string targetGrid;
    int housingOffset = 0; // y offset of each STile's house interior
    uint8_t background[3] = {0, 0, 0}; // camera clear color (the void between/behind tiles)
    std::vector<STileDef> stiles;
    std::vector<Spawn> spawns;
    std::vector<CollisionLayer> collision;
    std::vector<std::vector<Polygon>> shapes; // collision shapes (each a list of polygons)
    std::vector<TileLayer> layers; // drawn layers only, in draw order
    std::vector<SceneObject> objects; // parents before children
    std::vector<WorldSprite> worldSprites;
    std::vector<Anim> anims;
    std::vector<ObjectCollider> objectColliders;
    std::vector<ObjectCollider> triggerColliders; // of objects with interactions, trigger events, collectibles
    std::vector<ConditionCheckerDef> conditionCheckers;
    std::vector<TeleportDef> teleports; // payloads of teleport calls (CallDef::data)
    std::vector<InteractionDef> interactions;
    std::vector<TriggerEventDef> triggerEvents;
    std::vector<CollectibleDef> collectibles;
    std::vector<NpcDef> npcs;
    float fadeDuration = 1; // UIEffects.fadeDuration (s)
    // Each object's components (derived at load): componentRefs[componentStart[o] .. componentStart[o + 1]),
    // per object in the kind order TriggerEvent, Interaction, Collectible, ConditionChecker, Npc.
    std::vector<int> componentStart;
    std::vector<ComponentRef> componentRefs;
    ComponentRange components(int object) const {
        const ComponentRef* base = componentRefs.data();
        return {base + componentStart[object], base + componentStart[object + 1]};
    }
    PlayerDef player;
    ArtifactDef artifact;
    std::vector<TileDef> tiles;
    std::vector<SpriteDef> sprites;
    std::vector<std::string> sheetNames;

    const STileDef* stile(int islandId) const;
    const Spawn* spawn(const char* name) const;
    const std::string& pathOf(int object) const { return objects[object].path; }
    int findObject(const std::string& path) const; // index in objects, -1 if none
    int npcOn(int object) const; // index in npcs of the NPC on `object`, -1 if none
};

// Loads e.g. "village/village.json" through plat::readFile, then checks every index in it
// (tile -> sprite -> sheet, cell -> tile, collision cell -> shape, xform size), so a bad export
// fails here instead of reading out of bounds later.
bool loadArea(const char* path, Area& out, std::string* error = nullptr);
