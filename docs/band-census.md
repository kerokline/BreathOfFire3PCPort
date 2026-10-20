# The band census: what the wide picture shows that 320 columns did not

**Status:** BUILT (2026-10-13), headless only. Group BA of the ultrawide
round ([`ultrawide-round.md`](ultrawide-round.md) section 2). The instrument,
its self-test and the table tool are in; **no game run has written a census
yet** - that is EB's end batch (section 3 there). Not a divergence: an
instrument, no ledger entry.

The owner's case of 2026-10-13 - two party members standing still at the top
left of a forest path, in the 16:9 band, during a scene, where the original's
320 columns never showed them - is one row of this census. The census finds
every such row on a route, with the frames, the area, the object and the
shot to look at, instead of the owner finding them one by one in play.

## 0. Running it

    BOF3X_BANDLOG=<file>          the census, on (any view; narrow it writes no rows)
    BOF3X_BANDLOG_PARK=30         frames without a move before an object counts as parked (1..100000)
    BOF3X_BANDLOG_SPAN=60         a row is cut after this many frames (1..1000000)

    python tools/band_census.py <file> [--shots DIR] [--rows] [--min-frames N]
    python tools/band_census.py --self-test

A relative `BOF3X_BANDLOG` path is the game process's working directory's.
With a recipe, `--shots` takes the run's shot directory: `t<frame>.bmp`
(`BOF3X_SHOT_EVERY`) or `f<frame>.png` (`tools/recipe_shots.py`), numbered
by the recipe frame, and each scene gets the shot nearest its middle. For
EB: run each recipe with `BOF3X_WIDE=1` and `=2`, `BOF3X_BANDLOG` and
`BOF3X_SHOT_EVERY` set; the run tools do not pass `BOF3X_BANDLOG` through
yet (UW owns their width option; an environment variable set before
`input_run.py` reaches the launcher's child, as `BOF3X_SHOT_EVERY` does
for `abc_run.py` - not checked).

## 1. Where it reads

**The hook.** One line at the end of `Sprite_UpdateScreen` `0x588F20`
(ours, `src/game/sprite_screen.cpp`), after the sprite is appended to
`Sprite_DrawList`: `band_census::Seen(drawn)`. Off it is a load of one
`bool` and a branch; it writes nothing the game reads in either state. It is
armed in `InjectAll` after `Widescreen_ArmFills`, after every module's
self-test, because those fuzzes call `Sprite_UpdateScreen` with stand-in
sprites whose pointers the census must not follow.

**The rectangle** is what the draw pass draws, read from the sprite record
the way the three functions that draw it do:

- `Sprite_Draw` `0x5935B0` (ours, `sprite_draw.cpp`): nothing for flag
  bit 6 or a depth of 0; the scale - `(0x4650000 / depth) & ~0xFF` in 16.16
  for `+0x48 == 1` without `+0x24` bit 7, else `+0x40` / `+0x44` when
  `+0x48` is set, else 1; the frame `+0x54`'s data + u16 `+0x5A`, a count
  (0 draws nothing) and 5-byte pieces, each a size index into
  `SpriteCell_Sizes` `0x66A450` and signed x and y offsets.
- `SpriteCell_Add` `0x5A6790` packs w and h into a byte;
  `D3d_BuildCellTexture`'s `DrawCells` (`tex_cells.cpp`) unpacks them as
  `(b & 0xF) << 3` and `(b >> 1) & 0x78`, and takes the extent from (0, 0)
  over every cell. The census does the same.
- `D3d_DrawCellSprite` `0x5A2EB0` (`d3d_draw.cpp`): the extent times the
  scale from the screen point `+0x74` / `+0x78`, mirrored about the point
  (`x - extent * scale`) when the mode has `0x400`, which `Sprite_Draw` sets
  from `+0x2A`.

The census computes the scale itself rather than reading `+0x40` after the
draw: the hook runs before `Sprite_Draw` this frame.

**The class** of a rectangle `[left, right) x [top, bottom)` in the game's
320 x 240 units, for a picture of `c` columns a side (`Classify`):

| Class | Rule |
|---|---|
| not shown | no rows of 0..240, or `right <= -c` or `left >= 320 + c` (or a NaN) |
| inside | touches `[0, 320)` - the original showed some of it |
| 16:9, left / right | wholly left of 0 and `right > -53` / wholly right of 320 and `left < 373` |
| 21:9 only, left / right | `right <= -53` / `left >= 373` |

