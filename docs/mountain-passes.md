# Mountain passes on the world map: Mt. Zublo and Mt. Levett (DIV-0191)

**Status:** BUILT (2026-10-30) - the owner's question of the evening, read
live on their chapter-10 game with `tools/cell_peek.py` and the exe; the
fix on `fix/mountain-passes`, owed the owner's play.

The owner's report: once Mt. Zublo's road is open (adult Ryu) and once Mt.
Levett has been walked through, the world map still stops the party at each
mountain's entrance and makes them load the area and walk its road every
time, although the map is drawn as if the road went over. "Is there a simple
flag for those that never gets set?" - no: neither mountain is a flag, and
the two are blocked by different things.

## 1. How a world-map step is judged

A step on the world map is accepted or refused by `Field_CellKind`
(`src/game/member_sprites.cpp`) from the byte of the cell stepped onto and
the cell stepped from (the agent's read of 2026-10-30, every claim a
`file:line` in the code; the symbol `Field_InputFlags` and its bit 0 "on
foot" stay hypotheses):

| byte | the step | on arrival |
|---|---|---|
| `0x00` | passes | the road |
| `0x10`, `0x40`, `0xF5` | refused | ground, sea, a tree |
| `0xA0` / `0xA1` / `0xAE` | passes, place to place too | a place cell: the plate shows, confirm enters |
| `0xC0` | passes | `Area_LinkAt` arms the link (the pending area, cells, flags); nothing loads yet |
| `0xC0` -> `0xA1` or `0xAF` | the slope path | `Field_LeaderStepTarget` case 2: a link at the cell sets `Field_Request` 5 - **the load** |
| `0x91` | passes | a passage cell |
| `0xAF` | passes | `Field_ChangeArea` of whatever is pending - the stored exit |
| any, with an object in the cell ahead | refused | `Field_CellKind` 0xFF |

So a mountain can block the party three ways: ground cells where the drawn
road is, a link cell in the only path (a step from it onto the place cell
loads the area), or an object standing on the road.

Story-gated passages elsewhere are the first kind opened by code: areas 18,
33, 49, 50 and 67 carry field-script handlers that write cells
(`Area33_ClearCellsA`, `Area49_ClearCells`, ... `AreaMap_SetByte`), called
by the area's own script under a flag test, every time the map loads. The
map on disc never changes; the flags are the save's, read by the script.

## 2. Mt. Zublo (Rhapala map, area 87)

Sampled with the owner standing on each door (`tools/cell_peek.py`, 80 x 80
cells, rows z, columns x):

```
      32 33 34 35 36 37 38 39
z 1A  10 AF 10 10 10 10 10 10
z 1B  10 C0 91 C0 A1 10 10 10      A1 (0x36, 0x1B): the volcano's upper door
z 1C  10 10 10 10 10 10 10 10
z 1D  10 10 10 F5 10 A1 10 10      A1 (0x37, 0x1D): the lower door
z 1E  10 10 10 10 10 C0 10 10      C0 (0x37, 0x1E): its link (a choice-table link)
z 1F  10 10 10 10 10 .. 10 F5      the south road
```

The link list of area 87 (descriptor `0x612488` `+0x20`, 18 entries):
`(0x35, 0x1B)` -> area 195 (the volcano) at its cell `(0x14, 0x23)`,
`(0x33, 0x1B)` -> area 88 (the Urkan map) at `(0x34, 0x3A)`, `(0x37, 0x1E)`
-> choice table 0. Area 88's list sends `(0x34, 0x3A)` back to `(0x33,
0x1B)`. So the "exit" door's road is not a road: it is the passage `0x91`
and the stored exit `0xAF` to the Urkan map, which is why the Mt. Zublo
plate is on both maps.

**What blocks:** the `0x10` ground between the doors. No object - the map
had none placed when sampled - and no flag: area 87's placement script
places chapter-gated objects only on the town cells `(0x3C..0x3D, 0x22)`,
and its code (area 65's init `0x40FC40`, shared) writes no cell.

**When the way through is open:** area 195's placement script
(`tools/event_scan.py --area 195`) is one switch on the chapter byte
`Cond_ByteFA` (condition 0): chapters 6, 7 and 8 place a third object at
`(0x28, 0x3B)` that chapters 9 and later never place. That object is the
road block the owner remembers as a child; chapter 9 is "adult". The
volcano has no handlers and no init (`+0x3C` and `+0x40` both 0).

**The fix:** from chapter 9, four cells become road after the map's
placement runs: `(0x36, 0x1E)`, `(0x36, 0x1D)`, `(0x36, 0x1C)` and `(0x35,
0x1C)`. The route is then south road `(0x37, 0x1F)` -> `(0x36, 0x1E)` ->
`(0x36, 0x1D)` -> `(0x36, 0x1C)` -> `(0x35, 0x1C)` -> passage `(0x34,
0x1B)` -> link `(0x33, 0x1B)` -> stored exit `(0x33, 0x1A)`, and the same
back, never a step from a link cell onto a door. The volcano's own links
are untouched: walking onto either door from its link still enters it.

## 3. Mt. Levett (Yraall map, area 16)

```
      35 36 37 38 39 3A
