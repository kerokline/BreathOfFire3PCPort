# The field objects' movement script

**Status:** IN PROGRESS (2026-09-22) - the whole interpreter is ours: the
step, the flow pass and all eight group handlers - and so is what runs it
and what follows it, the object update (section 1b) and the sprite's screen
update (1c), the attached object's update and the counter ops; twenty-one
functions, each fuzzed against Capcom's, **and all through the batch check
in game** (section 1d). The op-length table is found and matches
the PSX; every shipped script decodes, and D6's list is exact. The port's
own changes to the script are listed in section 1a; the table's one wrong
length is [`known-defects.md`](known-defects.md) D7.

Found following [`known-defects.md`](known-defects.md) D6 back to its source:
the byte a sprite object is attached by comes from a command of this script.
It is not the frame-animation script of [`sprite-draw-order.md`](sprite-draw-order.md)
§6 (`Sprite_Script*`, two bytes a step), and not the area event script.

## 1. The interpreter

| | PC | PSX (`GAME.EMI` §0) | pairing | |
|---|---|---|---|---|
| one step | `0x576B50` `MoveScript_Step` | `FUN_801a9d38` | `call-anchored` | **ours** |
| the flow pass | `0x576E00` `MoveScript_Flow` | `FUN_801a9f94` | `callers` | **ours** |
| the `0xF0` group | `0x577760` `MoveScript_GroupF` | `FUN_801aade8` | by position | **ours** |
| the `0x60` group | `0x578B00` `MoveScript_Group6` | `FUN_801aca24` | by position | **ours** |
| the `0x80` group | `0x5786C0` `MoveScript_Group8` | `FUN_801ac458` | by position | **ours** |
| the `0x90` group | `0x578A00` `MoveScript_Group9` | `FUN_801ac8b0` | by position | **ours** |
| the `0xC0` group | `0x578010` `MoveScript_GroupC` | `FUN_801abb00` | by position | **ours** |
| the `0xD0` group | `0x577BD0` `MoveScript_GroupD` | `FUN_801ab470` | by position | **ours** |
| the `0xE0` group | `0x577420` `MoveScript_GroupE` | `FUN_801aa944` | `callers` | **ours** |
| the `0xB0` flow group | `0x577280` `MoveScript_GroupB` | `FUN_801aa6e4` | `callers` | **ours** |
| the label search | `0x579450` `MoveScript_FindLabel` | `FUN_801ad8f0` | read on both | |
| attach command, `F8` | `0x5792A0` `MoveCmd_Attach` | `FUN_801ad768` | `call` | |

PSX side read from the sibling's Ghidra output
(`analysis/ghidra/GAME_EMI0_80196800_decomp/`); pairs from
`analysis/pairs_propagated.json` (`attract-remaining.md` §5, tiers there).
"By position" means the PC step's jump table calls it where the PSX
dispatcher calls the twin. This doc had `0x578790` for the `0x80` group,
from a `callers` pairing; the step calls `0x5786C0`. Everything the three
owned functions call is typed in `symbols.toml` (2026-09-22 section).

One call runs one op at `base + ctx[+0xA]` (`u16`, the position). The flow
pass runs first: it loops over **flow ops** - `01`..`0F`, `A0`..`BF` - until
the byte at the position is anything else, and returns a value (`Sprite_Current[8]`
at entry, replaced by some ops). Then the step dispatches that byte on its
**high nibble**:

| high nibble | what |
|---|---|
| `0x10`..`0x40` | move: direction `(b - 0x10) >> 3`, `b & 7` steps to `ctx[+7]`; `MoveCmd_Move`, or `MoveCmd_MoveKind2` for the kind-2 object; returns the direction |
| `0x50` | `ctx[+1] = b & 0xF` |
| `0x60`, `0x80`, `0x90`, `0xC0`, `0xD0`, `0xE0`, `0xF0` | the group handler, whose result is the step's |
| `0xA0` | stays: the flow pass stopped on an `A` op that returned negative |
| `0x70`, and `0x00` / `0xB0`, which never get here | nothing |

then the position advances by one, read again after the handler.

