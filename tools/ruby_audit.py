"""The readings audit: a second opinion on the furigana builder's readings.

The follow-on group decided 2026-10-22 (docs/owner-review.md): the builder's
`--review` list is sharded by area into blind text (no readings) for agents,
each agent gives the kana of every kanji word of every message as JSON, and
this script diffs their kana against what the builder printed (SudachiPy)
into a disagreement list by surface and context, most frequent first, for
the owner to adjudicate into data/furigana/readings.toml.

    python tools/ruby_audit.py shard  REVIEW.txt OUTDIR [--size 300]
    python tools/ruby_audit.py diff   REVIEW.txt KANADIR OUT.txt [--json OUT.json]
    python tools/ruby_audit.py report OUT.json CANDIDATES.toml SHEET.md proposals_out_*.json

`shard` writes OUTDIR/shard_NN.json: [{"id", "label", "text"}] with every
distinct message once (the pools repeat the same string across 40 BOSS
files), controls taken out: `<01>` / `<02>` become "／", a runtime insert
(an item, a number, a name) becomes "＊". `diff` reads KANADIR/*.json, each
{"id": [[surface, kana], ...]} (an agent's output, surfaces in text order),
trims a reading the way the builder does (the kana the page shows at either
end of the word are not read over it), and lists where the two disagree.

Every file this writes is game text: the harness root only, never the tree
(hard rule 1). The only thing that goes in the tree is a rule the owner
approves into readings.toml.
"""
import argparse
import collections
import json
import os
import re
import sys

KANJI = re.compile(r"[一-鿿々]")        # 々 counts as a kanji
KANA = re.compile(r"[ぁ-ゟァ-ヿー]")
CTRL = re.compile(r"<([0-9a-f]+)>")
INSERTS = {"03", "04", "07", "08"}                   # the runtime inserts (reading-row-builder.md 4)


def parse_review(path):
    """[(label, text, [(surface, printed reading)])] in file order."""
    out = []
    with open(path, encoding="utf-8") as f:
        blocks = f.read().split("\n\n")
    for blk in blocks:
        lines = blk.split("\n")
        if len(lines) < 2 or not lines[0].startswith("== "):
            continue
        label = lines[0][3:].split(":")[0]
        words = []
        if len(lines) > 2 and lines[2].strip():
            for w in lines[2].split("  "):
                m = re.match(r"^(.*)\((.*)\)$", w)
                if m:
                    words.append((m.group(1), m.group(2)))
        out.append((label, lines[1], words))
    return out


def blind(text):
    """The message as an agent reads it: no readings, controls as separators."""
    def sub(m):
        code = m.group(1)
        if code in ("01", "02"):
            return "／"
        if code[:2] in INSERTS:
            return "＊"
        return ""
    return CTRL.sub(sub, text)


def distinct(entries):
    """One entry per distinct text, the first label kept; area order, pools last."""
    seen, out = {}, []
    for label, text, words in entries:
        if text in seen:
            continue
        seen[text] = len(out)
        out.append((label, text, words))

    def key(e):
        name = e[0].split()[0]
        return (0 if name.startswith("AREA") else 1, name)
    out.sort(key=key)
    return out


def cmd_shard(args):
    entries = distinct(parse_review(args.review))
    os.makedirs(args.outdir, exist_ok=True)
    shards = [entries[i:i + args.size] for i in range(0, len(entries), args.size)]
    for n, sh in enumerate(shards):
        rows = [{"id": "%02d-%03d" % (n, k), "label": label, "text": blind(text)}
                for k, (label, text, _) in enumerate(sh)]
        with open(os.path.join(args.outdir, "shard_%02d.json" % n), "w", encoding="utf-8") as f:
            json.dump(rows, f, ensure_ascii=False, indent=0)
    # The key back from an id to the review entry, for `diff`.
    with open(os.path.join(args.outdir, "index.json"), "w", encoding="utf-8") as f:
        json.dump({"size": args.size, "count": len(entries)}, f)
    print("%d distinct messages -> %d shards of up to %d in %s" % (len(entries), len(shards), args.size, args.outdir))


