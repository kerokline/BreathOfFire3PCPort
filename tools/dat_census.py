#!/usr/bin/env python
"""EMI<->DAT census: run dat.py's pairing over every file and write JSON.

    python tools/dat_census.py <DAT dir> <EMI dir> --out analysis/dat_census.json

Pairs EMI non-audio sections with DAT kind-0/1 chunks by order-preserving
alignment (see `align`). A file with no legal alignment is recorded as
`unpaired`, never force-paired. Output is derived from game data: analysis/ only.
"""
import argparse, collections, json, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dat

AUDIO = (6, 7, 8, 10)


def align(emi, pc, blob):
    """Order-preserving alignment of EMI sections onto DAT chunks.

    Every chunk must be paired; EMI sections may be skipped (the port drops
    some). A pair is legal when image<->image with tag == dest, or
    non-image<->data. Among legal alignments, prefer identical content, then
    equal size. Decided by content, never by address: an earlier greedy version
    keyed on `dest >= 0x80100000` mis-paired 47 files (analysis/census/audit.md).
    """
    n, m = len(emi), len(pc)
    NEG = -1
    best = [[NEG] * (m + 1) for _ in range(n + 1)]
    back = [[None] * (m + 1) for _ in range(n + 1)]
    for i in range(n + 1):
        best[i][0] = 0
    for i in range(1, n + 1):
        _, t, dest, data = emi[i - 1]
        for j in range(1, min(i, m) + 1):
            if best[i - 1][j] > best[i][j]:
                best[i][j], back[i][j] = best[i - 1][j], "skip"
            c = pc[j - 1]
            legal = (c.kind == 1 and t == 3 and c.tag == dest) or (c.kind == 0 and t != 3)
            if legal and best[i - 1][j - 1] != NEG:
                score = 1 if any(data) else 0   # zero-fill (BSS) loses ties to real content
                if c.size == len(data):
                    score += 2
                    if blob[c.offset:c.offset + c.size] == data:
                        score += 4
                if best[i - 1][j - 1] + score > best[i][j]:
                    best[i][j], back[i][j] = best[i - 1][j - 1] + score, "pair"
    if best[n][m] == NEG:
        return None
    pairs, i, j = [], n, m
    while j > 0:
        if back[i][j] == "pair":
            pairs.append((i - 1, j - 1)); i -= 1; j -= 1
        else:
            i -= 1
    return pairs[::-1]


def census_file(dpath, epath):
    blob, chunks = dat.load(dpath)
    emi = [s for s in dat.emi_sections(epath) if s[1] not in AUDIO]
    pc = [c for c in chunks if c.kind in (0, 1)]
    rec = {"emi_sections": len(emi), "dat_chunks": len(pc),
           "kinds": collections.Counter(str(c.kind) for c in chunks), "pairs": [], "dropped": []}
    al = align(emi, pc, blob)
    if al is None:
        rec["unpaired_chunks"] = [c.index for c in pc]
        return rec
    used = set()
    for ei, ci in al:
        i, t, dest, data = emi[ei]; c = pc[ci]; used.add(ei)
        p = blob[c.offset:c.offset + c.size]
        rec["pairs"].append({"emi": i, "type": t, "dest": "%08X" % dest, "emi_size": len(data),
                             "dat": c.index, "kind": c.kind, "tag": "%08X" % c.tag, "dat_size": c.size,
                             "identical": p == data,
                             "diff_bytes": sum(1 for a, b in zip(p, data) if a != b)})
    for ei, (i, t, dest, data) in enumerate(emi):
        if ei not in used:
            rec["dropped"].append({"emi": i, "type": t, "dest": "%08X" % dest, "size": len(data),
                                   "all_zero": not any(data)})
    rec["unpaired_chunks"] = []
    return rec


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("datdir"); ap.add_argument("emidir"); ap.add_argument("--out", required=True)
    a = ap.parse_args()
    res = {}
    for f in sorted(os.listdir(a.datdir)):
        stem = f[:-4]
        e = os.path.join(a.emidir, stem + ".EMI")
        if f.upper().endswith(".DAT") and os.path.exists(e):
            res[stem] = census_file(os.path.join(a.datdir, f), e)
    with open(a.out, "w") as fh:
        json.dump(res, fh, indent=1)
    tot = collections.Counter()
    fam = collections.defaultdict(collections.Counter)
    for stem, r in res.items():
        k = re.sub(r"\d+", "", stem)
        for p in r["pairs"]:
            key = ("img" if p["kind"] == 1 else "data") + ("_same" if p["identical"] else "_diff")
            tot[key] += 1; fam[k][key] += 1
        tot["dropped"] += len(r["dropped"]); tot["unpaired_chunks"] += len(r["unpaired_chunks"])
        fam[k]["files"] += 1
    print(len(res), "files;", dict(tot))
    for k in sorted(fam):
        print("  %-10s %s" % (k, dict(fam[k])))


if __name__ == "__main__":
    main()
