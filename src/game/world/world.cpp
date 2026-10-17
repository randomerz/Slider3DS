#include "game/world/world.h"

#include <algorithm>
#include <cmath>

#include "game/events/events.h"
#include "game/physics/physics.h"

namespace {

void setTileObjects(World& world, const STileDef& s, bool active) {
    if (s.objectsObject >= 0) world.objects.setActive(world.area, s.objectsObject, active);
    if (s.tileMapsObject >= 0) world.objects.setActive(world.area, s.tileMapsObject, active);
}

} // namespace

void World::runStarts() {
    animators.enable(*this); // OnEnable comes before Start
    ruins.activate(*this);   // (rods' Awake, the arrow's Start)
    chad.activate(*this);
    finale.activate(*this); // (RuinsSymbols.Start)
    starts.update(*this);
}

void World::sendGridEvents() {
    if (grid.events().empty()) return;
    const std::vector<GridEvent> events = grid.takeEvents();
    grid.clearEvents(); // (a moved-from vector is valid but unspecified)
    ruins.gridEvents(*this, events);
    chad.gridEvents(*this, events);
    walks.gridEvents(*this, events);
    runStarts(); // (the arrow may show the buried Slider)
}

void World::effectDone(EffectDone done) {
    switch (done) {
    case EffectDone::None: break;
    case EffectDone::DollyEnd: {
        const bool wasIntro = dollies.current() == area.areaGrid.introDolly;
        dollies.end(*this);
        if (wasIntro) abduction.introFinished(); // (OnRollercoasterEnd: VillageGrid.OnVillageCutsceneEnd)
        break;
    }
    case EffectDone::VillageShuffle: finale.doShuffle(*this); break;
    }
}

void World::camera(float& x, float& y) const {
    if (dollies.camera(area, x, y)) return; // (the brain cuts to the dolly's camera: no shake there)
    x = player.x + effects.shakeX();
    y = player.y + effects.shakeY();
}

void World::dropLanded(DropDone done, int arg) {
    switch (done) {
    case DropDone::None: break;
    case DropDone::ChadFinishFall: chad.finishFall(*this, arg); break;
    }
}

World::World(const Area& a, const Settings& s) : area(a), settings(s) {
    state.newProfile("Cat"); // (until a new-game screen names the profile)
    effects.reset();
    grid.init(area);
    artifact.init(grid);
    reloadObjects();
}

void World::reloadObjects() {
    objects.init(area);
    grid.clearEvents(); // (a fresh load: nothing has happened yet)
    for (const STileDef& s : area.stiles) setTileObjects(*this, s, grid.tileActive(s.islandId));
    // SGrid.Start: collectibles already in the inventory stay picked up.
    for (const CollectibleDef& c : area.collectibles)
        if (state.hasCollectible(c.name, c.area)) objects.setActive(area, c.object, false);
    triggers.reset(area);
    interactables.clear();
    npcs.reset(area);
    walks.reset(area);
    items.reset(area);
    animators.reset(area);
    particles.reset();
    ruins.reset(area);
    chad.reset(area);
    finale.reset(area);
    intro.reset();
    abduction.reset();
    dollies.reset();
    player.locks = 0; // (what held the player is reset)
    artifactButtons.reset(area);
    artifact.canAddMoves = true; // UIArtifact.Init (EnableMovement)
    starts.reset(area);
    runStarts();
    finale.start(*this); // VillageGrid.Start (after every Awake; CheckHole may hide the fragment)
    abduction.start(*this); // CatAbductionController.Start
    runStarts();
    knots.update(*this);
}

void World::setTileActive(int islandId, bool active) {
    // What happened before goes out first: the tile's objects subscribe (OnEnable) only now, so
    // they mustn't hear it.
    sendGridEvents();
    grid.setTileActive(islandId, active);
    if (const STileDef* s = area.stile(islandId)) setTileObjects(*this, *s, active);
    runStarts(); // Start runs before the newly active objects' first update
    sendGridEvents(); // (SGrid.OnSTileEnabled)
    knots.update(*this);
}

