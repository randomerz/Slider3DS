"""The README's install QR code: FBI's Remote Install > Scan QR Code reads it as a URL to install from.

  tools/cia/install_qr.png  the newest release's slider3ds.cia, so new releases need no new code
                            (as long as the file keeps its name)

Usage: python tools/make_install_qr.py   (needs segno)
"""
import os

import segno

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
URL = "https://github.com/randomerz/Slider3DS/releases/latest/download/slider3ds.cia"
OUT = os.path.join(REPO, "tools", "cia", "install_qr.png")
SCALE = 6  # px per module: big enough to scan off a monitor with the 3DS's camera


def main():
    qr = segno.make(URL, error="m", micro=False)
    qr.save(OUT, scale=SCALE, border=4, dark="#000000", light="#ffffff")  # light: GitHub's dark theme needs the white border
    print(f"install_qr: version {qr.designator}, {qr.symbol_size(scale=SCALE)[0]} px, {URL}")


if __name__ == "__main__":
    main()
