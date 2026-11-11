#!/usr/bin/env python3
"""Cut each pirate's original voice out of the player's own Gothic II speech archives, for local voice cloning.

Usage: extract_voices.py --gothic "<Gothic II dir>" --out "<dir>" [--seconds 20] [--data <mod/data dir>]

For every pirate in mod/data/cast.json, reads Data/Speech*.vdf (Speech_Addon.vdf first), takes the pirate's own
dialogue lines (DIA_ADDON_<NAME>_..._<voice>_NN.WAV, <NAME> = the instance name after "ADDON_", <voice> = the card's
voice.original), longest first, until --seconds of audio; if that gives under 5 s it adds the voice's SVM_<voice>_*
lines. The IMA-ADPCM clips are decoded and joined with 0.3 s of silence into <out>/<npc id>.wav (PCM 16-bit mono, the
original rate). <out>/voice_to_speaker.yaml is the matching openedai-speech (XTTS-v2) voice config.

--out is meant to be the openedai-speech "voices" folder: the yaml refers to the clips as voices/<id>.wav, relative to
the server's working directory. Copy the yaml to the server's config/voice_to_speaker.yaml (or merge its entries).

The clips are the original actors' voices from your own copy of the game: keep them on your PC, never share them.
Standard library only. Exit codes: 0 every pirate got a clip, 1 usage / bad mod data, 2 no Data/Speech*.vdf in the
Gothic dir, 3 some pirates got no clip (listed on stderr; everything else is still written).
"""
import argparse
import json
import re
import struct
import sys
import wave
from pathlib import Path

GAP_SECONDS = 0.3
MIN_OWN_SECONDS = 5.0
VDF_SIGNATURE = b"PSVDSC_V2.00\n\r\n\r"
VDF_DIRECTORY = 0x80000000
LINE_VOICE = re.compile(r"_(\d{2})_\d{2}\.WAV$")

IMA_STEPS = [
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
    107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871,
    5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623,
    27086, 29794, 32767,
]
IMA_INDEX_ADJ = [-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8]

# openedai-speech config format, checked 2026-11-11 against https://github.com/matatonic/openedai-speech
# (README "Custom Voices" + voice_to_speaker.default.yaml: tts-1-hd -> <voice>: model xtts, speaker voices/<f>.wav,
# language <code>; "pl" is in the XTTS language list). The repository was archived on 2026-01-04.
YAML_HEADER = """\
# openedai-speech voice config (config/voice_to_speaker.yaml), written by tools/voices/extract_voices.py.
# Format: https://github.com/matatonic/openedai-speech (README "Custom Voices", voice_to_speaker.default.yaml),
# checked 2026-11-11. The speaker paths are relative to the server's working directory (its voices/ folder).
# The clips are the original actors' voices from your own copy of Gothic II: keep them private, never share them.
tts-1-hd:
"""


class Entry:
    __slots__ = ("name", "vdf", "offset", "size")

    def __init__(self, name, vdf, offset, size):
        self.name, self.vdf, self.offset, self.size = name, vdf, offset, size

    def read(self):
        with open(self.vdf, "rb") as f:
            f.seek(self.offset)
            return f.read(self.size)


def read_vdf_index(path):
    """Returns the file entries of a VDF v2 archive (directories skipped). Raises ValueError if it is not one or its
    index does not fit in the file; entries whose data runs past the end of the file are skipped with a warning."""
    with open(path, "rb") as f:
        file_size = f.seek(0, 2)
        f.seek(0)
        head = f.read(296)
        if len(head) < 296 or head[256:272] != VDF_SIGNATURE:
            raise ValueError("not a VDF v2 archive")
        count, _files, _ts, _size, root, _version = struct.unpack("<6I", head[272:296])
        if root < 296 or root > file_size or count > (file_size - root) // 80:
            raise ValueError(f"index ({count} entries at offset {root}) does not fit in the file")
        f.seek(root)
        table = f.read(count * 80)
    if len(table) < count * 80:
        raise ValueError("truncated index")
    entries, broken = [], 0
    for i in range(count):
        e = table[i * 80:(i + 1) * 80]
        offset, size, kind, _attr = struct.unpack("<4I", e[64:80])
        if kind & VDF_DIRECTORY:
            continue
        if offset + size > file_size:
            broken += 1
            continue
        name = e[:64].rstrip(b" \0").decode("latin-1")
        entries.append(Entry(name, path, offset, size))
    if broken:
        print(f"warning: {Path(path).name}: skipped {broken} entries that run past the end of the file",
              file=sys.stderr)
    return entries


