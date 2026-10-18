#!/usr/bin/env python
"""The bytes of .data no disc carries: classified, and the port's held as the engine's own.

docs/exe-engine-own.md. A disc-built image of BOF3.exe's .data (tools/exe_tables.py,
docs/exe-import-engine.md) leaves every byte no disc carries unfilled. This tool lists,
per build, every unfilled region holding non-zero *number* bytes (not a pointer word
of recipes/exe-pointers.tsv), splits it at symbols.toml's [[data]] symbols, and says
for each: its size, who reads it, whether its bytes are on the disc at all, and its
class -

    port-platform  exists only because this is a Windows port: file names, window,
                   DirectX, MCI, config, sound-file names (held by src/hook/exe_own.cpp)
    port-relaid    a table the port re-laid from PSX data (DIV-0055's plates): on the
                   disc in another shape, a transform's job
    language       text, labels and glyph tables a language layer carries (LG's)
    game-value     a Capcom table of game numbers no disc carries in any shape found
    disc-unplaced  game data whose bytes ARE on the disc (a 16-byte window or more
                   found verbatim): the places' miss (exe_tables.find_places), not ours
    unread         nothing of ours reads it in the exe host
    rule           the engine's by a different rule (RD's key table)
    game-format    a draw format or mark of Capcom's code, read in place, that each disc keeps
                   inside its reading overlay where no search can place it (too short to be
                   unique): held by src/hook/exe_own.cpp as D254's is (D310, D278)

    python tools/exe_own.py measure --game DIR --disc DISC [--disc DISC ...] --out analysis/exe_own
    python tools/exe_own.py gen        # src/hook/exe_own_names.inc from recipes/pc-zh.toml, pc-zh.snd.toml
    python tools/exe_own.py check      # the include up to date; exe_own.cpp's regions well-formed (CI)
    python tools/exe_own.py text_words # exe-pointers.tsv's words inside the held strings: text, CP's to skip

The measurement's output is derived from the player's files and goes under analysis/
(CLAUDE.md rule 1); the tool and its rules hold addresses, names and counts only.
"""
import argparse
import bisect
import collections
import json
import os
import re
import struct
import sys
import tomllib

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)
sys.path.insert(0, TOOLS)
import exe_tables    # noqa: E402
import exe_twins     # noqa: E402

DATA_LO, DATA_HI = exe_tables.DATA_LO, exe_tables.DATA_HI
OWN_CPP = os.path.join(ROOT, "src", "hook", "exe_own.cpp")
NAMES_INC = os.path.join(ROOT, "src", "hook", "exe_own_names.inc")
DAT_RECIPE = os.path.join(ROOT, "recipes", "pc-zh.toml")
SND_RECIPE = os.path.join(ROOT, "recipes", "pc-zh.snd.toml")
XREF = "analysis/pc_xref.json"
KEY_TABLE = (0x66C648, 0x66C6C8)        # Key_TableDefault (64 halfwords): RD's (a rule of the engine and the launcher)


def fail(msg):
    raise SystemExit("exe_own: " + msg)


# ------------------------------------------------------------------ the classes, by region
#
# Each row: [lo, hi) of PC addresses, the class, and why. The first row holding a
# segment's start decides; a segment no row holds is classed by its symbol's name
# (NAME_RULES), then by its readers (none: unread), else game-value or
# disc-unplaced by the disc search. Addresses and words only.

