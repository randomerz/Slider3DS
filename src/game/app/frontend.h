// The app's screens and the save points between them (main's loop minus the platform, the assets
// and the dev tools), testable headless:
//   Title: the title art on the top screen (game/ui/title_screen); after "press any key" the menu
//          on the bottom one (game/ui/title_menu; Options opens the pause menu on its Options page).
//          Play / Continue / a new profile start the game; Quit ends the app. Quitting the game to
//          the title plays its intro again (Unity reloads the MainMenu scene).
//   Game:  the World on the top screen, the Artifact (game/ui/artifact_ui) on the bottom; START
//          pauses (game/ui/pause_menu), whose Quit saves and returns to the title (QuitHandler).
//   End:   the port's ending (game/ui/end_screen), once the game faded out through one of the
//          area's exits (World::leaveArea, which has it saved); then the title.
// The save profile is written as Unity's GameSaver / SaveSystem.SaveGame do: once the scene has
// started, when a script asks (World::saveRequested: a collectible's pick-up), on Quit, and when the
// app closes in the game — each stamped with the time (lastSaved: Continue's pick). A scene booted
// directly (dev) plays a temporary profile that is never written (Unity editor's "Boomo", index -1).
// The player's Settings (owned by main) are saved when a menu that may change them closes, and when
// the app closes with them changed.
// What to play (audio()) gathers the World's cues with the title's music (MainMenuManager: "Main
// Menu", MainMenuActivated set at StartMainMenu) and the menus' clicks; main plays them.
// Every switch between screens releases the stylus held across it, so a touch never carries over
// as a tap or a drag.
#pragma once

#include "game/audio/audio_cues.h"
#include "game/save/profile.h"
#include "game/save/settings.h"
#include "game/ui/artifact_ui.h"
#include "game/ui/end_screen.h"
#include "game/ui/pause_menu.h"
#include "game/ui/title_menu.h"
#include "game/ui/title_screen.h"
#include "game/world/world.h"
#include "platform/platform.h"

class AreaView;
class MenuSkin;
class SpriteSheets;

class Frontend {
public:
    enum class Mode { Title, Game, End };
    static constexpr float FRAME_DT = 1.0f / 60.0f; // the game steps a fixed 60 fps
    // The title menu ignores input this long after it appears (TitleScreen::MENU_LOCKOUT).
    static constexpr int MENU_LOCKOUT_FRAMES = (int)(TitleScreen::MENU_LOCKOUT / FRAME_DT + 0.5f);

    // Starts on the title screen. `settings` (main's) and the art (loaded) must outlive it.
    Frontend(const Area& area, const TitleArt& art, const EndArt& endArt, Settings& settings);

    // One frame of input. In the game, (moveX, moveY) is where the player walks (each -1..1; main
    // leaves out what its dev controls use) and `noclip` skips collisions (dev). False once the app
    // should end (Quit on the title).
    bool update(const plat::Input& in, float moveX, float moveY, bool noclip = false);
    // A scene booted directly (dev): the Village, a new game on a temporary profile (never written).
    void bootDirect();
    // The app is closing: changed Settings and, in the game, the profile are written.
    void shutdown();

    // The top screen: the world (or the title's backdrop), its overlays, the pause shade.
    void drawTop(AreaView& view, const SpriteSheets& sheets, const MenuSkin& skin, bool showInactive) const;
    // The bottom screen: the pause menu, the title menu, the Artifact or the ending's prompt.
    void drawBottom(const SpriteSheets& sheets, const MenuSkin& skin) const;

    Mode mode() const { return mode_; }
    bool playing() const { return mode_ == Mode::Game && !pause_.isOpen(); } // the game runs this frame
    World& world() { return world_; }
    const World& world() const { return world_; }
    const SaveProfile& profile() const { return profile_; }
    int slot() const { return slot_; } // the profile's slot, -1 = temporary
    const TitleMenu& title() const { return title_; }
    const TitleScreen& titleScreen() const { return screen_; }
    const PauseMenu& pauseMenu() const { return pause_; }
    const ArtifactUI& artifactUI() const { return artifactUI_; }
    // What's asked of AudioManager since main last took it.
    AudioCues& audio() { return audio_; }
    // Where sounds are heard from: the camera's target in the game.
    Listener listener() const;
    // The game's pausable sounds hold (PauseManager: the pause menu in the game, not the title's Options).
    bool audioPaused() const { return mode_ == Mode::Game && pause_.isOpen(); }

private:
    void startGame(SaveProfile profile, int slot);
    void saveGame();             // the profile into its slot (none for a temporary one)
    void saveSettingsIfChanged(); // one attempt per change
    void toTitle();

    const Area& area_;
    const TitleArt& art_;
    const EndArt& endArt_;
    Settings& settings_;
    Settings savedSettings_;
    World world_;
    TitleScreen screen_;
    TitleMenu title_;
    PauseMenu pause_;
    EndScreen end_;
    ArtifactUI artifactUI_;
    Mode mode_ = Mode::Title;
    AudioCues audio_;
    SaveProfile profile_;
    int slot_ = -1;
};
