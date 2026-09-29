# The labelling pass: what the catalogue's unlabelled and boot-resident starts are

**Status:** IN PROGRESS (verified 2026-09-29 at `61be26e`) - a measurement and
a tool, nothing taken over, nothing named. The labels are proposals in a
scratch TSV; promoting any of them into `symbols.toml` is the coordinator's
step. Written for round thirteen's successor
([`takeover-queue-field-battle.md`](takeover-queue-field-battle.md) section 9
named the remainder this pass labels).

## 0. The answer in one paragraph

At `61be26e` the catalogue leaves **1,343 unlabelled starts** (part 7) and
**891 boot-resident** ones (part 2) - 2,234 in all, 296 KiB. Read by the new
`tools/label_runs.py`, **2,047 of them are functions to take and 187 are
switch cases** (a `.text` jump table's targets inside another function, not
functions). Of the 2,047, **1,070 are one subsystem nobody had counted: the
effect objects** - the 187 kinds of `Effect_KindHandlers` (`0x655350`, read
by `Effect_RunObjects`), each an entry that jumps through its own state
table on a byte of `Sprite_Current`, with the states laid out after the
entry, and kind 0x18's 105-program table `EffectKind18_States` whose code
fills `0x4FD2E0..0x516B2E`. The rest: 374 field core (the party sets' field
actions `0x51B5A0..0x528CC8` above all), 239 menus and windows (the field
menu `0x58A190..0x5903E5`, the window kinds `0x598890..0x59CAF8`) with the
shop, 212 battle (203 engine - `Effect_Handlers`' slots `0x4468B0..0x454373`,
the BATE root - and 9 spell-effect starts), 2 event script, and 150 of area, minigame,
scenario, world-map, item and top-level code. The tool's
labels hold at 84.6% when a whole implementation file of ours is hidden and
re-labelled (93.6% at its `evidence` tier), and 41 of 46 functions read by
hand held (section 5). No start is an orphan, padding or a fall-through;
the CRT and the MP3 decoder are out, and the catalogue's MP3 range starts
0x2EA0 bytes too low (section 4). Six waves of about 350 (section 7).

## 1. The count, re-measured

`tools/remaining_catalog.py` against this checkout's `symbols.toml`, the
inputs read from the main checkout's `analysis/` (they are gitignored, so
they exist only there), outputs to a scratch folder:

```
python tools/remaining_catalog.py --funcs <main>/analysis/pc_funcs.json --hidden <main>/analysis/pc_hidden.json \
  --pairs <main>/analysis/pairs_propagated.json --area-pairs <main>/analysis/area_pairs.json \
  --xref <main>/analysis/pc_xref.json --runs <main>/analysis/calltrace \
  --sibling ../BreathOfFire3Recomp/ --symbols symbols.toml --out <scratch>/catalog_61be26e.md --tsv <scratch>/catalog_61be26e.tsv
```

and the same with `git show c4b0d32:symbols.toml` as `--symbols`, which
reproduces the plan's figures exactly (4,446 not ours; 1,349 and 891):

| Part | `c4b0d32` | `61be26e` | Moved |
|---|--:|--:|---|
| 0 Platform (CRT, MP3, shell, set-up) | 464 | 464 | |
| 1 Library layer | 69 | 69 | |
| **2 Boot-resident game code** | **891** | **891** | 3 taken, 3 in (below) |
| 3 Field modes | 323 | 323 | (wave two, running) |
| 4 Battle | 300 | 0 | 298 taken, 2 to part 2 |
| 5 Area overlays | 629 | 629 | |
| 6 Scenario and character sets | 421 | 421 | |
| **7 Unlabelled** | **1,349** | **1,343** | 5 taken, 1 to part 2 |
| Not ours, total | 4,446 | 4,140 | 306 rows fewer |

Ours went from 6,238 `impl` lines to 6,554 (316, round twelve wave one);
306 of those were catalogue rows, the other ten starts no list had (the
groups' docs name them, e.g. BE5's six). The five unlabelled starts taken
are round eleven's debts that BE3 and BE4 folded in (`0x4376F0`,
`0x441090`, `0x446DE0`, `0x446E00`, `0x446E20`). The three boot-resident
starts taken were `Boot: unnamed` pairs; the three that moved into part 2
got names in wave one without an `impl` (two from part 4, one from part 7).

