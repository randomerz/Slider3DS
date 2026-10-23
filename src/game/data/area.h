// Area data loaded from the Unity export (assets/<area>/<area>.json; format documented in the
// exporter, Slider repo: Assets/Scripts/Editor/Export3DS/Slider3DSExporter.cs).
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "game/data/curve.h"

struct SpriteDef {
    int sheet;            // -1 for an outline sprite
    int x, y, w, h;       // pixel rect in the sheet, top-left origin
    float pivotX, pivotY; // pixels from the sprite's bottom-left
    float ppu;
    // A sprite that is only a solid border (BGFrame): drawn as rectangles, no texture. 0 = a normal sprite.
    int outlineWidth = 0;
    uint32_t outlineColor = 0; // 0xRRGGBBAA
    // Every pixel this one opaque color (0xRRGGBBFF; the exporter's "solid"; 0 = not): AreaView draws
    // runs of such tile cells as rectangles.
    uint32_t solid = 0;
    // Or an opaque base color (0xRRGGBBFF; 0 = not) with straight lines along some edges: per side
    // (top, bottom, left, right, sprite space) a color and a width in px (0: no line). The
    // exporter's "base" / "edges" (a slider's outline cells): drawn as rectangles too.
    uint32_t edgedBase = 0;
    uint32_t edgeColor[4] = {0, 0, 0, 0};
    uint8_t edgeWidth[4] = {0, 0, 0, 0};
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

// Material tilemap (STileTilemap.materials): cells index Area::footsteps, the surface the player's
// footsteps sound on. An STile has one for its surface and one for its house.
struct MaterialLayer {
    int owner = 0;
    bool house = false; // its STile's houseTilemaps' (read while the player is in that house)
    float originX = 0, originY = 0;
    int minX = 0, minY = 0, w = 0, h = 0;
    std::vector<int8_t> cells; // Area::footsteps index, -1 = none

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
// A null sprite (a clip's null key, SpriteRenderer.sprite = null): draws nothing.
constexpr int NULL_SPRITE = -2;

struct Anim {
    std::string name;
    std::vector<int> frames;  // Area::sprites indices, or NULL_SPRITE
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
    bool enabled = true;   // the SpriteRenderer itself at the start (ObjectStates tracks it at runtime)
};

// A non-tilemap collider: solid (Area::objectColliders, blocks the player) or a trigger
// (Area::triggerColliders). World units relative to its owner's center (plus the object's runtime
// offset, ObjectStates).
struct ObjectCollider {
    enum class Kind { Poly, Circle, Edge };
    int object = 0;      // Area::objects index
    int owner = 0;
    bool enabled = true; // the component itself at the start (ObjectStates tracks it at runtime; active
                         // also needs its object active in hierarchy)
    int slot = 0;        // runtime enabled flag: objectColliders index, or objectColliders.size() + triggerColliders index
    std::string layer;   // Unity layer name (item pickups and drops filter by it)
    Kind kind = Kind::Poly;
    std::vector<Polygon> paths;       // Poly: closed polygons; Edge: one open polyline
    float cx = 0, cy = 0, radius = 0; // Circle
    float minX = 0, minY = 0, maxX = 0, maxY = 0; // bounds, for culling
};

// The spec conditions (Condition.checkBool methods) the port evaluates, as X(enum name, exported
// "<TargetType>.<Method>"):
//   VillageFishOn     VillageGrid.CheckFishOn: VillageGrid.TurnFishOn ran (GameState flag "villageFishOn")
//   KnotSolved        KnotBox.CheckPuzzle on knotBoxes[data]: no bad lines
//   KnotPartlySolved  KnotBox.CheckPuzzlePartial on knotBoxes[data]: at most 3 bad lines
//   RuinsRodsOn       RuinsArrow.CheckActiveRods on ruinsArrows[data]: some rod is lit
//   ChadFell          ChadJump.ChadFell on chadJumps[data]: he has fallen off the rock
//   ChadTipped        VillageChadJump.DidSlideAfterJumping on chadJumps[data]: a tile slid while he was up
// To add one: a line here, its cases in village::checkSpec / specReadsOnlyStateAndGrid (game/village),
// and its name in the exporter's PortSpecs.
#define SLIDER_SPEC_CHECKS(X)                   \
    X(VillageFishOn, "VillageGrid.CheckFishOn") \
    X(KnotSolved, "KnotBox.CheckPuzzle")        \
    X(KnotPartlySolved, "KnotBox.CheckPuzzlePartial") \
    X(RuinsRodsOn, "RuinsArrow.CheckActiveRods")      \
    X(ChadFell, "ChadJump.ChadFell")                  \
    X(ChadTipped, "VillageChadJump.DidSlideAfterJumping") \
    X(CatMittens, "CatTrick.CheckMittens")            \
    X(CatAutumn, "CatTrick.CheckAutumn")              \
    X(CatTofu, "CatTrick.CheckTofu")

// One Condition (Condition.cs) as exported. Spec conditions the port doesn't check load as Unsupported.
struct ConditionDef {
    enum class Type { Item, Grid, GridStationary, NoGrid, Flag, NoFlag, PlayerCarryingItem, Spec, Unsupported };
    enum class Spec {
#define SLIDER_SPEC_ENUM(id, name) id,
        SLIDER_SPEC_CHECKS(SLIDER_SPEC_ENUM)
#undef SLIDER_SPEC_ENUM
    };
    Type type = Type::Unsupported;
    std::string name;     // Item: collectible name; Flag/NoFlag: flag; PlayerCarryingItem: item name
    std::string itemArea; // Item: the collectible's area ("Village")
    std::string pattern;  // grid conditions: CheckGrid.contains pattern
    std::vector<int> stationaryTiles; // GridStationary: these tiles must not be moving
    Spec spec = Spec(0);              // Spec: which check (name: its exported method)
    int specTarget = -1;              // Spec: the object its check runs on (e.g. the KnotBox)
    int data = -1;                    // KnotSolved / KnotPartlySolved: Area::knotBoxes index; RuinsRodsOn: Area::ruinsArrows;
                                      // ChadFell / ChadTipped: Area::chadJumps; CatMittens / CatAutumn / CatTofu: Area::catTricks
};

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
//   NpcStartWalk            NPC.StartWalkAtIndex(intArg): its current conditional's walk (game/npc/npc_walks)
//   NpcStartValidWalk       NPC.StartValidWalk: once no tile moves, its current conditional's first walk whose crossings hold
//   SetFlipX                SpriteRenderer.set_flipX(boolArg) on the target's sprite
//   AnimSetBoolTrue/False   NPCAnimatorController.SetBoolToTrue/False(stringArg): the target drives an Animator
//   SetAnimatorController   Animator.set_runtimeAnimatorController(animatorControllers[data]) on the target
//   VillageActivateSlider   VillageGrid (SGrid).ActivateSliderCollectible(intArg): shows "Slider <n>" (game/village)
//   VillageActivateCollectible  SGrid.ActivateCollectible(stringArg): shows it, cancelling its hide-at-start
//   VillageTurnFishOn / VillageExplode / VillageWaterfallEntry / VillageHouseExit: VillageGrid's methods
//   SetColliderEnabled      BoxCollider2D.set_enabled(boolArg) on the collider with slot data
//   ItemSortingOrderInc/Dec PlayerAction.SetItemSortingOrderInc/Dec(intArg): the carried item's sorting order
//   KnotCheckParticles      KnotBox.CheckParticles on knotBoxes[data]: sparkles if solved
//   KnotSetToCorrect        KnotBox.SetToCorrectPositions on knotBoxes[data]: the nodes go to their solved spots
//   ChadJumpStarter         ChadJump.JumpStarter on chadJumps[data]: he jumps onto the rock
//   ChadSetIsOnRock         VillageChadJump.SetIsOnRock on chadJumps[data]: from now on slides tip him
//   HintTrigger / HintDisable / HintEnableDisabling  PlayerActionHints.TriggerHint / DisableHint /
//                           EnableDisabling(stringArg: a hint's name in Area::hints)
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
    X(NpcStartWalk, "NPC.StartWalkAtIndex")                            \
    X(NpcStartValidWalk, "NPC.StartValidWalk")                         \
    X(SetFlipX, "SpriteRenderer.set_flipX")                            \
    X(AnimSetBoolTrue, "NPCAnimatorController.SetBoolToTrue")          \
    X(AnimSetBoolFalse, "NPCAnimatorController.SetBoolToFalse")        \
    X(SetAnimatorController, "Animator.set_runtimeAnimatorController")  \
    X(VillageActivateSlider, "VillageGrid.ActivateSliderCollectible")  \
    X(VillageActivateCollectible, "VillageGrid.ActivateCollectible")   \
    X(VillageTurnFishOn, "VillageGrid.TurnFishOn")                     \
    X(VillageExplode, "VillageGrid.Explode")                           \
    X(VillageWaterfallEntry, "VillageGrid.OnWaterfallEntry")           \
    X(VillageHouseExit, "VillageGrid.OnHouseExit")                     \
    X(VillageFillInHole, "VillageGrid.FillInHole")                     \
    X(VillageShufflePuzzle, "VillageGrid.ShufflePuzzle")               \
    X(VillageRemoveSlider2Tracker, "VillageGrid.RemoveSlider2Tracker") \
    X(SetColliderEnabled, "BoxCollider2D.set_enabled")                 \
    X(ItemSortingOrderInc, "PlayerAction.SetItemSortingOrderInc")      \
    X(ItemSortingOrderDec, "PlayerAction.SetItemSortingOrderDec")      \
    X(KnotCheckParticles, "KnotBox.CheckParticles")                    \
    X(KnotSetToCorrect, "KnotBox.SetToCorrectPositions")              \
    X(ChadJumpStarter, "ChadJump.JumpStarter")                         \
    X(ChadSetIsOnRock, "VillageChadJump.SetIsOnRock")                  \
    X(CatPickedUp, "CatAbductionController.SetPickedUp")              \
    X(HintTrigger, "PlayerActionHints.TriggerHint")                    \
    X(HintDisable, "PlayerActionHints.DisableHint")                    \
    X(HintEnableDisabling, "PlayerActionHints.EnableDisabling")        \
    X(AudioPlay, "AudioManager.Play")                                  \
    X(PlaySoundString, "PlaySound.PlayString")                         \
    X(UIClick, "UIClick.Click")                                        \
    X(PlayErrorSound, "ConditionChecker.PlayErrorSound")               \
    X(VillagePlayDigSound, "VillageGrid.PlayDigSound")                 \
    X(CowMoo, "CowProp.Moo")                                           \
    X(DogBark, "DogProp.Bark")

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
    // Teleport*: Area::teleports index; CheckConditions: Area::conditionCheckers index;
    // SetAnimatorController: Area::animatorControllers index; SetColliderEnabled: ObjectCollider::slot;
    // KnotCheckParticles / KnotSetToCorrect: Area::knotBoxes index; ChadJumpStarter / ChadSetIsOnRock:
    // Area::chadJumps index; CowMoo / DogBark: the prop's emote object (-1 = none).
    int data = -1;
};

// An Animator controller (base layer; an override controller's clips applied): bool parameters,
// states playing a sprite clip, transitions on bool conditions and/or exit time (all instant).
struct AnimatorTransitionDef {
    int to = 0;           // state index
    float exitTime = -1;  // normalized time it waits for, -1 = none
    std::vector<std::pair<int, bool>> conds; // parameter index, value it must have
};
// A clip's Transform localPosition curves (an item holder bobbing, Chad's sprite shifting as he falls).
struct MoveCurveDef {
    std::string path;               // the transform, relative to the Animator ("" = its own)
    std::vector<CurveKey> x, y;     // empty = that axis isn't animated
    int id = -1;                    // numbered across the area (AnimatorDef::curveTarget)
};
struct AnimatorStateDef {
    std::string name;
    int anim = -1;        // Area::anims index, -1 = no clip (the sprite keeps its last frame)
    float speed = 1;
    std::vector<AnimatorTransitionDef> transitions;
    float length = 0;          // its clip's (0 = no clip, or a zero-length one)
    bool loop = true;
    bool writeDefaults = true; // what its clip doesn't move goes back to the default (authored) position
    std::vector<std::pair<float, std::string>> events; // its clip's AnimationEvents: time (s), function
    std::vector<MoveCurveDef> moves;
};
struct AnimatorControllerDef {
    std::string name;
    std::vector<std::string> params; // bools and triggers
    std::vector<uint8_t> paramDefaults;
    std::vector<uint8_t> paramTriggers; // per param: a trigger (the transition it fires resets it)
    int defaultState = 0;
    std::vector<AnimatorStateDef> states;
    std::vector<AnimatorTransitionDef> anyState;
    int stateIndex(const std::string& name) const; // -1 if none
    int paramIndex(const std::string& name) const; // -1 if none
};
// An Animator on a scene object, driving the world sprite on that object (or the player's: object -1,
// always enabled).
struct AnimatorDef {
    int object = -1;
    int controller = 0;      // Area::animatorControllers index at the start
    bool keepState = false;  // keepAnimatorStateOnDisable (else re-enabling rebinds: default state, parameters)
    int controllerObject = -1; // object of the NPCAnimatorController that drives it (calls target it), -1 = none
    struct Target {            // a transform its clips move
        std::string path;      // relative to the Animator (MoveCurveDef::path)
        int object = -1;       // Area::objects index, -1 = not a scene object (the player's item holder)
        float localX = 0, localY = 0; // its localPosition as authored (the default)
    };
    std::vector<Target> targets;
    // Per MoveCurveDef::id (of every controller): the target it moves on this Animator, -1 = none
    // here (a path its hierarchy doesn't have; the export warns). Built at load.
    std::vector<int> curveTarget;
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
    std::string animationOnStart, animationOnLeave; // NPCAnimatorController.Play states, "" = none
    std::vector<CallDef> onStart, onEnd;
};

// STileCrossing: where a walk passes from STile `from` to `to` (islandIds; 0 = off the grid, whose
// slot is then fromX / fromY or toX / toY). It holds while to's slot minus from's is one of `dirs`.
struct NpcCrossingDef {
    int from = 0, to = 0;
    int fromX = 0, fromY = 0, toX = 0, toY = 0;
    std::vector<std::pair<int, int>> dirs;
};

// NPCWalkData: the NPC walks from point to point of `path` (objects; it's parented to each it
// reaches). Moving a tile of a crossing still ahead breaks the walk (the NPC stops, or with
// teleportIfInterrupted jumps to the end); it resumes when a tile arrives and the crossings hold.
struct NpcWalkDef {
    std::vector<int> path; // at least one point
    std::vector<NpcCrossingDef> crossings; // in the order they're crossed
    bool turnAround = false;               // turnAroundAfterWalking: flipX toggles at the end
    bool teleportIfInterrupted = false;    // teleportToEndIfInterrupted
    std::vector<CallDef> onStarted, onFinished, onBroken, onResumed;
};

// NPCConditionals: when its conditions hold (all, or any with anyOf) it can be the NPC's current
// one; the highest in the list wins.
struct NpcCondDef {
    std::vector<ConditionDef> conditions;
    bool anyOf = false;
    bool alwaysStartFromBeginning = false;
    uint8_t emote = 0;                          // emoteOnEnter (NPCEmotes.Emotes)
    std::string animationOnEnter;               // NPCAnimatorController.Play state, "" = none
    std::vector<DialogueDef> dialogue;          // the dialogue chain
    std::vector<CallDef> onEnter, onExhausted;  // onConditionalEnter, onDialogueChainExhausted
    std::vector<NpcWalkDef> walks;              // started by NPC.StartWalkAtIndex / StartValidWalk
};

// Where and how world-space TMP text in the dialogue style shows (an NPC's DialogueDisplay types
// there; CatTrick's count): in a box (owner-relative world units, inside its margins), centered,
// bottom-aligned, word-wrapped, in the dialogue font.
struct TextBoxDef {
    float minX = 0, minY = 0, maxX = 0, maxY = 0;
    float lineAdvance = 0.43f, wordSpacing = 0, charSpacing = 0; // world units
    int sortLayer = 0, order = 0; // its canvas
};

// NPCEmotes: a small animated sprite beside an NPC's head showing one of NPCEmotes.Emotes (index:
// 0 None, 1 Sparkle, 2 Exclamation, 3 Question, 4 Annoyed, 5 Sweat) by playing the state of that
// name on its own Animator. It sits on the side the NPC faces.
struct NpcEmoteDef {
    enum : uint8_t { None, Sparkle, Exclamation, Question, Annoyed, Sweat, Count };
    int object = -1;       // the NPCEmotes object, -1 = the NPC has none
    SpritePlacement place; // owner-relative, at its exported local x; sprite may be -1 (the clips key it)
    float localX = 0;      // its exported local x; it's mirrored to +-|localX| by the facing
    bool facesRight = false; // npcDefaultFacesRight (its side follows NpcDef::sprite's flipX)
    int animator = -1;       // Area::animators index of its Animator (on `object`)
    static const char* name(int emote); // the Animator state an emote plays ("" for None)
};

// An NPC's voice: its DialogueDisplay's VocalizableParagraph preset (VocalizerParameters), what
// game/npc/vocalizer reads its lines with. (Per-clause presets aren't ported: the loader refuses
// them; the vowel lerp's smoothness and the fade-out cap aren't needed: a syllable ends on the
// synth's own release.)
struct VoiceDef {
    bool present = false;  // (none: the NPC types silently, at the default speed)
    std::string event;     // the FMOD synth (audio.json's "voices")
    bool syllables = true;
    float pitch = 1, duration = 0.1f, volume = 1, volumeDb = 0;
    float wordIntonation = 0, speedup = 0, lowToHigh = 0, highToLow = 0;
    float clauseGap = 0.1f, up = 0, down = 0;
    int intonation = -1;   // an override: 0 flat, 1 up, 2 down; -1 = none
    // Its modifier library, per NPCEmotes.Emotes (None, Sparkle, ...): what the line's emote adds.
    struct Mod {
        float pitch = 0, volumeDb = 0, duration = 0;
        int intonation = -1;
    };
    std::vector<Mod> emotes;
};

// An NPC (NPC.cs): conditionals with dialogue chains, typed above it while the player is in its
// dialogue trigger (whose OnTriggerEnter calls NpcTriggerEnter / Exit). Its position and owner are
// its object's.
struct NpcDef {
    int object = 0;
    std::string name;   // characterName
    bool facingLeft = false; // spriteDefaultFacingLeft: walks face the way they go
    float speed = 0;         // walking speed (units/s)
    int sprite = -1;         // its SpriteRenderer's object, a world sprite (walks set its flipX; the
                             // emote's side follows it), -1 = none
    TextBoxDef text;
    int pingObject = -1; // the message ping (a world sprite; shown while the chain isn't exhausted), -1 = none
    NpcEmoteDef emote;
    int animator = -1; // Area::animators index of its NPCAnimatorController's Animator, -1 = none
    VoiceDef voice;
    std::vector<NpcCondDef> conds;
};

// An Item the player can pick up, carry above its head and drop (action button; PlayerAction).
struct ItemDef {
    int object = 0;
    std::string type, name;   // class ("Item"), itemName
    bool canKeep = false, disableAtStart = false;
    float radius = 0.5f;      // itemRadius: the pick-up line-of-sight rays' spread
    int spriteObject = -1;    // its SpriteRenderer's object (the pick-up / drop animation moves it)
    int colliderSlot = -1;    // its collider (ObjectCollider::slot), -1 = none
    float pickUpDuration = 0.5f;
    SampledCurve xCurve, yCurve; // xPickUpMotion / yPickUpMotion
    std::vector<int> enableOnDrop;
    std::vector<CallDef> onPickUp, onDrop;
};

// PlayerAction: where the player carries an item and how drops are placed.
struct PlayerActionDef {
    bool present = false;
    float carryX = 0, carryY = 0.8125f;  // pickedItemLocation, player-relative (as authored)
    std::string carryPath;               // its path from the player's Animator
    int carryTarget = -1;                // that AnimatorDef::Target on the player's Animator (it bobs), -1 = none
    float boatDropX = 0, boatDropY = 0;  // boatDropItemBaseTransform, player-relative (drops from the boat)
    SpritePlacement dropIndicator;       // relative to the drop position; sprite -1 = none
    std::vector<std::string> itemLayers; // itemMask: colliders that can be picked up
    std::vector<std::string> dropLayers; // dropCollidingMask | noDropItemsLayerMask
};

// A behaviour component on a scene object (an entry in one of the Area's per-kind vectors), for
// handling things by object: what reacts when the player touches an object's trigger, which
// components have a Start. Built at load.
struct ComponentRef {
    enum class Kind : uint8_t { TriggerEvent, Interaction, Collectible, ConditionChecker, Npc, Item };
    Kind kind;
    int index; // into triggerEvents / interactions / collectibles / conditionCheckers / npcs / items
};

struct ComponentRange {
    const ComponentRef *first, *last;
    const ComponentRef* begin() const { return first; }
    const ComponentRef* end() const { return last; }
};

// A ParticleSystem the port plays (ParticleSystem.Play, or on activation with playOnAwake): bursts
// and a steady rate of flat squares from a circle, flying outward, pushed by a constant force, slowed
// by limit-velocity, fading by color-over-lifetime alpha; world space.
struct ParticleBurstDef {
    float time = 0;               // s after Play
    float countMin = 0, countMax = 0;
    int cycles = 1;
    float interval = 0;           // s between cycles
};
struct ParticlesDef {
    int object = -1;              // where it emits (Area::objects); -1 = a prefab's (spawned under objects)
    bool playOnAwake = false;     // plays when its object becomes active
    bool loop = false;            // starts over after `duration` (until stopped)
    float duration = 0;           // s; emission past it stops (non-looping)
    float lifetimeMin = 0, lifetimeMax = 0, speedMin = 0, speedMax = 0;
    float size = 0;               // world units (square)
    // Start color: colors[0], or (several) a random point on the gradient with these keys (RGBA),
    // stepping to the next key instead of blending when colorsFixed.
    std::vector<float> colorTimes;
    std::vector<uint32_t> colors;
    bool colorsFixed = false;
    float shapeRadius = 0;        // Circle shape: start inside it, moving away from its center
    float rateMin = 0, rateMax = 0; // emission per second
    float forceX = 0, forceY = 0; // force over lifetime (units/s^2)
    std::vector<ParticleBurstDef> bursts;
    bool limitVelocity = false;   // limit velocity over lifetime:
    float limit = 0, dampen = 0;  //   speed over `limit` loses the `dampen` fraction (per 1/30 s)
    float drag = 0;               //   and drag (on the particle's area / quadratic in speed when dragBySize / dragByVelocity)
    bool dragBySize = false, dragByVelocity = false;
    std::vector<std::pair<float, float>> alpha; // color over lifetime: (normalized age, alpha) keys; empty = 1
    int sortLayer = 0, order = 0;
    bool localSpace = false;      // Local simulation: its particles move with the object they're under
    // Textured particles only (flat squares don't show it): start rotation (radians, counter-clockwise
    // on screen) and a constant spin (rad/s), each a random pick in [min, max] per particle.
    float rotationMin = 0, rotationMax = 0, spinMin = 0, spinMax = 0;
    // A texture sheet (Grid; empty = a flat square of `size`): each tile's sprite, rows from the top,
    // `tilesX` a row. A particle plays one random row once over its life: its frame is
    // floor(curve(age / lifetime) * frameMultiplier * tilesX), its curve a random blend of
    // frameCurveMin and frameCurveMax (frameOverTime's two curves).
    std::vector<int> frames;
    int tilesX = 0;
    float frameMultiplier = 1;
    std::vector<CurveKey> frameCurveMin, frameCurveMax;
};

// ParticleManager.SpawnParticle's types the port spawns (Unity's ParticleType names, in order).
enum class ParticleType : uint8_t { SmokePoof, MiniSparkle, COUNT };
constexpr const char* PARTICLE_TYPE_NAMES[] = {"SmokePoof", "MiniSparkle"};
static_assert(sizeof(PARTICLE_TYPE_NAMES) / sizeof(PARTICLE_TYPE_NAMES[0]) == (size_t)ParticleType::COUNT);

// KnotBox (the knot puzzle): line i joins node i and node i + 1 (the last back to the first). A
// line is bad when it crosses a ray collider other than its own two nodes', or a line that isn't
// next to it; the knot is solved with no bad lines.
struct KnotLineDef {
    int object = -1;              // its LineRenderer's object (drawn while active)
    float width = 0;              // world units
    int sortLayer = 0, order = 0;
};
struct KnotBoxDef {
    static constexpr int MAX_NODES = 64;
    int object = -1;              // the KnotBox (Update draws and checks the lines while it's active)
    std::vector<int> nodes;       // objects; their positions are the knot's points
    std::vector<KnotLineDef> lines; // as many as nodes
    uint32_t good = 0, bad = 0;   // line colors, RGBA
    std::vector<ObjectCollider> rayColliders; // what CheckLines' raycast hits (the nodes' trigger circles)
    struct Pos { int owner; float x, y; };
    std::vector<Pos> correct;     // SetToCorrectPositions: where each node's parent goes
    std::vector<int> particles;   // Area::particles; CheckParticles plays them when solved
};

// RuinsArrow (the ruins puzzle, Slider 7): when tiles 3-1 and 6-2 sit side by side (grid "31..62")
// its arrow shows on tile 2, pointing from tile 2 to tile 5 (where Slider 7 is buried), with a
// trail of sparkles toward it. Rods on tiles 1, 2, 3 and 6 light up (with their runes, sparkles and
// Artifact corner icons) for each assembled pair; CheckActiveRods is true while any is lit.
struct RuinsRodDef {
    int island = 0;               // its rodN: 1, 2, 3 or 6
    int object = -1;              // its SpriteRenderer's object
    int off = -1, on = -1;        // sprites
    int particles = -1;           // Area::particles: sparkles while lit
    int runes = -1;               // the runes' SpriteRenderer object (shown, with a white flash, while lit)
    float flashTime = 0.25f;      // FlashWhiteSprite: white this long, then normal this long
};
struct RuinsArrowDef {
    static constexpr int ARROWS = 6; // NNE, NE, ENE, ESE, SE, SSE
    int object = -1;              // the RuinsArrow
    int renderer = -1;            // its SpriteRenderer's object (a world sprite, authored off)
    int arrows[ARROWS] = {};      // sprites
    int complete = -1;            // sprite once Slider 7 is collected, -1 = none (shows nothing)
    int buriedSlider = -1;        // object shown when the arrow first appears
    std::vector<RuinsRodDef> rods;
    uint32_t flashColor = 0xe6e6e6ff; // the white flash's color (RGBA)
    int particles = -1;           // Area::particles: arrowParticles
    struct Trail {                // ParticleTrail: sparkles every `distance` from start to target
        int particles = -1;       // Area::particles (a prefab spawned at each point, under `object`)
        int object = -1, start = -1, target = -1;
        float distance = 2, delay = 0.1f, repeat = 8;
    } trail;
    struct MapIcon { int corner, button, icon; }; // RuinsMapIcons cornerN: ArtifactButtonDef (islandId) icons[icon]
    std::vector<MapIcon> mapIcons;
    bool bob = false;             // BobAnimation on the renderer's object (one pixel down / up)
    float bobLength = 3, bobDown = 0.5f, bobMiddle = 1.5f, bobUp = 2, bobOffset = 0;
};

// ChadJump, with the VillageChadJump on the same object (Chad, holding the Ruins Fragment): JumpStarter
// moves him onto a rock along two curves; once up, a slide of his own STile makes him tip, fall
// off and drop the item, and (VillageChadJump) other slides make him tip for a while.
// CatTrick (Cathy's cats): a cat item whose collider (on: not carried) overlaps the detection zone
// counts as returned for good (game/village/cat_trick); while 1 or 2 are back, "n/3" shows.
struct CatTrickDef {
    static constexpr int CATS = 3;
    int object = -1;                             // the CatTrick (its zone is placed with it)
    float zoneMinX = 0, zoneMinY = 0, zoneMaxX = 0, zoneMaxY = 0; // detectionZone, owner-relative
    int cats[CATS] = {-1, -1, -1};               // Area::items: mittens, autumn, tofu
    int textObjects[2] = {-1, -1};               // text and backgroundText (UpdateText switches them)
    TextBoxDef text;                             // where the count shows
    int spawns[CATS] = {-1, -1, -1};             // <cat>CollectedSpawnLoc's objects: where Load puts returned cats
};

struct ChadJumpDef {
    int object = -1;              // the ChadJump (its transform moves: start -> jump -> fall)
    int renderer = -1;            // npcRenderer's object (sorting order 1 from the jump until he lands)
    int animator = -1;            // npcAnimator: Area::animators index (exported: its object)
    int collider = -1;            // npcCollider: ObjectCollider::slot (off from the jump until the item lands)
    int item = -1;                // artifactItem: Area::items index, -1 = none
    int island = 0;               // islandId: the STile whose move makes him fall
    int owner = 0;                // start / jump / fall transforms: positions on this owner
    float startX = 0, startY = 0, jumpX = 0, jumpY = 0, fallX = 0, fallY = 0;
    SampledCurve xCurve, yCurve;  // xJumpMotion / yJumpMotion
    float jumpDuration = 1, fallDuration = 0.5f;
    std::vector<CallDef> onJumpFinish;
    bool tips = false;            // a VillageChadJump on the same object:
    float timeTipped = 2;         //   how long a slide tips him
    int tipNpc = -1;              //   chadNPC: Area::npcs index (types its current dialogue after a tip; exported: its object)
    int tipAnimator = -1;         //   chadAnimator: Area::animators index (exported: its object)
};

// RuinsSymbols (the ruins' runes by the hole the Ruins Fragment fills): FillInHole hides the hole
// and the symbols, ShufflePuzzle flashes them back one by one (FlashWhiteSprite).
struct RuinsSymbolsDef {
    static constexpr int SYMBOLS = 4;
    bool present = false;
    int object = -1;              // the RuinsSymbols (its Start)
    int symbols[SYMBOLS] = {};    // ruinSymbols: their SpriteRenderers' objects (world sprites)
    float flashTime[SYMBOLS] = {0.25f, 0.25f, 0.25f, 0.25f};
    uint32_t flashColor = 0xe6e6e6ff; // the white material's color (RGBA)
    int hole = -1;                // ruinsHole's object (a world sprite)
};

// CatAbductionController (VillageGrid's): the cat item, its Animator, the bubble that carries it off
// (CatBubbleController), the sine wave it rides (SinWaveAnimation) and where it lands.
struct CatAbductionDef {
    bool present = false;
    int item = -1;           // catItem: Area::items index
    int animator = -1;       // catAnimator: Area::animators index (on the item's sprite object)
    int bubble = -1;         // bubbleSpriteRenderer's object (a world sprite, authored off)
    int electricity = -1;    // electricitySpriteRenderer's object (likewise)
    int bubbleAnimator = -1; // bubbleAnimator: Area::animators index (on `bubble`)
    float sinX = 0, sinY = 0; // catSinWave's world position (its parent never moves)
    float sinA = 1, sinB = 1, sinC = 0, sinD = 0; // A sin(B t + C) + D
    float landingX = 0, landingY = 0; // catBeachLandingSpot (world)
    SampledCurve lerpCurve;
};

// The area's SGrid subclass and the objects its methods act on.
struct AreaGridDef {
    std::string type;          // e.g. "VillageGrid"
    int caveDoorEntrance = -1;  // VillageGrid.Explode shows it
    int caveDoorRocks = -1;     // and hides these
    RuinsSymbolsDef ruinsSymbols; // FillInHole / ShufflePuzzle
    int ruinsFragment = -1;     // Area::items index: CheckHole hides it (or, Caves first, puts it in the hole)
    int slider8Floor = -1;      // slider8FloorTransform: where CheckHole puts it
    int introDolly = -1;        // introCameraDolly: Area::cameraDollies index (OnHouseExit's intro), -1 = none
    int romeo = -1;             // romeoNPC: Area::npcs index (the intro turns him and walks him in), -1 = none
    CatAbductionDef catAbduction; // after the intro, the cat floats off to the beach
    int slider2Collectible = -1; // its object: VillageGrid.Start's Artifact tracker (until Slider 2 is collected)
};

// UITrackerManager's pins (UITracker): the sprites the port's trackers use, by its DefaultSprites
// names (Area::sprites, -1 = unset). The player's are the playerFull* set (MiniPlayerIcon's default, off).
struct UiTrackersDef {
    bool present = false;
    int playerFullBlackCircle = -1, playerFullBlackCircleEmpty = -1;
    int playerFullWhiteCircle = -1, playerFullWhiteCircleEmpty = -1;
    int circle3 = -1, circleEmpty = -1, circle1W = -1, circleEmptyW = -1;
    float uiPerPixel = 1; // Artifact UI px per sprite pixel (SetNativeSize; the port draws 1)
    bool trackHousingAccurately = false; // (not ported: refused at load)
};

// PlayerActionHints: the area's tutorial hints (shown by game/hints) and what fires its calls.
struct HintDef {
    std::string name;               // hintName (what calls refer to it by)
    std::string text;               // hintText, its <...> placeholders unreplaced
    std::vector<std::string> binds; // controlBinds: Unity Control names, one per placeholder in order
    bool canDisable = false;        // canDisableHint
    float delay = 0;                // timeUntilTrigger, s
    bool onLoad = false;            // triggerOnLoad
};
// StileMoveHint: calls when tile `stile` starts or ends a move (STile.onChangeMove; 0 = none) and,
// with anyTile, on every grid move (SGrid.OnGridMove).
struct TileMoveHintDef {
    int object = -1;
    int stile = 0;
    bool anyTile = false;
    std::vector<CallDef> onMove;
};
struct HintsDef {
    bool present = false;
    int object = -1;
    float fadeDuration = 0.5f; // UIHints' box / text fades, s
    std::vector<HintDef> list;
    // Its input events (Controls bindings). The port has no OpenArtifact press (the Artifact is always
    // on the bottom screen: onArtifactOpen never runs; it only dismisses "tab", which the port drops),
    // CycleEquip or AltViewHold (their calls must be empty: checked at load, not kept).
    std::vector<CallDef> onArtifactOpen, onMove, onAction;
    std::vector<TileMoveHintDef> tileMoves;
    int find(const std::string& name) const; // index in list, -1 if none
};

// CameraDolly: a cutscene camera rolling along a Cinemachine path, given as where the camera is over
// its duration (sampled: the path and its pathMovementCurve). Holding the action button skips it.
struct CameraDollyDef {
    int object = -1;
    float duration = 0;       // s
    float rate = 60;          // track samples per second
    std::vector<float> track; // x, y (world) at t = k / rate, from 0 to duration
    float holdToSkip = 1;     // holdDurationToSkip (s)
    SampledCurve holdCurve;   // holdAnimationCurve: the skip prompt's fill over the hold (0..1)

