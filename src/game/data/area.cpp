#include "game/data/area.h"

#include <algorithm>
#include <cmath>

#include "game/data/json.h"
#include "platform/platform.h"

const STileDef* Area::stile(int islandId) const {
    for (const STileDef& s : stiles)
        if (s.islandId == islandId) return &s;
    return nullptr;
}

const Spawn* Area::spawn(const char* spawnName) const {
    for (const Spawn& s : spawns)
        if (s.name == spawnName) return &s;
    return nullptr;
}

// Area::componentStart / componentRefs from the per-kind vectors (objects already validated).
static void buildComponentIndex(Area& a) {
    using K = ComponentRef::Kind;
    std::vector<std::vector<ComponentRef>> byObject(a.objects.size());
    for (size_t i = 0; i < a.triggerEvents.size(); i++) byObject[a.triggerEvents[i].object].push_back({K::TriggerEvent, (int)i});
    for (size_t i = 0; i < a.interactions.size(); i++) byObject[a.interactions[i].object].push_back({K::Interaction, (int)i});
    for (size_t i = 0; i < a.collectibles.size(); i++) byObject[a.collectibles[i].object].push_back({K::Collectible, (int)i});
    for (size_t i = 0; i < a.conditionCheckers.size(); i++)
        byObject[a.conditionCheckers[i].object].push_back({K::ConditionChecker, (int)i});
    for (size_t i = 0; i < a.npcs.size(); i++) byObject[a.npcs[i].object].push_back({K::Npc, (int)i});
    for (size_t i = 0; i < a.items.size(); i++) byObject[a.items[i].object].push_back({K::Item, (int)i});
    a.componentStart.assign(1, 0);
    a.componentRefs.clear();
    for (const auto& refs : byObject) {
        a.componentRefs.insert(a.componentRefs.end(), refs.begin(), refs.end());
        a.componentStart.push_back((int)a.componentRefs.size());
    }
}

// Area::colliderGroups / objectColliderStart / objectCollidersOf (objects and owners already validated).
static void buildColliderIndex(Area& a) {
    int maxOwner = 0;
    for (const STileDef& s : a.stiles) maxOwner = std::max(maxOwner, s.islandId);
    a.colliderGroups.assign(2 * (maxOwner + 1), ColliderGroup());
    std::vector<bool> started(a.colliderGroups.size(), false);
    // Owner-relative y below this: a house interior.
    const float split = a.housingOffset * 0.5f;
    auto group = [&](int owner, float y0, float y1) { return 2 * owner + ((y0 + y1) * 0.5f < split ? 1 : 0); };
    auto grow = [&](int g, float x0, float y0, float x1, float y1) {
        ColliderGroup& c = a.colliderGroups[g];
        if (!started[g]) {
            c.minX = x0, c.minY = y0, c.maxX = x1, c.maxY = y1;
            started[g] = true;
            return;
        }
        c.minX = std::min(c.minX, x0), c.minY = std::min(c.minY, y0);
        c.maxX = std::max(c.maxX, x1), c.maxY = std::max(c.maxY, y1);
    };
    for (size_t i = 0; i < a.collision.size(); i++) {
        const CollisionLayer& L = a.collision[i];
        // (a cell's shape may poke one cell into its neighbours)
        const float x0 = L.originX + L.minX, y0 = L.originY + L.minY;
        const int g = group(L.owner, y0, y0 + L.h);
        a.colliderGroups[g].layers.push_back((int)i);
        grow(g, x0 - 1, y0 - 1, x0 + L.w + 1, y0 + L.h + 1);
    }
    std::vector<std::vector<int>> byObject(a.objects.size());
    for (size_t i = 0; i < a.objectColliders.size(); i++) {
        const ObjectCollider& c = a.objectColliders[i];
        const int g = group(c.owner, c.minY, c.maxY);
        a.colliderGroups[g].colliders.push_back((int)i);
        grow(g, c.minX, c.minY, c.maxX, c.maxY);
        byObject[c.object].push_back((int)i);
    }
    a.objectColliderStart.assign(1, 0);
    a.objectCollidersOf.clear();
    for (const auto& list : byObject) {
        a.objectCollidersOf.insert(a.objectCollidersOf.end(), list.begin(), list.end());
        a.objectColliderStart.push_back((int)a.objectCollidersOf.size());
    }
}

int Area::findObject(const std::string& path) const {
    for (size_t i = 0; i < objects.size(); i++)
        if (objects[i].path == path) return (int)i;
    return -1;
}

int Area::npcOn(int object) const {
    for (const ComponentRef& ref : components(object))
        if (ref.kind == ComponentRef::Kind::Npc) return ref.index;
    return -1;
}

const char* NpcEmoteDef::name(int emote) {
    static const char* names[Count] = {"", "Sparkle", "Exclamation", "Question", "Annoyed", "Sweat"};
    return emote >= 0 && emote < Count ? names[emote] : "";
}

int Area::collectible(const std::string& n) const {
    for (size_t i = 0; i < collectibles.size(); i++)
        if (collectibles[i].name == n && collectibles[i].area == name) return (int)i;
    return -1;
}

int Area::animatorDrivenBy(int controllerObject) const {
    if (controllerObject < 0) return -1; // (-1 marks the player's and emote Animators: nothing drives them by call)
    for (size_t i = 0; i < animators.size(); i++)
        if (animators[i].controllerObject == controllerObject) return (int)i;
    return -1;
}

int AnimatorControllerDef::stateIndex(const std::string& n) const {
    for (size_t i = 0; i < states.size(); i++)
        if (states[i].name == n) return (int)i;
    return -1;
}

int AnimatorControllerDef::paramIndex(const std::string& n) const {
    for (size_t i = 0; i < params.size(); i++)
        if (params[i] == n) return (int)i;
    return -1;
}

// An exported curve: SampledCurve::SAMPLES values (missing ones: linear 0..1).
static SampledCurve readCurve(const Json& j) {
    SampledCurve c;
    for (int k = 0; k < SampledCurve::SAMPLES; k++) c.s[k] = j.at(k).asFloat((float)k / (SampledCurve::SAMPLES - 1));
    return c;
}

int Anim::frameAt(float t) const {
    if (frames.empty()) return -1;
    if (length > 0) t = loop ? std::fmod(t, length) : std::min(t, length);
    if (t < 0) t += length;
    size_t i = 0;
    while (i + 1 < times.size() && times[i + 1] <= t) i++;
    return frames[i];
}

static bool fail(std::string* error, const std::string& msg) {
    if (error) *error = msg;
    return false;
}

// A sprite placement: "sprite", "pos", "sortY", "sortingLayerValue", "order", optional "flip", "anim".
static SpritePlacement readPlacement(const Json& j) {
    SpritePlacement p;
    p.sprite = j["sprite"].asInt(-1);
    p.anim = j["anim"].asInt(-1);
    p.x = j["pos"].at(0).asFloat();
    p.y = j["pos"].at(1).asFloat();
    p.sortY = j["sortY"].asFloat(p.y);
    p.sortLayer = j["sortingLayerValue"].asInt();
    p.order = j["order"].asInt();
    p.flags = (uint32_t)j["flip"].asInt() & 3u; // bit0 X, bit1 Y = plat::FLIP_X | FLIP_Y
    return p;
}

static CallDef::Method callMethod(const std::string& name) {
    using M = CallDef::Method;
#define SLIDER_CALL_MATCH(id, exported) \
    if (name == exported) return M::id;
    SLIDER_CALL_METHODS(SLIDER_CALL_MATCH)
#undef SLIDER_CALL_MATCH
    return M::Unsupported;
}

static bool isTeleport(CallDef::Method m) {
    using M = CallDef::Method;
    return m == M::TeleportHard || m == M::TeleportRelativePlayer || m == M::TeleportRelativeMe ||
           m == M::TeleportTransform;
}

static std::vector<AnimatorDef::Target> readTargets(const Json& j);

