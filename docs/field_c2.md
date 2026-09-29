# Group FC2: the field core's second half - effect kinds 0x30, 0x34, 0x3A and 0x41

**Status:** MEASURED (2026-09-29) - round twelve
([`takeover-queue-field-battle.md`](takeover-queue-field-battle.md) section
3, [`takeover-queue-round12.md`](takeover-queue-round12.md)), wave two, on
the round branch's tip `61be26e`. **44 functions ours**
(`src/game/field_c2.cpp`, shadow name `field_c2`): the cut table's 40 rows for
FC2 (`analysis/round12_cut.tsv`, the band `0x46BBF0..0x46D5ED`) and four
starts in the band that no row lists and `tools/band_rows.py` did not flag
(section 5). Each read to its last instruction with capstone and fuzzed
through the scenario harness in field mode
([`scenario_harness.md`](scenario_harness.md) section 7) without edits to it:
264,000 rounds, 0 mismatches. 166 controls planted one at a time, all refused by a count. **Fuzz only**: neither
recorded route enters any of the 44 (section 9).

| Part | Functions | Reached through |
|---|--:|---|
| Kind 0x30: the pushed object - its states 1..3, the way test, its cells, 24 shards and 8 sparks | 13 | FC1's `0x46BB30` (`Effect_KindHandlers[0x30]`) through `0x653FE4` entries 1..3; direct calls; the engine's `0x46D710` / `0x46D770` (nobody's) |
| Kind 0x34: `EffectKind34_Run` over five variants and their 14 states | 20 | `Effect_KindHandlers[0x34]` (`0x655420`), `EffectKind34_Variants` `0x653FF4`, five state tables |
| Kind 0x3A: the thrown object | 6 | `Effect_KindHandlers[0x3A]` (`0x655438`), `EffectKind3A_States` `0x654050`; FC1's `0x46B7C0` |
| Kind 0x41: the number over a member | 5 | `Effect_KindHandlers[0x41]` (`0x655454`), `EffectKind41_States` `0x65405C` |

Every name is a hypothesis from what the code does (`symbols.toml` status
`hypothesis`). "Push", "shatter", "debris", "thrown", "zenny" name the code's
shape - the leader's facing, the pieces, `Zenny_Add` in FE1's callee - not a
play-tested fact: which map objects are kind 0x30, what the player throws as
kind 0x3A and when kind 0x41 shows a number were not traced (section 9; the
owner's word, not this doc's).

## 1. What each function does

`Effect_RunObjects` (ours) makes each live record of `Effect_Objects` (20 of
0x80 bytes, `0x7E11E0`) `Sprite_Current` and calls
`Effect_KindHandlers[+5]`. The byte `+1` is a kind's state, `+2` a sub-state,
`+9` / `+0xA` frame counts, `+0xB` a flag or a member, `+0xC` / `+0x10` /
`+0x14` a step, `+0x34` / `+0x38` x / z (16.16; `+0x36` / `+0x3A` the cell),
`+0x3E` the height word. Every function's comment in `field_c2.cpp` is the full
read; each `symbols.toml` evidence string the summary.

### 1.1 Kind 0x30 (FC1's dispatcher `0x46BB30`, its table `0x653FE4`)

FC1's state 0 (`0x46BB50`) sets the record up from the area's object list and
calls `EffectKind30_ClaimCells`. Then:

| Address | Name | What |
|---|---|---|
| `0x46BBF0` | `EffectKind30_Push` | state 1: with `+0xB` set (someone asked it to move) and not in `Game_Mode` 1 or an area change (`Field_Request` 5, which frees its cells instead): the leader's facing `0x802D48`, a step of twice the facing's unit, 16 frames; its cells freed; `EffectKind30_WayBlocked` at the far end - clear: `EffectKind30_Slide`, state 2; blocked: its model copied into `0x8C5D80`, the shards and sparks set up, sound `0x10E`, state 3. Drawn (`Sprite_UpdateScreenA`) unless `Field_Request` is 3 |
| `0x46BD10` | `EffectKind30_Slide` | state 2: a step a frame; at the end in area `0x54` story flag `0x36` is set when it rests at exactly (`0x118000`, `0x4A8000`), cleared anywhere else; the cells claimed, `+0xB` 0, back to state 1 |
| `0x46BDA0` | `EffectKind30_Shatter` | state 3: `+9` frames of shards and sparks; at the end story flag `0x44` set in area `0x92`, the record released |
| `0x46BDF0` | `EffectKind30_WayBlocked(x, z)` | al: an object there (`Sprite_ObjectAt`, margin 1) is touched (`+0x80` bit 0) but does not block; any of the four cells from (x, z) solid - 1; their corner heights not all equal - 1; else 0 |
| `0x46BF40` | `EffectKind30_CellSolid(x, z)` | al 0 for the area bytes 0, `0x20`, `0xC0`, `0x80..0x8F`, `0xA0..0xAF`; else 1 |
| `0x46BF80` | `EffectKind30_ClaimCells` | its cell and those its fractions reach: height `0x10`, the old area byte kept in `+0x14`'s bytes, area byte `0x11` |
| `0x46C100` | `EffectKind30_FreeCells` | the same cells back: height 0, the kept bytes |
| `0x46C200` | `EffectKind30_ShardsInit` | the model's 24 faces into `EffectKind30_Shards` `0x92BF80`: a centre, the vertices relative to it, a velocity along the centre (`Gte_VectorNormalS` >> 6), no spin |
| `0x46C310` | `EffectKind30_ShardsStep` | each shard falls (`+0xC` up 2) and moves, then `EffectKind30_ShardTumble` |
| `0x46C360` | `EffectKind30_ShardTumble(shard, face)` | new random angles (`Rand & 0xFC0`), the face's four vertices rebuilt through the GTE (`Gte_RotMatrix`, `SetTransMatrix` 0, `SetRotMatrix`, `Gte_RotTrans`) plus the centre |
| `0x46C430` | `EffectKind30_SparksInit` | `EffectKind30_Sparks` `0x92C4C0`: shade `0x40`, size 0, eight sparks at the object with random normalised directions |
| `0x46C4B0` | `EffectKind30_SparksDraw` | the map camera (`0x494060`), each spark drawn and moved (twice as fast while `+9` is `0xC` or more); then the size grows `0x80` or the shade fades 5 |
| `0x46C550` | `EffectKind30_SparkQuad(point, size, shade)` | one semi-transparent POLY_FT4: the point projected (`0x494110`), the square size at its depth (`0x4941E0`), the corners in x87 floats at the game's 53-bit precision (`fild` the int, `fsubr` the float, `fiadd`, one rounding at the store), texture (u `0xE0..0xFF`, v `0x30..0x4F`), CLUT (`0xA0`, `0x1E3`), page (`0x2C0`, `0x100`), linked at the point (`MapView_LinkPrimAt`, `0x48`) |

### 1.2 Kind 0x34 (`Effect_KindHandlers[0x34]`)

`EffectKind34_Run` `0x46C730` jumps by `+1` through `EffectKind34_Variants`
`0x653FF4` (six: the five below and FC1's `0x46A310`); each variant's run by
`+2` through its own table.

| Variant (run, table) | States | What |
|---|---|---|
| 0 (`0x46C750`, `EffectKind34_V0States` `0x65400C`) | `0x46C770` `_V0Burst`, `0x46C810` `_V0BurstEnd`, `0x46C820` `_V0DebrisStart`, `0x46C980` `_V0DebrisFly` | the burst makes ten kind-0x34 records of variant 0 at state 2 (`+6` 0..9), sound `0x10C`, and with `+0xB` pulls the camera in (`Camera_Distance` - `0x80`, `MapView_Redraw` 3); its state 1 puts `Camera_Distance` back to 0. Each debris piece: bank `0x212` in the nine areas `EffectKind34_V0Areas` `0x65401C` lists, else `0x1B`; scattered x / z / height, a random step, falling (`+0x14` `0xFFF80000`), 16 frames drawn then 8 blinking |
| 1 (`0x46C9F0`, `_V1States` `0x654028`) | `0x46CA10` `_V1Start`, `0x46CA90` `_V1Fall` | bank `0x18`, animation 8, rising `0x40` and pulled down 8 a frame; released on the ground, blinking within `0x100` of it. `EffectKind3A_Hit` makes these |
| 2 (`0x46CB00`, `_V2States` `0x654030`) | `0x46CB20` `_V2Start`, `0x46CB40` `_V2Spawn`, `0x46CBD0` `_V2PieceStart`, `0x46CCA0` `_V2PieceFall` | a spawner making five pieces, one every four frames (variant 2 at state 2); each piece scattered, animation `0xB..0xD`, falling to the ground |
| 3 (`0x46CCF0`, `_V3States` `0x654040`) | `0x46CD10` `_V3Start`, `0x46CD90` `_V3Fall` | bank `0x18`, animation 9, falling; drawn while at or above the ground |
| 4 (`0x46CDE0`, `_V4States` `0x654048`) | `0x46CE00` `_V4Start`, `0x46CEA0` `_V4Move` | bank `0x46` (none: released), a fixed step (0, `-0x2000`, `0x40`); released when `+0` gets bit 7 |

The spawners in our code (an immediate `0x34` stored to a record's `+5`,
capstone over `.text`): `EffectKind34_V0Burst`, `_V2Spawn`,
`EffectKind3A_Hit`, and outside the band `PartyAction5_ByForm` (`0x522FDF`),
`Effect_SpawnAtCell` (`0x52489F`), `Scena11_Scene4` (`0x55C9D5`).

### 1.3 Kind 0x3A (`Effect_KindHandlers[0x3A]`)

`EffectKind3A_Run` `0x46CEF0` by `+1` through `EffectKind3A_States`
`0x654050` (three). Made by `PartyAction_Finish` (`0x51DE63`).

| Address | Name | What |
|---|---|---|
| `0x46CF10` | `EffectKind3A_Start` | ahead of the leader (the facing's half cell of `Field_DirectionSteps`, the leader's height + `0xC0`), a step of 16 units a frame, 14 frames |
| `0x46CFC0` | `EffectKind3A_Fly` | moves; released over a cell whose tile word is 0; `EffectKind3A_Hit` - takes the leader's graphics (`EffectKind3A_LeaderPose`, the leader's `+0x27`), animation `0x4A`, sound `0x10E`, state 2; else released when the 14 frames run out |
| `0x46D080` | `EffectKind3A_Wait` | the pose played out by `Sprite_ScriptTickOnce`; while a message is open (`Field_Request`) it holds, drawn while `+0xB` is 2 |
| `0x46D0E0` | `EffectKind3A_LeaderPose` | the record drawn with the leader's graphics words (`+0x2C`, `+0x4C`, `+0x29`); also FC1's `0x46B7C0` |
| `0x46D180` | `EffectKind3A_Hit` | al 1 on an object (touched), a ground far above the leader, an area byte `0x11`, or a `0xFD` cell (its own or a neighbour its fractions reach), which FE2's `0x5728D0` clears. After a `0xFD` cell, three times in sixteen (`Rand`'s low nibble above `0xC`): `+0xB` 2, FE1's `0x5307C0` with 10 (nibble `0xF`) or 5 - a system message and `Zenny_Add` of that many -, a variant-1 kind-0x34 piece at the place, and the leader's `+7` 1 |

