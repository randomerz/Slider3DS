"""The CIA's HOME Menu banner (`make RELEASE=1 cia`), from the title screen as it settles.

  tools/cia/banner.png  256x128: the title screen's top screen once its stars are in (the render golden
                        `title_stars_in`: logo, player, stars), cropped around the logo and the player
  tools/cia/banner.wav  the banner's sound: the Village pick-up jingle (16-bit mono, 32 kHz; under 3 s)

It runs the PC tests (build them first) with SLIDER3DS_DUMP_DRAWLISTS to get that draw list, so the banner
follows the title's art and layout. To use your own art, replace banner.png with 256x128 art (and
banner.wav with any short 16-bit WAV) and rebuild; re-running this overwrites both.

Usage: python tools/make_banner.py   (needs Pillow and ffmpeg)
"""
import os
import subprocess

import render_drawlist

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(REPO, "tools", "cia")
TESTS = os.path.join(REPO, "build-pc", "Debug", "slider3ds_tests.exe")
DRAWLIST = os.path.join(REPO, "build-pc", "test-out", "title_stars_in.txt")
JINGLE = os.path.join(REPO, "assets", "audio", "sfx_village_pick_up.ogg")
SCREEN = 400, 240
W, H = 256, 128
CROP_Y = 31  # (the logo's top to the player's feet, centered)
BACKGROUND = (26, 26, 26, 255)  # (title.json's background)


def main():
    os.makedirs(OUT, exist_ok=True)
    subprocess.run([TESTS, "render_title_screen"], check=True, stdout=subprocess.DEVNULL,
                   env=dict(os.environ, SLIDER3DS_DUMP_DRAWLISTS="1"))
    screen = render_drawlist.render(DRAWLIST, *SCREEN, background=BACKGROUND)
    x = (SCREEN[0] - W) // 2
    screen.crop((x, CROP_Y, x + W, CROP_Y + H)).save(os.path.join(OUT, "banner.png"))
    subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-i", JINGLE, "-ac", "1", "-ar", "32000", "-sample_fmt", "s16",
                    "-t", "2.9", os.path.join(OUT, "banner.wav")], check=True)
    print(f"banner: {W}x{H} from the title screen at ({x}, {CROP_Y}); sound from {os.path.basename(JINGLE)}")


if __name__ == "__main__":
    main()