**The flow ops** (`MoveScript_Flow`, case for case the PSX's):

| op | bytes | what |
|---|---|---|
| `00` | - | not a case: the pass loops on it for ever |
| `01 h l` | 3 | jump by `h:l`; to label `l` when `h & 0xC0 == 0x40` |
| `02 d n` | 3 | loop back by `d` (signed), `n` times, counting in `ctx[+2]` |
| `03 n` | 2 | call the area's handler `n` (descriptor `+0x3C`) |
| `04`..`09 a b p q r s` | 7 | compare `a` (constant in `04 06 08`, `MoveScript_Variable` in `05 07 09`) with variable `b`: equal, less, any bit; then go to `p:q` or `r:s`, a label when bit `0x4000` is set |
| `0A l` | 2 | a label |
| `0B` | 1 | back to 0 |
| `0C x` | 2 | bit `0x40` of three objects, cleared or set |
| `0D` 4 bytes | 5 | place at a map position and take the ground's elevation |
| `0E` / `0F` 3 or 4 bytes | 4 or 5 | an animation, by number or through `MoveScript_AnimByState` |
| `A0`..`AF` | 2, 2, 1, 1 by `op & 3` | the counters at `0x903848`: set, compare (stop if unequal), increment, nothing (`MoveScript_CounterOps`; the first three ours, `MoveScript_CounterSet` / `Test` / `Step`, fuzzed alone under `BOF3X_SHADOW=move_counters`, four controls refused) |
| `B0`..`BF` | from the table | `MoveScript_GroupB` |

**The `0xF0` group** (`MoveScript_GroupF`), lengths with the step's `+1`:

| op | bytes | what |
|---|---|---|
| `F0`, `F1` | 1 | `ScriptFlags_Clear40` / `Set40` |
| `F2 d n` | 3 | move, as `0x10`..`0x40` with its operands |
| `F3 d n h l` | 5 | move and rise to height `h:l` over the move's frames |
| `F4 h l n` | 4 | rise to `h:l` over `n` frames |
| `F5` | 1 | clear bit 7 of `ctx[+0]` |
| `F6 x` | 2 | `x` to `MoveScript_Var7` (PSX `0x80146870`) |
| `F7` 6 bytes | 7 | `MoveCmd_OpF7` with two 8.8 coordinates |
| `F8` | 2, or 5 when the mode is `7` | **attach**, section 2 |
| `F9 a b` | 3; repeats while `MoveScript_WaitTest(a, b)` is non-zero | |
| `FA d n h l` | 5 | move, and set the height to `h:l` |
| `FB`, `FC` | 1 | a test at the sprite's position; sound `0x103` if it passes |
| `FD` | 1, or repeats | waits on `Sprite_ScriptTickOnce` (PSX `0x8014D9E0`) |
| `FE s x` | 3 | sound `0x200 + s` when the sprite's script reaches `x` |
| `FF` | 0 - repeats | the end: the script stays on it |

The takeover, [`src/game/move_script.cpp`](../src/game/move_script.cpp):
every quirk kept - the loop on `00`, 16-bit position arithmetic, unguarded
divisions in `F3` / `F4`, the kind-2 path's shift that wraps. Its start-up
fuzz (`BOF3X_SHADOW=move_script`) generates scripts whose flow always
reaches an op the step dispatches, with recording stand-ins for every callee
and for the two pointer tables: **20,000 rounds, 0 mismatches**, every flow op
about 3,300 times, every step group 627..846 times and each of `F0`..`FF`
545..666. Seventeen negative controls, each one planted bug, all refused -
two of them (`02`'s counter end, `A`'s skip) by comparison before the bad
loop ran away. One was first **not** refused: dropping `FE`'s test of the
sprite's `+0x58`, because a random word so rarely equals a byte; the peek
stand-in now also makes the two equal, and the control is refused. In game:
section 1d.

## 1a. The group handlers, and what the port changed

All seven read on both sides, case for case, 2026-09-22; their lengths all
agree with `MoveScript_OpLengths` except `C1` (D7). What each group is, in
one line - `symbols.toml` has the ops:

| group | what |
|---|---|
| `0x60` | an animation from the sprite's direction; returns the direction |
| `0x80` | flags; `84` to another object's position (a handle, as `F8`'s - no shipped script uses it); `85` the kind-2 object toward ObjTrio 0; `86` hand over to a `+0x18` script; streams (`89`, `8B`); an effect (`8C`) |
| `0x90` | effects: spawn one at the sprite (`90`..`95`, `98`..`9D`), wait for it (`9F`) |
| `0xB0` | sound and music (a flow op: it runs in the flow pass) |
| `0xC0` | tints (`C0`..`C2`), 8.8 moves of the sprite's fields (`C3` `C4` `C7`), turning one step toward a direction (`C8` `CE`), an animation bank (`CB`) |
| `0xD0` | the kind-2 object placed (`D0`), a sound on a script position (`D1`), the party slot's moves (`D4`..`D6`), waits on the field's state (`D9` `DA` `DF`), ground tests, `DE` the area's handler, `D8` end |
| `0xE0` | facing and turning (`E0`..`E3`), flags, `E6` ends the object, `E9` repeats on a test, `EA` a tint, `EF` end |

