# Round thirteen: the area overlays' remainder, which is effect-kind code

**Status:** DRAFT (2026-09-29, measured at `61be26e`, the tip of round
twelve's wave one; branch `phase-3/round13-prep`) - a plan and a draft cut,
not scheduled. Nothing taken, nothing named, no C++ changed; one tool
extended (`tools/band_rows.py`, [`band-rows.md`](band-rows.md) section 6).
The cut is a **draft**: it must be regenerated at round twelve's tip once
wave two (FC1, FC2, FC3, FE1, FE2, FO, FS) has merged (section 6). Listed as
[`IDEAS.md`](IDEAS.md) I28; sketched in
[`takeover-queue-field-battle.md`](takeover-queue-field-battle.md) section 9.

## 0. The question, and the answer in one paragraph

The sketch (field-battle section 9) says round thirteen is "the area code
the walk did not reach", 629 functions, to be taken with `tools/area_rows.py`
"run the other way" on `area_harness` unchanged. The count holds: **629 at
`61be26e`, world by world the sketch's 171 / 112 / 199 / 47 / 100**. The
description does not. **627 of the 629 lie outside the area band** that
`area_rows.py` reads (`0x401000..0x42D710`), no area reaches them (the tool
says 2), and what does reach them is the effect-object engine: most are the
state handlers of an effect kind (`Effect_KindHandlers` `0x655350` ->
a kind's dispatcher -> `jmp [Sprite_Current[+1] * 4 + T]`), a sub-state of
kind 0x18 (`EffectKind18_States` `0x65406C` -> `jmp [Sprite_Current[+2] *
4 + T]`), or a callee of one. The world label is the PSX twin's: every AREA
overlay on the disc linked the common code it used, so a twin in `WORLD01`
names a copy, not an owner - which [`takeover-queue-areas.md`](takeover-queue-areas.md)
section 1.4 already said on 2026-09-26. The other two are jump-table cases
of functions that are ours. So the round is **the effect kinds' state code**:
thirteen groups of 14 to 63 by kind and state table in address order, on
`band_rows.py` (which reads any cut; `area_rows.py` cannot), behind a
stage-A harness group, because no harness makes `Sprite_Current` an effect
record (section 5).

## 1. How it was measured

All at `61be26e` in a worktree, reading the main checkout's `analysis/` and
`bof3/BOF3.exe`, writing only to the session scratchpad
(`.../scratchpad/round13/`; game-derived, never committed).

1. **The catalog**, against this checkout's `symbols.toml`:

   ```
   python tools/remaining_catalog.py --symbols symbols.toml \
     --funcs <main>/analysis/pc_funcs.json --hidden <main>/analysis/pc_hidden.json \
     --pairs <main>/analysis/pairs_propagated.json --area-pairs <main>/analysis/area_pairs.json \
     --xref <main>/analysis/pc_xref.json --runs <main>/analysis/calltrace \
     --sibling <sibling>/ --out <scratch>/catalog_61be26e.md --tsv <scratch>/catalog_61be26e.tsv
   ```

   4,140 not ours of 10,246 starts. The draft is every row whose `part` is
   `5 Area overlays`.
2. **The area tool**, pointed at a scratch copy of its inputs with the
   regenerated catalog (it writes `area_rows.tsv` / `area_funcs.tsv` into
   `--analysis`): `python tools/area_rows.py --exe ... --analysis <scratch
   copy> --sibling ... --quiet`. Its figures are in section 4.
3. **The band tool** over the draft cut, `--byte-tables` on (section 4):
   `python tools/band_rows.py --exe ... --analysis <main>/analysis --cut
   <scratch>/round13_cut_draft.tsv --byte-tables --groups | --edges |
   --group G [--clones --harness area]`.
4. **Units** (scratch `units.py`, importing `band_rows.Band`): each row's
   `.data` cells -> the run of code pointers holding it -> every instruction
   whose displacement is the run's start (or 4 below) -> that dispatcher,
   and the `Effect_KindHandlers` entry that holds the dispatcher (its kind).
   A row reached only by calls takes the unit of its caller.
5. **Wave two's reach** (scratch): the draft's rows against round twelve's
   cut `analysis/round12_cut.tsv` - by the section-8 bands of
   [`takeover-queue-field-battle.md`](takeover-queue-field-battle.md), and by
   `band_rows.py --edges` over a cut of the draft plus the 323 field rows.
6. **Spawners** (scratch `spawners3.py`): every `call Effect_Spawn`
   `0x57CE10` in the area band, the kind its last `push imm8` gives.
7. **Hand reads**, capstone: the dispatchers `0x466080`, `0x487C10`,
   `0x504F70`, `0x521A00`, `0x46C9F0`; the switches of `0x420060`,
   `0x422510`, `0x512490`, `0x578A00`.

## 2. The draft: 629, as the sketch counted, and what they are

| | Rows | Hidden | Bytes (catalog) | pair / neighbour / host |
|---|--:|--:|--:|---|
| World 0 | 171 | 120 | 32,706 | 131 / 35 / 5 |
| World 1 | 112 | 79 | 15,268 | 23 / 41 / 48 |
| World 2 | 199 | 141 | 33,556 | 45 / 119 / 35 |
| World 3 | 47 | 29 | 10,691 | 12 / 27 / 8 |
| World 4 | 100 | 74 | 19,774 | 37 / 45 / 18 |
| **All** | **629** | **443** | **111,995 (109.4 KiB)** | **248 / 267 / 114** |

Against the sketch (counted at `c4b0d32`): the same 629, the same split by
world, the same 443 hidden and 109 KiB, the same 267 neighbour and 114
host. Nothing between `c4b0d32` and `61be26e` took an area-labelled row:
round twelve's wave one took catalog part 4 only (part 4 has no row left
at `61be26e`; part 3, the field side, still has its 323).

**Where they lie** (address runs, a gap over 0x400 starting a new one):
2 rows in the area band (`0x4201F0`, `0x422530`); 93 at `0x464BA0..0x469BB0`;
2 at `0x46C820`, `0x46EA80`; 183 at `0x472440..0x47F2B0`; 142 at
`0x4811F0..0x48E0A0`; 131 at `0x502060..0x5144E1`; 63 at
`0x528CD0..0x52D07C`; 14 at `0x594060..0x594D8A`. Not one is in
`0x401000..0x42D710` but the two cases.

**What reaches them** (`band_rows.py`'s reach, 629 rows plus the 6 code no
list has, 635): 440 by `.data` cells only, 176 by calls only, 7 by calls
and jumps, 5 by a cell and a call, 4 by jumps only, 2 by a `.text` cell
only (the two cases), 1 as a case of its host's switch (`0x5124C0`). **Every
one of the 114 "host" rows is reached by a `.data` cell** - a table entry,
an entry by address, not a fall-through - so the host label says where it
sits, not that it is part of its host.

**The tables.** The cells lie in runs of code pointers at
`0x653AFC..0x655170` and `0x65E0B4..0x65FF08`, all unnamed in
`symbols.toml`; each run is indexed by one dispatcher of four
instructions, read by hand for five:

- `0x487C10` (`Effect_KindHandlers` entry 0x82): `mov ecx, [Sprite_Current];
  xor eax, eax; mov al, [ecx + 1]; jmp [eax*4 + 0x654C88]` - no bound;
- `0x466080` (entry 0xF): the same through `0x653C28`, after a test of
  `0x7E1360` (`Effect_Objects` record 3) that sends it to `0x589840`;
- `0x504F70` (`EffectKind18_States` entry 67): `mov al, [ecx + 2]; jmp
  [eax*4 + 0x65E2B8]` - kind 0x18's sub-state byte;
- `0x521A00`, `0x46C9F0`: the same `+2` shape through `0x65FDFC` and
  `0x654028`.

`Effect_RunObjects` (`symbols.toml`, evidence of 2026-09-22) sets
`Sprite_Current` to each of the 20 `Effect_Objects` records of 0x80 at
`0x7E11E0` and calls `[Effect_KindHandlers + byte +5 * 4]`. So these
functions run with `Sprite_Current` an **effect record**, not a field
object or a party member.

**The kinds** the draft's tables serve (units, the dispatcher's entry in
`Effect_KindHandlers`): 0x3, 0xA, 0xF, 0x17, 0x2E, 0x35, 0x46, 0x48, 0x50,
0x57, 0x61, 0x7F, 0x81, 0x82, 0x85, 0x87, 0x8A, 0x8B, 0x95, 0x9A; and
twenty-three `EffectKind18_States` entries (16, 20, 23, 24, 29, 33, 34,
35, 36, 40, 63, 65, 67, 69, 81, 82, 83, 85, 88, 89, 90, 91, 92). The 84
distinct dispatchers found are **none of them ours**: 23 are catalog rows
`Table EffectKind18_States`, 20 `Table Effect_KindHandlers`, 18
`Unlabelled`, 4 `Table Field_ActionBySet`, 18 are draft rows themselves,
and one is FC2's `0x46C9F0` (round twelve's cut).

