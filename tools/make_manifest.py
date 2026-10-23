#!/usr/bin/env python3
"""Writes manifest.json for a release: what the devices read to find out whether a newer firmware exists.

  tools/make_manifest.py <dist dir> <tag> <owner/repo>

The dist dir holds the glucopanel-<board>-update.bin files (tools/package_release.sh). Keys are the
PlatformIO env names, which is what BOARD_ID is in the firmware (tools/version.py).
"""
import hashlib
import json
import os
import sys

BOARDS = ["reterminal_e1001", "jc3248w535"]

dist, tag, repo = sys.argv[1:4]
manifest = {"version": tag.lstrip("v")}
for board in BOARDS:
    name = "glucopanel-%s-update.bin" % board.replace("_", "-")
    path = os.path.join(dist, name)
    with open(path, "rb") as f:
        data = f.read()
    manifest[board] = {
        "url": "https://github.com/%s/releases/download/%s/%s" % (repo, tag, name),
        "sha256": hashlib.sha256(data).hexdigest(),
        "size": len(data),
    }
with open(os.path.join(dist, "manifest.json"), "w") as f:
    json.dump(manifest, f, indent=2)
    f.write("\n")
