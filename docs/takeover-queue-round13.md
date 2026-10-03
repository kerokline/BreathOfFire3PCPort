# Round thirteen: the area overlays' remainder, which is effect-kind code

**Status:** MEASURED (2026-09-29; the draft measured at `61be26e`, the cut
regenerated at the round's base `d1b411c`, section 10, which replaces the
groups of sections 3 and 9) - a plan and a cut;
the scope is decided (section 9), the start waits for round twelve's tip. Nothing taken, nothing named, no C++ changed; one tool
extended (`tools/band_rows.py`, [`band-rows.md`](band-rows.md) section 6).
The cut is a **draft**: it must be regenerated at round twelve's tip once
wave two (FC1, FC2, FC3, FE1, FE2, FO, FS) has merged (section 6). Listed as
[`IDEAS.md`](IDEAS.md) I28; sketched in
[`takeover-queue-field-battle.md`](takeover-queue-field-battle.md) section 9.
**Read sections 9 and 10 first**: it joins this draft to the labelling pass
([`labelling-pass.md`](labelling-pass.md)), settles the harness from the
code, and replaces section 5's waves and section 8's questions 1 and 2.

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

## 9. Joined to the labelling pass: one effect-engine round

Written 2026-09-29 by the coordinator on `phase-3/round13-plan` (this
branch merged with `phase-3/labelling-pass`), from the two tables in the
session scratchpad (`round13/round13_cut_draft.tsv`,
`labelling/out/labels.tsv`; the join is `merged/effect_engine_draft.tsv`).
Still a draft, still to be regenerated at round twelve's tip.

**The two sets are one engine and do not overlap.** The labelling pass
read catalog parts 2 and 7 (2,234 starts) and proposes `effect objects`
for 1,071 of them, 1 a switch case; this draft is part 5's 629, 2 of them
cases. No entry is in both. Together: **1,697 takeable starts, 254,509
bytes** (this draft's read sizes, the labelling table's catalog sizes), in
the same address runs, interleaved:

| Addresses | This draft | Labelling, `evidence` | Labelling, `hypothesis` | Together |
|---|--:|--:|--:|--:|
| `0x433640` | 0 | 0 | 1 | 1 |
| `0x462B00..0x46FFB0` | 95 | 86 | 15 | 196 |
| `0x470300..0x477E30` | 127 | 51 | 12 | 190 |
| `0x4780D0..0x47FD80` | 55 | 122 | 12 | 189 |
| `0x4801F0..0x487FE0` | 47 | 151 | 12 | 210 |
| `0x488020..0x48FF00` | 95 | 125 | 20 | 240 |
| `0x490090..0x4941E0` | 0 | 51 | 14 | 65 |
| `0x4FD2E0..0x503FA0` | 10 | 150 | 3 | 163 |
| `0x5043B0..0x50BFF0` | 69 | 94 | 7 | 170 |
| `0x50C0D0..0x516A90` | 52 | 139 | 5 | 196 |
| `0x528CD0..0x52CFE0` (EKF) | 63 | 0 | 0 | 63 |
| `0x594060..0x594D50` (EKP) | 14 | 0 | 0 | 14 |
| **All** | **627** | **969** | **101** | **1,697** |

So the catalog's part (5 "area", 2 "boot table", 7 "unlabelled") says how
a start got its label, not what it belongs to, and a round cut by part
splits a kind from its dispatcher. **Open question 2 is answered by the
join**: the dispatchers are in the labelling set, and a group takes its
kinds whole - the entry in `Effect_KindHandlers`, the state table, the
states and their callees.

**The harness: `scenario_harness`, with one shape added.** Both reports
were half right, read against the code at `61be26e` and wave two's
branches:

- `scenario_harness` holds `Effect_Objects` as a region (`at::kEffects`,
  20 x 0x80) but `Sprite_Current` is only ever one of the first four
  `Sprite_Objects` records: `Fix` and the disturbance's case 4 both set it
  through `SpriteRecord(k % 4)`, and `kSprite`'s span draw writes `+1..+4`
  of whatever it points at (`src/game/scenario_harness.cpp`). So no shape
  runs a state on an effect record - section 5's finding stands.
- Wave two's groups already take effect kinds on it all the same, each by
  its own seed: FC1 (`field_c1_fuzz.cpp`: `Sprite_Current = Eff(...)` half
  the time, `sprite_span = 20`, a checked `Effect_Objects` index in ours)
  and FC2 (`field_c2.cpp`: kinds 0x30, 0x34, 0x3A, 0x41, a checked
  `EffectRecord`). Read on their branches at `ad206f0` and `bf1e08d`, not
  merged yet.

