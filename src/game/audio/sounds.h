// AudioManager's names for what the port plays, as the original's scripts spell them.
// tools/make_audio.py exports exactly these (its SOUNDS / MUSIC / AMBIENCE lists); audio_test checks
// the bank has each one, so a name added here needs a re-export.
#pragma once

namespace sounds {
inline constexpr const char* UI_CLICK = "UI Click"; // UIClick.Click: a menu button, toggle or key
inline constexpr const char* ALL[] = {UI_CLICK};
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