REGIONS = [
    (0x60ABF0, 0x60ABFC, "game-format", "Area75_DrawCounters' counter format (D310)"),
    (0x61BC74, 0x61BC80, "game-format", "Area104_Kind6ACountdown's format (D310)"),
    (0x64F368, 0x6528AC, "port-platform", "Dat_FileNames: the DAT/ file table's names, the DAT directory format, the save files' pattern"),
    (0x653084, 0x653098, "game-format", "CommuBoard_DrawRows' dash and formats (D310)"),
    (0x6531F4, 0x6536E0, "language", "the page-format and glyph tables rest_4e.cpp names (exe-import-engine.md 3.2)"),
    (0x6536F8, 0x653808, "language", "the config screen's labels (ConfigScreen_OpenStates+): text in the port's code page"),
    # The labels a Latin layer writes in place where FR / DE re-word them (docs/install-gap-labels.md
    # 1, 2.1; kind 16 tag 3, label groups 21..23); a twin or the map places the PC's words elsewhere.
    (0x653EA0, 0x653EB4, "language", "EffectKind0F_PanelTitles: the fishing panels' four fixed fields (kind 16 tag 3)"),
    (0x654900, 0x654904, "game-format", "the level-up's ability name format, EffectKind5E_DrawMenu's (D310)"),
    (0x656AEC, 0x656B00, "rule", "the field button map's defaults Boot_Task copies: the default button of each action, "
                                 "by name (docs/controls.md 2.1)"),
    (0x65AAC4, 0x65AAD0, "game-format", "the Identify panel's unknown marks (Identify_Draw*; D278)"),
    (0x65C1B8, 0x65C1BC, "unread", "MagicBallOrb_Shades' pairs 8 and 9: an orb's +0xB is 0..7 (MagicBall_Start's eight, "
                                   "kept), so MagicBallOrb_DrawDisc never reaches them (docs/install-gap-twins.md 6.3)"),
    (0x65DA44, 0x65DAE8, "port-platform", "Game_WinMain's and Cfg_Load's initial values and strings"),
    (0x663668, 0x66367C, "language", "Equip_SlotIcons' labels: the equipment and EXP panels' three words (label group 21)"),
    (0x6637E0, 0x6637E4, "game-format", "Menu_DrawPlayTime's separator (D310)"),
    (0x6639D4, 0x6639D8, "port-platform", "the save slot's number format (Menu_DrawSaveSlot; D254)"),
    (0x664068, 0x664078, "port-platform", "the save file's name format (FieldSave_Write, Save_QuickWrite)"),
    (0x664288, 0x664290, "language", "the shared list's title (SharedList_DrawList; label group 22)"),
    (0x664290, 0x664298, "unread", "the word after the shared list's title: no operand and no dword of BOF3.exe names it "
                                   "(docs/install-gap-labels.md 2.3)"),
    (0x6653BC, 0x666FC6, "port-platform", "Sound_LoadStream's 891 SND/ names, Music_LoadedTrack, the SND and BGM formats"),
    (0x666FC6, 0x6671F4, "unread", "three lists of the PSX title overlay and their tail: no instruction and no word of "
                                   "the exe's file names them (docs/install-gap-readers.md 1.4)"),
    (0x669CCC, 0x66A450, "language", "the battle command labels, the community's member names, Menu_Verbs, pause lines"),
    (0x66B1F0, 0x66B1F8, "language", "the gene list's title (MenuList_GeneWinDraw; label group 23)"),
    (0x66B5D8, 0x66B6A8, "port-platform", "the AVI player's MCI command strings (Fmv_Play, Fmv_WndProc)"),
    (0x66B708, 0x66BC58, "port-platform", "the display / sound / file layer's rect, caches, error strings and formats; the OT end; the GTE identity"),
    (0x66BC58, 0x66C45C, "rule", "Math_SinTable: PSX libgte's quarter-wave sine, computed"),
    (0x66C45C, KEY_TABLE[0], "unread", "a table of (DirectInput scan code, key name): no instruction and no word of the "
                                       "exe's file names it"),
    (KEY_TABLE[0], KEY_TABLE[1], "rule (RD)", "Key_TableDefault: the RD group's, a rule of the engine and the launcher"),
    (KEY_TABLE[1], 0x66C7B0, "unread", "the key names: only the unread table above points at them"),
    (0x66C7B0, 0x66C7D0, "port-platform", "the pad DInput_EnumJoystick looks for by product name"),
    (0x66C858, 0x675A00, "unread", "past Task_StackTop: the hand-assembled renderer's tables, the original MP3 decoder's "
                                   "(Xaudio) data and the C runtime's - read only by Capcom code the exe host never runs"),
]