**Which areas show them is not measured.** The area band's 76 calls to
`Effect_Spawn` all push the kind as an immediate; among the draft's kinds
only kind 0x3 is one, from areas 49, 67, 68, 77, 94, 117, 119, 130, 133,
134, 136 - worlds 1, 2 and 3, while the catalog labels kind 0x3's code
"world 0". The other kinds are spawned by code outside the band or by
script data. So "by world and area", round ten's grouping, has nothing to
stand on here; the groups are by kind and table in address order.

## 3. The groups

Address bands cut at unit boundaries (a unit is a state table and the rows
it names, or the rows a unit's rows call), about fifty rows a group,
thirteen groups and the two non-functions. **A group owns the functions the
regenerated cut table lists for it**, as in round twelve; the bands are how
the table is made (section 6). Figures from `band_rows.py --byte-tables
--groups` over the draft; "In ours" is a hidden row whose recorded host is
ours; "Risk" the rows section 7 lists; "Suspects" section 4.

| Group | Band | Fns | Hidden | In ours | Bytes (cut / read) | + not listed | Worlds (label) | pair / nb / host | What (kinds; kind-0x18 sub-states `s`) | Risk | Suspects |
|---|---|--:|--:|--:|--:|--:|---|---|---|--:|--:|
| EK1 | `0x464BA0..0x467810` | 60 | 49 | 0 | 11,051 / 10,706 | 0 | w0 60 | 53 / 2 / 5 | kinds 0x3, 0xA, 0xF | 60 | 0 |
| EK2 | `0x467810..0x46EB96` | 35 | 16 | 0 | 9,599 / 9,466 | 0 | w0 34, w1 1 | 33 / 2 / 0 | kinds 0xF, 0x17 | 35 | 0 |
| EK3 | `0x472440..0x475050` | 63 | 33 | 0 | 10,748 / 10,480 | 0 | w1 63 | 18 / 41 / 4 | kinds 0x2E, 0x35 | 0 | 0 |
| EK4 | `0x475050..0x477BA0` | 56 | 25 | 0 | 10,611 / 10,148 | 1 | w1 3, w2 25, w3 28 | 14 / 33 / 9 | kind 0x35; tables `0x654480`, `0x6544AC`, `0x654504` | 0 | 0 |
| EK5 | `0x477BC0..0x478F10` | 46 | 43 | 0 | 4,208 / 3,785 | 0 | w1 38, w2 8 | 3 / 7 / 36 | kinds 0x46, 0x48 | 0 | 1 |
| EK6 | `0x478F10..0x487C10` | 54 | 28 | 0 | 10,420 / 10,234 | 0 | w1 7, w2 43, w3 1, w4 3 | 11 / 22 / 21 | kinds 0x50, 0x57, 0x61, 0x7F, 0x81 | 0 | 2 |
| EK7 | `0x487C30..0x4891E7` | 53 | 52 | 0 | 5,350 / 4,799 | 2 | w2 48, w3 5 | 2 / 51 / 0 | kinds 0x82, 0x85, 0x87 | 0 | 0 |
| EK8 | `0x4891F0..0x48E0A0` | 52 | 38 | 0 | 7,327 / 6,420 | 1 | w2 1, w3 2, w4 49 | 13 / 28 / 11 | kinds 0x87, 0x8A, 0x8B, 0x95, 0x9A | 0 | 0 |
| ES1 | `0x502060..0x506290` | 41 | 29 | 0 | 8,907 / 8,358 | 1 | w2 33, w3 8 | 16 / 17 / 8 | s16, s20, s23, s24, s29, s33, s67, s88 | 1 | 0 |
| ES2 | `0x5064C0..0x511BAE` | 47 | 39 | 0 | 11,606 / 11,321 | 0 | w2 41, w3 1, w4 5 | 16 / 16 / 15 | s34, s35, s36, s40, s63, s65, s82 | 1 | 2 |
| ES3 | `0x511BB0..0x5144E1` | 43 | 33 | 0 | 8,640 / 8,152 | 1 | w4 43 | 21 / 17 / 5 | s69, s81, s83, s85, s89..s92 | 0 | 2 |
| EKF | `0x528CD0..0x52D07C` | 63 | 52 | 0 | 9,134 / 8,684 | 0 | w0 63 | 32 / 31 / 0 | 52 reached by one-slot tables `0x65FDF8..0x65FF08` (dispatchers `0x521A00..0x522B60`, four of them `Field_ActionBySet` entries); the rest helpers kinds 0x3 / 0xF call | 21 | 1 |
| EKP | `0x594060..0x594D8A` | 14 | 4 | 4 | 3,274 / 3,254 | 0 | w0 14 | 14 / 0 / 0 | a run of code pointers at `0x66A470` (after `SpriteCell_Sizes`) and callees FE2's `0x593960..` calls | 8 | 0 |
| NOTFN | `0x4201F0`, `0x422530` | 2 | 2 | 2 | 1,120 / 98 | 0 | w3 2 | 2 / 0 / 0 | jump-table cases of ours (section 4): **not to take** | 0 | 2 |
| **All** | | **629** | **443** | **6** | **111,995** | **6** | | **248 / 267 / 114** | | **126** | **10** |

Group sizes are 14 to 63; EK1 / EK2 split kind 0xF (58 rows in one table)
where a call-only row changes unit, and EKP is small because it is the
one run that sits next to FE2 alone. EKP's 14 could join EK2 (they call
each other, section 5) at the cost of a band that is not contiguous.

