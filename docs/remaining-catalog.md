# What is not ours, by what it is part of

**Status:** IN PROGRESS (2026-09-25)

The takeover queues so far were built from *reach*: what a traced run entered
that was not ours ([`call-trace.md`](call-trace.md) §9,
[`attract-remaining.md`](attract-remaining.md), the round docs). That answers
"what next" one route at a time and says nothing about the 9,000 functions no
route has entered. This document is the other axis: **every function of
`BOF3.exe` that is not ours, labelled by the part of the game it belongs to**,
with what each label rests on, so the next waves can be shaped by subsystem
rather than by whichever route was recorded last.

`tools/remaining_catalog.py` produces it. The per-function tables are
`analysis/remaining_catalog.md` and `.tsv` (game-derived, never committed -
CLAUDE.md rule 1); the counts below are its output of 2026-09-25, unedited.

## 1. The numbers

**9,241 functions of 10,246 starts are not ours.** The universe is every
recorded function start (`analysis/pc_funcs.json`, 2,952) plus every
pointer-reached start `pe_hidden.py` found (`analysis/pc_hidden.json`, 7,294);
"ours" is an `impl` line in `symbols.toml` (1,024 - `Fmv_WndProc` `0x59E570`
is detoured without one, so it is counted here as not ours; HANDOFF item 1).

