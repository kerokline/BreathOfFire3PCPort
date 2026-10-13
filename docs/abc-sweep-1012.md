# The A/B/C sweep of 2026-10-12 night: ten routes on the eu triple, between builds, and the state hash

**Status:** MEASURED (2026-10-12 23:00 .. 2026-10-13 01:00, the owner away;
branch `cleanup/abc-crash-1012`). Every claim below is from the runs named
with it; the frames, logs, heat maps and reports stay under
`C:\Users\kerok\ab` (game-derived, rule 1). Not seen by the owner.

The question: is the game otherwise stable on the three installs of the
`eu` set ([`abc-harness.md`](abc-harness.md) 2: `pc-en150`, `psx-eu`,
`psp-eu`) at tonight's build, did anything regress since the earlier runs,
and where do the items [`visual-findings-1012.md`](visual-findings-1012.md)
9 left open stand.

## 0. What came out

- **No regression.** The PC install's frames are identical to the earlier
  builds' on every route that has an earlier run (attract, combat, shop,
  cutsceneAndNue: 0 differing frames of 810). Every frame that changed on a
  disc-only install between builds is one of section 8's fixes (2).
- **One crash: PSP-EU, `fairyVillage`, recipe frame ~1704** - Capcom's glyph
  trap (`in al, dx`, `0xC0000096`) reached from the village board's help
  line, reproduced on the re-prepared cache (3.1). The other 29 runs ended
  `done`, no `CRASH` line in any log.
- **The PSX-EU route parts in the faerie village** (`fairyVillage`, tick
  3600): the board's area-limit table `0x6536B0` is zero on that image, a
  gap already measured (`loc-build-disc-only.md` 5.4); this is what it
  costs in play (3.2).
- **Two data differences not seen before** on both disc-only installs: the
  field Equip screen's slot icons all one glyph, and the gene window's `AP`
  label missing (3.3, 3.4).
- **Open items:** "You gain 3 EXP!" fixed; the PSP message boxes fixed; the
  shop's walking-ahead gone; the inn's black screen (D255) and the load
  screen's `"%X"` (D254) still open, as expected; the PSP Nue fight no
  longer parts from the others, and no battle page differs PSP against PSX
  during the fight (4).
- **The PSP installs' missing trees** seen in four routes are **D258**
  (fixed at `edc41de3` while this ran): after a re-prepare with those tools,
  cutsceneAndNue's PSX-PSP differing frames fell from 141 to 13 (3.5).
  The crash of 3.1 is not D258's: it recurs on the re-prepared install.
- **The state hash, PC path:** combat, shop and worldMapAndAreaTransition
  against `cn_<route>_orig2` / `_orig2b` - exit 0 / identical / identical on
  the repeat (5).

## 1. Build, root, method

- **Build:** the main checkout's `build/` of 2026-10-12 22:46 (the tree at
  `a7c7a9e5`, then `cleanup/abc-crash-1012`'s tip), in the ini-less launcher
  copy `C:\Users\kerok\ab\L`: SHA-1 prefixes `bof3x.dll` `D89DB1726E29`,
  `bof3x-launcher.exe` `504D4ED34347`, `bof3x.exe` `F0DB819224F4`, each
  equal to the main build's (checked by hash before the first run).