**What the PC port changed** - found by setting each PC handler against its
PSX twin; the rest is the same code:

- **`B0`..`BF`, the sound group, is rewritten.** Every case calls a PC-side
  function where the PSX calls another with more operands: `B0`..`B3` play
  sound `b1` in bank 0..3 (`Sound_PlayEffect`), `B4` and `BE` both start
  music `b1` (`Music_Play`; on the PSX two different functions), `B5` / `B9`
  / `BA` / `BB` volume ramps, `BC` / `BD` sound `b1:b2` with the rest of
  their operands unread.
- **`B8` does nothing on the PC**: it calls `0x4DF820`, a bare `ret`, where
  the PSX calls `0x8015DCF0(b1, b2, b3, b3)`. What that did is unread.
- **`8A` does nothing on the PC** but skip its operands; the PSX calls
  `0x80164A10(b1:b2, 1)` there - the neighbour of `89`'s stream call.
- The PC's effect records are `0x80` bytes (`Effect_Objects`), the PSX's
  `0x74`; the kind-2 object's x and z (`Field_Kind2X` / `Z`) sit in the
  other order in memory, used consistently.

The takeover, [`src/game/move_groups.cpp`](../src/game/move_groups.cpp),
keeps all of it. Its start-up fuzz (`BOF3X_SHADOW=move_groups`) drives each
handler directly: an op in or just past the group's range, random operands
and state - a context, a sprite, the field state, a party object, the tint
and party records, ObjTrio 0, `Sprite_Kind2`, eight globals - with each op's
boundaries seeded, and recording stand-ins for the 29 callees and the area's
two tables. **30,000 rounds, 0 mismatches, all 112 ops of the seven groups
generated.** Negative controls, one planted bug each: 21 that change
behaviour, all refused (from 35 to 803 mismatches). Four more were not
refused because they change nothing - `CE`'s `<= 0` as `< 0` (zero is the
equal case, which returns first), `E3`'s `& 0xDF` as `& 0xCF` (bit `0x10`
is already clear), and `D4`'s clamp as `>= 4` or `> 3` (a slot of 4 clamps
to 4). One control found a blind spot: a clamp to 3 instead of 4 went
unseen while the party pointer was drawn uniformly across the four
objects, which met slot 4 once in ~650 rounds and never a slot above it;
the fuzz now draws the slot, 0..7, and the control is refused. Not reached:
`D6` with a negative slot, which needs the pointer below the party objects.

## 1b. The object update around it

What runs the step for a field object, every frame, 2026-09-22 - ours in
[`src/game/field_objects.cpp`](../src/game/field_objects.cpp), each the PSX
twin branch for branch. The object's script context is object `+0x80`; on
the PSX it is `+0x74`, and every field of the object is `0xC` further on the
PC.

| PC | PSX | what |
|---|---|---|
| `0x517BF0` `Field_ObjectUpdate` | `FUN_801a238c` | unless a timed move is running (`Sprite_Current[9]`): one step of the area's `+0x10` script `[+0x83]`, or, with steps left in `+0x87`, the next step of a move unless `Field_ObjectBlocked`; then the two below |
| `0x518D10` `Field_ObjectSettle` | `FUN_801a3aec` | the direction's "moving" bit; consumes op `E4`'s flag (scratch byte 3); a party member's step for a type-`0x0A` sprite |
| `0x518980` `Field_ObjectMotion` | `FUN_801a3544` | a timed move: velocity into position and height each frame; on arrival, grounded, and a wait of 30 x `Sprite_Current[2]` frames in pose 5 |
| `0x5197F0` `Party_ApplyRecord` | `FUN_801a4d24` | the velocity op `D4` left in the party slot's record, while its counts run |
| `0x5192A0` `Field_ObjectFollow` | `ov_entry_801a4418` | the update of an attached object: it stands where the object its handle names stands, plus `MoveCmd_AttachOffset`, then `Field_ObjectUpdate` - the other reader of `F8`'s handle (D6) |
| `0x578EB0` `MoveCmd_AttachOffset` | `FUN_801ad1dc` | the offset: vector `+0x1C` of the area descriptor's `+0x0C` table, turned by the sprite's angles through the GTE (all of whose functions it calls are ours) |