**Names.** `EK`, `ES`, `EKF`, `EKP`, `EKH` and `NOTFN` appear nowhere in
`symbols.toml`, `src/game/`, `tools/*.py`, `docs/` or the main checkout's
`analysis/round*` files (`grep -E '\bE[KS][0-9A-Z]*\b'`, 2026-09-29). Earlier
rounds' names: `CA..CM`, `DA..DI`, `EA`, `SH`, `L`, `E`, `C1..C3`,
`S01..S38`, `SE`, `SC*`, `CALLS`, `ARH`, `ART`, `AR0A..AR4F`, `BH`,
`BSA..BSJ`, `EH`, `FH`, `RT`, `BE1..BE7`, `FC1..FC3`, `FE1`, `FE2`, `FO`,
`FS`; `boss_rows.py` names its units `K<n>`, which `EK<n>` does not match
as a word. Modules proposed: `effect_k1..effect_k8`, `effect_s1..effect_s3`,
`effect_kf`, `effect_kp` (no `src/game/effect_*` exists).

## 4. The tool: `band_rows.py` serves, `area_rows.py` cannot

**`area_rows.py`**, run at `61be26e` (section 1 step 2): band
`0x401000..0x430000`, 1,457 starts, 1,414 ours; 1,566 functions after
discovery, 1,554 ours; worlds 0..4 "to take" 3 / 0 / 0 / 0 / 0; **catalogue
"Area overlays" outside the band: 627; reached by an area: 2** (`0x486D60`
from ours `Area170_Init`, `0x511C10` from ours `Area189_LeaderStart` /
`Area189_StepBegin`). Its walk drops the two in-band rows as "inside
another". It is built on the descriptors' closures and the band constant
`BAND_HI = 0x430000`; running it "the other way" means another tool, and
`band_rows.py` already is one.