# Bytes inside a region above whose class differs from the region's (checked first).
BYTES = [
    (0x65DA50, 0x65DA54, "unread", "a dword initialised 1 that no instruction and no pointer word names"),
    (0x65DA78, 0x65DA84, "language", "the window title, in the port's code page (GBK)"),
    (0x65DA98, 0x65DAB8, "language", "Game_WinMain's two message-box texts, in GBK"),
    (0x66B71C, 0x66B720, "unread", "a dword initialised 1 that no instruction names"),
    (0x66B724, 0x66B928, "unread", "16 records {0.01f, 10.0f, -1}: no instruction, no word of the exe's file names them"),
]

NAME_RULES = (
    ("port-relaid", r"(DriftUV|PlateAnims|Record8Anims)$"),
    ("language", r"(Messages?[AB]?|Text|TextPointers|Names|Labels?|Verbs|Lines|Format|Glyph|Font|Choices|ChoiceMessages|ScriptMessages)$|^NameTable_|^Pause_Lines|^Commu.*Names"),
)

# A word recipes/exe-pointers.tsv calls a pointer inside a region exe_own.cpp
# holds is text (a pointer-shaped run of four characters no disc places): the
# fill writes it, verified equal to the PC's at every start, and CP's layout
# leaves it alone (`check` counts them; `text_words` lists them).


def region_class(lo):
    for a, b, cls, why in REGIONS:
        if a <= lo < b:
            return cls, why
    return None, None


def byte_class(x):
    for a, b, cls, why in BYTES:
        if a <= x < b:
            return cls
    return None


# ------------------------------------------------------------------ inputs

def data_symbols():
    with open(os.path.join(ROOT, "symbols.toml"), "rb") as f:
        sym = tomllib.load(f)
    data = sorted(sym["data"], key=lambda e: e["pc"])
    funcs = sorted(sym["func"], key=lambda e: e["pc"])
    return data, funcs


def own_regions(path=OWN_CPP):
    """exe_own.cpp's region table: [(lo, hi, name)] from its `{0x......, 0x......, "name"` rows
    between the `// regions:` and `// end regions` markers."""
    if not os.path.exists(path):
        return []
    with open(path, encoding="utf-8") as f:
        text = f.read()
    m = re.search(r"// regions:\n(.*?)// end regions", text, re.S)
    if not m:
        fail("%s has no `// regions:` .. `// end regions` table" % os.path.relpath(path, ROOT))
    return [(int(a, 16), int(b, 16), n) for a, b, n in
            re.findall(r"\{\s*(0x[0-9A-Fa-f]+),\s*(0x[0-9A-Fa-f]+),\s*\"([^\"]*)\"", m.group(1))]


def pointer_bytes():
    ptr = {}
    for a, n, cls in exe_tables.load_pointers():
        for i in range(a, a + 4 * n):
            ptr[i] = cls
    return ptr


def segments(pc, owner, ptr, dpcs):
    """Every maximal unfilled span split at [[data]] symbols, with its non-zero
    number bytes: [(lo, hi, nonzero)] (only those with some)."""
    out, n, i = [], len(pc), 0
    while i < n:
        if owner[i] is not None:
            i += 1
            continue
        j = i
        while j < n and owner[j] is None:
            j += 1
        a, b = DATA_LO + i, DATA_LO + j
        k = bisect.bisect_right(dpcs, a)
        cuts = [a] + [p for p in dpcs[k:bisect.bisect_left(dpcs, b)] if a < p < b] + [b]
        for lo, hi in zip(cuts, cuts[1:]):
            nz = sum(1 for x in range(lo, hi) if x not in ptr and pc[x - DATA_LO])
            if nz:
                out.append((lo, hi, nz))
        i = j
    return out


def disc_blob(disc):
    return b"\x00\xA5\x5A\x00".join(blob for _, _, _, blob in exe_twins.sources(disc))


def on_disc(pc, lo, hi, ptr, blob):
    """(windows found, windows tried): every 16-byte window of the segment's
    pointer-free stretches with 6 or more non-zero bytes, searched verbatim."""
    tried = found = 0
    a = lo
    while a < hi:
        if a in ptr:
            a += 1
            continue
        b = a
        while b < hi and b not in ptr:
            b += 1
        for w in range(a, max(a + 1, b - 15), 8):
            win = pc[w - DATA_LO:w - DATA_LO + 16]
            if len(win) == 16 and sum(1 for c in win if c) >= 6:
                tried += 1
                found += blob.find(win) >= 0
        a = b
    return found, tried


