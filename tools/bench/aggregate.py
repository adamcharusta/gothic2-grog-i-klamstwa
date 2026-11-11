#!/usr/bin/env python3
"""Compare "Grog i klamstwa" benchmark logs across runs and providers.

Usage: aggregate.py [--json] <log dir or files...>

Reads session-<seed>.jsonl (one per playthrough) and summary-<seed>.json files and prints one row per
(provider, actor model). Standard library only. Exit code 2 when no session logs are found.

A turn is one player line: it ends either in a "verdicts" event from an NPC (the reply was used) or in a
"fallback" event (a canned line was used instead).
"""
import argparse
import json
import math
import sys
from pathlib import Path


def iter_files(paths):
    for p in map(Path, paths):
        if p.is_dir():
            yield from sorted(p.glob("session-*.jsonl"))
            yield from sorted(p.glob("summary-*.json"))
        elif p.is_file():
            yield p


def read_jsonl(path, stats):
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                obj = json.loads(line)
            except json.JSONDecodeError:
                stats["skipped"] += 1  # e.g. the last line of a log cut off by a crash
                continue
            if isinstance(obj, dict):
                yield obj
            else:
                stats["skipped"] += 1


def new_group(provider, model):
    return {"provider": provider, "model": model, "runs": 0, "turns": 0, "fallbacks": 0, "requests": 0,
            "errors": 0, "latencies": [], "input_tokens": 0, "output_tokens": 0, "cost_usd": 0.0,
            "verdicts": 0, "rejected": 0, "endings": {}}


def percentile(values, q):
    if not values:
        return 0.0
    s = sorted(values)
    return s[min(len(s) - 1, max(0, math.ceil(q * len(s)) - 1))]


def aggregate(paths):
    stats = {"skipped": 0}
    groups = {}
    endings = {}   # seed -> outcome
    seed_group = {}
    for path in iter_files(paths):
        if path.suffix == ".json":
            try:
                s = json.loads(path.read_text(encoding="utf-8", errors="replace"))
                if isinstance(s, dict) and "seed" in s and "outcome" in s:
                    endings.setdefault(s["seed"], s["outcome"])
            except json.JSONDecodeError:
                stats["skipped"] += 1
            continue
        g = None
        for e in read_jsonl(path, stats):
            kind = e.get("kind")
            if kind == "session":
                key = (str(e.get("provider", "?")), str(e.get("actor_model", "?")))
                g = groups.setdefault(key, new_group(*key))
                g["runs"] += 1
                seed_group[e.get("seed")] = key
                continue
            if g is None:
                continue  # lines before the session header are not attributable
            if kind == "request":
                g["requests"] += 1
                if not e.get("ok"):
                    g["errors"] += 1
                    continue
                g["latencies"].append(float(e.get("latency_ms", 0.0)))
                g["input_tokens"] += int(e.get("input_tokens", 0))
                g["output_tokens"] += int(e.get("output_tokens", 0))
                g["cost_usd"] += float(e.get("cost_usd", 0.0))
            elif kind == "verdicts":
                vs = e.get("verdicts") or []
                g["verdicts"] += len(vs)
                g["rejected"] += sum(1 for v in vs if not v.get("accepted"))
                if e.get("speaker") != "director":
                    g["turns"] += 1
            elif kind == "fallback":
                g["turns"] += 1
                g["fallbacks"] += 1
            elif kind in ("summary", "ending") and "outcome" in e:
                endings[e.get("seed")] = e["outcome"]  # the log is newer than any summary file
    for seed, outcome in endings.items():
        key = seed_group.get(seed)
        if key:
            ends = groups[key]["endings"]
            ends[outcome] = ends.get(outcome, 0) + 1

    rows = []
    for g in groups.values():
        lat = g.pop("latencies")
        rows.append({
            "provider": g["provider"], "model": g["model"], "runs": g["runs"], "turns": g["turns"],
            "requests": g["requests"], "errors": g["errors"],
            "latency_ms_avg": round(sum(lat) / len(lat), 1) if lat else 0.0,
            "latency_ms_p95": percentile(lat, 0.95), "latency_ms_max": max(lat) if lat else 0.0,
            "input_tokens": g["input_tokens"], "output_tokens": g["output_tokens"],
            "cost_usd": g["cost_usd"], "cost_usd_per_run": g["cost_usd"] / g["runs"] if g["runs"] else 0.0,
            "rejected_pct": round(100.0 * g["rejected"] / g["verdicts"], 1) if g["verdicts"] else 0.0,
            "fallback_pct": round(100.0 * g["fallbacks"] / g["turns"], 1) if g["turns"] else 0.0,
            "endings": g["endings"],
        })
    rows.sort(key=lambda r: (r["provider"], r["model"]))
    return rows, stats["skipped"]


def print_table(rows, skipped):
    cols = [("provider", "provider"), ("model", "model"), ("runs", "runs"), ("turns", "turns"), ("requests", "req"),
            ("errors", "err"), ("latency_ms_avg", "lat avg"), ("latency_ms_p95", "lat p95"), ("latency_ms_max", "lat max"),
            ("input_tokens", "tok in"), ("output_tokens", "tok out"), ("cost_usd", "cost $"), ("cost_usd_per_run", "$/run"),
            ("rejected_pct", "rej %"), ("fallback_pct", "fallb %"), ("endings", "endings")]

    def fmt(key, v):
        if key == "endings":
            return " ".join(f"{k}:{n}" for k, n in sorted(v.items())) or "-"
        if key.startswith("cost"):
            return f"{v:.4f}"
        if isinstance(v, float):
            return f"{v:.0f}" if key.startswith("latency") else f"{v:.1f}"
        return str(v)

    table = [[h for _, h in cols]] + [[fmt(k, r[k]) for k, _ in cols] for r in rows]
    widths = [max(len(row[i]) for row in table) for i in range(len(cols))]
    for row in table:
        print("  ".join(c.ljust(w) for c, w in zip(row, widths)).rstrip())
    print(f"skipped lines: {skipped}")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--json", action="store_true", help="print rows as JSON")
    ap.add_argument("paths", nargs="+", help="log directories or files")
    args = ap.parse_args(argv)
    rows, skipped = aggregate(args.paths)
    if not rows:
        print("no session logs found", file=sys.stderr)
        return 2
    if args.json:
        json.dump(rows, sys.stdout, ensure_ascii=False, indent=2)
        print()
    else:
        print_table(rows, skipped)
    return 0


if __name__ == "__main__":
    sys.exit(main())