### 1.4 Kind 0x41 (`Effect_KindHandlers[0x41]`)

`EffectKind41_Run` `0x46D400` by `+1` through `EffectKind41_States`
`0x65405C` (four). Made by `Field_Bit80Tick` (`0x535072`). The number is
`+6`, printed and drawn by `0x46D5F0` over party member `+0xB`'s screen
position (its `+0x2E` / `+0x30`), the offset `+0x38`'s high word added to y,
the colour row `+0x27`:

| Address | Name | What |
|---|---|---|
| `0x46D420` | `EffectKind41_Start` | offset -10 (with `Field_InputFlags` bit 0) or -20, a rise `0x20000`, commit slot 3, 6 frames |
| `0x46D480` | `EffectKind41_Hold` | drawn 6 frames |
| `0x46D4E0` | `EffectKind41_Bounce` | falls (`+0x10` + `0x14000` a frame) until the offset is not negative, then bounces once (the rise negated, 6 frames) |
| `0x46D570` | `EffectKind41_Fade` | 6 more frames of the fall, drawn on odd frames, then released |

### 1.5 The PSX twins

`analysis/pairs_propagated.json` pairs 31 of the 44 (tiers `table-anchored`,
`gap67`, `gap74`, `gap44`, `callers`, `call-disputed`); each twin's address is
in its `symbols.toml` evidence. The sibling names none of them
(`../BreathOfFire3Recomp/names/*.toml`, its `symbols.toml`: no entry at any
of the 31 twins). No PSX code was read. None of the seventeen field names
`symbols.toml` held before this round lies in the band.

## 2. Divergence

None: every function is a faithful replacement, no DIVERGENCE.md entry is
owed (`DIVERGENCE.md` and `cheats.cpp` name no address of the band, nor its
tables). Where the original indexes past a table ours aborts with a `Fatal`
naming the function (the owner's rule, round9 doc section 6; nothing reaches
it): the eight dispatchers past their tables; `Effect_FindFree`'s answer past
the twenty records (it answers 0..19 or `0xFF`); `Sprite_ObjectAt`'s past the
30 objects and four extras (it answers 0..`0x21` or `0xFF`; `EffectKind3A_Hit`
widens it signed, `EffectKind30_WayBlocked` unsigned - both Fatal outside the
lists). Data reads by a byte stay unchecked, as in the rest of our field
code: the leader's facing into the 8-row step tables, member `+0xB` into
`ObjTrio` (section 10).

`eax` on return: `EffectKind30_WayBlocked`, `_CellSolid` and
`EffectKind3A_Hit` answer in `al` (the rest of eax is a callee's leftover; every
caller tests `al`); every other is `void` - `EffectKind3A_LeaderPose` leaves
eax 0 and neither caller reads it.