# ------------------------------------------------------------------ measure

def cmd_measure(a):
    out = os.path.abspath(a.out)
    if out.startswith(ROOT + os.sep) and not out.startswith(os.path.join(ROOT, "analysis") + os.sep):
        fail("--out inside the repository must be under analysis/ (gitignored)")
    os.makedirs(out, exist_ok=True)
    pc, _ = exe_tables.pc_data(os.path.join(a.game, "BOF3.exe"))
    data, funcs = data_symbols()
    dpcs, dnames = [e["pc"] for e in data], [e["name"] for e in data]
    fpcs = [f["pc"] for f in funcs]
    ptr = pointer_bytes()
    src = exe_tables.src_addresses()
    src_iv = sorted((v, v + ext, name) for v, (_, name, ext) in src.items())
    xref_path = a.xref or os.path.join(a.analysis, "pc_xref.json")
    with open(xref_path) as f:
        xref = {int(k, 16): v for k, v in json.load(f).items()}
    xkeys = sorted(xref)
    ptr_in = collections.defaultdict(int)             # target -> data pointer words into it
    for o in range(0, len(pc) - 3, 4):
        if DATA_LO + o in ptr:
            v = struct.unpack_from("<I", pc, o)[0]
            if DATA_LO <= v < DATA_HI:
                ptr_in[v] += 1
    ptr_targets = sorted(ptr_in)
    held = own_regions()
    held_set = set()
    for lo, hi, _ in held:
        held_set.update(range(lo, hi))
    refused = exe_tables.load_rebuild_refused()
    pointers = exe_tables.load_pointers()
    summary = {}
    for bid, disc in exe_tables.discs(a.disc):
        img, owner, ranges, _, _ = exe_tables.disc_built(disc, bid, pointers, refused[bid])
        blob = disc_blob(disc)
        rows = []
        for lo, hi, nz in segments(pc, owner, ptr, dpcs):
            k = bisect.bisect_right(dpcs, lo) - 1
            sym, base = (dnames[k], dpcs[k]) if k >= 0 else ("?", DATA_LO)
            names = sorted({n for s, e, n in src_iv if s < hi and e > lo})
            insns = [i for key in xkeys[bisect.bisect_left(xkeys, base):bisect.bisect_left(xkeys, hi)]
                     for i in xref[key]]
            fns = collections.Counter()
            for ins in insns:
                j = bisect.bisect_right(fpcs, ins[0]) - 1
                if j >= 0:
                    fns[funcs[j]["name"]] += 1
            pin = sum(ptr_in[t] for t in ptr_targets[bisect.bisect_left(ptr_targets, lo):bisect.bisect_left(ptr_targets, hi)])
            found, tried = on_disc(pc, lo, hi, ptr, blob)
            cls, why = region_class(lo)
            if cls is None:
                for c, rx in NAME_RULES:
                    if re.search(rx, sym):
                        cls, why = c, "by its symbol's name"
                        break
            if cls is None:
                if not names and not fns and not pin:
                    cls, why = "unread", "no src/ name, no instruction of Capcom's, no pointer word reaches it"
                elif found:
                    cls, why = "disc-unplaced", "%d of %d 16-byte windows on the disc verbatim" % (found, tried)
                else:
                    cls, why = "game-value", "%d of %d windows on the disc" % (found, tried)
            # Byte by byte: held by exe_own.cpp, else the BYTES row's class, else the REGIONS row's
            # holding the byte, else the segment's.
            per = collections.Counter()
            for x in range(lo, hi):
                if x in ptr or not pc[x - DATA_LO]:
                    continue
                per["held" if x in held_set else (byte_class(x) or region_class(x)[0] or cls)] += 1
            text_words = sorted({x & ~3 for x in range(lo, hi) if x in ptr and x in held_set})
            rows.append(dict(lo=lo, hi=hi, nonzero=nz, held=per["held"], symbol="%s+0x%X" % (sym, lo - base),
                             cls=cls, bytes=",".join("%s:%d" % kv for kv in sorted(per.items())),
                             text_words=len(text_words), why=why,
                             src=",".join(names[:4]) + ("..." if len(names) > 4 else ""),
                             capcom=",".join("%s(%d)" % kv for kv in fns.most_common(3)), ptr_in=pin,
                             disc="%d/%d" % (found, tried), per=per, tw=text_words))
        with open(os.path.join(out, "%s.own.tsv" % bid), "w", newline="\n", encoding="utf-8") as f:
            cols = ["lo", "hi", "nonzero", "held", "cls", "bytes", "text_words", "symbol", "src", "capcom", "ptr_in",
                    "disc", "why"]
            f.write("\t".join(cols) + "\n")
            for r in rows:
                f.write("\t".join(("0x%06X" % r[c]) if c in ("lo", "hi") else str(r[c]) for c in cols) + "\n")
        by = collections.defaultdict(lambda: [0, 0])          # byte class -> [regions, bytes]
        for r in rows:
            for c, n in r["per"].items():
                by[c][0] += 1
                by[c][1] += n
        total = sum(r["nonzero"] for r in rows)
        tw = sorted({w for r in rows for w in r["tw"]})
        summary[bid] = {"regions": len(rows), "nonzero": total, "held": by["held"][1] if "held" in by else 0,
                        "text_words": ["0x%06X" % w for w in tw],
                        "by_class": {c: {"regions": v[0], "bytes": v[1]} for c, v in sorted(by.items())},
                        "by_region_class": {c: sum(r["nonzero"] for r in rows if r["cls"] == c)
                                            for c in sorted({r["cls"] for r in rows})}}
        print("%s: %d regions, %d non-zero number bytes no disc carries; by byte:" % (bid, len(rows), total))
        for c, v in sorted(by.items(), key=lambda kv: -kv[1][1]):
            print("   %-14s %6d bytes in %4d regions" % (c, v[1], v[0]))
        print("   + %d pointer-shaped words of exe-pointers.tsv inside the held strings (text, held)" % len(tw))
    with open(os.path.join(out, "summary.json"), "w") as f:
        json.dump(summary, f, indent=1)
    print("-> %s" % out)


