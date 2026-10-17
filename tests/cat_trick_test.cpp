// CatTrick (Cathy's cats): cats dropped in the detection zone count as returned, for good; specs and
// the "n/3" text follow.
#include <string>
#include <vector>

#include "game/events/conditions.h"
#include "game/village/cat_trick.h"
#include "game/world/world.h"
#include "test.h"

namespace {

constexpr float DT = 1.0f / 60.0f;

// Tiles 5 (Autumn) and 9 (Cathy's house, Mittens, Tofu) collected; nothing returned yet.
World catWorld() {
    World w(villageArea(), DEFAULT_SETTINGS);
    w.setTileActive(5, true);
    w.setTileActive(9, true);
    w.reloadObjects();
    return w;
}

void zoneCenter(const World& w, float& x, float& y) {
    const CatTrickDef& c = w.area.catTricks[0];
    float bx, by;
    w.objectBase(c.object, bx, by);
    x = bx + (c.zoneMinX + c.zoneMaxX) / 2;
    y = by + (c.zoneMinY + c.zoneMaxY) / 2;
}

// The count is shown: its text objects on, and World offers its text (what AreaView draws).
bool textOn(const World& w) {
    const CatTrickDef& c = w.area.catTricks[0];
    std::vector<WorldText> texts;
    w.worldTexts(texts);
    const bool objectsOn = w.objects.activeInHierarchy(c.textObjects[0]) && w.objects.activeInHierarchy(c.textObjects[1]);
    check(objectsOn == (texts.size() == 1), "the text objects and the world text agree");
    return objectsOn && texts.size() == 1 &&
           std::string(texts[0].text) == village::CatTricks::text(village::CatTricks::count(w));
}

// Cathy's current conditional, by its first line's start ("" = none).
std::string cathySays(const World& w) {
    for (int i = 0; i < (int)w.area.npcs.size(); i++) {
        if (w.area.npcs[i].name != "Cathy") continue;
        const int cond = w.npcs.currentCond(i);
        if (cond < 0 || w.area.npcs[i].conds[cond].dialogue.empty()) return "";
        return w.area.npcs[i].conds[cond].dialogue[0].text.substr(0, 24);
    }
    return "(no Cathy)";
}

void frames(World& w, int n) {
    for (int i = 0; i < n; i++) w.update(DT, 0, 0, true);
}

// The spec condition `spec` as Cathy's conditionals have it.
bool spec(const World& w, ConditionDef::Spec s) {
    ConditionDef d;
    d.type = ConditionDef::Type::Spec;
    d.spec = s;
    d.data = 0;
    return checkCondition(d, w);
}

} // namespace

TEST(cat_trick_export) {
    const Area& area = villageArea();
    CHECK(area.catTricks.size() == 1);
    if (area.catTricks.empty()) return;
    const CatTrickDef& c = area.catTricks[0];
    CHECK(area.items[c.cats[0]].name == "Mittens" && area.items[c.cats[1]].name == "Autumn" &&
          area.items[c.cats[2]].name == "Tofu");
    CHECK(c.zoneMaxX - c.zoneMinX == 5 && c.zoneMaxY - c.zoneMinY == 8); // (the BoxCollider2D's size)
    CHECK(area.pathOf(c.textObjects[0]).find("CatTrick/Canvas/Dialogue") != std::string::npos);
}

TEST(cat_trick_counts_cats_dropped_in_the_zone) {
    World w = catWorld();
    const CatTrickDef& c = w.area.catTricks[0];
    frames(w, 2);
    // None back: "0/3" stays hidden (its objects are authored on; UpdateText turns them off).
    CHECK(village::CatTricks::count(w) == 0 && !textOn(w) && !spec(w, ConditionDef::Spec::CatMittens));
    float x, y;
    zoneCenter(w, x, y);
    w.items.drop(w, c.cats[0], x, y); // Mittens (its collider is on from the drop's start)
    frames(w, 2);
    CHECK(w.state.flag("villageHasMittens") && spec(w, ConditionDef::Spec::CatMittens));
    CHECK(!spec(w, ConditionDef::Spec::CatAutumn) && !spec(w, ConditionDef::Spec::CatTofu));
    check(village::CatTricks::count(w) == 1 && textOn(w), "1/3 shows");
    CHECK(std::string(village::CatTricks::text(1)) == "1/3");

    w.items.drop(w, c.cats[1], x - 1, y); // Autumn
    frames(w, 2);
    CHECK(village::CatTricks::count(w) == 2 && textOn(w) && spec(w, ConditionDef::Spec::CatAutumn));
    w.items.drop(w, c.cats[2], x + 1, y); // Tofu: all back, the count hides
    frames(w, 2);
    CHECK(village::CatTricks::count(w) == 3 && !textOn(w) && spec(w, ConditionDef::Spec::CatTofu));

    // Returned for good: moving a cat out again changes nothing.
    w.items.drop(w, c.cats[0], x + 20, y + 20);
    frames(w, 40);
    CHECK(w.state.flag("villageHasMittens") && village::CatTricks::count(w) == 3);
}

TEST(cat_trick_ignores_a_carried_cat) {
    // A carried cat's collider is off: carrying Mittens through the zone doesn't count; dropping it
    // there does.
    World w = catWorld();
    const CatTrickDef& c = w.area.catTricks[0];
    const int mittens = c.cats[0];
    float mx, my;
    w.objectPosition(w.area.items[mittens].object, mx, my);
    w.placeNear(mx - 0.75f, my);
    w.action();
    frames(w, 60); // the pick-up animation
    CHECK(w.items.carried() == mittens);
    float x, y;
    zoneCenter(w, x, y);
    w.placeNear(x, y); // (into the zone, carrying it: a free spot)
    frames(w, 5);
    CHECK(!w.state.flag("villageHasMittens") && village::CatTricks::count(w) == 0);
    for (int i = 0; i < 3; i++) w.update(DT, 0, -1, false); // a few steps (a drop goes the last way walked)
    CHECK(!w.state.flag("villageHasMittens"));
    w.action(); // drop it here
    frames(w, 60);
    CHECK(w.items.carried() < 0 && w.state.flag("villageHasMittens"));
}

TEST(cat_trick_needs_its_tile) {
    // CatTrick sits on tile 9: before it's collected (its objects off) nothing counts.
    World w(villageArea(), DEFAULT_SETTINGS);
    const CatTrickDef& c = w.area.catTricks[0];
    float x, y;
    zoneCenter(w, x, y);
    w.items.drop(w, c.cats[1], x, y);
    frames(w, 40);
    CHECK(village::CatTricks::count(w) == 0);
}

TEST(cat_trick_moves_cathy_along) {
    // Cathy's conditionals (the only readers of the specs) follow the cats home.
    World w = catWorld();
    const CatTrickDef& c = w.area.catTricks[0];
    frames(w, 2);
    check(cathySays(w).rfind("Goodness, hello!", 0) == 0, "none back: " + cathySays(w));
    float x, y;
    zoneCenter(w, x, y);
    w.items.drop(w, c.cats[0], x, y); // Mittens
    frames(w, 40);
    check(cathySays(w).rfind("Autumn and Tofu should", 0) == 0, "Mittens back: " + cathySays(w));
    w.items.drop(w, c.cats[2], x + 1, y); // Tofu
    frames(w, 40);
    check(cathySays(w).rfind("Autumn likes to hang out", 0) == 0, "Mittens and Tofu back: " + cathySays(w));
    w.items.drop(w, c.cats[1], x - 1, y); // Autumn
    frames(w, 40);
    check(cathySays(w).rfind("All the cats are back!", 0) == 0, "all back: " + cathySays(w));
}