`Field_MoveSpeeds` `0x6697F0` is `0, 1, 2, 4, 8, 16`: the update divides 16
by the entry at `+0x84`. An object of speed 0 with steps left would divide by
zero, on both platforms - kept, and unreachable unless a script gives a
speed-0 object a move.

The PC keeps the objects a handle names in two arrays, 30 of `0xA4` bytes
(`Sprite_Objects`) and the extra ones from number `0x1E` (`Sprite_ObjectsExtra`),
where the PSX has one array of `0x98`-byte objects.

The fuzz (`BOF3X_SHADOW=field_objects`) runs each of the four alone, with
recording stand-ins for the step, the pose, the blocked test, the move, the
ground's elevation and the party calls - the update's calls of the other two
included: **20,000 rounds, 0 mismatches** (the attach pair joined the same
fuzz, with the GTE's calls recorded by the matrix's contents - a local on
both sides - and nine more controls, all refused). Eighteen negative controls, all
refused (12 to 1,768 mismatches). One was first not refused, and it showed
what the update does: when a move is blocked on its first step, the update
puts back object `+0x8A` and `+0x82` - the context's script position and
loop counter - so the step that began the move is undone and runs again next
frame. The step's stand-in had left the position alone, so there was nothing
to put back; it now moves it, as every real step does.

## 1c. From the object to the draw: the screen update

`0x588F20` `Sprite_UpdateScreen` (PSX `FUN_8014D184`, the sibling's
`Actor_UpdateScreenPos`), 67,101 calls a cycle, turns `Sprite_Current` into
what the draw-order pass sorts ([`sprite-draw-order.md`](sprite-draw-order.md)):
the draw key `+0x32`, from the map cell less `MapView_Origin` plus a
per-class adjustment (`Sprite_KeyAdjust`); a key outside the map's 0x37
layers hides the sprite (flag bit 7). Then the attached key
(`Sprite_InheritDrawKey`), the projection through `Gte_RotTransPers` - the
depth to `+0x60`, the float screen point to `+0x74` / `+0x78` and its
truncation to `+0x2E` / `+0x30` - and a place on `Sprite_DrawList` (40). A
sprite with `+0x24` bit 7 goes to `Sprite_OverlayList` (30) instead, through
`0x5890E0` `Sprite_QueueOverlay`. Ours in
[`src/game/sprite_screen.cpp`](../src/game/sprite_screen.cpp), 2026-09-22.

It is the PSX's term for term; the PC's GTE works in float, so where the PSX
stores the screen point as two shorts the PC keeps the floats as well and
truncates them through the C runtime's `_ftol`. Ours does both itself, as the
x87 does them: a signalling NaN comes back quiet from the float move, and
`_ftol`'s answer for a NaN or a value past 2^63 - the integer indefinite -
is 0 in the word kept. **The third dropped PSX call:** for a sprite with
flag bit 8 both lists call `Port_DroppedCall`, the bare `ret` op `B8` also
calls, where the PSX calls `0x80161E44`.

The fuzz (`BOF3X_SHADOW=sprite_screen`) keeps the copy's real call to
`_ftol` and gives the projection's stand-in screen floats that include
signalling and quiet NaNs, infinities, halves and values past 2^63: 20,000
rounds, 0 mismatches. Thirteen negative controls that change behaviour, all
refused (72 to 13,106 mismatches) - dropping the NaN's quieting, `_ftol`'s
indefinite answer or its truncation among them; one more, the height term's
`^ 0xF01F` as `^ 0xF000`, changes only bits the `>> 5` discards.

## 1d. The batch check in game, 2026-09-22

