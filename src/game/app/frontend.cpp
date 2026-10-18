#include "game/app/frontend.h"

#include <cstdio>
#include <ctime>
#include <utility>

#include "game/render/area_view.h"
#include "game/render/sprite_sheets.h"
#include "game/ui/hud.h"
#include "game/ui/menu_skin.h"
#include "game/ui/palette.h"

Frontend::Frontend(const Area& area, const TitleArt& art, Settings& settings)
    : area_(area), art_(art), settings_(settings), savedSettings_(settings), world_(area, settings) {
    toTitle();
}

void Frontend::startGame(SaveProfile profile, int slot) {
    // SaveSystem.LoadSaveProfile / NewSavePanelManager.StartNewGame (the original's intro cutscene
    // scene isn't ported: a new game starts in the Village).
    profile_ = std::move(profile);
    slot_ = slot;
    world_.load(profile_);
    artifactUI_.init(area_, world_.artifact);
    artifactUI_.releaseStylus(); // (the touch that started it isn't a press on the Artifact)
    mode_ = Mode::Game;
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
}

void Frontend::shutdown() {
    saveSettingsIfChanged(); // (closed while paused: HOME > Close, the PC window)
    if (mode_ == Mode::Game) saveGame(); // (GameSaver.OnApplicationQuit)
}

bool Frontend::update(const plat::Input& in, float moveX, float moveY, bool noclip) {
    if (pause_.isOpen()) {
        const PauseMenu::Action action = pause_.update(in, settings_);
        if (action != PauseMenu::Action::None) saveSettingsIfChanged();
        if (action == PauseMenu::Action::Quit) { // (QuitHandler: save, then the main menu)
            saveGame();
            toTitle();
        } else if (action == PauseMenu::Action::Resume) {
            if (mode_ == Mode::Game) artifactUI_.releaseStylus();
            else title_.releaseStylus();
        }
        return true;
    }
    if (mode_ == Mode::Title) {
        const bool wasShown = screen_.menuShown();
        if (screen_.update(area_, in, FRAME_DT, settings_.screenShake)) title_.releaseStylus(); // (the key that shows it)
        if (!wasShown) return true;
        const TitleMenu::Action a = title_.update(in);
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
    if ((in.pressed & plat::BTN_START) && !world_.playerLocked()) {
        pause_.open();
        artifactUI_.releaseStylus();
        return true;
    }
    if (!world_.playerLocked()) artifactUI_.update(in, world_, FRAME_DT);
    else artifactUI_.releaseStylus(); // (the Artifact stays shut while the player is held)
    FrameInput frame{moveX, moveY, (in.held & plat::BTN_A) != 0};
    if (in.pressed & plat::BTN_A) world_.action();
    world_.update(FRAME_DT, frame, noclip);
    if (world_.saveRequested) {
        world_.saveRequested = false;
        saveGame();
    }
    return true;
}

void Frontend::drawTop(AreaView& view, const SpriteSheets& sheets, const MenuSkin& skin, bool showInactive) const {
    if (mode_ == Mode::Title) {
        screen_.draw(art_, skin.font());
        if (pause_.isOpen()) plat::drawRect(0, 0, plat::TOP_W, plat::TOP_H, plat::rgba(0, 0, 0, 0x80));
        return;
    }
    plat::beginScreen(plat::Screen::Top, plat::rgba(area_.background[0], area_.background[1], area_.background[2]));
    float camX, camY;
    world_.camera(camX, camY);
    view.begin(area_, sheets, camX, camY, plat::TOP_W, plat::TOP_H, world_.frame * FRAME_DT, showInactive);
    view.draw(world_);
    drawTopOverlays(world_, skin.font());
    if (pause_.isOpen()) plat::drawRect(0, 0, plat::TOP_W, plat::TOP_H, plat::rgba(0, 0, 0, 0x80));
}

void Frontend::drawBottom(const SpriteSheets& sheets, const MenuSkin& skin) const {
    plat::beginScreen(plat::Screen::Bottom, palette::DARK);
    if (pause_.isOpen()) pause_.draw(settings_, skin);
    else if (mode_ == Mode::Game) artifactUI_.draw(world_, sheets);
    else if (screen_.menuShown()) title_.draw(skin); // (before "press any key": empty)
}
