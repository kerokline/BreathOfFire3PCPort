"""The suites' engine half (docs/launcher-suites.md section 9): the PSP's music,
effects, exe tables and Ryu's forms (P8) as opt/ layers beside a PlayStation base.

    python tools/psp_layers.py check        # recipes/psp-layers.toml (CI, via importer.py check); no game data

`importer.py build --opt NAME` builds them (one call site, `take` and `build`);
`verify` checks them (`verify_layer`); `install` refuses the three that are not DAT
containers (`refuse_install`): the engine reads those from the cache only.

recipes/psp-layers.toml names each layer, its part (the tree under opt/<name>/ and
the engine seam that reads it), the builds that carry it and its rule. The bytes come
from the player's own PSP disc at build time; nothing of the game is in the tree
(CLAUDE.md rule 1). The manifest gets a `[psp_layers]` table after everything the
importer writes: one row a file, `[layer, path, source, sha256]`.
"""
import argparse, hashlib, json, os, shutil, sys, tempfile, tomllib

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)
sys.path.insert(0, TOOLS)

RECIPE = os.path.join(ROOT, "recipes", "psp-layers.toml")
PARTS = ("bgm", "snd", "exe", "dat")
PSP = ("psp-jp", "psp-eu")
# data.toml's classes whose bytes a disc gave (exe_tables.py write_exe): a layer
# byte must be one of them in the PSP-built image, so that it is the PSP's value
# and not a zero the image left unfilled.
DISC_HOWS = ("map", "place", "table", "widen")
EXE_FILE, EXE_BIN = "place.toml", "place.bin"
MANIFEST_TABLE = "psp_layers"


def sha(b):
    return hashlib.sha256(b).hexdigest()


def _importer():
    import importer
    return importer


def load(path=None):
    """{name: layer} of recipes/psp-layers.toml, in its order."""
    with open(path or RECIPE, "rb") as fh:
        return {l["name"]: l for l in tomllib.load(fh).get("layer", [])}


def take(asked, sources=None):
    """--opt's list split: (opt.toml's names, this recipe's names), each in the
    order given. With the sources, a layer this build cannot make is refused
    now, before the build's minutes, not after them."""
    mine = load()
    tg = [n for n in asked if n in mine]
    for n in tg if sources is not None else []:
        if not mine[n].get("auto"):
            _refuse_on_psp_base(n, mine[n], sources)
        if _psp(mine[n], sources) is None:
            raise SystemExit("--opt %s: needs a PSP disc (%s) among the sources" % (n, " or ".join(mine[n]["from"])))
    return [n for n in asked if n not in mine], tg


# ---------------------------------------------------------------- the rules

def exe_ranges(layer):
    """[(lo, hi, what)] over .data, sorted: each table's records by symbols.toml's
    PC address, count and stride, less a record's name field."""
    import tables
    cat, syms, _ = tables.load()
    name_len = cat["meta"]["name_len"]["pc-zh"]
    by_key = {t["key"]: t for t in cat["table"]}
    out = []
    for key in layer["tables"]:
        t = by_key[key]
        base = syms[t["symbol"]]
        name = t.get("name")
        for r in range(t["count"]):
            lo, hi = base + r * t["stride"], base + (r + 1) * t["stride"]
            if name is None:
                out.append([lo, hi, key])
                continue
            a, b = lo + name["at"], lo + name["at"] + name_len
            if a > lo:
                out.append([lo, a, key])
            if b < hi:
                out.append([b, hi, key])
    out.sort()
    merged = []
    for lo, hi, key in out:          # whole tables of nameless records: one range
        if merged and merged[-1][1] == lo and merged[-1][2] == key:
            merged[-1][1] = hi
        else:
            merged.append([lo, hi, key])
    return [tuple(m) for m in merged]


def dat_rows(layer, standins=None, recipe=None):
    """[(container, slot, pc chunk, stand-in row)] the P8 rule selects: the stand-in
    rows of the layer's classes whose output is not the PC's chunk."""
    import psp_standins
    rows = psp_standins.load(standins)
    imp = _importer()
    out = []
    for f in imp.load_recipe(recipe)["file"]:
        for slot, ch in enumerate(f["chunks"]):
            row = rows.get((f["name"], slot))
            if not row or row["class"] not in layer["standins"]:
                continue
            outs = {psp_standins.parse_on(x)[4] for x in row["on"]}
            if outs != {ch["sha256"]}:
                out.append((f["name"], slot, ch, row))
    return out