**`band_rows.py`** takes any cut table (`--cut`) and reads each row where
it lies: extent, reach with the `.data` run's name, flags, clones, raw
references, edges. Over the draft it printed every row, 6 functions no list
has in the groups' spans (`0x476560` EK4, `0x487DE0` / `0x4883D0` EK7,
`0x489390` EK8, `0x503E50` ES1, `0x512510` ES3), and 635 clones. Two gaps,
closed on this branch ([`band-rows.md`](band-rows.md) section 6):

- **`--harness area`** for `area_harness`'s `AH_` clone form, in case the
  owner keeps the sketch's harness (section 5).
- **`--byte-tables`**, off by default: a case of a two-level switch lying
  past the next start was not seen as a case (the table read stopped at it).
  With it the draft's two in-band rows are flagged and absorbed as cases.
  **Without it every output over round twelve's cut is unchanged, line for
  line**; with it round twelve's cut gains one verdict, `0x578A40` (FO) a
  case of ours `MoveScript_Group9` (section 7).

**The suspects** - starts that may not be functions, flagged, not decided:

| Row | Group | Why |
|---|---|---|
| `0x4201F0` | NOTFN | case 9 of ours `Area141_Tail52` `0x420060` (`cmp eax, 0x33; ja; mov cl, [eax + 0x420328]; jmp [ecx*4 + 0x4202EC]`, the cell `0x420310`); only that cell names it. The catalog's host `0x41FE20` is the recorded function before it, not the switch's owner |
| `0x422530` | NOTFN | case 0 of ours `Area148_Tail31` `0x422510` (`cmp eax, 0x15; ja; mov cl, [eax + 0x422778]; jmp [ecx*4 + 0x422750]`, the jmp at `0x422529` right before it) |
| `0x5124C0` | ES3 | case 3 of `0x512490`'s own switch (`jmp [eax*4 + 0x5124F8]` at `0x51249F`), a neighbour row; the tool absorbs it into `0x512490`'s clone |
| `0x477DB0` | EK5 | reached only by `jmp` from five neighbour rows: a shared tail |
| `0x506860` | ES2 | reached only by `jmp` from four neighbour rows: a shared tail |
| `0x513410` | ES3 | three neighbours' `jmp` and one call: a shared tail or second entry |
| `0x487940` | EK6 | a `jmp` from `0x4878F0` and a call from `0x487910`: a shared tail or second entry |
| `0x47B3F0` | EK6 | a call from `0x47B350` and a `jmp` from its host: a second entry |
| `0x510EB0` | ES2 | reached only by a `jmp` from `0x510C80`, which no cut row is: a thunk's target (1,771 bytes, hidden in `0x510C90`) |
| `0x52CD50` | EKF | calls and a `jmp` from EK1 rows `0x4656C0`, `0x465840` and `0x52CDF0`: a second entry |

