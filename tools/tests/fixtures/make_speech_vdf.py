#!/usr/bin/env python3
"""Build a tiny synthetic Gothic speech archive for tools/tests/test_voices.sh.

Usage: make_speech_vdf.py [--all] <gothic dir> <ref dir>

Writes <gothic dir>/Data/Speech_Addon.vdf (VDF v2) holding IMA-ADPCM WAVs (format tag 17, 44100 Hz, mono, block align
1024, 4 bits) encoded from synthetic sine tones, and for every WAV <ref dir>/<NAME>.raw with the int16 LE samples the
encoder's own reconstruction produces (what a correct decoder must output). Prints a JSON map name -> sample count.
With --all, adds one SVM_<voice>_TEST.WAV per original voice of the cast, so every pirate gets a clip.
No game data is involved: everything here is generated.
"""
import json
import math
import struct
import sys
from pathlib import Path

RATE = 44100
BLOCK_ALIGN = 1024
SAMPLES_PER_BLOCK = (BLOCK_ALIGN - 4) * 2 + 1  # 2041

STEPS = [
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
    107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871,
    5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623,
    27086, 29794, 32767,
]
INDEX_ADJ = [-1, -1, -1, -1, 2, 4, 6, 8]


def clamp(v, lo, hi):
    return lo if v < lo else hi if v > hi else v


def encode_nibble(sample, pred, index):
    step = STEPS[index]
    diff = sample - pred
    code = 0
    if diff < 0:
        code = 8
        diff = -diff
    delta = step >> 3
    if diff >= step:
        code |= 4
        diff -= step
        delta += step
    step >>= 1
    if diff >= step:
        code |= 2
        diff -= step
        delta += step
    step >>= 1
    if diff >= step:
        code |= 1
        delta += step
    pred = pred - delta if code & 8 else pred + delta
    pred = clamp(pred, -32768, 32767)
    index = clamp(index + INDEX_ADJ[code & 7], 0, 88)
    return code, pred, index


def encode(samples):
    """IMA ADPCM, mono, 1024-byte blocks. Returns (data bytes, reconstructed samples)."""
    out = bytearray()
    recon = []
    index = 0
    for start in range(0, len(samples), SAMPLES_PER_BLOCK):
        block = samples[start:start + SAMPLES_PER_BLOCK]
        pred = block[0]
        out += struct.pack("<hBB", pred, index, 0)
        recon.append(pred)
        codes = []
        for s in block[1:]:
            code, pred, index = encode_nibble(s, pred, index)
            codes.append(code)
            recon.append(pred)
        if len(codes) % 2:
            codes.append(0)  # padding nibble; a correct decoder relies on the fact chunk to drop it
        for i in range(0, len(codes), 2):
            out.append(codes[i] | (codes[i + 1] << 4))
    return bytes(out), recon


def wav(samples, with_fact=True):
    data, recon = encode(samples)
    fmt = struct.pack("<HHIIHHHH", 17, 1, RATE, RATE * BLOCK_ALIGN // SAMPLES_PER_BLOCK, BLOCK_ALIGN, 4, 2,
                      SAMPLES_PER_BLOCK)
    body = b"WAVE" + b"fmt " + struct.pack("<I", len(fmt)) + fmt
    if with_fact:
        body += b"fact" + struct.pack("<II", 4, len(samples))
    body += b"data" + struct.pack("<I", len(data)) + data
    if len(data) % 2:
        body += b"\0"
    return b"RIFF" + struct.pack("<I", len(body)) + body, recon


def tone(n, freq, amp=8000):
    return [int(amp * math.sin(2 * math.pi * freq * i / RATE)) for i in range(n)]


def vdf(files):
    """files: list of (name, bytes). One root directory entry ('SOUND') holding all files, like the game's archives."""
    entries = [("SOUND", 1, 0, 0x80000000 | 0x40000000, 0)]
    count = 1 + len(files)
    root = 296
    offset = root + count * 80
    blobs = []
    for i, (name, blob) in enumerate(files):
        last = 0x40000000 if i == len(files) - 1 else 0
        entries.append((name, offset, len(blob), last, 32))
        blobs.append(blob)
        offset += len(blob)
    head = b"Synthetic test archive".ljust(256, b"\x1a")
    head += b"PSVDSC_V2.00\n\r\n\r"
    head += struct.pack("<6I", count, len(files), 0, offset - root, root, 80)
    table = b"".join(n.encode("ascii").ljust(64, b" ") + struct.pack("<4I", o, s, t, a)
                     for n, o, s, t, a in entries)
    return head + table + b"".join(blobs)


def main(argv):
    full = "--all" in argv
    args = [a for a in argv if a != "--all"]
    if len(args) != 2:
        print(__doc__, file=sys.stderr)
        return 1
    gothic, ref = Path(args[0]), Path(args[1])
    # (name, samples, with fact chunk)
    spec = [
        ("DIA_ADDON_GREG_HELLO_01_00.WAV", tone(3000, 220), True),
        # lower case name, no fact chunk: the last block holds 919 samples (918 nibbles, no padding)
        ("dia_addon_greg_bye_01_02.wav", tone(2 * SAMPLES_PER_BLOCK + 919, 330), False),
        # the hero's line in a Greg dialogue: longest, must never be used
        ("DIA_ADDON_GREG_HELLO_15_00.WAV", tone(20000, 440), True),
        ("DIA_ADDON_SKIP_X_08_01.WAV", tone(SAMPLES_PER_BLOCK, 550), True),
        ("SVM_6_ADDON_HELLO.WAV", tone(4000, 260), True),
    ]
    if full:
        cast = Path(__file__).resolve().parents[3] / "mod" / "data"
        ids = [n["id"] for n in json.loads((cast / "cast.json").read_text(encoding="utf-8"))["npcs"]]
        voices = sorted({json.loads((cast / "npc" / f"{i}.json").read_text(encoding="utf-8"))["voice"]["original"]
                         for i in ids})
        spec += [(f"SVM_{v}_TEST.WAV", tone(1500, 200 + 10 * v), True) for v in voices if v != 6]
    files, counts = [], {}
    ref.mkdir(parents=True, exist_ok=True)
    for name, samples, fact in spec:
        blob, recon = wav(samples, fact)
        files.append((name, blob))
        counts[name] = len(recon)
        (ref / (Path(name).stem.upper() + ".raw")).write_bytes(struct.pack(f"<{len(recon)}h", *recon))
    (gothic / "Data").mkdir(parents=True, exist_ok=True)
    (gothic / "Data" / "Speech_Addon.vdf").write_bytes(vdf(files))
    print(json.dumps(counts))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
