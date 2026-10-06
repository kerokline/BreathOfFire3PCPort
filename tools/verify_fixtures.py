#!/usr/bin/env python
"""Check local game artifacts against fixtures.toml.

No game data lives in this repository. This answers "do you and I have the same
build?" from hashes alone, so a differential result can say what it was produced
against (docs/STATUS.md, open decisions).

    python tools/verify_fixtures.py                     # check the defaults
    python tools/verify_fixtures.py --exe bof3/BOF3.exe
    python tools/verify_fixtures.py --list              # what is catalogued
    python tools/verify_fixtures.py --tree "CDImage/Breath of Fire III (USA).cue"
    python tools/verify_fixtures.py --tree bof3/DAT

`--tree` hashes every file of a disc image (.cue / .bin / .iso) or a directory
and names the build whose per-file manifest (fixtures/*.files.tsv) it matches,
file by file - the identity layer of docs/ASSET_SOURCES.md section 1. A disc's
.STR files are hashed as tools/psx_disc.py reads them (2,048 bytes a sector).

An unrecognised hash is reported as unrecognised, not wrong. It may be a build
nobody has catalogued; that is an issue to open, not a row to edit.
"""
import argparse, hashlib, os, sys, tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULTS = {
    "pc-zh:exe": os.path.join(ROOT, "bof3", "BOF3.exe"),
    "psx-jp:boot_exe": os.path.join(ROOT, "..", "BreathOfFire3Recomp", "disc", "SLPS_009.90"),
}


def digests(path, want):
    h = {k: hashlib.new(k) for k in ("md5", "sha1", "sha256") if k in want}
    n = 0
    with open(path, "rb") as fh:
        while chunk := fh.read(1 << 20):
            n += len(chunk)
            for d in h.values():
                d.update(chunk)
    return n, {k: d.hexdigest() for k, d in h.items()}


def check_tree(fx, path):
    """Match a tree against every catalogued manifest; 0 if one matches whole."""
    sys.path.insert(0, os.path.join(ROOT, "tools"))
    import region_diff
    best, trees = None, {}
    for b in fx["build"]:
        for name, art in b.get("artifacts", {}).items():
            if "manifest" not in art:
                continue
            want = {}
            with open(os.path.join(ROOT, art["manifest"]), newline="") as fh:
                for line in fh.read().replace("\r\n", "\n").splitlines():
                    n, size, h = line.split("\t")
                    want[n] = (int(size), h)
            m = art.get("match")
            if m not in trees:
                body = region_diff.manifest(path, m)[0]
                trees[m] = {n: (int(s), h) for n, s, h in (l.split("\t") for l in body.splitlines())}
            got = trees[m]
            same = sum(1 for n in want if got.get(n) == want[n])
            if best is None or same > best[0]:
                best = (same, b["id"], name, art, want, got)
    if best is None:
        print("?  no manifest in fixtures.toml")
        return 1
    same, bid, name, art, want, got = best
    missing = [n for n in want if n not in got]
    differ = [n for n in want if n in got and got[n] != want[n]]
    extra = [n for n in got if n not in want]
    if not missing and not differ and not extra:
        print("OK %s: all %d files match %s:%s" % (path, len(want), bid, name))
        return 0
    print("?  %s: closest is %s:%s - %d of %d files match, %d differ, %d missing, %d not catalogued"
          % (path, bid, name, same, len(want), len(differ), len(missing), len(extra)))
    for n in (differ + missing + extra)[:20]:
        print("   ", n)
    return 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tree", help="a disc image or a directory to identify by its per-file manifest")
    ap.add_argument("--fixtures", default=os.path.join(ROOT, "fixtures.toml"))
    ap.add_argument("--exe", help="path to check as the PC port executable")
    ap.add_argument("--psx-exe", help="path to check as the PSX boot EXE")
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args()

    fx = tomllib.load(open(a.fixtures, "rb"))
    builds = {b["id"]: b for b in fx["build"]}

    if a.tree:
        return check_tree(fx, a.tree)

    if a.list:
        for b in fx["build"]:
            arts = ", ".join(b.get("artifacts", {})) or "-"
            print(f"{b['id']:<11} {b['status']:<8} {b['language']:<9} "
                  f"{b.get('serial','-'):<11} artifacts: {arts}")
        return 0

    targets = dict(DEFAULTS)
    if a.exe:
        targets["pc-zh:exe"] = a.exe
    if a.psx_exe:
        targets["psx-jp:boot_exe"] = a.psx_exe

    # every (build, artifact) pair with hashes, for recognising an odd file
    catalogue = []
    for b in fx["build"]:
        for name, art in b.get("artifacts", {}).items():
            catalogue.append((b["id"], name, art))

    rc = 0
    for key, path in sorted(targets.items()):
        bid, art_name = key.split(":")
        art = builds.get(bid, {}).get("artifacts", {}).get(art_name)
        if art is None:
            print(f"?  {key}: not catalogued in fixtures.toml")
            rc = 1
            continue
        if not os.path.exists(path):
            print(f"-  {key}: not found at {path}")
            print(f"   (expected {art['name']}, {art['size']} bytes)")
            rc = 1
            continue
        size, got = digests(path, art)
        algo = next((k for k in ("sha256", "sha1", "md5") if k in art), None)
        if size == art["size"] and got.get(algo) == art[algo]:
            print(f"OK {key}: {art['name']} matches {bid} ({builds[bid]['language']})")
            continue
        # recognised as something else?
        other = [(i, n) for i, n, c in catalogue
                 if c.get("size") == size and any(c.get(k) == got.get(k) for k in got)]
        if other:
            print(f"!! {key}: this is {other[0][0]}:{other[0][1]}, not {bid}")
        else:
            print(f"?  {key}: unrecognised build at {path}")
            print(f"   size {size}, {algo} {got.get(algo)}")
            print(f"   expected size {art['size']}, {algo} {art[algo]}")
            print("   Not necessarily wrong — it may be an uncatalogued build.")
            print("   Open an issue with these values rather than editing the row.")
        rc = 1
    return rc


sys.exit(main())
