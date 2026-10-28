#!/usr/bin/env python3
"""Writes a .chimeraProject for run-frontend.sh, by hand, as the wizard would.

A Retro Engine v3 project is its settings, its firmware pin (the data file,
by SHA-1) and an input log in the panel's own button names. The movie, if
given, is one of the gate's (a frame a line, the buttons held as letters:
U D L R, A B C, S Start, E Dev Menu; # comments), turned into Chimera's log rows.

usage: make-project.py <package> <out.chimeraProject> <frames> --data <Data.rsdk>
                       [--movie <movie>] [--settings <json>] [--log-out <path>]
--log-out also writes the rows alone, as chimera-run takes a movie.
"""
import argparse
import hashlib
import json
import zipfile


def inputs_of(log):
    """A movie's text block ("[Input]", "LogKey:...", its entries, "[/Input]")
    as a project file's "inputs": the key's groups of column names, and one
    entry a frame. A group starts where a '#' begins a name; a name ends at
    '|' (as Chimera splits a key)."""
    lines = log.splitlines()
    out = {}
    key = next((l[len("LogKey:"):] for l in lines if l.startswith("LogKey:")), None)
    if key is not None:
        groups, at = [], 0
        while at < len(key):
            assert key[at] == "#", key
            groups.append([])
            at += 1
            while at < len(key) and key[at] != "#":
                bar = key.index("|", at)
                groups[-1].append(key[at:bar])
                at = bar + 1
        out["columns"] = groups
    out["entries"] = [l for l in lines if l.startswith("|")]
    return out

LETTERS = {"P1 Up": "U", "P1 Down": "D", "P1 Left": "L", "P1 Right": "R",
           "P1 A": "A", "P1 B": "B", "P1 C": "C", "P1 Start": "S", "Dev Menu": "E"}


def sha1(path):
    h = hashlib.sha1()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest().upper()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("package")
    ap.add_argument("out")
    ap.add_argument("frames", type=int)
    ap.add_argument("--data", required=True)
    ap.add_argument("--movie")
    ap.add_argument("--settings", default="{}")
    ap.add_argument("--log-out")
    a = ap.parse_args()

    cfg = json.loads(zipfile.ZipFile(a.package).read("waterbox.config"))
    settings = {d["name"]: d["default"] for d in cfg["settings"]}
    settings.update(json.loads(a.settings))
    # a movie carries the live controls only: Dev Menu is there only with the
    # Developer Menu setting on (wbx-entry.c IsButtonActive)
    buttons = [b for b in cfg["input"]["buttons"] if b != "Dev Menu" or settings.get("devMenu")]
    rows = []
    if a.movie:
        for line in open(a.movie):
            if line.startswith("#"):
                continue
            held = line.strip()
            rows.append("".join(LETTERS[b] if LETTERS[b] in held else "." for b in buttons))
    while len(rows) < a.frames:
        rows.append("." * len(buttons))
    rows = ["|" + r + "|" for r in rows[:a.frames]]
    if a.log_out:
        open(a.log_out, "w").write("\n".join(rows) + "\n")
    log = "[Input]\nLogKey:#" + "".join(b + "|" for b in buttons) + "\n" + "\n".join(rows) + "\n[/Input]\n"

    project = {
        "id": "rsdkv3-frontend-gate",
        "title": "Retro Engine v3 through Chimera",
        "description": "written by waterbox/tests/run-frontend.sh",
        "core": {"name": cfg["coreName"], "version": cfg["version"], "sha1": sha1(a.package)},
        "rerecords": 0,
        "files": [],
        "settings": settings,
        "firmware": [{"id": d["id"], "sha1": sha1(a.data)} for d in cfg["firmware"]],
        "coreCache": [],
        "inputs": inputs_of(log),
        "markers": [],
        "branches": [],
        "headers": {
            "MovieVersion": "Chimera Project File v1.1",
            "Platform": cfg["systemId"],
            "SHA1": "",
            "LastInputFrame": str(a.frames - 1),
            "VsyncNumerator": str(cfg["video"]["vsyncNumerator"]),
            "VsyncDenominator": str(cfg["video"]["vsyncDenominator"]),
        },
    }
    with open(a.out, "w") as f:
        json.dump(project, f, indent="\t")


if __name__ == "__main__":
    main()
