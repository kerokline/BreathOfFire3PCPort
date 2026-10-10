#!/usr/bin/env python
"""The engine's cache walk (DIV-0089) against `importer.py install`'s.

docs/cache-read.md. With BOF3X_CACHE set, LoadDatFile (src/game/dat_load.cpp,
src/game/dat_cache.cpp) reads the shipped DAT\\NAME from the cache's
base/dat/ and loc/zh-CN/dat/ in the manifest's slot order when the cache holds
it whole, the install's file otherwise; then the language overlay and each
optional layer from the cache when the cache has that layer at all, else the
install's DAT\\<layer>.NAME. This tool restates that rule in Python and holds
it to what the install's own walk gives after `importer.py install` copied the
same layers into DAT/ - the two ways a player can play one cache must agree.

    python tools/cache_walk.py check
        no game data: a synthetic install and cache in a temporary directory,
        importer.py's own cmd_install run on it, every container's end state
        compared; and the controls (a walk without the slots, a precedence
        that mixes the install's stale overlay in) must disagree. importer.py
        check runs this too.
    python tools/cache_walk.py compare --game DIR --cache CACHE [--lang TAG] [--opt LAYER ...]
        a real install and cache, read only: every container of DIR/DAT
        walked both ways (the install's walk after a virtual install: the
        cache's layers under DAT/'s names, stale files dropped, as
        cmd_install does), the end states compared - arena bytes by
        address, VRAM tiles by place, and the other kinds' chunks in order.

The end state, not the chunk order, is compared: the engine lays an enemy
table's names after the container's other slots, where the PC's chunk
carries them in place, and that is the same bytes when no later chunk
overlaps the table (dat_cache.h). Nothing is written to DIR.
"""
import argparse
import collections
import hashlib
import os
import shutil
import struct
import sys
import tempfile
import tomllib

TOOLS = os.path.dirname(os.path.abspath(__file__))
TARGET_LAYER = "loc/zh-CN"


def chunks(path):
    """(kind, tag, payload) of each chunk of a DAT container."""
    with open(path, "rb") as f:
        blob = f.read()
    out, pos = [], 0
    while pos < len(blob):
        kind, = struct.unpack_from("<b", blob, pos)        # the s8 LoadDatFile switches on
        tag, size = struct.unpack_from("<Ii", blob, pos + 4)
        out.append((kind, tag, blob[pos + 16:pos + 16 + size]))
        pos += 16 + size
    return out


def container(parts):
    return b"".join(struct.pack("<IIiI", kind & 0xFF, tag, len(body), 0) + body for kind, tag, body in parts)


class State:
    """What LoadDatFile's handlers leave: kind-0 bytes by arena address, kind-1
    32x32 tiles by VRAM place, every other kind's chunks in order."""

    def __init__(self):
        self.arena, self.vram, self.seq = {}, {}, []

    def take(self, kind, tag, body):
        if kind == 0:
            for i, b in enumerate(body):
                self.arena[tag + i] = b
        elif kind == 1:
            x0, y, w = (tag >> 19) & 0x1FE0, (tag >> 11) & 0x1FE0, (tag >> 3) & 0x1FE0
            x = x0
            for k in range(len(body) >> 11):
                self.vram[(x, y)] = body[k * 0x800:(k + 1) * 0x800]
                x += 32
                if x == x0 + w:
                    x, y = x0, y + 32
        elif 2 <= kind <= 16:
            self.seq.append((kind, tag, hashlib.sha256(body).hexdigest()))

    def walk(self, path):
        if path and os.path.isfile(path):
            for c in chunks(path):
                self.take(*c)

    def key(self):
        return (hashlib.sha256(repr(sorted(self.arena.items())).encode()).hexdigest(),
                hashlib.sha256(repr(sorted(self.vram.items())).encode()).hexdigest(), tuple(self.seq))


def index(cache):
    """The manifest's containers as dat_cache.cpp Parse reads them: the slot
    order ('b' / 'z'), whether the zh file is needed, whether a row had no source."""
    p = os.path.join(cache, "manifest.toml")
    if not os.path.isfile(p):
        return {}
    with open(p, "rb") as f:
        rows = tomllib.load(f)["cache"]["assets"]
    out = collections.OrderedDict()
    for name, slot, layer, where, _ in rows:
        c = out.setdefault(name.upper(), {"order": "", "zh": False, "missing": False})
        how = where.split(":")[1] if where else ""
        if how == "names":
            c["zh"] = True
            continue
        assert slot == len(c["order"]), (name, slot)
        c["order"] += "b" if layer == "base" else "z"
        c["zh"] |= layer == TARGET_LAYER or how == "widen"
        c["missing"] |= not where
    return out


