#!/usr/bin/env python
"""What the PC's flat sound effects leave out of the disc's tones (docs/sound-import.md section 10).

    python tools/bgm/se_census.py --disc JP.cue --se-render <build>/se_render [--work DIR] [--json OUT]

Every sound bank of every EMI on the disc (vag.groups: the VAB header, cue table
and body the PC's kind-2 chunks are made from), every cue word that plays a
sample (vag.bank's rule: the chord's tones from the cue's tone, a silent sample a
stop): its tone's attributes the PC's bank drops (sound-import.md 2.3) - the
tone's volume, the program's and the VAB's master volume, the pan, the reverb
bit (mode bit 2), the ADSR words - counted per cue word and per distinct tone.
Then each distinct (sample, ADSR, pitch) through the SPU model twice
(tools/bgm/host se_render): with its ADSR, and with an envelope full at once and
held, as the PC plays every sample; the energy ratio in dB is what the envelope
alone takes away. A one-shot sample is rendered to its end, a repeating one for
2 s. The pitch is the cue word's rate (vag.frequency) as 4096 = 44,100 Hz.

What it does not measure: SE_Play's own volume path (SsUtKeyOnV's volL / volR,
the cue's pan flag and the per-cue reset to 0x17FF, the sibling's
SOUND_CUES.md) - not read here, so the static volumes are reported as the
attributes, not as a level the PSX plays at. Game data stays in --work
(scratch, CLAUDE.md rule 1).
"""
import argparse, collections, hashlib, json, os, struct, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import region_diff  # noqa: E402
import vag          # noqa: E402

TONES = 32 + vag.VAB_PROGRAMS * 16


def words(g):
    """(cue, j, tone bytes, program record, sample bytes, loops) for every playing voice word of a bank group."""
    vh, vb = g[6][3], g[7][3]
    cues = g[8][3] if 8 in g else b""
    programs, = struct.unpack_from("<H", vh, 0x12)
    nvag, = struct.unpack_from("<H", vh, 0x16)
    sizes = struct.unpack_from("<256H", vh, TONES + programs * 16 * vag.TONE)
    vags, pos = {}, 0
    for v in range(1, nvag + 1):
        vags[v] = vb[pos:pos + sizes[v] * 8]
        pos += sizes[v] * 8
    for i in range(len(cues) // 4):
        flags, prog, tone, chord = cues[4 * i:4 * i + 4]
        voices = ((chord >> 4) >> 1) + 1 if chord >> 4 else 0
        for j in range(voices):
            t = vh[TONES + (prog & 0x7F) * 16 * vag.TONE + ((tone >> 4) + j) * vag.TONE:][:vag.TONE]
            v, = struct.unpack_from("<H", t, 22)
            body = vags.get(v, b"")
            blocks, loops = vag.trim(body) if body else (b"", False)
            if not any(vag.decode_port(blocks)):
                continue                    # a stop word (sound-import.md 2.3)
            yield i, j, t, vh[0x20 + (prog & 0x7F) * 16:][:16], body, loops


def quart(xs):
    xs = sorted(xs)
    if not xs:
        return "-"
    q = lambda f: xs[min(len(xs) - 1, int(f * (len(xs) - 1) + 0.5))]
    return "min %s, quartiles %s / %s / %s, max %s" % (xs[0], q(0.25), q(0.5), q(0.75), xs[-1])


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--disc", required=True)
    ap.add_argument("--se-render", required=True)
    ap.add_argument("--work", default=None, help="scratch for the samples and jobs (default: a temporary directory)")
    ap.add_argument("--json", default=None)
    a = ap.parse_args()
    import tempfile
    work = a.work or tempfile.mkdtemp(prefix="se_census_")
    os.makedirs(work, exist_ok=True)
    disc = region_diff.Build("disc", a.disc)
    st = collections.Counter()
    vol, pvol, mvol, pans, adsr = [], [], [], collections.Counter(), collections.Counter()
    jobs, per_word = {}, []
    for key in sorted(disc.emis):
        for g in vag.groups(disc.sections(key)):
            if 8 not in g:
                continue
            st["banks"] += 1
            for i, j, t, pr, body, loops in words(g):
                st["words"] += 1
                st["loops"] += loops
                vol.append(t[2]); pvol.append(pr[1]); mvol.append(g[6][3][0x18])
                pans[t[3] == 64] += 1
                st["reverb"] += bool(t[1] & 4)
                a1, a2 = struct.unpack_from("<HH", t, 16)
                adsr["%04X/%04X" % (a1, a2)] += 1
                f = vag.frequency(t)
                pitch = min(0x3FFF, int(round(f * 4096 / 44100)))
                h = hashlib.sha256(body).hexdigest()[:16]
                jid = "%s_%04x_%04x_%04x" % (h, a1, a2, pitch)
                if jid not in jobs:
                    path = os.path.join(work, h + ".vag")
                    if not os.path.exists(path):
                        open(path, "wb").write(body)
                    n = len(body) // 16 * 28
                    frames = 88200 if loops else int(n * 44100 / max(f, 1)) + 4096
                    jobs[jid] = (path, a1, a2, pitch, frames, loops)
                per_word.append(jid)
    jf = os.path.join(work, "jobs.txt")
    with open(jf, "w") as fh:
        for jid, (path, a1, a2, pitch, frames, _) in jobs.items():
            fh.write("%s %s %x %x %x %d\n" % (jid, path, a1, a2, pitch, frames))
    r = subprocess.run([a.se_render, jf], capture_output=True, text=True, check=True)
    res = {}
    for line in r.stdout.splitlines():
        jid, fe, ee, ff, ef, att = line.split()
        res[jid] = dict(frames_env=int(fe), energy_env=float(ee), frames_flat=int(ff), energy_flat=float(ef), attack=int(att))
    import math
    loss = {}
    for jid, x in res.items():
        loss[jid] = 10 * math.log10(x["energy_env"] / x["energy_flat"]) if x["energy_flat"] > 0 and x["energy_env"] > 0 else None
    lw = [loss[j] for j in per_word if loss[j] is not None]
    ld = [v for v in loss.values() if v is not None]
    bins = collections.Counter()
    for v in lw:
        bins["0..-0.1 dB" if v > -0.1 else "-0.1..-1" if v > -1 else "-1..-3" if v > -3 else "-3..-6" if v > -6 else "-6..-12" if v > -12 else "below -12"] += 1
    att_ms = [res[j]["attack"] * 1000 / 44100 for j in per_word if res[j]["attack"] >= 0]
    out = dict(banks=st["banks"], words=st["words"], distinct=len(jobs), loops=st["loops"], reverb=st["reverb"],
               pan_centre=pans[True], pan_off=pans[False],
               tone_vol=quart(vol), prog_vol=quart(pvol), vab_vol=quart(mvol),
               envelope_loss_words=quart([round(v, 2) for v in lw]), envelope_loss_distinct=quart([round(v, 2) for v in ld]),
               envelope_bins=dict(bins), attack_ms_words=quart([round(v, 1) for v in att_ms]),
               never_full=sum(1 for j in per_word if res[j]["attack"] < 0),
               adsr_words=dict(adsr.most_common(12)), adsr_distinct_pairs=len(adsr))
    for k, v in out.items():
        print("%-24s %s" % (k, v))
    if a.json:
        json.dump(dict(summary=out, tones={j: dict(res[j], loss_db=loss[j]) for j in res}), open(a.json, "w"), indent=1)


if __name__ == "__main__":
    main()
