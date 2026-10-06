"""Music sections of every held disc against psx-jp: per-file VH / SEQ / VB md5, per sub-song md5.

Reads the images directly (never writes). Writes analysis/bgm/discs_compare.json.
"""
import hashlib, json, mmap, os, struct, sys
from bgm_paths import SIB
from bgm_paths import PC
sys.path.insert(0, SIB + "/tools")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import disc_ls, emi as emimod
from inventory import parse_sep

IMAGES = {
    "psx-jp": SIB + "/isos/Breath of Fire III (Japan).cue",
    "psx-us": SIB + "/isos/Breath of Fire III (USA).cue",
    "psx-eu": SIB + "/isos/Breath of Fire III (Europe).cue",
    "psx-fr": SIB + "/isos/Breath of Fire III (France).cue",
    "psx-de": SIB + "/isos/Breath of Fire III (Germany).cue",
    "psp-jp": PC + "/CDImage/Breath of Fire III (Japan).iso",
    "psp-eu": PC + "/CDImage/Breath of Fire III (PSP) (Europe).iso",
}


def files(img):
    path = disc_ls.resolve_cue(img) if img.endswith(".cue") else img
    fh = open(path, "rb")
    mm = mmap.mmap(fh.fileno(), 0, access=mmap.ACCESS_READ)
    read, _ = disc_ls.make_reader(mm)
    pvd = read(16)
    tree = disc_ls.walk(read, struct.unpack_from("<I", pvd, 158)[0], struct.unpack_from("<I", pvd, 166)[0])
    out = {}
    for p, ext, sz, isdir in tree:
        if isdir or not p.upper().endswith(".EMI"):
            continue
        out[p] = (ext, sz)
    return read, out


def key(p):
    """Normalise a path to BIN-relative: BGM/BGM000.EMI etc."""
    u = p.upper().replace("\\", "/")
    for pre in ("PSP_GAME/USRDIR/JPN/", "PSP_GAME/USRDIR/USA/", "PSP_GAME/USRDIR/", "BIN/"):
        if u.startswith(pre):
            return u[len(pre):]
    return u


def music(blob, name):
    e = emimod.Emi(blob, name)
    r = {}
    for ent in e.entries:
        if ent["type"] in (6, 7, 10, 8):
            d = blob[ent["offset"]:ent["offset"] + ent["size"]]
            k = "%d@%x" % (ent["type"], ent["dest"])
            r[k] = dict(md5=hashlib.md5(d).hexdigest(), size=ent["size"])
            if ent["type"] == 10:
                try:
                    _, subs = parse_sep(d)
                    r[k]["subs"] = [s["md5"] for s in subs]
                except Exception as ex:
                    r[k]["subs_error"] = str(ex)
    return r


def main():
    res = {}
    for tag, img in IMAGES.items():
        if not os.path.exists(img):
            res[tag] = "absent"; continue
        read, fl = files(img)
        res[tag] = {}
        for p, (ext, sz) in fl.items():
            k = key(p)
            if not (k.startswith("BGM/") or k.startswith("BOSS/") or k in ("BATTLE/BATTLE.EMI", "BATTLE/BATTLE2.EMI", "ETC/DEMO.EMI")):
                continue
            blob = disc_ls.read_extent(read, ext, sz)
            try:
                res[tag][k] = music(blob, p)
            except Exception as ex:
                res[tag][k] = {"error": str(ex)}
        print(tag, len(res[tag]), flush=True)
    json.dump(res, open(PC + "/analysis/bgm/discs_compare.json", "w"), indent=1)


if __name__ == "__main__":
    main()