z 37  10 10 10 10 .. ..        the north-east road
z 38  10 10 10 A1 C0 10        C0 (0x39, 0x38) -> area 70 at (0x38, 0x2F)
z 39  10 10 A1 A1 10 10
z 3A  10 A1 A1 10 10 10
z 3B  .. C0 10 10 10 10        C0 (0x36, 0x3B) -> area 70 at (0x16, 0x3A)
z 3C  .. 10 10 10 10 10        the south-west road
```

The mountain is a band of five place cells between two link cells, each
link to area 70 (Mt. Levett's interior, 100 x 80). Area 70's own list sends
its north-east exit `(0x39, 0x2E) x4` to `(0x39, 0x38)` and its south-west
exit `(0x15, 0x3B) x4` to `(0x36, 0x3B)`: **an exit lands the party on the
link cell itself**, which is why the owner had never stood on the band's
far cell by walking. The other Yraall map, area 33, has no Mt. Levett link
at all.

**What blocks:** with the owner on `(0x38, 0x38)` facing south (direction
5 = (0, +1)), `Sprite_Objects` record 0 stood on `(0x38, 0x39)`: flags
`0x41` (in use, not drawn), state 6. Area 16's placement script ends with
`1x 15 00 52 00 03 03 00 38 00 39 ...` at `0x5E5DCC` - that object, placed
on every load with no condition, where every other object in the script is
chapter-gated. An invisible wall, literally.

**No flag records a crossing.** `tools/mem_watch.py` over the flag block
`Cond_Flags` (0x108 bytes), the chapter bytes and the area word, from
inside area 70 to the map: only the area word changed. Area 70's script
places two objects unconditionally and has no handlers and no init.

**The fix:** bit 0 of `MountainPasses_Bits`, our byte at block `+0x1040`
(`0x904A20`, section 4), is set when the party enters area 70 by one door
and comes out by the other: the door from the arrival cell in area 70
(`kPendingX` / `AreaEntry_PendingZ` at placement time: `(0x38, 0x2F)` the
north-east, `(0x16, 0x3A)` the south-west), the exit from the arrival cell
in area 16 (the link cell). With the bit set, the object on `(0x38, 0x39)`
is dropped (`+0` = 0) after area 16's placement, and the band walks place to
place. Backing out of the door you came in by sets nothing.

## 4. The byte of ours in the save block

The live save block is `0x9039E0`, `0x12B0` bytes, built whole by
`Save_BuildBlock` and copied back whole by a load. Its `+0x103D..+0x10AF`
(`0x904A1D..0x904A8F`) has no named symbol, no absolute operand anywhere in
`analysis/pc_xref.json`, reads all zero on the owner's chapter-10 game, and
lies below the US 42-hour save's last non-zero byte `+0x103C`
([`save-interchange.md`](save-interchange.md) section 2a); task 0's first
body zeroes `0x10B0` bytes from the block's start at every launch
([`mode-flow.md`](mode-flow.md)). `MountainPasses_Bits` is `+0x1040`. Open:
whether New Game inside one process re-zeroes it (the launch does; a game
started after loading a crossed save in the same process is the case to
try).

## 5. Where it runs

`Area_RunPlacement` (ours, `src/game/event_script.cpp`), at its end. The
mode-5 step `0x594E60` (Capcom's) stores `Game_AreaNumber` at `0x594EF2`,
sets the cell plane at `0x594F76` (`0x56F670`) and calls the placement at
`0x59501F`, then the descriptor's init at `0x59503E` - so at the hook the
area, the plane and the link's pending cells are all current. The switch
`MountainPasses` is armed by `PatchBytes` after the group's self-test, which
compares the runner with Capcom's; `BOF3X_ORIGINAL=MountainPasses` leaves
every cell, object and byte as the original has them.

## 6. Owed

- The owner's play: Zublo from the south stairs to the Urkan map and back;
  Levett once through area 70 and then over the band both ways; the log
  lines `DIV-0191`.
- The four Zublo cells against the drawn path (the owner's screenshot
  outlines the walkway round the hut); move them by eye if the art says so.
- New Game after a crossed save in one process (section 4).
- `tools/cell_peek.py` is the sampler this used; `docs/event-ops.md` line
  172's "0 -> 1 (walk)" and the comment at `event_ops.cpp` `Field_LeaderStepTarget`
  read inverted against the code (the agent's note), not fixed here.
