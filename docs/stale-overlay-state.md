# Stale overlay state: memory the PlayStation reloaded and the PC keeps

**Status:** MEASURED (2026-11-05, read and test only - no game behaviour
changed). A static census of `BOF3.exe`'s overlay-born `.data`
(`tools/stale_census.py`), a dynamic check that reaches one point twice in
one process (`tools/revisit_diff.py`), the check run on D226's scene, and a
proposal for the owner (section 3). Harness root
`C:\Users\kerok\bof3x-runs\stale1105` (a junctioned game copy and an ini-less
launcher copy, built from `33f7cc6c`; the owner's game and slots untouched).

## 0. The class

On the PlayStation each area, battle, boss, spell, scenario chapter and
swap-slot screen (shop, menus, title, faerie village) was an overlay: its code
**and its initialised data** were loaded from disc into a band of RAM every
time the game needed it. A table the overlay's code wrote came back to its
disc bytes on the next load, by the console's loading and not by any code of
Capcom's. The 2001 PC port linked every overlay into one image, so the same
table is loaded once, when the process starts. A write survives into the
next showing **in the same process**.

A fresh process starts from the exe's bytes, so a replay, a recipe run from
launch and a clean load of a save never show these defects. Only a revisit
inside one process does: a second visit, a reload of a save after the scene
(F9 to the title, or the load menu), a second fight. This is why the owner's
reports in this class do not reproduce from a clean start.

Known members before this document:

| Member | What persists | Status |
|---|---|---|
| D145 / issue #149, areas 108 and 52 | area 108's cart models and its flags byte, area 52's scene byte | fixed by DIV-0182 (`AreaDataReload`, the area's bytes put back at its DAT load) |
| the Volt EXP row-done byte `+0xF1` | the enemy object's byte, `.bss` on the PC, inside `BATTLE.EMI#3`'s image on the PlayStation | fixed by DIV-0082 (`Battle_CopyEnemyData` writes `+0xF1 = 0`) |
| D226, area 196 | `EffectKind18Sub59_Fade` `0x512C30` writes the shades `0x65EF0C..` down to 0, nothing writes them back: the Manmo sand hill missing, Manmo in the open (the owner, 2026-11-05, 16:9) | open; its fix is in the session that started this one |

## 1. Static census

### 1.1 Method

`tools/stale_census.py` (no game process; reads `BOF3.exe` and the tree):

1. **Ranges.** `exe_maps/psx-jp.tsv` (`tools/exe_twins.py map`, committed)
   places 81 % of the PC's initialised `.data` byte for byte in a disc file:
   the boot EXE or an EMI section. Every segment of a file other than the
   boot EXE is overlay-born. Segments of one file and section closer than 64
   bytes, with no other file between, are merged: **465 ranges, 507,679
   bytes**. PSX-JP is the PC's parent; `--build` takes another map.
2. **When the PlayStation reloaded it**, by the file:
   `WORLDnn/AREAnnn.EMI` the area, `BATTLE/` the battle, `BOSS/` the boss
   fight, `BMAGIC/` the spell, `SCENARIO/` the chapter's scenario code,
   `ETC/<name>` that screen's swap slot (`START`, `SHOP`, `BATE`, `COMMU0n`,
   `SISYOU`, `SHISU`). `ETC/GAME.EMI` section 0 is the field engine, which the
   sibling's `docs/ADDRESS_MAPS.md` lists as the resident band
   `0x80196800..0x801CE400`: **not** in the class unless a reload of it is
   measured.
3. **Writers.** A linear sweep of `.text` with capstone, every memory operand
   whose displacement lies in `.data` **with or without a base or index
   register**. `tools/pe_xref.py`'s index keeps only base-less operands, so
   it finds no writer of D226's bytes at all (measured: `pe_xref.py --range
   0x65EF0C:0x65EF18` lists one read, at `0x512CCC`); the new scan finds the
   four `mov [reg+0x65EF0C..F], ..` of `EffectKind18Sub59_Fade`. An operand
   capstone marks written is a write; `lea`, and `mov` / `push` of an
   immediate in the range, take its address.
4. **Class** of a range: `written` (some instruction writes it directly),
   `address` (no direct write, the address taken: a writer through a pointer
   may follow, unread), `read-only`, `unreferenced` (no operand at all:
   reached through pointer tables - scripts, area descriptors, models).

```
python tools/stale_census.py --exe <BOF3.exe>            # summary, analysis/stale_census.tsv
python tools/stale_census.py --exe <BOF3.exe> --show 0x65EF0C
```

### 1.2 What it found

| Reload | written | address | read-only | unreferenced |
|---|---|---|---|---|
| area | 5 ranges, 4,041 B | 70, 91,902 B | 76, 98,239 B | 149, 269,275 B |
| battle | - | 7, 3,286 B | 5, 6,666 B | 3, 344 B |
| boss | - | 32, 1,693 B | - | - |
| spell | - | 11, 782 B | 33, 3,960 B | - |
| chapter | - | 17, 1,837 B | 7, 3,507 B | - |
| screens (START, SHOP, BATE, COMMU, SISYOU, SHISU) | - | 20, 9,002 B | 10, 2,080 B | 4, 646 B |
| game (resident on PSX) | 1, 1,160 B | 6, 8,142 B | 7, 1,084 B | 2, 33 B |

The six `written` ranges, each writer read:

| Range | PSX file | Writer | Verdict |
|---|---|---|---|
| `0x65EF0C..0x65EF58` | `WORLD04/AREA196.EMI#13` | `EffectKind18Sub59_Fade` `0x512C30` (`+0x2E`, `+0x41`, `+0x54`, `+0x5C`): the shades down to 0 | **stale on a second showing** - D226, confirmed dynamically (section 2) |
| `0x5FD498..0x5FE104` | `WORLD01/AREA052.EMI#13` | `Area52_TailBlock` `0x40A670`: byte `0x5FE049` 0 as the timed scene begins, `0xFF` as it ends | stale if the scene is left between the two (D145); put back by DIV-0182 |
| `0x61E508..0x61E660` | `WORLD02/AREA108.EMI#14` | `Area108_TailPlace` `0x417210` (`+0x1B1`): `Area108_Model`'s flags byte `0x61E50B` | stale (D145, issue #149); put back by DIV-0182 |
| `0x62D880..0x62D8DC` | `WORLD03/AREA135.EMI#14` | `Area135_Tail18` (`+0x3A4`, `+0x3F7`): bit 4 of `0x62D883` set in state `0x1C`, cleared in `0x1D` ([`area_w3d.md`](area_w3d.md) 1.1, area 135) | **resets itself** - stale only if the run is cut between the two states (an F9 or a load mid-glide), not measured |
| `0x654D9C..0x654EF9` | `WORLD03/AREA144.EMI#13` | `EffectKind87_Midpoint` `0x489630`: rows 8..13 of `0x654DA0` from rows 0..7 ([`effect_4b.md`](effect_4b.md) 7) | **idempotent** - the rows it reads are never written (D238) |
| `0x658926..0x658DAE` | `ETC/GAME.EMI#0` | `FieldTrigger09` `0x456B40`: `Shop_Records[n] +0` from the community record ([`rest_4a.md`](rest_4a.md)) | **idempotent**, and the field engine was resident on the PlayStation too |