53 is DIV-0041's 16:9 columns: it defines what the census calls the 16:9
band, not a width the game draws at. `c` is `Widescreen_Live()` read at the
arm, so the census follows UW's `BOF3X_WIDE=2` without a change here; at 53
no rectangle can be "21:9 only", at 0 none is logged.

**The row's other columns:**

| Column | From |
|---|---|
| `area` | `Game_AreaNumber` `0x904EFC` |
| `chapter` | `Cond_ByteFA` `0x8034E0` (s8), the index of the scenario tables `0x662C80` / `0x660B84` (`Field_ModeDispatch`, `Scenario_CallA`, symbols.toml) |
| `mode` | `Game_Mode` `0x66C7E8` |
| `who`, `index` | the record's address: `obj` 0..29 in `Sprite_Objects`, `ext` 30..33 in `Sprite_ObjectsExtra` (`Sprite_ObjectByHandle`'s numbering), `party` 0..2 in `ObjTrio` (stride `0x14C`), `kind2` (`Sprite_Kind2`), else `other` -1 |
| `type` | `+6` |
| `sprite` | `+0x54`, the frame table's address: names the sprite bank (an address of the game's data, not data) |
| `first`, `last`, `seen` | `Frame_Counter` `0x937F94`; `seen` counts the frames in the row it was drawn |
| `rfirst`, `rlast` | `InputScript_Frame()`, the recipe frame (0 with no recipe) |
| `xlo`..`yhi` | the union of the rectangles over the row |
| `side`, `band` | the class: `L` / `R`, `16:9` / `21:9` |
| `parked` | section 2 |
| `scene`, `mask` | section 3 |
| `script` | section 2 (`none` for every record but the 30 field objects) |

**Rows.** A row is one object's stretch of frames with area, chapter, mode,
record, type, sprite, class, parked and scene unchanged. It ends when any of
them changes, when the object is drawn inside 320 or not shown, when it is
not seen for two frames, or after `BOF3X_BANDLOG_SPAN` frames - then a new
one starts, and `tools/band_census.py` joins the cut pieces again. The cut
is there because the run tools end the game with `taskkill /F`
(`attract_run.py` `kill_game`): no exit handler runs, so a row still open
then is lost, at most a span of it. A clean exit writes the open rows and a
`# end: N rows` line; the tool says when the line is missing.

## 2. Parked

