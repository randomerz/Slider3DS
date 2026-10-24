#include "game/app/frontend.h"

#include <cmath>
#include <cstdio>
#include <ctime>
#include <utility>

#include "game/audio/sounds.h"
#include "game/render/area_view.h"
#include "game/render/sprite_sheets.h"
#include "game/ui/hud.h"
#include "game/ui/menu_skin.h"
#include "game/ui/palette.h"

Frontend::Frontend(const Area& area, const TitleArt& art, const EndArt& endArt, Settings& settings)
    : area_(area), art_(art), endArt_(endArt), settings_(settings), savedSettings_(settings), world_(area, settings) {
    for (const CollectibleDef& c : area.collectibles) cheats_.collectibles.push_back(c.name);
    toTitle();
}

void Frontend::startGame(SaveProfile profile, int slot) {
    // SaveSystem.LoadSaveProfile / NewSavePanelManager.StartNewGame (the original's intro cutscene
    // scene isn't ported: a new game starts in the Village).
    profile_ = std::move(profile);
    slot_ = slot;
    cheats_.noclip = noclipMarked_ = false; // (the scene's new Player collides; nothing marked carries over)
    cheats_.toGive.clear();
    world_.audio.take(); // (left from the World before this scene: its construction's Starts)
    world_.load(profile_);
    artifactUI_.init(area_, world_.artifact);
    artifactUI_.releaseStylus(); // (the touch that started it isn't a press on the Artifact)
    mode_ = Mode::Game;
    audio_.takeFrom(world_.audio); // (the scene's Starts: its music)
    saveGame(); // (GameSaver.Start)
}

void Frontend::bootDirect() { startGame(SaveProfile::make("Boomo"), -1); }

void Frontend::saveGame() {
    if (slot_ < 0) return;
    world_.save(profile_);
    profile_.lastSaved = (int64_t)std::time(nullptr);
    if (!profile_.save(slot_)) printf("Couldn't save the profile\n");
}

void Frontend::saveSettingsIfChanged() {
    if (settings_ == savedSettings_) return;
    if (!settings_.save()) printf("Couldn't save the settings\n");
    savedSettings_ = settings_;
}

void Frontend::toTitle() {
    mode_ = Mode::Title;
    screen_.start(art_); // (MainMenu reloads: its intro again)
    title_.open();       // (the profiles re-read; the held touch released)
    // MainMenuManager.Start: the title's music (it plays on from the Options), its full arrangement
    // held back until the menu opens.
    audio_.playMusic(tracks::MAIN_MENU, false);
    audio_.setParameter(audio_params::MAIN_MENU_ACTIVATED, 0);
}

Listener Frontend::listener() const {
    Listener l;
    if (mode_ != Mode::Game) return l; // (the title plays nothing spatial)
    world_.cameraTarget(l.x, l.y);
    l.indoor = world_.player.inHouse;
    l.housingOffset = (float)area_.housingOffset;
    return l;
}

void Frontend::shutdown() {
    saveSettingsIfChanged(); // (closed while paused: HOME > Close, the PC window)
    if (mode_ == Mode::Game) saveGame(); // (GameSaver.OnApplicationQuit)
}

