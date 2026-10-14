# The off-screen flag split and the Capcom-valued culls (ultrawide round, group UC2)

**Status:** BUILT (2026-10-13), stage B of
[`ultrawide-round.md`](ultrawide-round.md), on `caf7b768`. Self-tested
headless at `BOF3X_WIDE` unset, 1 and 2. Nothing here has been seen in play.
This builds the owner's two calls of section 4a there, from UC's table
([`ultrawide-culls.md`](ultrawide-culls.md) section 7):

**Numbered at the round's fold-in (2026-10-14):** draft B (section 7) is DIV-0041's 2026-10-13 amendment "the off-screen flag split", draft C its amendment "Capcom's remaining culls"; the defects are D290, D293 and D294.

- **Split the off-screen flag.** `Sprite_Draw` still draws by the widened
  bounds. Bit 7 of byte 0 is now set by Capcom's `[-64, 384]` at every
  width.
- **Widen the culls that still hold Capcom's values, at both widths**,
  unless reading one shows it gates logic.

## 0. The answer

- **The split is in** (section 1). Narrow, 16:9 and 21:9 now give every
  reader of bit 7 Capcom's decision. That restores, at 16:9, what the game
  did before 2026-09-30. A new fuzz checks that the flag matches Capcom's at
  the build's own columns (16,000 rounds, 0 mismatches at 53 and at 120).
  Its control, which moves the flag with the picture as the 2026-09-30
  build did, is refused at both widths.
- **Nine readers, all re-read; none reads the draw's result any other way.**
  Two scans that UC's shape missed (any base register; the field's object
  arrays by displacement) found no tenth reader of bit 7 alone. They did
  find `Sprite_FindFree` and `Effect_FindFree`, which test the whole of
  byte 0 for 0 and so read bit 7 too (section 1.2). The split gives them
  Capcom's value as well.
- **Four of the six bounds moved by the columns. Two were left because
  they gate logic** (section 3):
  - **Moved:** `MapCell_DrawTexQuads`, `MapCell_DrawGroundSprite`,
    `EffectKind18Sub0D_DrawGlow`, and `Sprite_ProjectA`'s draw half.
  - **Left:** `EffectKind18Sub1C_OnScreen` and `BreathMote_Fly`.
    - `EffectKind18Sub1C_OnScreen` gates `Sound_PlayEffect(0x200)`, which
      writes `Sound_Channels` and stops whatever plays on its channel.
    - `BreathMote_Fly` gates `BreathMote_DrawRing`, which calls `Rand`,
      the CRT generator that battle damage also draws from. Widening the
      mote's cull would change the random stream.
- **`Sprite_ProjectA`'s draw is widened without touching the original's
  draw list** (section 2). The list holds 40 entries. A band sprite that
  took a slot could push out a sprite the original draws, and that sprite's
  bit 7 would then go stale. So a band sprite is noted on a side list and
  appended just before the draw pass, into whatever slots the frame left. A
  self-test checks it: 4,000 rounds of `Sprite_ProjectA` with the band armed,
  compared with Capcom's over the whole state, at 0 mismatches.

## 1. The split: `Sprite_Draw` `0x5935B0`

### 1.1 What changed

`sprite_draw.cpp`'s `Cull(object, x, y, draw, flag)`. Bit 7 is set when
`y` lies outside `[-0x40, 0x130]` or `x` lies outside
`[-0x40 - flag, 0x180 + flag]`, and is cleared otherwise. The object is drawn
when it lies inside the same box with `draw` in place of `flag`.
`Sprite_Draw` passes `draw = Widescreen_Fill()` and `flag = 0`. The
original's order is kept: the bit is written before anything is drawn, and
nothing else in the object changes after the cull, since the scale dwords
`+0x40` / `+0x44` are written before it as they always were. When the view is
narrow both are 0, and the test is the original's one test. The y bounds are
the same at every width.

Inside the bands (x in `(384, 384 + c]` or `[-64 - c, -64)`), the object is
drawn and bit 7 is set. This is the case the original never produces. Every
logic reader then takes the decision Capcom's code takes, and only the
picture shows more.

