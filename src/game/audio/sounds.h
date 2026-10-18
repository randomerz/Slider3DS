// AudioManager's names for what the port plays, as the original's scripts spell them.
// tools/make_audio.py exports exactly these (its SOUNDS / MUSIC / AMBIENCE lists); audio_test checks
// the bank has each one, so a name added here needs a re-export.
#pragma once

namespace sounds {
inline constexpr const char* UI_CLICK = "UI Click";             // UIClick.Click; PlayerAction pick-up / drop (pitched)
inline constexpr const char* PUZZLE_COMPLETE = "Puzzle Complete"; // a collectible appears, the hole filled, the 8-puzzle placed
inline constexpr const char* HAT_CLICK = "Hat Click";             // small pops: dig, a knot solved, trail sparks, the cat
inline constexpr const char* SLIDE_RUMBLE = "Slide Rumble";       // a tile starts sliding; the shuffle's build-up
inline constexpr const char* SLIDE_EXPLOSION = "Slide Explosion"; // a tile lands; the cave door; the shuffle
inline constexpr const char* TFT_BELL = "TFT Bell";               // the shuffle's peak
inline constexpr const char* MAGIC_CHIMES_1 = "MagicChimes1";     // the ruins arrow appears; the cat's abduction
inline constexpr const char* MEOW = "Meow";
inline constexpr const char* POP = "Pop";
inline constexpr const char* FALL = "Fall";
inline constexpr const char* HURT = "Hurt";
inline constexpr const char* ARTIFACT_ERROR = "Artifact Error"; // ConditionChecker.PlayErrorSound
inline constexpr const char* COW_MOO = "Cow Moo";
inline constexpr const char* DOG_BARK_1 = "Dog Bark 1";
inline constexpr const char* DOG_BARK_2 = "Dog Bark 2";
inline constexpr const char* VILLAGE_PICK_UP = "Village Pick Up"; // ItemPickupEffect: "<area> Pick Up"
inline constexpr const char* ALL[] = {UI_CLICK, PUZZLE_COMPLETE, HAT_CLICK, SLIDE_RUMBLE, SLIDE_EXPLOSION, TFT_BELL,
                                      MAGIC_CHIMES_1, MEOW, POP, FALL, HURT, ARTIFACT_ERROR, COW_MOO, DOG_BARK_1,
                                      DOG_BARK_2, VILLAGE_PICK_UP};
} // namespace sounds

namespace tracks {
inline constexpr const char* MAIN_MENU = "Main Menu"; // MainMenuManager.Start
inline constexpr const char* VILLAGE = "Village";     // VillageGrid.Start
inline constexpr const char* ALL_MUSIC[] = {MAIN_MENU, VILLAGE};
} // namespace tracks

// FMOD's global parameters the port sets.
namespace audio_params {
inline constexpr const char* MAIN_MENU_ACTIVATED = "MainMenuActivated"; // 0 until StartMainMenu, then 1
} // namespace audio_params