// Calls; teleport payloads go to out.teleports.
static std::vector<CallDef> readCalls(const Json& j, Area& out) {
    std::vector<CallDef> calls;
    for (const Json& c : j.items()) {
        CallDef d;
        d.name = c["method"].asString();
        d.method = callMethod(d.name);
        d.target = c["target"].asInt(-1);
        d.boolArg = c["bool"].asBool();
        d.intArg = c["int"].asInt();
        d.stringArg = c["string"].asString();
        if (d.method == CallDef::Method::CheckConditions) d.data = c["checker"].asInt(-1);
        if (d.method == CallDef::Method::SetAnimatorController) d.data = c["controller"].asInt(-1);
        if (d.method == CallDef::Method::KnotCheckParticles || d.method == CallDef::Method::KnotSetToCorrect) {
            if (c.has("knotBox")) d.data = c["knotBox"].asInt(-1);
            else d.method = CallDef::Method::Unsupported; // its knot box was left out (the export says so)
        }
        if (d.method == CallDef::Method::ChadJumpStarter || d.method == CallDef::Method::ChadSetIsOnRock) {
            if (c.has("chadJump")) d.data = c["chadJump"].asInt(-1);
            else d.method = CallDef::Method::Unsupported; // its ChadJump was left out (the export says so)
        }
        if (d.method == CallDef::Method::SetColliderEnabled) { // (colliders load first)
            if (c.has("collider")) d.data = c["collider"].asInt(-1) + (c["trigger"].asBool() ? (int)out.objectColliders.size() : 0);
            else d.method = CallDef::Method::Unsupported; // a collider the export left out (it warns)
        }
        d.objectArg = c["objectArg"].asInt(-1);
        if (isTeleport(d.method)) {
            TeleportDef t;
            t.owner = c["owner"].asInt();
            t.x = c["pos"].at(0).asFloat();
            t.y = c["pos"].at(1).asFloat();
            t.dx = c["dPos"].at(0).asFloat();
            t.dy = c["dPos"].at(1).asFloat();
            t.tpToHouse = c["tpToHouse"].asBool();
            t.hasGoTo = c.has("goToPos");
            t.goTo = c["goTo"].asInt(-1);
            t.goToOwner = c["goToOwner"].asInt();
            t.goToX = c["goToPos"].at(0).asFloat();
            t.goToY = c["goToPos"].at(1).asFloat();
            d.data = (int)out.teleports.size();
            out.teleports.push_back(t);
        }
        calls.push_back(std::move(d));
    }
    return calls;
}

static ConditionDef readCondition(const Json& c) {
    using T = ConditionDef::Type;
    ConditionDef d;
    const std::string& type = c["type"].asString();
    if (type == "item") {
        d.type = T::Item;
        d.name = c["item"].asString();
        d.itemArea = c["itemArea"].asString();
    } else if (type == "grid" || type == "noGrid" || type == "gridStationary") {
        d.type = type == "grid" ? T::Grid : type == "noGrid" ? T::NoGrid : T::GridStationary;
        d.pattern = c["pattern"].asString();
        for (const Json& t : c["stationaryTiles"].items()) d.stationaryTiles.push_back(t.asInt());
    } else if (type == "flag" || type == "noFlag") {
        d.type = type == "flag" ? T::Flag : T::NoFlag;
        d.name = c["flag"].asString();
    } else if (type == "playerCarryingItem") {
        d.type = T::PlayerCarryingItem;
        d.name = c["playerItem"].asString();
    } else if (type == "spec") {
        d.name = c["spec"].asString(); // unchecked specs stay Unsupported (false)
        d.specTarget = c["specTarget"].asInt(-1);
#define SLIDER_SPEC_MATCH(id, exported)        \
    if (d.name == exported) {                  \
        d.type = T::Spec;                      \
        d.spec = ConditionDef::Spec::id;       \
    }
        SLIDER_SPEC_CHECKS(SLIDER_SPEC_MATCH)
#undef SLIDER_SPEC_MATCH
        // A KnotBox / RuinsArrow spec names its component; without one (the exporter left it out) it
        // stays unchecked.
        if (d.type == T::Spec && (d.spec == ConditionDef::Spec::KnotSolved || d.spec == ConditionDef::Spec::KnotPartlySolved)) {
            d.data = c["knotBox"].asInt(-1);
            if (!c.has("knotBox")) d.type = T::Unsupported;
        }
        if (d.type == T::Spec && d.spec == ConditionDef::Spec::RuinsRodsOn) {
            d.data = c["ruinsArrow"].asInt(-1);
            if (!c.has("ruinsArrow")) d.type = T::Unsupported;
        }
        if (d.type == T::Spec && (d.spec == ConditionDef::Spec::ChadFell || d.spec == ConditionDef::Spec::ChadTipped)) {
            d.data = c["chadJump"].asInt(-1);
            if (!c.has("chadJump")) d.type = T::Unsupported;
        }
        if (d.type == T::Spec && (d.spec == ConditionDef::Spec::CatMittens || d.spec == ConditionDef::Spec::CatAutumn ||
                                  d.spec == ConditionDef::Spec::CatTofu)) {
            d.data = c["catTrick"].asInt(-1);
            if (!c.has("catTrick")) d.type = T::Unsupported;
        }
    }
    return d;
}

static std::vector<ConditionDef> readConditions(const Json& j) {
    std::vector<ConditionDef> conditions;
    for (const Json& d : j.items()) conditions.push_back(readCondition(d));
    return conditions;
}

static ConditionalsDef readConditionals(const Json& j, Area& out) {
    ConditionalsDef c;
    c.conditions = readConditions(j["conditions"]);
    c.onSuccess = readCalls(j["onSuccess"], out);
    c.onFail = readCalls(j["onFail"], out);
    return c;
}

static Hitbox readHitbox(const Json& j) {
    return {j["offset"].at(0).asFloat(), j["offset"].at(1).asFloat(), j["size"].at(0).asFloat(), j["size"].at(1).asFloat()};
}

// [r, g, b, a] 0-255 -> RGBA.
static uint32_t readColor(const Json& j) {
    uint32_t c = 0;
    for (int i = 0; i < 4; i++) c = (c << 8) | (uint32_t)(j.at(i).asInt(255) & 0xff);
    return c;
}

static void readObjectCollider(const Json& j, ObjectCollider& c);

static ParticlesDef readParticles(const Json& j) {
    ParticlesDef d;
    d.object = j["object"].asInt(-1);
    d.playOnAwake = j["playOnAwake"].asBool();
    d.loop = j["loop"].asBool();
    d.duration = j["duration"].asFloat();
    d.lifetimeMin = j["lifetime"].at(0).asFloat();
    d.lifetimeMax = j["lifetime"].at(1).asFloat();
    d.speedMin = j["speed"].at(0).asFloat();
    d.speedMax = j["speed"].at(1).asFloat();
    d.size = j["size"].asFloat();
    for (const Json& t : j["colorTimes"].items()) d.colorTimes.push_back(t.asFloat());
    for (const Json& c : j["colors"].items()) d.colors.push_back(readColor(c));
    d.colorsFixed = j["colorsFixed"].asBool();
    d.shapeRadius = j["shapeRadius"].asFloat();
    d.rateMin = j["rate"].at(0).asFloat();
    d.rateMax = j["rate"].at(1).asFloat();
    d.forceX = j["force"].at(0).asFloat();
    d.forceY = j["force"].at(1).asFloat();
    for (const Json& b : j["bursts"].items())
        d.bursts.push_back({b["time"].asFloat(), b["count"].at(0).asFloat(), b["count"].at(1).asFloat(),
                            b["cycles"].asInt(1), b["interval"].asFloat()});
    if (j.has("limitVelocity")) {
        const Json& lv = j["limitVelocity"];
        d.limitVelocity = true;
        d.limit = lv["limit"].asFloat();
        d.dampen = lv["dampen"].asFloat();
        d.drag = lv["drag"].asFloat();
        d.dragBySize = lv["dragBySize"].asBool();
        d.dragByVelocity = lv["dragByVelocity"].asBool();
    }
    for (const Json& k : j["alpha"].items()) d.alpha.push_back({k.at(0).asFloat(), k.at(1).asFloat()});
    d.sortLayer = j["sortingLayerValue"].asInt();
    d.order = j["order"].asInt();
    return d;
}

static KnotBoxDef readKnotBox(const Json& j) {
    KnotBoxDef d;
    d.object = j["object"].asInt(-1);
    for (const Json& n : j["nodes"].items()) d.nodes.push_back(n.asInt(-1));
    for (const Json& l : j["lines"].items())
        d.lines.push_back({l["object"].asInt(-1), l["width"].asFloat(), l["sortingLayerValue"].asInt(), l["order"].asInt()});
    d.good = readColor(j["good"]);
    d.bad = readColor(j["bad"]);
    for (const Json& c : j["rayColliders"].items()) {
        ObjectCollider oc;
        readObjectCollider(c, oc);
        d.rayColliders.push_back(std::move(oc));
    }
    for (const Json& c : j["correct"].items())
        d.correct.push_back({c["owner"].asInt(), c["pos"].at(0).asFloat(), c["pos"].at(1).asFloat()});
    for (const Json& p : j["particles"].items()) d.particles.push_back(p.asInt(-1));
    return d;
}

