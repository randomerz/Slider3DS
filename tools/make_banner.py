"""The CIA's HOME Menu banner (`make RELEASE=1 cia`): a placeholder until the user draws one.

  tools/cia/banner.png  256x128: the original's logo (pixel_logo_0) centered on Slider's #1a1a1a
  tools/cia/banner.wav  the banner's sound: the Village pick-up jingle (16-bit mono, 32 kHz; under 3 s)

Replace banner.png with your own 256x128 art (and banner.wav with any short 16-bit WAV) and rebuild;
re-running this overwrites both.

Usage: python tools/make_banner.py [path to Slider/Assets]   (needs Pillow and ffmpeg)
"""
import os
import subprocess
import sys

from PIL import Image

import make_ui

OUT = os.path.join(make_ui.REPO, "tools", "cia")
W, H = 256, 128
BACKGROUND = (26, 26, 26, 255)
JINGLE = os.path.join(make_ui.REPO, "assets", "audio", "sfx_village_pick_up.ogg")


def main():
    os.makedirs(OUT, exist_ok=True)
    logo, _ = make_ui.texture_sprites("Sprites/UI/menus/pixel_logo.png")["pixel_logo_0"]
    logo = logo.crop(logo.getbbox())
    banner = Image.new("RGBA", (W, H), BACKGROUND)
    banner.alpha_composite(logo, ((W - logo.width) // 2, (H - logo.height) // 2))
    banner.save(os.path.join(OUT, "banner.png"))
    subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-i", JINGLE, "-ac", "1", "-ar", "32000", "-sample_fmt", "s16",
                    "-t", "2.9", os.path.join(OUT, "banner.wav")], check=True)
    print(f"banner: {W}x{H}, logo {logo.width}x{logo.height}; sound from {os.path.basename(JINGLE)}")


if __name__ == "__main__":
    if len(sys.argv) > 1:
        make_ui.ASSETS = sys.argv[1]
    main()