def has_layer(cache, kind, layer):
    d = os.path.join(cache, kind, layer, "dat")
    return os.path.isdir(d) and any(f.upper().endswith(".DAT") for f in os.listdir(d))


def held(cache, name, c):
    b = os.path.join(cache, "base", "dat", name)
    z = os.path.join(cache, *TARGET_LAYER.split("/"), "dat", name)
    return not c["missing"] and ("b" not in c["order"] or os.path.isfile(b)) and (not c["zh"] or os.path.isfile(z)), b, z


def engine_walk(game, cache, name, lang, opts, idx, mixing=False, slotless=False):
    """DIV-0089's LoadDatFile. `mixing` and `slotless` are the controls."""
    st = State()
    c = idx.get(name.upper())
    whole, b, z = held(cache, name, c) if c else (False, None, None)
    if whole:
        bs = chunks(b) if "b" in c["order"] else []
        zs = chunks(z) if c["zh"] else []
        order = sorted(c["order"]) if slotless else c["order"]
        at = {"b": 0, "z": 0}
        for o in order:
            st.take(*(bs if o == "b" else zs)[at[o]])
            at[o] += 1
        for ch in zs[at["z"]:]:          # the enemy names
            st.take(*ch)
    else:
        st.walk(os.path.join(game, "DAT", name))
    for kind, layer in ([("loc", lang)] if lang else []) + [("opt", o) for o in opts]:
        mine = os.path.join(cache, kind, layer, "dat", name)
        theirs = os.path.join(game, "DAT", "%s.%s" % (layer, name))
        if has_layer(cache, kind, layer):
            st.walk(mine if os.path.isfile(mine) else (theirs if mixing else None))
        else:
            st.walk(theirs)
    return st.key()


def virtual_install(game, cache, lang, opts):
    """DAT/'s files after cmd_install of these layers, as {name: path}: each
    layer the cache has replaces the install's <layer>.* whole."""
    files = {f: os.path.join(game, "DAT", f) for f in os.listdir(os.path.join(game, "DAT"))}
    for kind, layer in ([("loc", lang)] if lang else []) + [("opt", o) for o in opts]:
        if not has_layer(cache, kind, layer):
            continue
        for f in [f for f in files if f.startswith(layer + ".")]:
            del files[f]
        src = os.path.join(cache, kind, layer, "dat")
        for f in os.listdir(src):
            files["%s.%s" % (layer, f)] = os.path.join(src, f)
    return files


def install_walk(files, name, lang, opts):
    """The engine without a cache: DIV-0005's and DIV-0086's walk over DAT/."""
    st = State()
    upper = {k.upper(): v for k, v in files.items()}
    st.walk(upper.get(name.upper()))
    for layer in ([lang] if lang else []) + list(opts):
        st.walk(upper.get(("%s.%s" % (layer, name)).upper()))
    return st.key()


def shipped(game):
    return sorted(f for f in os.listdir(os.path.join(game, "DAT")) if f.count(".") == 1 and f.upper().endswith(".DAT"))


def compare(game, cache, lang, opts, **control):
    idx = index(cache)
    files = virtual_install(game, cache, lang, opts)
    same, differ = 0, []
    for name in shipped(game):
        if engine_walk(game, cache, name, lang, opts, idx, **control) == install_walk(files, name, lang, opts):
            same += 1
        else:
            differ.append(name)
    return same, differ, idx


