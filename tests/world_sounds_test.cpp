// What the World asks AudioManager for (World::audio): slides, collectibles, the exported sound
// calls, the sound props, the final puzzle's shuffle, footsteps and the ambience.
#include <cmath>
#include <functional>
#include <string>
#include <vector>

#include "game/audio/audio_bank.h"
#include "game/audio/sounds.h"
#include "game/events/events.h"
#include "game/village/village_grid.h"
#include "game/world/world.h"
#include "test.h"

namespace {

constexpr float DT = 1.0f / 60.0f;
using K = AudioCue::Kind;

int count(const std::vector<AudioCue>& cues, K kind, const std::string& name) {
    int n = 0;
    for (const AudioCue& c : cues) n += c.kind == kind && c.name == name ? 1 : 0;
    return n;
}

const AudioCue* find(const std::vector<AudioCue>& cues, const std::string& name) {
    for (const AudioCue& c : cues)
        if (c.name == name) return &c;
    return nullptr;
}

// Every exported call in the area.
void forEachCall(const Area& area, const std::function<void(const CallDef&)>& f) {
    auto all = [&](const std::vector<CallDef>& calls) {
        for (const CallDef& c : calls) f(c);
    };
    for (const ConditionCheckerDef& c : area.conditionCheckers) {
        all(c.conditionals.onSuccess);
        all(c.conditionals.onFail);
    }
    for (const InteractionDef& i : area.interactions) all(i.onSuccess);
    for (const TriggerEventDef& t : area.triggerEvents) {
        all(t.onEnter);
        all(t.onExit);
    }
    for (const CollectibleDef& c : area.collectibles) all(c.onCollect);
    for (const ItemDef& it : area.items) {
        all(it.onPickUp);
        all(it.onDrop);
    }
    for (const NpcDef& n : area.npcs)
        for (const NpcCondDef& c : n.conds) {
            all(c.onEnter);
            all(c.onExhausted);
            for (const DialogueDef& d : c.dialogue) {
                all(d.onStart);
                all(d.onEnd);
            }
        }
}

// Tile 1 slides into an empty slot beside it; the frame's cues.
std::vector<AudioCue> slideTile1(World& w) {
    int bx, by;
    check(w.artifact.findButton(1, bx, by), "tile 1 has a button");
    bool moved = false;
    for (auto [dx, dy] : {std::pair{1, 0}, {-1, 0}, {0, 1}, {0, -1}})
        if (!moved && w.artifact.tryMove(w.grid, bx, by, bx + dx, by + dy, w.player.stile)) moved = true;
    check(moved, "tile 1 moves");
    w.update(DT, FrameInput{});
    return w.audio.take();
}

} // namespace

TEST(world_slides_rumble_as_they_start_and_explode_as_they_land) {
    // The player off the tile: between the tiles (here, on tile 1), always outdoors.
    World w(villageArea(), DEFAULT_SETTINGS);
    w.player.stile = 0;
    w.audio.take();
    std::vector<AudioCue> cues = slideTile1(w);
    const AudioCue* rumble = find(cues, sounds::SLIDE_RUMBLE);
    CHECK(rumble && count(cues, K::Play, sounds::SLIDE_RUMBLE) == 1 && rumble->volume == 1);
    CHECK(rumble && rumble->positioned && rumble->alwaysOutdoor);
    CHECK(w.effects.shakeGain() > 0); // (and it shakes: ShakeConstant)
    int explosions = 0;
    for (int i = 0; i < 120; i++) {
        w.update(DT, FrameInput{});
        explosions += count(w.audio.take(), K::Play, sounds::SLIDE_EXPLOSION);
    }
    CHECK(explosions == 1);
    // Riding the tile: at the listener.
    World r(villageArea(), DEFAULT_SETTINGS);
    r.player.stile = 1;
    r.audio.take();
    cues = slideTile1(r);
    rumble = find(cues, sounds::SLIDE_RUMBLE);
    CHECK(rumble && !rumble->positioned);
}