# ---------------------------------------------------------------- build

def _fresh(d):
    if os.path.isdir(d):
        shutil.rmtree(d)
    os.makedirs(d)


def available(sources):
    """This recipe's layers the sources can build, in its order: a PSP disc
    the layer lists among them, and - unless the layer is `auto` - a base
    that is not the PSP's own (what `take` would otherwise refuse)."""
    names = []
    for n, layer in load().items():
        if _psp(layer, sources) is None:
            continue
        if not layer.get("auto"):
            try:
                _refuse_on_psp_base(n, layer, sources)
            except SystemExit:
                continue
        names.append(n)
    return names


def _psp(layer, sources):
    """The first PSP disc among the sources that the layer lists."""
    return next((s for s in sources if s.id in layer["from"] and hasattr(s, "build")), None)


def _refuse_on_psp_base(name, layer, sources):
    import psp_standins
    if psp_standins.policy(sources) == "psp":
        raise SystemExit("--opt %s: every source is a PSP disc, so the cache's base is the PSP's already "
                         "(base_policy psp); the layer is for a PlayStation base" % name)
    if layer["part"] in ("bgm", "snd") and not any(s.id.startswith("psx-") and hasattr(s, "build") for s in sources):
        raise SystemExit("--opt %s: no PlayStation disc among the sources, so the cache's base/%s is already the "
                         "PSP's (or none); the layer is beside a PlayStation disc's" % (name, layer["part"]))


def build_bgm(name, layer, disc, out):
    import ppms
    root = os.path.join(out, "opt", name)
    dst = os.path.join(root, "bgm")
    with tempfile.TemporaryDirectory(dir=out, prefix=".psp_layers_") as tmp:
        bid, rows, st = ppms.build(disc.path, tmp, disc.id)
        _fresh(root)
        shutil.move(os.path.join(tmp, "base", "bgm"), dst)
    got = [(name, "opt/%s/bgm/%s" % (name, rel[len("base/bgm/"):]), where, h) for rel, where, h in rows]
    print("  opt/%-16s %d songs and %d banks from %s (ppms.py)" % (name, st["songs"], st["banks"], bid))
    return got


def build_snd(name, layer, disc, out):
    import at3
    ffmpeg = at3.find_ffmpeg()
    if not ffmpeg:
        raise SystemExit("--opt %s: no ffmpeg on the PATH or in BOF3X_FFMPEG; the PSP's effects, voice and jingles "
                         "are ATRAC3plus, which only the player's ffmpeg decodes" % name)
    root = os.path.join(out, "opt", name)
    dst = os.path.join(root, "snd")
    _fresh(root)
    written, skipped = at3.build(disc.path, dst, ffmpeg)
    if skipped:
        shutil.rmtree(root)
        raise SystemExit("--opt %s: a layer is written whole or not at all, and %d clip(s) were not: %s"
                         % (name, len(skipped), "; ".join(skipped[:3])))
    how = "%s:psp-at3" % disc.id
    print("  opt/%-16s %d files from %s (at3.py, %s)" % (name, len(written), disc.id, at3.ffmpeg_version(ffmpeg)))
    return [(name, "opt/%s/snd/%s.DAT" % (name, n), how, h) for n, h in sorted(written.items())]


