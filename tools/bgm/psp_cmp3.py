"""P10 third pass: the subs whose notes or programs differ, mapped to songs; loop markers."""
import json, collections
import psp_notes as P
from discs_compare import IMAGES

d = json.load(open(P.PC + "/analysis/bgm/psp_notes2.json"))
inv = json.load(open(P.PC + "/analysis/bgm/inventory.json"))
song = collections.defaultdict(list)
for t in inv["table"]:
    song[(t["file"][4:], t["sub"])].append(t["song"])
bad = [(r["file"], r["sub"]) for r in d["psp-jp"]["rows"] if not r["notes_same"]]
files = set(f for f, _ in bad) | {"BGM/BGM000.EMI"}
jp = P.get(IMAGES["psx-jp"], files); pp = P.get(IMAGES["psp-jp"], files)
out = []
for f, s in bad:
    x = P.psx_subs(P.sections(jp[f])[10][0])[s]; y = P.psp_subs(P.sections(pp[f])[10][0])[s]
    r = y["res"] // x["res"]
    def notes(ev, sc):
        return collections.Counter((e[0] * sc, e[1], e[2], e[3]) + ((e[4],) if e[1] == 0x90 else ()) for e in ev if e[1] in (0x80, 0x90))
    a, b = notes(x["ev"], r), notes(y["ev"], 1)
    dx, dy = a - b, b - a
    on_x = sum(v for k, v in dx.items() if k[1] == 0x90); on_y = sum(v for k, v in dy.items() if k[1] == 0x90)
    row = dict(file=f, sub=s, songs=song.get((f, s), []), psx_only=sum(dx.values()), psp_only=sum(dy.values()),
               note_on_psx_only=on_x, note_on_psp_only=on_y,
               sample_psx=[str(k) for k in sorted(dx)[:3]], sample_psp=[str(k) for k in sorted(dy)[:3]])
    out.append(row)
    print(row)
x = P.psx_subs(P.sections(jp["BGM/BGM000.EMI"])[10][0])[0]; y = P.psp_subs(P.sections(pp["BGM/BGM000.EMI"])[10][0])[0]
print("psx cc99/6", [e for e in x["ev"] if e[1] == 0xB0 and e[3] in (99, 6)][:6])
print("psp cc99/6", [e for e in y["ev"] if e[1] == 0xB0 and e[3] in (99, 6)][:6])
print("psp metas", [e for e in y["ev"] if e[1] == "meta"][:12])
json.dump(out, open(P.PC + "/analysis/bgm/psp_notes3.json", "w"), indent=1)
