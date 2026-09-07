"""Rebuild original geometric animation samples using only the Python standard library."""

import json
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1] / "Game" / "Animations"
TEXTURE = "ad100000-0000-4000-8000-000000000001"


def write_json(name, value):
    (ROOT / name).write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def main():
    ROOT.mkdir(parents=True, exist_ok=True)
    width, height = 320, 128
    pixels = bytearray(width * height * 4)

    def rect(x, y, w, h, color):
        for row in range(y, y + h):
            for col in range(x, x + w):
                index = (row * width + col) * 4
                pixels[index:index + 4] = bytes(color)

    for frame in range(4):
        # Transparent-center border, then a deliberately simple robot walk cycle.
        x = frame * 64
        color = (100, 255, 195, (255, 200, 135, 65)[frame])
        for rx, ry, rw, rh in ((0, 0, 64, 3), (0, 61, 64, 3), (0, 3, 3, 58), (61, 3, 3, 58)):
            rect(x + rx, ry, rw, rh, color)
        bob = frame % 2 * 2
        rect(x + 18, 72 + bob, 28, 23, (90, 225, 210, 255))
        rect(x + 23, 79 + bob, 5, 5, (12, 30, 48, 255))
        rect(x + 36, 79 + bob, 5, 5, (12, 30, 48, 255))
        rect(x + 22, 97 + bob, 20, 14, (80, 155, 240, 255))
        rect(x + 20 - frame % 2 * 3, 111, 8, 10, (100, 200, 250, 255))
        rect(x + 36 + frame % 2 * 3, 111, 8, 10, (100, 200, 250, 255))

    rect(256, 64, 64, 64, (9, 14, 22, 255))
    # World sprites use bottom-up UVs; the top row is for top-down UI Image rendering.
    lower_rows = [pixels[row * width * 4:(row + 1) * width * 4] for row in range(64, 128)]
    pixels[64 * width * 4:] = b"".join(reversed(lower_rows))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    scanlines = b"".join(b"\0" + pixels[row * width * 4:(row + 1) * width * 4] for row in range(height))
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    (ROOT / "DemoAtlas.png").write_bytes(png + chunk(b"IDAT", zlib.compress(scanlines, 9)) + chunk(b"IEND", b""))

    def clip(name, indices, row, duration, loop):
        write_json(name, {"version": 1, "texture": TEXTURE, "loop": loop,
                         "frames": [{"uvMin": [(index * 64 + 0.5) / width, (row * 64 + 0.5) / height],
                                     "uvMax": [((index + 1) * 64 - 0.5) / width, ((row + 1) * 64 - 0.5) / height],
                                     "duration": duration} for index in indices]})

    clip("CardPulse.clip.json", range(5), 0, 0.2, False)
    clip("RobotWalk.clip.json", range(4), 1, 0.2, True)
    clip("RobotAction.clip.json", [3, 2, 1, 0], 1, 0.08, False)


if __name__ == "__main__":
    main()