**What the static census cannot see.** Area 108's models, which DIV-0182
puts back, lie in `0x61D2E4..0x61E504` (`WORLD02/AREA108.EMI#13`), a range
this scan classes `unreferenced`: the blast (effect kind 0x35) writes them
through the pointer `Area108_Model`'s entries hold. So `address` and
`unreferenced` ranges are not safe by the static class; the 70 `address`
ranges of the areas and the boss, spell, chapter and screen ones are listed in
appendix A with the functions that take their addresses, for reading - or
for the dynamic check, which sees every write whoever made it.

**`.bss` is outside it.** A PlayStation overlay's zero bytes match nothing by
bytes, so the PC `.bss` that was overlay memory on the console (the Volt
byte's enemy objects, `0x93B960`, inside `BATTLE.EMI#3`'s image) is found
only by the dynamic check or by reading.

## 2. The dynamic check: `tools/revisit_diff.py`

### 2.1 Method

One process, one recipe (`OUT/revisit.txt`) played through `input_run.py`'s
launch, slot-0 swap (`--slot0-hold`) and frozen shots:

| Shot | Where |
|---|---|
| `rv_title1` | the title (`Title_LogoState` `0x66C7FA` == 2) |
| `rv_x1` | BODY played: the title's load of slot 0 and on to X |
| `rv_end1` | AFTER played: the first showing runs on (the scene, its fade, its fight) |
| (F9, F9) | the restart to the title (`Game_RestartFlag`), as `cartReload.txt` does |
| `rv_title2` | the title again |
| `rv_x2` | BODY again: the same save, the same inputs, X again |

At each `rv_*` shot the game is frozen (`BOF3X_SHOT_WAIT`) and the script
reads `0x5DA000..0x93E000` (`.data` and `.bss`) from outside. `report`
compares them, `tools/statehash_skip.txt`'s ranges left out. A byte with
`rv_x1 != rv_x2` is **stale** when `rv_x1 == rv_title1` and `rv_x2 ==
rv_end1` (the fresh process holds at X what it held before the load; the
revisit holds what the first showing left - timers and Rand move on, so they
fail the second test), **carried** when `rv_x2 == rv_end1 != rv_title1` and
`rv_x1 != rv_title1`, else **other**. One row per class and symbol; the
overlay ranges of section 1 first, named by file and reload.

**`--same-rand`**, the one write the check makes. The owner's desert route
meets random encounters; on the second visit Rand had drawn 24,415 more times
and the route ended in area 125 instead of 196 (run `r2`, `until 0x904EFC
timed out ... value 0x7D`). With `--same-rand` the script reads Rand's draw
count from the input driver's randlog at the frozen shots, finds at `rv_x1`
the one word of the process holding the seed that count implies (the CRT's
LCG from 1), and at `rv_title2` writes back the seed of `rv_title1`'s count.
The second visit then draws what the first drew; Rand-driven state compares
equal, and what still differs differs for another reason.

### 2.2 D226's scene, confirmed