TEST(world_collectibles_chime_as_they_appear_and_play_the_pick_up) {
    World w(villageArea(), DEFAULT_SETTINGS);
    const Area& area = villageArea();
    // A collectible hidden (Slider 2, as its Start leaves it until its quest hands it out).
    const int c = area.collectible("Slider 2");
    CHECK(c >= 0);
    w.objects.setActive(area, area.collectibles[c].object, false);
    w.audio.take();
    village::activateCollectible(w, "Slider 2", false);
    CHECK(count(w.audio.take(), K::Play, sounds::PUZZLE_COMPLETE) == 1);
    village::activateCollectible(w, "Slider 2", false); // (already out: no sound)
    CHECK(w.audio.take().empty());
    // Walking into one: the area's Pick Up and the music's dip.
    int shown = -1;
    for (int i = 0; i < (int)area.collectibles.size() && shown < 0; i++)
        if (w.objects.activeInHierarchy(area.collectibles[i].object)) shown = i;
    CHECK(shown >= 0);
    w.objectPosition(area.collectibles[shown].object, w.player.x, w.player.y);
    w.update(DT, FrameInput{});
    const std::vector<AudioCue> cues = w.audio.take();
    CHECK(count(cues, K::Play, area.name + " Pick Up") == 1);
    const AudioCue* dip = find(cues, "");
    CHECK(dip && dip->kind == K::DampenMusic && dip->value == 0.2f && dip->length == 2);
}

TEST(world_exported_sound_calls_all_play) {
    // Each of the Village's sound calls cues something AudioManager has (a re-export's typo in a
    // PlayString name would only print at runtime).
    AudioBank bank;
    CHECK(bank.load());
    using M = CallDef::Method;
    int calls = 0;
    forEachCall(villageArea(), [&](const CallDef& c) {
        if (c.method != M::AudioPlay && c.method != M::PlaySoundString && c.method != M::UIClick &&
            c.method != M::PlayErrorSound && c.method != M::VillagePlayDigSound && c.method != M::CowMoo &&
            c.method != M::DogBark)
            return;
        calls++;
        World w(villageArea(), DEFAULT_SETTINGS);
        w.audio.take();
        runCall(w, c);
        const std::vector<AudioCue> cues = w.audio.take();
        check(cues.size() == 1 && cues[0].kind == K::Play && bank.sound(cues[0].name) != nullptr, c.name + " plays a known sound");
    });
    CHECK(calls >= 8); // (two Puzzle Completes, two UIClicks, the error, the dig, the cow, the dog)
}

TEST(world_sound_props_wait_out_their_cooldown_and_show_their_emote) {
    const Area& area = villageArea();
    // The Village's cow, as its exported call has it (with its emote).
    CallDef moo;
    forEachCall(area, [&](const CallDef& c) {
        if (c.method == CallDef::Method::CowMoo) moo = c;
    });
    CHECK(moo.method == CallDef::Method::CowMoo && moo.data >= 0);
    World w(area, DEFAULT_SETTINGS);
    w.audio.take();
    CHECK(!w.objects.activeInHierarchy(moo.data));
    runCall(w, moo);
    runCall(w, moo); // (inside MOO_COOLDOWN)
    std::vector<AudioCue> cues = w.audio.take();
    CHECK(count(cues, K::Play, sounds::COW_MOO) == 1 && cues[0].positioned);
    CHECK(w.objects.activeInHierarchy(moo.data)); // (the emote shows)
    w.props.update(0.1f);
    runCall(w, moo);
    CHECK(count(w.audio.take(), K::Play, sounds::COW_MOO) == 1);
    // The emote goes off a second after the last moo (each restarts it).
    for (int i = 0; i < 55; i++) w.update(DT, FrameInput{});
    CHECK(w.objects.activeInHierarchy(moo.data));
    for (int i = 0; i < 10; i++) w.update(DT, FrameInput{});
    CHECK(!w.objects.activeInHierarchy(moo.data));
    // A dog barks one of its two barks, at half volume.
    CallDef bark;
    bark.method = CallDef::Method::DogBark;
    bark.target = 1;
    runCall(w, bark);
    cues = w.audio.take();
    CHECK(cues.size() == 1 && cues[0].volume == 0.5f &&
          (cues[0].name == sounds::DOG_BARK_1 || cues[0].name == sounds::DOG_BARK_2));
}