static RuinsArrowDef readRuinsArrow(const Json& j) {
    RuinsArrowDef d;
    d.object = j["object"].asInt(-1);
    d.renderer = j["renderer"].asInt(-1);
    for (int i = 0; i < RuinsArrowDef::ARROWS; i++) d.arrows[i] = j["arrows"].at(i).asInt(-2);
    if (j["arrows"].size() != (size_t)RuinsArrowDef::ARROWS) d.arrows[0] = -2; // (fails validation)
    d.complete = j["complete"].asInt(-1);
    d.buriedSlider = j["buriedSlider"].asInt(-1);
    for (const Json& r : j["rods"].items())
        d.rods.push_back({r["island"].asInt(), r["object"].asInt(-1), r["off"].asInt(-1), r["on"].asInt(-1),
                          r["particles"].asInt(-1), r["runes"].asInt(-1), r["flashTime"].asFloat(0.25f)});
    d.flashColor = readColor(j["flashColor"]);
    d.particles = j["particles"].asInt(-1);
    const Json& t = j["trail"];
    d.trail.particles = t["particles"].asInt(-1);
    d.trail.object = t["object"].asInt(-1);
    d.trail.start = t["start"].asInt(-1);
    d.trail.target = t["target"].asInt(-1);
    d.trail.distance = t["distance"].asFloat(2);
    d.trail.delay = t["delay"].asFloat(0.1f);
    d.trail.repeat = t["repeat"].asFloat(8);
    for (const Json& m : j["mapIcons"].items())
        d.mapIcons.push_back({m["corner"].asInt(), m["button"].asInt(), m["icon"].asInt(-1)});
    if (j.has("bob")) {
        const Json& b = j["bob"];
        d.bob = true;
        d.bobLength = b["length"].asFloat(3);
        d.bobDown = b["down"].asFloat(0.5f);
        d.bobMiddle = b["middle"].asFloat(1.5f);
        d.bobUp = b["up"].asFloat(2);
        d.bobOffset = b["offset"].asFloat();
    }
    return d;
}

static ChadJumpDef readChadJump(const Json& j, Area& out) {
    ChadJumpDef d;
    d.object = j["object"].asInt(-1);
    d.renderer = j["renderer"].asInt(-1);
    d.animator = j["animator"].asInt(-1);
    d.collider = j["collider"].asInt(-1);
    d.item = j["item"].asInt(-1);
    d.island = j["island"].asInt();
    d.owner = j["owner"].asInt();
    d.startX = j["start"].at(0).asFloat();
    d.startY = j["start"].at(1).asFloat();
    d.jumpX = j["jump"].at(0).asFloat();
    d.jumpY = j["jump"].at(1).asFloat();
    d.fallX = j["fall"].at(0).asFloat();
    d.fallY = j["fall"].at(1).asFloat();
    d.xCurve = readCurve(j["xCurve"]);
    d.yCurve = readCurve(j["yCurve"]);
    d.jumpDuration = j["jumpDuration"].asFloat(1);
    d.fallDuration = j["fallDuration"].asFloat(0.5f);
    d.onJumpFinish = readCalls(j["onJumpFinish"], out);
    if (j.has("tip")) {
        const Json& t = j["tip"];
        d.tips = true;
        d.timeTipped = t["time"].asFloat(2);
        d.tipNpc = t["npc"].asInt(-1);
        d.tipAnimator = t["animator"].asInt(-1);
    }
    return d;
}

static void readObjectCollider(const Json& j, ObjectCollider& c) {
    c.object = j["object"].asInt(-1);
    c.owner = j["owner"].asInt();
    c.enabled = j["enabled"].asBool(true);
    c.layer = j["layer"].asString();
    const std::string& kind = j["kind"].asString();
    auto readPoints = [](const Json& pts) {
        Polygon p;
        for (const Json& v : pts.items()) p.push_back(v.asFloat());
        return p;
    };
    if (kind == "circle") {
        c.kind = ObjectCollider::Kind::Circle;
        c.cx = j["center"].at(0).asFloat();
        c.cy = j["center"].at(1).asFloat();
        c.radius = j["radius"].asFloat();
        c.minX = c.cx - c.radius;
        c.maxX = c.cx + c.radius;
        c.minY = c.cy - c.radius;
        c.maxY = c.cy + c.radius;
        return;
    }
    if (kind == "edge") {
        c.kind = ObjectCollider::Kind::Edge;
        c.paths.push_back(readPoints(j["points"]));
    } else {
        c.kind = ObjectCollider::Kind::Poly; // "none" (unsupported shape) loads as no paths
        for (const Json& path : j["paths"].items()) c.paths.push_back(readPoints(path));
    }
    bool first = true;
    for (const Polygon& p : c.paths) {
        for (size_t i = 0; i + 1 < p.size(); i += 2) {
            c.minX = first ? p[i] : std::min(c.minX, p[i]);
            c.maxX = first ? p[i] : std::max(c.maxX, p[i]);
            c.minY = first ? p[i + 1] : std::min(c.minY, p[i + 1]);
            c.maxY = first ? p[i + 1] : std::max(c.maxY, p[i + 1]);
            first = false;
        }
    }
}

static TextBoxDef readTextBox(const Json& t) {
    TextBoxDef b;
    b.minX = t["box"].at(0).asFloat();
    b.minY = t["box"].at(1).asFloat();
    b.maxX = t["box"].at(2).asFloat();
    b.maxY = t["box"].at(3).asFloat();
    b.lineAdvance = t["lineAdvance"].asFloat(0.43f);
    b.wordSpacing = t["wordSpacing"].asFloat();
    b.charSpacing = t["charSpacing"].asFloat();
    b.sortLayer = t["sortingLayerValue"].asInt();
    b.order = t["order"].asInt();
    return b;
}

static CatTrickDef readCatTrick(const Json& j) {
    CatTrickDef d;
    d.object = j["object"].asInt(-1);
    const Json& z = j["zone"];
    d.zoneMinX = z.at(0).asFloat();
    d.zoneMinY = z.at(1).asFloat();
    d.zoneMaxX = z.at(2).asFloat();
    d.zoneMaxY = z.at(3).asFloat();
    for (int i = 0; i < CatTrickDef::CATS; i++) d.cats[i] = j["cats"].at(i).asInt(-1);
    for (int i = 0; i < 2; i++) d.textObjects[i] = j["textObjects"].at(i).asInt(-1);
    d.text = readTextBox(j["text"]);
    return d;
}

static NpcDef readNpc(const Json& j, Area& out) {
    NpcDef n;
    n.object = j["object"].asInt(-1);
    n.name = j["name"].asString();
    n.facingLeft = j["facingLeft"].asBool();
    n.text = readTextBox(j["text"]);
    n.pingObject = j["pingObject"].asInt(-1);
    if (j.has("animator")) {
        const Json& a = j["animator"];
        n.animator = (int)out.animators.size();
        out.animators.push_back({a["object"].asInt(-1), a["controller"].asInt(-1), a["keepState"].asBool(),
                                 a["controllerObject"].asInt(-1), readTargets(a["targets"]), {}});
    }
    if (j.has("emote")) {
        const Json& e = j["emote"];
        n.emote.object = e["object"].asInt(-1);
        n.emote.place = readPlacement(e);
        n.emote.localX = e["localX"].asFloat();
        n.emote.facesRight = e["facesRight"].asBool();
        n.emote.npcSprite = e["npcSprite"].asInt(-1);
        n.emote.npcSpriteFlipped = e["npcSpriteFlipped"].asBool();
        n.emote.animator = (int)out.animators.size();
        out.animators.push_back({n.emote.object, e["controller"].asInt(-1), e["keepState"].asBool(), -1, {}, {}});
    }
    for (const Json& c : j["conds"].items()) {
        NpcCondDef d;
        d.conditions = readConditions(c["conditions"]);
        d.anyOf = c["anyOf"].asBool();
        d.alwaysStartFromBeginning = c["alwaysStartFromBeginning"].asBool();
        d.emote = (uint8_t)c["emote"].asInt();
        d.animationOnEnter = c["animationOnEnter"].asString();
        d.onEnter = readCalls(c["onConditionalEnter"], out);
        d.onExhausted = readCalls(c["onDialogueChainExhausted"], out);
        for (const Json& l : c["dialogue"].items())
            d.dialogue.push_back({l["text"].asString(), l["delay"].asFloat(0.5f), l["waitForAction"].asBool(),
                                  l["manual"].asBool(), l["noRepeatEvents"].asBool(), l["dontInterrupt"].asBool(),
                                  (uint8_t)l["emoteOnStart"].asInt(), (uint8_t)l["emoteOnLeave"].asInt(),
                                  l["animationOnStart"].asString(), l["animationOnLeave"].asString(),
                                  readCalls(l["onStart"], out), readCalls(l["onEnd"], out)});
        n.conds.push_back(std::move(d));
    }
    return n;
}