- **Root:** `C:\Users\kerok\ab`, prepared earlier the same evening (stamps:
  `c_pc_eu` 22:05, `c_psxeu` 22:50, `c_pspeu` 22:54). The confirming
  `prepare --set eu` at the start was refused by the session's permission
  check (it names the main checkout's `build/`); currency was checked
  instead by the hashes above and the stamps. After the runs, `prepare
  --set eu --only psp-eu` (with the main build) rebuilt `c_pspeu` and
  `g_pspeu` with `edc41de3`'s tools (3.5); the launcher's three binaries kept
  their hashes.
- **Runs:** `python tools/abc_run.py run --set eu --tag n1012 --route R
  --root C:/Users/kerok/ab`, one game at a time, `--every 30` (the default).
  Reports `report_n1012_<route>.md`, frames `runs/n1012_<route>/`.
  `cutsceneAndNue` also `--statehash --statehash-skip` the union of both
  disc caches' `exe_tables.py gate-skip` lists (1,384 ranges, written to the
  session's scratchpad). Another agent ran the harness in `C:\Users\kerok\abx`
  at the same time (areas 16 / 45; caughFish and masterAndManillo were left
  to it), so the machine was not quiet.
- **fairyVillage** has no `# save` header (the owner ran it with
  `input_run.py --save fairyVillage`), so the harness would load whatever
  each install's slot 0 holds. It ran from a scratch copy with `# save
  fairyVillage` added as a comment (no frame moves). It was recorded under
  `en-US` and plays here under `en-150` on all three sides, so its timing
  can drift from the owner's; the sides are still compared like for like.
- **Between builds:** `abc_compare.py --side A=<earlier run> --side B=<tonight>`
  per install (`runs/bb_<old>_<new>/<install>/`). The earlier runs:
  `eu_*` (harness head `07eae8e4`, before VF), `vf_combat` (`5fff554e`,
  after VF's first batch), `vf2_*` (`5d1b32c1` / `ced85f42`, after the
  second batch's `%d`; PC + PSX-EU only).
- Every recipe save was present (`tools/recipe_saves`: combat, town,
  adult_ryu, nue, bossAndFlash, fairyVillage). No route was dropped.

## 2. The sweep, route by pair

Per pair: frames compared / differing / unexplained / first differing tick /
first unexplained tick / where the scene key parts. A = `pc-en150`,
B = `psx-eu`, C = `psp-eu`.

| Route | A-B | A-C | B-C | Runs |
|---|---|---|---|---|
| attract | 366 / 61 / 31 / 420 / 6120 / - | 366 / 285 / 123 / 420 / 420 / - | 366 / 285 / 123 / 420 / 420 / - | all done |
| combat | 87 / 6 / 3 / 90 / 240 / - | 87 / 18 / 18 / 90 / 90 / - | 87 / 15 / 15 / 90 / 90 / - | all done |
| shop | 105 / 16 / 13 / 90 / 240 / - | 105 / 87 / 74 / 90 / 90 / - | 105 / 84 / 71 / 90 / 90 / - | all done |
| cutsceneAndNue | 252 / 18 / 15 / 30 / 150 / - | 252 / 143 / 143 / 30 / 30 / - | 252 / 141 / 141 / 30 / 30 / - | all done |
| worldMapAndAreaTransition | 71 / 25 / 22 / 90 / 240 / - | 71 / 49 / 22 / 90 / 90 / - | 71 / 45 / 19 / 90 / 90 / - | all done |
| menu_screens | 64 / 36 / 33 / 420 / 570 / - | 64 / 38 / 38 / 420 / 420 / - | 64 / 16 / 16 / 420 / 420 / - | all done |
| field_menu | 55 / 27 / 24 / 420 / 570 / - | 55 / 29 / 29 / 420 / 420 / - | 55 / 23 / 23 / 420 / 420 / - | all done |
| dragonTransform | 145 / 118 / 116 / 60 / 270 / - | 145 / 122 / 122 / 60 / 60 / - | 145 / 27 / 27 / 60 / 60 / - | all done |
| bossAndFlash | 184 / 17 / 14 / 60 / 210 / - | 184 / 156 / 156 / 60 / 60 / - | 184 / 147 / 147 / 60 / 60 / - | all done |
| fairyVillage | 157 / 131 / 128 / 60 / 210 / **3600** | 56 / 55 / 51 / 60 / 60 / - (101 missing) | 56 / 52 / 48 / 60 / 60 / - (101 missing) | **psp-eu FATAL at ~1704** |

The unexplained differences, each opened and looked at, by kind. *Layout*
is a moved region, *data* a changed texture or text, *timing* the same
picture at another moment. Descriptions are of what is drawn.

**On every route (A-B, A-C, B-C):**
- The title (mode 0): the PC's (R) (`pc-title-mark`, explained); the PSP's
  own title page (data: DIV-0103's amendment keeps it; D256 its framing).
  From this branch the `psp-title` known entry explains the latter (5 below).
- The load screen (mode 1, ticks 150..300 or 510..720): the slot number and
  the empty slots' text, three regions at `56,56..128,184` (data: D254's
  `"%X"`; on the PC the port's Chinese empty-record word,
  `visual-findings-1012.md` 9). Unchanged, both open.
- The PSP's battle and item windows: the `L` / `R` shoulder labels at the
  scroll bar (`224,80-240,92` and `224,180-240,192`, data: the PSP's UI page,
  deliberately not a known rule).

**A-B (the PC against the PSX disc alone), beyond those:**
- combat: nothing else; frame `t002610` equal (4.1).
- shop: the inn, 2880..3150, 80% of the frame (timing: D255).
- cutsceneAndNue: one frame, `t005370`, a Chlorine glow's phase on Teepo
  (timing, an effect a frame apart); the save menu at 7020..7350 (D254).
- worldMapAndAreaTransition: the world map's legend panel (`64,20-124,56`,
  1110..1770): the PC's keyboard legend against the disc's button legend
  (data: the port's art, which no disc carries - `importer-transforms.md`'s
  dial-page note: "a note, not a divergence").
- menu_screens / field_menu: Ryu's name drawn as the save's Chinese glyph on
  the PC and blank on the disc (data: the save's name text, not the build);
  the play-time colon at `68,24-72,36` in some frames (timing: its blink);
  **the field Equip screen's slot icons** (3.3).
- dragonTransform: the name glyph as above; **the gene window's `AP`
  label** at `200,76-224,88` on 101 frames (3.4).
- bossAndFlash: `t000510` 75% (timing: the area's fade-in a frame later on
  the PC), three battle frames of a boss effect's phase (timing).
- fairyVillage: **the board's empty slots** from 1350 and the parting at
  3600 (3.2).
- attract: 6120..7020, the second title (data + timing: the port's title art
  in a scene the key logs as mode 2 area 2, which `pc-title-mark`'s mode 0
  does not cover; unchanged from `eu_attract`).

**A-C and B-C (the PSP disc alone), beyond those:**
- The attract's dialogue scenes (mode 4, `196,12-320,84`, 123 frames): a
  logo drawn at the top right (data: the PSP's own title art, D256's page).
- Missing trees in the field, the shop's area 7 and area 23, the world
  map's transition (mode 6, area 7) and the village (D258, 3.5).
- The shop's and inn's backdrops in menu modes (4 / 7): the PSP's area art
  behind a window (data, DIV-0103; the `psp-area-art` rule is mode 2 only
  on purpose). Two regions labelled *moved* (`304,120` +8,-7 and `0,156`
  -8,-8) are pieces of that art, not a layout fault: opened, the windows
  and text sit in the same place on every side.
- menu_screens / field_menu: the sixth tab icon of the field menu
  (`184,40-200,56`, data: the PSP's UI page).
- bossAndFlash: the boss arena's smoke dome (`32,12-140,120`) on every
  battle frame, and the arena's bottom-left wall trim (`0,228-16,240`,
  labelled *moved* +8,+6, a texture difference) - data, the PSP's arena.
- fairyVillage: the population-change board never shows on the PSP side at
  810 (the others show it), then the crash (3.1).

## 3. Findings

### 3.1 PSP-EU crashes in the faerie village (new)

`fairyVillage`, PSP-EU only, at recipe frame ~1704 (the last timed frame
`t001680`): `CRASH 0: exception 0xC0000096 at 0x5CC2429A (bof3x.dll+0x3429A)`,
`ecx 0000FFDE`, `edi FFFFFFDE`, bytes at eip `EC ...`, scene `area 0x00B1,
mode 2`. `llvm-symbolizer` on `L\bof3x.dll` (ImageBase `0x10000000`): eip is
`Text_DrawString` (`text_draw.cpp` 38, `GlyphTrap`); the stack scan names
`Text_DrawAt` <- `CommuBoard_DrawPanel` (`rest_4b.cpp` 1197, the help line:
`MessagePools` by the help word) <- `CommuBoard_MoveGridCursor` <-
`CommuBoard_Dispatch` <- `GameMode_Field`. The glyph index `0xFFDE` is
`-0x22`, which `dialogue-localisation.md`'s table gives for a byte `0x04`
(a byte at or below `0x20` falls into the glyph path with a negative index):
**the board's help message on the PSP-EU install holds a byte the PC's text
format does not treat as a control code** (an inference from the register;
the message was not read). Reproduced after the re-prepare with
`edc41de3`'s tools: the same address and registers (`runs/d258_fairyVillage`).
Classification: data (the PSP-EU text) reaching Capcom's trap - a crash a
player can reach. The PSP side had already parted from the others (no
population board at 810), so the route's inputs were landing in another
menu by then; the help line is drawn whenever the board panel is up.
Next: dump the help message (`BOF3X_TEXTLOG=1` on the PSP-EU run), then
the language layer's handling of that byte - for the coordinator to number.

### 3.2 The PSX-EU faerie village: no area limits

From 1350 (mode 2, area 177) the board's slots draw empty on PSX-EU where
the PC draws two house slots, and from 3600 the scene key parts - the route
plays on a different board. The PSX-EU image holds `0x6536AC..0x6536BB` as a
`gap` (`language-zero`, `CommuRank_Modes+0x538`, `data.toml`), zero; the
PSP-EU image maps it from `BOOT.BIN` and equals the PC's. This is the
area-limit table `loc-build-disc-only.md` 5.4 lists ("every area's limit 0:
placements refused") and `loc-build-psp.md` 6 ("reproduced on all but
PSX-EU"): known, not numbered as a defect. The sweep adds what it costs:
a PSX-EU player cannot place faeries, and the village route parts.

### 3.3 The field Equip screen's slot icons (new)

`menu_screens`, `t001440` and the `equip` shot (mode 3, area 141): on both
disc-only installs every row of the equipment list draws the same flame
glyph (`168,148-180,224`); the PC draws a weapon, shield, helmet, armour and
two accessory icons. The names are right on every side. `visual-findings-1012.md`
2.2 says the field's panel reads `Equip_SlotIcons` `0x663660`, placed on
every build: the PSX-EU image's bytes there equal the PC's (`map`,
`ETC/BATE.EMI#0`). So this screen reads another table - not located here.
Data (a gap of the D252 class), both disc-only installs.

### 3.4 The gene window's `AP` label (new)

`dragonTransform`, 990..3990 (mode 5, area 141, the gene splicing window):
the `AP` before the cost (`200,76-224,88`) is drawn on the PC and blank on
PSX-EU (cropped and compared). The cost digit is drawn on both. Data: a
label no layer writes on the disc-only image (the `"AP%2d"` format filled
by section 8 is another string). Not traced to its address.

### 3.5 The PSP installs' trees: D258

Seen before the coordinator's note: no tree in area 23 on PSP-EU
(cutsceneAndNue 240..390, 3930..4230, the battle backdrop 4230..6720,
6750..7410), on the world map's transition (mode 6, area 7, 720..870), in
the village (fairyVillage 810). Already in `eu_cutsceneAndNue` (`07eae8e4`),
so not a regression. After `prepare --only psp-eu` with `edc41de3`'s tools,
a PSP-EU cutsceneAndNue (`runs/d258_cutsceneAndNue`) against tonight's PC and
PSX-EU frames: **B-C 141 to 13 differing frames, A-C 143 to 28**; the 13 are
the title (4) and the battle's `L` / `R` labels (9). The trees are back.

## 4. Section 9's open items

| Item | Status | Evidence |
|---|---|---|
| "You gain 3 EXP!" (combat `t002610`) | **fixed** | the number drawn on PSX-EU and PSP-EU; `t002610` the only frame of 87 that changed from `vf_combat` on both disc-only installs; tonight's PSX-EU equals `vf2_combat`'s (`5d1b32c1`) on all 87 |
| PSP message boxes at the top left | **fixed** (`boxxy`) | shop `t000630` and attract `t004170`: the box at the bottom on PSP-EU (at the top left in `eu_shop` / `eu_attract`); the shop's A-C frames there differ by 1.7% (art), not the box |
| Shop t420..450, PSX-EU walking ahead | **gone** | tonight's A-B shop differs at 240..300 and 2880..3150 only; 420 and 450 equal |
| The inn's black screen on PSX-EU (D255) | **open**, as expected | A-B shop 2880..3150, 10 frames, worst 80% |
| The Nue divergence on PSP from ~4530 | **gone** | no scene split (was 6750 in `eu_cutsceneAndNue`); B-C in the fight only the `L` / `R` labels and the arena's tree (D258). State hash B-C (`compare/statehash_B-C.txt`, 145 pages): outside the VRAM / CLUT pages, the name / text pages from tick 1, the music and draw pages from 205, `0x904000` at one tick (3229), and `EnemyWorkingRecords` (`0x93B000`, `0x93C000`) only from **6696** to the end - the fight's last ticks (the last battle frame is `t006720`, the first field frame `t006750`), when the result's messages are up. No battle page differs before that. The two pages hold the battle message words (`state-hash.md` 7 item 3), read here as text pointers into different pools - not read to the byte |
| The load screen `"%X"` (D254) | open (the owner's call) | A-B 240..300 on every route |
| Areas 16 / 45 | not run here | the other agent's |

## 5. Between builds

| Route | Install | Earlier | Differing / compared | What changed |
|---|---|---|---|---|
| attract | pc-en150 | `eu_attract` | 0 / 366 | - |
| attract | psx-eu | `eu_attract` | 1 / 366 (`t006390`) | the second title's fade, one frame; the attract is not frame-exact there (`abc-harness.md` 8's PSP repeat: 2 of 366) |
| attract | psp-eu | `eu_attract` | 91 / 366 | the message boxes moved to the bottom (`boxxy`) |
| combat | pc-en150 | `eu_combat`, `vf_combat` | 0 / 87, 0 / 87 | - |
| combat | psx-eu, psp-eu | `eu_combat` | 77 / 87 each | the battle framing and enemy name box (D251), the load screen's TIME (D252) |
| combat | psx-eu, psp-eu | `vf_combat` | 1 / 87 each (`t002610`) | the EXP number (`%d`, D252) |
| combat | psx-eu | `vf2_combat` | 0 / 87 | - |
| shop | pc-en150 | `eu_shop`, `vf2_shop` | 0 / 105, 0 / 105 | - (`vf2_shop`'s PC run had timed out at 900 s; its frames are the same) |
| shop | psx-eu | `eu_shop` | 26 / 105 | the load screen's TIME (240..300) and the shop's item counts (960..2580): D252 |
| shop | psx-eu | `vf2_shop` | 0 / 105 | - |
| shop | psp-eu | `eu_shop` | 48 / 105 | the boxes (`boxxy`) and D252 |
| cutsceneAndNue | pc-en150 | `eu_cutsceneAndNue` | 0 / 252 | - |
| cutsceneAndNue | psx-eu | `eu_cutsceneAndNue` | 92 / 252 | the load screen and the whole fight (D251, D252's equip window) |
| cutsceneAndNue | psp-eu | `eu_cutsceneAndNue` | 180 / 252 | the same, the boxes, and the fight no longer parting |

Every change is one of section 8's fixes (D251, D252, `boxxy`), and none on
the PC install. **No regression.**

## 6. The state hash, PC path

From the ini-less `C:\Users\kerok\ab\L`, the player's install
(`<main>/bof3`, the owner's saves as the references were recorded with), no
language overlay, `BOF3X_OPT=none BOF3X_LAYERING=0`, the 173-range
`tools/statehash_skip.txt`, `python tools/input_run.py tools/recipes/R.txt
--no-front --game <main>/bof3 --launcher C:/Users/kerok/ab/L/bof3x-launcher.exe`
(shop `--slot0-hold`); files `C:\Users\kerok\ab\sh\cn_<route>_n1012.sh`.
`statehash.py check cn_<route>_orig2.sh cn_<route>_orig2b.sh <run>` (the
references under 168 ranges; the tool leaves out the three pages the five
added ranges touch).

| Route | Ticks | Result |
|---|--:|---|
| combat | 2,561 | 1 page, `0x7DE000` (the sound page) at tick 227 only - the same noise `state3-build.md` 5.2 recorded for this pair |
| shop | 3,137 | **identical** on every tick |
| worldMapAndAreaTransition | 2,113 | first run: tick-3 noise, `0x7DE000` on 62 ticks and 47 VRAM-shadow pages over 1130..1212 (the world map's uploads), the other agent's game running beside it; **the repeat identical on every tick**; a fresh run of Capcom's side (`--original "*,-Game_Clock"`) the same hour: 7 pages at tick 3 only. The first run's pages were the loaded machine |

No game-state page differs on any of the three. Not run: the us set
(section 4 of the brief; time went to the crash and the re-prepare).

## 7. For the other files

Not edited here; the coordinator folds them in.

- **`docs/known-defects.md`**, three for numbering: 3.1 (PSP-EU, the
  village board's help line reaches the glyph trap - a crash); 3.3 (both
  disc-only installs, the field Equip screen's slot icons one glyph); 3.4
  (both disc-only installs, the gene window's `AP` label blank). 3.2 is the
  area-limit table already listed in `loc-build-disc-only.md` 5.4; a number
  for it, now that its cost in play is seen, is the coordinator's call.
- **`recipes/abc-known.toml`**: `psp-title` added (DIV-0103's amendment,
  D256), mode 0 area 0 only. Checked on tonight's combat frames: A-C and B-C
  unexplained 18 to 15 and 15 to 12, the three title frames now explained.
- **`docs/abc-harness.md`** 9: a recipe without a `# save` header loads the
  install's own slot 0 (fairyVillage); the harness could refuse such a
  recipe or take `--save`.
- **`docs/visual-findings-1012.md`** 2.2's sentence that the field's panel
  is right holds for `0x663660`, not for the Equip screen (3.3).