**Still:** the record's map position (`+0x34`, `+0x38`, 16.16) and height
(`+0x3E`) unchanged for `BOF3X_BANDLOG_PARK` frames, the object seen on
every frame between (one frame's gap allowed). Screen motion from a camera
pan does not count as moving.

**The script.** For the 30 field objects the census reads the op at the
movement script's position - the area descriptor's `+0x10` table
(`Area_Descriptors` `0x667590` by the area) entry `+0x83`, position u16
`+0x8A` - as `Field_ObjectUpdate` `0x517BF0` finds it
([`movement-script.md`](movement-script.md) 1b), following each pointer only
inside `BOF3.exe`'s image (else `unread`). Read from the handlers
(`move_script.cpp`, `move_groups.cpp`):

| State | When |
|---|---|
| `end` | op `FF` (steps the position back onto itself), `86` (hands over to a `+0x18` script and stays), or the byte before the position `EF`, `D8` or `E6` (they end the script and the position has passed them); also speed index `+0x84` 0, which `Field_ObjectUpdate` never steps |
| `wait` | ops that step back onto themselves while their test holds: `FD` (`Sprite_ScriptTickOnce`), `D9` (`Field_Request` 2), `DA` (`Transition_Busy`), `E9`, `F9`, and a counter op `A0`..`AF` the flow pass stopped on; also a timed move or its wait running (`+9` or steps left `+0x87`, `Field_ObjectMotion` `0x518980`) |
| `run` | anything else |
| `none` | not a field object: the party, the kind-2 object, the extra objects, others |

`parked` is `yes` when still and the script is `end` or `wait`, `still` when
still and no script was read (`none` or `unread`), `no` otherwise. The
party members of a scene are moved by the scenario banks, not by these
scripts, so a party member standing in a band is `still`, never `yes` - the
table keeps the two apart.

## 3. The scene flag

What says a scene is running is read where the game itself decides the
leader may not move. The `mask` column holds each test, OR'd over the row:

| Bit | Test | Where the game makes it |
|---|---|---|
| 1 | `(Field_ScriptFlags \| Field_ScriptFlags2)` bit 0 | `Field_MembersFrame` `0x517350` does not run the leader's frame (`field_frame.cpp`) |
| 2 | `Field_ScriptFlags` `0x9039A2` bit `0x100` | `Field_LeaderStand` `0x52DA70` returns first thing (`field_event.cpp`); set by ops `F1` / `80` (`ScriptFlags_Set40`), cleared by `F0` / `81` / `E5` |
| 4 | `Field_ScriptFlags2` `0x905BA4` bit `0x40` | `Field_LeaderStand`'s second return |
| 8 | `Field_Request` `0x66C7D8` == 5 | its third |
| 0x10 | the leader's state `ObjTrio +1` is not 1 (`Field_LeaderControl`) | `Field_LeaderFrame` `0x52D8F0`'s table: a menu, an encounter, a passage, a swap, an area's own state - kept for the table, **not** part of `scene` |

`scene` is 1 when any of bits 1..8 is set. Not used: "the scenario bank is
active" - the chapter's vtable slot 0 runs every field frame
(`Field_ModeDispatch`), so it is not a flag; the chapter is a column
instead. **Not verified in play:** that these bits are set during the
scenes a player would call scenes, and clear in free walking. EB's runs
answer it: a route's free walking should read `scene 0` throughout.

## 4. The self-test

`BOF3X_SHADOW=band_census` (and `'*'`), in `band_census::Inject`:

1. The band edges as one-column rectangles at x = -121, -120, -54, -53, -1,
   0, 319, 320, 372, 373, 439 and 440, at 0, 53 and 120 columns - 21:9's
   bounds passed in, since `BOF3X_WIDE=2` is UW's; eleven rectangles that
   straddle an edge, touch 0..320, lie above or below the picture, or hold a
   NaN; and a sweep of every whole x from -200 to 520 at every width 0..130
   against the classes written another way (94,251 cases).
2. The rectangle of a stand-in sprite of two pieces: plain, mirrored, scaled
   by a depth of 2250 (0x8000, half size), a depth of 0 and flag bit 6.
3. The script state of sixteen ops.
4. Rows from made-up samples through the same merge the hook uses: an
   object still in the 21:9-only columns (moving, then parked; the parked
   row cut where the scene ends), walking into the 16:9 band; one parked
   700 frames and updated twice in some frames (the span's cuts, `seen`
   counted once a frame); a party member still in the right band, unseen
   five frames (stillness restarts): twelve rows, every field checked,
   recipe frames included.
5. Off: the hook records nothing.

**94,532 checks, 0 mismatches** (`l_q`, below). **Fourteen planted changes,
all refused** (`controls.py` in the session scratchpad: the change applied,
built, the self-test run alone): the left band's edge `>=` (79 mismatches),
the right's `<=` (79), the wide picture's edges (269), y untested (2),
parked a frame late (8), stillness across a gap (1), still without a script
called parked (1), no span (4), scene left out of the row key (3), the
mirror not swapped (1), the cell offsets unsigned (3), the scale ignored
(1), `FF` not an end (1), `seen` counting repeats (3). One more was dropped
as no control: unpacking the cell height as the table's raw value changes
nothing for `SpriteCell_Sizes`' 8..32.

`python tools/band_census.py --self-test`: the span pieces joined, scenes
grouped by area and frames, the nearest shot, the classes: 0 mismatches.

**The logger off is a no-op for the game:** the `sprite_screen` fuzz runs in
`SpriteScreen_Inject`, before the census is armed, and its line is
unchanged - 20,000 rounds, 5,018 appended to `Sprite_DrawList`, 0
mismatches, as at `6b4c97cc`.

## 5. What it does not see

- **The void channel is not built.** `BOF3X_BANDLOG_VOID=1` is Fatal and
  says so. The fraction of each band's columns no primitive covered needs a
  coverage pass where every primitive's vertices are known - the device's
  `DrawPrimitive` in `src/render/render_shim.cpp`, or the six
  `DrawVertices` callers in `d3d_draw.cpp` and `battle_draw.cpp` - files
  this group does not own. A bitmap of 560 columns OR'd per primitive and
  summed at the present is cheap; it is a small follow-up for whoever owns
  the renderer seam (UW, or a later round).
- **Type-`0x0A` sprites** go through `Sprite_UpdateScreenA` `0x57B830` /
  `0x57B860` (ours, `field_frame.cpp`), not `Sprite_UpdateScreen`, and the
  four extra party objects' screen pass uses it too (`Party_ExtraScreens`).
  Not hooked: `field_frame.cpp` is not this group's. One more line there
  would cover them.
- **Overlay sprites** (`+0x24` bit 7, `Sprite_QueueOverlay`) and sprites
  refused by a full draw list (40) are not drawn by this pass and not
  logged.
- **Everything not a sprite object:** the map's cells and uprights,
  effects, the battle field. An actor drawn as an effect or a map cell in a
  scene is not in the census.
- **The rectangle is the cells' bounding box**, transparent pixels included:
  a sprite whose visible pixels end before the 320 edge but whose box
  crosses it is "inside".

## 6. Runs

All headless (`BOF3X_SELFTEST_ONLY=1 BOF3X_LANG=original`, a copy of the
launcher, `bof3x.dll` and `bof3x.exe` in an ini-less scratch folder,
`--game bof3 --no-config`; `st.sh` in the session scratchpad's `uw_ba/`):

- `l_q`, `BOF3X_SHADOW=band_census,sprite_screen`, `BOF3X_WIDE=1`: exit 0,
  the two lines above.
- `l_bl`, `BOF3X_BANDLOG` set, `BOF3X_WIDE=1`: exit 0, `bandlog     BOF3X_BANDLOG
  on: ..., 53 columns a side, parked after 30 still frames, rows cut at 60
  frames`; the file holds its two header lines and no row (no frame ran).
- The `'*'` runs with `BOF3X_WIDE` unset, `=1` and `=2`: section 7.

**Not verified:** any row from a real frame. The hook's reading of a live
sprite (the frame table, the scale, the script position) is checked only
against stand-ins built from the same reading of the code; EB's first
census is its first test against the game. A quick check for EB: the
attract cycle at `BOF3X_WIDE=1` should log rows only in the field scenes,
with `scene 0` while nobody steers.

## 7. The `'*'` self-tests

`BOF3X_SHADOW='*'`, `st.sh` as above, the branch at `40294298` (the code as
committed):

| `BOF3X_WIDE` | Exit | `inject:` | `DIV-0041` lines | Census and `sprite_screen` lines |
|---|---|---|---|---|
| unset | 0 | `10087 ours, 0 left original by BOF3X_ORIGINAL` | none | 94,532 checks, 0 mismatches; 20,000 rounds, 0 mismatches |
| `1` | 0 | `10087 ours, 0 left original by BOF3X_ORIGINAL` | `widescreen: 53 columns a side; terrain cull [-150, 470]; ...` and `full-frame fills at (-53, 0) 426 x 240` | the same |
| `2` | 3 | - | - | `FATAL: BOF3X_WIDE=2: 1 or 0` - `6b4c97cc`'s `Widescreen_Columns`, before UW's merge |

No line in either log reports a non-zero mismatch count (a search of both
for `[1-9][0-9]* MISMATCHES`). The `=2` run is the base's answer, not this
branch's: it is owed again after UW merges, when `Widescreen_Live()` gives
120 and the census's arm line says `120 columns a side`.

## 8. For the other files

**A. `docs/README.md`**, a row after `state-hash.md`'s:

| [`band-census.md`](band-census.md) | The band census (`BOF3X_BANDLOG`, `tools/band_census.py`), an instrument of the ultrawide round: every field sprite drawn inside the wide picture but wholly outside the original's 320 columns, run-length merged per object - area, chapter, frames, x / y range, side, band (16:9 or 21:9 only), parked (still, its movement script at an end or a wait), scene (the four tests the game makes before the leader may move) - and the per-scene table with the nearest shot. Self-tests and fourteen controls; no game run yet (EB). BUILT. |

**B. `docs/STATUS.md` / `docs/HANDOFF.md`**: the census is built and
self-tested; its first live use is EB's. Two small follow-ups, unowned: the
void channel (section 5, the renderer seam) and the type-`0x0A` hook line
in `field_frame.cpp`.

**No ledger entry**: an instrument (it reads and writes its own file;
`BOF3X_ORIGINAL` does not apply). No defect found.