def build_exe(name, layer, disc, out):
    import exe_tables
    ranges = exe_ranges(layer)
    with tempfile.TemporaryDirectory(dir=out, prefix=".psp_layers_") as tmp:
        bid, h = exe_tables.build_from(disc.path, disc.id, tmp)
        with open(os.path.join(tmp, "base", "exe", "data.bin"), "rb") as fh:
            img = fh.read()
        with open(os.path.join(tmp, "base", "exe", "data.toml"), "rb") as fh:
            meta = tomllib.load(fh)
    va = meta["image"]["va"]
    how = {}
    for lo, hi, k, _ in meta["image"]["ranges"]:
        for a in range(lo, hi):
            how[a] = k
    for lo, hi, what in ranges:
        bad = [a for a in range(lo, hi) if how.get(a) not in DISC_HOWS]
        if bad:
            raise SystemExit("--opt %s: 0x%06X (%s) is `%s` in the %s image, not a byte the disc gave"
                             % (name, bad[0], what, how.get(bad[0]), bid))
    body = b"".join(img[lo - va:hi - va] for lo, hi, _ in ranges)
    root = os.path.join(out, "opt", name)
    _fresh(os.path.join(root, "exe"))
    with open(os.path.join(root, "exe", EXE_BIN), "wb") as fh:
        fh.write(body)
    lines = ["# opt/%s/exe/%s - written by tools/psp_layers.py (docs/launcher-suites.md 9)." % (name, EXE_FILE),
             "# %s holds these ranges of the %s-built .data image, in order; the exe host lays" % (EXE_BIN, bid),
             "# them over .data when BOF3X_OPT names the layer (src/hook/exe_image.cpp).",
             "", 'build = "%s"' % bid, 'image_sha256 = "%s"' % h, 'file = "%s"' % EXE_BIN,
             'sha256 = "%s"' % sha(body), "bytes = %d" % len(body), "", "ranges = ["]
    lines += ['  [0x%06X, 0x%06X, "%s"],' % r for r in ranges]
    lines.append("]")
    with open(os.path.join(root, "exe", EXE_FILE), "w", newline="\n", encoding="utf-8") as fh:
        fh.write("\n".join(lines) + "\n")
    with open(os.path.join(root, "exe", EXE_FILE), "rb") as fh:
        th = sha(fh.read())
    print("  opt/%-16s %d ranges, %d bytes of the %s image's .data" % (name, len(ranges), len(body), bid))
    return [(name, "opt/%s/exe/%s" % (name, EXE_BIN), "%s:exe:%s" % (bid, h), sha(body)),
            (name, "opt/%s/exe/%s" % (name, EXE_FILE), "%s:exe:%s" % (bid, h), th)]


def build_dat(name, layer, sources, out):
    import psp_standins
    imp = _importer()
    rows = dat_rows(layer)
    if len(rows) != layer["count"]:
        raise SystemExit("--opt %s: the rule selects %d stand-in rows, the recipe says %d"
                         % (name, len(rows), layer["count"]))
    root = os.path.join(out, "opt", name)
    _fresh(os.path.join(root, "dat"))
    got, used = [], {}
    for fname, slot, ch, row in rows:
        for b, how, key, sec, h in map(psp_standins.parse_on, row["on"]):
            s = next((s for s in sources if s.id == b and hasattr(s, "build")), None)
            if s is None:
                continue
            body = psp_standins.transform(how, s.sections(key)[sec][3])
            if body is None or sha(body) != h or len(body) != ch["size"]:
                raise SystemExit("--opt %s: %s slot %d from %s: not the stand-in recipes/psp-standins.toml records"
                                 % (name, fname, slot, b))
            blob = imp.chunk_bytes(ch["kind"], ch["tag"], body)     # one chunk: the PC's kind and tag
            with open(os.path.join(root, "dat", fname), "wb") as fh:
                fh.write(blob)
            got.append((name, "opt/%s/dat/%s" % (name, fname), "%s:%s:%s#%d" % (b, how, key, sec), sha(blob)))
            used[b] = used.get(b, 0) + 1
            break
        else:
            raise SystemExit("--opt %s: %s slot %d is on %s; the sources given carry none of them"
                             % (name, fname, slot, " / ".join(x.split(":")[0] for x in row["on"])))
    print("  opt/%-16s %d containers, %s" % (name, len(got), ", ".join("%d from %s" % (n, b) for b, n in used.items())))
    return got