Plus the six hidden rows whose recorded host is ours: the two cases above,
and EKP's `0x594060`, `0x5940F0`, `0x594100`, `0x594240`, in
`Sprite_ClutWord` `0x593860`'s recorded extent - each named by its own
cell of the run at `0x66A470`, so entries by address, but the group must
read ours `Sprite_ClutWord` to its last instruction and say so. The
dispatcher that indexes `0x66A470` was not identified (the unit scan
attributed the naming instruction to `Sprite_ClutWord`'s span, which
holds only its own four-case switch at `0x593881`): open, section 8.

## 5. The harness, the merge order, the waves

**No harness makes `Sprite_Current` an effect record.** `area_harness`
draws it from the field objects and the party only (`ObjectOrMember`,
`src/game/area_harness.cpp`, in the per-call set-up and the disturbance's
case 0); `scenario_harness` has `Effect_Objects` as a region (`at::kEffects`,
20 x 0x80) but no shape that runs a state with it current. These functions
read the record's bytes (+1 the state, +2 the sub-state, and what the
kind keeps) through `Sprite_Current`. So the round wants a **stage-A group
EKH**, as rounds eleven and twelve had: one shape, "an effect state"
(`Sprite_Current` one of the 20 records, +0 in use, +5 the group's kind,
+1 / +2 drawn below the state table's own length, `Effect_Release` /
`Effect_FindFree` louder stand-ins), in whichever harness the owner picks
(open question 1). The sketch's "area_harness unchanged" would leave each
group to fake the record in its own `Seed` and fight the disturbance.