| Part | Group | Functions | Hidden | KiB | Attract | Shop | World map | Combat | Any + | Any ~ | Named | Paired |
|---|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| 0 Platform | MSVC CRT | 242 | 10 | 41 | 100 | 115 | 115 | 115 | 115 | 1 | 12 | 0 |
| 0 Platform | MP3 decoder | 200 | 94 | 54 | 77 | 77 | 77 | 77 | 77 | 37 | 8 | 0 |
| 0 Platform | Not functions | 66 | 64 | 9 | 7 | 3 | 4 | 2 | 7 | 59 | 1 | 0 |
| 0 Platform | Platform set-up | 11 | 6 | 2 | 11 | 11 | 11 | 11 | 11 | 0 | 1 | 0 |
| 0 Platform | Windows shell | 9 | 1 | 1 | 8 | 8 | 8 | 8 | 8 | 1 | 6 | 0 |
| 0 Platform | Task system | 7 | 1 | 0 | 6 | 7 | 7 | 7 | 7 | 0 | 5 | 0 |
| 1 Library layer | Renderer | 44 | 5 | 15 | 0 | 0 | 0 | 0 | 0 | 4 | 4 | 0 |
| 1 Library layer | PSX library layer | 18 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 4 | 0 |
| 1 Library layer | Sound | 10 | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 2 | 2 | 0 |
| 2 Boot-resident game code | Boot: battle | 235 | 233 | 24 | 0 | 0 | 0 | 42 | 42 | 191 | 1 | 1 |
| 2 Boot-resident game code | Table `Effect_KindHandlers` | 177 | 177 | 8 | 0 | 0 | 2 | 1 | 3 | 0 | 0 | 0 |
| 2 Boot-resident game code | Boot: field, map and sprites | 148 | 142 | 10 | 0 | 0 | 2 | 0 | 2 | 140 | 5 | 1 |
| 2 Boot-resident game code | Boot: unnamed | 51 | 14 | 6 | 0 | 0 | 2 | 5 | 6 | 1 | 0 | 38 |
| 2 Boot-resident game code | Boot: party state, items and saves | 47 | 40 | 4 | 0 | 10 | 2 | 8 | 16 | 23 | 1 | 6 |
| 2 Boot-resident game code | Top-level modes | 41 | 21 | 4 | 0 | 3 | 7 | 0 | 10 | 12 | 0 | 0 |
| 2 Boot-resident game code | Boot: top-level modes and tasks | 33 | 32 | 12 | 0 | 2 | 8 | 3 | 9 | 21 | 1 | 0 |
| 2 Boot-resident game code | Boot: text, windows and menus | 14 | 13 | 1 | 0 | 1 | 0 | 11 | 12 | 2 | 1 | 0 |
| 2 Boot-resident game code | Text and windows | 7 | 5 | 2 | 0 | 2 | 0 | 0 | 2 | 1 | 0 | 0 |
| 2 Boot-resident game code | Boot: platform and utilities | 2 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 1 | 0 |
| 3 Field modes | Event script | 186 | 115 | 29 | 0 | 7 | 20 | 6 | 27 | 63 | 1 | 0 |
| 3 Field modes | Field core (`GAME.EMI`) | 166 | 133 | 23 | 0 | 4 | 7 | 11 | 11 | 1 | 0 | 90 |
| 3 Field modes | Shop / inn / save point (`SHOP.EMI`) | 109 | 94 | 22 | 0 | 45 | 0 | 0 | 45 | 0 | 0 | 56 |
| 3 Field modes | Field objects | 78 | 34 | 17 | 0 | 1 | 10 | 4 | 14 | 13 | 20 | 0 |
| 3 Field modes | Field menu (`START.EMI`) | 60 | 46 | 13 | 0 | 0 | 6 | 0 | 6 | 11 | 0 | 4 |
| 3 Field modes | Map and draw layers | 15 | 12 | 4 | 0 | 7 | 9 | 3 | 9 | 3 | 0 | 0 |
| 3 Field modes | Sprite draw | 7 | 7 | 2 | 0 | 0 | 0 | 0 | 0 | 7 | 0 | 0 |
| 4 Battle | Battle magic effects (`BMAGIC` overlays) | 1,094 | 894 | 214 | 0 | 0 | 0 | 14 | 14 | 2 | 0 | 138 |
| 4 Battle | Boss battle scripts (`BOSS` overlays) | 427 | 415 | 31 | 0 | 0 | 0 | 1 | 1 | 37 | 0 | 215 |
| 4 Battle | Battle engine (`BATTLE.EMI`) | 376 | 297 | 61 | 0 | 0 | 2 | 100 | 102 | 59 | 0 | 301 |
| 4 Battle | Battle result (`BATL_END.EMI`) | 25 | 20 | 5 | 0 | 0 | 0 | 17 | 17 | 1 | 0 | 22 |
| 4 Battle | Battle extra (`BATE.EMI`) | 16 | 13 | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 6 |
| 4 Battle | `BATL_OVR.EMI` | 12 | 8 | 2 | 0 | 0 | 0 | 4 | 4 | 0 | 0 | 8 |
| 5 Area overlays | World 2 | 407 | 334 | 59 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 194 |
| 5 Area overlays | World 3 | 377 | 301 | 49 | 0 | 0 | 1 | 0 | 1 | 0 | 0 | 207 |
| 5 Area overlays | World 1 | 358 | 297 | 43 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 179 |
| 5 Area overlays | World 4 | 357 | 297 | 51 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 190 |
| 5 Area overlays | World 0 | 266 | 214 | 39 | 0 | 0 | 10 | 1 | 11 | 11 | 0 | 196 |
| 6 Scenario and character sets | Scenario event banks (`SCENA` overlays) | 329 | 267 | 128 | 0 | 0 | 0 | 0 | 0 | 3 | 0 | 62 |
| 6 Scenario and character sets | `COMMU` overlays | 205 | 145 | 38 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 70 |
| 6 Scenario and character sets | `SHISU` / `SISYOU` | 58 | 44 | 10 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 7 |
| 6 Scenario and character sets | Scenario effects (`SCE1xEF` overlays) | 52 | 42 | 10 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 5 |
| 6 Scenario and character sets | Party character sets (`PLP` overlays) | 20 | 17 | 3 | 0 | 0 | 1 | 0 | 1 | 0 | 0 | 2 |
| 7 Unlabelled | Unlabelled | 2,879 | 2,178 | 532 | 0 | 22 | 14 | 45 | 77 | 36 | 0 | 0 |
| | **Total** | **9,241** | **7,085** | **1,583** | | | | | | | | |

Columns. **Hidden**: a start `pe_funcs.py` did not record, reached only through
a pointer. **KiB**: extent to the next start, an upper bound. **Attract .. Combat**:
functions a traced run entered - for the recorded starts the all-calls runs
(`hidden_b`, `all_a`, `all_b`; `recipe_*`), for the hidden starts the
first-call runs over `entries_hidden.txt` (`hidden_a`; `hidden_shop`,
`hidden_worldmap`, `hidden_combat`, 2026-09-25, §4). **Any +**: entered by at
least one of them. **Any ~**: not entered by any, but the recorded function
it was folded into was - kept because it was the first draft's only reach
measure for hidden starts, and §4 shows what it was worth. **Named**:
`symbols.toml` names it. **Paired**: `tools/psx_pair.py` pairs it with a PSX
function.