bool Frontend::update(const plat::Input& in, float moveX, float moveY) {
    if (cheats_.noclip != noclipMarked_) { // (toggled: the Cheats page, or a dev shortcut before this)
        noclipMarked_ = cheats_.noclip;
        if (mode_ == Mode::Game) cheats::markUsed(world_);
    }
    if (pause_.isOpen()) {
        const PauseMenu::Action action = pause_.update(in, settings_);
        if (pause_.clicked()) audio_.play(sounds::UI_CLICK);
        if (action != PauseMenu::Action::None) saveSettingsIfChanged();
        if (cheats_.respawn) {
            cheats_.respawn = false;
            cheats::respawnPlayer(world_);
        }
        if (action == PauseMenu::Action::Resume && mode_ == Mode::Game) { // (CheckCheatsOnPauseClosed)
            if (settings_.cheats) // (turned off since: the marks are dropped)
                for (int index : cheats_.toGive) cheats::giveCollectible(world_, index);
            cheats_.toGive.clear();
        }
        if (action == PauseMenu::Action::Quit) { // (QuitHandler: save, then the main menu)
            saveGame();
            world_.sfx.end(world_); // (the scene unloads: its ambience stops)
            audio_.takeFrom(world_.audio);
            toTitle();
        } else if (action == PauseMenu::Action::Resume) {
            if (mode_ == Mode::Game) artifactUI_.releaseStylus();
            else title_.releaseStylus();
        }
        return true;
    }
    if (mode_ == Mode::Title) {
        const bool wasShown = screen_.menuShown();
        if (screen_.update(area_.cameraShake, in, FRAME_DT, settings_.screenShake)) {
            title_.lockOut(MENU_LOCKOUT_FRAMES); // (StartMainMenu: the buttons come in; the key that shows it isn't theirs)
            audio_.setParameter(audio_params::MAIN_MENU_ACTIVATED, 1);
        }
        if (!wasShown) return true;
        const TitleMenu::Action a = title_.update(in);
        if (title_.clicked()) audio_.play(sounds::UI_CLICK);
        switch (a.kind) {
        case TitleMenu::Action::Kind::None: break;
        case TitleMenu::Action::Kind::Play: {
            SaveProfile p;
            if (SaveProfile::load(a.slot, p) == SaveProfile::Read::Ok) startGame(std::move(p), a.slot);
            else title_.open(); // (the file changed since the cards were read)
            break;
        }
        case TitleMenu::Action::Kind::NewGame: startGame(SaveProfile::make(a.name), a.slot); break;
        case TitleMenu::Action::Kind::Options: pause_.open(PauseMenu::Page::Options); break;
        case TitleMenu::Action::Kind::Quit: return false;
        }
        return true;
    }
    if (mode_ == Mode::End) {
        const bool done = end_.update(in);
        if (end_.clicked()) audio_.play(sounds::UI_CLICK);
        if (done) toTitle();
        return true;
    }
    if ((in.pressed & plat::BTN_START) && !world_.playerLocked()) {
        pause_.open(PauseMenu::Page::Paused, &cheats_);
        artifactUI_.releaseStylus();
        return true;
    }
    if (!world_.playerLocked()) {
        artifactUI_.update(in, world_, FRAME_DT);
        if (artifactUI_.clicked()) audio_.play(sounds::UI_CLICK);
    } else {
        artifactUI_.releaseStylus(); // (the Artifact stays shut while the player is held)
        if (in.touching) artifactUI_.touched();
    }
    FrameInput frame{moveX, moveY, (in.held & plat::BTN_A) != 0};
    if (in.pressed & plat::BTN_A) world_.action();
    world_.player.noClip = noclip();
    world_.update(FRAME_DT, frame);
    audio_.takeFrom(world_.audio);
    if (world_.saveRequested) {
        world_.saveRequested = false;
        saveGame();
    }
    if (world_.effects.leftArea) { // (through an exit, faded out and saved: the port's ending)
        world_.sfx.end(world_); // (the scene unloads: its ambience stops)
        audio_.takeFrom(world_.audio);
        artifactUI_.releaseStylus();
        mode_ = Mode::End;
        end_.start();
    }
    return true;
}

void Frontend::drawTop(AreaView& view, const SpriteSheets& sheets, const MenuSkin& skin) const {
    if (mode_ == Mode::End) {
        end_.drawTop(endArt_);
        return;
    }
    if (mode_ == Mode::Title) {
        screen_.draw(art_, skin.font());
        if (pause_.isOpen()) plat::drawRect(0, 0, plat::TOP_W, plat::TOP_H, plat::rgba(0, 0, 0, 0x80));
        return;
    }
    plat::beginScreen(plat::Screen::Top, plat::rgba(area_.background[0], area_.background[1], area_.background[2]));
    float camX, camY;
    world_.camera(camX, camY);
    view.begin(area_, sheets, camX, camY, plat::TOP_W, plat::TOP_H, world_.frame * FRAME_DT, false);
    view.draw(world_);
    drawTopOverlays(world_, skin.font(), skin.captionFont());
    if (pause_.isOpen()) plat::drawRect(0, 0, plat::TOP_W, plat::TOP_H, plat::rgba(0, 0, 0, 0x80));
}

void Frontend::drawBottom(const SpriteSheets& sheets, const MenuSkin& skin) const {
    if (mode_ == Mode::End) {
        end_.drawBottom(skin);
        return;
    }
    plat::beginScreen(plat::Screen::Bottom, palette::DARK);
    if (pause_.isOpen()) pause_.draw(settings_, skin);
    else if (mode_ == Mode::Game) artifactUI_.draw(world_, sheets);
    else if (screen_.menuShown()) { // (before "press any key": empty)
        // It slides up from below as it comes in (the original's buttons; whole UI px).
        const MenuSkin::Shift slide(skin, 0, (int)std::lround(screen_.menuDrop(art_) * plat::BOTTOM_H / MenuSkin::UI_SCALE));
        title_.draw(skin);
    }
}