def bank_rows(disc, recipe=None):
    """The `dropped-tone` rule on one PSP disc: [(container, slot, pc chunk, EMI
    key, header section, payload)] for every kind-2 chunk of pc-zh.toml whose
    PSP group is the case vag.psp_dropped_tone restores, the payload the bank in
    the PSP's own one-tone form (psp_vab without the cue table). The restored
    form is held to the PC's chunk's hash (MB's measurement), and the one-tone
    form must differ from it."""
    import vag
    imp = _importer()
    b = disc.build
    stems = {k.rsplit("/", 1)[-1][:-4]: k for k in b.emis}
    out = []
    for f in imp.load_recipe(recipe)["file"]:
        banks = [(slot, ch) for slot, ch in enumerate(f["chunks"]) if ch["kind"] == 2]
        key = stems.get(f["name"][:-4])
        if not banks or key is None:
            continue
        gs = vag.groups(b.sections(key))
        for (slot, ch), g in zip(banks, gs):
            if g[6][3][:4] != vag.PSP_HEAD or 8 not in g:
                continue
            programs, samples = vag.psp_bank(g[6][3], g[7][3])
            if vag.psp_dropped_tone(programs, len(samples), g[8][3]) is None:
                continue
            restored = vag.bank(*vag._vab_cues(g), vag._CACHE)
            vh, vb = vag.psp_vab(g[6][3], g[7][3])
            one = vag.bank(vh, g[8][3], vb, vag._CACHE)
            if sha(restored) != ch["sha256"] or one == restored:
                raise SystemExit("%s slot %d on %s: the dropped-tone rule's bank is %s the PC's, its one-tone form %s"
                                 % (f["name"], slot, disc.id, "not" if sha(restored) != ch["sha256"] else "",
                                    "the same" if one == restored else "differs"))
            out.append((f["name"], slot, ch, key, g[6][0], one))
    return out


def build_banks(name, layer, disc, out):
    imp = _importer()
    rows = bank_rows(disc)
    if len(rows) != layer["count"]:
        raise SystemExit("--opt %s: the dropped-tone rule finds %d banks on %s, the recipe says %d"
                         % (name, len(rows), disc.id, layer["count"]))
    root = os.path.join(out, "opt", name)
    _fresh(os.path.join(root, "dat"))
    got = []
    for fname, slot, ch, key, sec, body in rows:
        blob = imp.chunk_bytes(ch["kind"], ch["tag"], body)
        with open(os.path.join(root, "dat", fname), "wb") as fh:
            fh.write(blob)
        got.append((name, "opt/%s/dat/%s" % (name, fname), "%s:psp-vab-one-tone:%s#%d" % (disc.id, key, sec), sha(blob)))
    print("  opt/%-16s %d containers, the bank as %s plays it (one tone; off by default)" % (name, len(got), disc.id))
    return got


def _layer_inputs(name, layer, disc, sources):
    """What a layer draws on beside the code and data build_reuse measures:
    its PSP disc; the player's ffmpeg for the sound; every source for a dat
    layer, which takes chunks from any."""
    import at3
    extra = ()
    if layer["part"] == "snd":
        ffmpeg = at3.find_ffmpeg()
        extra = (at3.ffmpeg_version(ffmpeg) if ffmpeg else "no ffmpeg",)
    elif layer["part"] == "dat" and "banks" not in layer:
        extra = tuple(s.id for s in sources)
    return ("psp-layer", name, disc.id) + extra


def build(asked, sources, out, reuse=None):
    """Each layer asked for into out/opt/<name>/, and each `auto` layer whenever
    a PSP disc it lists is a source (on any base); then the manifest's
    [psp_layers] table appended. A layer is written whole or not at all.

    Each layer is built in a folder of its own beside the cache's opt/
    (out/.psp_layer_<name>_*) and renamed into place only when it is whole; a
    failure removes its folder and any opt/<name>/ an earlier build left. The
    manifest's table is written whatever happens, with the rows of every layer
    that is in place, before a failure goes on: what is on disk under opt/ from
    here is exactly what the manifest lists, which is what the game and the
    launcher offer (common/cache_manifest.h) and what `verify` holds it to.
    (The code review of 2026-10-12: a failure in the second layer left the
    first in the cache with no row, offered and played unverified.)

    `reuse`: a previous cache whose layer, and its rows, are taken when built
    the same way (build_reuse.Step, _layer_inputs; IDEAS I42)."""
    from build_reuse import Step, read_rows, take, write_rows
    layers = load()
    asked = list(asked) + [n for n, l in layers.items() if l.get("auto") and n not in asked and _psp(l, sources)]
    if not asked:
        return []
    rows, placed = [], []
    try:
        for name in asked:
            layer = layers[name]
            if not layer.get("auto"):
                _refuse_on_psp_base(name, layer, sources)
            disc = _psp(layer, sources)
            if disc is None:
                raise SystemExit("--opt %s: needs a PSP disc (%s) among the sources" % (name, " or ".join(layer["from"])))
            final = os.path.join(out, "opt", name)
            stage = tempfile.mkdtemp(dir=out, prefix=".psp_layer_%s_" % name)
            try:
                with Step(out, reuse, "opt-" + name, *_layer_inputs(name, layer, disc, sources)) as st:
                    kept = st.previous(os.path.join("opt", name))
                    kept_rows = read_rows(reuse, "opt-" + name) if kept else None
                    if kept_rows is not None:
                        take(kept, os.path.join(stage, "opt", name))
                        got = kept_rows
                        print("  opt/%-16s the previous cache's, built from %s the same way" % (name, disc.id))
                    else:
                        st.adopted = None       # a record without its rows: built again, and measured
                        got = _build_one(name, layer, disc, sources, stage)
                if os.path.isdir(final):
                    shutil.rmtree(final)
                os.makedirs(os.path.join(out, "opt"), exist_ok=True)
                os.replace(os.path.join(stage, "opt", name), final)
            except BaseException:
                if os.path.isdir(final):        # an earlier build's tree: not this build's, and unlisted now
                    shutil.rmtree(final)
                print("  opt/%-16s not built: removed; the manifest lists %s" % (name, ", ".join(placed) or "no layer of this recipe"))
                raise
            finally:
                shutil.rmtree(stage, ignore_errors=True)
            write_rows(out, "opt-" + name, got)
            rows += got
            placed.append(name)
    finally:
        if rows:
            _append_table(out, rows)
    return rows