    // Where the camera is at t s (clamped to the track).
    void at(float t, float& x, float& y) const {
        const int n = (int)track.size() / 2;
        const float k = std::min(std::max(t * rate, 0.0f), (float)(n - 1));
        const int i = std::min((int)k, n - 2);
        const float f = k - (float)i;
        x = track[2 * i] + (track[2 * i + 2] - track[2 * i]) * f;
        y = track[2 * i + 1] + (track[2 * i + 3] - track[2 * i + 1]) * f;
    }
};

// CameraShake's Cinemachine noise (CinemachineBasicMultiChannelPerlin's profile): position channels,
// camera-relative x and y.
struct CameraShakeDef {
    struct Channel {
        float frequency[2] = {}, amplitude[2] = {}; // x, y
        bool constant[2] = {};                     // a cosine instead of Perlin noise
    };
    float frequencyGain = 1;
    std::vector<Channel> position; // empty: shakes don't move the camera
};

// What collides on one owner (an STile, or 0 = the world) in one band (its surface, or its house
// interiors), for physics' broadphase: its collision tilemaps and the object colliders whose objects
// belong to it as exported, and their bounds relative to the owner's center.
struct ColliderGroup {
    std::vector<int> layers;    // Area::collision indices
    std::vector<int> colliders; // Area::objectColliders indices
    float minX = 0, minY = 0, maxX = 0, maxY = 0;
    bool empty() const { return layers.empty() && colliders.empty(); }
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
    int animator = -1;       // Area::animators index of Player.playerAnimator (drives the body), -1 = none
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
    std::vector<uint8_t> iconsEnabled; // per icon: its Image on at the start (RuinsMapIcons switches some)
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
    // The solid object collider in collider slot `slot` (ObjectCollider::slot numbering), null for a
    // trigger's slot or none.
    const ObjectCollider* solidCollider(int slot) const {
        return slot >= 0 && slot < (int)objectColliders.size() ? &objectColliders[slot] : nullptr;
    }