**So the pass's input is 1,343 + 891 = 2,234 starts**, 916 and 852 of them
pointer-reached ("hidden"), 224,238 and 71,549 bytes by extent.

## 2. The tool: `tools/label_runs.py`

Its docstring is the method; in short, for every start of the universe
(`pc_funcs.json`, `pc_hidden.json` and every `symbols.toml` function, 10,246
plus the symbol-only ones) it reads the extent by recursive descent
(`band_rows.read_extent`) and every absolute address the reached
instructions name, including the zero-fill `.bss` globals; it sweeps
`.text` for every direct transfer and immediate naming a start, and every
aligned `.rdata` / `.data` dword holding one. A start is *known* from its
`impl` file (ours), else its catalogue label, else its name, mapped onto
eighteen coarse classes (`CLASSES`). The unknown ones get a vote from
eight families - callers, tables (classed by the code that *indexes* the
table, not the long run of pointers it sits in), dispatches (the slots of
tables it indexes), callees and data touched (each weighted against how
many classes share it), neighbours, PSX pairs (never `call-disputed`), and
a weak family for anchors the catalogue placed only by address - and a
tier: `evidence` when the structural families are unanimous and another
agrees, `hypothesis` otherwise, `unnamed` when nothing votes. Three passes
feed the `evidence` proposals back as anchors.

```
python tools/label_runs.py --exe <main>/bof3/BOF3.exe --analysis <main>/analysis --sibling ../BreathOfFire3Recomp \
  --catalog <scratch>/catalog_61be26e.tsv --out <scratch>/out        # labels.tsv, runs.tsv, boot_tables.tsv, boot_members.tsv
python tools/label_runs.py ... --function 0x42D710,0x51CA60          # one start's every vote
python tools/label_runs.py ... --validate [--holdout start|file]     # the measurement below
```

About 25 s a run. `labels.tsv` has a row per start of parts 2 and 7:
entry, size (extent), run, set, hidden, proposed class, detail (the impl
file stem, catalogue label, table or effect kind that carried the vote),
tier, pass, suspect flags, catalogue label, and the evidence (callers,
tables with slot index, dispatches, PSX, callees, imports, named data,
neighbours). It is derived from the game and stays in scratch (CLAUDE.md
rule 1).