bool World::setGrid(const std::string& ids) {
    const int stile = player.stile;
    const TileState* t = grid.tile(stile);
    const float ox = t ? t->posX : 0, oy = t ? t->posY : 0;
    if (!artifact.setGrid(grid, ids)) return false;
    finale.gridSet(*this);
    if (t) {
        player.x += (t->posX - ox) * grid.tileWidth();
        player.y += (t->posY - oy) * grid.tileWidth();
    }
    return true;
}

void World::placeAt(const Spawn& spawn) {
    float bx, by;
    grid.ownerBase(spawn.owner, bx, by);
    player.x = bx + spawn.x;
    player.y = by + spawn.y;
    player.onWater = spawn.inBoat;
    player.stile = grid.stileUnder(player.x, player.y);
}

void World::placeNear(float x, float y) {
    player.onWater = false;
    findFreeNear(*this, player.hitbox(area), x, y);
    player.x = x;
    player.y = y;
    player.stile = grid.stileUnder(x, y);
}

void World::update(float dt, const FrameInput& in, bool noclip) {
    float moveX = in.moveX, moveY = in.moveY;
    if (playerLocked()) moveX = moveY = 0; // (Player.SetCanMove(false))
    // Tiles slide; the player is parented to the tile underneath and rides along.
    const TileState* t = grid.tile(player.stile);
    const float ox = t ? t->posX : 0, oy = t ? t->posY : 0;
    chad.update(*this, dt); // (scripts' Update comes before the coroutine that ends moves)
    sendGridEvents(); // moves started since the last frame (the stylus), before their tiles slide
    artifact.update(grid, dt, player.stile);
    sendGridEvents(); // tiles arrived, queued moves started
    finale.checkButtons(*this); // (completions: the Artifact's buttons moved)
    if (t) {
        player.x += (t->posX - ox) * grid.tileWidth();
        player.y += (t->posY - oy) * grid.tileWidth();
    }

    if (moveX != 0) player.facingLeft = moveX < 0;
    if (moveX != 0 || moveY != 0) {
        player.lastMoveX = moveX;
        player.lastMoveY = moveY;
    }
    if (area.player.animator >= 0) { // Player.Update
        animators.setBool(*this, area.player.animator, "isRunning", moveX != 0 || moveY != 0);
        animators.setBool(*this, area.player.animator, "isOnWater", player.onWater);
    }
    float dx = 0, dy = 0;
    if (moveX != 0 || moveY != 0) {
        const float len = std::sqrt(moveX * moveX + moveY * moveY);
        dx = moveX / len * MOVE_SPEED * dt;
        dy = moveY / len * MOVE_SPEED * dt;
    }
    if (noclip) {
        player.x += dx;
        player.y += dy;
    } else {
        // Also with no input: Box2D pushes a resting player out of whatever it overlaps.
        moveWithCollision(*this, player.hitbox(area), player.x, player.y, dx, dy, player.onWater);
    }
    player.stile = grid.stileUnder(player.x, player.y, player.stile);
    items.update(*this, dt); // (PlayerAction.Update: the carried item follows, the drop spot)
    cats.update(*this);      // (CatTrick.Update: a cat dropped in the zone is back)
    abduction.update(*this, dt); // (CatAbductionController.Update + its sine wave's)

    // Start runs before an object's first update (here: before triggers can touch it), and again
    // for what calls activated. Each skips when nothing became active.
    runStarts();
    triggers.update(*this);
    runStarts();
    npcs.update(*this, dt);
    runStarts();
    walks.coroutines(*this, dt); // (coroutines resume after the scripts' Update, before the Animator)
    chad.coroutines(*this, dt);
    finale.coroutines(*this, dt);
    intro.coroutines(*this, dt);
    abduction.coroutines(*this, dt);
    dollies.update(*this, dt, in.actionHeld);
    runStarts(); // (the final Slider may have appeared)
    animators.update(*this, dt); // (Unity: the animation update follows the scripts' Update)
    items.follow(*this);          // (the carried item is a child of the item holder the Animator moves)
    ruins.update(*this, dt);
    particles.update(*this, dt);
    knots.update(*this); // (KnotBox.LateUpdate)
    const EffectCallbacks due = effects.update(area, dt, settings.screenShake, frame);
    effectDone(due.flashMiddle);
    effectDone(due.fadeEnd); // (UIEffects; CameraShake.Update, then the camera's noise in LateUpdate)
    frame++;
}