The stack MATRIX of `EffectKind30_ShardTumble`: its padding (`+0x12`) is
never written by the original; `Gte_SetRotMatrix` copies it into `Gte_Matrix`
where nothing reads it (DIV-0021's hole). Ours zero-initialises the local.

## 3. The arguments pushed with leftovers

Where the original pushes a byte or word in a register whose upper bytes are
leftovers, ours passes the value and the fuzz lists the callee with the mask
its code reads (each read, capstone):

| Callee | Pushed | Read by the callee |
|---|---|---|
| `AreaMap_SetHeight`, `AreaMap_SetByte` | x, z as `cx` / `dx` (`inc dx` for the neighbour), the value as `cl` | x, z `s16`, the value a byte (`map_field_objects.cpp`, `scena_sx2.cpp`) |
| `EffectKind30_CellSolid` | z's high word and the caller's two stack bytes above it (`mov esi, [esp + 0x1E]`) | 16 bits each (`AreaMap_ByteAt(short, short)`) |
| `EffectKind30_SparkQuad` | the size word `cx`, the shade `al` (upper: `0x494060`'s eax, then `Sprite_Current`) | `mov ax, [esp + 0x34]`, `mov al, [esp + 0x5C]` |
| `0x46D5F0` | x `ax` (upper: the member offset), y `dx`, `+6` in `dl`, `+0x27` in `dl` | x, y `movsx` 16 bits; the third never; the fourth `and 0xFF` |
| FE2's `0x5728D0` | x, z `cx` / `dx` | `movsx` 16 bits of each (read to its last use) |

## 4. The fuzz (`field_c2_fuzz.cpp`)

One `Run` under `BOF3X_SHADOW=field_c2`, field mode, 6,000 rounds a function
(`BOF3X_FC2_ONLY=<name>` runs the clones whose name holds it). Shapes: 40
`kSprite` (every state and every helper without arguments; the dispatchers'
state bytes seeded inside their tables per function, `sprite_span` 0), four
`kCall` (`_WayBlocked`, `_CellSolid` - `ret_mask 0xFF` -, `_ShardTumble` with
a shard and a face of the fuzz's model, `_SparkQuad` with a spark record);
`EffectKind3A_Hit` `ret_mask 0xFF`. `DataTable`s: the eight state tables
(`0x653FF4` 6, `0x65400C` 4, `0x654028` 2, `0x654030` 4, `0x654040` 2,
`0x654048` 2, `0x654050` 3, `0x65405C` 4 - none bounded by a compare, each to
the next table the code names).

**Callees beyond the standard sets:** the group's own called directly, by
name (`_FreeCells`, `_ClaimCells`, `_Slide`, `_ShardsInit`, `_ShardsStep`,
`_SparksInit`, `_SparksDraw`, `EffectKind3A_LeaderPose` `kPhase`;
`EffectKind3A_Hit`, `_WayBlocked`, `_CellSolid` `kFlag` with their masks;
`_ShardTumble`, `_SparkQuad`); FE2's `0x5728D0` and FE1's `0x5307C0` by
address (section 7); and **nine standard entries re-listed** - FH's stand-ins
meet their first use here, and these are wrong for this group's callers
(each proved by reading the callee; for the fold):

| Callee | FH's row | This group's | Why |
|---|---|---|---|
| `0x494110` | 2 words, masks all, no effect | the point hashed (12), the out not logged; the out filled with three floats (random mantissa, exponent 2^-17..2^18) | the out is the caller's stack (its address differs per side); the caller reads x, y and the depth back |
| `0x4941E0` | 3 words, masks all | the point hashed, the size in hashed (4), the out not logged and filled (two s16) | as above; the caller reads (w, h) back |
| `0x46D5F0` | 4 words, masks all | `s16`, `s16`, 0, byte | section 3 |
| `Gte_RotMatrix`, `Gte_RotTrans` | the out logged by value | the out not logged (still filled) | a stack out |
| `Gte_SetTransMatrix` | the matrix's first 12 bytes hashed | its translation `+0x14..+0x1F` noted | what it reads (`psx_gte.cpp`) |
| `Gte_SetRotMatrix` | 12 bytes hashed | 18 bytes | the nine s16 (its fifth dword's high half is the padding, not compared) |
| `AreaMap_SetHeight`, `AreaMap_SetByte` | masks all | `s16`, `s16`, byte | section 3 |
| `Sprite_ObjectAt` | `kFlag` (any byte) | none half the time, else 0..`0x21` | its range; `0x22..0xFE` would make ours Fatal where the original writes outside the lists |
| `AreaMap_ByteAt`, `AreaMap_Elevation` | garbage | the bytes the callers compare (0, `0x20`, `0xC0`, `0x8x`, `0xAx`, `0x11`, `0xFD`); the round's ground two times in three | reach, not correctness: random answers never meet `0xFD` twice or four equal heights |

An effect draws from `Noise()` only: the first run drew its picks with the
harness's `Next()` (via `Pick`) and mismatched in 9,435 rounds, all the
answers of `AreaMap_ByteAt` / `AreaMap_Elevation` - the two passes do not
share that stream. `NoisePick` fixed it.

**Regions beyond field mode's:** `Game_Mode` `0x66C7E8`, the shards and
sparks `0x92BF80..0x92C5C4`, the frame buffer `0x8C5D80` (0x400: the copied
model), the fuzz's model (0x400) and count bytes (4).

**Seeds** (per round, then per function): every one of the four sprite
records `Sprite_Current` may move among has `+0x50` at the fuzz's model or
`0x8C5D80` and `+0x54` at a count byte (0, 1, `0x18`, `0x19`, `0x80`, `0xFF`,
0..`0x19`), its cell words below `0x1E`, fractions 0, `0x8000` or any, and
half the time a step inside a cell; `Game_Mode` 1 / 0 / 2 / any;
`Field_Request` 0, 3, 5, 2 or any; the area `0x54`, `0x92`, one of
`EffectKind34_V0Areas` or any; `+9` 0, 1, 2, `0xB`, `0xC`, `0xD`, `0x10`;
`+0xA` 0..2; `+0xB` 0..2; the height word against the round's ground (equal,
one either side, `0x80`, `0x100`, `0x101`, `0xFF` above), the leader's height
`0x80` / `0x81` / `0x100` / `0x101` either side of it; `Rand`'s hint near
`0xC`..`0xF`; per function: the resting place (`0x118000`, `0x4A8000`)
after the step (`_Slide`), `+9` at `0xB` / `0xC` on all four records
(`_SparksDraw`), each dispatcher's byte inside its table, `+9` 3
(`_V2Spawn`), `+0` bit 7 (`_V4Move`), the tile word the fly reaches 0 half the
time, member `+0xB` below 3 and the offset's sign (kind 0x41). **The group's
disturbance** (from its hash): `+9`, `+0xA`, `+0xB`, `+0x14`, the height word,
the sparks' shade and size, the leader's `+0x27`, `Camera_Distance` - what the
functions read again after a call.

**Self-test** (2026-09-29, this worktree, `BOF3X_SELFTEST_ONLY=1
BOF3X_SHADOW=field_c2`, exit 0): 264,000 rounds over 44 functions, 1,113,607
calls to the stand-ins, **0 mismatches**, 26,224 bytes of state (41 regions),
286 stand-ins (174 of the field-standard set). Every callee listed and every
entry of the eight tables reached (FC1's `0x46A310` 956 times); FE2's
`0x5728D0` and FE1's `0x5307C0` are reached through `EffectKind3A_Hit`'s
narrow path (592 and 150 calls).

`BOF3X_SHADOW='*'` (the same build): exit 0, `inject: 6597 ours` (6,553 + 44), 970
lines of `0 MISMATCHES`, none other; `field_c2` there 264,000 rounds, 1,113,711
calls (another stream than alone), 0 mismatches.

## 5. What the cut and the tool said, settled

- **Four starts in the band were in no list** and `band_rows.py` printed "0
  not listed": `0x46C730`, `0x46CEF0`, `0x46D400` are `Effect_KindHandlers`
  entries `0x34`, `0x3A`, `0x41` (the dwords `0x655420`, `0x655438`,
  `0x655454`), each an 18-byte dispatcher in the padding after the function
  before it; `0x46C820` is `EffectKind34_V0States[2]` (the dword `0x654014`),
  0x154 bytes between `0x46C810` and `0x46C980`. A capstone scan for dwords
  pointing into the band found them. Taken as this group's (the addendum's
  rule).
- **Extents:** the tool's are the code's; the cut's sizes differ by padding
  for 26 rows (e.g. `0x46C550` 0x1DB, the cut 480; `0x46D180` 0x274, the cut
  640), none in code.
- **Hidden in hosts:** `0x46BBF0` in FC1's `0x46BA90` (its line in
  `entries_logic.txt` covers it; not ours before - FC1's), `0x46BDA0` in
  `0x46BD10`'s old `D7` extent, `0x46C750..0x46D080` in `0x46C550`'s old
  `B89`, `0x46D420..0x46D570` in `0x46D180`'s old `46D`. None of the hosts was
  ours before this round, so no source of ours held their code.
- **No start is a case of another.** Every row is reached by a `.data` table
  entry or a direct call.

## 6. Controls

**166 planted, 166 refused, every one by a count** (none by a Fatal or a
hang), one at a time: `controls.py` in the session scratchpad (`fc2/`) plants
each in `field_c2.cpp` at its first occurrence after the function's
definition, rebuilds, runs `BOF3X_SHADOW=field_c2` with `BOF3X_FC2_ONLY` set
to the function (a helper's control runs its caller), restores the file and
rebuilds; its log is `fc2/controls_final.log`. At least two a function, and
each boundary the seeds plant; the re-reads after a call (`L6`: `+0x14`
through the pointer taken before `AreaMap_ByteAt`; `K5`: `Sprite_Current`
read after `Effect_FindFree`), the float rounding of the spark's corners
(`Q6`: a float intermediate instead of x87's 53-bit one) and the sign of a
16-bit halving (`Q9`).

**The first pass (with the seeds before the final ones) left five not
refused**, each the fuzz's fault: `G5` (`> 0x800` against `> 0x7FF` on `Rand &
0xFFF` - no draw landed on 0x800) and `V1`, `V2`, `K9`, `X3` (the falls'
equality and `+ 0x100` boundaries: the height after the add was random against
the ground). Seeded (`Rand`'s hint 0 for the debris; the fall's pull and the
height set so the height after the add meets the ground, one either side,
`0x100` above and one either side), all five refused; the table is the whole
set re-run on the final fuzz. The weakest are `W1` (7 rounds: the fourth
corner the only one differing needs three equal heights first) and `G5` (9).

No equivalent mutant was kept: three candidates were read as equivalent and
replaced by a near variant before running (reading `Sprite_Current` again
where nothing between could move it: `SparksDraw`'s last `+9`, `ClaimCells`'
cell pointer, `V2Spawn`'s `+0xA`); and `EffectKind3A_Hit`'s signed widening of
`Sprite_ObjectAt`'s answer is equivalent to an unsigned one for every answer
the callee gives (section 10), so it has no control.

| # | Function | Planted | Refused in (of 6,000) |
|---|---|---|--:|
| P1 | `EffectKind30_Push` | `if (Game_Mode != 1) {` -> `if (Game_Mode != 2) {` | 1951 |
| P2 | `EffectKind30_Push` | ` << 1);` -> ` << 2);` | 1716 |
| P3 | `EffectKind30_Push` | `S()[1] = 3;` -> `S()[1] = 4;` | 1510 |
| P4 | `EffectKind30_Push` | ` * 40;` -> ` * 39;` | 504 |
| P5 | `EffectKind30_Push` | `if (Field_Request != 3)` -> `if (Field_Request != 4)` | 773 |
| P6 | `EffectKind30_Push` | `if (Field_Request == 5)` -> `if (Field_Request == 4)` | 734 |
| P7 | `EffectKind30_Push` | `S()[9] = 0x10;` -> `S()[9] = 0x11;` | 2302 |
| P8 | `EffectKind30_Push` | `SetUL(S() + 0x50, at::kModelCopy);` -> `SetUL(S() + 0x50, at::kModelCopy + 1);` | 1524 |
| S1 | `EffectKind30_Slide` | `0x118000` -> `0x118001` | 102 |
| S2 | `EffectKind30_Slide` | `S()[1] = 1;` -> `S()[1] = 2;` | 738 |
| S3 | `EffectKind30_Slide` | `== 0x54)` -> `== 0x55)` | 202 |
| S4 | `EffectKind30_Slide` | `SH_CALL(Flags_Clear)(At(at::kStoryFlags), 0x36);` -> `SH_CALL(Flags_Clear)(At(at::kStoryFlags), 0x37);` | 99 |
| S5 | `EffectKind30_Slide` | `if (s[9] == 0) {` -> `if (s[9] == 1) {` | 1511 |
| H1 | `EffectKind30_Shatter` | `== 0x92)` -> `== 0x93)` | 200 |
| H2 | `EffectKind30_Shatter` | `kStoryFlags), 0x44);` -> `kStoryFlags), 0x45);` | 200 |
| H3 | `EffectKind30_Shatter` | `if (S()[9] != 0) {` -> `if (S()[9] != 1) {` | 1500 |
| W1 | `EffectKind30_WayBlocked` | `return (h1 != h0 \|\| h2 != h0 \|\| h3 != h0) ? 1 : 0;` -> `return (h1 != h0 \|\| h2 != h0) ? 1 : 0;` | 7 |
| W2 | `EffectKind30_WayBlocked` | `(xc + 1, zc + 1)` -> `(xc + 1, zc)` | 224 |
| W3 | `EffectKind30_WayBlocked` | `static_cast<long>(x0 + 0x10000), static_cast<long>(z0 + 0x10000)` -> `static_cast<long>(x0 + 0x10000), static_cast<long>(z0 + 0x8000)` | 75 |
| W4 | `EffectKind30_WayBlocked` | `(x, z, 1)` -> `(x, z, 2)` | 6000 |
| W5 | `Touch` | `o[0x80] \| 1` -> `o[0x80] \| 2` | 2289 |
| C1 | `EffectKind30_CellSolid` | `b == 0xC0` -> `b == 0xC1` | 571 |
| C2 | `EffectKind30_CellSolid` | `row == 0xA0` -> `row == 0xB0` | 637 |
| C3 | `EffectKind30_CellSolid` | `b == 0x20` -> `b == 0x21` | 613 |
| L1 | `EffectKind30_ClaimCells` | `CellZ(s, 0), 0x10);` -> `CellZ(s, 0), 0x11);` | 6000 |
| L2 | `EffectKind30_ClaimCells` | `<< 16);` -> `<< 8);` | 2661 |
| L3 | `EffectKind30_ClaimCells` | `(CellX(s, 1), CellZ(s, 1), 0x11)` -> `(CellX(s, 1), CellZ(s, 1), 0x12)` | 1377 |
| L4 | `EffectKind30_ClaimCells` | `SetUL(S() + 0x14, b0);` -> `SetUL(S() + 0x14, b0 + 1u);` | 5872 |
| L5 | `EffectKind30_ClaimCells` | `if (Word(s + 0x38) != 0) {` -> `if (Word(s + 0x36) != 0) {` | 2991 |
| L6 | `EffectKind30_ClaimCells` | `SetUL(cell, UL(cell) \| static_cast<U>(b) << 8);` -> `SetUL(S() + 0x14, UL(S() + 0x14) \| static_cast<U>(b) << 8);` | 75 |
| F1 | `EffectKind30_FreeCells` | `s[0x14]);` -> `s[0x15]);` | 3688 |
| F2 | `EffectKind30_FreeCells` | `>> 16));` -> `>> 15));` | 963 |
| F3 | `EffectKind30_FreeCells` | `CellZ(s, 0), 0);` -> `CellZ(s, 0), 1);` | 6000 |
| F4 | `EffectKind30_FreeCells` | `>> 24));` -> `>> 23));` | 470 |
| I1 | `EffectKind30_ShardsInit` | `>> 2));` -> `>> 1));` | 6000 |
| I2 | `EffectKind30_ShardsInit` | `SetWord(r + 0xA, static_cast<U>(SW(r + 0xA) >> 6));` -> `SetWord(r + 0xA, static_cast<U>(SW(r + 0xA) >> 5));` | 6000 |
| I3 | `EffectKind30_ShardsInit` | `SetWord(r + 0x12, 0);` -> `SetWord(r + 0x12, 1);` | 6000 |
| I4 | `EffectKind30_ShardsInit` | `Word(v + 4) - Word(r + 4)` -> `Word(v + 4) - Word(r + 2)` | 6000 |
| I5 | `EffectKind30_ShardsInit` | `long centre[3] = {SW(r + 0), SW(r + 2), SW(r + 4)};` -> `long centre[3] = {SW(r + 0), SW(r + 4), SW(r + 2)};` | 6000 |
| T1 | `EffectKind30_ShardsStep` | `Word(r + 0xC) + 2u` -> `Word(r + 0xC) + 3u` | 6000 |
| T2 | `EffectKind30_ShardsStep` | `SetWord(r + 2, Word(r + 2) + Word(r + 0xA));` -> `SetWord(r + 2, Word(r + 2) + Word(r + 0xC));` | 6000 |
| T3 | `EffectKind30_ShardsStep` | `(r, face);` -> `(r, face + 2);` | 6000 |
| U1 | `EffectKind30_ShardTumble` | `& 0xFC0u);` -> `& 0xFE0u);` | 2168 |
| U2 | `EffectKind30_ShardTumble` | `m.t[0] = m.t[1] = m.t[2] = 0;` -> `m.t[0] = m.t[1] = 0; m.t[2] = 1;` | 6000 |
| U3 | `EffectKind30_ShardTumble` | `static_cast<U>(out[1]) + Word(shard + 2)` -> `static_cast<U>(out[1]) + Word(shard + 4)` | 6000 |
| U4 | `EffectKind30_ShardTumble` | `shard + 0x18 + 8 * k` -> `shard + 0x18 + 6 * k` | 6000 |
| N1 | `EffectKind30_SparksInit` | `At(at::kSparkShade)[0] = 0x40;` -> `At(at::kSparkShade)[0] = 0x41;` | 5082 |
| N2 | `EffectKind30_SparksInit` | `- 0x80u);` -> `- 0x7Fu);` | 6000 |
| N3 | `EffectKind30_SparksInit` | `UL(p + 0x18) << 9` -> `UL(p + 0x18) << 8` | 6000 |
| N4 | `EffectKind30_SparksInit` | `SetUL(p + 8, UL(s + 0x3C));` -> `SetUL(p + 8, UL(s + 0x38));` | 6000 |
| D1 | `EffectKind30_SparksDraw` | `s[9] >= 0xC ? 1 : 0` -> `s[9] >= 0xD ? 1 : 0` | 2392 |
| D2 | `EffectKind30_SparksDraw` | `Word(At(at::kSparkSize)) + 0x80u` -> `Word(At(at::kSparkSize)) + 0x81u` | 3975 |
| D3 | `EffectKind30_SparksDraw` | `+ 0xFB)` -> `+ 0xFC)` | 2025 |
| D4 | `EffectKind30_SparksDraw` | `SetUL(p + 8, UL(p + 8) + (UL(p + 0x18) << shift));` -> `SetUL(p + 8, UL(p + 8) + (UL(p + 0x14) << shift));` | 6000 |
| Q1 | `EffectKind30_SparkQuad` | `prim[0x45] = 0x4F;` -> `prim[0x45] = 0x4E;` | 6000 |
| Q2 | `EffectKind30_SparkQuad` | `StoreFloat(prim + 0x2C, top + static_cast<double>(h));` -> `StoreFloat(prim + 0x2C, top + static_cast<double>(w));` | 6000 |
| Q3 | `EffectKind30_SparkQuad` | `(0xA0, 0x1E3)` -> `(0xA0, 0x1E2)` | 6000 |
| Q4 | `EffectKind30_SparkQuad` | `prim[6] = static_cast<unsigned char>(shade);` -> `prim[6] = static_cast<unsigned char>(shade + 1);` | 6000 |
| Q5 | `EffectKind30_SparkQuad` | `std::memcpy(prim + 0x30, &screen[2], 4);` -> `std::memcpy(prim + 0x30, &screen[1], 4);` | 6000 |
| Q6 | `EffectKind30_SparkQuad` | `const double left = static_cast<double>(screen[0]) - static_cast<double>(hw);` -> `const double left = static_cast<float>(screen[0] - static_cast<float>(hw));` | 209 |
| Q7 | `EffectKind30_SparkQuad` | `2, 0x48);` -> `2, 0x40);` | 6000 |
| Q8 | `EffectKind30_SparkQuad` | `static_cast<short>(size), static_cast<short>(size)};` -> `static_cast<short>(size), static_cast<short>(size + 1)};` | 6000 |
| Q9 | `EffectKind30_SparkQuad` | `const int hw = static_cast<short>(wh[0]) >> 1` -> `const int hw = static_cast<unsigned short>(wh[0]) >> 1` | 2955 |
| R0 | `EffectKind34_Run` | `{ Run("EffectKind34_Run"` -> `{ Sprite_Current[1] ^= 1; Run("EffectKind34_Run"` | 6000 |
| R1 | `EffectKind34_V0Run` | `AddressOf(EffectKind34_V0States), 4, 2)` -> `AddressOf(EffectKind34_V2States), 4, 2)` | 6000 |
| R2 | `EffectKind34_V1Run` | `AddressOf(EffectKind34_V1States), 2, 2)` -> `AddressOf(EffectKind34_V3States), 2, 2)` | 6000 |
| R3 | `EffectKind34_V2Run` | `AddressOf(EffectKind34_V2States), 4, 2)` -> `AddressOf(EffectKind34_V0States), 4, 2)` | 6000 |
| R4 | `EffectKind34_V3Run` | `AddressOf(EffectKind34_V3States), 2, 2)` -> `AddressOf(EffectKind34_V4States), 2, 2)` | 6000 |
| R5 | `EffectKind34_V4Run` | `AddressOf(EffectKind34_V4States), 2, 2)` -> `AddressOf(EffectKind34_V1States), 2, 2)` | 6000 |
| B1 | `EffectKind34_V0Burst` | `i < 10` -> `i < 9` | 6000 |
| B2 | `EffectKind34_V0Burst` | `e[5] = 0x34;` -> `e[5] = 0x35;` | 6000 |
| B3 | `EffectKind34_V0Burst` | `e[6] = static_cast<unsigned char>(i);` -> `e[6] = static_cast<unsigned char>(i + 1);` | 6000 |
| B4 | `EffectKind34_V0Burst` | `Camera_Distance - 0x80` -> `Camera_Distance - 0x7F` | 4361 |
| B5 | `EffectKind34_V0Burst` | `MapView_Redraw = 3;` -> `MapView_Redraw = 2;` | 4361 |
| B6 | `EffectKind34_V0Burst` | `(0x10C);` -> `(0x10D);` | 6000 |
| B7 | `CopyPlace` | `SetUL(e + 0x3C, UL(s + 0x3C));` -> `SetUL(e + 0x3C, UL(s + 0x38));` | 6000 |
| E1 | `EffectKind34_V0BurstEnd` | `Camera_Distance = 0;` -> `Camera_Distance = 1;` | 5963 |
| G1 | `EffectKind34_V0DebrisStart` | `bank = 0x212;` -> `bank = 0x213;` | 1551 |
| G2 | `EffectKind34_V0DebrisStart` | `int i = 8; i >= 0` -> `int i = 7; i >= 0` | 166 |
| G3 | `EffectKind34_V0DebrisStart` | `(r & 0x7F)` -> `(r & 0x3F)` | 774 |
| G4 | `EffectKind34_V0DebrisStart` | `0xFFF80000u` -> `0xFFF90000u` | 1916 |
| G5 | `EffectKind34_V0DebrisStart` | `static_cast<std::int32_t>(UL(s + 0xC)) > 0x800` -> `static_cast<std::int32_t>(UL(s + 0xC)) > 0x7FF` | 9 |
| G6 | `EffectKind34_V0DebrisStart` | `s[0x2A] = 1;` -> `s[0x2A] = 2;` | 465 |
| G7 | `EffectKind34_V0DebrisStart` | `s[0xA] = 8;` -> `s[0xA] = 9;` | 1939 |
| G8 | `EffectKind34_V0DebrisStart` | `(r & 0x7FFF)` -> `(r & 0x7FFE)` | 1070 |
| G9 | `EffectKind34_V0DebrisStart` | `SetWord(s + 0x3C, 0);` -> `SetWord(s + 0x3C, 1);` | 1939 |
| Y1 | `EffectKind34_V0DebrisFly` | `SetUL(s + 0x3C, UL(s + 0x3C) + UL(s + 0x14));` -> `SetUL(s + 0x3C, UL(s + 0x3C) + UL(s + 0x10));` | 6000 |
| Y2 | `EffectKind34_V0DebrisFly` | `if ((Frame_Counter & 1) != 0)` -> `if ((Frame_Counter & 2) != 0)` | 264 |
| Y3 | `EffectKind34_V0DebrisFly` | `s[9] = static_cast<unsigned char>(s[9] - 1);` -> `s[9] = static_cast<unsigned char>(s[9] - 2);` | 5254 |
| A1 | `EffectKind34_V1Start` | `SetUL(s + 0x14, 0x40);` -> `SetUL(s + 0x14, 0x41);` | 5970 |
| A2 | `EffectKind34_V1Start` | `SH_CALL(Sprite_SetAnimation)(8);` -> `SH_CALL(Sprite_SetAnimation)(7);` | 6000 |
| A3 | `EffectKind34_V1Start` | `SetUL(s + 0x20, 0xFFFFFFF8u);` -> `SetUL(s + 0x20, 0xFFFFFFF7u);` | 6000 |
| A4 | `PieceBank` | `SH_CALL(Sprite_SetAnimationBank)(0x18);` -> `SH_CALL(Sprite_SetAnimationBank)(0x19);` | 6000 |
| A5 | `ClearTints` | `s[0x5E] = 0;` -> `s[0x5E] = 1;` | 6000 |
| A6 | `PieceBank` | `s[0x48] = 0;` -> `s[0x48] = 1;` | 6000 |
| V1 | `EffectKind34_V1Fall` | `if (height <= ground) {` -> `if (height < ground) {` | 1065 |
| V2 | `EffectKind34_V1Fall` | `+ 0x100) {` -> `+ 0xFF) {` | 269 |
| V3 | `EffectKind34_V1Fall` | `SetWord(s + 0x3E, Word(s + 0x3E) + Word(s + 0x14));` -> `SetWord(s + 0x3E, Word(s + 0x3E) + Word(s + 0x16));` | 5241 |
| V4 | `EffectKind34_V1Fall` | `if ((Frame_Counter & 1) != 0)` -> `if ((Frame_Counter & 1) == 0)` | 1831 |
| K1 | `EffectKind34_V2Start` | `s[0xA] = 5;` -> `s[0xA] = 6;` | 6000 |
| K2 | `EffectKind34_V2Spawn` | `if (s[9] != 4) return;` -> `if (s[9] != 3) return;` | 3082 |
| K3 | `EffectKind34_V2Spawn` | `e[1] = 2;` -> `e[1] = 3;` | 2924 |
| K4 | `EffectKind34_V2Spawn` | `e[2] = 2;` -> `e[2] = 3;` | 2924 |
| K5 | `EffectKind34_V2Spawn` | `s = S();  if (n != 0xFF) {` -> `if (n != 0xFF) {` | 109 |
| K6 | `EffectKind34_V2PieceStart` | `(r & 0xFFFF)` -> `(r & 0x7FFF)` | 508 |
| K7 | `EffectKind34_V2PieceStart` | `if (pick > 2) pick = 2;` -> `if (pick > 1) pick = 1;` | 2407 |
| K8 | `EffectKind34_V2PieceStart` | `pick + 0xB` -> `pick + 0xC` | 6000 |
| K9 | `EffectKind34_V2PieceFall` | `if (SW(S() + 0x3E) <= ground)` -> `if (SW(S() + 0x3E) < ground)` | 1057 |
| KA | `EffectKind34_V2PieceFall` | `SetUL(s + 0x14, UL(s + 0x14) + UL(s + 0x20));` -> `SetUL(s + 0x14, UL(s + 0x14) + UL(s + 0x1C));` | 6000 |
| X1 | `EffectKind34_V3Start` | `SH_CALL(Sprite_SetAnimation)(9);` -> `SH_CALL(Sprite_SetAnimation)(10);` | 6000 |
| X2 | `EffectKind34_V3Start` | `SetUL(s + 0x14, 0);` -> `SetUL(s + 0x14, 1);` | 5970 |
| X3 | `EffectKind34_V3Fall` | `>= ground)` -> `> ground)` | 1065 |
| X4 | `EffectKind34_V3Fall` | `Word(s + 0x3E) + Word(s + 0x14)` -> `Word(s + 0x3E) - Word(s + 0x14)` | 5484 |
| Z1 | `EffectKind34_V4Start` | `s[0x48] = 1;` -> `s[0x48] = 2;` | 1967 |
| Z2 | `EffectKind34_V4Start` | `0xFFFFE000u` -> `0xFFFFF000u` | 1967 |
| Z3 | `EffectKind34_V4Start` | `(0x46)` -> `(0x47)` | 6000 |
| Z4 | `EffectKind34_V4Start` | `s[0x29] = static_cast<unsigned char>(s[0x29] + 1);` -> `s[0x29] = static_cast<unsigned char>(s[0x29] + 2);` | 1967 |
| M1 | `EffectKind34_V4Move` | `(s[0] & 0x80)` -> `(s[0] & 0x40)` | 2999 |
| M2 | `EffectKind34_V4Move` | `Word(s + 0x3E) + Word(s + 0x14)` -> `Word(s + 0x3E) + Word(s + 0x10)` | 5956 |
| J0 | `EffectKind3A_Run` | `AddressOf(EffectKind3A_States), 3, 1)` -> `AddressOf(EffectKind34_V2States), 3, 1)` | 6000 |
| J1 | `EffectKind3A_Start` | `s[0xA] = 0xE;` -> `s[0xA] = 0xF;` | 6000 |
| J2 | `EffectKind3A_Start` | `0xC0u);` -> `0xC1u);` | 6000 |
| J3 | `EffectKind3A_Start` | ` << 4);` -> ` << 3);` | 4509 |
| J4 | `EffectKind3A_Start` | `UL(At(at::kLeader + 0x38))` -> `UL(At(at::kLeader + 0x34))` | 6000 |
| J5 | `EffectKind3A_Start` | `s[0xB] = 0;` -> `s[0xB] = 1;` | 6000 |
| Fy1 | `EffectKind3A_Fly` | `Word(At(at::kAreaTileBase)) * 2u` -> `Word(At(at::kAreaTileBase)) * 1u` | 2964 |
| Fy2 | `EffectKind3A_Fly` | `At(at::kLeader)[0x27]` -> `At(at::kLeader)[0x26]` | 2017 |
| Fy3 | `EffectKind3A_Fly` | `(0x4A);` -> `(0x4B);` | 2022 |
| Fy4 | `EffectKind3A_Fly` | `if (S()[0xA] == 0)` -> `if (S()[0xA] == 1)` | 486 |
| Fy5 | `EffectKind3A_Fly` | `(0x10E);` -> `(0x10F);` | 2022 |
| Wa1 | `EffectKind3A_Wait` | `if (S()[0xB] == 2)` -> `if (S()[0xB] == 1)` | 2086 |
| Wa2 | `EffectKind3A_Wait` | `S()[0xB] = 1;` -> `S()[0xB] = 2;` | 1560 |
| Wa3 | `EffectKind3A_Wait` | `if (Field_Request != 0) {` -> `if (Field_Request != 2) {` | 3036 |
| Wa4 | `EffectKind3A_Wait` | `\|\| S()[0xB] != 0` -> `\|\| S()[0xB] == 1` | 287 |
| Lp1 | `EffectKind3A_LeaderPose` | `s[0x25] = 0x1D;` -> `s[0x25] = 0x1E;` | 6000 |
| Lp2 | `EffectKind3A_LeaderPose` | `SetWord(s + 0x2C, Word(l + 0x2C));` -> `SetWord(s + 0x2C, Word(l + 0x2E));` | 6000 |
| Lp3 | `EffectKind3A_LeaderPose` | `s[0x28] = 2;` -> `s[0x28] = 3;` | 6000 |
| Lp4 | `EffectKind3A_LeaderPose` | `SetUL(s + 0x70, 0);` -> `SetUL(s + 0x70, 1);` | 6000 |
| Ht1 | `EffectKind3A_Hit` | `if (leader - ground > 0x80) return 0;` -> `if (leader - ground > 0x81) return 0;` | 291 |
| Ht2 | `EffectKind3A_Hit` | `if (ground - leader > 0x100) return 1;` -> `if (ground - leader > 0xFF) return 1;` | 322 |
| Ht3 | `EffectKind3A_Hit` | `== 0x11) return 1;` -> `== 0x12) return 1;` | 145 |
| Ht4 | `EffectKind3A_Hit` | `luck <= 0xC` -> `luck <= 0xB` | 69 |
| Ht5 | `EffectKind3A_Hit` | `luck == 0xF ? 0xA : 5` -> `luck == 0xF ? 0xA : 6` | 111 |
| Ht6 | `EffectKind3A_Hit` | `At(at::kLeader)[7] = 1;` -> `At(at::kLeader)[7] = 2;` | 138 |
| Ht7 | `EffectKind3A_Hit` | `static_cast<U>(at_ground) + 0x100u` -> `static_cast<U>(at_ground) + 0xFFu` | 132 |
| Ht8 | `EffectKind3A_Hit` | `e[1] = 1;` -> `e[1] = 2;` | 132 |
| Ht9 | `EffectKind3A_Hit` | `static_cast<signed char>(o));   return 1;` -> `static_cast<signed char>(o));   return 0;` | 3029 |
| HtA | `EffectKind3A_Hit` | `Long(s + 0x38), 0)` -> `Long(s + 0x38), 1)` | 6000 |
| HtB | `EffectKind3A_Hit` | `if (Word(s + 0x38) != 0 && !found) {` -> `if (Word(s + 0x38) != 0) {` | 226 |
| HtC | `EffectKind3A_Hit` | `} else if (!found) {   return 0;` -> `} else if (found) {   return 0;` | 1133 |
| HtD | `EffectKind3A_Hit` | `s[0xB] = 2;` -> `s[0xB] = 3;` | 137 |
| Q0 | `EffectKind41_Run` | `AddressOf(EffectKind41_States), 4, 1)` -> `AddressOf(EffectKind34_V0States), 4, 1)` | 6000 |
| Ks1 | `EffectKind41_Start` | `0xFFF6u : 0xFFECu` -> `0xFFF6u : 0xFFEDu` | 2983 |
| Ks2 | `EffectKind41_Start` | `S()[0x29] = 3;` -> `S()[0x29] = 4;` | 6000 |
| Ks3 | `EffectKind41_Start` | `SetUL(S() + 0x10, 0x20000);` -> `SetUL(S() + 0x10, 0x20001);` | 6000 |
| Ks4 | `EffectKind41_Start` | `SetWord(S() + 0x38, 0);` -> `SetWord(S() + 0x38, 1);` | 6000 |
| Kh1 | `EffectKind41_Hold` | `if (S()[9] == 0)` -> `if (S()[9] == 1)` | 1456 |
| Kh2 | `DrawNumber` | `Word(o + 0x2E)` -> `Word(o + 0x2C)` | 6000 |
| Kh3 | `DrawNumber` | `s[6], s[0x27]);` -> `s[6], s[0x26]);` | 5980 |
| Kh4 | `DrawNumber` | `s[0xB] * at::kMemberStride` -> `(s[0xB] + 1) * at::kMemberStride` | 6000 |
| Kb1 | `EffectKind41_Bounce` | `0x14000u);` -> `0x14001u);` | 6000 |
| Kb2 | `EffectKind41_Bounce` | `if (SW(s + 0x3A) >= 0) {` -> `if (SW(s + 0x3A) > 0) {` | 459 |
| Kb3 | `EffectKind41_Bounce` | `S()[9] = 6;` -> `S()[9] = 5;` | 2728 |
| Kf1 | `EffectKind41_Fade` | `if (s[9] == 0) {` -> `if (s[9] == 1) {` | 1454 |
| Kf2 | `EffectKind41_Fade` | `if ((Frame_Counter & 1) != 0)` -> `if ((Frame_Counter & 2) != 0)` | 2677 |
| Kf3 | `EffectKind41_Fade` | `0x14000u);` -> `0x15000u);` | 5294 |

## 7. Calls across groups

**Outbound, raw (another wave-two group's, in `field_c2_callees.h`):** FE2's
`0x5728D0` (4 sites in `EffectKind3A_Hit`), FE1's `0x5307C0` (1 site in
`EffectKind3A_Hit`) - the edges RT's `--edges` printed. **Raw, nobody's:**
`0x494060`, `0x494110`, `0x4941E0` (the camera and projection helpers,
catalog part 2) and `0x46D5F0` (the number draw, just past the band). The
table entry FC1's `0x46A310` is read in place from `EffectKind34_Variants[5]`.

**Inbound from outside the group** (for the rebinding pass):

| Caller | Owner | Calls |
|---|---|---|
| `0x46BB30` (`jmp [0x653FE4 + 4 * +1]`) | FC1 | `EffectKind30_Push`, `_Slide`, `_Shatter` (read in place: no rebinding) |
| `0x46BB50` (`E8` at `0x46BBD2`) | FC1 | `EffectKind30_ClaimCells` |
| `0x46B7C0` (`E8` at `0x46B7CE`) | FC1 | `EffectKind3A_LeaderPose` |
| `0x46D710` (`E8` at `0x46D74D`, `0x46D752`) | nobody (engine) | `EffectKind30_ShardsInit`, `_SparksInit` |
| `0x46D770` (`E8` at `0x46D770`, `E9` at `0x46D775`) | nobody (engine) | `EffectKind30_ShardsStep`, `_SparksDraw` |
| `Effect_RunObjects` through `Effect_KindHandlers` `0x655420` / `0x655438` / `0x655454` | ours | the three runs (read in place) |

## 8. The rebinding

`grep -rn -i` of all 44 addresses and the eleven tables' in `src/game`: the
only raw references were three comments - `scena_sx2.cpp` (`AreaMap_
SetHeight`'s callers `0x46BF80` / `0x46C100`) and `area_w3d_callees.h` (what
`0x46D710` / `0x46D770` call) - now naming `EffectKind30_ClaimCells` /
`_FreeCells`, `_ShardsInit` / `_SparksInit`, `_ShardsStep` / `_SparksDraw`
beside the unchanged addresses. No constant, call site or fuzz key of another
file names an FC2 address; nothing left raw. The addresses in other entries'
`symbols.toml` evidence (`AreaMap_SetHeight`'s) are left as they are.

## 9. The live route

`analysis/calltrace/reach_whelp` and `reach_dragon` (`bof3x.calltrace.tsv`,
all original, 2026-09-29 at `979a567`): **no start of `0x46BBF0..0x46D5F0` is
entered by either route**. The coordinator's frame-hash A/B covers none of
the 44; they are fuzz-only.

## 10. Latent defects (Capcom's, described, not fixed)

- **The eight dispatchers index unchecked** (`jmp [table + 4 * byte]`): a
  `+1` / `+2` past its table jumps through the next table's entry (the
  tables lie back to back) or, past `EffectKind34_V0States`, into the area
  bytes `0x65401C`. Every writer of those bytes in the band steps them inside;
  who else sets `+1` for a kind-0x34 record (the spawners outside the band) was
  not traced. Ours aborts.
- **`EffectKind3A_Hit` widens `Sprite_ObjectAt`'s answer signed** (`movsx`,
  `cmp al, 0x1E; jge`) where `EffectKind30_WayBlocked` widens it unsigned: an
  answer of `0x80..0xFE` would set a bit below `Sprite_Objects`. The callee
  answers 0..`0x21` or `0xFF`, so it is unreachable; ours aborts.
- **`EffectKind3A_Fly` reads the tile layer at the cell with no bound** (x, z
  s16 against no dimension): a thrown object leaving the map reads outside
  the layer - a non-zero word there keeps it flying until its 14 frames run
  out. A data read, reproduced.
- **Kind 0x41 draws member `+0xB` of `ObjTrio` unchecked** (three members): a
  byte past 2 reads another record's screen words. Reproduced.
- **The leader's facing indexes the 8-row step tables unchecked**
  (`EffectKind30_Push`, `EffectKind3A_Start`). Reproduced.
- **`EffectKind30_Push` copies `40 x` a signed count byte** from `*(+0x54)`
  into `0x8C5D80` with no bound (up to 5,080 bytes; the buffer is used with
  `0x1800` elsewhere, so it fits). `EffectKind30_ShardsInit` then reads 24 faces
  of it regardless of the count: a model of fewer faces leaves the rest from
  whatever the buffer held.
- **`EffectKind34_V0Burst` relies on `Effect_Release` clearing `+1`**: its ten
  records are given `+2` 2 but not `+1`, so they run variant 0 only because a
  released record's `+1` is 0 (`Effect_Release` clears bytes 0..4).
- **`EffectKind3A_Hit` sets the leader's `+7` even when no record was free**
  for the piece.
- **Not ours, met on the way:** `0x4941E0` divides by the camera-space depth
  (`idiv esi`) - a spark at depth 0 faults. Nobody's (catalog part 2).

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-09-29, a commented block): the 34
starts without an exact line - the four new ones, the 27 hidden starts and
the three shorter extents (`0x46BD10` 0x8F, `0x46C550` 0x1DB, `0x46D180`
0x274) whose host lines covered the next starts. The ten other lines were
already exact.