**Merge order, callee first**, from `band_rows.py --byte-tables --edges`
over the draft (209 edges): EK1 -> EK2 62, EK2 -> EK1 10, EK1 -> EKF 18,
EK2 -> EKF 58, EK2 -> EKP 8, EKF -> EK2 2, EKP -> EK2 9, EKP -> EKF 23,
EK3 -> EK4 1, EK7 -> EK8 3, ES2 -> ES3 15. World 0's four (EK1, EK2, EKF,
EKP) are one cluster; the rest are nearly independent. A clone re-aims
every call at a recorder, so edges order the rebinding, not the fuzz:
**EKF, EK2, EK1, EKP; EK4, EK3; EK8, EK7; ES3, ES2; then EK5, EK6, ES1 in
any order.**

**Waves.** EKH alone in stage A. Wave one: EKF, EK2, EK1, EKP, EK4, EK3,
EK5 (world 0's cluster, which calls into FE1 and FE2, first, so the
rebinding against wave two's names is done once). Wave two: EK8, EK7, EK6,
ES3, ES2, ES1. Each on round twelve's routine: a brief per group (the
template is in the session scratchpad, `round13_brief_template.md`), one
agent per group in a worktree reset onto the round's tip, headless
self-tests, one merge at a time.

**The live check.** None of the 629 is on a recording: the catalog's
reach columns are empty for all but four EKP rows, whose host the attract
cycle reached (`~63734`, entry not armed), and the first-call traces
`reach_dragon` and `reach_whelp` enter none. Fuzz-only, until the owner
records a walk through a place that shows these effects - which places
those are is the owner's to say, not this doc's (section 2's last
paragraph).

## 6. What must be regenerated at round twelve's tip