static std::vector<AnimatorTransitionDef> readTransitions(const Json& j) {
    std::vector<AnimatorTransitionDef> out;
    for (const Json& t : j.items()) {
        AnimatorTransitionDef d;
        d.to = t["to"].asInt(-1);
        d.exitTime = t["exitTime"].asFloat(-1);
        for (const Json& c : t["conds"].items()) d.conds.push_back({c.at(0).asInt(-1), c.at(1).asBool()});
        out.push_back(std::move(d));
    }
    return out;
}

static std::vector<AnimatorDef::Target> readTargets(const Json& j) {
    std::vector<AnimatorDef::Target> targets;
    for (const Json& t : j.items())
        targets.push_back({t["path"].asString(), t["object"].asInt(-1), t["local"].at(0).asFloat(), t["local"].at(1).asFloat()});
    return targets;
}

static AnimatorControllerDef readController(const Json& j) {
    AnimatorControllerDef c;
    c.name = j["name"].asString();
    for (const Json& p : j["params"].items()) {
        c.params.push_back(p["name"].asString());
        c.paramDefaults.push_back(p["default"].asBool() ? 1 : 0);
    }
    c.defaultState = j["default"].asInt();
    for (const Json& s : j["states"].items()) {
        AnimatorStateDef st;
        st.name = s["name"].asString();
        st.anim = s["anim"].asInt(-1);
        st.speed = s["speed"].asFloat(1);
        st.transitions = readTransitions(s["transitions"]);
        st.length = s["length"].asFloat();
        st.loop = s["loop"].asBool(true);
        st.writeDefaults = s["writeDefaults"].asBool(true);
        for (const Json& cj : s["moves"].items()) {
            MoveCurveDef mc;
            mc.path = cj["path"].asString();
            for (int axis = 0; axis < 2; axis++) {
                const Json& k = cj[axis ? "y" : "x"];
                std::vector<CurveKey>& keys = axis ? mc.y : mc.x;
                for (size_t i = 0; i + 3 < k.size(); i += 4)
                    keys.push_back({k.at(i).asFloat(), k.at(i + 1).asFloat(), k.at(i + 2).asFloat(), k.at(i + 3).asFloat()});
            }
            st.moves.push_back(std::move(mc));
        }
        c.states.push_back(std::move(st));
    }
    c.anyState = readTransitions(j["any"]);
    return c;
}

static UiPart readPart(const Json& j) {
    UiPart p;
    p.sprite = j["sprite"].asInt(-1);
    p.x = j["rect"].at(0).asInt();
    p.y = j["rect"].at(1).asInt();
    p.w = j["rect"].at(2).asInt();
    p.h = j["rect"].at(3).asInt();
    return p;
}

static void readArtifact(const Json& j, ArtifactDef& a) {
    a.present = !j.isNull();
    if (!a.present) return;
    a.buttonSpacing = j["buttonSpacing"].asInt(37);
    a.frame = readPart(j["frame"]);
    a.background = readPart(j["background"]);
    for (const Json& u : j["underlays"].items()) a.underlays.push_back(readPart(u));
    a.pressed = readPart(j["parts"]["pressed"]);
    a.highlight = readPart(j["parts"]["highlight"]);
    a.hover = readPart(j["parts"]["hover"]);
    a.empty = j["empty"].asInt(-1);
    a.blank = j["blank"].asInt(-1);
    a.hoverIcon = j["hoverIcon"].asInt(-1);
    a.pressedDrop = j["pressedDrop"].asInt(1);
    a.nudgeDistance = j["nudge"]["distance"].asFloat(2);
    a.nudgeDuration = j["nudge"]["duration"].asFloat(0.2f);
    for (const Json& b : j["buttons"].items()) {
        ArtifactButtonDef d;
        d.islandId = b["islandId"].asInt();
        d.island = b["island"].asInt(-1);
        d.completed = b["completed"].asInt(-1);
        for (const Json& icon : b["icons"].items()) {
            d.icons.push_back(readPart(icon));
            d.iconsEnabled.push_back(icon["enabled"].asBool(true) ? 1 : 0);
        }
        a.buttons.push_back(std::move(d));
    }
}