def _build_one(name, layer, disc, sources, out):
    """One layer into out/opt/<name>/ (`out` the layer's own staging folder)."""
    part = layer["part"]
    if part == "bgm":
        return build_bgm(name, layer, disc, out)
    if part == "snd":
        return build_snd(name, layer, disc, out)
    if part == "exe":
        return build_exe(name, layer, disc, out)
    if "banks" in layer:
        return build_banks(name, layer, disc, out)
    return build_dat(name, layer, sources, out)


def _append_table(out, rows):
    lines = ["", "# opt/ layers beside a PlayStation base (tools/psp_layers.py, recipes/psp-layers.toml,",
             "# docs/launcher-suites.md 9): each file's layer, path, source and hash.",
             "[%s]" % MANIFEST_TABLE, "recipe_sha256 = %s" % json.dumps(_recipe_sha()), "rows = ["]
    lines += ["  [%s]," % ", ".join(json.dumps(x) for x in r) for r in rows]
    lines.append("]")
    with open(os.path.join(out, "manifest.toml"), "a", newline="\n", encoding="utf-8") as fh:
        fh.write("\n".join(lines) + "\n")


def listed(cache):
    """The opt/ layers the cache's manifest lists - its [cache] opt rows'
    opt/<layer> and its [psp_layers] rows' layers - as common/cache_manifest.h
    reads them; None when there is no manifest."""
    path = os.path.join(cache, "manifest.toml")
    if not os.path.exists(path):
        return None
    with open(path, "rb") as fh:
        man = tomllib.load(fh)
    names = {r[1][len("opt/"):] for r in man.get("cache", {}).get("opt", []) if str(r[1]).startswith("opt/")}
    names |= {r[0] for r in man.get(MANIFEST_TABLE, {}).get("rows", [])}
    return names


def verify_listed(cache):
    """An opt/<layer>/ folder the manifest does not list: a problem - the game
    and the launcher do not offer it (a build that stopped part-way). Returns
    the names, printing each."""
    names = listed(cache)
    d = os.path.join(cache, "opt")
    if names is None or not os.path.isdir(d):
        return []
    bad = sorted(n for n in os.listdir(d) if os.path.isdir(os.path.join(d, n)) and n not in names)
    for n in bad:
        print("verify opt/%s: on disk, not listed in the manifest (a build that stopped part-way?); the game and the "
              "launcher do not offer it - 1 problem" % n)
    return bad


def _recipe_sha():
    with open(RECIPE, "rb") as fh:
        return sha(fh.read())


# ---------------------------------------------------------------- verify, install

def is_layer(name):
    return name in load()


