# The A/B/C sweep's defects of 2026-10-12 night: causes and fixes

**Status:** MEASURED (2026-10-13; the owner away; branch off
`cleanup/abc-crash-1012` `34024c93`). Section 3 is D260..D263 of
[`abc-sweep-1012.md`](abc-sweep-1012.md) 3.1..3.4. Every claim cites the
run, dump or tool output that produced it; the frames, logs, dumps and
caches stay under `C:\Users\kerok\abx` and `C:\Users\kerok\ab` (game-derived,
rule 1). Not seen by the owner.

## 3. D260..D263: four disc-image gaps, four `twin` rows

All four are the D252 class: a short run of `.data` that no map places on a
disc-built image (`recipes/exe-gaps.tsv`), carried verbatim by the overlay
whose code reads it. Each is filled by a `twin` row (`tools/psp_exe.py`
`TWINS`, [`visual-findings-1012.md`](visual-findings-1012.md) 5); no `src/`
change. `TWINS` gains an optional sixth field, the n-th copy in the file,
for D262.

| Defect | Range | Rule | Builds whose gap it closes | Disc copy (PSX-EU / PSP-EU) |
|---|---|---|---|---|
| D260 | `0x652AF8..0x652AFC` | `twin`, `ETC/COMMU01.EMI` | all seven | `#7@0x801D4B2C` / `#7@0x801D4BCC` |
| D261 | `0x6536AC..0x6536BB` | `twin`, `ETC/COMMU00.EMI`, PSX-JP and PSX-EU only | PSX-JP, PSX-EU | `#0@0x801F2918` / (placed by the map from `BOOT.BIN`) |
| D262 | `0x66386C..0x663872` | `twin`, `ETC/STATUS.EMI`, its second copy | all seven | `#0@0x801ECB24` / `#0@0x801ECA94` |
| D263 | `0x66AF38..0x66AF3C` | `twin`, `BATTLE/BATTLE.EMI` | all seven | `#15@0x800B6D1C` / `#15@0x800B6C5C` |

`psp_exe.py recipe` over the seven discs: every new row "0 not BOF3.exe's"
(23 rows: 7 + 2 + 7 + 7). The disc addresses are that run's; where each
run sits on every disc, and how many copies, is from a scratch probe over
`exe_twins.sources` (addresses and counts only).

### 3.1 D260: the PSP-EU crash in the faerie village

