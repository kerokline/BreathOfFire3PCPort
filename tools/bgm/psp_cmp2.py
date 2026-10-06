"""P10 second pass: PSX SEP vs PSP SMF with ticks scaled by the resolution ratio."""
import collections, json, sys
import psp_notes as P
from discs_compare import IMAGES

inv = json.load(open(P.PC + "/analysis/bgm/inventory.json"))
wanted = set(f[4:] for f in inv["disc"])
jp = P.get(IMAGES["psx-jp"], wanted)
out = {}
for tag in ("psp-jp", "psp-eu"):
    pp = P.get(IMAGES[tag], wanted)
    C = collections.Counter(); rows = []
    extra_kinds = collections.Counter(); psx_ctx = collections.Counter()
    for k in sorted(wanted):
        sa = P.psx_subs(P.sections(jp[k])[10][0]); sb = P.psp_subs(P.sections(pp[k])[10][0])
        for i, (x, y) in enumerate(zip(sa, sb)):
            r = y["res"] // x["res"] if y["res"] % x["res"] == 0 else None
            C["res x%s" % r] += 1
            def norm(ev, scale):
                return [(e[0] * scale,) + tuple(e[1:]) for e in ev]
            ex = norm(x["ev"], r); ey = y["ev"]
            notes = lambda ev: sorted((e[0], e[1], e[2], e[3]) + ((e[4],) if e[1] == 0x90 else ()) for e in ev if e[1] in (0x80, 0x90))
            same_notes = notes(ex) == notes(ey)
            C["notes same" if same_notes else "notes differ"] += 1
            prog = lambda ev: sorted(e for e in ev if e[1] == 0xC0)
            C["program changes same" if prog(ex) == prog(ey) else "program changes differ"] += 1
            cc = lambda ev: collections.Counter((e[0], e[2], e[3], e[4]) for e in ev if e[1] == 0xB0)
            dx, dy = cc(ex) - cc(ey), cc(ey) - cc(ex)
            C["cc same" if not dx and not dy else "cc differ"] += 1
            for (t, ch, a, b), n in dx.items(): extra_kinds[("psx-only cc", a)] += n
            for (t, ch, a, b), n in dy.items(): extra_kinds[("psp-only cc", a)] += n
            bend = lambda ev: collections.Counter((e[0], e[2], e[3], e[4]) for e in ev if e[1] == 0xE0)
            bx, by = bend(ex), bend(ey)
            C["bend same" if bx == by else "bend differ"] += 1
            extra_kinds["psp-only bend"] += sum((by - bx).values()); extra_kinds["psx-only bend"] += sum((bx - by).values())
            tempo = lambda ev: [(e[0], e[3]) for e in ev if e[1] == "meta" and e[2] == 0x51]
            tx, ty = [(t, v) for t, v in tempo(ex)], tempo(ey)
            C["tempo changes same" if tx == ty[1:] or tx == ty else "tempo differ"] += 1
            rows.append(dict(file=k, sub=i, res=(x["res"], y["res"]), notes_same=same_notes, programs_same=prog(ex) == prog(ey), programs_psx=sorted(set((e[2], e[3]) for e in ex if e[1] == 0xC0)), programs_psp=sorted(set((e[2], e[3]) for e in ey if e[1] == 0xC0)),
                             bends_psx=sum(bx.values()), bends_psp=sum(by.values()),
                             cc_only_psx=sum(dx.values()), cc_only_psp=sum(dy.values())))
    out[tag] = dict(counts=dict(C), extra={str(k): v for k, v in extra_kinds.items()}, rows=rows)
    print(tag, dict(C)); print("  ", extra_kinds.most_common(12))
json.dump(out, open(P.PC + "/analysis/bgm/psp_notes2.json", "w"), indent=1)
