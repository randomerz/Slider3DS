// What the World asks AudioManager for (World::audio): slides, collectibles, the sound props' calls.
#include <string>
#include <vector>

#include "game/audio/sounds.h"
#include "game/events/events.h"
#include "game/village/village_grid.h"
#include "game/world/world.h"
#include "test.h"

namespace {

constexpr float DT = 1.0f / 60.0f;

int count(const std::vector<AudioCue>& cues, AudioCue::Kind kind, const std::string& name) {
    int n = 0;
    for (const AudioCue& c : cues) n += c.kind == kind && c.name == name ? 1 : 0;
    return n;
}

const AudioCue* find(const std::vector<AudioCue>& cues, const std::string& name) {
    for (const AudioCue& c : cues)
        if (c.name == name) return &c;
    return nullptr;
}

} // namespace

TEST(world_slides_rumble_as_they_start_and_explode_as_they_land) {
    World w(villageArea(), DEFAULT_SETTINGS);
    w.audio.take();
    // Tile 1 into an empty slot beside it.
    int bx, by;
    CHECK(w.artifact.findButton(1, bx, by));
    bool moved = false;
    for (auto [dx, dy] : {std::pair{1, 0}, {-1, 0}, {0, 1}, {0, -1}})
        if (!moved && w.artifact.tryMove(w.grid, bx, by, bx + dx, by + dy, w.player.stile)) moved = true;
    CHECK(moved);
    w.update(DT, FrameInput{});
    std::vector<AudioCue> cues = w.audio.take();
    const AudioCue* rumble = find(cues, sounds::SLIDE_RUMBLE);
    CHECK(rumble && count(cues, AudioCue::Kind::Play, sounds::SLIDE_RUMBLE) == 1 && rumble->volume == 1);
    // (between the tiles, or at the listener while the player rides one; always outdoors)
    CHECK(rumble && (rumble->positioned ? rumble->alwaysOutdoor : w.player.stile == 1));
    int explosions = 0;
    for (int i = 0; i < 120; i++) {
        w.update(DT, FrameInput{});
        explosions += count(w.audio.take(), AudioCue::Kind::Play, sounds::SLIDE_EXPLOSION);
    }
    CHECK(explosions == 1);
}

TEST(world_collectibles_chime_as_they_appear_once) {
    World w(villageArea(), DEFAULT_SETTINGS);
    const Area& area = villageArea();
    // A collectible hidden (Slider 2, as its Start leaves it until its quest hands it out).
    const int c = area.collectible("Slider 2");
    CHECK(c >= 0);
    w.objects.setActive(area, area.collectibles[c].object, false);
    w.audio.take();
    village::activateCollectible(w, "Slider 2", false);
    CHECK(count(w.audio.take(), AudioCue::Kind::Play, sounds::PUZZLE_COMPLETE) == 1);
    village::activateCollectible(w, "Slider 2", false); // (already out: no sound)
    CHECK(w.audio.take().empty());
}

TEST(world_sound_props_wait_out_their_cooldown) {
    World w(villageArea(), DEFAULT_SETTINGS);
    w.audio.take();
    CallDef moo;
    moo.method = CallDef::Method::CowMoo;
    moo.target = 0;
    runCall(w, moo);
    runCall(w, moo); // (inside MOO_COOLDOWN)
    std::vector<AudioCue> cues = w.audio.take();
    CHECK(count(cues, AudioCue::Kind::Play, sounds::COW_MOO) == 1 && cues[0].positioned);
    w.playTime += 0.2;
    runCall(w, moo);
    CHECK(count(w.audio.take(), AudioCue::Kind::Play, sounds::COW_MOO) == 1);
    // A dog barks one of its two barks, at half volume.
    CallDef bark;
    bark.method = CallDef::Method::DogBark;
    bark.target = 1;
    runCall(w, bark);
    cues = w.audio.take();
    CHECK(cues.size() == 1 && cues[0].volume == 0.5f &&
          (cues[0].name == sounds::DOG_BARK_1 || cues[0].name == sounds::DOG_BARK_2));
    // Exported sound calls play by name.
    CallDef play;
    play.method = CallDef::Method::PlaySoundString;
    play.stringArg = sounds::PUZZLE_COMPLETE;
    runCall(w, play);
    CHECK(count(w.audio.take(), AudioCue::Kind::Play, sounds::PUZZLE_COMPLETE) == 1);
}
