# The patch cells and the wide band (ultrawide round, group RN, part 2)

**Status:** MEASURED (2026-10-13), a reading pass. Branch `wide/rn2` off the
round tip `afd7e51d`. Nothing in `src/` changed. Nothing was run, live or
headless. This answers the owner's call on [`ultrawide-rand.md`](ultrawide-rand.md)
section 3.3, **measure first**: which map cells carry the area patch handlers
(`MapCell_Handlers` kinds `0x24` / `0x25`), what each one patches, under what
condition, what switches it on, and whether a cell that only the lowered
inset walks (a **band cell**) can apply it before the original's walk would.

The scripts are in the scratchpad's `uw_rn/` and are not committed. Their
output holds positions, kinds, counts and condition codes only:

- `patch_cells.py` scans the area blocks;
- `fb_sites.py` lists the triggers;
- `join.py` and `gen.py` make the tables;
- `names.py` gathers the names.

## 0. The answer

- **No area file holds a kind `0x24` or `0x25` record.** All 670 patch-cell
  records are kind `0x23`, a bare `ret`, in 659 cells of 93 areas. The scan
  covered all 200 `AREAnnn.DAT` area blocks and their 26,219 cell runs.
  `0x24` and `0x25` only exist at run time: `MoveCmd_TestFB` `0x572650` /
  `MoveCmd_TestFC` `0x572790` rewrite a cell's `0x23` records as `0x24`.
  The draw walk then applies the patch: `0x24` applies it and becomes
  `0x25`, which applies it again and becomes `0x23`. That is one
  application per display buffer
  ([`field-misc.md`](field-misc.md) 1.2). So the question is about the
  **triggers**, not the data.
- **Most patches are picture only.** `AreaMap_ApplyPatch` writes four kinds of
  record:
  - kind 0 is a texture word in the area block;
  - kind 1 is a draw item's texture;
  - kind 2 is an `AreaMap_Corners` dword (ground heights) plus
    `MapView_Redraw = 2`;
  - any other kind is a byte of `AreaMap_Bytes`.

  Only kinds 2 and other change state that logic reads. **32 records in 13
  areas** carry them (28, 43, 84, 103, 108, 127, 135, 139, 140, 170, 171,
  173, 197). The other 638 records patch textures only.
- **The triggers** (section 2). The image has 68 call sites of FB / FC.
  Every one with a fixed cell names a patch cell in this scan, which
  cross-checks both scans. Of the 32 logic records:
  - **15: never in the band (inferred).** The trigger acts on the cell the
    party stands on, steps into or interacts with. These are the object
    handlers of areas 43, 84, 108, 127, 139, 140 and 171, and area 140's
    cell hook.
  - **11: depend on where the party stands** when a scene or a map gadget
    fires them:
    - area 28: the scenario-2 entry;
    - area 103: effect kind `0x18` sub-kind `0x17`, six height cells;
    - area 135: sub-kind `0x20`;
    - areas 170 and 197: sub-kind `0x3C`;
    - area 173: sub-kind `0x5B`.
  - **6: trigger not identified**, all in area 140. These are probably the
    move-script ops FB / FC, which act on the script object's own cell, but
    that was not read.
- **What a band cell can change at all** (section 3). The patch's values are
  the same whichever walk applies them. A band cell only applies them
  **earlier**: from the frame FB fires, instead of the frame the cell enters
  the original's walk. And only when the cell is 8 to 10 ring columns from
  the focus at 16:9, or 8 to 13 at 21:9, at the field camera. Nearer, both
  walks reach it. Farther, FB cannot find it on the ring at all, in either
  mode. In that window, a non-party object on the cell sees the new heights
  or map bytes early, and a height patch's `MapView_Redraw` rebuilds the view
  earlier. Or the party leaves before the narrow walk would have reached the
  cell, and narrow never applies the patch on that visit.
- **No recipe reaches any of the 13 areas** (the headers of
  `tools/recipes/*.txt` were searched for their area numbers).

## 1. How the cells were read

**The area block.** Each `AREAnnn.DAT` carries a kind-0 chunk at tag
`0xC8000`. That is `AreaMap_Header` `0x8CB580`, the arena `0x803580` plus
`0xC8000` ([`DAT_CONTAINER.md`](DAT_CONTAINER.md)). The header fields used:

- byte 0 is the width and byte 1 the height;
- `+0x24` is `AreaMap_CellBase` (low word), the first cell run's dword;
- `+0x28` is `AreaMap_PatchBase`, the patch list's dword.

The cell runs are read as `AreaMap_FillRing` `0x571FF0` reads them
(`symbols.toml`):

- each run is headed by a dword: map y in byte 0, map x in byte 1, its
  length in dwords in the high half;
- runs follow one another until a zero dword;
- records start at the head plus 1 and step by their byte `+2`, as
  `DrawLayer_Open` steps them.