# ------------------------------------------------------------------ gen / check

def dat_names():
    with open(DAT_RECIPE, "rb") as f:
        names = [x["name"] for x in tomllib.load(f)["file"]]
    if names != sorted(names):
        fail("recipes/pc-zh.toml's files are not in sorted order")
    return names


def snd_names():
    """{kind: [name by id]} from recipes/pc-zh.snd.toml."""
    with open(SND_RECIPE, "rb") as f:
        rows = tomllib.load(f)["file"]
    by = collections.defaultdict(dict)
    for r in rows:
        if r["kind"] < 0:                  # a file no stream id names (019_02, dir1)
            continue
        if r["id"] in by[r["kind"]] and by[r["kind"]][r["id"]] != r["name"]:
            fail("pc-zh.snd.toml: kind %d id %d named twice" % (r["kind"], r["id"]))
        by[r["kind"]][r["id"]] = r["name"]
    out = {}
    for k, d in sorted(by.items()):
        if sorted(d) != list(range(len(d))):
            fail("pc-zh.snd.toml: kind %d's ids are not 0..%d" % (k, len(d) - 1))
        out[k] = [d[i] for i in range(len(d))]
    return out


def gen_text():
    lines = ["// src/hook/exe_own_names.inc - GENERATED by `tools/exe_own.py gen`; do not edit by hand.",
             "// The file names the port's two name tables hold (docs/exe-engine-own.md section 2):",
             "// recipes/pc-zh.toml's DAT/ containers in their (sorted) order, and recipes/pc-zh.snd.toml's",
             "// SND/ clips by Sound_LoadStream kind and id. Names only (CLAUDE.md rule 1).",
             "",
             "// clang-format off"]
    names = dat_names()
    lines.append("constexpr const char* kDatNames[%d] = {" % len(names))
    for i in range(0, len(names), 6):
        lines.append("    " + " ".join('"%s",' % n for n in names[i:i + 6]))
    lines.append("};")
    for k, ns in snd_names().items():
        lines.append("constexpr const char* kSndKind%d[%d] = {" % (k, len(ns)))
        for i in range(0, len(ns), 8):
            lines.append("    " + " ".join('"%s",' % n for n in ns[i:i + 8]))
        lines.append("};")
    lines.append("// clang-format on")
    return "\n".join(lines) + "\n"


