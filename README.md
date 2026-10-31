# Slider 3DS

A homebrew Nintendo 3DS port of [Slider](https://store.steampowered.com/app/1916890/Slider/): slide chunks of the world to solve puzzles and help others.

**v0.1 has the first area, Stonybrook Village**, from the intro to the final puzzle, with a short ending. More areas will come later.

It runs on Old and New 3DS / 2DS systems with custom firmware.

## Install

Download a file from the [Releases](https://github.com/randomerz/Slider3DS/releases) page. There are two kinds.

**`slider3ds.cia`: an icon on the HOME Menu**
1. Copy it to the SD card.
2. Install it with [FBI](https://github.com/Steveice10/FBI/releases) (or another CIA installer).

**`slider3ds.3dsx`: runs from the Homebrew Launcher**
1. Copy it to `sdmc:/3ds/` on the SD card.
2. Start it from the Homebrew Launcher.

**Sound needs `sdmc:/3ds/dspfirm.cdc`.** Without it the game runs silently. To create it, run [DSP1](https://github.com/zoogie/DSP1/releases) once. Most custom firmware setup guides already have you do this.

Saves and settings are kept in `sdmc:/3ds/slider3ds/`.

## Controls

| Button | Action |
|---|---|
| Circle Pad | Walk |
| A | Talk, pick up, put down. Hold A to skip a cutscene. |
| Touch screen | Slide tiles on the Artifact |
| D-pad + X | Slide tiles without the stylus: the D-pad moves the cursor, X selects |
| START | Pause menu (options, quit) |

In the menus, the D-pad or Circle Pad moves, A selects and B goes back. You can also tap with the stylus.

## Building

The game is C++ on devkitPro (libctru + citro2d). It also has an SDL2 PC build for development and headless tests.

```sh
make              # 3DS dev build: slider3ds.3dsx
make RELEASE=1    # 3DS release build: slider3ds_release.3dsx
make RELEASE=1 cia  # slider3ds.cia (needs makerom and bannertool)
make RELEASE=1 dist # dist/slider3ds.3dsx + dist/slider3ds.cia: the files to publish
```

For the PC build, configure the repo with CMake; it fetches SDL2 on its own. The tests are `slider3ds_tests`.

The content in `assets/` is exported from the original Unity project by an editor script, which isn't published. Everything needed to build the game is in this repo.

## Credits

Slider is by Boomo and the Georgia Tech VGDev team. This port is an unofficial homebrew project, not affiliated with Nintendo.

The code and game content are here so you can build and play the port. If you'd like to reuse any of it, just ask!
