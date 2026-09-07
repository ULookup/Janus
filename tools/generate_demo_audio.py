"""Generate original PCM16 game tones using only the Python standard library."""
from pathlib import Path
import math
import struct
import wave

ROOT = Path(__file__).resolve().parents[1] / "Game" / "Audio"
RATE = 24000


def write(name, seconds, sample):
    count = round(seconds * RATE)
    pcm = bytearray()
    for i in range(count):
        value = max(-1.0, min(1.0, sample(i / RATE, seconds)))
        pcm.extend(struct.pack("<h", round(value * 32767)))
    with wave.open(str(ROOT / name), "wb") as output:
        output.setparams((1, 2, RATE, count, "NONE", "not compressed"))
        output.writeframes(pcm)


def card(t, duration):
    envelope = min(t / 0.005, 1) * (1 - t / duration) ** 2
    return 0.45 * envelope * math.sin(2 * math.pi * (520 * t + 500 * t * t))


def music(t, duration):
    # Eight half-second notes, each with zero-valued boundaries for a quiet loop seam.
    notes = (261.6256, 329.6276, 391.9954, 329.6276, 293.6648, 349.2282, 440.0, 349.2282)
    local = t % 0.5
    envelope = math.sin(math.pi * local / 0.5) ** 2
    frequency = notes[min(int(t / 0.5), 7)]
    return 0.25 * envelope * (math.sin(2 * math.pi * frequency * t)
                              + 0.2 * math.sin(4 * math.pi * frequency * t))


if __name__ == "__main__":
    ROOT.mkdir(parents=True, exist_ok=True)
    write("Card.wav", 0.18, card)
    write("Background.wav", 4.0, music)