**Evidence that this is Capcom's own choice for a wider view:** the PSP
left this cull at `[-64, 384]` for its 32 columns
([`psp-widescreen.md`](psp-widescreen.md) section 3b, the `Actor_Task`
twin `0x258c`). It widened only MAGIC105's own sprite cull (section 3,
row 9 there). So on the PSP the flag is Capcom's by construction, and the
sprites pop at 32 px inside its edge. Ours draws into the band and keeps
the PSP's logic.

### 1.2 The readers

UC listed nine ([`ultrawide-culls.md`](ultrawide-culls.md) section 2). Each was
re-read in our source for whether it reads anything the draw produces besides
bit 7: a primitive, `SpriteCell_Count`, `Gfx_PacketNext` or the ordering
table.

| Reader | Address | Reads of the object besides bit 7 | Draw output read? |
|---|---|---|---|
| `Member_FollowStep` | `0x51B050` | positions `+0x34` / `+0x38`, `+8`, the leader's `+9` / `+0xC` / `+0x10` | no |
| `Member_CatchUp` | `0x51B5D0` | positions, `Member(0)[0x138]`, ground heights | no |
| `Scena08_Scene4` | `0x551DE0` | none (`test byte [edx*4 + 0x7DEE80], 0x80`) | no |
| `BossNina_WalkStep` | `0x439EC0` | `+0x34`, `+2` | no |
| `EffectKind32_Arc` / `_Settle` | `0x46A1E0` / `0x46A290` | the ground, `+0x3E`, `+1`, `+2` | no |
| `EffectKind34_V4Move` | `0x46CEA0` | the motion dwords | no |
| `Area65_Record8Move` | `0x40C490` | `+0xB`, the motion dwords | no |
| `Fish_Swim` | `0x52B8F0` | `+9`, `+6`, `+0x3E`, the bite globals | no |

**A scan for readers UC's shape missed.**
`uw_uc2/scan_bit7_any.py` takes any base register, not only one just loaded
from `Sprite_Current`. It matches `test byte [R], 0x80`, `cmp byte [R], 0`
followed by a sign jump, and a byte load followed within four instructions
by `test` / `and 0x80`, a sign test or `shr 7`. It gave 28 hits in 26
functions. Each of the nine is among them except `Scena08_Scene4`, which
addresses the pool by displacement. The rest were read:

- Not sprite byte 0: text lead bytes (`Text_*`, `MsgBox_Step`, `Commu*`,
  `EffectKind0F_*`), `BattleObj_IdleTint*` (`[0x939FA0]`'s byte),
  `BattleWin_MemberGaugeWait` (a target byte), `EffectKind7D_Input`
  (`[+0x4C]`'s byte), `LeaderPanel_EffectsStep`, and `Fx105_MoteDraw`
  (a frame piece's flag).
- `Field_ObjectUpdate` `0x517D47` reads the object's `+0x80` context byte.
  `MoveCmd_Move` `0x578CB1` reads the move-script object `MoveScript_Object`
  (`ebp`), not the sprite. Neither is the sprite's byte 0.

`uw_uc2/scan_bit7_pool.py` covers any byte operand whose displacement is
byte 0 of a record of `Sprite_Objects` (`0x7DEE80`, 30 x `0xA4`),
`Sprite_ObjectsExtra`, `Sprite_Kind2` or `ObjTrio`. It found
`Scena08_Scene4`, and also `Sprite_FindFree` `0x57CD90`
(`mov dl, [ecx*4 + 0x7DEE80]; test dl, dl`): the first record whose whole
byte 0 is 0. `Effect_FindFree` `0x589810` does the same over the 20
`Effect_Objects` (read in `sprite_pose.cpp`). These read bit 7 as part of a
byte. They are not "off screen" readers. `Effect_Release` zeroes an effect
record's bytes 0..4, bit 7 included. How a `Sprite_Objects` record is freed
was not read. Either way, after the split they see Capcom's bit as well.

**What the split leaves to the draw.** In the bands, `SpriteCell_Count`,
the cell table, the packet pool and the ordering table now hold more than
the original's. That has been true since 2026-09-30. None of the nine
readers reads them. A fuller packet pool can make the room test drop a
later primitive, which affects only the picture.

### 1.3 The fuzz and its control

The start-up fuzz runs before the fills are armed, so its 24,000 rounds
compare the narrow cull, as before. A second loop follows, run only when
`Widescreen_Columns()` is above 0. It does 16,000 rounds, with x biased to
the four edges `-0x40`, `0x180`, `-0x40 - c` and `0x180 + c` and to the
band between them. Ours runs with `draw = c, flag = 0`, Capcom's clone
with its own bounds, and the 0x80-byte object is compared. **The control**
runs ours again with `flag = c`, which is the build before this one. It
must differ from Capcom's in some round, or the start-up is Fatal. Narrow,
the loop logs "not run".

## 2. `Sprite_ProjectA` `0x57B860`: the draw half

`Sprite_ScreenMargins` (entry by the `+0x54` record's byte 3 bit 3): x
`[-80, 400]` for entry 0 and `[-100, 420]` for entry 1, and y by the
table. Both the flag and the place on the list keep these margins. The
change is all in `field_frame.cpp`:

- **`Sprite_ProjectA`.** When a sprite is culled for x alone and lies within
  the margins moved out by `Widescreen_Fill()`, it is noted on a side list
  of up to 40 entries (`BandNote`). The bit is set and the function returns
  as the original does.
- **`Field_DrawFrame` `0x592F00`.** This is the only caller of the draw
  pass (`call 0x593060` at `0x592F0A`, the only one in `.text`).
  `BandAppend` runs just before the pass. It appends each noted sprite
  while the list has room, skipping one whose bit 7 has been cleared since,
  one with bit 6 set, a model (`+0x24` bit 6, drawn through
  `Sprite_AddDrawRecords`, which reads `+0x40` and can leave a GTE matrix
  pushed when the pool is full), and one already on the list.
  `BandRestore` runs after the pass and puts back each appended sprite's
  `+0x40..+0x47`, the scale dwords `Sprite_Draw` writes when `+0x48` is 1.
- **Why the flag stays right for a band sprite:** its x lies outside
  `ProjectA`'s x margins, which contain `Sprite_Draw`'s `[-64, 384]`, so
  `Sprite_Draw` sets bit 7 again and the bit is the one `ProjectA` set.

The original's list therefore holds the same sprites in the same order. The
pass's bubble sort is stable, so the original entries keep their order
among themselves. Every original `Sprite_Draw` call happens as before.

**Self-test** (`BandSelfTest`, in `field_frame`'s shadow, wide only):

- 4,000 rounds of `Sprite_ProjectA` with the band armed at the build's
  columns, against Capcom's clone with the same stand-ins. The whole
  field-frame state is compared: 30 objects, 4 extra, `ObjTrio`, both
  lists and counts, and the stand-ins' log. Any sprite in the band must
  change nothing but the side list. The run also counts the sprites noted.
- A fixed case for the append: 38 entries, then six notes covering each
  skip rule and the full list. It checks the order, the count and the
  restore.

**Left, and not verified:**

- After the pass, `Sprite_Current` points at the last object drawn, which
  can be a band sprite.
- `Sprite_DrawList`'s slots past the count can hold a band pointer. The
  count is 0 after `Field_DrawFrame`, so nothing reads those slots. The
  state hash would show them as `.data` differences.
- `Sprite_Draw` writes the draw scratch for band sprites. The draw scratch
  is not the `0x903850` scratchpad.
- The side list is emptied only by `Field_DrawFrame`. If `Sprite_ProjectA`
  ran in frames with no field draw, its notes (at most 40) would wait for
  the next one. The guards (bit 7 still set, not a model, not listed) keep
  a freed record out. A stale note can only draw a sprite at a stale
  position for one frame, which affects only the picture. Whether such
  frames exist was not read.

## 3. The other five bounds

| Site | Function | Before (all widths) | After: 0 / 53 / 120 | Logic? | Done |
|---|---|---|---|---|---|
| `battle_e6.cpp` | `MapCell_DrawTexQuads` `0x4CEB40` (first vertex x, `.rdata` `0x5C4208` / `0x5C4204`, exclusive) | `(-100, 420)` | `(-100, 420)` / `(-153, 473)` / `(-220, 540)` | no: past the cull, Gte, `Prim_SetTexture` (reads `AreaMap_Header`, writes the primitive), `Gfx_CommitPrim` | **moved**: `.rdata` float minus / plus the fill, compared in our code. The floats themselves are untouched |
| `battle_e6.cpp` | `MapCell_DrawGroundSprite` `0x4CF270` (centre x, `0x5C4210` / `0x5C420C`, inclusive) | `[-60, 380]` | `[-60, 380]` / `[-113, 433]` / `[-180, 500]` | no: the frame comes from `Frame_Counter` (read only), then one FT4 | **moved** |
| `effect_5b.cpp` | `EffectKind18Sub0D_DrawGlow` `0x4FF610` (centre x, `0x5C4210` / `0x5C420C`) | `[-60, 380]` | as above | no: a draw mode, `MapView_LinkPrimAt`, four G3s, the vertex scratch | **moved** |
| `effect_5d.cpp` | `EffectKind18Sub1C_OnScreen` `0x505480` (x and y, `[-20, 340]`) | `[-20, 340]` | unchanged | **yes**: its only caller, `WaitThenSound` (sub-kinds 0x1C / 0x1D, state 1), plays `Sound_PlayEffect(0x200)` when it answers 1. `Sound_PlayEffect` (`sound.cpp`) stops the buffer on the cue's channel and writes `Sound_Channels[channel]`. Widening would change which sound a later cue stops, as well as what is heard | **left**, as `Fish_Swim`'s sound gate is left by the split |
| `field_frame.cpp` | `Sprite_ProjectA` `0x57B860` | entry 0 `[-80, 400]`, 1 `[-100, 420]` | flag and list as before; drawn to `[-80 - c, 400 + c]` / `[-100 - c, 420 + c]` | the flag and the list: yes, kept. The draw: no | **moved, draw only** (section 2) |
| `magic_s28.cpp` | `BreathMote_Fly` `0x4DECA0` (`0 < x < 320`, `0 < y < 240`) | `(0, 320)` | unchanged | **yes**: past the bound, `BreathMote_DrawRing` `0x4DEFD0` calls `Rand` (`(Rand & 7) + 2 * +0xA`, the ring's outer radius), the MSVC CRT `rand` the damage variance draws from (`symbols.toml`, `Rand`). Widening would draw one more number per frame for each mote in the band and shift every later roll | **left** |

UC's row 13 called `BreathMote_Fly` "drawing (the state has stepped
already)". It missed the `Rand` in the ring. A draw that consumes the shared
generator counts as logic for this round. Of the other draw-only culls
already moved (UC's rows 1..7), none of their functions calls `Rand`
(`grep Rand` over their files: none in `map_field_objects.cpp`,
`map_layers.cpp`, `rest_3g.cpp` or `sprite_draw.cpp`. In `map_cells.cpp`
the calls are `MapCell_DrawRising`'s, not `MapCell_DrawQuads`'s. In
`magic_s24.cpp` they are `Fx105_MoteStart`'s and `Fx106_*`'s, not
`Fx105_MoteDraw`'s).

**The two sweeps** (`EffectKind0D`, `Area172_EffectA5Grow`) are ST's and were
not touched.

**What the picture keeps popping** (Capcom's bounds, now on purpose):

- A column's start-up sound is not played for a column in the band
  (`[-20, 340]`, so 33 px inside the 16:9 edge and 100 px inside the
  21:9 one). This one is heard, not seen.
- A breath mote stops being drawn at the 320 view's edge. At 16:9 it has
  done so since the mode was built.
- `ProjectA`'s model sprites (`+0x24` bit 6) pop at the table's margins.

## 4. Measured

Builds at the branch tip with `cmake --build build`, with no diagnostic from
any touched file. Self-tests used
`BOF3X_SELFTEST_ONLY=1 BOF3X_LANG=original BOF3X_SHADOW='*'
bof3x-launcher.exe --game bof3 --no-config`, from three ini-less copies of
the launcher, `bof3x.exe` and the dll (`uw_uc2/l0`, `l1`, `l2`;
`uw_uc2/selftest.ps1`):

| `BOF3X_WIDE` | exit | `inject:` | `sprite_draw` flag split | `field_frame` band | any non-zero `MISMATCH` line |
|---|---|---|---|---|---|
| unset | 0 | 10087 ours, 0 left original | not run (narrow: no band) | not run | none |
| 1 | 0 | 10087 ours, 0 left original | 53 columns: 16,000 rounds (6,945 in the band), 0 mismatches; control refused in 4,112 | 4,000 rounds, 425 sprites noted, 0 mismatches; the append ok | none |
| 2 | 0 | 10087 ours, 0 left original | 120 columns: 16,000 rounds (7,483 in the band), 0 mismatches; control refused in 4,429 | 4,000 rounds, 545 sprites noted, 0 mismatches; the append ok | none |

At every width: `sprite_draw`'s 24,000 narrow rounds 0 mismatches (12,780
culled, 6,043 drawn); `field_frame` 24,000 rounds 0 (`Sprite_ProjectA` 1,129
shown / 871 culled); `battle_e6` five self-test lines 0; `effect_5b` 200,000
rounds 0 (`EffectKind18Sub0D_DrawGlow` reached 12,000 times); `effect_5d`
208,000 rounds 0. The `DIV-0041` lines: narrow the width-rules line only; at
`=1` also "53 columns a side (426 x 240) ..." and "full-frame fills at (-53,
0) 426 x 240"; at `=2` "120 columns a side (560 x 240) ..." and "fills at
(-120, 0) 560 x 240". `python tools/ledger_check.py`: 114 entries, 0 errors
(drafts B and C are not in the ledger yet).

## 5. Not verified

- Nothing was run live. No band sprite, glow or ground sprite has been seen
  drawn.
- The state hash was not run. It would check the split's claim directly:
  wide runs equal the narrow references wherever only bit 7 differed, apart
  from section 2's leftovers.
- That every logic read of a sprite's bit 7 is one of the nine plus the
  two whole-byte free-record scans. The scans match the shapes listed in
  section 1.2. A read through a pointer held in a stack slot and loaded
  later with a displacement of 0 into a register they do not track would
  be missed. Everything is ours, so a grep of our source backs the scans
  up: `[0] & 0x80`, `[0] >> 7` and the sign-test variants over
  `src/game/*.cpp` without the fuzz files give 38 lines. Each line was read,
  with its function where the line alone did not settle it. Those
  on a sprite object are the readers above; the rest are text lead bytes,
  battle flags, a frame piece, a CLUT-tint record, the field object's
  context byte and the move-script object.
- That no logic reads `Sprite_ProjectA`'s band sprites' `+0x40` / `+0x44`
  between `Field_DrawFrame` and the next draw. These are restored, so they
  only differ while the pass runs.
- Which objects are type `0x0A`, and how often a frame fills the 40-entry
  list. That would show how many band sprites go undrawn.
- The draw-only culls' scratch writes (the vertex scratch, `0x903850` in
  `BreathMote_DrawHex`): it is assumed, as for every draw so far, that any
  reader of a scratch word writes it first.

## 6. The owner's calls met

1. **The column sound** (`EffectKind18Sub1C_OnScreen`). Left at
   `[-20, 340]` because it changes sound state. If the owner wants the band
   heard, the change is one rule (`x` at `-20 - c`, `340 + c`, y split
   off). It needs a DIV of its own that says it moves the sound channel
   state, so it is the owner's call, not this group's.
2. **The breath mote.** Left, because its ring draws from `Rand`. A wide
   mote would need a copy of the ring that does not draw a number (for
   example, the last frame's radius kept for a mote in the band). That is a
   staging-class change to how Capcom's effect looks, not a cull.
3. **`ProjectA`'s models** in the band: drawing them means checking
   `Sprite_AddDrawRecords` for state left behind (the GTE matrix push). No
   decision is needed unless a census picture shows one.

## 7. For the other files

**`docs/DIVERGENCE.md`, draft B: an amendment of DIV-0041:**

> **Amended (ultrawide round, group UC2, [`ultrawide-culls-fix.md`](ultrawide-culls-fix.md) section 1): the off-screen flag split from the sprite cull, 2026-10-13.** `Sprite_Draw` `0x5935B0` sets bit 7 of an object's byte 0 when it culls it, and nine functions of game logic read that bit: `Member_FollowStep` and `Member_CatchUp` (a follower off screen may jump or catch up), `Scena08_Scene4` (waits for an actor to leave), `BossNina_WalkStep` (walks until off screen), `EffectKind32_Arc` / `_Settle`, `EffectKind34_V4Move` and `Area65_Record8Move` (effects that hop, land or free themselves off screen), and `Fish_Swim` (a sound) ([`ultrawide-culls.md`](ultrawide-culls.md) section 2). **The 2026-09-30 amendment above changed logic, not only the picture.** It moved that cull out by the columns, so from then until this amendment those decisions were taken at the wide edge under `BOF3X_WIDE=1` (`[-117, 437]`, 53 px past Capcom's), and "a cull's margin changes what is drawn, not logic" did not hold for this cull. The change was never ledgered or measured. *Original behaviour:* one bound `[-64, 384]` for both drawing and the bit. *New behaviour:* the object is drawn within `[-64 - c, 384 + c]` (c the columns: 0, 53 or 120), and bit 7 is set by `[-64, 384]` at every width. In the bands the object is drawn with the bit set, a state the original never produces. Every reader then decides as Capcom's code does, narrow, 16:9 and 21:9 alike, and at 16:9 that is a return to the game before 2026-09-30. *Why:* the owner, 2026-10-13 ([`ultrawide-round.md`](ultrawide-round.md) section 4a): widen the picture, not the logic. The PSP also kept this cull at `[-64, 384]` for its 32 columns ([`psp-widescreen.md`](psp-widescreen.md) 3b), so its flag is Capcom's by construction. *Verification:* `sprite_draw`'s self-test, a second loop at the build's columns (16,000 rounds, x biased to the narrow and wide edges, the object compared with Capcom's clone): 0 mismatches at 53 and at 120. Its control, the flag moved with the picture, is refused at both widths (counts in the doc's section 4). Narrow, the loop does not run and the first loop's 24,000 rounds are unchanged. *Owed:* the state hash at `BOF3X_WIDE=1` / `=2` against the narrow references. Any difference left on a route that only moves actors across the edge would be section 2's leftovers or a reader the scans missed.

**`docs/DIVERGENCE.md`, draft C: an amendment of DIV-0041 (or a new entry, if the coordinator prefers):**

> **Amended (ultrawide round, group UC2, [`ultrawide-culls-fix.md`](ultrawide-culls-fix.md) sections 2 and 3): Capcom's remaining culls, 2026-10-13.** UC found six screen-x bounds of ours still holding Capcom's values, four of them inside or within 7 px of the 16:9 view ([`ultrawide-culls.md`](ultrawide-culls.md) section 1b). The owner, 2026-10-13: widen them at both widths, as DIV-0041 has done so far, an exception to the round's "16:9 bit for bit" taken on purpose. Each was read for whether it gates logic first. *Moved by the columns* (bound minus / plus `Widescreen_Fill()`, compared in our code; `.rdata`'s floats untouched): `MapCell_DrawTexQuads` `0x4CEB40` `(-100, 420)`, `MapCell_DrawGroundSprite` `0x4CF270` and `EffectKind18Sub0D_DrawGlow` `0x4FF610` `[-60, 380]` (x only; at 53 `(-153, 473)` / `[-113, 433]`, at 120 `(-220, 540)` / `[-180, 500]`). *`Sprite_ProjectA` `0x57B860`, the draw half only:* the off-screen flag and the place on `Sprite_DrawList` keep `Sprite_ScreenMargins`' margins (`[-80, 400]` / `[-100, 420]`). A sprite culled for x alone but inside the margins moved out by the columns is noted on a side list, and `Field_DrawFrame` `0x592F00` appends it just before the draw pass, into slots the frame left, then puts back the scale dwords `Sprite_Draw` wrote. The original's list, every original `Sprite_Draw` call and every flag are unchanged. A band sprite in the original's list could have pushed out one the original draws and left that one's flag stale. Model sprites (`+0x24` bit 6) are not noted. *Left at Capcom's because they gate logic:* `EffectKind18Sub1C_OnScreen` `0x505480` `[-20, 340]`, which gates `Sound_PlayEffect(0x200)`, and that writes `Sound_Channels` and stops the channel's sound; and `BreathMote_Fly` `0x4DECA0` `(0, 320)`, which gates `BreathMote_DrawRing`, a draw that calls `Rand`, the generator battle damage shares. *Original behaviour:* those bounds at every width. *New behaviour:* the four above drawn into both bands; the two left pop at Capcom's edges, inside both wide views. *Verification:* the three modules' self-tests at 0 mismatches at every width (the fills are unarmed during them, so they compare Capcom's bounds); `field_frame`'s band test at 53 and 120 (`Sprite_ProjectA` with the band armed against Capcom's clone, the whole state compared, 4,000 rounds, 0 mismatches; the append and restore checked on a fixed case). Not seen in play.

**`docs/README.md`, one row:**

> | [`ultrawide-culls-fix.md`](ultrawide-culls-fix.md) | The ultrawide round's cull fixes (group UC2, 2026-10-13): `Sprite_Draw`'s off-screen flag split from its cull, so bit 7 is Capcom's `[-64, 384]` at every width while the picture draws to the wide bound (the nine readers re-read; two wider scans find no tenth, and two whole-byte free-record tests); four of the six Capcom-valued culls moved by the columns (`MapCell_DrawTexQuads`, `MapCell_DrawGroundSprite`, `EffectKind18Sub0D_DrawGlow`, `Sprite_ProjectA`'s draw through a side list that leaves the original's 40-entry draw list untouched); two left because they gate logic (the column's sound, and the breath mote's ring, which calls `Rand`). Fuzz and control at 53 and 120. BUILT; not seen. |

**Defects for the coordinator to number:**

- *A breath mote is not drawn in the wide bands* (`BreathMote_Fly`, `(0, 320)`):
  it stops at the 320 view's edge at 16:9 and 21:9. Ours by design: its draw
  consumes `Rand` (section 3). Unseen in play.
- *A column's start-up sound is silent in the wide bands*
  (`EffectKind18Sub1C_OnScreen`, `[-20, 340]`), inside both wide views. As
  the original decides; left (section 3).
- UC's defect *"16:9: the sprite cull's off-screen flag moved with the
  picture"* is fixed by draft B. UC's *"four Capcom culls inside the 16:9
  view"* is fixed for rows 9 and 10 by draft C. Rows 11 and 13 remain, as
  the two defects above.

**`docs/STATUS.md`:** "UC2: the off-screen flag split (bit 7 Capcom's at every
width; 16:9's logic back to before 2026-09-30); four of six Capcom culls
widened at both widths, two left as logic (a sound, a `Rand`)."

**`docs/HANDOFF.md`:**

- EB-2 should run the state hash at `=1` and `=2` against the narrow
  references. Draft B predicts no difference that comes from an actor
  crossing `[-64, 384]`.
- The owner's calls of section 6.

**`docs/ultrawide-culls.md`:**

- Row 13's "drawing (the state has stepped already)" should read "logic:
  the ring draws from `Rand`".
- Section 2's nine readers gain the two whole-byte free-record tests
  (section 1.2 here).
