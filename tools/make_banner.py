"""The CIA's HOME Menu banner (`make RELEASE=1 cia`): what the top screen shows while the title is
selected on the HOME Menu.

  tools/cia/banner.png  256x128: the original's key art (production/art/Slider_Main_1920x1080.png in the
                        Slider repo) scaled to 256 wide and cut to 128 tall around its middle
  tools/cia/banner.wav  the banner's sound: the Village pick-up jingle (16-bit stereo, 44.1 kHz; under 3 s), brought up to
                        BANNER_PEAK: the HOME Menu plays it as it is, and the game's clip is quiet

Usage: python tools/make_banner.py [path to the Slider repo]   (needs Pillow and ffmpeg)
"""
import array
import os
import subprocess
import sys
import wave

from PIL import Image

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SLIDER = os.path.join(os.path.dirname(REPO), "Slider")
ART = os.path.join("production", "art", "Slider_Main_1920x1080.png")
OUT = os.path.join(REPO, "tools", "cia")
JINGLE = os.path.join(REPO, "assets", "audio", "sfx_village_pick_up.ogg")
W, H = 256, 128
BANNER_PEAK = 29000  # about -1 dBFS


def normalize(path):
    with wave.open(path, "rb") as w:
        params = w.getparams()
        samples = array.array("h", w.readframes(w.getnframes()))
    peak = max(abs(s) for s in samples)
    with wave.open(path, "wb") as w:
        w.setparams(params)
        w.writeframes(array.array("h", (s * BANNER_PEAK // peak for s in samples)).tobytes())


def main(slider):
    os.makedirs(OUT, exist_ok=True)
    art = Image.open(os.path.join(slider, ART)).convert("RGB")
    scaled = art.resize((W, round(art.height * W / art.width)), Image.LANCZOS)
    top = (scaled.height - H) // 2
    scaled.crop((0, top, W, top + H)).save(os.path.join(OUT, "banner.png"))
    sound = os.path.join(OUT, "banner.wav")
    subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-i", JINGLE, "-ac", "2", "-ar", "44100", "-sample_fmt", "s16",
                    "-t", "2.9", sound], check=True)
    normalize(sound)
    print(f"banner: {W}x{H} from {os.path.basename(ART)} ({scaled.width}x{scaled.height}, rows {top}-{top + H}); "
          f"sound from {os.path.basename(JINGLE)}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else SLIDER)