**What a tier is worth** (`--validate`, 2026-09-29): every start of ours
voted on with its own class hidden *and its whole implementation file's*
(so its neighbours are unknown, as an unlabelled run's are):

| Tier | Right | Of | Rate |
|---|--:|--:|--:|
| `evidence` | 3,408 | 3,642 | 93.6% |
| `hypothesis` | 2,136 | 2,912 | 73.4% |
| all | 5,544 | 6,554 | 84.6% |

Hiding only the start itself gives 98.1% / 64.2% - optimistic, because
its own file's neighbours vote. The commonest confusions are between
classes this project's own files split finely: area overlays read as field
core (121) or world map (85), event script as field core (99), spell
effects as battle engine (52), menus as shop (48). A class the tool gets
wrong is usually a neighbouring subsystem, not a random one.

## 3. What the 2,234 are

**Counts by proposed class**, the 187 case starts left out (section 4):

| Class | Unlabelled | Boot | Takeable | `hypothesis` |
|---|--:|--:|--:|--:|
| effect objects | 778 | 292 | 1,070 | 101 |
| field core (incl. the party sets' field actions) | 283 | 91 | 374 | 70 |
| text, windows, menus | 142 | 72 | 214 | 57 |
| battle engine | 31 | 172 | 203 | 28 |
| area overlays | 54 | 21 | 75 | 42 |
| minigames, master | 36 | 0 | 36 | 17 |
| shop, inn, save point | 6 | 19 | 25 | 3 |
| scenario banks | 6 | 11 | 17 | 15 |
| top-level, platform | 4 | 10 | 14 | 9 |
| battle effects | 0 | 9 | 9 | 5 |
| party state, items | 0 | 6 | 6 | 6 |
| world map, event script | 0 | 4 | 4 | 1 |
| **total** | **1,340** | **707** | **2,047** | |

**The effect objects** are the finding. `Effect_KindHandlers` (`0x655350`)
has exactly 187 distinct code pointers, the one reader `Effect_RunObjects`;
173 of its slots lie in `0x462B00..0x494200`, 12 in the area band, 18 are
ours. A slot is an entry of 18 bytes - load `Sprite_Current`, jump through
a table on its byte +1 (or +2) - and the table's states follow the entry
in address order (read: kinds 40, 81, 158, 160, section 5). 671 of the
effect-object rows are in that band (149 kind units); 398 more are
`0x4FD2E0..0x516B2E`, the programs of kind 0x18: `EffectKind18_States` at
`0x65406C` is **105** slots read by `EffectKind18_Run`, not the 160 its
`symbols.toml` count gives - the dwords after it are thirteen other kinds'
state tables, each read by its own entry (`0x654210` by `0x46D890` ...
`0x6542E0` by `0x46FEE0`; measured by the touched bases inside the run).
Several of these kinds' neighbours are paired by the catalogue to PSX
scenario-effect overlays (SCE1xEF) or area code; on the PC they are one
table's kinds. What any kind looks like in play is not read here (a
question for the owner, with the attract or a route running).

**Runs.** `runs.tsv` has 241 runs of unlabelled starts (adjacent in the
start list, no known start between, gaps under 0x400); merged by class
across small gaps (under 0x3000) they are these bands, both sets, 8 starts
or more (143 smaller bands hold 285 more starts, 37 of them area, 21 field,
15 effect, 14 battle):

| Band | Starts | Unlabelled | Boot | Proposed | `evidence` | Cases |
|---|--:|--:|--:|---|--:|--:|
| `0x42D710..0x42E362` | 9 | 8 | 1 | battle engine (BATE's root and steps) | 3 | 0 |
| `0x4468B0..0x454373` | 166 | 20 | 146 | battle engine (`Effect_Handlers`' slots) | 161 | 4 |
| `0x4561A0..0x45763C` | 43 | 23 | 20 | field core (but see R0008, section 5) | 23 | 0 |
| `0x4603F0..0x460CAE` | 13 | 13 | 0 | minigames, master | 9 | 0 |
| `0x460CB0..0x461710` | 10 | 10 | 0 | text, windows, menus | 0 | 1 |
| `0x462B00..0x463452` | 11 | 0 | 11 | effect objects | 6 | 0 |
| `0x46D780..0x475DF2` | 139 | 58 | 81 | effect objects | 120 | 0 |
| `0x4771B0..0x478562` | 13 | 8 | 5 | effect objects | 11 | 0 |
| `0x4790C0..0x47996E` | 9 | 9 | 0 | area overlays | 5 | 0 |
| `0x47A560..0x47C0E9` | 41 | 32 | 9 | effect objects | 37 | 0 |
| `0x47C350..0x47CF14` | 11 | 11 | 0 | effect objects | 10 | 1 |
| `0x47D910..0x4861B2` | 201 | 167 | 34 | effect objects | 190 | 0 |
| `0x4862F0..0x4889F2` | 26 | 17 | 9 | effect objects | 19 | 0 |
| `0x4896A0..0x48B212` | 34 | 25 | 9 | effect objects | 29 | 0 |
| `0x48B850..0x4910E2` | 119 | 102 | 17 | effect objects | 106 | 0 |
| `0x491AA0..0x492CE2` | 21 | 11 | 10 | effect objects | 16 | 0 |
| `0x492DC0..0x4941AF` | 30 | 24 | 6 | effect objects | 23 | 0 |
| `0x4FD2E0..0x4FEE69` | 46 | 37 | 9 | effect objects (kind 0x18) | 46 | 0 |
| `0x4FEF50..0x504F82` | 112 | 89 | 23 | effect objects (kind 0x18) | 108 | 0 |
| `0x507CB0..0x509A6E` | 41 | 35 | 6 | effect objects (kind 0x18) | 38 | 0 |
| `0x509C00..0x50FD42` | 139 | 112 | 27 | effect objects (kind 0x18) | 137 | 0 |
| `0x514270..0x516B2E` | 45 | 37 | 8 | effect objects (kind 0x18) | 43 | 0 |
| `0x51B5A0..0x528CC8` | 289 | 235 | 54 | field core (`Field_ActionBySet`'s party actions, `Member_States`) | 266 | 2 |
| `0x52AF60..0x52B1AA` | 8 | 8 | 0 | field core | 7 | 0 |
| `0x52B8F0..0x52CD47` | 16 | 16 | 0 | area overlays | 10 | 0 |
| `0x537990..0x5389B9` | 10 | 3 | 7 | scenario banks | 0 | 3 |
| `0x53B4A0..0x53E612` | 8 | 0 | 8 | scenario banks | 0 | 8 |
| `0x5453D0..0x558F4C` | 61 | 0 | 61 | scenario banks | 0 | 61 |
| `0x562C60..0x56AC98` | 18 | 0 | 18 | scenario banks | 0 | 18 |
| `0x56E110..0x56E2E0` | 29 | 0 | 29 | field core | 0 | 29 |
| `0x57DFF0..0x57F4FC` | 13 | 13 | 0 | minigames, master | 7 | 0 |
| `0x57FA40..0x580628` | 11 | 5 | 6 | shop, inn, save point | 11 | 0 |
| `0x5809C0..0x58336E` | 8 | 0 | 8 | shop, inn, save point | 7 | 0 |
| `0x585A00..0x587733` | 9 | 9 | 0 | minigames, master | 3 | 0 |
| `0x58A190..0x5903E5` | 102 | 88 | 14 | text, windows, menus (the field menu's screens) | 91 | 0 |
| `0x596090..0x597FBA` | 18 | 14 | 4 | text, windows, menus | 8 | 1 |
| `0x598890..0x59CAF8` | 70 | 24 | 46 | text, windows, menus (`Window_Handler7/8` kinds, `MenuList_Kinds`) | 49 | 0 |

The four scenario bands and `0x56E110..` are all cases: the scenario
groups' docs already declined them as switch cases (e.g. `scena_sc9a.md`,
19 listed starts that are cases); their names stay, their rows are not
functions.

## 4. Exclusions and suspects

**The MSVC CRT: `0x5B9380..0x5C4000`** (the end of `.text`), 242 starts.
Its code imports only `kernel32.dll` (128 import references by the
descent); its one call out of the range is the PE entry point `0x5BA057`
calling `Game_WinMain`; the game calls into it at 20 entries
(`Crt_sprintf`, `Rand`, `Crt_malloc` / `Crt_free`, the `f*` file
functions, `Crt_filelength` ...). None of it is ours. The catalogue's range
is right.

**The MP3 decoder: `0x5ADEA0..0x5B9380`**, 188 starts. Measured: the
closure of the eight `Mp3_*` entries (`Mp3_Create` `0x5ADF00` ...
`Mp3_MemoryIo` `0x5B0D50`) over calls, immediates and the tables its code
indexes reaches 117 of the 188, lowest `0x5ADEA0`, and never leaves the
range except into the CRT (44 call edges; no imports); the other 71 are
reached from inside it only (function pointers stored by `0x5AE3B0` and
`0x5AF160`, a jump-table case `0x5B8000`). Code outside calls in only at
the eight `Mp3_*` entries, all from `Music_OpenDecoder`, `Music_Decode`
and `Music_Release`. (`0x5B8000`'s one outside "reference" is an immediate
in `Scena07_EnterArea` that happens to equal it; `0x5B281C`'s is from
`0x5ACBD0`, which is data.)

**The catalogue's MP3 range starts at `0x5AB000`, 0x2EA0 bytes too low.**
Its twelve starts in `0x5AB000..0x5ADEA0` are not the decoder: seven span
routines (`0x5AB7E0` ... `0x5AC750`, four of them MMX) reached only through the tables
`0x6722E4`, `0x672360`, `0x6723DC`, which the renderer's `0x5AA80F`,
`0x5AA9EE`, `0x5AAB39` index; four import thunks (`0x5ACBB0`
DirectDrawCreate, `0x5ACBB6` DirectDrawEnumerateA, `0x5ACBBC` dsound
ordinal 1, `0x5ADE90` DirectInputCreateA); and `0x5ACBD0`, which is not
code (the 4,800 bytes to the next start are 16-byte records - the first
four read each begin with the same `.rdata` pointer - that the descent
decodes into nonsense until it fails). `attract_catalog.py`'s `GROUPS` should say
renderer for the spans, platform for the thunks, not-a-function for
`0x5ACBD0`, and MP3 from `0x5ADEA0`; the counts of parts 0 and 1 would move
by twelve. Not changed here - other tools read that table.

**Suspects in the 2,234** (`labels.tsv`'s `suspect` column):

| Flag | Unlabelled | Boot | What |
|---|--:|--:|---|
| case, both tests | 3 | 85 | a `.text` jump table holds it *and* the descent of the start before reaches it |
| case, jump table only | 0 | 99 | a run of two or more code pointers in `.text` holds it |
| falls-in, padding, orphan | 0 | 0 | none |

The 187 cases are the round-twelve kind (`0x44B8D0`, `0x452460` are among
them, both already known to be cases). **No start is an orphan**: every
one has a caller, a table cell or an immediate. Closing reach over the
targets from code outside them, 2,045 are reached and 189 are not - 181
of those are cases (their jump table is inside their own span, so the
closure does not see the host) and the other 8 are reached only through
such cases; so no dead code is shown. 40 starts are named by immediates
only (a stack-built call table, a stored pointer) - normal for this
binary, not a sign of anything. Two catalogue defects on the way: **seven
catalogue rows take their label from a `call-disputed` pair** (the
catalogue's first-pair-wins does not skip the tier; `0x52A8F0` is one), and
its "Field objects" range `0x517200..0x519900` covers the game-mode frames
`0x517330` / `0x517340` (they call BATE's root and another mode's).

## 5. The spot check

Functions read to their last instruction (`band_rows.read_extent`'s
descent, printed with named targets; nothing of it pasted here), each
against the label the tool gave it.

**Sample A: 31 functions**, picked across runs and classes before the last
three fixes. **26 held, 5 missed (84%).**

| Start | Run | Label then | Held | What the code is |
|---|---|---|---|---|
| `0x47BC30` | R0037 | effect objects | yes | state 3 of kind 81's table (`0x47BC10` jumps on +1): 64 random offsets through a helper, a sound, the state advanced |
| `0x470320` | R0021 | effect objects | yes | state 0 of kind 40: height from `AreaMap_Elevation`, a flag test picks state 1 or 2 |
| `0x4FD2E0` | boot | effect objects | yes | kind 0x18's program 1: a timed overlay drawn by the helper below, by `Cond_ByteFE` |
| `0x4FD350` | R0108 | effect objects | yes | its gouraud quad over the screen |
| `0x515020` | R0170 | effect objects | yes | a state of the sub-dispatcher `0x515000` |
| `0x48FC40` | R0099 | effect objects | yes | kind 160's state 0: a position from the party object, a sound |
| `0x50C160` | R0150 | effect objects | yes | a state that closes on the party object's position |
| `0x5146B0` | R0168 | effect objects | yes | the same shape, another program |
| `0x48F5D0` | R0098 | effect objects (hyp.) | yes | a semi-transparent quad for kind 158, linked at map depth |
| `0x51DED0` | R0180 | field core | yes | a party-action dispatcher by +2 |
| `0x5288A0` | R0201 | field core | yes | a dispatcher by +3 beside `FieldCore_FadeSteps` |
| `0x455540` | R0008 | field core (hyp.) | **no** | per-area bookkeeping in a chain run only from `Area179_Init` (below) |
| `0x51CA60` | R0176 | battle engine | **no** | a party action on a cell (`AreaMap_ByteAt` 0xF2 / 0xF8, `Inventory_Add`, messages): field code |
| `0x51DE90` | R0179 | battle engine | **no** | a party-action form dispatcher: field code |
| `0x42D710` | R0001 | field core | **no** | BATE's root dispatcher (the step byte `0x929F00` through `0x64ADAC`): battle engine |
| `0x57DFF0` | R0207 | field core | **no** | another mode's root, same shape, through `0x663DD0` |
| `0x52ADA0` | R0202 | area overlays (hyp.) | yes | a 23-choice prompt state in a nest the catalogue pairs to world-0 area code |
| `0x4790C0` | R0029 | area overlays | yes | clears an 8-record pool; only area code calls it |
| `0x48B850` | R0084 | area overlays | yes | clears a 128-record pool; only area code calls it |
| `0x58D7D0`, `0x58AAF0`, `0x58C2D0` | R0220, R0217, R0218 | text, windows, menus | yes | field-menu screen steps (backdrop, draw, sound, step byte) |
| `0x596330` | R0225 | text, windows, menus | yes | message-box sizing from the script text's control codes |
| `0x59C130` | R0234 | text, windows, menus | yes | a window-slide state of record `0x905B84` |
| `0x461970` | R0012 | text, windows, menus (hyp.) | yes | a list of text rows, beside the config screen's draws |
| `0x44F650` | R0005 | battle engine | yes | a clamped (-25..50) signed-byte adjust in a battle record |
| `0x4CF4B0` | R0107 | battle engine | yes | BMAGIC's map-cell height lookup (the `MapCell_Draw*` callers) |
| `0x4603F0` | R0011 | minigames, master (hyp.) | yes | an item record used up, in the COMMU-paired run |
| `0x580560` | R0209 | shop, inn, save point | yes | a step of the table `0x580300` reads (black screen, a stream load, a wait) |
| `0x5A9860` | R0241 | top-level, platform | yes | copies the key table (`Cfg_Load`'s) |
| `0x537DE0` | R0205 | scenario banks (hyp.) | yes | spawns an effect object at the current sprite, for bank code |

**The misses had patterns, and the tool was fixed for them**: a mapping
error (the `PartyAction*` / `Member_States` tables and the PSX PLP
overlays are field code, `field_hidden.cpp`, not battle); and mode roots
labelled by their only caller, a game-mode frame the catalogue places in
"Field objects" by range - fixed twice over, by the weak family (an
anchor the catalogue placed by address is not structure) and the
dispatches family (a dispatcher votes with its slots). Four of the five
now hold; **R0008 does not** and is reported instead: `0x455450` (boot,
`Boot: unnamed`) is called only by `Area179_Init` and runs the chain
`0x455540..0x456080` on an area change, zeroing `0x9039A0`, `0x904A90`,
`0x937F80`, which the COMMU-paired `0x460510` (R0011) reads. So R0008 and
R0011 look like one system entered from area 179 - labelled `minigames,
master` would match the pairing; what it is in play is a question for the
owner.

**Sample B: 15 functions** drawn at random (seed 20260929, stratified by
class, 3 from the `hypothesis` tier) *after* the fixes: `0x48D9F0`,
`0x500320`, `0x5151A0`, `0x486AB0` (effect draws and states), `0x524450`,
`0x51E4A0`, `0x523B40` (party-action dispatchers and a facing state),
`0x58E2C0`, `0x590020`, `0x596550` (field-menu ability and formation
screens, a window kind), `0x52B330` (area prompt), `0x460510` (the R0011
counts), `0x44FB30` (a battle status bit, party or enemy record),
`0x42D760` (a BATE step dispatcher), `0x5144F0` (a textured quad at the
sprite's map position). **15 held; `0x5144F0` only weakly** - its callers
carry only the catalogue's neighbour label, so "effect objects" rests on
its unit.

**Hit rate, honestly: 41 of 46 read (89%)**, at the tool state each was
checked against; sample A alone 84%, which agrees with the validation's
84.6%. The fixes were made on sample A, so its after-fix 30 of 31 is not
a measurement; sample B is.

## 6. The boot-resident 891, from their tables

`boot_tables.tsv` / `boot_members.tsv`: each boot-resident start's
`.rdata` / `.data` cells, each cell's table bounded by the code that
indexes it (`table_bounds`: from the touched base at or below the cell to
the next touched cell or the run's end).

- **632 sit in 115 tables** (56 named in `symbols.toml`, 59 not; 16 starts
  in two tables). **1,127 slots** in all: **26 are a bare `ret`** (the
  engine's empty handler), **14 repeat** a target inside one table, and
  **152 targets are shared** with another table.
- **259 sit in no `.data` table**: 184 are the switch cases of section 4,
  the other 75 came to part 2 by host (34), a PSX pair (19), range
  (12) or name (10).

The tables that hold most of them (slots / distinct / boot-resident /
ours / reader):

| Table | Name | Slots | Distinct | Boot | Ours | Read by |
|---|---|--:|--:|--:|--:|---|
| `0x655350` | `Effect_KindHandlers` | 187 | 187 | 163 | 18 | `Effect_RunObjects` |
| `0x64E73C` | `Effect_Handlers` | 130 | 130 | 119 | 6 | `Effect_ApplyResult` |
| `0x65406C` | `EffectKind18_States` | 105 | 103 | 93 | 9 | `EffectKind18_Run` |
| `0x654210..0x6542E0` | (13 kinds' state tables inside `EffectKind18_States`' count) | 55 | | 48 | 0 | `0x46D890` ... `0x46FEE0`, one each |
| `0x6609D0` | `Field_ActionBySet` | 19 | 19 | 18 | 1 | `Field_ActionState` |
| `0x66AF94` | `MenuList_Kinds` | 21 | 21 | 15 | 6 | `MenuList_Run` |
| `0x662E1C` | `Field_ObjectTriggers` (read as base - 4) | 66 | 65 | 13 | 52 | `0x56E020` |
| `0x6672EC` | - | 10 | 10 | 10 | 0 | `0x58A3C0` |
| `0x66AFE8` | `MenuList_PanelStates` | 11 | 11 | 9 | 2 | `MenuList_MemberPanel` |
| `0x662CE8` | `Field_ModeTailKinds` | 64 | 64 | 8 | 49 | `Field_ModeTailRun` |
| `0x66B1F8` | `Window_Handler7KindTable` | 19 | 19 | 8 | 11 | `Window_Handler7Kinds` |
| `0x6672B4` | `FieldMenu_States` | 9 | 9 | 7 | 2 | `FieldMenu_Run` |

The rest hold five or fewer each (`FieldCore_State2Steps`,
`ShopMode_States`, `GameMode_Handlers`, `ShopSell_States`, `Member_States`,
`FieldSave_States`, the battle step tables ...). For the coordinator:
`EffectKind18_States`' `count = 160` should be 105, with the thirteen
tables after it named per kind; `Effect_KindHandlers`' 187 is exact (the
run ends at a non-code dword, every slot distinct, one reader).

## 7. A proposed split into rounds

2,047 takeable starts, about 350 a wave, whole bands, each wave on one
harness:

| Wave | What | Starts | `hypothesis` | Harness |
|---|---|--:|--:|---|
| EO1 | effect kinds `0x433640..0x483B00` (the kind entries with their states) | 370 | 44 | `scenario_harness` (a `Sprite_Current` object, FH's `kSprite`) |
| EO2 | effect kinds `0x483BA0..0x4FEC20` | 347 | 42 | the same |
| EO3 | kind 0x18's programs `0x4FEDD0..0x516A90` | 353 | 15 | the same |
| FL | field core: the party sets' actions `0x51B5A0..0x528CC8` and the rest (374, and 2 event-script starts) | 376 | 71 | `scenario_harness` |
| MW | menus, windows, shop: the field menu's screens, the window kinds, `MenuList_Kinds` | 239 | 60 | `scenario_harness` |
| BX | battle engine (`Effect_Handlers`' slots, BATE's root) with the remainder: area, minigames (R0008 + R0011 together), scenario, top-level | 212 + 150 | 33 + 89 | `boss_harness` for the battle half, `area_harness` / `scenario_harness` for the rest |

EO1..EO3 cut at a kind boundary (the `detail` column's unit changes).
The effect kinds have no live route of their own yet; the attract cycle
and the owner's routes will enter some - the tracer's reach switch
([`takeover-queue-field-battle.md`](takeover-queue-field-battle.md)
section 7) measures which, before a wave is staged. BX's remainder is the
wave with the most `hypothesis` rows and the one where reading before
cutting pays most.

## 8. Not verified

- **No label is a name.** The classes say which subsystem a function
  belongs to; nothing here says what any function does in play, and none
  is proposed for `symbols.toml` beyond the TSV's `detail`.
- The `evidence` tier is 93.6% right on our own code, not 100%: about one
  in sixteen of the 1,193 unlabelled `evidence` rows is expected to be a
  neighbouring subsystem.
- The effect-object reading rests on `Effect_KindHandlers` and
  `EffectKind18_States`, both `hypothesis` in `symbols.toml` as to what the
  kinds are; the table structure (entries, state tables, readers) is
  measured, the meaning is not.
- The MP3 decoder's lower bound is the closure's lowest start plus the
  thunk before it; a decoder function reached by nothing at all below
  `0x5ADEA0` would not show - none of the twelve starts there is one
  (section 4).
- Reach: the case / island closure is static; it shows no start is
  unreferenced, not that every one runs.