So stage A (EKH) is a fold-back of what FC1 and FC2 wrote, as round
eleven's cleanup folded its groups' stand-ins: a shape `kEffect`
(`Sprite_Current` one of the 20 records, `+5` the group's kind, `+1` /
`+2` below the state table's length, the disturbance moving it among
effect records and not onto a sprite). It needs wave two merged first -
its model is their code - and it touches one file no wave-two group owns
after the merge. `area_harness` is not used.

**Waves**, about 350 each, by address so that a kind stays whole; the
group cut inside each (about fifty a group, at kind boundaries, callee
first) is made at round twelve's tip with `band_rows.py --byte-tables`
over the joined table:

| Wave | Addresses | Starts | This draft's groups inside |
|---|---|--:|---|
| EKH | stage A: the `kEffect` shape | 0 | |
| EW1 | `0x433640`, `0x462B00..0x46FFB0`, EKF, EKP | 274 | EK1, EK2, EKF, EKP (world 0's cluster, section 5) |
| EW2 | `0x470300..0x47FD80` | 379 | EK3, EK4, EK5, part of EK6 |
| EW3 | `0x4801F0..0x487FE0` | 210 | the rest of EK6 |
| EW4 | `0x488020..0x4941E0` | 305 | EK7, EK8 |
| EW5 | `0x4FD2E0..0x50BFF0` | 333 | ES1, part of ES2 (kind 0x18) |
| EW6 | `0x50C0D0..0x516A90` | 196 | the rest of ES2, ES3 (kind 0x18) |

EW3 and EW6 are small enough to run as one wave of 406. The labelling
pass's waves EO1..EO3 are these addresses without this draft's rows; its
FL, MW and BX (field core, menus, battle and the remainder, 977 starts)
are the rounds after, unchanged. Its `community` rows (the faerie
village, [`labelling-pass.md`](labelling-pass.md) section 5) belong to BX's
remainder and want a recorded route.

**Decided by the owner, 2026-09-29** (replacing questions 1 and 2):

1. **Round thirteen is the whole effect engine**: the 1,697 in six waves
   after EKH, not this draft's 627 first.
2. **The 101 `hypothesis` rows are not read before the cut.** Each group's
   brief says: a row that turns out not to be effect code is not taken,
   and goes back to the coordinator with what it is.

**Not verified here**: the join is by entry address only - no unit
(state table) was computed for the labelling rows, so the wave bounds are
address bounds and a kind may straddle one; FC1's and FC2's seeds were
read, not run; the labelling table's one `effect objects` row at
`0x433640` lies in the battle band and is `hypothesis`.

## 10. Regenerated at the round's base (`d1b411c`, 2026-09-29)

Round twelve merged as PR #33 (`d1b411c` on `main`); the round branch is
`phase-3/capture-round-thirteen` from it. Section 6's steps were run there,
all output in the session scratchpad (`.../56ff1eb2-8c2d-4d5f-82f0-a85df7f2d489/scratchpad/round13/`)
and the cut in the main checkout's `analysis/round13_cut.tsv` (gitignored).

**The catalog**: 3,815 not ours of 10,246 (4,140 at `61be26e`). Part 5 is
628, part 2 907, part 7 1,320, part 3 has 7 left. **The labelling tool** at
the same `symbols.toml`: 2,227 rows, 1,070 of them `effect objects` (964
`evidence`, 106 `hypothesis`).

**The join** (scratch `join.py`): 1,698 starts, **1,695 takeable**, three
cases. Against section 9's draft of 1,697:

- two gone, both FC2's in round twelve as "code no list has":
  `0x46C820` (`EffectKind34_V0DebrisStart`) and `0x46D400`
  (`EffectKind41_Run`);
- one new in the labelling table and not a function: `0x47C420`, flagged a
  case of `0x47C370`'s switch (text table `0x47C498`), beside the two
  `NOTFN` cases of section 4;
- five more rows carry the `hypothesis` tier than in the draft (the feed-back
  passes saw wave two's names), none moved in or out.

**The groups** (scratch `make_cut.py`): each wave's rows in address order,
cut at about 48 where the unit changes (the labelling table's `detail`, or
the draft's `unit` for a part-5 row), never above 64, a run under 12 joined
to its neighbour. 35 groups, and the stage-A group EGT beside them. `0x433640` goes with E4A, whose `0x488220`
reads its table, not with wave one. Section 3's thirteen groups and section
9's wave table's third column are replaced by this:

| Group | Band | Fns | Hidden | `hypothesis` | Bytes (catalog) | Module |
|---|---|--:|--:|--:|--:|---|
| E1A | `0x462B00..0x467270` | 64 | 53 | 8 | 11,645 | effect_1a |
| E1B | `0x4672F0..0x46A5E0` | 48 | 30 | 1 | 10,825 | effect_1b |
| E1C | `0x46A850..0x46F1C0` | 52 | 35 | 4 | 6,910 | effect_1c |
| E1D | `0x46F2B0..0x46FFB0` | 30 | 28 | 5 | 3,075 | effect_1d |
| E1E | `0x528CD0..0x52A4A0` | 48 | 46 | 0 | 6,624 | effect_1e |
| E1F | `0x52A6C0..0x52CFE0` | 15 | 6 | 0 | 2,510 | effect_1f |
| E1G | `0x594060..0x594D50` | 14 | 4 | 0 | 3,274 | effect_1g |
| E2A | `0x470300..0x473100` | 64 | 36 | 5 | 11,720 | effect_2a |
| E2B | `0x4731A0..0x474D20` | 51 | 30 | 4 | 7,387 | effect_2b |
| E2C | `0x474F40..0x477180` | 51 | 29 | 2 | 8,596 | effect_2c |
| E2D | `0x4771B0..0x4789B0` | 54 | 39 | 2 | 6,065 | effect_2d |
| E2E | `0x4789D0..0x47B790` | 51 | 41 | 4 | 6,496 | effect_2e |
| E2F | `0x47B7D0..0x47DAC0` | 52 | 36 | 4 | 8,006 | effect_2f |
| E2G | `0x47DBE0..0x47FD80` | 56 | 50 | 4 | 5,836 | effect_2g |
| E3A | `0x4801F0..0x482360` | 48 | 29 | 3 | 7,825 | effect_3a |
| E3B | `0x4823D0..0x484000` | 49 | 40 | 1 | 6,764 | effect_3b |
| E3C | `0x484050..0x485C50` | 50 | 32 | 2 | 7,125 | effect_3c |
| E3D | `0x485CB0..0x487FE0` | 63 | 44 | 6 | 8,660 | effect_3d |
| E4A | `0x433640..0x489020` | 48 | 48 | 4 | 4,095 | effect_4a |
| E4B | `0x489030..0x48B1F0` | 61 | 45 | 5 | 8,254 | effect_4b |
| E4C | `0x48B200..0x48C7F0` | 49 | 28 | 4 | 5,790 | effect_4c |
| E4D | `0x48C990..0x48DF70` | 51 | 39 | 0 | 5,526 | effect_4d |
| E4E | `0x48DF90..0x491C60` | 48 | 29 | 9 | 8,931 | effect_4e |
| E4F | `0x491D70..0x493F70` | 45 | 30 | 9 | 5,607 | effect_4f |
| EGT (stage A) | `0x494060..0x4941E0` | 4 | 0 | 4 | 464 | effect_gte |
| E5A | `0x4FD2E0..0x4FF150` | 53 | 45 | 0 | 7,949 | effect_5a |
| E5B | `0x4FF320..0x501430` | 48 | 38 | 0 | 8,583 | effect_5b |
| E5C | `0x501500..0x503D30` | 60 | 51 | 3 | 10,393 | effect_5c |
| E5D | `0x503DE0..0x506860` | 49 | 37 | 3 | 11,181 | effect_5d |
| E5E | `0x506A10..0x508BA0` | 51 | 44 | 2 | 8,769 | effect_5e |
| E5F | `0x508CC0..0x50AD70` | 48 | 42 | 2 | 8,471 | effect_5f |
| E5G | `0x50AF90..0x50BFF0` | 24 | 21 | 0 | 4,406 | effect_5g |
| E6A | `0x50C0D0..0x50E1C0` | 48 | 43 | 0 | 8,983 | effect_6a |
| E6B | `0x50E400..0x510C80` | 50 | 46 | 1 | 7,419 | effect_6b |
| E6C | `0x510C90..0x5140C0` | 48 | 35 | 2 | 13,629 | effect_6c |
| E6D | `0x514270..0x516A90` | 50 | 39 | 2 | 10,366 | effect_6d |

Wave one is E1A..E1G (271), two E2A..E2G (379), three E3A..E3D (210), four
E4A..E4F (302, EGT's four taken out), five E5A..E5G (333), six E6A..E6D (196). `band_rows.py
--byte-tables --groups` over the cut prints every row and **18 functions no
list has** in the groups' spans (the draft's six among them); they are the
groups', as in round twelve. No group name or `src/game/effect_*` file
exists yet.

**The edges** (`--edges`, 591 across and inside groups). Three rows of E4F
are called from nearly every group of waves one to four: `0x494110` (188
sites from other groups), `0x494060` (79), `0x4941E0` (34). Then E1F's
`0x52CFE0` (75) and `0x52CF60` (22), E1B's `0x469750` (24) and `0x468AC0`
(15), E5D's `0x503FA0` (15). **The first three, and `0x494180` between them (6 sites), are the
stage-A group EGT** (the owner's word, 2026-09-29): each is a short cdecl
helper whose every callee is ours (`Gte_RotMatrix`, `Gte_ApplyMatrix`,
`Gte_SetRotMatrix`, `Gte_SetTransMatrix`, `Gte_RotTransPers`,
`Gte_StoreDepthF`, `Gte_RotTrans`; read 2026-09-29 by capstone for the
calls only, `0x494180` not read), the labelling tool's "kind 186" for them
a `hypothesis` by address. 120 rows of the cut call them, and so do
functions already ours (`Area143_DrawGlowCylinder`, `Area148_DrawBeam`,
`EffectKind30_SparksDraw` among them, raw today). Taken beside EKH and
merged before wave one, every group calls them by name. For the other
five EKH's brief asks for a harness stand-in each.
**Merge order inside a wave, callee first**: wave one E1F, E1B, E1A, E1G,
then E1C, E1D, E1E; wave two any order but E2F before E2E; wave three E3B
before E3C; wave four E4F, then E4D, E4B, E4A; wave five E5D before
E5C; wave six E6A and E6C before E6B. Waves five and six (kind 0x18) have no
edge to the others but E6A's one call into E5G.

**The base's proof**: built in the detached verification worktree, `BOF3X_SHADOW='*'` headless exit 0, `inject: 6891 ours, 0 left original`, `ledger_check` 63 entries, 0 errors (scratch `verify_base_d1b411c.log`).

**The tip** after `main`'s PR #34 (the capture review's fixes, which touch
both harnesses and not `symbols.toml`) was merged in: `34f1c40`, the same
proof passing (scratch `verify_34f1c40.log`).

**Next**: EKH and EGT in parallel (scratch `brief_ekh.md`, `brief_egt.md`),
then wave one's briefs from section 9's template with this table's lines.

## 11. Stage A (2026-09-29 afternoon, from `d19d803`)

Two Opus agents in worktrees, in parallel, each merged with
`merge_group13.sh` and verified by its own build and `'*'` in the detached
verification worktree.

| Group | Branch tip | Merge | What | Proof at the merge |
|---|---|---|---|---|
| EGT | `1a0ad06` | `a7233b6` | the four helpers taken, all cdecl: `EffectGte_LoadMapCamera` `0x494060`, `EffectGte_ProjectPoint` `0x494110`, `EffectGte_SetDiagonalOne` `0x494180`, `EffectGte_ProjectSize` `0x4941E0` ([`effect_gte.md`](effect_gte.md)); 41 controls, 38 refused, 3 equivalent with a refused near variant; five files rebound | `effect_gte` and `'*'` exit 0, 6,895 ours, ledger 0 errors |
| EKH | `5d04585` | `dc32be3` | `kEffect` in `scenario_harness` (FC1's form: the current object an effect record), `Arg::kEffect`, 12 arguments, `DataTable` to 128, 109 effect-standard stand-ins and 10 louder re-listings, nine regions ([`scenario_harness.md`](scenario_harness.md) section 8); takes nothing | `scenario_harness_ekh` and `'*'` exit 0, 6,895 ours, ledger 0 errors |

Then the coordinator's `80385d6`: `kField`'s three rows for EGT's helpers by
name (`FIELD_OURS`), a standard effect row for `EffectGte_SetDiagonalOne`
(18 bytes written). Verified the same way, `field_c2` and `battle_e7` among
the shadows run: all exit 0, 6,895 ours.

**What the groups must know** (the agents' reports):

- A group's inject goes **after** `ScenarioHarnessEkh_Inject`, which copies
  eight of the cut's rows.
- A table's span is its own length, not the run of code pointers: the tables
  overlap, and `EffectKind18_States[n]` past about 100 names other kinds'
  tables.
- `0x593950` is the dispatcher of the run at `0x66A470` (section 4's open
  item, E1G's rows).
- `EffectGte_ProjectSize` aborts at depth 0 (Capcom's divides unchecked): a
  fuzz that calls it for real keeps the point off the eye plane.
- The weakest stand-ins: `0x4794D0` and `0x479B70` hash the first 16 bytes
  of a large record only; `0x469750` and `0x468AC0` (E1B's) advance the
  packet cursor by amounts EKH chose. E1B merges second in wave one, after
  which they are called by name.
- `0x4941B0`, a fifth helper between EGT's (43 bytes, three callers, catalog
  part 2), is in no group: the coordinator's to place.

**Debts from stage A**: FC1's and FC2's fuzz files are not rewritten onto
`kEffect`; EGT's latent defects to number (`0x494180` writes 1 where the PSX
twin writes 0x1000; `0x4941E0` divides by an unchecked depth and keeps 16
bits of the quotient); `reach_whelp` enters `0x494110` and `0x494060`, so
the whelp route's frame hash is owed at this tip.

**Found beside it, not this round's**: `battle_e7` fails in the main
checkout's build directory (`GeneWin_ListSlideOut`, `List2`, `List3`: 484,
440 and 876 rounds) and passes in the verification worktree at the same
commit; it does so at `d1b411c` already. Capcom's clone never reaches the
`Window_FreeCurrent` recorder there and ours does. Handed to a session of
its own, and **fixed** there (2026-09-29): not the layout but the main
build directory's `bof3x.ini`, whose `wide=1` the launcher turns into
`BOF3X_WIDE=1` under `--no-config` too. DIV-0041 widens the three
slide-outs' `cmp ax, imm16` bounds before `BattleE7_Inject` clones them, and
ours held the original bounds; in every failing round `+4` sat on the
original bound. Ours now reads the operand ([`battle_e7.md`](battle_e7.md)
§3). The verification worktree has no ini, so `verify_tip.sh` is narrow
only: a wave's verification adds one `BOF3X_WIDE=1` star run.

## 12. Wave one (launched 15:30 from `1bb41df`, merged 16:41..18:35)

Seven Opus agents in worktrees. Each merge built and run in the detached
verification worktree: the group's shadow and `'*'` narrow at every merge,
`'*'` with `BOF3X_WIDE=1` at the first four and at the tip. The agents
branched before `080e1c1` (`battle_e7`'s bounds), so each met `battle_e7`'s
failure in its own wide run and reported it; their own modules passed wide.

| Group | Branch tip | Merge | Functions | Controls (planted / refused by a count / equivalent) | Ours after | What it turned out to be |
|---|---|---|--:|---|--:|---|
| E1F | `5c0889d` | `643b2d4` | 15 | 92 / 90 / 1, one refused by a fault with a refused near variant | 6,910 | a field menu kept in effect record 6, `UiSprite_Draw` / `UiSprite_SetMode` (131 sites), game mode 8's steps, FE1's panel arithmetic, a scaled sprite pass |
| E1B | `1d22386` | `40bc9dc` | 48 | 48 / 48 / 0 | 6,958 | kind 0xF's states 26..40 and six child sub-kinds, the panel and window helpers, kinds 0x11, 0x12, 0x14, 0x92 |
| E1G | `edf4ddf` | `d48ffc8` | 14 | 45 / 45 / 0 | 6,972 | the rest of the item-trade screen (FE2 took its first seven states); not effect code |
| E1D | `15ec038` | `d9e2d85` | 30 | 72 / 71 / 1 | 7,002 | kinds 0x21..0x27 |
| E1E | `e7fd3f3` | `62f4b68` | 48 | 58 / 58 / 0 | 7,050 | the steps of the leader's state 9 (`Field_LeaderStates[9]` `0x528880`); not effect code |
| E1A | `bc71a4d` | `50088e9` | 70 | 133 / 130 / 3 | 7,120 | kinds 1, 2, 3, 5, 7..0xD, 0xF, 0x1A; the cut's 64 and six dispatchers no list held |
| E1C | `7e4e78c` | `1ad5ce3` | 53 | 73 / 71 / 2 | 7,173 | kinds 0x1C..0x20, 0x36, 0x3C, 0x70; the cut's 52 and `0x46F230` |

**278 functions, 6,895 -> 7,173**, every group 0 mismatches, 521 controls:
513 refused by a count, 7 equivalent mutants each with a refused near
variant, 1 refused by a fault with a near variant refused by a count. Then
the coordinator's `82cc33b`: sixteen `scenario_harness` rows by name. **The
tip's proof** (`82cc33b`, scratch `verify_w1.log`): `scenario_harness_ekh`,
`field_e1`, `field_e2` and `'*'` exit 0, 7,173 ours; `'*'` with
`BOF3X_WIDE=1` exit 0; `ledger_check` 0 errors. The main checkout's build
directory was not used for it: a running game held its DLL.

**What the wave learned** (in the later waves' briefs):

- The cut's `unit`, `unit_desc` and `label` columns are often wrong: the
  unit scan attributed word-indexed dispatchers and cells 0x400 off. They
  made the groups, they do not say what a row is.
- A good part of the "effect engine" by address is field code: the item
  trade, the leader's state 9, game mode 8. Catalogued rows are taken all
  the same, in field mode.
- The labelling pass filed six kind dispatchers of E1A's band under other
  subsystems, so the join dropped them. A group looks at every
  `Effect_KindHandlers` entry that points into its band.
- The effect-mode stand-in for `Gte_RotTransPers` fills whole-number
  floats; a function that rounds wants fractions (E1D's control 41).

**In no group, to place** (the agents' reports; none read by the
coordinator): `0x4941B0`; `0x46D770`, `0x46E190`; `0x46F570`, `0x46F690`,
`0x46FAE0`; the rest of the leader's state 9 - `0x528880`, `0x5288A0`,
`0x5289A0`, `0x52AF60`, `0x52B0E0`, `0x52B1B0`, `0x52B200`, `0x52B250`,
`0x52B2A0`, `0x52B2E0`, `0x52B330`, `0x52B370`, `0x52B460`; `0x52B6C0`;
`0x593950`, `0x594D90`; and 23 unlabelled catalogue rows in E1F's band.

**Debts from wave one**: the rebinding of raw calls between the wave's own
groups (each called the others raw; the addresses are in each group's doc);
`EffectKind36_Frames` `0x653F88` lies inside FC1's hypothesis count of 32
for `EffectKind14_Op`; `EffectKind1A_States` `0x653C5C` named by E1A holds
E1B's states 13..16; the `entries_logic.txt` lines for `0x46FCF0` and
`0x46FFB0` that are too long; the latent defects to number (each group's
doc); E1B's read of up to four bytes before an animation script
([`effect_1b.md`](effect_1b.md) section 7); `scenario_harness_ekh.cpp`
still clones three of E1A's and E1D's functions by address, which works
while its inject runs first. **Live**: `reach_whelp` enters two of EGT's
helpers and one full cycle of kind 0x1C, so the whelp route's frame hash is
owed at this tip; every other function of the wave is fuzz-only.

## 13. Wave two (launched about 19:20 from `dcef661`, merged 19:55..21:11)

Seven Opus agents in worktrees, the owner away and the waves his to launch
at will; paused after this wave at the owner's word (the account's usage
cap). Each merge built and run in the detached verification worktree, the
group's shadow and `'*'` narrow; every agent ran `'*'` narrow and with
`BOF3X_WIDE=1` in its own worktree, all exit 0.

| Group | Branch tip | Merge | Functions (cut + added) | Controls (planted / refused by a count / equivalent) | Ours after | Kinds |
|---|---|---|--:|---|--:|---|
| E2G | `e1ac01e` | `83022d9` | 56 + 2 | 80 / 78 / 2 | 7,231 | 0x15, 0x54, 0x55, 0x57, 0x5A, 0x5B, 0x5D's and 0x5E's dispatchers, 0x5F, 0x66, kind 0x18's sub-kind 0x20 |
| E2B | `f92e99e` | `a7182d3` | 51 + 1 | 100 / 99 / 0, one stopped by ours' abort with a refused near variant | 7,283 | 0x2F, 0x33, 0x35, 0x38, 0x39, 0x3B, 0x3D |
| E2D | `dec056b` | `99eb61a` | 54 | 148 / 145 / 3 | 7,337 | 0x45..0x48, 0x49's dispatcher and variants 0..4 |
| E2C | `2d38a24` | `c0a9da9` | 51 + 2 | 169 / 167 / 2 | 7,390 | 0x3E, 0x3F, 0x40, 0x42, 0x43, 0x44, 0x6B |
| E2F | `510d5e7` | `7dbb79c` | 52 + 10 | 126 / 125 / 1 | 7,452 | 0x4F..0x53, 0x56, kind 0x4E's disc; nine part-7 rows of no group; the `NOTFN` start `0x47C420` inside `0x47C370` |
| E2E | `a8a271d` | `9f351c2` | 51 | 94 / 93 / 1 | 7,503 | kind 0x48's states 7..12, 0x4A..0x4E, the angle and sphere helpers |
| E2A | `1d2e0ec` | `45a698a` | 64 + 1 | 134 / 131 / 3 | 7,568 | 0x28..0x2E |

**395 functions, 7,173 -> 7,568**, every group 0 mismatches, 851 controls:
838 refused by a count, 12 equivalent mutants each with a refused near
variant, 1 stopped by ours' own abort with a refused near variant. All of
it effect code, unlike wave one. Then the coordinator's fold: three
`scenario_harness` rows by name (E2F's). **The tip's proof** (scratch
`verify_w2.log`): `scenario_harness_ekh`, `effect_2d`, `effect_2e` and `'*'`
exit 0, 7,568 ours; `'*'` with `BOF3X_WIDE=1` exit 0; `ledger_check` 0 errors.

**What the wave learned**:

- The harness's effect-standard rows are too quiet for much of this code,
  and five groups re-listed the same ones in their own fuzz files: the
  `EffectGte_ProjectPoint`, `EffectGte_ProjectSize` and `Gte_VectorNormal`
  rows log stack pointers by value; `Math_Cos`, the square root `0x5A7A90`,
  `0x47CF20` and `0x47A130` answer garbage where the callers need the real
  answer; the `0x4941B0` row writes where the function only reads;
  `Sprite_UpdateScreen` and `MapView_LinkPrimAt` do not log
  `Sprite_Current`. **The fold is owed before wave three** (each group's
  doc has its list: `effect_2b.md`, `effect_2c.md` section 8, `effect_2d.md`,
  `effect_2e.md`), beside the rows that must not change what merged groups
  draw.
- A draw that several states tail-jump to is a function of its own when it
  has its own frame and `ret` (E2A's `0x471A60`, E2B's `0x473F10`, E2D's
  `0x477DB0`, E2G's `0x47E120`): four of section 4's "shared tail" kind,
  all taken whole.
- The tool's table counts run on into the next table: E2G's kinds 0x5D and
  0x5F (5 and 10, not 9 and 16), E2A's kind 0x2E (3 of a run of 17).

**In no group, to place** (beside section 12's list): the states of kinds
0x5D and 0x5E, `0x47F2D0..0x47FD7F` but `0x47F5D0`; of `0x4790C0..0x47A3D0`
what E2F did not take - `0x4790C0`, `0x4790F0`, `0x479260`, `0x4792E0`,
`0x4794D0`, `0x4796B0`, `0x47A110`, `0x47A130`, `0x47A150`, `0x47A200`;
`0x59E930`.

**Debts from wave two**: the harness fold above; one defect entry for the
pools that overlap inside `EffectKind30_Shards` (E2A, E2B, E2E, E2F each
describe it), and the symbol's count (E2B: used to `0x92CFC0`); E2C's
reading that E2B's kind 0x35 table ends before `0x654468`, to check against
E2B's `[[data]]` count; `Window_DrawFrame`'s evidence names `0x47DAC0` as a
caller where the callers are E2G's (E2F); the latent defects to number;
`scenario_harness_ekh.cpp` clones `0x472770` and `0x479970` by address.
**Live**: `reach_whelp` enters E2A's `0x471D10` and `0x471E20`; with EGT's
two helpers and E1C's kind 0x1C the whelp route's frame hash is owed at this
tip, not run (the agents' self-tests and the owner's game held the machine).

**The fold, done 2026-10-03** (`f8d410a`; [`scenario_harness.md`](scenario_harness.md)
8.5 has the rows): in effect mode `EffectGte_ProjectPoint`, `EffectGte_ProjectSize`
and `Gte_VectorNormal` hash their points and log no pointer by value;
`Math_Cos` never answers 0 or -1; `Sprite_UpdateScreen` logs `Sprite_Current`
and its record; `MapView_LinkPrimAt` moves the packet cursor;
`EffectSpark_FindFree` and `0x47A130` answer the first free record or null;
`0x4941B0` reads its three points and writes nothing. Left to the groups: the
square root `0x5A7A90` (what a caller needs of it differs by caller). The
groups' own re-listings stand over these rows, so only the functions that used
the standard rows see them: wave one's and the rest of wave two's fuzz ran
under the louder rows with 0 mismatches. **The proof** (the verification
worktree, scratch `verify_98d0631.log` for the tip before it and
`verify_f8d410a.log`): `scenario_harness_ekh`, `effect_1a`, `effect_2c`,
`effect_2g` and `'*'` exit 0, 7,570 ours (7,568 and the two sky draws of
2026-09-30), `'*'` with `BOF3X_WIDE=1` exit 0, `ledger_check` 0 errors.
**Not done before wave three: the whelp route's frame hash** - it wants the
machine hands-off and the hash reference re-recorded first (HANDOFF); it
checks merged code and does not gate the wave.

## 14. Wave three (2026-10-03: launched about 08:35 from `0e0532c`, merged 09:16..10:10)

Four Opus agents in worktrees; the briefs from `make_briefs.py <scratch> 3
<tip> band_edges_w2.txt` with wave two's addendum (the fold's rows, the tail
draws, the table counts). Each merge built and run in the detached
verification worktree, the group's shadow and `'*'` narrow; every agent ran
`'*'` narrow and with `BOF3X_WIDE=1` in its own worktree, all exit 0.

| Group | Branch tip | Merge | Functions (cut + added) | Controls (planted / refused by a count / equivalent) | Ours after | Kinds |
|---|---|---|--:|---|--:|---|
| E3B | `d2b5954` | `6faad58` | 49 | 101 / 100 / 0, one stopped by ours' abort with a refused near variant | 7,619 | 0x63, 0x65, 0x67, 0x69, 0x6C |
| E3D | `37bb107` | `a8700be` | 63 + 5 | 148 / 146 / 1, one stopped by ours' abort | 7,687 | 0x77 (whole: its dispatcher and three sub-states were rows of no group), 0x78, 0x7B..0x7D, 0x7F..0x82 (kind 0x82's states 11..23 are E4A's) |
| E3C | `60f8859` | `8ee6c6a` | 50 + 1 | 85 / 84 / 1 | 7,738 | 0x6D, 0x6E, 0x6F, 0x72..0x75, the shared debris draw and set-up; `0x485C60`, kind 0x75's tail with its own frame |
| E3A | `5a01f47` | `bfc8162` | 48 | 65 / 65 / 0 | 7,786 | 0x60, 0x61, 0x62, 0x64, 0x68 |

**216 functions, 7,570 -> 7,786**, every group 0 mismatches, 399 controls:
395 refused by a count, 2 equivalent mutants each with a refused near variant,
2 stopped by ours' own abort (E3B's with a near variant refused by a count).
`fold_names.py` found no harness row to name; `consolidate_entries.py` 8,577
entries, every one of the 216 with a line. **The tip's proof**
(`bfc8162`, scratch `verify_w3.log`): `scenario_harness_ekh`, the four
groups' shadows and `'*'` exit 0, 7,786 ours; `'*'` with `BOF3X_WIDE=1` exit
0; `ledger_check` 68 entries, 0 errors.

**One divergence came with the wave, for the owner's word: DIV-0068**
(Forced, E3A). `EffectKind64_DrawGlow` `0x481740` reads its rim vertices'
depth from a stack word the original never writes; ours writes the centre's
depth, as the two sibling discs do. `BOF3X_ORIGINAL=EffectKind64_DrawGlow`
runs Capcom's. Not seen live.

**What the wave learned**:

- The first wave under the fold's rows. E3D re-listed a trail projection
  louder in its own file for one control; what else the groups re-listed is
  in their docs and not yet read against the fold.
- A memory read in the same C++ expression as a call can run before the
  call (E3B's 347 mismatches in `EffectKind6C_ScatterSparks`): compute each
  call's answer into a local first, in the original's order.
- A seed can hand the original a loop that never ends (E3A's frame height
  0x100 in `EffectKind61_Scatter`: `'*'` died twice with no FATAL line
  before the seed was fixed) - a silent death that repeats is a seed.
- The tool's count for `EffectKind82_States` was 53; the dispatcher reaches 24.

**In no group, to place** (beside sections 12's and 13's lists): `0x4837B0`
(called by `EffectKind69_Part2`); `0x480210`, `0x480270`, `0x4802C0` (kind
0x60's states) and `0x480300`; `0x492750` (kind 0x80's states 4..7),
`0x492530` and `0x492CF0` (callers of E3D's drop helpers).

**Debts from wave three**: the rebinding between the wave's groups (E3C's
raw calls to E3B's `0x483C10` and `0x483DA0`) and of the inbound calls each
doc lists for wave four (E4A's table entry `EffectKind82_Start`, E4C's call
to `0x4841C0`, E4D's, E4E's and E4F's calls into E3A's); the spark cursor
`0x67626C`, written by E3B's and E3C's code and named by neither;
`scenario_harness_ekh.cpp` still copies `0x4857C0` by address and
`scenario_harness.h`'s comment names `0x486AB0` (`EffectKind7D_FillF4`);
E3D's `EffectKind7D_SetMap` is DIV-0062's site and is in `draw_pool.cpp`'s
`kOwnedUsers` now; E3C's `entries_logic.txt` line for `0x485960` spanned
sixteen of E3D's (the consolidation cut it); the latent defects to number
(each group's doc). **Live**: every function of the wave is fuzz-only; the
whelp route's frame hash is still owed for waves one and two.

**Next**: wave four, `make_briefs.py <scratch> 4 <tip> band_edges_w2.txt`,
E4A..E4F (302); merge order E4F, then E4D, E4B, E4A.