def cmd_gen(a):
    with open(NAMES_INC, "w", newline="\n", encoding="utf-8") as f:
        f.write(gen_text())
    print("-> %s" % os.path.relpath(NAMES_INC, ROOT))


def check():
    errs = []
    if not os.path.exists(NAMES_INC):
        errs.append("src/hook/exe_own_names.inc: absent (tools/exe_own.py gen)")
    else:
        with open(NAMES_INC, encoding="utf-8") as f:
            if f.read() != gen_text():
                errs.append("src/hook/exe_own_names.inc: not what tools/exe_own.py gen writes from the recipes")
    regions = own_regions()
    if not regions:
        errs.append("src/hook/exe_own.cpp: no regions")
    else:
        # Every row of the table must be one own_regions() read (a row written
        # another way would be held by the engine and unknown to the tools).
        with open(OWN_CPP, encoding="utf-8") as f:
            block = re.search(r"// regions:\n(.*?)// end regions", f.read(), re.S).group(1)
        rows = sum(1 for line in block.splitlines() if line.strip().startswith("{"))
        if rows != len(regions):
            errs.append("src/hook/exe_own.cpp: %d rows in the region table, %d read by own_regions()" %
                        (rows, len(regions)))
    ptr = pointer_bytes()
    end = DATA_LO
    for lo, hi, name in regions:
        if lo < end or hi <= lo or hi > DATA_HI:
            errs.append("exe_own.cpp: %s 0x%06X..0x%06X is out of order, empty or outside .data" % (name, lo, hi))
        end = max(end, hi)
        if lo < KEY_TABLE[1] and hi > KEY_TABLE[0]:
            errs.append("exe_own.cpp: %s overlaps Key_TableDefault (RD's)" % name)
        if region_class(lo)[0] not in ("port-platform", "rule", "game-format"):
            errs.append("exe_own.cpp: %s 0x%06X is not in a port-platform, rule or game-format region of exe_own.py's "
                        "REGIONS" %
                        (name, lo))
    return errs


def text_words():
    """exe-pointers.tsv's words inside exe_own.cpp's regions: [(word, region name)]."""
    ptr = pointer_bytes()
    return [(w, name) for lo, hi, name in own_regions() for w in range(lo & ~3, hi, 4) if w in ptr]


def cmd_check(a):
    errs = check()
    for e in errs:
        print("exe_own: " + e)
    if errs:
        sys.exit(1)
    regions = own_regions()
    print("exe_own: %d regions, %d bytes; exe_own_names.inc matches the recipes; %d words exe-pointers.tsv "
          "calls pointers are text inside them (`text_words` lists them)" %
          (len(regions), sum(hi - lo for lo, hi, _ in regions), len(text_words())))


def cmd_text_words(a):
    by = collections.Counter(name for _, name in text_words())
    for w, name in text_words():
        print("0x%06X	%s" % (w, name))
    print("# %d words: %s" % (sum(by.values()), dict(by)), file=sys.stderr)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("measure")
    p.add_argument("--game", required=True)
    p.add_argument("--disc", action="append", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--analysis", default=os.path.join(ROOT, "analysis"))
    p.add_argument("--xref", help="pc_xref.json (default: <analysis>/pc_xref.json)")
    p.set_defaults(fn=cmd_measure)
    sub.add_parser("gen").set_defaults(fn=cmd_gen)
    sub.add_parser("check").set_defaults(fn=cmd_check)
    sub.add_parser("text_words").set_defaults(fn=cmd_text_words)
    a = ap.parse_args()
    a.fn(a)


if __name__ == "__main__":
    main()