def verify_layer(cache, name):
    """opt/<name>/ of this recipe against the manifest's rows: every file there
    has a row and its hash, no row without its file; a bgm layer's rows against
    fixtures/bgm-psp.tsv (what ppms.py makes of that build); an exe layer's
    ranges against the rule; a dat layer's chunks against the PC's kind, tag and
    size. Returns 0 or 1, printing what it found."""
    layer = load()[name]
    with open(os.path.join(cache, "manifest.toml"), "rb") as fh:
        man = tomllib.load(fh)
    rows = {r[1]: r for r in man.get(MANIFEST_TABLE, {}).get("rows", []) if r[0] == name}
    root = os.path.join(cache, "opt", name)
    have = set()
    for d, _, files in os.walk(root):
        for f in files:
            have.add(os.path.relpath(os.path.join(d, f), cache).replace(os.sep, "/"))
    bad = ["%s: in the cache, not in the manifest" % p for p in sorted(have - set(rows))]
    bad += ["%s: in the manifest, not in the cache" % p for p in sorted(set(rows) - have)]
    for p in sorted(have & set(rows)):
        with open(os.path.join(cache, *p.split("/")), "rb") as fh:
            if sha(fh.read()) != rows[p][3]:
                bad.append("%s: not the file the manifest recorded" % p)
    part = layer["part"]
    if part == "bgm":
        import ppms
        fix = ppms.load_fixture()
        for p, r in rows.items():
            bid = r[2].split(":")[0]
            want = fix.get(bid, {}).get("base/bgm/" + p.split("/bgm/", 1)[1])
            if not want or want[2] != r[3]:
                bad.append("%s: not what ppms.py makes of the %s disc (fixtures/bgm-psp.tsv)" % (p, bid))
    elif part == "exe":
        toml_path = os.path.join(root, "exe", EXE_FILE)
        if os.path.exists(toml_path):
            with open(toml_path, "rb") as fh:
                meta = tomllib.load(fh)
            if [tuple(x) for x in meta["ranges"]] != exe_ranges(layer):
                bad.append("%s: its ranges are not the rule's (recipes/psp-layers.toml)" % EXE_FILE)
            with open(os.path.join(root, "exe", EXE_BIN), "rb") as fh:
                body = fh.read()
            if sha(body) != meta["sha256"] or len(body) != sum(hi - lo for lo, hi, _ in meta["ranges"]):
                bad.append("%s: not the %s its %s records" % (EXE_BIN, EXE_BIN, EXE_FILE))
            import exe_tables
            if exe_tables.load_recipe()["build"].get(meta["build"], {}).get("sha256") != meta["image_sha256"]:
                bad.append("%s: cut from an image recipes/exe.toml does not record for %s" % (EXE_FILE, meta["build"]))
    elif "banks" in layer:
        import dat
        rec = {f["name"]: f for f in _importer().load_recipe()["file"]}
        for p in rows:
            fname = p.rsplit("/", 1)[1]
            _, chunks = dat.load(os.path.join(cache, *p.split("/")))
            tags = {c["tag"] for c in rec.get(fname, {}).get("chunks", []) if c["kind"] == 2}
            if len(chunks) != 1 or chunks[0].kind != 2 or chunks[0].tag not in tags:
                bad.append("%s: not one bank chunk at a tag of that container's banks" % p)
        if len(rows) != layer["count"]:
            bad.append("%d containers, the rule %d" % (len(rows), layer["count"]))
    elif part == "dat":
        import dat
        sel = {f: (ch, row) for f, _, ch, row in dat_rows(layer)}
        for p in rows:
            fname = p.rsplit("/", 1)[1]
            if fname not in sel:
                bad.append("%s: not a container the rule selects" % p)
                continue
            blob, chunks = dat.load(os.path.join(cache, *p.split("/")))
            ch, row = sel[fname]
            if len(chunks) != 1 or (chunks[0].kind, chunks[0].tag, chunks[0].size) != (ch["kind"], ch["tag"], ch["size"]):
                bad.append("%s: not one chunk of the PC's kind, tag and size" % p)
                continue
            import psp_standins
            body = blob[chunks[0].offset:chunks[0].offset + chunks[0].size]
            want = {b: h for b, _, _, _, h in map(psp_standins.parse_on, row["on"])}
            if sha(body) != want.get(rows[p][2].split(":")[0]):
                bad.append("%s: its chunk is not the stand-in recipes/psp-standins.toml records" % p)
        if len(rows) != layer["count"]:
            bad.append("%d containers, the rule %d" % (len(rows), layer["count"]))
    print("verify opt/%s (%s, recipes/psp-layers.toml): %d files, %d problem(s)" % (name, part, len(have), len(bad)))
    for b in bad[:10]:
        print("   ", b)
    return 1 if bad else 0


