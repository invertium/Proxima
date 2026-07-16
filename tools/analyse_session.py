#!/usr/bin/env python3
"""Analyse a recorded SpaceGame session (Saved/SessionLogs/session_*.jsonl).

The in-game "record mode" (CVar `sg.RecordSession 1`, see
Source/SpaceGame/Core/SessionRecorderSubsystem.cpp) writes one JSON object per line:
  {"k":"meta",...}   one header per file
  {"k":"s",...}      periodic state sample (default 2 Hz)
  {"k":"e",...}      discrete event (damage / kill / death / warp / dock / mission / outcome)
  {"k":"end",...}    footer with the session duration

This prints a plain-text summary you can eyeball to tune the game: how long each
mission took, how much hull the player bled and where, kills, deaths, distance
flown, warp/dock usage. Pass --json for a machine-readable digest instead.

Usage:
    python3 tools/analyse_session.py Saved/SessionLogs/session_20260716_1200.jsonl
    python3 tools/analyse_session.py            # newest session under Saved/SessionLogs
    python3 tools/analyse_session.py --json <file>
"""
import glob
import json
import math
import os
import sys

DIFFICULTY = {0: "Ensign", 1: "Captain", 2: "Admiral"}


def load(path):
    meta, samples, events, end = None, [], [], None
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                o = json.loads(line)
            except json.JSONDecodeError:
                continue  # tolerate a truncated last line (crash mid-write)
            k = o.get("k")
            if k == "meta":
                meta = o
            elif k == "s":
                samples.append(o)
            elif k == "e":
                events.append(o)
            elif k == "end":
                end = o
    return meta, samples, events, end


def summarise(meta, samples, events, end):
    d = {}
    d["meta"] = meta or {}
    d["duration_s"] = round(end["t"], 1) if end else (round(samples[-1]["t"], 1) if samples else 0)
    d["samples"] = len(samples)

    # distance flown (planar), integrated across samples
    dist = 0.0
    for a, b in zip(samples, samples[1:]):
        dist += math.hypot(b["px"] - a["px"], b["py"] - a["py"])
    d["distance_uu"] = round(dist)
    d["distance_km"] = round(dist / 100000.0, 1)  # 1 uu = 1 cm → 100k uu = 1 km

    # hull / speed envelopes
    if samples:
        d["min_hull"] = round(min(s["hull"] for s in samples), 1)
        d["max_speed"] = round(max(s["spd"] for s in samples), 1)
        d["end_credits"] = samples[-1].get("credits", 0)
        d["end_xp"] = samples[-1].get("xp", 0)
        d["end_rank"] = samples[-1].get("rank", 1)

    # event tallies
    tally = {}
    dmg_total = 0.0
    for e in events:
        t = e.get("type", "?")
        tally[t] = tally.get(t, 0) + 1
        if t == "damage":
            dmg_total += e.get("amount", 0)
    d["events"] = tally
    d["hull_damage_taken"] = round(dmg_total, 1)

    # per-mission timeline: split time between "mission" events (and the initial mission)
    marks = [(0.0, (meta or {}).get("mission", 0), "")]
    for e in events:
        if e.get("type") == "mission":
            marks.append((e["t"], e.get("index"), e.get("name", "")))
    outcome = None
    for e in events:
        if e.get("type") in ("victory", "defeat"):
            outcome = e["type"]
    d["outcome"] = outcome or ("in-progress" if not end else "quit")
    per_mission = []
    for i, (t0, idx, name) in enumerate(marks):
        t1 = marks[i + 1][0] if i + 1 < len(marks) else d["duration_s"]
        per_mission.append({"index": idx, "name": name, "t_start": round(t0, 1), "dur_s": round(t1 - t0, 1)})
    d["missions"] = per_mission
    return d


def print_report(path, d):
    m = d["meta"]
    print(f"=== Session: {os.path.basename(path)} ===")
    print(f"  version {m.get('ver','?')}  map {m.get('map','?')}  "
          f"difficulty {DIFFICULTY.get(m.get('difficulty'), m.get('difficulty'))}  "
          f"ship#{m.get('ship','?')}  sample {m.get('sampleHz','?')}Hz")
    print(f"  duration      {d['duration_s']}s  ({d['samples']} samples)")
    print(f"  outcome       {d['outcome']}")
    print(f"  flown         {d['distance_km']} km ({d['distance_uu']} uu)   max speed {d.get('max_speed','?')} uu/s")
    print(f"  hull          min {d.get('min_hull','?')}   damage taken {d['hull_damage_taken']}")
    print(f"  economy       credits {d.get('end_credits','?')}  xp {d.get('end_xp','?')}  rank {d.get('end_rank','?')}")
    ev = d["events"]
    print(f"  events        " + ("  ".join(f"{k}:{v}" for k, v in sorted(ev.items())) or "(none)"))
    print("  missions:")
    for mm in d["missions"]:
        nm = f" — {mm['name']}" if mm["name"] else ""
        print(f"    [{mm['index']}]{nm}  starts {mm['t_start']}s  lasts {mm['dur_s']}s")


def main():
    args = [a for a in sys.argv[1:] if a != "--json"]
    as_json = "--json" in sys.argv[1:]
    if args:
        path = args[0]
    else:
        repo = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
        files = sorted(glob.glob(os.path.join(repo, "Saved", "SessionLogs", "session_*.jsonl")))
        if not files:
            sys.exit("no session logs found under Saved/SessionLogs/ (record one with `sg.RecordSession 1`)")
        path = files[-1]

    meta, samples, events, end = load(path)
    d = summarise(meta, samples, events, end)
    if as_json:
        print(json.dumps(d, indent=2))
    else:
        print_report(path, d)


if __name__ == "__main__":
    main()