The craft the brief named (chapter 14, state 2, run 2, step 0, area `0xC4`
flags `0x80` at `(0x298000, 0x168000)`, chapter flag 4 clear, donor
`adult_ryu`) loads into area 196 but **does not start the scene**: 4,000
frames with a circle every 40 left `MoveScript_Var7` at run 2 step 0 and the
shades at `0x80` (`probe`). `Scena14_Run2` step 0 waits for counter 0 to be 2
(`scena_sc13.cpp`), which the arrival from area 189 and its talk give. So the
check uses the owner's `desertToManmo` route (recorded 2026-11-05 under
`en-US`; its save `desertToManmo`), split for the tool:
[`tools/recipes/revisit_manmo_body.txt`](../tools/recipes/revisit_manmo_body.txt)
(the load, area 191, the desert to the walk into area 196, X 30 frames after
the arrival) and
[`revisit_manmo_after.txt`](../tools/recipes/revisit_manmo_after.txt) (the
talk, the ring's fade, the Manmo fight). A watch on the shades in the probe
(`probe2`) placed the scene: run 2 starts in area 189 at recipe frame 3,623,
area 196 at 3,641, the fade from 4,641 (`0x65EF0C` 0x80 -> 0x7E, two a
frame), the fight at 4,853.

```
python tools/revisit_diff.py run --same-rand --save desertToManmo --lang en-US \
    --body tools/recipes/revisit_manmo_body.txt --after tools/recipes/revisit_manmo_after.txt \
    --watch 0x65EF0C:12 --out <root>/r3 --game <root>/game --launcher <root>/L/bof3x-launcher.exe
```

Run `r3`: `done` at recipe frame 9,202; area 196 at X both times; Rand's seed
found at one word, 24,415 draws at `rv_x1`, put back to `rv_title1`'s 0.
The report's first row:

```
class    first      runs  bytes symbol                   overlay (psx-jp)               title1/x1/end1/x2
stale    0x65EF0C     1     12 EffectKind18Sub55_States WORLD04/AREA196.EMI#13 (area)  80../80../00../00..
```

**The twelve shades `0x65EF0C..0x65EF17`: `0x80` at X on the first visit,
`0x00` at the same X on the revisit - D226 found by the tool, and the only
overlay-born stale run of the scene.** (The symbol is the nearest earlier
name; `EffectKind18Sub55_States` is three words, the shades follow it.)

The rest of `r3`'s stale and carried rows are `.bss` of the PC's own, none
overlay-born by section 1's map, and are leads, not verdicts: the Manmo
fight's enemy state (`Battle_Enemies`, `EnemyWorkingRecords`,
`BattleTask_Slots`, `BattleHook_Event`, `BossSi_CentreX`), carried into the
field at X - the Volt byte's family (`0x93B960` lay in `BATTLE.EMI#3`'s
image), harmless at X unless a later fight reads a byte its spawn does not
clear; `Encounter_Rows`; `Effect_Objects`, `Sprite_DrawRecords`,
`Sprite_Kind2`, `MapView_Origin`, `SpriteCell_Table` (pools the next
spawn overwrites); `Gfx_VramShadow` and `Gfx_UnpackScratch` (graphics
caches, by construction). The no-fade baseline (`r1`, the brief's craft with
nothing happening) gave no overlay-born row at all, and two one-byte party
bytes (`ObjTrio+0x8`, `PartyWorkingRecords+0x1A`, 0 fresh, 3 and 1 on the
revisit) - unread.

### 2.3 The candidates