static bool validate(const Area& a, std::string* error) {
    if (a.gridW <= 0 || a.gridH <= 0 || (int)a.stiles.size() != a.gridW * a.gridH)
        return fail(error, "grid: expected " + std::to_string(a.gridW * a.gridH) + " stiles");
    std::vector<bool> slotUsed(a.gridW * a.gridH, false);
    for (const STileDef& s : a.stiles) {
        if (s.islandId <= 0 || s.islandId >= 64 || s.width <= 0 || s.x < 0 || s.x >= a.gridW || s.y < 0 || s.y >= a.gridH ||
            slotUsed[s.y * a.gridW + s.x])
            return fail(error, "stile " + std::to_string(s.islandId) + ": bad id, width or slot");
        slotUsed[s.y * a.gridW + s.x] = true;
    }
    auto ownerOk = [&](int owner) { return owner == 0 || a.stile(owner) != nullptr; };
    for (const TileLayer& L : a.layers)
        if (!ownerOk(L.owner)) return fail(error, "tilemap " + L.path + ": unknown owner STile");
    for (const CollisionLayer& C : a.collision)
        if (!ownerOk(C.owner)) return fail(error, "tilemap " + C.path + ": unknown owner STile");
    for (const Spawn& sp : a.spawns)
        if (!ownerOk(sp.owner)) return fail(error, "spawn " + sp.name + ": unknown owner STile");
    auto spriteIndexOk = [&](int s) { return s >= 0 && s < (int)a.sprites.size(); };
    auto animOk = [&](int i) { return i == -1 || (i >= 0 && i < (int)a.anims.size()); };
    auto objectOk = [&](int i) { return i >= 0 && i < (int)a.objects.size(); };
    // World sprites are drawn 1:1 (SpriteSheets draws native pixels), so they must match the area's ppu.
    auto placementOk = [&](const SpritePlacement& p) {
        return spriteIndexOk(p.sprite) && animOk(p.anim) && a.sprites[p.sprite].ppu == (float)a.ppu;
    };
    for (size_t i = 0; i < a.objects.size(); i++) {
        if (a.objects[i].parent < -1 || a.objects[i].parent >= (int)i)
            return fail(error, "object " + a.objects[i].path + ": parent must come before it");
        if (!ownerOk(a.objects[i].owner)) return fail(error, "object " + a.objects[i].path + ": unknown owner STile");
    }
    // Components are placed relative to their object's owner (ObjectStates tracks it at runtime).
    auto sameOwner = [&](int object, int owner) { return objectOk(object) && a.objects[object].owner == owner; };
    for (const Anim& an : a.anims) {
        if (an.frames.empty() || an.frames.size() != an.times.size())
            return fail(error, "anim " + an.name + ": frames/times mismatch");
        for (int f : an.frames)
            if (!spriteIndexOk(f) || a.sprites[f].ppu != (float)a.ppu) return fail(error, "anim " + an.name + ": bad sprite");
    }
    for (const WorldSprite& ws : a.worldSprites) {
        if (!objectOk(ws.object)) return fail(error, "world sprite: bad object index");
        if (!sameOwner(ws.object, ws.owner)) return fail(error, "world sprite " + a.pathOf(ws.object) + ": owner isn't its object's");
        if (!placementOk(ws.place))
            return fail(error, "world sprite " + a.pathOf(ws.object) + ": bad sprite, anim or ppu");
    }
    for (const ObjectCollider& c : a.objectColliders) {
        if (!objectOk(c.object)) return fail(error, "collider: bad object index");
        if (!sameOwner(c.object, c.owner)) return fail(error, "collider " + a.pathOf(c.object) + ": owner isn't its object's");
        for (const Polygon& p : c.paths)
            if (p.size() % 2 != 0) return fail(error, "collider " + a.pathOf(c.object) + ": odd point list");
    }
    for (const AnimatorControllerDef& c : a.animatorControllers) {
        const int n = (int)c.states.size(), params = (int)c.params.size();
        bool ok = c.paramDefaults.size() == c.params.size() && (n == 0 || (c.defaultState >= 0 && c.defaultState < n));
        auto transitionsOk = [&](const std::vector<AnimatorTransitionDef>& ts) {
            for (const AnimatorTransitionDef& t : ts) {
                if (t.to < 0 || t.to >= n) return false;
                for (const auto& k : t.conds)
                    if (k.first < 0 || k.first >= params) return false;
            }
            return true;
        };
        for (const AnimatorStateDef& s : c.states) ok = ok && animOk(s.anim) && transitionsOk(s.transitions);
        if (!ok || !transitionsOk(c.anyState)) return fail(error, "animator controller " + c.name + ": bad state, transition or anim");
    }
    for (const AnimatorDef& an : a.animators)
        if ((an.object != -1 && !objectOk(an.object)) || an.controller < 0 || an.controller >= (int)a.animatorControllers.size() ||
            (an.controllerObject != -1 && !objectOk(an.controllerObject)))
            return fail(error, "animator: bad object or controller");
    for (const AnimatorDef& an : a.animators)
        for (const AnimatorDef::Target& t : an.targets)
            if (t.object != -1 && !objectOk(t.object)) return fail(error, "animator: bad target " + t.path);
    // The player's item holder: found if named; nothing else inside the player is moved by the port.
    if (a.playerAction.present && !a.playerAction.carryPath.empty() && a.playerAction.carryTarget < 0)
        return fail(error, "player action: carryPath " + a.playerAction.carryPath + " isn't a target of the player's Animator");
    if (a.player.animator >= 0) {
        const std::vector<AnimatorDef::Target>& targets = a.animators[a.player.animator].targets;
        for (int t = 0; t < (int)targets.size(); t++)
            if (targets[t].object == -1 && t != a.playerAction.carryTarget)
                return fail(error, "player animator: target " + targets[t].path + " inside the player isn't moved by the port");
    }
    std::vector<bool> isNpc(a.objects.size(), false);
    for (const NpcDef& n : a.npcs)
        if (objectOk(n.object)) isNpc[n.object] = true;
    auto callsOk = [&](const std::vector<CallDef>& calls) {
        using M = CallDef::Method;
        for (const CallDef& c : calls) {
            if ((c.target != -1 && !objectOk(c.target)) || (c.objectArg != -1 && !objectOk(c.objectArg))) return false;
            if ((c.method == M::SetActive || c.method == M::SetFlipX) && c.target == -1) return false;
            if ((c.method == M::NpcTriggerEnter || c.method == M::NpcTriggerExit || c.method == M::NpcTypeCurrent ||
                 c.method == M::NpcTeleport) &&
                (c.target == -1 || !isNpc[c.target]))
                return false;
            if (c.method == M::NpcTeleport && c.objectArg == -1) return false;
            if ((c.method == M::AnimSetBoolTrue || c.method == M::AnimSetBoolFalse) && a.animatorDrivenBy(c.target) < 0)
                return false;
            if (c.method == M::VillageExplode && (!objectOk(a.areaGrid.caveDoorEntrance) || !objectOk(a.areaGrid.caveDoorRocks)))
                return false;
            if (c.method == M::SetColliderEnabled) {
                const int nSolid = (int)a.objectColliders.size();
                if (c.data < 0 || c.data >= nSolid + (int)a.triggerColliders.size()) return false;
                const ObjectCollider& col = c.data < nSolid ? a.objectColliders[c.data] : a.triggerColliders[c.data - nSolid];
                if (col.object != c.target) return false;
            }
            if (c.method == M::SetAnimatorController &&
                (c.target == -1 || a.animatorByObject[c.target] < 0 || c.data < 0 || c.data >= (int)a.animatorControllers.size()))
                return false;
            if (c.method == M::CheckConditions && (c.data < 0 || c.data >= (int)a.conditionCheckers.size())) return false;
            if ((c.method == M::KnotCheckParticles || c.method == M::KnotSetToCorrect) &&
                (c.data < 0 || c.data >= (int)a.knotBoxes.size() || a.knotBoxes[c.data].object != c.target))
                return false;
            if ((c.method == M::ChadJumpStarter || c.method == M::ChadSetIsOnRock) &&
                (c.data < 0 || c.data >= (int)a.chadJumps.size() || a.chadJumps[c.data].object != c.target ||
                 (c.method == M::ChadSetIsOnRock && !a.chadJumps[c.data].tips)))
                return false;
            if (isTeleport(c.method)) {
                if (c.data < 0 || c.data >= (int)a.teleports.size()) return false;
                const TeleportDef& t = a.teleports[c.data];
                if (!ownerOk(t.owner) || !ownerOk(t.goToOwner)) return false;
                if (c.method == M::TeleportTransform && !t.hasGoTo) return false;
                if (t.goTo != -1 && !objectOk(t.goTo)) return false;
            }
        }
        return true;
    };
    for (const ObjectCollider& c : a.triggerColliders)
        if (!sameOwner(c.object, c.owner)) return fail(error, "trigger collider: bad object or owner");
    for (const STileDef& s : a.stiles)
        if ((s.objectsObject != -1 && !objectOk(s.objectsObject)) || (s.tileMapsObject != -1 && !objectOk(s.tileMapsObject)))
            return fail(error, "stile " + std::to_string(s.islandId) + ": bad objects / allTileMaps index");
    for (const ConditionCheckerDef& cc : a.conditionCheckers)
        if (!objectOk(cc.object) || !callsOk(cc.conditionals.onSuccess) || !callsOk(cc.conditionals.onFail))
            return fail(error, "condition checker: bad object or call");
    for (const InteractionDef& in : a.interactions)
        if (!objectOk(in.object) || !callsOk(in.onSuccess)) return fail(error, "interaction: bad object or call");
    for (const TriggerEventDef& te : a.triggerEvents)
        if (!objectOk(te.object) || !callsOk(te.onEnter) || !callsOk(te.onExit))
            return fail(error, "trigger event: bad object or call");
    for (const CollectibleDef& c : a.collectibles)
        if (!objectOk(c.object) || (c.sprite != -1 && !spriteIndexOk(c.sprite)) || !callsOk(c.onCollect))
            return fail(error, "collectible " + c.name + ": bad object, sprite or call");
    for (const NpcDef& n : a.npcs) {
        bool ok = objectOk(n.object) && (n.pingObject == -1 || objectOk(n.pingObject)) &&
                  n.text.minX < n.text.maxX && n.text.minY < n.text.maxY;
        const NpcEmoteDef& e = n.emote;
        if (e.object != -1) {
            ok = ok && objectOk(e.object) && (e.npcSprite == -1 || objectOk(e.npcSprite)) &&
                 (e.place.sprite == -1 || placementOk(e.place));
        }
        auto emoteOk = [&](uint8_t v) { return v < NpcEmoteDef::Count; };
        for (const NpcCondDef& c : n.conds) {
            ok = ok && callsOk(c.onEnter) && callsOk(c.onExhausted) && emoteOk(c.emote);
            for (const DialogueDef& d : c.dialogue)
                ok = ok && callsOk(d.onStart) && callsOk(d.onEnd) && emoteOk(d.emoteOnStart) && emoteOk(d.emoteOnLeave);
        }
        if (!ok) return fail(error, "npc " + n.name + ": bad object, ping, emote, text box or call");
    }
    const int colliderSlots = (int)(a.objectColliders.size() + a.triggerColliders.size());
    for (const ItemDef& it : a.items) {
        bool ok = objectOk(it.object) && (it.spriteObject == -1 || objectOk(it.spriteObject)) &&
                  it.colliderSlot >= -1 && it.colliderSlot < colliderSlots && it.pickUpDuration > 0 &&
                  callsOk(it.onPickUp) && callsOk(it.onDrop);
        for (int o : it.enableOnDrop) ok = ok && objectOk(o);
        if (!ok) return fail(error, "item " + it.name + ": bad object, collider or call");
    }
    auto conditionsOk = [&](const std::vector<ConditionDef>& conditions) {
        using S = ConditionDef::Spec;
        for (const ConditionDef& d : conditions) {
            if (d.type != ConditionDef::Type::Spec) continue;
            if ((d.spec == S::KnotSolved || d.spec == S::KnotPartlySolved) &&
                (d.data < 0 || d.data >= (int)a.knotBoxes.size() || a.knotBoxes[d.data].object != d.specTarget))
                return false;
            if (d.spec == S::RuinsRodsOn &&
                (d.data < 0 || d.data >= (int)a.ruinsArrows.size() || a.ruinsArrows[d.data].object != d.specTarget))
                return false;
            if ((d.spec == S::ChadFell || d.spec == S::ChadTipped) &&
                (d.data < 0 || d.data >= (int)a.chadJumps.size() || a.chadJumps[d.data].object != d.specTarget ||
                 (d.spec == S::ChadTipped && !a.chadJumps[d.data].tips)))
                return false;
            if ((d.spec == S::CatMittens || d.spec == S::CatAutumn || d.spec == S::CatTofu) &&
                (d.data < 0 || d.data >= (int)a.catTricks.size() || a.catTricks[d.data].object != d.specTarget))
                return false;
        }
        return true;
    };
    for (const ConditionCheckerDef& cc : a.conditionCheckers)
        if (!conditionsOk(cc.conditionals.conditions)) return fail(error, "condition checker: bad spec target");
    for (const NpcDef& n : a.npcs)
        for (const NpcCondDef& c : n.conds)
            if (!conditionsOk(c.conditions)) return fail(error, "npc " + n.name + ": bad spec target");
    for (const ParticlesDef& pd : a.particles) {
        bool ok = objectOk(pd.object) && pd.lifetimeMin <= pd.lifetimeMax && pd.speedMin <= pd.speedMax &&
                  pd.rateMin >= 0 && pd.rateMin <= pd.rateMax && pd.size >= 0 && pd.duration >= 0 &&
                  !(pd.loop && pd.duration <= 0) && !pd.colors.empty() && pd.colors.size() == pd.colorTimes.size();
        for (size_t i = 1; i < pd.colorTimes.size(); i++) ok = ok && pd.colorTimes[i - 1] <= pd.colorTimes[i];
        for (const ParticleBurstDef& b : pd.bursts) ok = ok && b.countMin <= b.countMax && b.cycles >= 1 && b.interval >= 0;
        if (!ok) return fail(error, "particles " + (objectOk(pd.object) ? a.pathOf(pd.object) : std::string("?")) +
                                        ": bad object, ranges, colors or bursts");
    }
    auto particlesOk = [&](int i) { return i >= 0 && i < (int)a.particles.size(); };
    for (const KnotBoxDef& k : a.knotBoxes) {
        bool ok = objectOk(k.object) && k.nodes.size() >= 3 && k.nodes.size() <= (size_t)KnotBoxDef::MAX_NODES &&
                  k.lines.size() == k.nodes.size() &&
                  (k.correct.empty() || k.correct.size() == k.nodes.size());
        for (int n : k.nodes) ok = ok && objectOk(n) && a.objects[n].parent >= 0; // (SetToCorrectPositions moves the parent)
        for (const KnotLineDef& l : k.lines) ok = ok && objectOk(l.object) && l.width > 0;
        for (const ObjectCollider& c : k.rayColliders) ok = ok && sameOwner(c.object, c.owner);
        for (const KnotBoxDef::Pos& p : k.correct) ok = ok && ownerOk(p.owner);
        for (int pd : k.particles) ok = ok && particlesOk(pd);
        if (!ok) return fail(error, "knot box: bad object, node, line, ray collider, position or particles");
    }
    std::vector<bool> worldSpriteOn(a.objects.size(), false); // objects with a world sprite
    for (const WorldSprite& ws : a.worldSprites) worldSpriteOn[ws.object] = true;
    for (const RuinsArrowDef& r : a.ruinsArrows) {
        auto spriteOrNone = [&](int sp) { return sp == -1 || (spriteIndexOk(sp) && a.sprites[sp].ppu == (float)a.ppu); };
        bool ok = objectOk(r.object) && objectOk(r.renderer) && worldSpriteOn[r.renderer] && objectOk(r.buriedSlider) &&
                  spriteOrNone(r.complete) && particlesOk(r.particles) && particlesOk(r.trail.particles) &&
                  objectOk(r.trail.object) && objectOk(r.trail.start) && objectOk(r.trail.target) && r.trail.distance > 0 &&
                  r.trail.delay >= 0 && r.trail.repeat > 0 && (!r.bob || r.bobLength > 0);
        for (int sp : r.arrows) ok = ok && sp != -1 && spriteOrNone(sp);
        for (const RuinsRodDef& rod : r.rods)
            ok = ok && a.stile(rod.island) && objectOk(rod.object) && worldSpriteOn[rod.object] && spriteOrNone(rod.off) &&
                 spriteOrNone(rod.on) && rod.off != -1 && rod.on != -1 && particlesOk(rod.particles) && objectOk(rod.runes) &&
                 worldSpriteOn[rod.runes] && rod.flashTime >= 0;
        for (const RuinsArrowDef::MapIcon& m : r.mapIcons) {
            const ArtifactButtonDef* b = a.artifact.button(m.button);
            ok = ok && b && m.icon >= 0 && m.icon < (int)b->icons.size();
        }
        if (!ok) return fail(error, "ruins arrow: bad object, sprite, rod, particles, trail or map icon");
    }
    if (const RuinsSymbolsDef& rs = a.areaGrid.ruinsSymbols; rs.present) {
        bool ok = objectOk(rs.object) && objectOk(rs.hole) && worldSpriteOn[rs.hole];
        for (int k = 0; k < RuinsSymbolsDef::SYMBOLS; k++)
            ok = ok && objectOk(rs.symbols[k]) && worldSpriteOn[rs.symbols[k]] && rs.flashTime[k] >= 0;
        if (!ok) return fail(error, "ruins symbols: bad object, hole, symbol or flash time");
    }
    if (a.areaGrid.ruinsFragment < -1 || a.areaGrid.ruinsFragment >= (int)a.items.size())
        return fail(error, "area grid: bad ruinsFragment item");
    if (a.areaGrid.slider8Floor != -1 && !objectOk(a.areaGrid.slider8Floor)) return fail(error, "area grid: bad slider8Floor");
    for (const ChadJumpDef& c : a.chadJumps) {
        const bool ok = objectOk(c.object) && objectOk(c.renderer) && worldSpriteOn[c.renderer] && c.animator >= 0 &&
                        c.animator < (int)a.animators.size() && c.collider >= 0 && c.collider < (int)a.objectColliders.size() &&
                        a.objectColliders[c.collider].object == c.object && c.item >= -1 && c.item < (int)a.items.size() &&
                        a.stile(c.island) && ownerOk(c.owner) && c.jumpDuration > 0 && c.fallDuration > 0 &&
                        callsOk(c.onJumpFinish) &&
                        (!c.tips || (c.timeTipped >= 0 && c.tipNpc >= 0 && c.tipNpc < (int)a.npcs.size() && c.tipAnimator >= 0 &&
                                     c.tipAnimator < (int)a.animators.size()));
        if (!ok) return fail(error, "chad jump: bad object, renderer, animator, collider, item, island, owner or call");
    }
    for (const CatTrickDef& c : a.catTricks) {
        bool ok = objectOk(c.object) && objectOk(c.textObjects[0]) && objectOk(c.textObjects[1]) &&
                  c.zoneMinX < c.zoneMaxX && c.zoneMinY < c.zoneMaxY;
        for (int cat : c.cats) // (the zone tests each cat's collider)
            ok = ok && cat >= 0 && cat < (int)a.items.size() && a.solidCollider(a.items[cat].colliderSlot);
        if (!ok) return fail(error, "cat trick: bad object, zone, cat or text object");
    }
    if (a.playerAction.dropIndicator.sprite != -1 && !placementOk(a.playerAction.dropIndicator))
        return fail(error, "player action: bad drop indicator sprite");
    for (const SpritePlacement* p : {&a.player.body, &a.player.boat, &a.player.actionIndicator})
        if (p->sprite != -1 && !placementOk(*p)) return fail(error, "player: bad sprite, anim or ppu");
    for (const TileDef& t : a.tiles)
        for (int f : t.frames)
            if (spriteIndexOk(f) && a.sprites[f].ppu != (float)a.ppu) return fail(error, "tile: sprite ppu differs from the area's");
    if (a.artifact.present) {
        const ArtifactDef& d = a.artifact;
        auto spriteOk = [&](int s) { return s >= 0 && s < (int)a.sprites.size(); };
        bool ok = spriteOk(d.frame.sprite) && spriteOk(d.background.sprite) && spriteOk(d.pressed.sprite) &&
                  spriteOk(d.highlight.sprite) && spriteOk(d.hover.sprite) && spriteOk(d.empty) &&
                  spriteOk(d.blank) && spriteOk(d.hoverIcon) && d.buttonSpacing > 0;
        for (const UiPart& u : d.underlays) ok = ok && spriteOk(u.sprite);
        for (const ArtifactButtonDef& b : d.buttons) {
            ok = ok && a.stile(b.islandId) && spriteOk(b.island) && spriteOk(b.completed);
            for (const UiPart& icon : b.icons) ok = ok && spriteOk(icon.sprite);
            ok = ok && b.iconsEnabled.size() == b.icons.size();
        }
        for (const STileDef& s : a.stiles) ok = ok && d.button(s.islandId);
        if (!ok) return fail(error, "artifact: bad sprite index or button set");
    }
    for (const SpriteDef& sp : a.sprites)
        if (sp.outlineWidth > 0 ? sp.sheet != -1 : (sp.sheet < 0 || sp.sheet >= (int)a.sheetNames.size()))
            return fail(error, "sprite: bad sheet index");
    for (const TileDef& t : a.tiles)
        for (int f : t.frames)
            if (f < 0 || f >= (int)a.sprites.size()) return fail(error, "tile: bad sprite index");
    for (const TileLayer& L : a.layers) {
        for (int16_t c : L.cells)
            if (c < -1 || c >= (int)a.tiles.size()) return fail(error, "tilemap " + L.path + ": bad tile index");
        if (!L.xform.empty() && L.xform.size() != L.cells.size())
            return fail(error, "tilemap " + L.path + ": xform size mismatch");
    }
    for (const CollisionLayer& C : a.collision)
        for (int16_t c : C.cells)
            if (c < -1 || c >= (int)a.shapes.size()) return fail(error, "tilemap " + C.path + ": bad shape index");
    return true;
}