def check():
    """The synthetic case; returns a list of errors (importer.py check)."""
    sys.path.insert(0, TOOLS)
    import importer
    errs = []
    with tempfile.TemporaryDirectory() as tmp:
        game, cache = os.path.join(tmp, "game"), os.path.join(tmp, "cache")
        for d in ("game/DAT", "cache/base/dat", "cache/loc/zh-CN/dat", "cache/loc/xx-YY/dat", "cache/opt/lay/dat"):
            os.makedirs(os.path.join(tmp, *d.split("/")))

        def put(rel, parts):
            with open(os.path.join(tmp, *rel.split("/")), "wb") as f:
                f.write(container(parts))
        # A.DAT as the PC ships it: base, zh, base - the zh image under the
        # base one's tiles (FIRST.DAT's shape), so the slot order matters.
        b0, z0, b1 = (0, 0x1000, b"\x01" * 8), (1, 0x1C080200, b"\x02" * 0x1000), (1, 0x1A080400, b"\x03" * 0x2000)
        put("game/DAT/A.DAT", [b0, z0, b1])
        put("game/DAT/FIRST.DAT", [(0, 0x2000, b"\x04" * 4)])
        put("game/DAT/xx-YY.A.DAT", [(0, 0x3000, b"\x05" * 4)])       # an old language overlay
        put("game/DAT/lay.A.DAT", [(0, 0x1000, b"\x06" * 4)])         # stale: the cache's lay has no A
        put("cache/base/dat/A.DAT", [b0, b1])
        put("cache/loc/zh-CN/dat/A.DAT", [z0])
        put("cache/loc/xx-YY/dat/A.DAT", [(0, 0x3000, b"\x07" * 4)])
        put("cache/opt/lay/dat/FIRST.DAT", [(0, 0x2000, b"\x08" * 4)])
        h = "a" * 64
        with open(os.path.join(cache, "manifest.toml"), "w", newline="\n") as f:
            f.write('[meta]\ntarget = "pc-zh"\n\n[cache]\nassets = [\n'
                    '  ["A.DAT", 0, "base", "pc-zh:chunk", "%s"],\n  ["A.DAT", 1, "loc/zh-CN", "pc-zh:chunk", "%s"],\n'
                    '  ["A.DAT", 2, "base", "pc-zh:chunk", "%s"],\n]\n' % (h, h, h))
        lang, opts = "xx-YY", ["lay"]
        want = compare(game, cache, lang, opts)
        if want[1]:
            errs.append("cache_walk: the engine's walk and the virtual install's differ in %s" % ", ".join(want[1]))
        # The real install, importer.py's own code, on a copy of the game.
        real = os.path.join(tmp, "installed")
        shutil.copytree(game, real)
        import io
        import contextlib
        with contextlib.redirect_stdout(io.StringIO()):
            importer.cmd_install(argparse.Namespace(cache=cache, game=real, lang=[lang], opt=opts, no_opt=[]))
        idx = index(cache)
        files = {f: os.path.join(real, "DAT", f) for f in os.listdir(os.path.join(real, "DAT"))}
        for name in shipped(game):
            if engine_walk(game, cache, name, lang, opts, idx) != install_walk(files, name, lang, opts):
                errs.append("cache_walk: %s: the engine's walk is not cmd_install's" % name)
        if sorted(files) != sorted(virtual_install(game, cache, lang, opts)):
            errs.append("cache_walk: the virtual install is not cmd_install's DAT/")
        for what, control in (("a walk without the slots", {"slotless": True}),
                              ("a precedence mixing in the stale overlay", {"mixing": True})):
            if not compare(game, cache, lang, opts, **control)[1]:
                errs.append("cache_walk: the control (%s) was not refused" % what)
    return errs


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    s = ap.add_subparsers(dest="cmd", required=True)
    s.add_parser("check")
    p = s.add_parser("compare")
    p.add_argument("--game", required=True)
    p.add_argument("--cache", required=True)
    p.add_argument("--lang")
    p.add_argument("--opt", action="append", default=[])
    a = ap.parse_args()
    if a.cmd == "check":
        errs = check()
        for e in errs:
            print("ERROR", e)
        print("cache_walk check: the engine's walk against importer.py install on a synthetic cache, the two "
              "controls refused: %d error(s)" % len(errs))
        return 1 if errs else 0
    same, differ, idx = compare(a.game, a.cache, a.lang, a.opt)
    held_n = sum(1 for n, c in idx.items() if held(a.cache, n, c)[0])
    print("cache_walk compare %s against %s%s%s: %d of %d containers the same end state (%d held by the cache)"
          % (a.cache, a.game, " --lang " + a.lang if a.lang else "", "".join(" --opt " + o for o in a.opt),
             same, same + len(differ), held_n))
    for n in differ[:20]:
        print("  differs:", n)
    slotless = compare(a.game, a.cache, a.lang, a.opt, slotless=True)[1]
    print("  the control, a walk without the slots: %d differ%s" % (len(slotless), " (%s)" % ", ".join(slotless[:5])
                                                                  if slotless else ""))
    return 1 if differ else 0


if __name__ == "__main__":
    sys.exit(main())
