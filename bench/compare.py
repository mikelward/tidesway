#!/usr/bin/env python3
"""The rows of tidewm's experiments/sway/REPORT.md "Measurements", for
tidewm and tidesway, from bench.py's results:

    compare.py RESULTS_DIR

RESULTS_DIR holds tidewm-bench.json and tidesway-bench.json. It prints
Markdown: times as median / p95 in ms, counts as runs out of all runs.
"""

import json
import os
import sys

MODES = ("tidewm", "tidesway")


def load(d, mode):
    path = os.path.join(d, f"{mode}-bench.json")
    try:
        with open(path) as f:
            return json.load(f)
    except (OSError, ValueError) as e:
        print(f"compare.py: can't read {path}: {e}", file=sys.stderr)
        return {}


def time(b, key):
    s = (b.get("summary") or {}).get(key)
    return f"{round(s['median'], 3):g} / {round(s['p95'], 3):g}" if s else "–"


def rows(b, section, kind):
    found = b.get(section)
    if not isinstance(found, list):
        return []
    return [r for r in found if r.get("kind") == kind]


def runs_with(b, section, kind, field):
    """How many runs had field nonzero, of how many ran"""
    rs = rows(b, section, kind)
    if not rs:
        return "–"
    return f"{sum(1 for r in rs if r.get(field))} / {len(rs)}"


def total(b, section, kind, field):
    rs = rows(b, section, kind)
    if not rs:
        return "–"
    return f"{sum(r.get(field) or 0 for r in rs)} in {len(rs)} runs"


def table(title, lines, benches):
    print(f"**{title}**\n")
    print("| | " + " | ".join(MODES) + " |")
    print("|---|" + "---|" * len(MODES))
    for label, fn in lines:
        print(f"| {label} | " + " | ".join(fn(benches[m]) for m in MODES) + " |")
    print()


def main(d):
    benches = {m: load(d, m) for m in MODES}
    table("Latency (ms, median / p95)", [
        ("New window: seen → its `new` event reaches tide",
         lambda b: time(b, "open_close.open.seen_to_event")),
        ("New window: seen → final geometry", lambda b: time(b, "open_close.open.seen_to_settled")),
        ("Close: gone → final geometry", lambda b: time(b, "open_close.close.gone_to_settled")),
        ("Workspace switch: command → reply", lambda b: time(b, "commands.workspace.reply")),
        ("Workspace switch: command → tree settled",
         lambda b: time(b, "commands.workspace.settled")),
        ("`focus next`: command → reply", lambda b: time(b, "commands.focus.reply")),
        ("Granted window: seen → focused", lambda b: time(b, "guard.granted.seen_to_focused")),
    ], benches)
    table("Intermediate states and flicker", [
        ("New window: runs with tree states before the final one",
         lambda b: runs_with(b, "open_close", "open", "intermediate_states")),
        ("New window: runs with focus changes before settling",
         lambda b: runs_with(b, "open_close", "open", "focus_changes_before_settled")),
        ("Close: runs with tree states before the final one",
         lambda b: runs_with(b, "open_close", "close", "intermediate_states")),
        ("Close: runs with a wrong frame on screen",
         lambda b: runs_with(b, "flicker", "close", "wrong_frames")),
        ("Open: runs with a wrong frame on screen",
         lambda b: runs_with(b, "flicker", "open", "wrong_frames")),
        ("Held window (guard on): tree samples where it had focus",
         lambda b: total(b, "guard", "held", "samples_with_wrong_focus")),
    ], benches)
    table("IPC cost (ms, median / p95)", [
        ("`GET_VERSION` direct", lambda b: time(b, "ipc.get_version_direct")),
        ("`GET_TREE` direct", lambda b: time(b, "ipc.get_tree_direct")),
    ], benches)
    print("**Layout parity with tidewm** (x, y, w, h)\n")
    ref, got = benches["tidewm"].get("layouts"), benches["tidesway"].get("layouts")
    if not ref or not got:
        print("- no data\n")
    else:
        diffs = [(a["step"], a["rects"], b["rects"]) for a, b in zip(ref, got)
                 if a["rects"] != b["rects"]]
        print(f"- {len(ref) - len(diffs)} of {len(ref)} steps match exactly")
        for step, a, b in diffs:
            print(f"  - {step}: tidewm {a}, tidesway {b}")
        print()
    print("**Frames and errors**\n")
    for m in MODES:
        b = benches[m]
        print(f"- {m}: frames {b.get('frames')}; errors {b.get('errors')}")


if __name__ == "__main__":
    main(sys.argv[1])