def to_hiragana(s):
    return "".join(chr(ord(c) - 0x60) if "ァ" <= c <= "ヶ" else c for c in s)


def trim(surface, reading):
    """The builder's trim (reading-row-builder.md 3.3): the kana the surface shows
    at either end are taken off the reading; kana between kanji stay."""
    reading = to_hiragana(reading)
    head = 0
    while head < len(surface) and KANA.match(surface[head]) and reading.startswith(to_hiragana(surface[head])):
        reading = reading[1:]
        head += 1
    tail = len(surface)
    while tail > head and KANA.match(surface[tail - 1]) and reading.endswith(to_hiragana(surface[tail - 1])):
        reading = reading[:-1]
        tail -= 1
    return reading


def spans(text, words):
    """[(start, end, surface, reading)]: each surface found in order in the text."""
    out, pos = [], 0
    for surface, reading in words:
        at = text.find(surface, pos)
        if at < 0:
            at = text.find(surface)      # a token found earlier than expected: the builder's order is per row
            if at < 0:
                continue
        out.append((at, at + len(surface), surface, reading))
        pos = at + len(surface)
    return out


def kana_extension(token, s0, e0):
    """The agent's reading narrowed to the builder's span [s0, e0) when the agent's
    token (start, end, surface, reading) is the same word written longer by kana
    only (入れた for the builder's 入れ): the extra kana come off the reading. None
    when the token is not that."""
    start, end, surface, reading = token
    if start > s0 or end < e0:
        return None
    pre, post = surface[:s0 - start], surface[len(surface) - (end - e0):] if end > e0 else ""
    if not all(KANA.match(c) for c in pre + post):
        return None
    r = to_hiragana(reading)
    pre, post = to_hiragana(pre), to_hiragana(post)
    if not r.startswith(pre) or (post and not r.endswith(post)):
        return None
    return r[len(pre):len(r) - len(post)] if post else r[len(pre):]


def walk_kana(text, start, end, tokens, kana_of):
    """The kana of text[start:end] built from the tokens lying inside it (each
    read by kana_of) and the kana written between them; None when a character
    is neither (a kanji nobody read)."""
    starts = {t[0]: t for t in tokens if start <= t[0] and t[1] <= end}
    out, k = [], start
    while k < end:
        if k in starts:
            out.append(to_hiragana(kana_of(starts[k])))
            k = starts[k][1]
        elif KANA.match(text[k]):
            out.append(to_hiragana(text[k]))
            k += 1
        else:
            return None
    return "".join(out)


def context(text, start, end, width=10):
    return text[max(0, start - width):start] + "【" + text[start:end] + "】" + text[end:end + width]