void World::offer(const Interactable& it) {
    if (std::find(interactables.begin(), interactables.end(), it) == interactables.end()) interactables.push_back(it);
}

void World::withdraw(const Interactable& it) {
    interactables.erase(std::remove(interactables.begin(), interactables.end(), it), interactables.end());
}

int World::priority(const Interactable& it) const {
    switch (it.kind) {
    case Interactable::Kind::Interaction: return area.interactions[it.index].priority;
    case Interactable::Kind::Npc: return 0; // NPCDialogueContext's priority isn't serialized
    }
    return 0;
}

bool World::showsPrompt(const Interactable& it) const {
    switch (it.kind) {
    case Interactable::Kind::Interaction: return true; // PlayerConditionals: IInteractable's default
    case Interactable::Kind::Npc: return npcs.showsPrompt(area, it.index);
    }
    return true;
}

void World::action() {
    if (playerLocked()) { // (PlayerAction: not while the player can't move)
        dollies.actionPressed();
        return;
    }
    const bool picked = items.tryPickUp(*this);
    const bool interacted = interact();
    if (!interacted && !picked) items.tryDrop(*this);
}

bool World::interact() {
    if (interactables.empty()) return false;
    bool interacted = false;
    int highest = priority(interactables[0]);
    for (const Interactable& it : interactables) highest = std::max(highest, priority(it));
    // Calls may change what's on offer (a door teleports the player away), so run a snapshot.
    const std::vector<Interactable> offered = interactables;
    for (const Interactable& it : offered) {
        if (priority(it) != highest) continue;
        switch (it.kind) {
        case Interactable::Kind::Interaction: {
            // PlayerConditionals.CheckCondition: with isCarryingItem, only while carrying the item;
            // InvokeSuccess then withdraws it if the item is gone.
            const InteractionDef& in = area.interactions[it.index];
            if (in.isCarryingItem && !items.carrying(area, in.itemName)) break;
            runCalls(*this, in.onSuccess);
            if (in.isCarryingItem && !items.carrying(area, in.itemName)) withdraw(it);
            interacted = true;
            break;
        }
        case Interactable::Kind::Npc:
            npcs.interact(*this, it.index);
            interacted = true; // (NPCDialogueContext.Interact returns true)
            break;
        }
    }
    return interacted;
}

bool World::promptShown() const {
    return std::any_of(interactables.begin(), interactables.end(), [&](const Interactable& it) { return showsPrompt(it); });
}

void World::worldTexts(std::vector<WorldText>& out) const {
    out.clear();
    cats.texts(*this, out);
}

void World::objectPosition(int object, float& x, float& y) const {
    objectBase(object, x, y);
    x += area.objects[object].x;
    y += area.objects[object].y;
}

void World::placeObject(int object, int owner, float x, float y) {
    float bx, by;
    grid.ownerBase(owner, bx, by);
    // Placement offsets add up the hierarchy: this object's own is relative to its parent's.
    const int parent = objects.parent(object);
    const float px = parent >= 0 ? objects.offsetX(parent) : 0, py = parent >= 0 ? objects.offsetY(parent) : 0;
    objects.setPlacement(area, object, owner, x - bx - area.objects[object].x - px, y - by - area.objects[object].y - py);
}

void World::objectBase(int object, float& x, float& y) const {
    grid.ownerBase(objects.owner(object), x, y);
    x += objects.offsetX(object);
    y += objects.offsetY(object);
}