    std::string name;  // e.g. "Village" (Area enum name: collectibles' area)
    int ppu = 16;
    int gridW = 0, gridH = 0;
    std::string targetGrid;
    int housingOffset = 0; // y offset of each STile's house interior
    uint8_t background[3] = {0, 0, 0}; // camera clear color (the void between/behind tiles)
    std::vector<STileDef> stiles;
    std::vector<Spawn> spawns;
    std::vector<CollisionLayer> collision;
    std::vector<MaterialLayer> materials;
    std::vector<std::string> footsteps; // per material: its surface ("grass": the "Footstep grass" sound)
    std::string footstepFallback;       // the surface off any material tile (MaterialSoundMapping's fallback)
    std::vector<std::vector<Polygon>> shapes; // collision shapes (each a list of polygons)
    struct ShapeBounds { float minX, minY, maxX, maxY; };
    std::vector<ShapeBounds> shapeBounds;     // per shape, relative to its cell (physics culls by them)
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
    std::vector<ItemDef> items;
    std::vector<KnotBoxDef> knotBoxes;
    std::vector<RuinsArrowDef> ruinsArrows;
    std::vector<ChadJumpDef> chadJumps;
    std::vector<CatTrickDef> catTricks;
    // DistanceBasedAmbience: an ambience that plays while its object's scene runs, its global
    // parameter set to the player's distance to the nearest active node.
    struct AmbienceDef {
        int object = -1;
        std::string name, parameter; // AudioManager's ambience; the FMOD parameter
        float defaultValue = 0;      // the parameter without a node (and once the scene ends)
        bool enabled = true;
        std::vector<int> nodes;      // objects
    };
    std::vector<AmbienceDef> ambiences;
    std::vector<ParticlesDef> particles;
    PlayerActionDef playerAction;
    std::vector<AnimatorControllerDef> animatorControllers;
    std::vector<AnimatorDef> animators;
    float fadeDuration = 1; // UIEffects.fadeDuration (s)
    float flashDuration = 1; // UIEffects.flashDuration (s): FlashWhite
    CameraShakeDef cameraShake;
    std::vector<CameraDollyDef> cameraDollies;
    AreaGridDef areaGrid;
    int collectible(const std::string& name) const; // index in collectibles of the area's own one named so, -1 if none
    // Each object's components (derived at load): componentRefs[componentStart[o] .. componentStart[o + 1]),
    // per object in the kind order TriggerEvent, Interaction, Collectible, ConditionChecker, Npc, Item.
    std::vector<int> componentStart;
    std::vector<ComponentRef> componentRefs;
    ComponentRange components(int object) const {
        const ComponentRef* base = componentRefs.data();
        return {base + componentStart[object], base + componentStart[object + 1]};
    }
    // Physics broadphase (derived at load): groups by owner islandId and band, at index
    // 2 * owner + band (band 1: below halfway to housingOffset, the house interiors; their bounds and
    // the surface's never overlap, so splitting keeps a tall box from covering both); and each
    // object's solid colliders: objectCollidersOf[objectColliderStart[o] .. objectColliderStart[o + 1]).
    std::vector<ColliderGroup> colliderGroups;
    std::vector<int> objectColliderStart, objectCollidersOf;
    PlayerDef player;
    ArtifactDef artifact;
    UiTrackersDef uiTrackers;
    HintsDef hints;
    // ParticleManager's prefabs by ParticleType: Area::particles indices, -1 = not exported.
    int particlePrefabs[(int)ParticleType::COUNT] = {-1, -1};
    std::vector<TileDef> tiles;
    std::vector<SpriteDef> sprites;
    std::vector<std::string> sheetNames;

    const STileDef* stile(int islandId) const;
    const Spawn* spawn(const char* spawnName) const;
    const std::string& pathOf(int object) const { return objects[object].path; }
    int findObject(const std::string& path) const; // index in objects, -1 if none
    int npcOn(int object) const; // index in npcs of the NPC on `object`, -1 if none
    int animatorOn(int object) const { return animatorByObject[object]; } // Area::animators index, -1 if none
    int animatorDrivenBy(int controllerObject) const; // the Animator an NPCAnimatorController object drives, -1 if none
    std::vector<int> animatorByObject; // per object (derived at load)
};

// Loads e.g. "village/village.json" through plat::readFile, then checks every index in it
// (tile -> sprite -> sheet, cell -> tile, collision cell -> shape, xform size), so a bad export
// fails here instead of reading out of bounds later.
bool loadArea(const char* path, Area& out, std::string* error = nullptr);