A record of kind `0x23` / `0x24` / `0x25` names patch-list entry
`low word + PatchBase`. That entry's head has the `Area_TestCondition` code
in its low word and its length in its high word. Its records are read by
`AreaMap_ApplyPatch`'s steps (kind 0 and 2: 3 dwords; others: 2).

238 DATs carry a kind-0 chunk in the area block's range: the 200 area files,
37 `MAGIC*.DAT` and one test file. Only the 200 area files are counted here;
the others hold no patch cells.

**The condition.** `Area_TestCondition` ([`field-misc.md`](field-misc.md) 1.3,
`map_cells.cpp`) gives `c`, and the patch applies dword `+4 + 4c`. The codes
seen are:

| Code | Means | Records |
|---|---|--:|
| `0x8000` | **switch**: always false until FB sets bit 0 (`0x8001`, always true); FC clears it | 505 |
| `0x4000` / `0x4001` | always `c = 0` / `c = 1` (FB rewrites the record but cannot switch the entry) | 121 |
| `0x14nn` / `0x34nn` | story flag `nn` set / clear (`Cond_Flags` row `0x14` is `0x904030`, the story flags `Flags_Set` takes) | 35 |
| `0x22nn`, `0x0Dnn` and the like | another `Cond_Flags` row's bit | 3 |
| `0xFEnn` | `Cond_ByteFE == nn` | 4 |
| `0xFF00` | `Cond_ByteFF` | 2 |

**The camera.** `Field_ViewReset` sets the yaw to `0x200` and the distance
to 0, so the original inset is 6 ([`ultrawide-culls.md`](ultrawide-culls.md)
3.2). The original's walk then covers 7.5 ring columns either side of the
focus. The lowered inset (3 at 16:9, 0 at 21:9) covers 10.5 and 13.5, the
whole ring ([`ultrawide-rand.md`](ultrawide-rand.md) 1, `wide_rand::Row`).
A ring column offset is `((x - fx) - (z - fz)) / 2` for map cell (x, z) and
focus cell (fx, fz) (`MapView_CellToMap`'s inverse, UC 3.1).

So a patch cell is a band cell **when FB fires** if its column offset
from the focus is more than 7.5 and at most 10.5 (16:9) or 13.5 (21:9).

Area 140's tail 41 pulls the camera to distance `0x680` and back
(`area_w3e.cpp`), which gives inset 4 while it lasts. No other patch area's
code sets the distance. A script zoom (`CameraZoom_States`, `field_c1.cpp`)
can, and was not traced per area.

## 2. The triggers

`fb_sites.py <exe> <symbols.toml>` sweeps `.text` (capstone, `skipdata`) for
every `call` / `jmp` to `0x572650` / `0x572790` and prints the instructions
before each. There are **68 sites**.

**Fixed cells.** The two pushes before the call are immediates: x is the
last push, z the one before. That gives 28 sites. Five more push their cells
before an unrelated store, and were read by hand:

- `Area100_SpawnEffect9B` / `B7`;
- `Area171_Trigger55`;
- `EffectKind8E_Fall`;
- `BossGary_EndAwait`.

Two more compute theirs:

- `EffectKind18Sub17_Start10`, at `(0x55, 0x19)`;
- `_Settle14`, at x `0x42` and z `0x18` plus a quotient.

`Area140_CellHook` takes the cell of the matching entry of
`Area140_CellEntries` `0x62EFF8`. Its 8 entries were read from the image,
and are exactly the 8 story-flag byte cells of area 140.

**Every fixed cell matches a patch cell in the scan**, in the area whose
code holds the trigger where it is an `AreaNN_` function.

**The party's own cell.** These sites push their own position rather than a
fixed cell:

- `Field_LeaderStepTarget` and `Field_LeaderStepLands` (the leader object
  `0x802D40`);
- area 56's nine choice poses (the leader's position words `0x802D76`);
- `Area_Enter` (the entry cell);
- `Field_PendingWalkIn` (FC).

These act at the focus and are never band cells. For the leader's sites
this is read from the code. For `Area_Enter`'s and `Field_PendingWalkIn`'s
position words it is inferred from their names; the words were not traced.

**An object's own cell.** These act at the running object's own position:

- `MoveScript_GroupF` / `GroupD` (the move-script ops FB / FC);
- `Scena12_CellDoor`;
- the kind-`0x18` sub-kinds `0x1B` and `0x42`;
- `Area140_Effect71End` (the effect sits on the hook's cell).

Which cells the move scripts aim at was not read. That is how most of the
505 switch cells (doors, by their texture pairs, a guess) and area 140's
other six logic cells are likely reached.

**The object handlers** of areas 84, 108, 127, 139 and 140 (handler 0 in
each) clear the active member's `+0x80` bit 0 and test the leader's `+0x89`.
They set a story flag, call FB at a fixed cell, and clear the running
object's byte 0. These are object interactions: `Area43_Trigger58` and
`Area171_Trigger55` are `Field_ObjectTriggers` entries. The party is
therefore at or beside the object. That the cell FB names is the object's
own was **not checked against the object's placement**. The "never" class
rests on this inference.

## 3. What a band application changes

- **The values do not change.** `AreaMap_ApplyPatch` reads the condition
  when it runs. For a switch entry that is always `c = 1` once FB has fired;
  for a flag entry it is the flag's value. The narrow walk applies the same
  values when the cell enters it, unless a flag changes in between.
- **The timing does.** A band cell applies on the next drawn frame after FB.
  The narrow walk applies when the camera brings the cell within 7.5
  columns, or never, if the party leaves first. Until then:
  - **Heights** (area 103's six cells, 96 records; area 140's three): the
    ground at those cells. `AreaMap_Elevation`, the steepness and blocked
    tests, and any object walking there read it. `MapView_Redraw = 2`
    rebuilds the view at once, so the rebuild frames move too, which
    changes `Scratch_Swap`, `DamageScratch`'s byte and the draw pool
    ([`ultrawide-rand.md`](ultrawide-rand.md) 3.3).
  - **Map bytes** (`AreaMap_Bytes`, the per-cell byte that
    `Area140_BlockedAhead` tests against `0x89`): passability, as that
    reader uses it. What the other values mean was not read.
- **The party itself cannot reach a band cell** in the gap: it is at least
  8 ring columns (16 map cells along the diagonal) away, and the original's
  walk takes the cell long before the party arrives. The risk is an NPC,
  enemy or effect standing there, a scene that checks the cell from afar, or
  a state comparison (the state hash) that sees the early rebuild.

## 4. For the owner: the ones worth a look in play

None of these was looked at, and no recipe or save reaches any of them. Area
names are the sibling's `names/areas.toml` aliases or `places.toml`'s
English. "Unnamed" means neither has one. What happens in each scene comes
from the owner, not from here.

1. **Area 103 (unnamed): six height cells**, `(85, 25)` and `(66, 27..31)`,
   16 corner dwords each. A map gadget (effect kind `0x18` sub-kind `0x17`)
   switches them on in steps. The area also has a handler that shakes the
   view (`Area103_ShakeElevation`, [`area_w2d.md`](area_w2d.md)). Look for
   ground that rises or falls in a row of cells while the party stands well
   to the side. At 16:9, the first frames of the change may land early in
   the band.
2. **Area 135 (unnamed): `(39, 47)`, 2 map bytes, story flag `0x2A`.**
   Sub-kind `0x20` lowers and raises two textured strips with that flag
   ([`effect_2g.md`](effect_2g.md) 1.2). The bytes follow the strips.
3. **Areas 170 and 197 (unnamed): `(9, 15)`, 1 map byte each, story flag
   `0x67`.** Sub-kind `0x3C` ("Rise"). Area 111 (Steel Tower) has a texture
   patch at the same cell from the same effect.
4. **Area 173 (unnamed): `(48, 113)`, 3 map bytes, story flag `0x81`.**
   Sub-kind `0x5B` ("Lift").
5. **Area 28 (McNeil Rooftop): `(39, 21)`, 1 map byte.** Switched on as
   scenario 2 enters the area (`Scena02_EnterArea`). Whether the entry
   point is far enough from the cell was not read.

These are picture only, but are scenes the owner may want to watch in the
bands:

- area 24 (Dauna Mine - Ryu): the end of the Gary fight (`BossGary_EndStart`
  / `_EndAwait`, 39 item and 39 texture records; area 24 hosts set-up 1,
  [`boss_harness.md`](boss_harness.md) 8.1);
- area 132 (unnamed): a 90-texture patch in scenario 11, scene 8;
- area 40 (Inside Tower): 54 cells, 784 item records.

**The owner's call stays open:** split the patch handlers or leave them.
Leaving them changes heights and bytes only in the 11 "depends" records,
only while the party stands 8 or more ring columns away, and with the
values the narrow walk applies later. Splitting them would mean a band cell
skips its `0x24` / `0x25` record until the original's walk arrives.

## 5. Not verified

- **No run.** Every band class is reasoned from code and data, not measured.
  The party's position when a scene or gadget trigger fires was not read.
- The "never" class rests on the inference that an object handler's FB names
  the interacted object's own cell (section 2).
- Which cells the move-script ops FB / FC aim at (`MoveScript_GroupF` /
  `GroupD`, the script object's own cell). This covers area 140's six
  unidentified logic cells and most switch cells.
- What `AreaMap_Bytes` values other than `0x89` mean.
- Script camera zooms per area (only area 140's tail and the default camera
  were read).
- Names: about half the 93 areas have no English name in the sibling's
  files. Whether the switch-cell texture pairs are doors is a guess.

## 6. The tables

The per-area table lists every area with a patch cell. The second table
lists every cell that has a fixed or computed trigger, or a patch that logic
reads. The columns:

- **Condition:** the entry's `Area_TestCondition` code, decoded.
- **Effect:** "logic" when the patch writes heights or map bytes.
- **Band:** section 0's classes.

At 21:9 the band is the whole ring past 7.5 columns, so "depends" means
16:9 within 10.5 columns and 21:9 within 13.5.

### Per area

| Area | Name | Map | Cells | Records | Patch records by class | Conditions |
|---|---|---|--:|--:|---|---|
| 0 | MacNeil Village - Spring | 100 x 80 | 12 | 12 | texture 24 | switch (FB on / FC off) 12 |
| 1 | Dauna Mines - Outside Entrance | 90 x 70 | 6 | 6 | texture 12 | switch (FB on / FC off) 6 |
| 3 | Cedar Woods Pond - Spring | 90 x 65 | 2 | 2 | texture 4 | switch (FB on / FC off) 2 |
| 5 | Cedar Woods Treehouse - Spring | 120 x 50 | 5 | 5 | texture 9 | switch (FB on / FC off) 5 |
| 7 | MacNeil Village - Fall | 100 x 80 | 12 | 12 | texture 24 | switch (FB on / FC off) 12 |
| 8 | Cedar Woods Pond - Fall | 90 x 65 | 2 | 2 | texture 4 | switch (FB on / FC off) 2 |
| 10 | Cedar Woods Treehouse - Fall | 120 x 50 | 5 | 5 | texture 9 | switch (FB on / FC off) 5 |
| 15 | Farm (Outside Manor) - Night | 80 x 85 | 2 | 2 | texture 6 | switch (FB on / FC off) 2 |
| 17 | Farm (Outside Manor) | 80 x 85 | 2 | 2 | texture 6 | switch (FB on / FC off) 2 |
| 23 | Glauss Mountain - Fall | 80 x 102 | 4 | 4 | texture 8 | Cond_ByteFF (the byte, XOR bit 0) 2, switch (FB on / FC off) 2 |
| 24 | Dauna Mine - Ryu | 200 x 40 | 2 | 2 | item 39, texture 39 | always c = 0 (0x40) 1, bit 0x5 of Cond_Flags + 0x0 set 1 |
| 26 | McNeil Manor (Gardens) | 82 x 95 | 2 | 2 | texture 4 | switch (FB on / FC off) 2 |
| 27 | McNeil Manor | 100 x 80 | 6 | 6 | texture 12 | switch (FB on / FC off) 6 |
| 28 | McNeil Rooftop | 80 x 100 | 1 | 1 | byte 2 | bit 0x16 of Cond_Flags + 0x10 clear 1 |
| 36 | unnamed | 80 x 44 | 2 | 2 | texture 4 | switch (FB on / FC off) 2 |
| 37 | Wyndia | 114 x 70 | 14 | 14 | texture 28 | switch (FB on / FC off) 14 |
| 38 | unnamed | 130 x 60 | 18 | 18 | texture 48 | switch (FB on / FC off) 18 |
| 39 | Wyndia Castle - Throne Room | 100 x 80 | 2 | 2 | texture 4 | switch (FB on / FC off) 2 |
| 40 | Inside Tower | 200 x 40 | 54 | 54 | item 784, texture 36 | always c = 0 (0x40) 28, switch (FB on / FC off) 26 |
| 42 | Tower | 129 x 62 | 4 | 4 | texture 8 | switch (FB on / FC off) 4 |
| 43 | Checkpoint | 100 x 40 | 4 | 5 | byte 2, texture 8 | switch (FB on / FC off) 4, story flag 0x7f clear 1 |
| 44 | Upper Tower | 200 x 40 | 14 | 14 | texture 28 | switch (FB on / FC off) 14 |
| 46 | Checkpoint | 100 x 80 | 4 | 4 | texture 8 | switch (FB on / FC off) 4 |
| 47 | Wyndia Castle Wall | 133 x 60 | 4 | 4 | texture 8 | switch (FB on / FC off) 4 |
| 48 | Tower Top | 160 x 50 | 5 | 5 | texture 13 | switch (FB on / FC off) 5 |
| 49 | Research Plant | 82 x 97 | 6 | 6 | texture 12 | switch (FB on / FC off) 6 |
| 56 | Wyndia - Tomb Exit | 160 x 60 | 58 | 58 | item 248 | always c = 0 (0x40) 58 |
| 57 | Rapala | 94 x 85 | 10 | 10 | texture 20 | switch (FB on / FC off) 10 |
| 58 | Junk Town | 145 x 55 | 20 | 20 | texture 40 | switch (FB on / FC off) 20 |
| 65 | World Map - Wyndia East | 80 x 80 | 1 | 1 | item 1 | always c = 0 (0x40) 1 |
| 67 | Maekyss Gorge | 65 x 100 | 2 | 2 | texture 4 | switch (FB on / FC off) 2 |
| 68 | Tea Shop | 80 x 100 | 2 | 2 | texture 4 | switch (FB on / FC off) 2 |
| 69 | Eggnog Highway | 100 x 80 | 2 | 2 | texture 4 | switch (FB on / FC off) 2 |
| 70 | unnamed | 100 x 80 | 6 | 6 | item 72 | always c = 0 (0x40) 6 |
| 76 | unnamed | 80 x 100 | 4 | 4 | texture 8 | switch (FB on / FC off) 4 |
| 78 | unnamed | 44 x 181 | 2 | 2 | texture 4 | switch (FB on / FC off) 2 |
| 79 | unnamed | 222 x 36 | 14 | 14 | texture 28 | switch (FB on / FC off) 14 |
| 80 | unnamed | 222 x 36 | 14 | 14 | texture 28 | switch (FB on / FC off) 14 |
| 81 | unnamed | 222 x 36 | 14 | 14 | texture 28 | switch (FB on / FC off) 14 |
| 84 | unnamed | 80 x 100 | 1 | 2 | byte 2, texture 2 | story flag 0x31 set 1, story flag 0x31 clear 1 |
| 86 | Container Ship | 70 x 75 | 2 | 2 | texture 4 | switch (FB on / FC off) 2 |
| 92 | unnamed | 94 x 85 | 10 | 10 | texture 20 | switch (FB on / FC off) 10 |
| 94 | Rapala Docks | 100 x 80 | 2 | 2 | texture 4 | switch (FB on / FC off) 2 |
| 99 | Boumou Mountain | 53 x 181 | 5 | 5 | texture 9 | switch (FB on / FC off) 4, always c = 1 (0x40) 1 |
| 100 | Relay Point B | 80 x 60 | 5 | 6 | item 2, texture 10 | switch (FB on / FC off) 4, story flag 0x40 set 1, always c = 0 (0x40) 1 |
| 101 | unnamed | 80 x 60 | 2 | 2 | texture 4 | switch (FB on / FC off) 2 |
| 103 | unnamed | 100 x 80 | 7 | 7 | height 96, texture 16 | switch (FB on / FC off) 7 |
| 106 | unnamed | 100 x 80 | 1 | 1 | texture 6 | story flag 0x50 clear 1 |
| 108 | unnamed | 80 x 100 | 10 | 11 | byte 2, item 20, texture 33 | always c = 0 (0x40) 4, story flag 0x30 set 1, story flag 0x30 clear 1, story flag 0x1d set 1, story flag 0x1b clear 1, story flag 0x1a set 1, story flag 0x19 set 1, story flag 0x18 set 1 |
| 111 | Steel Tower | 32 x 250 | 15 | 15 | item 16, texture 24 | switch (FB on / FC off) 10, always c = 0 (0x40) 4, story flag 0x28 set 1 |
| 112 | Relay Point A | 80 x 100 | 12 | 12 | texture 24 | switch (FB on / FC off) 12 |
| 114 | Urukan ? | 80 x 60 | 2 | 2 | texture 4 | switch (FB on / FC off) 2 |
| 116 | unnamed | 80 x 60 | 2 | 2 | texture 4 | switch (FB on / FC off) 2 |
| 117 | unnamed | 100 x 80 | 8 | 8 | texture 16 | switch (FB on / FC off) 8 |
| 118 | unnamed | 80 x 100 | 2 | 2 | texture 4 | switch (FB on / FC off) 2 |
| 120 | Cliff | 60 x 130 | 4 | 4 | texture 8 | switch (FB on / FC off) 4 |
| 126 | unnamed | 120 x 60 | 6 | 6 | texture 12 | switch (FB on / FC off) 6 |
| 127 | unnamed | 94 x 85 | 1 | 2 | byte 2, texture 3 | story flag 0x32 set 1, story flag 0x32 clear 1 |
| 128 | Dock | 100 x 80 | 4 | 4 | texture 8 | switch (FB on / FC off) 4 |
| 130 | unnamed | 55 x 145 | 8 | 8 | texture 16 | switch (FB on / FC off) 8 |
| 131 | unnamed | 80 x 100 | 4 | 4 | texture 8 | switch (FB on / FC off) 4 |
| 132 | unnamed | 100 x 80 | 11 | 11 | texture 110 | switch (FB on / FC off) 11 |
| 133 | unnamed | 80 x 100 | 10 | 10 | texture 20 | switch (FB on / FC off) 10 |
| 134 | Freighter | 120 x 60 | 6 | 6 | texture 12 | switch (FB on / FC off) 6 |
| 135 | unnamed | 50 x 160 | 19 | 21 | byte 4, texture 40 | switch (FB on / FC off) 20, story flag 0x2a clear 1 |
| 136 | unnamed | 60 x 130 | 10 | 10 | texture 20 | switch (FB on / FC off) 10 |
| 137 | unnamed | 160 x 50 | 6 | 6 | texture 12 | switch (FB on / FC off) 6 |
| 139 | unnamed | 100 x 80 | 1 | 2 | byte 2, texture 2 | story flag 0x4d set 1, story flag 0x4d clear 1 |
| 140 | unnamed | 100 x 80 | 23 | 25 | byte 129, height 12, texture 18 | switch (FB on / FC off) 11, story flag 0x54 set 2, story flag 0x54 clear 1, story flag 0x62 set 1, story flag 0x65 set 1, story flag 0x64 set 1, story flag 0x62 clear 1, story flag 0x63 set 1, story flag 0x61 set 1, story flag 0x60 set 1, story flag 0x5f set 1, story flag 0x5e set 1, story flag 0x5e clear 1, story flag 0x5f clear 1 |
| 141 | Ryu's Dream 1 | 107 x 74 | 16 | 16 | item 128 | always c = 0 (0x40) 16 |
| 143 | Dragonier | 70 x 114 | 1 | 1 | texture 2 | Cond_ByteFE == 0x1 1 |
| 145 | Colony | 72 x 110 | 1 | 1 | texture 2 | bit 0xa of Cond_Flags + 0x68 set 1 |
| 150 | Oasis | 80 x 100 | 8 | 8 | texture 16 | switch (FB on / FC off) 8 |
| 155 | Kombinat | 100 x 80 | 4 | 4 | texture 8 | switch (FB on / FC off) 4 |
| 168 | Station Myria | 175 x 45 | 6 | 6 | texture 12 | switch (FB on / FC off) 6 |
| 169 | unnamed | 50 x 80 | 2 | 2 | texture 4 | switch (FB on / FC off) 2 |
| 170 | unnamed | 50 x 160 | 14 | 14 | byte 2, texture 77 | switch (FB on / FC off) 12, story flag 0x67 clear 1, always c = 1 (0x40) 1 |
| 171 | Station Myria - Garden | 65 x 120 | 8 | 9 | byte 2, texture 16 | switch (FB on / FC off) 6, Cond_ByteFE == 0x1 2, Cond_ByteFE == 0x0 1 |
| 173 | unnamed | 60 x 130 | 1 | 1 | byte 6 | story flag 0x81 clear 1 |
| 175 | Fairy Village | 80 x 100 | 2 | 2 | texture 4 | switch (FB on / FC off) 2 |
| 176 | unnamed | 80 x 100 | 2 | 2 | texture 4 | switch (FB on / FC off) 2 |
| 177 | unnamed | 80 x 100 | 4 | 4 | texture 8 | switch (FB on / FC off) 4 |
| 178 | unnamed | 80 x 100 | 4 | 4 | texture 8 | switch (FB on / FC off) 4 |
| 179 | unnamed | 80 x 100 | 4 | 4 | texture 8 | switch (FB on / FC off) 4 |
| 180 | unnamed | 80 x 100 | 4 | 4 | texture 8 | switch (FB on / FC off) 4 |
| 181 | unnamed | 80 x 100 | 4 | 4 | texture 8 | switch (FB on / FC off) 4 |
| 182 | unnamed | 80 x 100 | 4 | 4 | texture 8 | switch (FB on / FC off) 4 |
| 183 | unnamed | 80 x 100 | 4 | 4 | texture 8 | switch (FB on / FC off) 4 |
| 184 | unnamed | 80 x 100 | 4 | 4 | texture 8 | switch (FB on / FC off) 4 |
| 185 | unnamed | 80 x 100 | 4 | 4 | texture 8 | switch (FB on / FC off) 4 |
| 187 | unnamed | 80 x 100 | 4 | 4 | texture 8 | switch (FB on / FC off) 4 |
| 188 | Freighter | 60 x 130 | 10 | 10 | texture 20 | switch (FB on / FC off) 10 |
| 197 | unnamed | 50 x 160 | 2 | 2 | byte 2, texture 53 | story flag 0x67 clear 1, switch (FB on / FC off) 1 |

### Cells with a fixed-coordinate trigger or a logic patch

| Area | Name | Cell (x, z) | Patch (records) | Effect | Condition | Trigger | Band |
|---|---|---|---|---|---|---|---|
| 23 | Glauss Mountain - Fall | (73, 12) | texture 2 | picture | Cond_ByteFF (the byte, XOR bit 0) | FB `Scena01_EnterArea` | picture only |
| 24 | Dauna Mine - Ryu | (99, 20) | item 39 | picture | always c = 0 (0x40) | FB `BossGary_EndStart` | picture only |
| 24 | Dauna Mine - Ryu | (100, 20) | texture 39 | picture | bit 0x5 of Cond_Flags + 0x0 set | FB `BossGary_EndAwait` | picture only |
| 28 | McNeil Rooftop | (39, 21) | byte 2 | logic | bit 0x16 of Cond_Flags + 0x10 clear | FB `Scena02_EnterArea` | depends on where the party stands |
| 40 | Inside Tower | (87, 18) | texture 2 | picture | switch (FB on / FC off) | FB `Scena04_Scene1` | picture only |
| 43 | Checkpoint | (68, 11) | texture 2 | picture | switch (FB on / FC off) | FB `Area43_Trigger58` | picture only |
| 43 | Checkpoint | (68, 11) | byte 2 | logic | story flag 0x7f clear | FB `Area43_Trigger58` | never (party at or beside the cell; inferred) |
| 49 | Research Plant | (11, 84) | texture 2 | picture | switch (FB on / FC off) | FB `Scena09_Run4` | picture only |
| 84 | unnamed | (9, 16) | texture 2 | picture | story flag 0x31 set | FB `Area84_SetFlag31At5` | picture only |
| 84 | unnamed | (9, 16) | byte 2 | logic | story flag 0x31 clear | FB `Area84_SetFlag31At5` | never (party at or beside the cell; inferred) |
| 86 | Container Ship | (12, 9) | texture 2 | picture | switch (FB on / FC off) | FB `Scena13_Run4` | picture only |
| 100 | Relay Point B | (74, 30) | texture 2 | picture | switch (FB on / FC off) | FB `Area100_Trigger16` | picture only |
| 100 | Relay Point B | (74, 47) | texture 2 | picture | story flag 0x40 set | FB `Area100_Trigger13`, FB `Area100_SpawnEffect9B`, FB `Area100_SpawnEffectB7` | picture only |
| 100 | Relay Point B | (74, 47) | item 2 | picture | always c = 0 (0x40) | FB `Area100_Trigger13`, FB `Area100_SpawnEffect9B`, FB `Area100_SpawnEffectB7` | picture only |
| 103 | unnamed | (85, 25) | height 16 | logic | switch (FB on / FC off) | FB `EffectKind18Sub17_Start10` | depends on where the party stands |
| 103 | unnamed | (67, 26) | texture 16 | picture | switch (FB on / FC off) | FB `EffectKind18Sub17_WaitCue11`, FC `EffectKind18Sub17_Close` | picture only |
| 103 | unnamed | (66, 27) | height 16 | logic | switch (FB on / FC off) | FB `EffectKind18Sub17_Settle14` | depends on where the party stands |
| 103 | unnamed | (66, 28) | height 16 | logic | switch (FB on / FC off) | FB `EffectKind18Sub17_Settle14` | depends on where the party stands |
| 103 | unnamed | (66, 29) | height 16 | logic | switch (FB on / FC off) | FB `EffectKind18Sub17_Settle14` | depends on where the party stands |
| 103 | unnamed | (66, 30) | height 16 | logic | switch (FB on / FC off) | FB `EffectKind18Sub17_Settle14` | depends on where the party stands |
| 103 | unnamed | (66, 31) | height 16 | logic | switch (FB on / FC off) | FB `EffectKind18Sub17_Settle14` | depends on where the party stands |
| 106 | unnamed | (19, 37) | texture 6 | picture | story flag 0x50 clear | FB `EffectKind18Sub41_Hold` | picture only |
| 108 | unnamed | (1, 11) | texture 2 | picture | story flag 0x30 set | FB `Area108_FlagIfEffectState5` | picture only |
| 108 | unnamed | (1, 11) | byte 2 | logic | story flag 0x30 clear | FB `Area108_FlagIfEffectState5` | never (party at or beside the cell; inferred) |
| 108 | unnamed | (32, 47) | texture 11 | picture | story flag 0x1d set | FB `Area108_TailPlace` | picture only |
| 111 | Steel Tower | (10, 8) | texture 2 | picture | switch (FB on / FC off) | FB `Scena13_Run5` | picture only |
| 111 | Steel Tower | (9, 15) | texture 2 | picture | switch (FB on / FC off) | FB `EffectKind18Sub3C_Rise` | picture only |
| 111 | Steel Tower | (17, 173) | texture 4 | picture | story flag 0x28 set | FB `Area111_TailGate` | picture only |
| 111 | Steel Tower | (17, 183) | item 4 | picture | always c = 0 (0x40) | FB `Area111_TailGate` | picture only |
| 127 | unnamed | (78, 51) | texture 3 | picture | story flag 0x32 set | FB `Area127_ClearActive80` | picture only |
| 127 | unnamed | (78, 51) | byte 2 | logic | story flag 0x32 clear | FB `Area127_ClearActive80` | never (party at or beside the cell; inferred) |
| 132 | unnamed | (83, 27) | texture 90 | picture | switch (FB on / FC off) | FB `Scena11_Scene8` | picture only |
| 135 | unnamed | (39, 47) | byte 4 | logic | story flag 0x2a clear | FB `EffectKind18Sub20_Lower`, FB `EffectKind18Sub20_WaitClear` | depends on where the party stands |
| 139 | unnamed | (4, 13) | texture 2 | picture | story flag 0x4d set | FB `Area139_FlagIfLeader89Is5` | picture only |
| 139 | unnamed | (4, 13) | byte 2 | logic | story flag 0x4d clear | FB `Area139_FlagIfLeader89Is5` | never (party at or beside the cell; inferred) |
| 140 | unnamed | (2, 7) | texture 1 | picture | story flag 0x54 set | FB `Area140_FlagIfLeader89Is567` | picture only |
| 140 | unnamed | (2, 7) | byte 2 | logic | story flag 0x54 clear | FB `Area140_FlagIfLeader89Is567` | never (party at or beside the cell; inferred) |
| 140 | unnamed | (2, 7) | texture 1 | picture | story flag 0x54 set | FB `Area140_FlagIfLeader89Is567` | picture only |
| 140 | unnamed | (85, 7) | byte 17 | logic | story flag 0x62 set | FB `Area140_CellHook` | never (party at or beside the cell; inferred) |
| 140 | unnamed | (70, 8) | byte 11 | logic | story flag 0x65 set | FB `Area140_CellHook` | never (party at or beside the cell; inferred) |
| 140 | unnamed | (70, 10) | byte 22 | logic | story flag 0x64 set | FB `Area140_CellHook` | never (party at or beside the cell; inferred) |
| 140 | unnamed | (88, 10) | height 4 | logic | story flag 0x62 clear | none at a fixed cell | trigger not identified |
| 140 | unnamed | (89, 11) | byte 4 | logic | switch (FB on / FC off) | none at a fixed cell | trigger not identified |
| 140 | unnamed | (72, 13) | byte 18 | logic | story flag 0x63 set | FB `Area140_CellHook` | never (party at or beside the cell; inferred) |
| 140 | unnamed | (92, 16) | byte 12 | logic | story flag 0x61 set | FB `Area140_CellHook` | never (party at or beside the cell; inferred) |
| 140 | unnamed | (80, 26) | byte 19 | logic | story flag 0x60 set | FB `Area140_CellHook` | never (party at or beside the cell; inferred) |
| 140 | unnamed | (72, 29) | height 4 | logic | story flag 0x5f set | none at a fixed cell | trigger not identified |
| 140 | unnamed | (78, 29) | height 4 | logic | story flag 0x5e set | none at a fixed cell | trigger not identified |
| 140 | unnamed | (73, 30) | byte 4 | logic | switch (FB on / FC off) | none at a fixed cell | trigger not identified |
| 140 | unnamed | (79, 30) | byte 4 | logic | switch (FB on / FC off) | none at a fixed cell | trigger not identified |
| 140 | unnamed | (83, 31) | byte 3 | logic | story flag 0x5e clear | FB `Area140_CellHook` | never (party at or beside the cell; inferred) |
| 140 | unnamed | (74, 34) | byte 13 | logic | story flag 0x5f clear | FB `Area140_CellHook` | never (party at or beside the cell; inferred) |
| 143 | Dragonier | (11, 34) | texture 2 | picture | Cond_ByteFE == 0x1 | FB `EffectKind18Sub45_Spread` | picture only |
| 145 | Colony | (64, 107) | texture 2 | picture | bit 0xa of Cond_Flags + 0x68 set | FB `Scena13_ArriveHook` | picture only |
| 150 | Oasis | (19, 37) | texture 2 | picture | switch (FB on / FC off) | FB `EffectKind18Sub41_Hold` | picture only |
| 170 | unnamed | (9, 15) | byte 2 | logic | story flag 0x67 clear | FB `EffectKind18Sub3C_Rise` | depends on where the party stands |
| 170 | unnamed | (2, 119) | texture 53 | picture | always c = 1 (0x40) | FB `EffectKind8E_Fall` | picture only |
| 171 | Station Myria - Garden | (28, 74) | texture 2 | picture | Cond_ByteFE == 0x1 | FB `Area171_Trigger55` | picture only |
| 171 | Station Myria - Garden | (28, 74) | byte 2 | logic | Cond_ByteFE == 0x0 | FB `Area171_Trigger55` | never (party at or beside the cell; inferred) |
| 173 | unnamed | (48, 113) | byte 6 | logic | story flag 0x81 clear | FB `EffectKind18Sub5B_Lift` | depends on where the party stands |
| 197 | unnamed | (9, 15) | byte 2 | logic | story flag 0x67 clear | FB `EffectKind18Sub3C_Rise` | depends on where the party stands |
| 197 | unnamed | (2, 119) | texture 53 | picture | switch (FB on / FC off) | FB `EffectKind8E_Fall` | picture only |