All twenty-one of sections 1-1c at once (`analysis/validate_ab20.sh`, local;
the log is `analysis/attract/ab20_batch.log`), with the keyboard left alone:

| check | result |
|---|---|
| the five start-up fuzzes, on the build under test | 0 mismatches each |
| capture A/B, the 21 original against ours: save 5's field | **4 of 4** identical |
| the new game's opening, its scripted scenes and battle | **9 of 9** |
| the attract cycle, frozen shots | **55 of 55**, and 55 of 55 against `ab19_attract_ours` |
| the oracle, all ours | identical over 7,478 frames |
| memory dump, all ours, against `clutref_a` | arena, VRAM and palettes identical |
| frame hash, all original twice | identical on all 10,062 frames (the noise floor) |
| frame hash, original against ours | **identical on all 10,062 frames** |

`ab20_orig` is the frame hash's reference from here. Against `ab19_orig` it
differs on 7,421 frames, and all of it is the functions taken over: the
eleven of the 21 that the traced list holds are gone from the counts, every
other function's total is the same, no frame has a call more, and the
310,287 fewer calls are those functions' own.

Beside the all-ours attract run, a read-only watch of the run-time script
block `0x6758E0` (section 3) and the area number: the cycle visited area 4
three times, and **the block stayed zero throughout**. Those array entries are
placeholders, not scripts built later - a zero script could not run anyway
(the flow pass loops on `00`), and the update skips the step for an object of
speed 0. Other areas that use the block (9, 16, 17) are not in the attract
cycle.

## 2. `F8`: attach

`F8 m h s`: store `m` in the object's byte `+1` - or, on the
`0x8015CB38() == 1` path, `m == 7` in byte `+2`; if `m` is `7`, also store `h`
as a dword at `+0x18` - the **handle** - and `s` at `+0x1C`, and advance past
them. The PC's
`0x5792A0` is the same (`and ecx, 0xFF; mov [obj + 0x18], ecx` at `0x5792EE`
and `0x579356`). Nothing else writes object `+0x18` as a byte: a scan of the
exe for stores to `[reg + 0x18]` finds primitive colour triples, and by
displacement from `Sprite_Objects` only two dword stores (`0x41E99E`,
`0x5380E8`, the second into object 0 only).

