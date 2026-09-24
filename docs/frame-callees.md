# The field frame loop's unread callees

**Status:** IN PROGRESS (2026-09-22) - eight functions ours: the effect
objects' runner, the camera turn (effect kind `0x13`) and its three states,
the slot records, the tint frame and the party members' screen updates,
each fuzzed against a byte-copy of Capcom's at start-up
(`BOF3X_SHADOW=frame_callees`, 24,000 rounds, 0 mismatches) with
49 of 51 planted bugs refused, the other two changes that change nothing. **Through the live batch check `ab22` + `ab22b`** (2026-09-22, the second parallel round - [`HANDOFF.md`](HANDOFF.md)).

[`field-frame.md`](field-frame.md) section 3 left these as the field frame's
unread callees: `Field_Frame` / `Field_FrameScripted` (`0x517200` /
`0x517240`) call `Party_UpdateScreens`, `Effect_RunObjects`,
`Field_RunSlots` and `MoveScript_TintFrame` every field frame. With them,
everything those two entries call is read, except `Kind2_Run`, the map, the
object handlers, `Field_LeaderFrame` / `Field_MemberFrame` and the callees
named in section 4 - which stay Capcom's. Sprite object offsets are those of
[`sprite-draw-order.md`](sprite-draw-order.md) sections 5 and 6; the tint
records are [`movement-script.md`](movement-script.md)'s ops C0..C2.

## 1. The functions

| PC | name | bytes | PSX twin (`GAME.EMI`) | how paired | hidden_b calls |
|---|---|--:|---|---|--:|
| `0x494030` | `Effect_RunObjects` | 47 | `FUN_8019B0EC` | 9th call of `FUN_8019A1B8`; read on both | 12,165 |
| `0x469E30` | `Effect_CameraTurn` | 26 | `FUN_8019BC3C` | `jalr` through `0x801C90E4`, then `0x8014932F = 2`; read on both | 1,662 |
| `0x469E50` | `CameraTurn_Start` | 153 | `FUN_8019B90C` | the same stores and divisions; read on both | 15 |
| `0x469EF0` | `CameraTurn_Step` | 119 | `FUN_8019BA4C` | read on both | 1,635 |
| `0x469F70` | `CameraTurn_End` | 54 | `FUN_8019BB04` | calls `FUN_80197070` where the PC tail-jumps to `Effect_Release`; read on both | 12 |
| `0x455250` | `Field_RunSlots` | 50 | `FUN_80197C8C` | 10th call of `FUN_8019A1B8`; read on both | 12,165 |
| `0x454AD0` | `MoveScript_TintFrame` | 491 | `FUN_8019725C` | 11th call; read on both | 12,165 |
| `0x531B60` | `Party_UpdateScreens` | 75 | `FUN_801BEAB0` | 7th call; read on both | 12,165 |

Bytes are to the last instruction, padding not counted (capstone,
2026-09-22): `0x494030..0x49405E`, `0x469E30..0x469E49`,
`0x469E50..0x469EE8`, `0x469EF0..0x469F66`, `0x469F70..0x469FA5` (the tail
`jmp` is its last instruction), `0x455250..0x455281`, `0x454AD0..0x454CBA`,
`0x531B60..0x531BAA`. None has a jump table. The catalogue's 128 for
`0x469EF0` and 64 for `0x469F70` include the `nop` padding after them. The
PSX bodies are the sibling's Ghidra output
(`analysis/ghidra/GAME_EMI0_80196800_decomp/`), read line by line against
the PC's; each is term for term the same except where noted below. Call
counts are `analysis/calltrace/hidden_b` (`caller 0` rows), 16,128 frames of
the attract cycle.

`0x469E30` and `0x469F70` are reached only through tables (`pe_hidden`
"indirect call"): `Effect_KindHandlers` entry `0x13` and `CameraTurn_States`
entry 2. A detour at the entry catches a call through a table like any
other.

## 2. What each does