**The crash, read from the dump.** The sweep's re-run crash
(`C:\Users\kerok\ab\runs\d258_fairyVillage`, its minidump in `ab\L`, read
with a scratch reader): `esi` = `0x803ACA`, the text being drawn, is
`MessagePools` (`0x803580`) + `0x54A`; the help word `0x675F7A` is **0**,
and `MessagePools`' offset word 0 is `0x54A`. So the board's help line was
message 0 of area 177's script pool - a dialogue message, whose first byte
is a control code (`0x04`, confirming the sweep's inference) that
`Text_DrawAt` takes as a glyph with a negative index, into `GlyphTrap`
(`text_draw.cpp`).

**Where help 0 came from.** `CommuBoard_PickList` (`0x457DD0`, `rest_4b.cpp`:
the job list opened from a slot) sets the help to the u16 `0x652AF8[row]`
every frame it runs; `CommuBoard_PickHelp` (`0x459E50`) reads the same words
as `0x652AF0[kind]` for kinds 4 and 5. The images against `BOF3.exe` (a
scratch byte compare, equal / zero / other per byte): the board's range
`0x652A00..0x652CA0` equal **except `0x652AF8` and `0x652AFA`, zero on
PSX-EU and PSP-EU alike**; `recipes/exe-gaps.tsv` had `0x652AF8..0x652AFC`
as `gap disc-unplaced CommuTail26_States+0x14` on all seven builds. The map
placed the table's other sixteen bytes from `COMMU01.EMI#7` at `+4`
(PSP-EU `data.toml`: `0x652AFC..0x652B0C map ETC/COMMU01.EMI#7@0x801D4BD0`)
and missed the first four because the PC words before them are code
pointers the disc does not share (`exe-layout.tsv`:
`CommuTail_LoadSoundBank`, `CommuTail26_End`) - D252's shape. The probe
finds the four bytes once in `COMMU01.EMI` on every disc, directly before
that placed run.

So pick rows 0 and 1 (and slots of kinds 4 and 5) asked for message 0 on
**every disc-built install**. PSX-EU has the same gap; it never reached the
list in the sweep because D261 refused its placements and the route parted
first. The PC path reads `BOF3.exe`'s bytes and is not affected.

**The drift before it** (no population board at tick 810 on PSP-EU) is
**D259**, not this: the board is the sign's cell hook in area 177, and
`Area_CellHooks` was zero on PSP-EU before `cellhk`. The sweep's first run
(`n1012_fairyVillage`, a cache from before `edc41de3`) shows no board at
810; its re-run on the cache rebuilt with `edc41de3`'s tools
(`d258_fairyVillage`; `c_pspeu`'s `data.toml` has `0x662F28 place
ETC/GAME.EMI#0:cellhk`) shows the board at 840 and still crashes at the
pick list. D259's "not on either route" now has a route.

**Fix:** the `twin` row. Not a language-layer fault: the PSP text holds no
byte the engine refuses; the board asked for the wrong message.

### 3.2 D261: the village's area limits on PSX-EU

`CommuBoard_PickHelp` and the other `rest_4b` readers compare a slot with
the byte `0x653601 + Game_AreaNumber` (`0x6536B0..0x6536BA` for areas
175..185); zero puts every slot past the limit. **A disc carries it
verbatim:** the probe finds `0x6536AC..0x6536BB` in `ETC/COMMU00.EMI#0` and
in section 14 of each village area overlay (`WORLD04/AREA175..185.EMI`, the
shared community module), at one address per disc, on all seven discs -
`0x801F2918` on PSX-EU, the place PSX-US's map takes it from (`c_psxus`
`data.toml`: `0x6536AC..0x6536C0 map ETC/COMMU00.EMI#0@0x801F2918`). Only
the PSX-JP and PSX-EU maps miss it, so the row is limited to those two
builds. The first four bytes are the last faerie record's field before the
table (`loc-build-disc-only.md` 5.4's `FAERIE_UNNAMED`); they are in the
gap row and equal the PC's.

### 3.3 D262: the field Equip screen's icons

The screen of `menu_screens`' `equip` shot is `Menu_DrawEquipCompare`
(`0x574EC0`, `field_o.cpp`), which reads its icons at `0x66386C`
(`field_o_callees.h` `kEquipIcons`), not `Equip_SlotIcons` `0x663660`
(`Menu_DrawEquipPanel`'s). `0x66386C..0x663872` was a `game-value` gap on
all seven builds. `ETC/STATUS.EMI#0` holds the six bytes twice on every
disc (US and EU `0x801EC950` and `0x801ECB24`, PSP-EU `0x801EC8EC` and
`0x801ECA94`); a scan of that overlay's MIPS for `lui` + `lbu` pairs forming
them (US disc, scratch) finds one reader of the first (`0x801DC0D4`, the
panel's single loop: `0x663660`'s twin) and five of the second
(`0x801DEA50..0x801DEBD0`, the compare panel's rows). So the row takes the
second copy, through `TWINS`' new sixth field. Both copies equal the PC's
bytes; this picks the reader's source, not different bytes.

Left: the PSX builds' `0x663866..0x66386C` (`Menu_ButtonSets+0x2A`, inside
the old gap row) is now classed `unread` by `exe_own measure`; not looked at.

### 3.4 D263: the gene window's AP label

`GeneWin_DrawGrid` (`0x598A30`, `battle_e7.cpp`) draws the label at
`0x66AF38` (`kApLabel`, `Text_DrawFont12`), then the cost by `0x64D3EC`.
The cost's format was placed (both images equal the PC's there); the label
was a `game-value` gap on all seven builds (`Win2_TitleBoxStates+0x38`).
`BATTLE/BATTLE.EMI#15` carries it once, and the 18 bytes
`0x66AF38..0x66AF4A` (the label and the choice pieces after it) match there
as one run on every disc.

### 3.5 Runs (harness root `C:\Users\kerok\abx`, the eu triple, this branch's build and tools)

`abc_run.py prepare --set eu --installs abx\installs.toml --build <worktree>/build`
rebuilt the three caches (704 s). The new images against `BOF3.exe`:
`0x652AF0..0x652B10`, `0x663860..0x663880` (but PSX's `0x663866`) and
`0x66AF30..0x66AF40` equal on PSX-EU and PSP-EU; `0x6536A2..0x6536C0` equal
on both (the two bytes before are a name, written at load).

| Route (tag `fx`) | Before (the sweep, `n1012_*`) | After |
|---|---|---|
| fairyVillage | PSP-EU FATAL at ~1704; PSX-EU's scenes part from the PC's at 3600; A-C / B-C 101 frames missing | **all three `done` at recipe frame 4719, no `CRASH` line in any log**; no pair's scenes part; at 1710 both disc-only sides show the pick list with its help line; at 1350 PSX-EU's board draws the two house slots like the others |
| menu_screens | equip region `160,140-184,228` at `t001440`: PC against either disc 1,470 differing pixels (crop at 2x) | **0** on all three pairs, at `t001440` and in the `equip` shot |
| dragonTransform | the AP label `200,76-224,88` at `t001500`: 616 pixels PC against either disc | **0**; of the 101 gene-window frames 990..3990 none differs there past 16 pixels but `t002700` / `t002730` PC against PSX-EU: a battle effect a frame apart with the window gone (cropped, looked at) |

Whole-route differing frames (A-B / A-C / B-C): fairyVillage 89 / 95 / 92
of 157 (the sweep: 131 of 157, 55 of 56 and 52 of 56 with 101 missing),
menu_screens 36 / 38 / 16 of 64, dragonTransform 119 / 123 / 29 of 145 -
the other differences the sweep listed. What is left in the village,
opened (A-B; A-C the same plus the PSP's art):
- `236,44-264,52`, 810..930: the population board's page counter, the
  `"%d/%d"` `0x6531F4` (engine-own, `loc-build-disc-only.md` 5.4).
- `80,76-92,92` on every board frame and the faerie pop-ups'
  `52..228,80..100`: **the food counter (84 on the PC, 34 on both discs) and
  the faeries' stat bars (drawn on the PC, black on both discs)** - the
  faerie stats `0x653210 + 20 r`, zero on both images (5.4: game values
  "no chunk kind carries"). Not only a picture: the simulation's traits are
  zero, so the village runs differently. **Seen in play here, not fixed** (a
  fill from `COMMU00`'s nine-byte records would be a new rule shape) - for
  numbering.
- PSP-EU only: the "Culture" label beside the counter is blank (PSX-EU
  draws it). Not traced.
- Area 65 (the world map, 330..600 and 4650..): the PC's legend art, known.

**A harness trap met:** the first `fx` run's PC side ended "done at frame
5519" after 1 s with no frames: `input_run` read the `recipe done` line the
previous run (masterAndManillo, 5519 frames) had left in `abx\L\bof3x.log`
before the new game rewrote it. Re-run alone (`--only pc-en150 --tag fxpc`,
202 s, 157 frames) and compared with `abc_compare.py` against the `fx`
disc sides (`runs\fx_fairyVillage\compare_abc`). For `abc-harness.md` 9.

### 3.6 Checks

| Check | Result |
|---|---|
| `exe_tables recipe`, `psp_exe recipe`, `exe_tables recipe`, `exe_own measure`, `exe_tables gaps`, the seven discs | every `psp_exe` row "0 not BOF3.exe's"; `exe-gaps.tsv` loses the 23 gap rows, and the PSX builds' `0x663866..0x663872` becomes `0x663866..0x66386C unread`; `exe-layout.tsv` loses the number word `0x652AF8` (now placed); nothing else |
| `importer.py check`, `exe_tables.py check`, `psp_exe.py check` | 0 errors / OK (`exe_tables`: 18,833 pointer words, the layout 19,217 words, 384 numbers) |
| `ledger_check.py` | 114 entries, 0 errors, 2 notes |
| exe host self-test (`BOF3X_SELFTEST_ONLY=1`, `abx\L`, no ini) on `abx\g_pspeu` and `abx\g_psxeu` | exit 0 both; `inject: 10088 ours`; `self-test only: done`; PSX-EU `place 708 ranges 6551 bytes`, `gap 282 ranges 3301 bytes` |
| `BOF3X_SHADOW='*'` self-test | not run: no `src/` change |

### 3.7 For the other files

- **`docs/DIVERGENCE.md`**, DIV-0101 (the gaps entry), an amendment:
  "Amended 2026-10-13 (the A/B/C sweep's fixes,
  [`abc-sweep-1012-fixes.md`](abc-sweep-1012-fixes.md) 3): four more runs
  no longer zero on a disc-built image, each read verbatim from the overlay
  whose code reads it (`twin`): the faerie board's help words for kinds 4
  and 5 / pick rows 0 and 1 (`COMMU01.EMI#7`, every build), the village's
  area limits (`COMMU00.EMI#0`, PSX-JP and PSX-EU), the field equipment
  compare panel's slot icons (`STATUS.EMI#0`, the second of its two copies,
  every build) and the gene window's AP label (`BATTLE.EMI#15`, every
  build). Every filled byte equals `BOF3.exe`'s. Verification: that doc 3.5
  and 3.6."
- **`docs/known-defects.md`**: D260..D263 fixed by that amendment (causes
  3.1..3.4); D259 gains its route (3.1); the faerie stats (3.5) for a new
  number.
- **`docs/abc-harness.md`** 9: the stale-log trap (3.5).
- **`docs/loc-build-disc-only.md`** 5.4: `0x653601`'s row is placed on
  every build now (by map or `twin`).