def cmd_diff(args):
    entries = distinct(parse_review(args.review))
    size = json.load(open(os.path.join(args.kanadir, "index.json"), encoding="utf-8"))["size"] \
        if os.path.exists(os.path.join(args.kanadir, "index.json")) else args.size
    by_id = {"%02d-%03d" % (k // size, k % size): e for k, e in enumerate(entries)}
    agent = {}
    for name in sorted(os.listdir(args.kanadir)):
        if not name.endswith(".json") or name == "index.json":
            continue
        with open(os.path.join(args.kanadir, name), encoding="utf-8") as f:
            data = json.load(f)
        if isinstance(data, dict):
            agent.update(data)
    # (surface, builder reading, agent reading) -> [contexts]; kind reading|segment
    reading_dis, segment_dis = collections.defaultdict(list), collections.defaultdict(list)
    n_msgs = n_words = n_agree = n_missing = 0
    missing_msgs = 0
    for mid, (label, text, words) in by_id.items():
        if mid not in agent:
            missing_msgs += 1
            continue
        n_msgs += 1
        bt = blind(text)
        ours = spans(bt, words)
        theirs = spans(bt, [(s, r) for s, r in agent[mid] if isinstance(s, str) and isinstance(r, str)])
        covered = set()
        for s0, e0, surf, printed in ours:
            n_words += 1
            if (s0, e0) in covered:
                continue
            over = [t for t in theirs if t[0] < e0 and s0 < t[1]]
            if not over:
                n_missing += 1
                continue
            if len(over) == 1 and kana_extension(over[0], s0, e0) is not None:
                mine = trim(surf, kana_extension(over[0], s0, e0))
                if mine == trim(surf, printed):
                    n_agree += 1
                else:
                    reading_dis[(surf, printed, mine)].append((label, context(bt, s0, e0)))
                continue
            if len(over) == 1 and over[0][0] <= s0 and e0 <= over[0][1]:
                # The agent read a compound whole that the builder read in pieces
                # (変異体 as 変異 + 体): the builder's pieces, with the kana between
                # them as written, against the agent's kana for the whole.
                ts, te, tsurf, tkana = over[0]
                pieces = [o for o in ours if ts <= o[0] and o[1] <= te]
                for o in pieces:
                    covered.add((o[0], o[1]))
                expect = walk_kana(bt, ts, te, pieces, lambda o: o[3])
                if expect is not None and expect == to_hiragana(tkana):
                    n_agree += 1
                    continue
                # A numeral the agent read into its word (3人 さんにん; digits are
                # not kanji, so the builder's token is 人 alone): the rest must match.
                ts2 = ts
                while ts2 < te and not KANJI.match(bt[ts2]) and not KANA.match(bt[ts2]):
                    ts2 += 1
                if ts2 > ts:
                    expect = walk_kana(bt, ts2, te, pieces, lambda o: o[3])
                    if expect is not None and to_hiragana(tkana).endswith(expect):
                        n_agree += 1
                        continue
                theirs_txt = "%s(%s)" % (tsurf, to_hiragana(tkana))
                ours_txt = " ".join("%s(%s)" % (o[2], o[3]) for o in pieces)
                segment_dis[(tsurf, ours_txt, theirs_txt)].append((label, context(bt, ts, te)))
                continue
            if all(s0 <= t[0] and t[1] <= e0 for t in over):
                # The builder's token holds the agent's (この先 against 先, 次の日
                # against 次 + 日): the agent's kana with the kana between, trimmed
                # as the builder trims, against what the builder printed.
                got = walk_kana(bt, s0, e0, over, lambda t: t[3])
                if got is not None:
                    mine = trim(surf, got)
                    if mine == trim(surf, printed):
                        n_agree += 1
                    else:
                        reading_dis[(surf, printed, mine)].append((label, context(bt, s0, e0)))
                    continue
            theirs_txt = " ".join("%s(%s)" % (t[2], to_hiragana(t[3])) for t in over)
            segment_dis[(surf, printed, theirs_txt)].append((label, context(bt, s0, e0)))

    def write_group(f, title, dis):
        by_surface = collections.Counter()
        for (surf, _, _), ctxs in dis.items():
            by_surface[surf] += len(ctxs)
        f.write("## %s: %d distinct, %d occurrences\n\n" % (title, len(dis), sum(by_surface.values())))
        for surf, total in by_surface.most_common():
            rows = sorted(((k, v) for k, v in dis.items() if k[0] == surf), key=lambda kv: -len(kv[1]))
            for (s, printed, mine), ctxs in rows:
                f.write("%5d  %s\tbuilder %s\tagent %s\n" % (len(ctxs), s, printed, mine))
                for label, c in ctxs[:args.contexts]:
                    f.write("           %-22s %s\n" % (label, c))
            f.write("\n")

    with open(args.out, "w", encoding="utf-8") as f:
        f.write("# Readings audit: the agents' kana against the builder's (SudachiPy)\n")
        f.write("# messages compared %d (agent output missing for %d), words %d: agree %d, "
                "reading differs %d, segmentation differs %d, no agent token %d\n\n"
                % (n_msgs, missing_msgs, n_words, n_agree,
                   sum(len(v) for v in reading_dis.values()), sum(len(v) for v in segment_dis.values()), n_missing))
        write_group(f, "Reading differs (same word)", reading_dis)
        write_group(f, "Segmentation differs", segment_dis)
    if args.json:
        with open(args.json, "w", encoding="utf-8") as f:
            json.dump({"reading": [{"surface": k[0], "builder": k[1], "agent": k[2], "n": len(v), "contexts": v}
                                   for k, v in sorted(reading_dis.items(), key=lambda kv: -len(kv[1]))],
                       "segment": [{"surface": k[0], "builder": k[1], "agent": k[2], "n": len(v), "contexts": v}
                                   for k, v in sorted(segment_dis.items(), key=lambda kv: -len(kv[1]))]},
                      f, ensure_ascii=False, indent=1)
    print("messages %d (missing %d), words %d: agree %d, reading differs %d (%d distinct), "
          "segmentation differs %d (%d distinct), no agent token %d -> %s"
          % (n_msgs, missing_msgs, n_words, n_agree, sum(len(v) for v in reading_dis.values()), len(reading_dis),
             sum(len(v) for v in segment_dis.values()), len(segment_dis), n_missing, args.out))


def rule_blocks(path):
    """The [[reading]] blocks of a candidate fragment, each as its text."""
    blocks, cur = [], []
    for line in open(path, encoding="utf-8"):
        if line.startswith("[[reading]]"):
            if cur:
                blocks.append("".join(cur))
            cur = [line]
        elif cur and line.strip() and not line.startswith("#"):
            cur.append(line)
        elif cur and not line.strip():
            blocks.append("".join(cur))
            cur = []
    if cur:
        blocks.append("".join(cur))
    return blocks


def cmd_reach(args):
    """Each candidate rule built alone over the base sidecar: how many placements
    it changes against the baseline review, so a rule meant for n lines that
    rewrites hundreds is caught before the owner reads it."""
    import subprocess
    import tempfile
    import concurrent.futures
    base = open(args.base, encoding="utf-8").read()
    baseline = {t: w for _, t, w in parse_review(args.review)}
    blocks = rule_blocks(args.candidates)
    os.makedirs(args.outdir, exist_ok=True)
    builder = os.path.join(os.path.dirname(os.path.abspath(__file__)), "ruby_build.py")

    def one(k):
        d = os.path.join(args.outdir, "rule_%03d" % k)
        os.makedirs(d, exist_ok=True)
        toml_path = os.path.join(d, "readings.toml")
        with open(toml_path, "w", encoding="utf-8") as f:
            f.write(base + "\n" + blocks[k])
        rev = os.path.join(d, "review.txt")
        r = subprocess.run([sys.executable, builder, "furigana", "--game", args.game, "--lang", args.lang,
                            "--out", os.path.join(d, "out"), "--readings", toml_path, "--review", rev],
                           capture_output=True, text=True, encoding="utf-8", errors="replace")
        if r.returncode:
            return k, {"error": (r.stderr or r.stdout)[-800:]}
        changes = collections.Counter()
        for _, t, w in parse_review(rev):
            if t in baseline and baseline[t] != w:
                for x, y in zip(baseline[t], w):
                    if x != y:
                        changes["%s(%s) -> %s(%s)" % (x[0], x[1], y[0], y[1])] += 1
                if len(baseline[t]) != len(w):
                    changes["words %d -> %d" % (len(baseline[t]), len(w))] += 1
        return k, {"changed": sum(changes.values()), "top": changes.most_common(6)}

    out = {}
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as ex:
        for k, res in ex.map(one, range(len(blocks))):
            m = re.search(r'surface = (".*?")\s*\nreading = (".*?")', blocks[k])
            res["surface"], res["reading"] = json.loads(m.group(1)), json.loads(m.group(2))
            res["block"] = blocks[k]
            out[k] = res
            print("%3d  %-14s %-14s %s" % (k, res["surface"], res["reading"],
                                            res.get("error", "changed %d" % res.get("changed", -1))[:60]), flush=True)
    with open(args.json, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=1)
    print("%d rules measured -> %s" % (len(out), args.json))


def cmd_report(args):
    """The proposal pass merged: a candidate readings.toml fragment and a review
    sheet, grouped by verdict, most frequent first."""
    dis = json.load(open(args.disagreements, encoding="utf-8"))
    lines = [dict(kind="reading", **r) for r in dis["reading"]] + [dict(kind="segment", **r) for r in dis["segment"]]
    props = {}
    for path in args.proposals:
        for o in json.load(open(path, encoding="utf-8")):
            props[o["line"]] = o
    groups = collections.defaultdict(list)
    for k, ln in enumerate(lines):
        o = props.get(k)
        groups[o["verdict"] if o else "unjudged"].append((ln, o))
    for g in groups.values():
        g.sort(key=lambda t: -t[0]["n"])
    order = ["agent", "context", "owner", "builder", "unjudged"]
    titles = {"agent": "The agents are right everywhere: a plain rule",
              "context": "Both occur: a context rule, or the builder already has the common case",
              "owner": "The owner's call (a proper noun or taste)",
              "builder": "The builder is right in these contexts (no rule)",
              "unjudged": "No verdict returned"}
    with open(args.sheet, "w", encoding="utf-8") as f:
        f.write("# Readings audit, the proposals\n\n")
        f.write("| verdict | lines | occurrences | with a rule |\n|---|---:|---:|---:|\n")
        for v in order:
            g = groups.get(v, [])
            if g:
                f.write("| %s | %d | %d | %d |\n" % (v, len(g), sum(ln["n"] for ln, _ in g),
                                                   sum(1 for _, o in g if o and o.get("rule"))))
        for v in order:
            g = groups.get(v, [])
            if not g:
                continue
            f.write("\n## %s (%d)\n\n" % (titles[v], len(g)))
            for ln, o in g:
                f.write("- **%s** x%d, builder %s, agent %s" % (ln["surface"], ln["n"], ln["builder"], ln["agent"]))
                if o:
                    if o.get("rule"):
                        r = o["rule"]
                        extra = "".join(", %s %s" % (k, r[k]) for k in ("next", "prev") if r.get(k))
                        f.write("  \n  rule: `%s` = %s%s" % (r.get("surface"), r.get("reading"), extra))
                    if o.get("note"):
                        f.write("  \n  %s" % o["note"])
                for label, c in ln["contexts"][:2]:
                    f.write("  \n  `%s` %s" % (label, c))
                f.write("\n")
    rules, seen = [], set()
    for v in ("agent", "context", "owner"):
        for ln, o in groups.get(v, []):
            r = (o or {}).get("rule")
            if not r or not r.get("surface") or not r.get("reading"):
                continue
            key = (r["surface"], r["reading"], tuple(r.get("next") or ()), tuple(r.get("prev") or ()))
            if key in seen:
                continue
            seen.add(key)
            rules.append((v, ln, o, r))
    # The reach measurement (cmd reach --json), keyed by the rule's order in the
    # fragment this same report wrote before: cmd reach numbers the blocks the same way.
    reach = {}
    if args.reach:
        for k, m in json.load(open(args.reach, encoding="utf-8")).items():
            reach[int(k)] = m

    def verdict_of(k, ln):
        m = reach.get(k)
        if m is None:
            return "", ""
        if "error" in m:
            return "ERROR", m["error"][-200:]
        c = m["changed"]
        if c == 0:
            return "NO EFFECT", "matches no token: the builder reads the compound in pieces (a multi-token rule is wanted)"
        if c > 2 * ln["n"] + 2:
            return "OVERREACH", "changes %d placements for %d lines: %s" % (
                c, ln["n"], "; ".join("%s x%d" % (t, n) for t, n in m["top"][:3]))
        return "ok", "changes %d placements for %d lines" % (c, ln["n"])

    with open(args.toml, "w", encoding="utf-8") as f:
        f.write("# Candidate rules from the readings audit (tools/ruby_audit.py report), for the owner\n"
                "# to approve into data/furigana/readings.toml. Grouped by verdict; each note is the\n"
                "# proposing agent's. Nothing here is game text: surfaces, readings and reasons.\n")
        for v in ("agent", "context", "owner"):
            rs = [x for x in rules if x[0] == v]
            if not rs:
                continue
            f.write("\n# -- %s --\n" % titles[v])
            for kk, (_, ln, o, r) in ((kk, x) for kk, x in enumerate(rules) if x[0] == v):
                flag, why = verdict_of(kk, ln)
                if flag and flag != "ok":
                    f.write("\n# %s: %s" % (flag, why))
                elif flag:
                    f.write("\n# reach: %s" % why)
                f.write("\n[[reading]]\nsurface = %s\nreading = %s\n" % (json.dumps(r["surface"], ensure_ascii=False),
                                                                        json.dumps(r["reading"], ensure_ascii=False)))
                for k in ("next", "prev"):
                    if r.get(k):
                        f.write("%s = %s\n" % (k, json.dumps(list(r[k]), ensure_ascii=False)))
                note = (o.get("note") or "").replace("\n", " ")
                f.write("note = %s\n" % json.dumps("%s (x%d; builder %s, agent %s)" % (note, ln["n"], ln["builder"], ln["agent"]),
                                                   ensure_ascii=False))
    if reach:
        flags = collections.Counter(verdict_of(kk, x[1])[0] for kk, x in enumerate(rules))
        with open(args.sheet, "a", encoding="utf-8") as f:
            f.write("\n## Reach: each rule built alone over the base sidecar\n\n")
            f.write("| | rules |\n|---|---:|\n")
            for fl in ("ok", "OVERREACH", "NO EFFECT", "ERROR"):
                if flags.get(fl):
                    f.write("| %s | %d |\n" % (fl, flags[fl]))
            f.write("\n")
            for kk, (v, ln, o, r) in enumerate(rules):
                fl, why = verdict_of(kk, ln)
                if fl in ("OVERREACH", "NO EFFECT", "ERROR"):
                    f.write("- **%s** = %s (%s): %s %s\n" % (r["surface"], r["reading"], v, fl, why))
        print("reach: " + ", ".join("%s %d" % kv for kv in flags.most_common()))
    print("lines %d, judged %d; rules %d -> %s; sheet %s" % (len(lines), len(props), len(rules), args.toml, args.sheet))
    for v in order:
        if groups.get(v):
            print("  %-9s %4d lines %5d occurrences" % (v, len(groups[v]), sum(ln["n"] for ln, _ in groups[v])))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("shard", help="the review list as blind text shards for agents")
    s.add_argument("review")
    s.add_argument("outdir")
    s.add_argument("--size", type=int, default=300)
    s.set_defaults(fn=cmd_shard)
    d = sub.add_parser("diff", help="the agents' kana against the builder's")
    d.add_argument("review")
    d.add_argument("kanadir")
    d.add_argument("out")
    d.add_argument("--json")
    d.add_argument("--size", type=int, default=300)
    d.add_argument("--contexts", type=int, default=4, help="contexts listed per disagreement")
    d.set_defaults(fn=cmd_diff)
    r = sub.add_parser("report", help="the proposal pass merged into a candidate sidecar fragment and a sheet")
    r.add_argument("disagreements", help="the diff's --json")
    r.add_argument("toml", help="the candidate readings.toml fragment written")
    r.add_argument("sheet", help="the review sheet (markdown, game text: harness root only)")
    r.add_argument("proposals", nargs="+", help="the agents' proposals_out_*.json")
    r.add_argument("--reach", help="the reach measurement (cmd reach --json): rules that change far more than "
                                   "their lines are set apart as overreaching")
    r.set_defaults(fn=cmd_report)
    m = sub.add_parser("reach", help="each candidate rule built alone: how many placements it changes")
    m.add_argument("base", help="the sidecar in the tree (data/furigana/readings.toml)")
    m.add_argument("candidates", help="the candidate fragment (cmd report's toml)")
    m.add_argument("review", help="the baseline review (the build with the base sidecar alone)")
    m.add_argument("--game", required=True)
    m.add_argument("--lang", default="ja-JP")
    m.add_argument("--outdir", required=True, help="scratch builds, one per rule (harness root)")
    m.add_argument("--json", required=True)
    m.add_argument("--jobs", type=int, default=4)
    m.set_defaults(fn=cmd_reach)
    args = ap.parse_args(argv)
    args.fn(args)


if __name__ == "__main__":
    main(sys.argv[1:])