bool loadArea(const char* path, Area& out, std::string* error) {
    std::string text;
    if (!plat::readFile(path, text))
        return fail(error, std::string("can't read ") + path);

    Json doc;
    std::string parseError;
    if (!Json::parse(text, doc, &parseError))
        return fail(error, std::string(path) + ": " + parseError);
    text.clear();
    text.shrink_to_fit();

    out = Area();
    out.name = doc["area"].asString();
    out.ppu = doc["ppu"].asInt(16);
    out.gridW = doc["grid"]["width"].asInt();
    out.gridH = doc["grid"]["height"].asInt();
    out.targetGrid = doc["grid"]["targetGrid"].asString();
    out.housingOffset = doc["grid"]["housingOffset"].asInt();

    for (const Json& s : doc["stiles"].items())
        out.stiles.push_back({s["islandId"].asInt(), s["name"].asString(), s["x"].asInt(), s["y"].asInt(),
                              s["width"].asInt(), s["isTileActive"].asBool(), s["objects"].asInt(-1),
                              s["allTileMaps"].asInt(-1)});

    for (int i = 0; i < 3; i++) out.background[i] = (uint8_t)doc["background"].at(i).asInt();

    for (const Json& o : doc["objects"].items())
        out.objects.push_back({o["path"].asString(), o["parent"].asInt(-1), o["active"].asBool(true), o["owner"].asInt(),
                               o["pos"].at(0).asFloat(), o["pos"].at(1).asFloat()});

    for (const Json& s : doc["worldSprites"].items()) {
        WorldSprite ws;
        ws.object = s["object"].asInt(-1);
        ws.owner = s["owner"].asInt();
        ws.place = readPlacement(s);
        ws.animStart = s["animStart"].asFloat();
        ws.animRandom = s["animRandom"].asFloat();
        ws.enabled = s["enabled"].asBool(true);
        out.worldSprites.push_back(std::move(ws));
    }

    for (const Json& a : doc["anims"].items()) {
        Anim an;
        an.name = a["name"].asString();
        for (const Json& f : a["frames"].items()) an.frames.push_back(f.asInt());
        for (const Json& t : a["times"].items()) an.times.push_back(t.asFloat());
        an.length = a["length"].asFloat();
        an.loop = a["loop"].asBool(true);
        out.anims.push_back(std::move(an));
    }

    for (const Json& c : doc["objectColliders"].items()) {
        ObjectCollider oc;
        readObjectCollider(c, oc);
        out.objectColliders.push_back(std::move(oc));
    }

    for (const Json& c : doc["triggerColliders"].items()) {
        ObjectCollider oc;
        readObjectCollider(c, oc);
        out.triggerColliders.push_back(std::move(oc));
    }
    for (size_t i = 0; i < out.objectColliders.size(); i++) out.objectColliders[i].slot = (int)i;
    for (size_t i = 0; i < out.triggerColliders.size(); i++)
        out.triggerColliders[i].slot = (int)(out.objectColliders.size() + i);
    for (const Json& c : doc["conditionCheckers"].items())
        out.conditionCheckers.push_back({c["object"].asInt(-1), c["enabled"].asBool(true), c["checkOnStart"].asBool(),
                                         readConditionals(c, out)});
    for (const Json& c : doc["interactions"].items())
        out.interactions.push_back({c["object"].asInt(-1), c["addToOnAction"].asBool(), c["excludeBoat"].asBool(),
                                    c["excludeLand"].asBool(), c["isCarryingItem"].asBool(), c["itemName"].asString(),
                                    c["priority"].asInt(), readCalls(c["onSuccess"], out)});
    for (const Json& c : doc["triggerEvents"].items())
        out.triggerEvents.push_back({c["object"].asInt(-1), c["onPlayerEnter"].asBool(true), readCalls(c["onEnter"], out),
                                     readCalls(c["onExit"], out)});
    for (const Json& c : doc["collectibles"].items())
        out.collectibles.push_back({c["object"].asInt(-1), c["name"].asString(), c["area"].asString(),
                                    c["disableAtStart"].asBool(), c["sprite"].asInt(-1), readCalls(c["onCollect"], out)});
    for (const Json& c : doc["animatorControllers"].items()) out.animatorControllers.push_back(readController(c));
    for (const Json& n : doc["npcs"].items()) out.npcs.push_back(readNpc(n, out));
    for (const Json& j : doc["items"].items()) {
        ItemDef d;
        d.object = j["object"].asInt(-1);
        d.type = j["type"].asString();
        d.name = j["name"].asString();
        d.canKeep = j["canKeep"].asBool();
        d.disableAtStart = j["disableAtStart"].asBool();
        d.radius = j["radius"].asFloat(0.5f);
        d.spriteObject = j["spriteObject"].asInt(-1);
        const int collider = j["collider"].asInt(-1);
        d.colliderSlot = collider < 0 ? -1 : collider + (j["colliderTrigger"].asBool() ? (int)out.objectColliders.size() : 0);
        d.pickUpDuration = j["pickUpDuration"].asFloat(0.5f);
        d.xCurve = readCurve(j["xCurve"]);
        d.yCurve = readCurve(j["yCurve"]);
        for (const Json& o : j["enableOnDrop"].items()) d.enableOnDrop.push_back(o.asInt(-1));
        d.onPickUp = readCalls(j["onPickUp"], out);
        d.onDrop = readCalls(j["onDrop"], out);
        out.items.push_back(std::move(d));
    }
    for (const Json& j : doc["knotBoxes"].items()) out.knotBoxes.push_back(readKnotBox(j));
    for (const Json& j : doc["ruinsArrows"].items()) out.ruinsArrows.push_back(readRuinsArrow(j));
    for (const Json& j : doc["chadJumps"].items()) out.chadJumps.push_back(readChadJump(j, out));
    for (const Json& j : doc["catTricks"].items()) out.catTricks.push_back(readCatTrick(j));
    for (const Json& j : doc["particles"].items()) out.particles.push_back(readParticles(j));
    if (doc.has("playerAction")) {
        const Json& pa = doc["playerAction"];
        PlayerActionDef& d = out.playerAction;
        d.present = true;
        d.carryX = pa["carryPos"].at(0).asFloat();
        d.carryY = pa["carryPos"].at(1).asFloat();
        d.carryPath = pa["carryPath"].asString();
        d.boatDropX = pa["boatDropBase"].at(0).asFloat();
        d.boatDropY = pa["boatDropBase"].at(1).asFloat();
        if (pa.has("dropIndicator")) d.dropIndicator = readPlacement(pa["dropIndicator"]);
        for (const Json& n : pa["itemLayers"].items()) d.itemLayers.push_back(n.asString());
        for (const Json& n : pa["dropLayers"].items()) d.dropLayers.push_back(n.asString());
    }
    out.fadeDuration = doc["fadeDuration"].asFloat(1);
    out.areaGrid.type = doc["areaGrid"]["type"].asString();
    out.areaGrid.caveDoorEntrance = doc["areaGrid"]["caveDoorEntrance"].asInt(-1);
    out.areaGrid.caveDoorRocks = doc["areaGrid"]["caveDoorRocks"].asInt(-1);
    if (const Json& rs = doc["areaGrid"]["ruinsSymbols"]; !rs.isNull()) {
        RuinsSymbolsDef& d = out.areaGrid.ruinsSymbols;
        d.present = true;
        d.object = rs["object"].asInt(-1);
        for (int k = 0; k < RuinsSymbolsDef::SYMBOLS; k++) {
            d.symbols[k] = rs["symbols"].at(k).asInt(-1);
            d.flashTime[k] = rs["flashTime"].at(k).asFloat(0.25f);
        }
        if (!rs["flashColor"].isNull()) d.flashColor = readColor(rs["flashColor"]);
        d.hole = rs["hole"].asInt(-1);
    }
    out.areaGrid.ruinsFragment = doc["areaGrid"]["ruinsFragment"].asInt(-1);
    out.areaGrid.slider8Floor = doc["areaGrid"]["slider8Floor"].asInt(-1);
    out.flashDuration = doc["flashDuration"].asFloat(1);
    out.cameraShake.frequencyGain = doc["cameraShake"]["frequencyGain"].asFloat(1);
    for (const Json& ch : doc["cameraShake"]["position"].items()) {
        CameraShakeDef::Channel c;
        for (int axis = 0; axis < 2; axis++) {
            c.frequency[axis] = ch.at(axis * 3).asFloat();
            c.amplitude[axis] = ch.at(axis * 3 + 1).asFloat();
            c.constant[axis] = ch.at(axis * 3 + 2).asFloat() != 0;
        }
        out.cameraShake.position.push_back(c);
    }

    if (doc.has("player")) {
        if (doc["player"].has("body")) out.player.body = readPlacement(doc["player"]["body"]);
        if (doc["player"].has("boat")) out.player.boat = readPlacement(doc["player"]["boat"]);
        if (doc["player"].has("actionIndicator")) out.player.actionIndicator = readPlacement(doc["player"]["actionIndicator"]);
        out.player.footBox = readHitbox(doc["player"]["footBox"]);
        if (doc["player"].has("animator")) {
            out.player.animator = (int)out.animators.size();
            out.animators.push_back({-1, doc["player"]["animator"].asInt(-1), false, -1,
                                     readTargets(doc["player"]["animatorTargets"]["targets"]), {}});
        }
        out.player.boatBox = readHitbox(doc["player"]["boatBox"]);
    }

    readArtifact(doc["artifact"], out.artifact);

    for (const Json& s : doc["spawns"].items())
        out.spawns.push_back({s["name"].asString(), s["owner"].asInt(), s["pos"].at(0).asFloat(),
                              s["pos"].at(1).asFloat(), s["inBoat"].asBool()});

    for (const Json& shape : doc["collisionShapes"].items()) {
        std::vector<Polygon> polys;
        for (const Json& poly : shape.items()) {
            Polygon p;
            for (const Json& v : poly.items()) p.push_back(v.asFloat());
            polys.push_back(std::move(p));
        }
        Area::ShapeBounds b = {1e30f, 1e30f, -1e30f, -1e30f};
        for (const Polygon& p : polys)
            for (size_t k = 0; k + 1 < p.size(); k += 2) {
                b.minX = std::min(b.minX, p[k]), b.maxX = std::max(b.maxX, p[k]);
                b.minY = std::min(b.minY, p[k + 1]), b.maxY = std::max(b.maxY, p[k + 1]);
            }
        out.shapes.push_back(std::move(polys));
        out.shapeBounds.push_back(b);
    }

    for (const Json& sh : doc["sheets"].items())
        out.sheetNames.push_back(sh["name"].asString());

    for (const Json& s : doc["sprites"].items()) {
        const Json& r = s["rect"];
        SpriteDef d{s["sheet"].asInt(), r.at(0).asInt(), r.at(1).asInt(), r.at(2).asInt(), r.at(3).asInt(),
                    s["pivot"].at(0).asFloat(), s["pivot"].at(1).asFloat(), s["ppu"].asFloat(16)};
        if (s.has("outline")) {
            const Json& o = s["outline"];
            d.outlineWidth = o.at(4).asInt();
            d.outlineColor = (uint32_t)o.at(0).asInt() << 24 | (uint32_t)o.at(1).asInt() << 16 | (uint32_t)o.at(2).asInt() << 8 |
                             (uint32_t)o.at(3).asInt();
        }
        out.sprites.push_back(d);
    }

    for (const Json& t : doc["tiles"].items()) {
        TileDef def;
        for (const Json& f : t["frames"].items()) def.frames.push_back(f.asInt());
        def.fps = t["fps"].asFloat();
        out.tiles.push_back(std::move(def));
    }

    for (const Json& t : doc["tilemaps"].items()) {
        if (t["kind"].asString() != "collision" || !t["enabled"].asBool() || t["trigger"].asBool() ||
            t["size"].at(0).asInt() == 0)
            continue;
        CollisionLayer C;
        C.path = t["path"].asString();
        C.owner = t["owner"].asInt();
        C.active = t["active"].asBool(true);
        const std::string& mode = t["mode"].asString();
        C.mode = mode == "onFoot" ? CollisionMode::OnFoot : mode == "onWater" ? CollisionMode::OnWater : CollisionMode::Always;
        C.originX = t["origin"].at(0).asFloat();
        C.originY = t["origin"].at(1).asFloat();
        C.minX = t["cellMin"].at(0).asInt();
        C.minY = t["cellMin"].at(1).asInt();
        C.w = t["size"].at(0).asInt();
        C.h = t["size"].at(1).asInt();
        C.cells.reserve(C.w * C.h);
        for (const Json& c : t["cells"].items()) C.cells.push_back((int16_t)c.asInt());
        if ((int)C.cells.size() != C.w * C.h)
            return fail(error, "tilemap " + C.path + ": cell count mismatch");
        out.collision.push_back(std::move(C));
    }

    for (const Json& t : doc["tilemaps"].items()) {
        if (t["kind"].asString() != "visual" || !t["render"]["enabled"].asBool() || t["size"].at(0).asInt() == 0)
            continue;
        TileLayer L;
        L.path = t["path"].asString();
        L.owner = t["owner"].asInt();
        L.active = t["active"].asBool(true);
        L.originX = t["origin"].at(0).asFloat();
        L.originY = t["origin"].at(1).asFloat();
        L.anchorX = t["tileAnchor"].at(0).asFloat(0.5f);
        L.anchorY = t["tileAnchor"].at(1).asFloat(0.5f);
        L.sortLayer = t["render"]["sortingLayerValue"].asInt();
        L.order = t["render"]["order"].asInt();
        L.minX = t["cellMin"].at(0).asInt();
        L.minY = t["cellMin"].at(1).asInt();
        L.w = t["size"].at(0).asInt();
        L.h = t["size"].at(1).asInt();
        L.cells.reserve(L.w * L.h);
        for (const Json& c : t["cells"].items()) L.cells.push_back((int16_t)c.asInt());
        if (t.has("xform"))
            for (const Json& x : t["xform"].items()) L.xform.push_back((uint8_t)x.asInt());
        if ((int)L.cells.size() != L.w * L.h)
            return fail(error, "tilemap " + L.path + ": cell count mismatch");
        out.layers.push_back(std::move(L));
    }

    // Animators by object (before validation: calls are checked against it).
    out.animatorByObject.assign(out.objects.size(), -1);
    for (size_t i = 0; i < out.animators.size(); i++) {
        const int o = out.animators[i].object;
        if (o >= 0 && o < (int)out.objects.size()) out.animatorByObject[o] = (int)i;
    }
    // Transform curves: numbered, then each Animator's target for each (by path). A state without its
    // clip's length (older / hand-written data) takes its sprite clip's.
    int curves = 0;
    for (AnimatorControllerDef& c : out.animatorControllers)
        for (AnimatorStateDef& st : c.states) {
            if (st.length <= 0 && st.anim >= 0 && st.anim < (int)out.anims.size()) {
                st.length = out.anims[st.anim].length;
                st.loop = out.anims[st.anim].loop;
            }
            for (MoveCurveDef& mc : st.moves) mc.id = curves++;
        }
    for (AnimatorDef& an : out.animators) {
        an.curveTarget.assign(curves, -1);
        for (const AnimatorControllerDef& c : out.animatorControllers)
            for (const AnimatorStateDef& st : c.states)
                for (const MoveCurveDef& mc : st.moves)
                    for (int t = 0; t < (int)an.targets.size(); t++)
                        if (an.targets[t].path == mc.path) an.curveTarget[mc.id] = t;
    }
    if (out.player.animator >= 0 && out.playerAction.present && !out.playerAction.carryPath.empty()) {
        const std::vector<AnimatorDef::Target>& targets = out.animators[out.player.animator].targets;
        for (int i = 0; i < (int)targets.size(); i++)
            if (targets[i].path == out.playerAction.carryPath) out.playerAction.carryTarget = i;
    }
    for (ChadJumpDef& c : out.chadJumps) { // exported as objects
        auto animatorOf = [&](int o) { return o >= 0 && o < (int)out.objects.size() ? out.animatorByObject[o] : -1; };
        c.animator = animatorOf(c.animator);
        c.tipAnimator = animatorOf(c.tipAnimator);
        int npc = -1; // (npcOn's component index isn't built yet)
        for (int i = 0; i < (int)out.npcs.size(); i++)
            if (out.npcs[i].object == c.tipNpc) npc = i;
        c.tipNpc = c.tipNpc >= 0 ? npc : -1;
    }
    if (!validate(out, error)) return false;
    buildComponentIndex(out);
    buildColliderIndex(out);

    // Unity draws by (sorting layer, order); stable keeps file order within equal keys.
    std::stable_sort(out.layers.begin(), out.layers.end(), [](const TileLayer& a, const TileLayer& b) {
        return a.sortLayer != b.sortLayer ? a.sortLayer < b.sortLayer : a.order < b.order;
    });
    return true;
}
