"""Rebuild the original Janus demo bitmap alphabet using only Python's standard library.

These simple 5x7 glyph patterns are authored here; no system font or third-party
font data is copied. This does not select or change the repository's license.
"""

import json
from pathlib import Path
import struct
import zlib


# Seven rows of five bits, encoded as hex. Lowercase deliberately uses small caps.
PATTERNS = {
    "A": "0e 11 11 1f 11 11 11", "B": "1e 11 11 1e 11 11 1e",
    "C": "0e 11 10 10 10 11 0e", "D": "1e 11 11 11 11 11 1e",
    "E": "1f 10 10 1e 10 10 1f", "F": "1f 10 10 1e 10 10 10",
    "G": "0e 11 10 17 11 11 0f", "H": "11 11 11 1f 11 11 11",
    "I": "0e 04 04 04 04 04 0e", "J": "07 02 02 02 12 12 0c",
    "K": "11 12 14 18 14 12 11", "L": "10 10 10 10 10 10 1f",
    "M": "11 1b 15 15 11 11 11", "N": "11 19 15 13 11 11 11",
    "O": "0e 11 11 11 11 11 0e", "P": "1e 11 11 1e 10 10 10",
    "Q": "0e 11 11 11 15 12 0d", "R": "1e 11 11 1e 14 12 11",
    "S": "0f 10 10 0e 01 01 1e", "T": "1f 04 04 04 04 04 04",
    "U": "11 11 11 11 11 11 0e", "V": "11 11 11 11 11 0a 04",
    "W": "11 11 11 15 15 15 0a", "X": "11 11 0a 04 0a 11 11",
    "Y": "11 11 0a 04 04 04 04", "Z": "1f 01 02 04 08 10 1f",
    "0": "0e 11 13 15 19 11 0e", "1": "04 0c 04 04 04 04 0e",
    "2": "0e 11 01 02 04 08 1f", "3": "1e 01 01 0e 01 01 1e",
    "4": "02 06 0a 12 1f 02 02", "5": "1f 10 10 1e 01 01 1e",
    "6": "0e 10 10 1e 11 11 0e", "7": "1f 01 02 04 08 08 08",
    "8": "0e 11 11 0e 11 11 0e", "9": "0e 11 11 0f 01 01 0e",
    "?": "0e 11 01 02 04 00 04", "!": "04 04 04 04 04 00 04",
    ".": "00 00 00 00 00 00 04", ",": "00 00 00 00 00 04 08",
    ":": "00 04 00 00 04 00 00", ";": "00 04 00 00 04 08 00",
    "-": "00 00 00 1f 00 00 00", "+": "00 04 04 1f 04 04 00",
    "/": "01 01 02 04 08 10 10", "=": "00 00 1f 00 1f 00 00",
    "(": "02 04 08 08 08 04 02", ")": "08 04 02 02 02 04 08",
    "[": "0e 08 08 08 08 08 0e", "]": "0e 02 02 02 02 02 0e",
    "<": "01 02 04 08 04 02 01", ">": "10 08 04 02 04 08 10",
    "_": "00 00 00 00 00 00 1f", "'": "04 04 08 00 00 00 00",
    '"': "0a 0a 00 00 00 00 00", "%": "19 19 02 04 08 13 13",
    "*": "00 15 0e 1f 0e 15 00", "#": "0a 0a 1f 0a 1f 0a 0a",
    " ": "00 00 00 00 00 00 00",
}


def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


def generate(destination):
    destination.mkdir(parents=True, exist_ok=True)
    patterns = dict(PATTERNS)
    patterns.update({c.lower(): PATTERNS[c] for c in "ABCDEFGHIJKLMNOPQRSTUVWXYZ"})
    width, height = 128, 64
    # White RGB even for transparent texels avoids dark fringes with alpha sampling.
    pixels = bytearray([255, 255, 255, 0] * width * height)
    glyphs = []
    for index, char in enumerate(sorted(patterns)):
        x, y = (index % 16) * 8 + 1, (index // 16) * 10 + 1
        for row, value in enumerate(patterns[char].split()):
            for column in range(5):
                if int(value, 16) & (1 << (4 - column)):
                    pixels[((y + row) * width + x + column) * 4 + 3] = 255
        glyphs.append(dict(codepoint=ord(char), x=x, y=y,
                           width=0 if char == " " else 5, height=0 if char == " " else 7,
                           offsetX=0, offsetY=-7, advance=6))
    scanlines = b"".join(b"\0" + pixels[row * width * 4:(row + 1) * width * 4] for row in range(height))
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">2I5B", width, height, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(scanlines, 9)) + chunk(b"IEND", b"")
    (destination / "JanusPixel.png").write_bytes(png)
    metadata = dict(version=1, atlas="fa100000-0000-4000-8000-000000000001",
                    width=width, height=height, lineHeight=10, baseline=8, fallback=ord("?"), glyphs=glyphs)
    (destination / "JanusPixel.font.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8", newline="\n")


if __name__ == "__main__":
    generate(Path(__file__).resolve().parents[1] / "SandboxProject" / "Fonts")