Read the reach columns first: **of the 9,241, 442 game functions were entered
by a run on record** (the platform rows aside - CRT, MP3, shell, task system,
set-up, which are I8 / I12's and not per-function). 35 of them are recorded
starts, the tail of the routes; 407 are pointer-reached functions the
routes' all-calls traces could not see, and the first-call traces of §4 did.
The battle engine holds a quarter of them (102 in `BATTLE.EMI`, 42 in the
boot-resident battle code, 17 in `BATL_END`); the shop overlay 45; the event
script 27. Everything else is beyond the routes, and its waves are either
fuzz-only or need routes recorded for them.

## 2. Where each label comes from

Each function takes the first of these that speaks; the source is kept beside
it in the tables (`how`), so a reader can weigh the label.

| Source | Functions | What it is |
|---|--:|---|
| pair | 1,998 | `analysis/pairs_propagated.json` puts it in a PSX overlay; the sibling's `names/overlays.toml` gives the overlay's family and role, and `names/functions.toml` / its `symbols.toml` may name it |
| neighbour | 1,962 | the nearest labelled function below and above agree and are within `0x3000` bytes - the linker kept the source's file order ([`SHARED_SOURCE.md`](SHARED_SOURCE.md) §2), so a run of starts between two functions of one file is usually that file |
| host | 1,341 | a hidden function inherits the label of the recorded function it was folded into |
| range | 874 | an address range `attract_catalog.py` already names: CRT, MP3, shell, task system, set-up, sound, renderer, PSX library layer, and the field-side ranges of [`attract-remaining.md`](attract-remaining.md) §4 |
| table | 177 | a hidden function whose pointer sits in a table `symbols.toml` names - here only `Effect_KindHandlers` `0x655350`, whose 187-slot count is itself an upper bound ([`frame-callees.md`](frame-callees.md)) |
| name | 10 | `symbols.toml` names it but it is not ours; the name's prefix |
| none | 2,879 | nothing above speaks |

Two things to keep in mind when reading a label:

- **A pair label is the PSX side's word for where the twin lived.** On the
  PlayStation an area's handlers, a spell's effect and a scenario bank were
  each an overlay loaded on demand; on the PC they are all linked into one
  image. So "Area overlays, world 2" means *functions whose PSX twins are in a
  `WORLD02/AREAnnn.EMI`* - one file per area, a handful of handlers each - not
  one subsystem. The pair methods' measured precision is in
  [`attract-remaining.md`](attract-remaining.md) §5.1; the neighbour fill is
  weaker than any of them and is counted separately for that reason.
- **The boot-resident groups are named by prefix** of a PSX or PC name
  (`Battle_*`, `Msg_*`, ...), and 233 of "Boot: battle"'s 235 are hidden
  functions inheriting a host's name - 121 of them the effect handlers after
  `Effect_ApplyResult` ([`battle_damage.md`](battle_damage.md)), 30 after
  `BattleFx_RollingDigits`, 18 after `Battle_ActorSkipped`. A host label says what file the function sits in, not what it does.

### The 2,879 nothing labels

They are 82 address runs, 2,178 of them hidden, and **no PSX pair lands in
any of them** - that is what left them unlabelled. 77 of them the routes
enter (section 4), most in the hosts of battle and shop code. The sibling captured 406
overlays out of what the disc holds, so the likeliest reading is that these are
the PC twins of overlays never captured: spells never cast, areas never
entered, scenario banks never played. The tool gives each run two things to
weigh that by:

- **Touches and calls**: the named data the run references (by
  `analysis/pc_xref.json`) and the named functions it calls, by count. The
  large runs read plainly: `0x4A33E0`..`0x4FB6E4` (nine runs of twenty or more, 834 functions)
  touch `DamageScratch`, `Sprite_Current` and `Math_Sin` / `Math_Cos` and set
  primitives - the shape of the paired `BMAGIC` code; `0x53D3B0`..`0x56AD79`
  (six runs, 213) touch `WindowRecords`, `MoveScript_Var7` and
  `Field_Request` and call `Msg_OpenScript`, `Party_DropIn` and
  `Scenario_CallA` - the shape of the paired `SCENA` banks; `0x58CD40` (55)
  touches `WindowRecords`, `Input_Pressed` and `Item_EquipMask` and calls
  `Menu_DrawBackdrop` - the field menu; `0x51DE90`..`0x525370` (225) and
  `0x51C270` (37) call `Sprite_EnsureAnimation`, `Msg_OpenSystem` and
  `Sound_PlayEffect` over `Field_State` - field-side.
- **A profile hint**: the label whose functions touch and call the same names
  (cosine similarity over IDF-weighted counts). **Measured before use**,
  leave-one-run-out on the 127 labelled runs of three or more starts: it gets
  37 back at or over 0.5, and when it does answer it is wrong about two fifths of
  the time (the confusion list is in the output). So it is written beside each
  run as a hint and never into the label column. For the record, on the runs
  above it says `BMAGIC` at 0.59..0.71 for the nine, `SCENA` at 0.60..0.82 for
  the six, `START.EMI` 0.71 for `0x58CD40`, and `PLP` 0.81..0.86 for the two
  field-side runs - which agrees with the touches, and is still a hint. For
  the menu-draw runs `0x596330`..`0x59CCFC` it has nothing over 0.33: there
  the calls (`Text_DrawAt`, `Menu_DrawBox`, `Menu_DrawPieces`) are the only
  evidence.

## 3. The shape this gives the next waves

By the numbers, in the order the work can be checked, not the order of size:

1. **What the routes reach already - 442 functions, checkable today.** The
   `Any +` column, platform rows excluded: `BATTLE.EMI` 102, the unlabelled
   77 (mostly hidden battle and shop code by their hosts), `SHOP.EMI` 45,
   the boot-resident battle handlers 42, the event script 27, `BATL_END` 17,
   party state 16, field objects 14, `BMAGIC` 14, field core 11, world 0's
   areas 11, the rest under ten. 407 of the 442 are pointer-reached
   functions the all-calls traces never armed (section 4), which is why
   rounds six and seven closed their queues at zero with these still
   Capcom's. Each has a live check the day it is taken: this is round
   eight's queue, and it is bigger than round seven's (209).
2. **The field menu - a route that already exists and was never traced.**
   `tools/recipes/menu_screens.txt` walks Items, Ability, Equipment, Tactics
   and Status ([`input-script.md`](input-script.md)). Traced all-original it
   would arm `START.EMI`'s 60, the `0x58CD40` run's 55 and, by their calls,
   the menu-draw runs `0x596330`..`0x59CCFC` (128) - about 240 functions with a
   live check, the cheapest new reach on the list. Same for
   `config_screen.txt`, `load_list.txt` and `field_menu.txt`.
3. **`SHOP.EMI`'s other 64.** The shop route entered 45 of its 109 (item
   1); the overlay also holds the inn and the save point, by the sibling's
   alias for it. A recipe through an inn and a save (X's `Save_QuickWrite`
   `0x5809C0` is on HANDOFF's unowned list) is the route for the rest.
4. **The battle engine's remainder - 274 in `BATTLE.EMI` the combat route
   does not enter** (item 1 has the 102 it does). Round seven's docs name
   what the route does not reach: boss encounters, the event-battle paths, a
   full task table. `BATL_END`'s other 8, `BATE` 16 and `BATL_OVR`'s other
   8 go with it. This is the group
   where a second recorded fight - a boss, from a save the owner picks - pays
   most per minute of route.
5. **The battle magic effects - 1,094 paired plus ~830 hinted by touches, the largest
   single block, 214 KiB paired alone.** One overlay per spell on the disc,
   reached by casting it. There is no route that reaches many at once; but
   they share one shape (the hints separate them from everything else at
   0.6+), so this is the candidate for a **class harness** - one fuzz set-up
   with recording stand-ins for the primitive setters and `DamageScratch`,
   run across the block - with a live check per spell as the cheats
   (DIV-0045) allow. A wave of *tooling* before a wave of functions.
6. **The area overlays - 1,765 paired across five worlds, ~10 per area.**
   Each area's handlers are reached by entering the area, and only then. Like
   the spells, one shape many times: the area descriptor table `0x667590`
   gives every area's handler array and init, so a harness can call each
   handler under a fuzzed sprite and map state. A recorded walk through one
   world gives the live check for its areas; 11 of world 0's sit in hosts the
   attract cycle entered, none was entered itself.
7. **The scenario banks - 329 paired plus ~210 hinted by touches, and 128 KiB paired,
   the largest bytes per function on the list**, plus `COMMU` 205,
   `SHISU` / `SISYOU` 58, `SCE1xEF` 52, `PLP` 20. Story code: reached by
   playing the story, so the routes are saves at chapters, which the owner
   records. Furthest from a check today; last by that measure.
8. **The boot-resident hidden functions - `Effect_KindHandlers`' 177 and the
   ~470 in `Boot:` groups.** Handler tables the frame loops call through; each
   reached when its kind occurs. Fuzz-only until a route hits the kind;
   the round docs' "fuzz only" lists are the precedent.

What this does **not** shape: the platform rows (I8 / I12 and the owner's
scope) and the library layer's 72, which go with their callers.

## 4. The expectation, checked: the routes under the hidden entry list

The first draft of this document (the morning of 2026-09-25) had reach for
the 7,294 pointer-reached starts only by proxy: "the recorded function it was
folded into was entered" (`Any ~`), an upper bound. The owner asked for the
measurement: the three recorded routes played once each, all original, under
a **first-call trace over `entries_hidden.txt`** - the `hidden_a` method of
[`attract-remaining.md`](attract-remaining.md) §1 - by
`analysis/trace_hidden_recipes.sh` (7,085 armed, 210 left unarmed as owned).
Each route replayed to the same recipe frame as its all-calls trace (shop
3,157, world map 2,143, combat 2,620), so it is the same route.

| Route | Recorded starts entered | Hidden starts in those hosts (`~`) | Hidden starts entered (measured) | Of them inside an entered host |
|---|--:|--:|--:|--:|
| attract (`hidden_a`) | 539 | 427 | 89 | 62 |
| shop | 401 | 267 | 125 | 38 |
| world map | 262 | 277 | 95 | 30 |
| combat | 435 | 589 | 289 | 139 |
| any | | 1,130 | 496 | 249 |

**The proxy was wrong in both directions.** Of the 1,130 hidden starts in an
entered host, 881 were never entered; of the 496 entered, 247 sit in a host
whose own entry no run called. That is what a pointer-reached function is: a
handler the frame loop calls through a table, not through the function that
happens to precede it in the file. 62 of the 496 are ours already (owned
since their host was read); **434 are not**, 407 of them game code, and they
are item 1 of section 3. `Any ~` stays in the table only to show what it was
worth.

Two things came out of the runs besides the numbers:

- **A crash of the current build on the A/B original side**, every run,
  tracer or not, at the combat route's first command phase: our
  `Text_DrawString`'s glyph guard (DIV-0016) is set by our
  `Font_SetGlyphData`, which the `*,KEEP` side leaves Capcom's, so the
  bound stayed at the shipped `0xA00` while the English font reaches
  `0xA6E` - and DIV-0052's EX suffix, glyph `0xA6B`, tripped it
  (`in al, dx` at `bof3x.dll+0x7A8A`, `ecx` = `0xA6B`, the banner text
  `0x904EC5` in `ebp`). The all-ours side played the route through. No A/B
  had played the combat route since DIV-0052 landed. Fixed by setting the
  bound in `LoadDatFile` as well, which is ours on both sides (DIV-0016,
  amended); the combat trace above is with the fix. The trap is in
  [`HANDOFF.md`](HANDOFF.md).
- **An all-original side cannot write its own frames** (no Direct3D 11
  device for `render::SaveFrame`), so the runner grabs the window at each
  frozen shot, and a covered screen hung one run at its shot. The script
  plays the routes' shot-less files; a trace needs no captures.

## 5. Limits

- **Reach is measured for every start now, once per route.** A first-call
  trace says entered or not; the all-calls counts are for recorded starts
  only. Rerun `trace_hidden_recipes.sh` after a wave lands, since the tracer
  never arms an owned function and the hosts' owned ranges move.
- **Sizes are extents to the next start**, so any function `pe_hidden.py`
  still misses (the inline-jump-table case, §3 there) inflates its
  predecessor and is absent from the count. The 10,246 is a floor.
- **The neighbour fill and the host label are file-level claims**, not
  function-level ones, and the profile hint is a hint. A wave built on a group
  should expect the round docs' usual finding - some of the group turns out to
  be something else (round four's P, "queued as the title's hidden cluster,
  found to be the field menu's item use").
- The catalogue is a snapshot of `symbols.toml`'s `impl` lines; rerun it after
  a wave lands.

## 6. Regenerating

```
python tools/remaining_catalog.py
```

reads `analysis/pc_funcs.json`, `analysis/pc_hidden.json`,
`analysis/pairs_propagated.json`, `analysis/pc_xref.json`, `symbols.toml`, the
sibling's `names/overlays.toml`, `names/functions.toml` and `symbols.toml`, and
the traced runs under `analysis/calltrace/` (the all-calls `recipe_*` and the
first-call `hidden_*`); writes
`analysis/remaining_catalog.md` (a summary, then one section per group: address
runs with touches, calls and hint, then every function with its extent, host,
label source, name or PSX twin, and per-route reach) and
`analysis/remaining_catalog.tsv` (one row a function, for sorting).
