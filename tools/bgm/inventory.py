"""Step 1 of docs/bgm-comparison.md: inventory both sides and pair them.

Writes analysis/bgm/inventory.json and prints a summary. Numbers only.
"""
import glob, hashlib, json, os, re, struct, subprocess, sys

from bgm_paths import PC
from bgm_paths import SIB
from bgm_paths import BIN
from bgm_paths import SLPS
OUT = PC + "/analysis/bgm"
sys.path.insert(0, SIB + "/tools")
import emi as emimod  # sibling tool, read only
import file_ids       # sibling tool, read only

BR = {1: [0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320],
      2: [0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160]}
SR = {3: [44100, 48000, 32000], 2: [22050, 24000, 16000], 0: [11025, 12000, 8000]}
MODES = ["stereo", "joint", "dual", "mono"]


def mp3_walk(b):
    """Every frame header of a bare MPEG audio stream."""
    pos, frames = 0, []
    id3 = b[:3] == b"ID3"
    if id3:
        sz = (b[6] << 21) | (b[7] << 14) | (b[8] << 7) | b[9]
        pos = 10 + sz
    junk = 0
    while pos + 4 <= len(b):
        h = struct.unpack_from(">I", b, pos)[0]
        if (h >> 21) & 0x7FF != 0x7FF:
            junk += 1; pos += 1; continue
        ver = (h >> 19) & 3; lay = (h >> 17) & 3; prot = (h >> 16) & 1
        bri = (h >> 12) & 15; sri = (h >> 10) & 3; pad = (h >> 9) & 1
        mode = (h >> 6) & 3; mext = (h >> 4) & 3; emph = h & 3
        if ver == 1 or lay != 1 or bri in (0, 15) or sri == 3:
            junk += 1; pos += 1; continue
        br = BR[1 if ver == 3 else 2][bri] * 1000
        sr = SR[ver][sri]
        spf = 1152 if ver == 3 else 576
        flen = (spf // 8) * br // sr + pad
        frames.append((pos, ver, br, sr, mode, mext, prot, (h >> 3) & 1, (h >> 2) & 1, emph, flen))
        pos += flen
    return frames, junk, id3, b[-128:-125] == b"TAG"


def mp3_info(path):
    b = open(path, "rb").read()
    fr, junk, id3, id3v1 = mp3_walk(b)
    f0 = fr[0]
    side = 32 if f0[4] != 3 else 17
    xing = b[f0[0] + 4 + side: f0[0] + 4 + side + 4]
    vers = {3: "MPEG-1", 2: "MPEG-2", 0: "MPEG-2.5"}
    brs = sorted(set(f[2] for f in fr))
    modes = sorted(set(MODES[f[4]] for f in fr))
    # main_data_begin of the first frame (9 bits after the header/CRC)
    mdb0 = (b[f0[0] + 4 + (2 if f0[6] == 0 else 0)] << 1) | (b[f0[0] + 5 + (2 if f0[6] == 0 else 0)] >> 7)
    enc_strings = [m.group().decode() for m in re.finditer(rb"(LAME|Lavf|Xing|Info|FhG|Fraunhofer|GOGO|BladeEnc)[ -~]{0,20}", b)][:5]
    nframes = len(fr)
    return {
        "bytes": len(b), "md5": hashlib.md5(b).hexdigest(),
        "version": vers[f0[1]], "layer": 3, "sample_rate": f0[3],
        "bitrates": brs, "cbr": len(brs) == 1, "channel_modes": modes,
        "crc": f0[6] == 0, "copyright": f0[7], "original": f0[8], "emphasis": f0[9],
        "frames": nframes, "samples_coded": nframes * 1152,
        "seconds_coded": nframes * 1152 / f0[3],
        "xing_or_info": xing in (b"Xing", b"Info"), "id3v2": id3, "id3v1": id3v1,
        "junk_bytes": junk, "first_main_data_begin": mdb0,
        "encoder_strings": enc_strings,
    }


# ---- SEQ / SEP ---------------------------------------------------------

def varlen(d, p):
    v = 0
    while True:
        c = d[p]; p += 1
        v = (v << 7) | (c & 0x7F)
        if not c & 0x80:
            return v, p


def parse_track(d, p, end, res, tempo):
    """Walk one SEQ track; returns events summary and the end position."""
    tick, run = 0, None
    t_us = 0.0
    cur_tempo = tempo
    loops, tempos, notes, chans, progs = [], [], 0, set(), set()
    ccs = {}
    last_note_tick = 0
    while p < end:
        dt, p = varlen(d, p)
        tick += dt
        t_us += dt * cur_tempo / res
        st = d[p]
        if st & 0x80:
            p += 1
            if st != 0xFF:
                run = st
        else:
            st = run
        if st == 0xFF:
            typ = d[p]; p += 1
            if typ == 0x2F:
                p += 1  # Sony writes FF 2F 00
                return dict(ticks=tick, seconds=t_us / 1e6, loops=loops, tempos=tempos,
                            notes=notes, channels=sorted(chans), programs=sorted(progs),
                            ccs={hex(k): v for k, v in sorted(ccs.items())},
                            last_note_tick=last_note_tick), p
            if typ == 0x51:
                cur_tempo = (d[p] << 16) | (d[p + 1] << 8) | d[p + 2]; p += 3
                tempos.append((tick, cur_tempo))
                continue
            raise ValueError("meta %02X at %d" % (typ, p))
        hi, ch = st & 0xF0, st & 0x0F
        chans.add(ch)
        if hi in (0x80, 0x90, 0xA0, 0xB0, 0xE0):
            a, b2 = d[p], d[p + 1]; p += 2
            if hi == 0x90 and b2:
                notes += 1; last_note_tick = tick
            if hi == 0xB0:
                ccs[a] = ccs.get(a, 0) + 1
                if a == 0x63 and b2 in (20, 30):
                    loops.append(dict(kind="start" if b2 == 20 else "end", tick=tick,
                                      seconds=t_us / 1e6, ch=ch))
                if a == 0x06 and loops and loops[-1]["tick"] == tick:
                    loops[-1]["count"] = b2
        elif hi in (0xC0, 0xD0):
            if hi == 0xC0:
                progs.add(d[p])
            p += 1
        else:
            raise ValueError("status %02X" % st)
    raise ValueError("no end of track")


def parse_sep(d):
    assert d[:4] == b"pQES", d[:4]
    ver = struct.unpack_from(">H", d, 4)[0]
    subs = []
    if ver == 0:  # SEP: several sequences
        p = 6
        while p + 13 <= len(d):
            sid, res = struct.unpack_from(">HH", d, p)
            tempo = (d[p + 4] << 16) | (d[p + 5] << 8) | d[p + 6]
            rhy = d[p + 7], d[p + 8]
            size = struct.unpack_from(">I", d, p + 9)[0]
            body = p + 13
            info, endp = parse_track(d, body, len(d), res, tempo)
            info.update(id=sid, resolution=res, tempo_us=tempo, bpm=round(6e7 / tempo, 3),
                        rhythm=rhy, data_bytes=size, data_bytes_walked=endp - body,
                        md5=hashlib.md5(d[p:body + size]).hexdigest())
            subs.append(info)
            p = body + size
            if p >= len(d) or d[p:p + 2] == b"\xff\xff":
                break
    else:
        res = struct.unpack_from(">H", d, 8)[0]
        tempo = (d[10] << 16) | (d[11] << 8) | d[12]
        info, endp = parse_track(d, 15, len(d), res, tempo)
        info.update(id=0, resolution=res, tempo_us=tempo, bpm=round(6e7 / tempo, 3),
                    md5=hashlib.md5(d).hexdigest())
        subs.append(info)
    return ver, subs


def main():
    os.makedirs(OUT, exist_ok=True)
    # PC side
    pc = {}
    for f in sorted(glob.glob(PC + "/bof3/BGM/*")):
        name = os.path.basename(f)
        m = re.match(r"(\d{3})(N?)\.dat$", name, re.I)
        info = mp3_info(f)
        info["file"] = name; info["once"] = bool(m.group(2))
        pc[int(m.group(1))] = info
    # ffprobe cross-check of duration and codec
    for n, info in pc.items():
        r = subprocess.run(["ffprobe", "-v", "error", "-show_entries",
                            "stream=codec_name,sample_rate,channels,bit_rate:format=duration",
                            "-of", "json", PC + "/bof3/BGM/" + info["file"]],
                           capture_output=True, text=True)
        info["ffprobe"] = json.loads(r.stdout)
    # disc side: every SEQ section
    secs = json.load(open(SIB + "/analysis/emi_sections.json"))["sections"]
    seqfiles = sorted(set(s["file"] for s in secs if s["type"] == 10))
    disc = {}
    for rel in seqfiles:
        blob = open(BIN + "/" + rel[4:], "rb").read()
        e = emimod.Emi(blob, rel)
        for ent in e.entries:
            if ent["type"] == 10:
                d = blob[ent["offset"]:ent["offset"] + ent["size"]]
                ver, subs = parse_sep(d)
                disc[rel] = dict(section=ent["index"], bytes=ent["size"], dest=ent["dest"],
                                 md5=hashlib.md5(d).hexdigest(), sep_version=ver, subs=subs)
    # the PSX song table
    x = open(SLPS, "rb").read()
    off = 0x800 + 0x80182830 - 0x80093800
    rows = file_ids.build(cue=SIB + "/isos/Breath of Fire III (Japan).cue", exe=SLPS)
    ids = {int(r["id"], 16): r["path"] for r in rows}
    table = []
    for i in range(165):
        fid, seq, sub = struct.unpack_from("<HBB", x, off + 4 * i)
        table.append(dict(song=i, file_id=fid, file=ids.get(fid), seq=seq, sub=sub))
    json.dump(dict(pc=pc, disc=disc, table=table), open(OUT + "/inventory.json", "w"), indent=1)
    print("pc files", len(pc), "disc SEQ files", len(disc))


if __name__ == "__main__":
    main()