- **Issue #190** (window record 21 run by battle-window handler 3 as an
  enemy gauge): the window records are `.bss` at `0x803160` (22 of `0x24`
  bytes, [`window-task.md`](window-task.md)), PSX `0x8014832C`. That address
  lies in the band `0x80117000..0x80149800`, which the sibling's
  `ADDRESS_MAPS.md` lists as an overlay band with no named occupant. Whether
  an overlay load cleared it on the console decides the issue's membership;
  not measured. Run `r3` crossed a fight and back to the field and no window
  record row appeared (stale, carried or the overlay map). The check that
  would settle it: a body that opens the gene menu in the station's boss rush
  (area 142, the owner's suspicion), after an AFTER that fights there.
- **ScrubOasis's "stuck" windows read as the wrong type**: the same records;
  same check. Not run.
- Neither is proven a member; both are now a run of `revisit_diff.py` away
  once a save and a body for the scene exist.

### 2.4 Cost and limits

A run is one game: the Manmo check took five minutes at speed 1 (`r3`'s snapshots 11:06 to 11:11). It
needs the body to replay alike from the title both times: a route through
random encounters needs `--same-rand`; a route whose second visit diverges
for a stale byte's sake (the class itself) shows it as a `FAILED` `until` -
read the shots. The report compares X to X; a stale byte that the scene
itself rewrites before X is not seen, so X belongs before the effect that
reads it. Snapshots are game memory: they stay under the harness root.

## 3. Proposal for the owner: one reload table, against a fix per member

**The general fix.** DIV-0182 already is the mechanism for two areas: the
image's bytes copied at inject, before any scene, and put back when
`Area_Enter` loads an area's DAT - the moment the PlayStation loaded the
area's overlay. Generalise it into one ledger entry with a table: for each
reload class, the overlay-born ranges of section 1 and the load point where
the PlayStation reloaded them.

| Class | Ranges (psx-jp map) | PC load point | Put back |
|---|---|---|---|
| area | 300, 463,457 B, keyed by area number | `Area_Enter`'s DAT load (DIV-0182's hook: area number differs, `Field_Request` 5, the save load's `0xFFFF`) | that area's ranges only |
| battle | 15, 10,296 B, plus the `.bss` of `BATTLE.EMI#3`'s image once mapped | the battle's start | all |
| boss | 32, 1,693 B, by boss file | the boss fight's start | that boss's |
| spell | 44, 4,742 B, by spell file | the spell's effect load | that spell's |
| chapter | 24, 5,344 B, by chapter | the chapter change | that chapter's |
| screens | 34, 11,728 B | the screen's open | that screen's |
| game | 16, 10,419 B | none - resident on the PlayStation | - |

Put back **whole ranges, not only the `written` ones**: area 108's models
show that a pointer write is invisible to the static scan, and a range that
nothing writes costs a compare and no change. The bytes put back are the
image's after inject (DIV-0182's snapshot point), so language layers,
disc-built images and every inject-time ledger entry stay as they are. One
`BOF3X_ORIGINAL` switch keeps the port's behaviour; one log line per put-back
that changed bytes makes each member visible as it happens (a field census
of the class for free).

**Against a fix per member** (D226 alone: the shades back to `0x80` as the
effect starts, USER_CHECKS 8's suggestion):

| | One reload table | A fix per member |
|---|---|---|
| Members found so far | all of them, and every one not yet found in the mapped ranges | one each, after a report each |
| Reports that do not reproduce from a clean start | stop arising for mapped data | each costs a hunt like 2026-11-05's |
| Size | one table generated from `exe_maps/`, one hook per load point (the area one exists) | small, but grows per member |
| Risk | a range mis-placed by `exe_maps` (a `name`-placed or many-copies segment, or one shared with resident data) put back wrongly; a value the port writes on purpose after inject into an overlay range (none known; the put-back log would show it at once) | each fix right by itself; the class stays open |
| Rationale in the ledger | as DIV-0182's: the console's loading, at its moment, with the image's own bytes; nothing invented | per member |

Recommendation: the table, built in steps - **areas first** (DIV-0182's hook
already exists; D226 is an area member; area ranges are 91 % of the
overlay-born bytes), then battle and boss (with the `.bss` of the battle
overlay's image mapped, which the Volt byte shows matters), then spells,
chapters and screens. Before each step, the ranges kept to segments placed
`by run` with one copy in two builds' maps (psx-jp and psx-us agreeing), and
a `revisit_diff.py` run over a scene of the class with the step on, which
should show no overlay-born stale row. D226's own fix, already in progress
in the starting session, fits as the area step's first row or stands alone
until the owner chooses.

The choice is the owner's: whether the general fix is wanted, and if so
whether it amends DIV-0182 or opens a new entry that absorbs it.

## Appendix A: overlay-born ranges whose address is taken

`tools/stale_census.py` on `exe_maps/psx-jp.tsv`, 2026-11-05: no direct
write, the address taken by the functions named (first four). Read these, or
run the dynamic check over their scenes, before trusting them clean.

| Range | Bytes | PSX file#section | Reload | Takers of the address |
|---|--:|---|---|---|
| `0x5DFD4C..0x5E0340` | 1524 | `WORLD00/AREA007.EMI#13` | area | `Scena02_Scene0F`, `Scena03_Scene3`, `Scena07_Scene3`, `Scena13_Run3` |
| `0x5E5BDC..0x5E6024` | 1096 | `WORLD00/AREA016.EMI#13` | area | `Area16_DrawFrame`, `Area16_PlaceMessage`, `Area16_PlateShow` |
| `0x5E7E2C..0x5E875C` | 2352 | `WORLD00/AREA023.EMI#13` | area | `EffectKind29_SetSegments` |
| `0x5EF31C..0x5EF6BE` | 930 | `WORLD00/AREA033.EMI#13` | area | `WorldMap33_PlaceMessage`, `WorldMap33_PlateShow`, `WorldMap_DrawFrame` |
| `0x5EF6CF..0x5F01E4` | 2837 | `WORLD00/AREA034.EMI#13` | area | `Area145_Tail20`, `Scena01_Scene02` |
| `0x5F3EBC..0x5F4C50` | 3476 | `WORLD01/AREA040.EMI#13` | area | `Area40_Init`, `Area40_PlaceObject` |
| `0x5F5F6E..0x5F6307` | 921 | `WORLD01/AREA042.EMI#13` | area | `Area42_Init` |
| `0x5F7074..0x5F7096` | 34 | `WORLD01/AREA044.EMI#13` | area | `Area44_SwitchHook` |
| `0x5F7096..0x5F771C` | 1670 | `WORLD01/AREA045.EMI#13` | area | `Area45_DrawFrame`, `Area45_PlaceMessage`, `Area45_PlateShow` |
| `0x5F7EA0..0x5F8320` | 1152 | `WORLD01/AREA047.EMI#13` | area | `Scena14_Run6` |
| `0x5F98FD..0x5FBC68` | 9067 | `WORLD01/AREA049.EMI#14` | area | `Area49_CellHook`, `Area49_MemberZone` |
| `0x5FF5B4..0x60079C` | 4584 | `WORLD01/AREA055.EMI#13` | area | `Area186_TailShift`, `CombustionSprite_Start`, `EffectKind1C_Start`, `LavaburstChild_Launch` +1 |
| `0x6043C8..0x604948` | 1408 | `WORLD01/AREA065.EMI#13` | area | `Area65_DrawFrame`, `Area65_PlaceMessage`, `Area65_PlateShow` |
| `0x607DFC..0x608E90` | 4244 | `WORLD01/AREA069.EMI#13` | area | `EffectKind2A_Beam`, `EffectKind2A_Beams` |
| `0x60AAE0..0x60ABF0` | 272 | `WORLD01/AREA075.EMI#13` | area | `Area75_SpawnFive` |
| `0x60B678..0x60BC23` | 1451 | `WORLD02/AREA077.EMI#14` | area | `Area77_CellHook` |
| `0x60FDDC..0x6108B4` | 2776 | `WORLD02/AREA082.EMI#13` | area | `EffectKind2A_PushParty`, `Scena03_Scene5`, `Scena15_Run6` |
| `0x611824..0x611C94` | 1136 | `WORLD02/AREA086.EMI#13` | area | `Area86_SwitchHook` |
| `0x611C94..0x6126EC` | 2648 | `WORLD02/AREA087.EMI#13` | area | `Area87_DrawFrame`, `Area87_PlaceMessage`, `Area87_PlateShow` |
| `0x612700..0x613060` | 2400 | `WORLD02/AREA088.EMI#13` | area | `Area88_DrawFrame`, `Area88_PlaceMessage`, `Area88_PlateShow` |
| `0x61BB20..0x61BC74` | 340 | `WORLD02/AREA104.EMI#14` | area | `Area104_DrawFrame`, `Area104_DrawPanel` |
| `0x61FB5C..0x61FBD4` | 120 | `WORLD02/AREA111.EMI#13` | area | `Area111_Init` |
| `0x61FBD4..0x61FFB4` | 992 | `WORLD02/AREA112.EMI#13` | area | `Area112_CellHook`, `Area112_MemberNearBoxes` |
| `0x620144..0x620310` | 460 | `WORLD03/AREA115.EMI#13` | area | `Area115_PlaceMessage`, `Area115_PlateShow` |
| `0x62074F..0x621610` | 3777 | `WORLD03/AREA117.EMI#13` | area | `Area117_MemberRect`, `Area117_SwitchHook` |
| `0x62161D..0x621E40` | 2083 | `WORLD03/AREA118.EMI#13` | area | `Area118_MemberRect`, `Area118_SwitchHook` |
| `0x623690..0x623904` | 628 | `WORLD03/AREA121.EMI#14` | area | `Area121_PlateShow` |
| `0x624668..0x6246F4` | 140 | `WORLD03/AREA121.EMI#14` | area | `Area121_DrawFrame` |
| `0x62D970..0x62DA9B` | 299 | `WORLD03/AREA135.EMI#14` | area | `Area135_CellHook`, `Area135_Init`, `Area135_MarkerRoute`, `Area135_SpawnCopy` |
| `0x62EC44..0x62EDC4` | 384 | `WORLD03/AREA139.EMI#13` | area | `Area139_CellHook` |
| `0x62EDC7..0x62F038` | 625 | `WORLD03/AREA140.EMI#13` | area | `Area140_CellHook` |
| `0x62F04C..0x62F6A4` | 1624 | `WORLD03/AREA141.EMI#13` | area | `Area141_CellHook`, `Area141_ChoiceFlag7D`, `Area141_InitCells`, `Area141_PlaceOneAnimated` +5 |
| `0x62FCEC..0x630C40` | 3924 | `WORLD03/AREA143.EMI#13` | area | `Area112_ChangeAreaByFlags`, `Area145_Tail20`, `Area149_Tail33`, `EffectKind5A_Place` +4 |
| `0x633650..0x63377C` | 300 | `WORLD03/AREA145.EMI#14` | area | `Area145_CellHook`, `Area145_TrailLeg1`, `Area145_TrailLeg2` |
| `0x6353AC..0x635454` | 168 | `WORLD03/AREA148.EMI#13` | area | `Area148_BeamTurnMembers`, `Area148_SwitchHook` |
| `0x636A65..0x6371A4` | 1855 | `WORLD03/AREA151.EMI#13` | area | `Area151_PlaceMessage`, `Area151_PlateShow` |
| `0x637286..0x637486` | 512 | `WORLD04/AREA152.EMI#13` | area | `Area152_DrawFrame`, `Area152_PlaceMessage`, `Area152_PlateShow` |
| `0x637486..0x638428` | 4002 | `WORLD04/AREA153.EMI#13` | area | `Area145_Tail20`, `Area145_TrailLeg0`, `Area145_TrailStart`, `EffectKind09_Start` +1 |
| `0x63A7A8..0x63C598` | 7664 | `WORLD04/AREA167.EMI#13` | area | `Area167_CellHook` |
| `0x63CA3C..0x63CA46` | 10 | `WORLD04/AREA169.EMI#13` | area | `Area169_MemberRect` |
| `0x63D650..0x63D856` | 518 | `WORLD04/AREA171.EMI#13` | area | `Area171_MemberRect` |
| `0x63FFC8..0x6415D4` | 5644 | `WORLD04/AREA174.EMI#13` | area | `Area173_Tail38`, `Area186_Start`, `EffectKind52_TrailTurn`, `Scena08_Scene1` +1 |
| `0x641674..0x64174C` | 216 | `WORLD04/AREA174.EMI#13` | area | `Area174_RestartSlotScript` |
| `0x649830..0x649E17` | 1511 | `WORLD04/AREA198.EMI#13` | area | `Area198_SpawnDrop`, `Area198_StartSlotScript0`, `Area198_StartSlotScript11` |
| `0x64B038..0x64B1D4` | 412 | `BATTLE/BATTLE.EMI#3` | battle | `BattleFx_WatchIconStart`, `EnemyOp_Begin` |
| `0x64B397..0x64C1D0` | 3641 | `ETC/START.EMI#8` | start | `NewGame_InitCharacters` |
| `0x64C770..0x64C7B0` | 64 | `BOSS/BOSS001.EMI#13` | boss | `BossGary_EndAwait`, `BossGary_Enter`, `BossMogu_EndCount`, `BossMogu_Enter` |
| `0x64C870..0x64C890` | 32 | `BOSS/BOSS002.EMI#16` | boss | `BossNue2_Enter`, `BossNue_Enter`, `BossSample1_Enter` |
| `0x64C964..0x64C984` | 32 | `BOSS/BOSS004.EMI#13` | boss | `BossEngineer_Enter`, `BossEngineer_HitStep` |
| `0x64C9E4..0x64CA0C` | 40 | `BOSS/BOSS007.EMI#16` | boss | `BossOperator_Enter`, `BossWorker_Enter` |
| `0x64CA84..0x64CAA4` | 32 | `BOSS/BOSS008.EMI#16` | boss | `BossDoksen_Enter`, `BossGaltel_Enter`, `BossKassen_Enter`, `BossTorast_Enter` |
| `0x64CBC4..0x64CC14` | 80 | `BOSS/BOSS012.EMI#16` | boss | `BossAmalgam_Enter` |
| `0x64CC88..0x64CCD0` | 72 | `BOSS/BOSS013.EMI#16` | boss | `Boss16_Event`, `BossBalio_Enter`, `BossNina_Enter`, `BossNina_HookHit` +1 |
| `0x64CD90..0x64CDAC` | 28 | `BOSS/BOSS014.EMI#16` | boss | `BossRocky_Enter`, `BossSample8_Enter` |
| `0x64CE24..0x64CE38` | 20 | `BOSS/BOSS015.EMI#16` | boss | `BossPooch_Enter` |
| `0x64CE74..0x64CEB0` | 60 | `BOSS/BOSS017.EMI#16` | boss | `BossMutant_Enter` |
| `0x64CF04..0x64D0A0` | 412 | `BOSS/BOSS018.EMI#16` | boss | `BossCawer_Enter`, `BossClaw_Enter`, `BossDodai_Dispatch`, `BossDodai_Enter` +1 |
| `0x64D0A0..0x64D0C8` | 40 | `BOSS/BOSS021.EMI#16` | boss | `BossEmitai_Enter`, `BossGolem_Enter` |
| `0x64D1C0..0x64D1F0` | 48 | `BOSS/BOSS023.EMI#16` | boss | `BossBully_Enter` |
| `0x64D22C..0x64D240` | 20 | `BOSS/BOSS024.EMI#16` | boss | `BossSample10_Enter`, `BossStallion_Enter` |
| `0x64D2B8..0x64D2E0` | 40 | `BOSS/BOSS025.EMI#13` | boss | `BossBeyd2_Enter`, `BossBeyd_Enter`, `BossZig_Enter` |
| `0x64D3F0..0x64D404` | 20 | `BOSS/BOSS028.EMI#16` | boss | `BossGazer_Enter` |
| `0x64D4C8..0x64D4F0` | 40 | `BOSS/BOSS030.EMI#16` | boss | `BossGisshan_Enter`, `BossScylla_Enter` |
| `0x64D568..0x64D57C` | 20 | `BOSS/BOSS031.EMI#13` | boss | `BossGarr2_Enter` |
| `0x64D5D0..0x64D5E4` | 20 | `BOSS/BOSS032.EMI#16` | boss | `BossDZombie_Enter` |
| `0x64D638..0x64D650` | 24 | `BOSS/BOSS033.EMI#16` | boss | `BossWeretigr_Enter` |
| `0x64D6CC..0x64D78E` | 194 | `BOSS/BOSS034.EMI#16` | boss | `BossGaist_Enter`, `BossMikba_Enter`, `BossSample3_Enter`, `BossSample9_Enter` |
| `0x64D85C..0x64D870` | 20 | `BOSS/BOSS036.EMI#16` | boss | `BossAngler_Enter`, `BossSample5_Enter` |
| `0x64D8FC..0x64D910` | 20 | `BOSS/BOSS037.EMI#16` | boss | `BossElder_Enter` |
| `0x64D94C..0x64D960` | 20 | `BOSS/BOSS038.EMI#16` | boss | `BossAmmonite_Enter`, `BossSample6_Enter` |
| `0x64D9D8..0x64D9EC` | 20 | `BOSS/BOSS040.EMI#16` | boss | `BossHugeSlug_Enter`, `BossSample2_Enter` |
| `0x64DA64..0x64DA78` | 20 | `BOSS/BOSS042.EMI#16` | boss | `BossSample4_Enter` |
| `0x64DAB4..0x64DAC8` | 20 | `BOSS/BOSS049.EMI#16` | boss | `BossManmo_Enter`, `BossSample7_Enter` |
| `0x64DB40..0x64DB58` | 24 | `BOSS/BOSS050.EMI#16` | boss | `BossChimera_Enter` |
| `0x64DB94..0x64DBA8` | 20 | `BOSS/BOSS051.EMI#16` | boss | `BossArwan_Enter` |
| `0x64DBEC..0x64DC00` | 20 | `BOSS/BOSS052.EMI#16` | boss | `BossDLord_Enter` |
| `0x64DC58..0x64DC6F` | 23 | `BOSS/BOSS054.EMI#16` | boss | `BossShroom_Enter` |
| `0x64DC6F..0x64DD03` | 148 | `BOSS/BOSS055.EMI#16` | boss | `BossMyria_Enter` |
| `0x64DD03..0x64DECC` | 457 | `ETC/GAME.EMI#0` | game | `BossMyria_Enter` |
| `0x64DF6F..0x64E3E8` | 1145 | `BATTLE/BATTLE.EMI#3` | battle | `BattleEquipWin_Draw`, `BattleHold_ShowAll`, `BattleWin_CrossGrow`, `BattleWin_DrawCommandCross` +12 |
| `0x64EC99..0x64F128` | 1167 | `BATTLE/BATTLE.EMI#15` | battle | `DragonForm_Mix` |
| `0x6528F1..0x652AE4` | 499 | `ETC/COMMU00.EMI#0` | commu00 | `CommuSim_QueueAreas`, `CommuSim_TickKindD` |
| `0x652AFC..0x652C8A` | 398 | `ETC/COMMU01.EMI#7` | commu01 | `CommuBoard_DrawRecordCard` |
| `0x652D94..0x653084` | 752 | `ETC/COMMU02.EMI#8` | commu02 | `CommuMember_DrawPanel`, `CommuMember_DrawPortrait`, `Commu_DrawStakeBox`, `EffectKind5D_Show` +2 |
| `0x653180..0x6531F4` | 116 | `ETC/COMMU05.EMI#7` | commu05 | `CommuRank_DrawBackdrop` |
| `0x653C03..0x653EA0` | 669 | `WORLD00/AREA030.EMI#4` | area | `EffectKind0F_Child3Grid` |
| `0x653F78..0x653FBC` | 68 | `SCENARIO/SCENA03.EMI#0` | chapter | `EffectKind14_Start`, `EffectKind3C_Start` |
| `0x654470..0x654480` | 16 | `WORLD03/AREA145.EMI#14` | area | `EffectKind3E_Bolts` |
| `0x65451C..0x654576` | 90 | `WORLD02/AREA085.EMI#13` | area | `EffectKind47_Start` |
| `0x6546B0..0x6546C3` | 19 | `WORLD00/AREA032.EMI#13` | area | `EffectKind4F_Start` |
| `0x654839..0x654900` | 199 | `SCENARIO/SCENA10.EMI#0` | chapter | `EffectKind5D_Close`, `EffectKind5D_DrawBoard`, `EffectKind5D_Open`, `EffectKind5D_Show` |
| `0x654C3C..0x654C5C` | 32 | `WORLD03/AREA143.EMI#13` | area | `EffectKind81_PlaceSources` |
| `0x654EF9..0x654F28` | 47 | `WORLD04/AREA168.EMI#13` | area | `EffectKind8A_Block`, `EffectKind8A_Lines`, `EffectKind8A_Unblock` |
| `0x6551D8..0x6551E4` | 12 | `SCENARIO/SCENA15.EMI#0` | chapter | `EffectKind9E_DrawTexPlate` |
| `0x6552AC..0x6552BC` | 16 | `SCENARIO/SCE15EF3.EMI#0` | chapter | `EffectKindAB_PlaceSources` |
| `0x658F48..0x65A4F0` | 5544 | `ETC/GAME.EMI#0` | game | `BattleResultWin_ExpToNext`, `Char_ExpForLevel`, `Char_LevelUp`, `Char_LevelUpGain` +1 |
| `0x65A730..0x65A750` | 32 | `BMAGIC/MAGIC039.EMI#3` | spell | `Focus_Start` |
| `0x65A988..0x65A9E8` | 96 | `BMAGIC/MAGIC050.EMI#3` | spell | `Pollen_Start` |
| `0x65ABC0..0x65ABF9` | 57 | `BMAGIC/MAGIC063.EMI#3` | spell | `SuddenDeathMote_Draw` |
| `0x65AC54..0x65AC78` | 36 | `BMAGIC/MAGIC066.EMI#3` | spell | `TsunamiRings_Draw` |
| `0x65ACB0..0x65ACE0` | 48 | `BMAGIC/MAGIC068.EMI#3` | spell | `Foretell_DrawTiles` |
| `0x65B200..0x65B228` | 40 | `BMAGIC/MAGIC078.EMI#3` | spell | `LeechShell_Draw` |
| `0x65B52C..0x65B5A8` | 124 | `BMAGIC/MAGIC088.EMI#3` | spell | `Magic088_DrawWave`, `Magic088_WaveStep` |
| `0x65B828..0x65B868` | 64 | `BMAGIC/MAGIC104.EMI#3` | spell | `Fx104_SecondRing`, `Fx104_Start` |
| `0x65BB00..0x65BB24` | 36 | `BMAGIC/MAGIC120.EMI#3` | spell | `WhelpBreathFlames_Draw` |
| `0x65C22C..0x65C309` | 221 | `BMAGIC/MAGIC219.EMI#3` | spell | `MagmaBreath_Spawn` |
| `0x65C37F..0x65C4C8` | 329 | `BATTLE/BATTLE.EMI#15` | battle | `Magic225_Apply` |
| `0x65DA28..0x65DA44` | 28 | `BMAGIC/MAGIC080.EMI#0` | spell | `Magic080_DrawGlyphs` |
| `0x65DB69..0x65DDB0` | 583 | `WORLD02/AREA086.EMI#13` | area | `EffectKind18_06_Draw` |
| `0x65DDB0..0x65DE60` | 176 | `WORLD01/AREA041.EMI#13` | area | `EffectKind18_07_Draw`, `EffectKind18_08_DrawFallen`, `EffectKind18_08_DrawTilted` |
| `0x65DE8C..0x65DF7E` | 242 | `WORLD01/AREA048.EMI#13` | area | `EffectKind18_0A_DrawDoor` |
| `0x65DF8C..0x65DFF8` | 108 | `WORLD01/AREA042.EMI#13` | area | `EffectKind18Sub0B_DrawTiles` |
| `0x65E11C..0x65E18B` | 111 | `WORLD02/AREA095.EMI#13` | area | `EffectKind18Sub11_DrawTiles`, `EffectKind18Sub12_DrawTiles` |
| `0x65E194..0x65E1A8` | 20 | `WORLD01/AREA060.EMI#13` | area | `EffectKind18Sub15_Draw` |
| `0x65E1E8..0x65E27C` | 148 | `WORLD02/AREA103.EMI#13` | area | `EffectKind18Sub17_DrawPatch` |
| `0x65E333..0x65E698` | 869 | `WORLD02/AREA112.EMI#13` | area | `EffectKind18Sub22_Draw`, `EffectKind18Sub22_SetMap` |
| `0x65E6A4..0x65E710` | 108 | `WORLD02/AREA082.EMI#13` | area | `EffectKind18Sub24_Draw`, `EffectKind18Sub24_DrawTrail`, `EffectKind18Sub24_Trails`, `EffectKind18Sub24_TrailsOut` |
| `0x65E7C4..0x65E890` | 204 | `WORLD03/AREA148.EMI#13` | area | `EffectKind18Sub39_Draw` |
| `0x65E8E0..0x65E940` | 96 | `WORLD03/AREA140.EMI#13` | area | `EffectKind18Sub42_Draw` |
| `0x65E9BC..0x65EA01` | 69 | `WORLD04/AREA170.EMI#13` | area | `EffectKind18Sub3C_Draw` |
| `0x65EA1C..0x65EAB0` | 148 | `WORLD04/AREA168.EMI#13` | area | `EffectKind18Sub4A_Place` |
| `0x65EC80..0x65ED46` | 198 | `WORLD02/AREA106.EMI#13` | area | `EffectKind18Sub41_Draw`, `EffectKind18Sub41_DrawPanels` |
| `0x65ED46..0x65EECC` | 390 | `WORLD04/AREA189.EMI#13` | area | `EffectKind18Sub44_Draw`, `EffectKind18Sub44_DrawTwinkle` |
| `0x65EEDC..0x65EEFF` | 35 | `WORLD03/AREA141.EMI#13` | area | `EffectKind18Sub53_Run` |
| `0x65EF70..0x65F422` | 1202 | `WORLD04/AREA173.EMI#14` | area | `EffectKind18Sub5B_DrawPart`, `EffectKind18Sub5C_DrawPanel` |
| `0x65F470..0x65F4E6` | 118 | `WORLD04/AREA199.EMI#13` | area | `EffectKind18Sub61_DrawOverlay`, `EffectKind18Sub61_DrawTracks` |
| `0x65F4E6..0x65F55A` | 116 | `WORLD03/AREA128.EMI#13` | area | `EffectKind18Sub63_Glow` |
| `0x6608D8..0x660C2C` | 852 | `ETC/GAME.EMI#0` | game | `Field_CellHasEvent`, `Field_CellsBlock`, `Field_EncounterArea`, `Field_GatewayExit` |
| `0x660E30..0x660E3C` | 12 | `SCENARIO/SCENA01.EMI#0` | chapter | `Scena01_CellHook` |
| `0x660FC0..0x660FC8` | 8 | `SCENARIO/SCENA03.EMI#0` | chapter | `Scena03_CellHook` |
| `0x661008..0x661010` | 8 | `SCENARIO/SCENA04.EMI#0` | chapter | `Scena04_CellHook` |
| `0x6610C0..0x6610CC` | 12 | `SCENARIO/SCENA05.EMI#0` | chapter | `Scena05_CellHook` |
| `0x661180..0x6611A8` | 40 | `SCENARIO/SCENA06.EMI#0` | chapter | `Scena06_CellHook` |
| `0x6611FC..0x66137C` | 384 | `SCENARIO/SCENA07.EMI#0` | chapter | `Scena07_CellHook`, `Scena07_PlaceObjects` |
| `0x66137C..0x6613D8` | 92 | `SCENARIO/SCENA08.EMI#0` | chapter | `Scena08_SpawnPair` |
| `0x6613E7..0x6614D8` | 241 | `SCENARIO/SCENA09.EMI#0` | chapter | `Scena09_CellHook` |
| `0x661524..0x661640` | 284 | `SCENARIO/SCENA10.EMI#0` | chapter | `Scena10_SpriteOp` |
| `0x661764..0x661778` | 20 | `SCENARIO/SCENA12.EMI#0` | chapter | `Scena12_CellHook` |
| `0x6617D0..0x661810` | 64 | `SCENARIO/SCENA13.EMI#0` | chapter | `Scena13_SpawnPairA`, `Scena13_SpawnPairB` |
| `0x661830..0x661884` | 84 | `SCENARIO/SCENA14.EMI#0` | chapter | `Scena14_Run6` |
| `0x6618BC..0x6619E1` | 293 | `SCENARIO/SCENA15.EMI#0` | chapter | `Scena15_EventObjects` |
| `0x662F28..0x663003` | 219 | `ETC/GAME.EMI#0` | game | `Area_CellHook` |
| `0x6632C8..0x663660` | 920 | `ETC/BATE.EMI#0` | bate | `Menu_DrawAbilityPanel`, `Menu_DrawButtonRow`, `Menu_DrawEquipCompare`, `Menu_DrawExpPanel` +9 |
| `0x663660..0x663661` | 1 | `ETC/SHOP.EMI#0` | shop | `Menu_DrawEquipPanel` |
| `0x663661..0x6636AC` | 75 | `ETC/BATE.EMI#0` | bate | `Menu_DrawBigIcon`, `Menu_DrawEquipPanel`, `Menu_DrawExpPanel` |
| `0x6637C7..0x663970` | 425 | `ETC/BATE.EMI#0` | bate | `Menu_DrawBackdrop`, `Menu_DrawButtonRow`, `Menu_DrawEquipCompare`, `Menu_DrawPlayTime` |
| `0x663B5A..0x663D7C` | 546 | `ETC/GAME.EMI#0` | game | `Menu_PieceRect` |
| `0x663FD8..0x664118` | 320 | `ETC/SHOP.EMI#0` | shop | `FieldSave_Write`, `LoadMenu_Read`, `PartyForm_Draw`, `PartyForm_Reload` +1 |
| `0x6641E4..0x664298` | 180 | `ETC/SHOP.EMI#0` | shop | `SharedList_DrawItemCount`, `SharedList_DrawList`, `SharedList_DrawMember` |
| `0x6642A4..0x6644B8` | 532 | `ETC/SISYOU.EMI#0` | sisyou | `MasterPanel_DrawMember`, `MasterPanel_DrawStats` |
| `0x6644B8..0x6644E8` | 48 | `ETC/BATE.EMI#0` | bate | `MasterPanel_DrawFace` |
| `0x669AC0..0x669CCC` | 524 | `ETC/GAME.EMI#0` | game | `Encounter_FillSlots` |
| `0x66A4B0..0x66ADBA` | 2314 | `WORLD00/AREA030.EMI#4` | area | `Fish_Hooked`, `LeaderPanel_EffectsStep`, `LeaderPanel_SetRecords` |
| `0x66AE75..0x66AE96` | 33 | `ETC/SHISU.EMI#0` | shisu | `Win1_DrawShisuPanel` |
| `0x66AE96..0x66AF26` | 144 | `ETC/BATE.EMI#0` | bate | `Win2_DrawItemList` |
| `0x66AF3C..0x66AF89` | 77 | `BATTLE/BATTLE.EMI#15` | battle | `GeneWin_DrawChoices`, `GeneWin_DrawFormIcon`, `GeneWin_DrawListFrame` |
| `0x66B090..0x66B1DA` | 330 | `ETC/START.EMI#8` | start | `MenuList_DrawLabelList`, `MenuList_GeneWinDraw` |
| `0x66B1DA..0x66B1F0` | 22 | `BATTLE/BATTLE.EMI#15` | battle | `MenuList_GeneWinDraw` |
| `0x66B1F0..0x66B1F6` | 6 | `ETC/START.EMI#8` | start | `MenuList_GeneWinDraw` |
| `0x66B268..0x66B470` | 520 | `ETC/SHOP.EMI#0` | shop | `MasterWin_DrawList`, `ShopWin_DrawRowMenu` |
| `0x66B470..0x66B4A0` | 48 | `ETC/BATE.EMI#0` | bate | `MasterWin_DrawPortrait` |
| `0x66B4A0..0x66B4AE` | 14 | `ETC/SHOP.EMI#0` | shop | `Shop_DrawBuyDetail`, `Shop_DrawBuyList`, `Shop_DrawSellDetail` |
| `0x66B4AE..0x66B534` | 134 | `BATTLE/BATTLE.EMI#15` | battle | `BattleEquipWin_Draw`, `BattleEquipWin_DrawItems`, `BattleMenu_DrawItemList`, `BattleMenu_DrawSkillList` +1 |