1. **The catalog**, with the tip's `symbols.toml` (section 1 step 1).
   Wave two takes part-3 rows only; no draft row's label rests on a
   part-3 anchor (the 267 neighbour rows' anchors and the 114 host rows'
   hosts are all area-labelled or ours: 0 in round twelve's cut, checked),
   so the count should stay 629 unless a wave-two group takes a draft row
   as "code in its band the cut does not list" - which the brief allows
   for unlisted code only, and no draft row is unlisted. Recount anyway.
2. **The cut**, by the bands of section 3 (scratch `regen_cut.py`, which
   reproduces the draft's groups exactly from the catalog):

   ```
   python - <<'EOF'
   import csv, collections
   rows = list(csv.DictReader(open('analysis/remaining_catalog.tsv', encoding='utf-8'), delimiter='\t'))
   fs = sorted((int(r['entry'], 16), r) for r in rows if r['part'] == '5 Area overlays')
   G = [('EK1', 0x460000, 0x467810), ('EK2', 0x467810, 0x472000), ('EK3', 0x472000, 0x475050),
        ('EK4', 0x475050, 0x477BC0), ('EK5', 0x477BC0, 0x478F10), ('EK6', 0x478F10, 0x487C30),
        ('EK7', 0x487C30, 0x4891F0), ('EK8', 0x4891F0, 0x4A0000), ('ES1', 0x500000, 0x5064C0),
        ('ES2', 0x5064C0, 0x511BB0), ('ES3', 0x511BB0, 0x516000), ('EKF', 0x528000, 0x52D100),
        ('EKP', 0x594000, 0x595000)]
   NOTFN = {0x4201F0, 0x422530}      # cases of ours Area141_Tail52 / Area148_Tail31
   grp = lambda a: 'NOTFN' if a in NOTFN else next((g for g, lo, hi in G if lo <= a < hi), 'LOOSE')
   w = csv.writer(open('analysis/round13_cut.tsv', 'w', newline='', encoding='utf-8'), delimiter='\t')
   w.writerow(['group', 'entry', 'size', 'label', 'hidden', 'host', 'name', 'how', 'note'])
   n = collections.Counter()
   for a, r in fs:
       n[grp(a)] += 1
       w.writerow([grp(a), f'0x{a:06X}', r['size'], r['label'], r['hidden'], r['host'], r['name'], r['how'], r['note']])
   print(len(fs), dict(n))
   EOF
   ```

   A `LOOSE` row is one no band holds: place it by hand.
3. **`band_rows.py --cut analysis/round13_cut.tsv --byte-tables --groups`,
   `--edges`**: the figures of sections 3 and 5, and the not-listed six.
4. **The wave-two rows of section 7**: once FC1..FS are ours, their
   functions are callees by name and `--edges` over a combined cut is empty;
   read the 36 rows with call edges again against the merged sources.
5. **The dispatchers** (open question 2), if they join: add the part-2
   `Table Effect_KindHandlers` / `Table EffectKind18_States` rows that
   dispatch a group's tables to that group.

## 7. The rows at risk from wave two

126 draft rows (scratch `wave2_risk.tsv` has the list with reasons):

- **97 lie inside a wave-two band** as the section-8 script draws it: EK1's
  60 and EK2's 33 world-0 rows in `0x461000..0x46BBF0` (FC1's band; FC1's
  own rows are `0x461800` and the `0x469D10..` run), EK2's `0x46C820` and
  `0x46EA80` in FC2's `0x46BBF0..0x470000`, and EKP's `0x594060` and
  `0x5940F0` in FE2's `0x593960..0x594100`. The cut, not the band, owns a
  function, so these move only if a wave-two group takes one.
- **36 have call edges with wave-two rows** (7 of them also in a band):
  27 with FE1 - EKF's rows and five of EK1's call FE1's message helpers
  `0x52D080`, `0x52D0C0`, `0x52D140`, `0x52D320`, `0x52D560`, `0x52D5C0`,
  `0x52D610`, `0x52D750`, `0x52D8C0`, and FE1 calls EKF's `0x52CE60`,
  `0x52CED0`, `0x52CF60`, `0x52CFE0` and EK2's `0x468950`; 8 with FE2 -
  FE2's `0x593960..0x593E60` call EKP's `0x5942C0`, `0x594410`,
  `0x594700`, `0x594790`, `0x5947D0`, `0x594AD0` and EK2's `0x469750`, and
  ES1's `0x505690` calls FE2's `0x534C20`; 1 with FC2 - ES2's `0x50A510`
  sits in the table `0x654028` that FC2's `0x46C9F0` dispatches.

**For wave two, now** (the coordinator's, not round thirteen's): with
`--byte-tables`, FO's cut row `0x578A40` is entry 0 of ours
`MoveScript_Group9` `0x578A00`'s four-entry table at `0x578AD8`, not a
function - a ninth such start in round twelve's cut
([`band-rows.md`](band-rows.md) section 6).

## 8. Open questions for the owner

1. **Is round thirteen the effect kinds' round?** The rows are effect
   state code (section 2), not area code. Rename it so, and choose the
   harness: widen `area_harness` or `scenario_harness` with an
   effect-state shape in a stage-A group EKH (this doc's proposal), or
   keep the sketch's `area_harness` unchanged and let each group fake the
   record.
2. **Take the dispatchers with their states?** The 84 dispatchers of the
   draft's tables are all Capcom's: 43 of them catalog part-2 rows (`Table
   Effect_KindHandlers` 20, `Table EffectKind18_States` 23), 18
   unlabelled, 4 `Table Field_ActionBySet`. A kind taken without its
   dispatcher is fuzzed through clones only. Adding the 43 gives whole
   kinds (about 672 rows); the whole part-2 families are 163 and 141 rows.
3. **Which places show which kind.** Only kind 0x3's spawns are in area
   code (11 areas, worlds 1..3); a live walk per kind needs the owner to
   say where the game shows it, or a recording that happens to.
4. **`--byte-tables` as the default** once wave two has merged (it changes
   one verdict in round twelve's cut, FO's `0x578A40`).
5. **EKP**: 14 rows beside FE2's sprite-draw prompt states; keep it a
   group of its own, or fold it into EK2, its caller and callee.

**Not verified**: which instruction indexes the run at `0x66A470` (EKP's
cells); the shared-tail suspects beyond the reach the tool prints (no
hand read); the `Effect_Spawn` scan is a linear sweep with restarts, and a
kind stored by other means (script data, a computed byte) is not seen;
the draft has not been regenerated at round twelve's tip, which does not
exist yet.
