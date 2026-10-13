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
// lists them parents before children; runtime active state is in ObjectStates.
struct SceneObject {
    std::string path;      // hierarchy path, e.g. "Map/World Grid/Alcove/Cave Door"
    int parent = -1;       // index in Area::objects, -1 = root
    bool activeSelf = true; // authored
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

// A non-tilemap collider that blocks the player, in world units relative to its owner's center.
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

// One UnityEvent persistent call, as exported. `method` is the subset game/events runs; the rest
// load as Unsupported (the exporter's report lists them by name).
struct CallDef {
    enum class Method {
        SetActive,            // GameObject.SetActive(boolArg) on target
        TeleportHard,         // PlayerPositionChanger.UPPHard: player to dPos
        TeleportRelativePlayer, // UPPRelativePlayer: player += dPos
        TeleportRelativeMe,   // UPPRelativeMe: player to the changer's position + dPos
        TeleportTransform,    // UPPTransform: player to goTo's position + dPos
        ActivateSTile,        // Collectible.ActivateSTile(intArg): SGrid.CollectSTile
        SetFlagOn,            // VarManager.SetBoolOn(stringArg)
        SetOnWater,           // WaterLandColliderManager.SetOnWater(boolArg)
        FadeFromBlack,        // UIEffectApplier.FadeFromBlack
        CheckConditions,      // ConditionChecker.CheckConditions on conditionCheckers[checker]
        ChangeScenes,         // SceneChanger.ChangeScenes (leaves the Village)
        Unsupported,
    };
    Method method = Method::Unsupported;
    std::string name; // "<TargetType>.<Method>", for logs and tests
    int target = -1;  // Area::objects index of the target, -1 = none
    bool boolArg = false;
    int intArg = 0;
    std::string stringArg;
    // PlayerPositionChanger: its position (owner-relative), offset, house flag, goTo position.
    int owner = 0, goToOwner = 0;
    float x = 0, y = 0, dx = 0, dy = 0, goToX = 0, goToY = 0;
    bool tpToHouse = false;
    int checker = -1; // CheckConditions: Area::conditionCheckers index
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
    std::vector<InteractionDef> interactions;
    std::vector<TriggerEventDef> triggerEvents;
    std::vector<CollectibleDef> collectibles;
    float fadeDuration = 1; // UIEffects.fadeDuration (s)
    PlayerDef player;
    ArtifactDef artifact;
    std::vector<TileDef> tiles;
    std::vector<SpriteDef> sprites;
    std::vector<std::string> sheetNames;

    const STileDef* stile(int islandId) const;
    const Spawn* spawn(const char* name) const;
    const std::string& pathOf(int object) const { return objects[object].path; }
    int findObject(const std::string& path) const; // index in objects, -1 if none
};

// Loads e.g. "village/village.json" through plat::readFile, then checks every index in it
// (tile -> sprite -> sheet, cell -> tile, collision cell -> shape, xform size), so a bad export
// fails here instead of reading out of bounds later.
bool loadArea(const char* path, Area& out, std::string* error = nullptr);