def find_archives(gothic):
    """Data/Speech*.vdf, Speech_Addon first (it overrides the base game), names matched case-insensitively."""
    data_dirs = [d for d in Path(gothic).iterdir() if d.is_dir() and d.name.lower() == "data"] \
        if Path(gothic).is_dir() else []
    found = []
    for d in data_dirs:
        found += [p for p in d.iterdir()
                  if p.is_file() and p.name.lower().startswith("speech") and p.name.lower().endswith(".vdf")]
    return sorted(found, key=lambda p: (p.name.lower() != "speech_addon.vdf", p.name.lower()))


def parse_wav(blob):
    """Returns (format tag, channels, rate, block align, bits, sample count, data bytes) or None."""
    if len(blob) < 12 or blob[:4] != b"RIFF" or blob[8:12] != b"WAVE":
        return None
    pos, fmt, fact, data = 12, None, None, None
    while pos + 8 <= len(blob):
        cid, size = blob[pos:pos + 4], struct.unpack("<I", blob[pos + 4:pos + 8])[0]
        body = blob[pos + 8:pos + 8 + size]
        if cid == b"fmt " and len(body) >= 16:
            fmt = struct.unpack("<HHIIHH", body[:16]) + (body[16:],)
        elif cid == b"fact" and len(body) >= 4:
            fact = struct.unpack("<I", body[:4])[0]
        elif cid == b"data":
            data = body
        pos += 8 + size + (size & 1)
    if fmt is None or data is None:
        return None
    tag, channels, rate, _bps, align, bits, extra = fmt
    if channels != 1:
        return None
    if tag == 1 and bits == 16:
        count = len(data) // 2
    elif tag == 17 and bits == 4 and align > 4:
        per_block = (align - 4) * 2 + 1
        full, rest = divmod(len(data), align)
        count = full * per_block + ((rest - 4) * 2 + 1 if rest > 4 else 0)
        if fact is not None:
            count = min(count, fact)
    else:
        return None
    return tag, rate, align, count, data


def decode_ima(data, align, count):
    """IMA ADPCM mono: per block an int16 predictor, a uint8 step index, a reserved byte, then 4-bit codes, low
    nibble first. Returns `count` int16 samples."""
    out = []
    steps, adj = IMA_STEPS, IMA_INDEX_ADJ
    for start in range(0, len(data), align):
        if len(out) >= count:
            break
        block = data[start:start + align]
        if len(block) < 4:
            break
        pred, index = struct.unpack("<hB", block[:3])
        index = min(index, 88)
        out.append(pred)
        for byte in block[4:]:
            for code in (byte & 15, byte >> 4):
                step = steps[index]
                delta = step >> 3
                if code & 4:
                    delta += step
                if code & 2:
                    delta += step >> 1
                if code & 1:
                    delta += step >> 2
                pred = pred - delta if code & 8 else pred + delta
                pred = -32768 if pred < -32768 else 32767 if pred > 32767 else pred
                index += adj[code]
                index = 0 if index < 0 else 88 if index > 88 else index
                out.append(pred)
    return out[:count]


def decode(clip):
    tag, _rate, align, count, entry = clip
    data = parse_wav(entry.read())[4]
    if tag == 1:
        return list(struct.unpack(f"<{count}h", data[:count * 2]))
    return decode_ima(data, align, count)


def load_cast(data_dir):
    cast = json.loads((data_dir / "cast.json").read_text(encoding="utf-8"))
    pirates = []
    for npc in cast["npcs"]:
        instance = npc["instance"].upper()
        if "ADDON_" not in instance:
            raise ValueError(f"{npc['id']}: instance {instance} has no ADDON_ part")
        card = json.loads((data_dir / "npc" / f"{npc['id']}.json").read_text(encoding="utf-8"))
        pirates.append((npc["id"], instance.split("ADDON_", 1)[1], int(card["voice"]["original"])))
    return pirates