What reads the handle: `Sprite_ObjectByHandle` `0x57C0A0` turns it into an
object number, for `0x5192A0` (PSX `ov_entry_801A4418`: the object is put at
the other's position, plus an offset from `s` through the descriptor's
`+0x0C` vectors, `FUN_801ad1dc`) and `Sprite_InheritDrawKey` `0x589770` (it
takes the other's draw key). Bit 7 of the handle is D6.

## 3. Where the scripts are

**The area descriptor table** (`0x667590` on the PC, `0x801802EC` on the
PSX; `attract-remaining.md` §5), indexed by `Game_AreaNumber` `0x904EFC`. The
interpreter's three callers, read on both sides:

| PC call site | PSX caller | script base |
|---|---|---|
| `0x517C7A`, in `0x517BF0` (the object update) | `FUN_801a238c` | descriptor `+0x10` `[object +0x77]` (PC `+0x83`) |
| `0x5730E6`, in `0x573090` | `FUN_801c686c` | descriptor `+0x1C` `[byte 0x7E09C3]` |
| `0x5254CF`, in `0x5253E0` | `FUN_801b7bc0` | a pointer at struct `+0x130` (PSX `+0x124`), which op `86` fills from descriptor `+0x18` |

On the PSX the scripts are in each area's section at `0x801F2C00`, which the
PC's `DAT`s drop ([`DAT_CONTAINER.md`](DAT_CONTAINER.md) §2): **on the PC they
are in `BOF3.exe`'s `.data`**, laid out as the area section was. Area 100, in
address order: its `+0x10` scripts, then the `+0x10` array at `0x619F30`,
`+0x14`, `+0x18`, the `+0x1C` scripts and array at `0x61A3D4`, `+0x04`'s
pointer array at `0x61A420`, the `+0x08` block at `0x61A42C` (one entry's
data - what `FUN_801a78c8` gives an object it spawns), `+0x08` itself at
`0x61AAC0`, `+0x3C`, `+0x34`, and the descriptor at `0x61AAF0`.

**Scripts built at run time.** Ten array entries in four areas - area 4's
`+0x10` entries 0, 5, 6, 11, 17 and 19, area 9's 10 and 11, and entry 0 of
areas 16 and 17 - point into
`0x6758E0..0x675960`, which is zero in the file. No code immediate points
there. **Watched live through three visits to area 4 (section 1d), it stayed
zero**: placeholders, not scripts written at run time - areas 9, 16 and 17
not yet seen.

## 4. The op-length table and the decoder

**`MoveScript_OpLengths` `0x6639FC`**, 256 bytes: `MoveScript_FindLabel`
steps over every op but three by its entry (`F8 07` 5, other `F8` 2, `0E` /
`0F` 4 or 5 by bit 2 of their fourth byte). It is **byte for byte the PSX
`GAME.EMI` table at `0x801C97F4`**, and the 36 bytes before it
(`MoveScript_AnimByState`) are the PSX's at `0x801C97D0` - both came across
unchanged. All 107 lengths read from the code on 2026-09-22 agree with it:
every flow op, `0x10`..`0x5F`, `F0`..`FF`. For the groups not yet read it is
the only source: `C7` is the longest op, 11 bytes. **36 entries are 0** -
`00`, `67`..`7F` and nine more: no such op, and a label search that met one
would never end.

`python tools/movement_scan.py --decode` follows every script's control flow
from its start, with the table read out of the exe (not copied) and labels
resolved by the game's own walk. A path ends at `FF`, `0B`, `01`, `04`..`09`,
or at `EF`, `D8`, `E6` or `86` - read in the PSX handlers: `EF` and `D8` set
bit 3 of the script context's flags, `E6` clears the sprite's flag byte, `86`
hands the sprite a `+0x18` script and stays on itself - and scripts are
padded to four bytes with `00` after exactly these. 2026-09-22:

| | |
|---|---|
| script starts | 3,633 in 171 areas, from `+0x10`, `+0x18` and `+0x1C` |
| ops reached | 59,597 |
| paths ended at | `EF` 1,851, `FF` 1,106, `04`..`09` 359, `E6` 344, `01` 196, `D8` 112, `0B` 88, `86` 77, a length-0 byte 31 |
| attachments (`F8 07`) reached | **230**: handle `00` x137, `01` x58, `02` x28, `03` x7 - **none with bit 7** |
| labels the game's walk would never find | 11 |
| label searches a `C1` puts out of step (D7) | 0 |
| ops reached past the next script's start | 36, in 7 scripts |

Paths step `C1` by 4, as it executes; labels are searched as the game
searches them, by the table's 5, and each search is also run at 4 to find
any the difference changes (D7). Stepping paths by 5 left 90 of them in a
length-0 byte and 137 ops past the next start.

The 230 are exactly the raw scan's `F8 07` triples with those handles; the
raw scan's other five are D6's bit-7 byte, which no path reaches, and four
pointer words. Two findings on the way: code after an `FF` is reached by
jumps in some scripts (area 158's `+0x18[2]` jumps over its `FF`), so a
decoder must follow control flow rather than stop at `FF`; and a loop
`02 01 n` lands on its own count byte, which then reads as an `01` - the
interpreter works byte by byte, so this is legitimate (three reached).

## 5. Open

- **The decoder's remaining noise**: 31 paths that fall into a length-0
  byte (10 after `F3`) and 11 missing labels (areas 66, 135, 158..160,
  174) - an op that ends a path and is not in `PATH_ENDS`, or bytes that
  are not script; the game would hang on one only if the branch is taken.
  With every group read, the ops that jump or end can now be listed from
  the handlers rather than guessed.
- **What `B8` and `8A` did on the PSX**, before the port dropped them.
- **The run-time scripts at `0x6758E0`**: their writer, then read one live.
- The PC's store to struct `+0x130` (the twin of opcode `0x86`), and what the
  struct is.
- For the sibling, not acted on here: its `docs/loader_records/AREA.md`
  lists `FUN_801ad1dc` as a user of descriptor `+0x04`; the function reads
  `+0x0C` (its 6-byte vectors, `index * 6`).
- The object update `0x517BF0` (69,494 calls), which runs the step, is the
  hottest remaining function of the field-objects group
  ([`attract-remaining.md`](attract-remaining.md) §4.5); `MoveCmd_Move`
  `0x578C10` and the handlers' other callees are the rest of the cluster.