def refuse_install(names, cache):
    """`install --opt NAME` for a layer that is not DAT containers: refused, the
    engine reads it from the cache (cache= / BOF3X_CACHE) only."""
    layers = load()
    for n in names:
        if n in layers and layers[n]["part"] != "dat":
            raise SystemExit("--opt %s: its %s/ tree is read from the cache, not from DAT/: play it with cache=%s "
                             "and opt=%s (BOF3X_CACHE, BOF3X_OPT)" % (n, layers[n]["part"], cache, n))


# ---------------------------------------------------------------- check (CI)

def check(path=None):
    """The recipe, no game data: four layers of known parts, PSP builds, names
    the engine takes and opt.toml does not; the exe rule's ranges inside .data,
    each table a tables.toml key with a symbol; the dat rule's count."""
    errs = []
    try:
        layers = load(path)
    except (OSError, tomllib.TOMLDecodeError) as e:
        return ["psp-layers.toml: %s" % e]
    imp = _importer()
    with open(os.path.join(ROOT, "fixtures.toml"), "rb") as fh:
        roles = {b["id"]: b.get("role") for b in tomllib.load(fh)["build"]}
    opt = imp.load_opt_recipe()
    for name, l in layers.items():
        where = "layer %s" % name
        if not (0 < len(name) < 24) or any(not (c.isascii() and (c.isalnum() or c == "-")) for c in name):
            errs.append("%s: a name the engine refuses (dat_load.cpp ReadOptLayers)" % where)
        if name in opt:
            errs.append("%s: also a recipes/opt.toml layer" % where)
        if l.get("part") not in PARTS:
            errs.append("%s: part %r" % (where, l.get("part")))
        if not l.get("from") or any(roles.get(b) != "psp-disc" for b in l["from"]):
            errs.append("%s: from %r, not PSP discs of fixtures.toml" % (where, l.get("from")))
        if l.get("part") == "exe":
            try:
                import exe_tables
                rs = exe_ranges(l)
                prev = 0
                for lo, hi, what in rs:
                    if lo < prev or hi <= lo or lo < exe_tables.DATA_LO or hi > exe_tables.DATA_HI:
                        errs.append("%s: range 0x%X..0x%X (%s) out of order or outside .data" % (where, lo, hi, what))
                    prev = hi
                if not rs:
                    errs.append("%s: no range" % where)
            except KeyError as e:
                errs.append("%s: table or symbol %s unknown" % (where, e))
        if l.get("suite") not in (None, "jukebox", "music"):
            errs.append("%s: suite %r (none, the suites.toml catalogue's, \"jukebox\" or \"music\")"
                        % (where, l.get("suite")))
        if "banks" in l:
            if l.get("part") != "dat" or l["banks"] != "dropped-tone" or not isinstance(l.get("count"), int):
                errs.append("%s: a banks layer is a dat part, rule \"dropped-tone\", with a count" % where)
        elif l.get("part") == "dat":
            n = len(dat_rows(l))
            if n != l.get("count"):
                errs.append("%s: the rule selects %d stand-in rows, the recipe says %s" % (where, n, l.get("count")))
    return errs


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    s = p.add_subparsers(dest="cmd", required=True)
    s.add_parser("check")
    s.add_parser("ranges", help="the exe layer's ranges, by rule (addresses and counts only)")
    a = p.parse_args(argv)
    if a.cmd == "ranges":
        for name, l in load().items():
            if l["part"] == "exe":
                rs = exe_ranges(l)
                print("%s: %d ranges, %d bytes" % (name, len(rs), sum(hi - lo for lo, hi, _ in rs)))
                for r in rs[:4]:
                    print("  0x%06X..0x%06X %s" % r)
        return 0
    errs = check()
    for e in errs:
        print("ERROR", e)
    print("psp_layers check: %d layer(s), %d error(s)" % (len(load()), len(errs)))
    return 1 if errs else 0


if __name__ == "__main__":
    sys.exit(main())
