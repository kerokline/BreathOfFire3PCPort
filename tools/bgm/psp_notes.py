"""P10: do the PSP's sequences play the same notes as psx-jp's?

The PSP re-containers the music: VB gains a 0xC0-byte 'pBVC' header in front of
the same VAG bytes, VH becomes 'PPHD', and the SEP becomes 'pPMS', a table of
Standard MIDI Files. This compares, per sub-song, the event streams (tick,
channel, status, data) of the PSX SEP and the PSP SMF, notes first, plus the
VAG bytes and the tempo/loop events. Writes analysis/bgm/psp_notes.json.
"""
import hashlib, json, mmap, os, struct, sys
from collections import Counter
from bgm_paths import SIB
from bgm_paths import PC
sys.path.insert(0, SIB + "/tools")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import disc_ls, emi as emimod
from discs_compare import files, key, IMAGES


def varlen(d, p):
    v = 0
    while True:
        c = d[p]; p += 1
        v = (v << 7) | (c & 0x7F)
        if not c & 0x80:
            return v, p


def events(d, p, end, smf):
    """(tick, kind, ch, a, b) for every channel event; metas as ('meta', type, data)."""
    tick, run, out = 0, None, []
    while p < end:
        dt, p = varlen(d, p)
        tick += dt
        st = d[p]
        if st & 0x80:
            p += 1
            if st < 0xF0:
                run = st
        else:
            st = run
        if st == 0xFF:
            typ = d[p]; p += 1
            if smf:
                ln, p = varlen(d, p)
            else:
                ln = 0 if typ == 0x2F else (3 if typ == 0x51 else None)
                if typ == 0x2F:
                    p += 1
            data = d[p:p + ln]; p += ln
            out.append((tick, "meta", typ, data.hex()))
            if typ == 0x2F:
                return out, p
            continue
        if st in (0xF0, 0xF7):
            ln, p = varlen(d, p); p += ln; continue
        hi, ch = st & 0xF0, st & 0x0F
        if hi in (0xC0, 0xD0):
            out.append((tick, hi, ch, d[p], None)); p += 1
        else:
            a, b = d[p], d[p + 1]; p += 2
            if hi == 0x90 and b == 0:
                hi = 0x80  # note-on velocity 0 is a note-off
            out.append((tick, hi, ch, a, b))
    return out, p


def psx_subs(d):
    assert d[:4] == b"pQES"
    p, subs = 6, []
    while p + 13 <= len(d) and len(subs) < 4:
        res = struct.unpack_from(">H", d, p + 2)[0]
        tempo = (d[p + 4] << 16) | (d[p + 5] << 8) | d[p + 6]
        size = struct.unpack_from(">I", d, p + 9)[0]
        ev, _ = events(d, p + 13, len(d), False)
        subs.append(dict(res=res, tempo=tempo, ev=ev))
        p = p + 13 + size
    return subs


def psp_subs(d):
    assert d[:4] == b"pPMS", d[:4]
    hdr = struct.unpack_from("<I", d, 8)[0]
    subs = []
    for i in range((hdr - 16) // 16):
        o = 16 + 16 * i
        sid, _, off, size = struct.unpack_from("<HHII", d, o)
        s = d[off:off + size]
        assert s[:4] == b"MThd", s[:4]
        fmt, ntr, div = struct.unpack_from(">HHH", s, 8)
        p = 14
        ev = []
        for t in range(ntr):
            assert s[p:p + 4] == b"MTrk"
            ln = struct.unpack_from(">I", s, p + 4)[0]
            e, _ = events(s, p + 8, p + 8 + ln, True)
            ev += e
            p += 8 + ln
        subs.append(dict(res=div, fmt=fmt, tracks=ntr, ev=sorted(ev, key=lambda x: x[0]) if ntr > 1 else ev))
    return subs


def get(img, wanted):
    read, fl = files(img)
    out = {}
    for p, (ext, sz) in fl.items():
        k = key(p)
        if k in wanted:
            out[k] = disc_ls.read_extent(read, ext, sz)
    return out


def sections(blob):
    e = emimod.Emi(blob)
    r = {}
    for ent in e.entries:
        r.setdefault(ent["type"], []).append(blob[ent["offset"]:ent["offset"] + ent["size"]])
    return r


def main():
    inv = json.load(open(PC + "/analysis/bgm/inventory.json"))
    wanted = sorted(set(f[4:] for f in inv["disc"]))
    jp = get(IMAGES["psx-jp"], set(wanted))
    res = {}
    for tag in ("psp-jp", "psp-eu"):
        psp = get(IMAGES[tag], set(wanted))
        rows = {}
        for k in wanted:
            a, b = sections(jp[k]), sections(psp[k])
            r = {}
            # samples: the PSX VB inside the PSP VB after its header
            vb_same = []
            for va, vb in zip(a.get(7, []), b.get(7, [])):
                hdr = struct.unpack_from("<I", vb, 4)[0] if vb[:4] == b"pBVC" else 0
                vb_same.append(vb[hdr:hdr + len(va)] == va)
            r["vb_same"] = vb_same
            sa = psx_subs(a[10][0]); sb = psp_subs(b[10][0])
            subs = []
            for i, (x, y) in enumerate(zip(sa, sb)):
                nx = [e for e in x["ev"] if e[1] in (0x80, 0x90)]
                ny = [e for e in y["ev"] if e[1] in (0x80, 0x90)]
                cx = [e for e in x["ev"] if e[1] not in ("meta",)]
                cy = [e for e in y["ev"] if e[1] not in ("meta",)]
                kinds_x = Counter((e[1], e[3]) if e[1] == 0xB0 else e[1] for e in cx)
                kinds_y = Counter((e[1], e[3]) if e[1] == 0xB0 else e[1] for e in cy)
                subs.append(dict(
                    res=(x["res"], y["res"]), tempo_psx=x["tempo"],
                    notes=(len([e for e in nx if e[1] == 0x90]), len([e for e in ny if e[1] == 0x90])),
                    notes_same=nx == ny,
                    channel_events_same=cx == cy,
                    programs_psx=sorted(set(e[3] for e in cx if e[1] == 0xC0)),
                    programs_psp=sorted(set(e[3] for e in cy if e[1] == 0xC0)),
                    kinds_only_psx={str(k): v for k, v in (kinds_x - kinds_y).items()},
                    kinds_only_psp={str(k): v for k, v in (kinds_y - kinds_x).items()},
                    metas_psp=sorted(set(e[2] for e in y["ev"] if e[1] == "meta")),
                    tempo_psp=[e[3] for e in y["ev"] if e[1] == "meta" and e[2] == 0x51][:3],
                ))
            r["subs"] = subs
            r["vh_sizes"] = (len(a[6][0]), len(b[6][0]))
            rows[k] = r
        res[tag] = rows
        print(tag, "done", flush=True)
    json.dump(res, open(PC + "/analysis/bgm/psp_notes.json", "w"), indent=1, default=str)


if __name__ == "__main__":
    main()