TEST(world_shuffle_builds_up_with_rumbles_then_the_bell) {
    World w(villageArea(), DEFAULT_SETTINGS);
    w.audio.take();
    w.finale.shufflePuzzle(w);
    std::vector<AudioCue> cues = w.audio.take();
    for (int i = 0; i < 4 * 60; i++) {
        w.update(DT, FrameInput{});
        for (AudioCue& c : w.audio.take()) cues.push_back(std::move(c));
    }
    std::vector<std::string> order;
    for (const AudioCue& c : cues)
        if (c.name == sounds::SLIDE_RUMBLE || c.name == sounds::TFT_BELL || (c.name == sounds::SLIDE_EXPLOSION && c.volume == 0.2f))
            order.push_back(c.name);
    CHECK((order == std::vector<std::string>{sounds::SLIDE_RUMBLE, sounds::SLIDE_RUMBLE, sounds::SLIDE_RUMBLE,
                                             sounds::SLIDE_EXPLOSION, sounds::TFT_BELL}));
}

TEST(world_footsteps_follow_the_run_and_the_surface) {
    World w(villageArea(), DEFAULT_SETTINGS);
    w.placeAtStart();
    // Inside tile 1's house (a new game's start): wood.
    CHECK(w.player.inHouse && WorldSounds::surfaceAt(w, w.player.x, w.player.y) == "wood");
    w.audio.take();
    int steps = 0;
    for (int i = 0; i < 60; i++) {
        w.update(DT, FrameInput{1, 0});
        steps += count(w.audio.take(), K::Play, "Footstep wood");
    }
    // Two a run cycle (at 0.25 and 0.75 s of it).
    float cycle = 0;
    for (const AnimatorStateDef& st : villageArea().animatorControllers[w.animators.controller(villageArea().player.animator)].states)
        if (!st.events.empty()) cycle = st.length;
    CHECK(cycle > 0 && std::abs(steps - (int)std::floor(60 * DT / cycle * 2 + 0.25f)) <= 1);
    // Held (a cutscene): no steps.
    w.lockPlayer(LOCK_DOLLY, true);
    w.audio.take();
    for (int i = 0; i < 60; i++) w.update(DT, FrameInput{1, 0});
    CHECK(count(w.audio.take(), K::Play, "Footstep wood") == 0);
    w.lockPlayer(LOCK_DOLLY, false);
    // Standing still: none.
    for (int i = 0; i < 30; i++) w.update(DT, FrameInput{});
    w.audio.take();
    for (int i = 0; i < 60; i++) w.update(DT, FrameInput{});
    CHECK(count(w.audio.take(), K::Play, "Footstep wood") == 0);
    // Outside, on tile 1: its surface map (not its house's wood).
    w.player.inHouse = false;
    w.player.stile = 1;
    float tx, ty;
    w.grid.ownerBase(1, tx, ty);
    CHECK(WorldSounds::surfaceAt(w, tx, ty) != "wood");
    // Off every map: the fallback.
    w.player.inHouse = false;
    w.player.stile = 0;
    CHECK(WorldSounds::surfaceAt(w, -1000, -1000) == villageArea().footstepFallback);
}

TEST(world_ambience_plays_by_the_distance_and_stops_with_the_scene) {
    World w(villageArea(), DEFAULT_SETTINGS);
    const Area::AmbienceDef& a = villageArea().ambiences.at(0);
    // (the constructor's scene start queued it)
    std::vector<AudioCue> cues = w.audio.take();
    CHECK(count(cues, K::PlayAmbience, a.name) == 1 && count(cues, K::SetParameter, a.parameter) == 1);
    // Walking toward a node: the parameter follows (only when it changes).
    float nx, ny;
    w.objectPosition(a.nodes[0], nx, ny);
    w.player.inHouse = false;
    w.player.x = nx + 3;
    w.player.y = ny;
    w.sfx.update(w);
    cues = w.audio.take();
    const AudioCue* p = find(cues, a.parameter);
    CHECK(p && p->value <= 3.01f);
    w.sfx.update(w);
    CHECK(w.audio.take().empty());
    w.sfx.end(w);
    cues = w.audio.take();
    p = find(cues, a.parameter);
    CHECK(count(cues, K::StopAmbience, a.name) == 1 && p && p->value == a.defaultValue);
}