**`Effect_RunObjects`**: for each of the 20 `Effect_Objects` (`0x7E11E0`,
`0x80` bytes each; the PSX's are `0x74`) whose byte `+0` is non-zero - any
bit, re-read each turn, so an object a handler spawns further along runs the
same frame - `Sprite_Current` is the object and
`Effect_KindHandlers[+5]()` runs (`0x655350`, the PSX `PTR_FUN_801C8C68`).
The kind is unbounded.

**What the objects at `0x7E11E0` are** (the question this group was asked).
A pool of 20 short-lived field objects, each run every field frame by a kind
handler, sharing `Sprite_Current` (the PSX's scratchpad `0x1F800044`) with
the sprites: `+0` in use, `+1` the kind's own state, `+5` the kind, `+9` a
frame count, `+0x64` / `+0x68` / `+0x6C` three dwords of parameters.
`0x589810` hands out the first free one (index, or `0xFF` when all 20 are
taken), `Effect_Release` `0x589840` clears bytes 0..4 of the current one and
`0x589870` the same by index. A scan of `.text` finds 266 immediate stores
to byte `+5` of a record here, **98 different kinds**, the highest `0xB0`,
plus 9 stores of a register; `Effect_KindHandlers` has 187 consecutive
`.text` pointers before the first that is not one, so the table's real length
lies between `0xB1` and 187 (`symbols.toml` gives the upper bound). The name
`Effect_Objects` (from op 9F, [`movement-script.md`](movement-script.md))
stands, `hypothesis`: "effect" fits the camera turn only loosely - these are
the field's timed helpers, of which one kind is a camera move. What the other
97 kinds do is unread.

**The attract cycle runs only kind `0x13`, the camera turn** - `0x469E30` is
the only `Effect_KindHandlers` entry in `hidden_b` (1,662 calls).
**`Effect_CameraTurn`** runs its state `+1` through `CameraTurn_States`
(`0x653F18`, the PSX `0x801C90E4`), unbounded - entries 3..9 are further
`.text` addresses (`0x46A040` first), another kind's states, whose PSX twin
`0x8019BC8C` calls `CameraTurn_Start`'s - and then sets `MapView_Redraw` to 2,
**overwriting** whatever the state stored there.

- **State 0, `CameraTurn_Start`**: `CameraTurn_Angles` (`0x905B90`, three
  16.16 dwords) = the three s16 `Camera_Angles` (`0x929EC8`) `<< 16`;
  `CameraTurn_Steps` (`0x905B70`) n = `((dword +0x64 + 4n - angle n) << 16)`
  in 32 bits, divided signed (`cdq` / `idiv`, truncating) by byte `+9`;
  `MapView_Redraw` = `+9` (dead: `Effect_CameraTurn` stores 2 over it on
  return); `Field_StatusBits` (`0x8034E1`) bit 5 set; `+1` = 1. A `+9` of 0
  divides by zero on both platforms (the PSX `trap(0x1C00)`).
- **State 1, `CameraTurn_Step`**: with `+9` 0, `+1` = 2 and nothing else.
  Otherwise each 16.16 angle plus its step, stored back, its high half to
  `Camera_Angles`, and `+9` one less. So an `n`-frame turn runs `n` steps and
  ends on the frame after.
- **State 2, `CameraTurn_End`**: `Camera_Angles` = the low words of the three
  dwords exactly (the steps' truncation is not left behind), bit 5 of
  `Field_StatusBits` cleared, and `Effect_Release` (a tail `jmp`; the PSX
  calls `FUN_80197070`).

So bit 5 of `0x8034E1` is "the camera is turning", and the dwords are the
target angles. The spawners say what turns: 106 sites store kind `0x13`, for
example `0x406B56` / `0x406BE3` (angle 0 to -770 or -682, the others kept,
over 16 frames) and group A's mode handler `0x56B450` at `0x56B4C5` (angle 0
less `0xAA`, over `0x40` frames). In `hidden_b` 15 turns start, 1,635 step
frames run and 12 end; the other three are cut off - the pool cleared under
them, or the cycle's end (unmeasured).

**`Field_RunSlots`**: each of the 8 `Field_Slots` records (`0x9035C0`,
`0x10` bytes; the PSX `0x80145D90`) whose byte `+0` has bit 0, re-read each
turn, runs `Field_RunSlot(index)` (`0x455300`, PSX `FUN_80197D84`). The
original pushes the index as a whole stack dword of which it stored only the
low byte - the other three are the caller's `ecx`, pushed at entry - and
`Field_RunSlot` masks it (`and esi, 0xFF` at `0x455305`), so the clean byte
ours passes is the same. **`Field_RunSlot` is never called in `hidden_b`**:
no slot is in use during the attract cycle. What a slot is, is unread
(`Field_RunSlot` compares byte `+0x4B` of the pointer at `+0xC` with byte
`+2`).

**`MoveScript_TintFrame`**: for each of the 32 `MoveScript_TintRecords`
(`0x7E0700`, 12 bytes; the walk ends at `0x7E0880`) with bit 0 of `+0`:

| byte | use |
|---|---|
| `+0` | bit 0 in use, bit 1 force bit 15, bit 6 hand the CLUT to the sprite (once), bit 7 with it set the sprite's `+0x24` bit 2 |
| `+1` | source CLUT number |
| `+2` `+3` `+4` | red, green, blue tint, added in 8 bits |
| `+5` | CLUT depth, indexing `Clut_Sixteens` / `Clut_PerRow` / `Clut_Stride` |
| `+6` | target CLUT number |
| `+8` | the sprite |

The three byte tables at `0x6528AC` (the PSX `0x801C8AB8`..`AC8`) are 1, 16,
2, 4, 8 sixteens of colours; 16, 1, 8, 4, 2 CLUTs to a 256-colour row; 16, 0,
32, 64, 128 colours of stride - depths 0..4 are CLUTs of 16, 256, 32, 64 and
128 colours, and **5..7 are all zero**, so a depth of 5..7 divides by zero
(both platforms; the depth is unbounded, so 8 and up read the next table).
A CLUT number `n` is the word index `(n / per_row) << 8` plus
`(n % per_row) * stride` kept to 8 bits, into `Gfx_ClutStrip` (`0x80F580`,
the working copy the game uploads, [`sprite-draw-order.md`](sprite-draw-order.md)
section 5). Then `sixteens * 16` words are copied from the source to the target
CLUT in order, each component (5 bits) plus its tint **in 8 bits**, clamped: a
result negative as a signed byte is 0, one above 31 is 31 - so a tint of
`0x61` or more on a bright component wraps negative and gives 0, not 31. Bit
15 is 1 with `+0` bit 1, else the source's. **The target's first word is
always 0** (the transparent colour), whatever the source's. Then
`Gfx_ClutStripDirty` is 1, for every active record whether or not it copied.
With bit 6 of `+0`: bit 6 cleared, the sprite's CLUT number `+0x27` becomes
`+6`, and with bit 7 (re-read) its `+0x24` gets bit 2 - the bit
`Sprite_ClutWord` reads as "rows from `0x1F0`".

So a tint record is a live recolour: every frame it rebuilds CLUT `+6` from
CLUT `+1` with an offset per channel, which ops C1 / C2 step and op C0 /
`Sprite_SetTint` start. The fades `Field_ObjectFadeOut` / `FadeIn`
([`object-kinds.md`](object-kinds.md)) step the three tints to 0 or 31 and
release the record.

**`Party_UpdateScreens`**: `Field_MemberCount` read once; from member
`count - 1` down to 0 (`ObjTrio`, `0x14C` bytes apart), each whose `+0` lacks
bit 6 becomes `Sprite_Current` and is updated - by `Sprite_UpdateScreenSlot`
(`0x588F00`: `+0x29` = `Draw_OtSlot`, then `Sprite_UpdateScreen`) when
`Field_Request`, re-read each time, is 3, else by `Sprite_UpdateScreen`. The
PSX's records are `0x140` apart; otherwise the same. **`0x588F00` is never
called in `hidden_b`**: the request is never 3 there.

## 3. Quirks kept

Each is in the code's comment:

- `Effect_RunObjects` tests the whole byte `+0`, re-read each turn
  (controls 1, 5); `Field_RunSlots` bit 0 of it, re-read (24, 27).
- `Effect_CameraTurn` stores `MapView_Redraw` after the state has run (6),
  so `CameraTurn_Start`'s store of the frame count there never survives.
- The steps are 32-bit `(target - angle) << 16`, signed-divided (8, 9); the
  frame count unsigned (10). The angles' sign extension is unobservable
  (control 14, below).
- `MoveScript_TintFrame`: the 8-bit add and its clamp (28, 29, 45); the first
  word 0 (30); the copy forward through overlapping CLUTs (39); the column
  kept to 8 bits (37); the tail's re-read of `+0` after the `+0x27` store
  (38); the dirty flag for every active record (33).
- `Party_UpdateScreens` reads the count once and the request every time (47,
  51); the count is unsigned, so 0x80 and 0xFF walk on past `ObjTrio` (48).

**Division by zero is kept**: a `CameraTurn_Start` with `+9` 0 and a tint
depth of 5..7 raise the original's `#DE` at the same point of the same
computation (a `cdq` / `idiv` in inline assembly, as `kind2_object.cpp`'s),
not a C++ `/`, which would be undefined there.

**Not reproduced, because unobservable** (a control says so):

- `CameraTurn_Start` reads the angles with `movsx`; `(target - angle) << 16`
  and `angle << 16` keep only the low 16 bits of the angle, so a zero-extended
  read gives the same steps (control 14, not refused, and no input can tell).
  Ours sign-extends anyway, as the original.
- `MoveScript_TintFrame` captures `+0`'s bit 1, `+3` and `+4` before the copy
  and reads `+2` within it; the copy writes only `Gfx_ClutStrip`, which cannot
  reach the records, so the order is invisible (control 40, not refused).
  Ours keeps the order.

## 4. The callees, and what is not ours

| PC | name | read | who |
|---|---|---|---|
| `0x455300` | `Field_RunSlot` | its head: the argument masked to a byte | Capcom's; never reached |
| `0x588F00` | `Sprite_UpdateScreenSlot` | 4 instructions: `+0x29` = `Draw_OtSlot`, `jmp Sprite_UpdateScreen` | Capcom's; never reached |
| `0x588F20` | `Sprite_UpdateScreen` | [`sprite-draw-order.md`](sprite-draw-order.md) | ours (`sprite_screen.cpp`) |
| `0x589840` | `Effect_Release` | whole: bytes 0..4 of `Sprite_Current` to 0, the pointer re-read before each store | Capcom's (as asked) |
| `Effect_KindHandlers` | 186 other kinds | not read | Capcom's |
| `CameraTurn_States` 3..9 | another kind's states | not read | Capcom's |

## 5. The fuzz, and its controls

`BOF3X_SHADOW=frame_callees`: eight byte-copies (`CloneOriginal`), every call
out re-aimed at a recorder - `Field_RunSlot`, `Sprite_UpdateScreenSlot`,
`Sprite_UpdateScreen`, and `Effect_Release` (the tail `jmp` at `+0x31`). The
two tables' first entries - 32 of `Effect_KindHandlers`, 10 of
`CameraTurn_States` - are recorders for the test's duration, for both sides
(the `DrawLayer_Open` pattern). The release recorder clears bytes 0..4 as the
real one does; every recorder may disturb what its caller reads again - the
effect objects' `+0` and kind, the slots' and members' `+0`, the request
byte, the member count, `Sprite_Current`, `MapView_Redraw`.

One round: one of the eight; random bytes in the 20 effect objects, the 8
slots, five members' span, the 32 tint records (and 16 bytes below them),
four fuzz-owned sprites, the camera words and the flags, then seeded: `+0`
as 0, bit 0 alone, other bits without bit 0; kinds below 32, `0x13` a third
of the time; states 0..2 mostly, 3..9 sometimes; frame counts 0, 1, 2, 0x10,
0x7F, 0x80, 0xFF (never 0 for the start); angles at the s16 edges, the
16.16 words at the 32-bit wrap; targets at the extremes or within 32 of the
angle (truncation toward zero either way); member counts 0..5, 0x80, 0xFF;
the request 3 half the time. The tint records: depths with a non-zero row
count (mostly 0..4), source and target CLUTs equal, adjacent or apart, tints
at the clamp's edges (0, 1, 0x1F, 0x20, 0x60, 0x61, 0x7F, 0x80, 0x81, 0xC0,
0xE0, 0xE1, 0xFF), the sprite pointer into the fuzz's sprites or into the
records themselves; and for the tint frame's rounds every word of
`Gfx_ClutStrip` it can reach (`0x10FF0` words, the strip and `0x1DFE0` bytes of
the arena after it) random. Theirs, then ours from the same state; all of it,
the strip and the recorders' log compared.

```
shadow      frame_callees self-test: 24000 rounds (3000 per function), 169122 calls to the stand-ins, 83852 tint records run, 13424100 strip words changed, 222 turns ended, 0 MISMATCHES; ...
```

A pointer into the records aims its `+0x27` at the same or an earlier
record, never at this record's own pointer (`+8..+11`) nor a later record's
depth or pointer: those would fault the original too - the first draft of the
fuzz did exactly that, an access violation at start-up, the process ending
with `0xC0000005` rather than hanging.

**Negative controls** - a bug planted in ours, rebuilt, the self-test run,
the bug removed:

| # | planted | mismatches (of 3,000) |
|---|---|--:|
| 1 / 2 / 3 | run: in use by bit 0 / kind xor 1 / `Sprite_Current` not set | 3,000 / 3,000 / 3,000 |
| 4 / 5 | run: 19 objects / `+0` read once up front | 2,562 / 1,926 |
| 6 / 7 | turn: redraw stored before the state / state xor 1 | 315 / 3,000 |
| 8 / 9 / 11 | start: `<< 15` / unsigned division / redraw not set | 3,000 / 2,364 / 2,987 |
| 10 | start: frame count sign-extended | 12 logged, then the process ended (`0xC0000005`) |
| 12 / 13 / 15 | start: status bit 4 / z step toward `+0x68` / state 2 | 2,252 / 2,894 / 3,000 |
| **14** | **start: angles zero-extended** | **0 - not refused, expected** |
| 16 / 17 / 18 | step: ends at 1 frame / angle from the low half / not counted down | 231 / 2,651 / 2,778 |
| 19 / 20 | step: state 2 and still steps / 16.16 not stored back | 222 / 2,544 |
| 21 / 22 / 23 | end: bit 5 left set / not released / z from `+0x68` | 1,463 / 3,000 / 2,884 |
| 24 / 25 / 26 / 27 | slots: bit 1 / index + 1 / 7 slots / `+0` read once up front | 2,991 / 2,966 / 1,284 / 402 |
| 28 / 29 / 45 | tint: clamp without the 8-bit wrap / upper clamp 0x1E / below zero to 1 | 3,000 / 3,000 / 3,000 |
| 30 / 31 / 32 | tint: first word copied / force by bit 2 / never forced | 3,000 / 3,000 / 3,000 |
| 33 / 34 / 35 / 36 | tint: dirty not set / bit 6 not cleared / `+0x24` without bit 7 / CLUT from `+1` | 2,987 / 3,000 / 2,200 / 3,000 |
| 37 / 42 / 43 | tint: column not kept to 8 bits / source column from the target / rows of 128 | 2,161 / 3,000 / 3,000 |
| 38 | tint: bit 7 from the flags read before the `+0x27` store | 106 |
| 39 / 41 / 44 | tint: copied backward / green and blue swapped / 31 records | 2,495 / 3,000 / 2,555 |
| **40** | **tint: red tint read once** | **0 - not refused, expected** |
| 46 / 47 / 48 | party: bit 5 / request read once / count signed | 2,350 / 892 / 566 |
| 49 / 50 / 51 | party: upward / callees swapped / count re-read each turn | 2,128 / 2,590 / 725 |

Controls 14 and 40 change nothing (section 3): no input can tell them apart,
the HANDOFF's "a change that changes nothing". **Control 10** was refused by
comparison - 12 mismatches logged - and then the planted division faulted
(a sign-extended 0xFF divides `0x80000000` by -1), ending the process; the
original's unsigned read cannot fault that way. **Control 38 was refused only
22 times at first**: the sprite pointers aimed at a record's own `+0` too
rarely. A third of them now do; 106.


## 6. What the attract sequence does not reach

- `Field_RunSlot` (no slot in use) and `Sprite_UpdateScreenSlot` (no
  request 3 during the members' update): both in the traced list, 0 calls.
  So `Field_RunSlots` and `Party_UpdateScreens` have only ever run their
  "skip" and `Sprite_UpdateScreen` paths in a check. A slot would need
  whatever sets `Field_Slots` bit 0 (unread); request 3 is the field
  request byte's value 3, whose setter is unread.
- 186 of the effect kinds, and `CameraTurn_States` 3..9. Any scene with
  another effect kind would reach them - the spawners are listed by the scan
  in section 2.
- Whether any tint record is active during the attract cycle is unmeasured:
  `MoveScript_TintFrame` runs every field frame but its copy runs only for a
  record with bit 0. The fades that release records are never reached there
  ([`object-kinds.md`](object-kinds.md)); op C0 or `Sprite_SetTint` would
  start one. A watch of `0x7E0700` in the attract run would answer it.
- The camera turn's divide by zero, and depths 5..7: no spawner seen gives
  a frame count of 0, and no tint record seen has a depth above 4.

## 7. Found on the way

- **A dead store**: `CameraTurn_Start` stores the frame count to
  `MapView_Redraw`, and its only caller, `Effect_CameraTurn`, overwrites it
  with 2 on return. The PSX does the same (`DAT_8014932F`). Harmless; kept.
- **A candidate defect, not acted on**: the tint's 8-bit add wraps. A
  component of 31 plus a tint of `0x61`..`0x7F` is `0x80`..`0x9E`, negative
  as a signed byte, so it clamps to 0 - a colour pushed toward white turns
  black in that channel. The PSX does the same (`iVar8 * 0x1000000 >> 0x18`).
  The fades step the tints by one to 0 or 31, so they never reach it; a
  script op C1 with a large operand would. Whether any script does is
  unmeasured (`tools/movement_scan.py` could list C1 operands).
- The tint depths 5..7 divide by zero, and `CameraTurn_Start` with a frame
  count of 0; both are the original's, both platforms.
- The tint copy's row is a whole byte, and the strip has 32 rows: a CLUT
  number whose row is 32 or more reads and writes the DAT arena after the
  strip (section 5's `0x1DFE0` bytes). D13 in
  [`known-defects.md`](known-defects.md).
- `Party_UpdateScreens` walks past `ObjTrio`'s three members for a count
  above 3, as `Field_MembersFrame` does not (it stops at 3,
  [`field-frame.md`](field-frame.md) section 2). Nothing seen makes the count
  exceed 3.
- For group A: the mode handler `0x56B450` spawns a camera turn at
  `0x56B4C5`; `Field_StatusBits` bit 5 is set for the 64 frames it runs.

## 8. Open

- The batch check in game: frame hash, oracle, captures, with these eight on
  the `--original` list.
- Read `Field_RunSlot` `0x455300` and what sets `Field_Slots`.
- Measure whether the attract cycle has an active tint record.
- The other effect kinds - 97 of them - when a scene reaches them.