def pick(candidates, seconds, already=0.0):
    """Longest first until the total reaches `seconds` (the last clip may overshoot)."""
    chosen, total = [], already
    for name, clip in sorted(candidates, key=lambda c: (-c[1][3] / c[1][1], c[0])):
        if total >= seconds:
            break
        chosen.append((name, clip))
        total += clip[3] / clip[1]
    return chosen, total


class _Parser(argparse.ArgumentParser):
    def error(self, message):
        """Usage errors exit 1 (argparse's own 2 means "no speech archives" for this tool)."""
        self.print_usage(sys.stderr)
        self.exit(1, f"{self.prog}: error: {message}\n")


def main(argv=None):
    ap = _Parser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--gothic", required=True, help="Gothic II install dir (the one holding Data/Speech*.vdf)")
    ap.add_argument("--out", required=True, help="output dir: the openedai-speech voices/ folder")
    ap.add_argument("--seconds", type=float, default=20.0, help="target clip length per pirate (default 20)")
    ap.add_argument("--data", default=str(Path(__file__).resolve().parents[2] / "mod" / "data"),
                    help="mod data dir with cast.json and npc/ (default: ../../mod/data next to this tool)")
    args = ap.parse_args(argv)

    try:
        pirates = load_cast(Path(args.data))
    except (OSError, ValueError, KeyError, TypeError) as e:
        print(f"error: cannot read the cast from {args.data}: {e}", file=sys.stderr)
        return 1

    archives = find_archives(args.gothic)
    index = {}
    for vdf in archives:
        try:
            entries = read_vdf_index(vdf)
        except (OSError, ValueError) as e:
            print(f"warning: skipping {vdf.name}: {e}", file=sys.stderr)
            continue
        for e in entries:
            index.setdefault(e.name.upper(), e)  # the first archive (Speech_Addon) wins
    if not index:
        print(f"error: no Data/Speech*.vdf found in {args.gothic}", file=sys.stderr)
        return 2

    header_cache = {}

    def clips(prefix, voice=None):
        """(name, parsed wav) for every WAV whose name starts with prefix (and has the voice digits, if given)."""
        result = []
        for name, entry in index.items():
            if not name.startswith(prefix) or not name.endswith(".WAV"):
                continue
            if voice is not None:
                m = LINE_VOICE.search(name)
                if not m or int(m.group(1)) != voice:
                    continue
            if name not in header_cache:
                parsed = parse_wav(entry.read())  # keep the header only; the audio is read again when used
                header_cache[name] = parsed[:4] + (entry,) if parsed else None
            if header_cache[name] is not None and header_cache[name][3] > 0:
                result.append((name, header_cache[name]))
        return result

    out = Path(args.out)
    done, missing = [], []
    for npc_id, name, voice in pirates:
        chosen, total = pick(clips(f"DIA_ADDON_{name}_", voice), args.seconds)
        if total < MIN_OWN_SECONDS:
            more, total = pick(clips(f"SVM_{voice}_"), args.seconds, total)
            chosen += more
        rate = chosen[0][1][1] if chosen else 0
        chosen = [c for c in chosen if c[1][1] == rate]
        if not chosen:
            missing.append(npc_id)
            continue
        gap = [0] * round(GAP_SECONDS * rate)
        samples = []
        for i, (_n, clip) in enumerate(chosen):
            if i:
                samples += gap
            samples += decode(clip)
        out.mkdir(parents=True, exist_ok=True)
        with wave.open(str(out / f"{npc_id}.wav"), "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(rate)
            w.writeframes(struct.pack(f"<{len(samples)}h", *samples))
        done.append(npc_id)
        print(f"{npc_id}: {len(samples) / rate:.1f} s from {len(chosen)} clips")

    if done:
        lines = [YAML_HEADER]
        for npc_id in done:
            lines.append(f"  {npc_id}:\n    model: xtts\n    speaker: voices/{npc_id}.wav\n    language: pl\n")
        with open(out / "voice_to_speaker.yaml", "w", encoding="utf-8", newline="\n") as f:
            f.write("".join(lines))
    if missing:
        print("error: no clip found for: " + ", ".join(missing), file=sys.stderr)
        return 3
    return 0


if __name__ == "__main__":
    sys.exit(main())
