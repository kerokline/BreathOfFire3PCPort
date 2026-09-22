# The field's frame loop

**Status:** IN PROGRESS (2026-09-22) - twelve functions ours, from the
field's per-frame entries down to the overlay list's draw, each fuzzed
against a byte-copy of Capcom's at start-up (`BOF3X_SHADOW=field_frame`,
24,000 rounds, 0 mismatches) with 62 of 63 planted bugs refused. **Not yet
through the batch check in game** (the frame hash, the oracle, the captures);
that runs centrally after the parallel takeovers merge.

With these, the field's frame is ours from its entry to `Sprite_DrawPass`,
except for what the entries call that belongs to other groups: the kind-2
object's runner (`Kind2_Run`), the map (`AreaMap_Frame`,
`AreaMap_HeaderPass`), the object handlers (`Field_ObjectIdle`, `Linked`,
`IdleLong` and the handler table) and the unread callees listed in section 3.
Sprite object offsets used below are those of
[`sprite-draw-order.md`](sprite-draw-order.md) §5 / §6.

## 1. The functions

| PC | name | bytes | PSX twin | how paired | all_b | hidden_b |
|---|---|---|---|---|---|---|
| `0x517200` | `Field_Frame` | 60 | `FUN_8019A1B8` (`GAME.EMI`) | `call-disputed`, confirmed by its 12 calls | 6,709 | 9,168 |
| `0x517240` | `Field_FrameScripted` | 70 | `FUN_8019A230` | `gap67`, confirmed by its 14 calls | - | 2,997 |
| `0x517350` | `Field_MembersFrame` | 139 | `FUN_8019A440` | `callers`, read on both | 8,846 | 12,165 |
| `0x5173E0` | `Field_ObjectsScreen` | 96 | `FUN_801A151C` | `call-anchored`, read on both | 8,846 | 12,165 |
| `0x517490` | `Field_ObjectsFrame` | 419 | `FUN_801A16E0` | `callers`, read on both | 8,846 | 12,165 |
| `0x57B780` | `Party_ExtraScreens` | 45 | `FUN_8015B148` (`SLPS`) | by position (6th call of `FUN_8019A1B8`); its one callee is `0x8015B334` | 8,846 | 12,165 |
| `0x57B7B0` | `Party_ExtraFrame` | 123 | `FUN_8015B1D8` | by position (2nd call); callees `0x801A27A8`, `0x801A28D0`, `0x801A4A10` and one `jalr`, the PC's three and the table | 8,846 | 12,165 |
| `0x57B830` | `Sprite_UpdateScreenA` | 46 | `FUN_8015B334` | `call-disputed`, confirmed: `FUN_801A151C` calls it where the PC calls `0x57B830` | 14,890 | 20,793 |
| `0x57B860` | `Sprite_ProjectA` | 506 | `FUN_8015B394` | `FUN_8015B334`'s one callee, as `0x57B860` is `0x57B830`'s | not an entry | not an entry |
| `0x592F00` | `Field_DrawFrame` | 28 | `FUN_8014B948` | `call-anchored` | 8,846 | 12,165 |
| `0x592F20` | `Sprite_DrawOverlays` | 254 | `FUN_8014BA14` | a callee of `FUN_8014B948`, calling `0x8014B9E0` and `0x8014C3C8` | 8,846 | 12,165 |
| `0x593020` | `Sprite_SwapOverlays` | 54 | `FUN_8014B9E0` | `FUN_8014BA14`'s callee, 52 bytes | not an entry | not an entry |

Sizes are to the last instruction (capstone, 2026-09-22; the padding after
is not counted). Call counts are the traced entries' totals in
`analysis/calltrace/all_b` and `hidden_b` (`caller 0` rows); "not an entry"
means the address was not in the traced list. PSX side from the sibling's
Ghidra output: the `GAME_EMI0_80196800_decomp` bodies for the `GAME.EMI`
functions, and `SLPS_009.90.json`'s callee lists for the boot ones, which the
sibling did not decompile - so the five `SLPS` twins are paired by their call
graphs, not read line by line. Pairs from `analysis/pairs_propagated.json`.

**Callers** (E8 / E9 rel32 scan of the whole image, 2026-09-22):
`Field_Frame` six, two of them tail jumps (`0x496420`, `0x4966CE`);
`Field_FrameScripted` one, the tail jump at `0x495E83`; `Field_MembersFrame`,
`Field_ObjectsScreen` and `Party_ExtraScreens` six each (the other field-frame
variants at `0x517290`..`0x517320` among them); `Field_ObjectsFrame` nine;
`Sprite_UpdateScreenA` ten, two tail jumps; `Field_DrawFrame` ten, seven of
them tail jumps. **`0x57B860` has exactly one: `Sprite_UpdateScreenA`'s tail
`jmp`**, and no pointer to it anywhere in the file - it is a separate
function only in that the PSX has it as one (`FUN_8015B394`), which is why it
is its own entry here. `0x593020` has one call, from `Sprite_DrawOverlays`.

## 2. What each does

**`Field_Frame` / `Field_FrameScripted`**: straight call lists. The field's
frame is: the kind-2 object, the four extra party objects' frames, the three
members', the thirty objects', the map, the extra objects' screen updates,
the members' (`Party_UpdateScreens`), the thirty objects', the effect
objects, `Field_RunSlots`, the tint records, and `Field_DrawFrame` as a tail
jump. `Field_FrameScripted` puts the area's mode dispatch and the task
records in front. The PSX lists are the same, call for call.

**`Field_MembersFrame`**: `Field_PendingJump`, then each of `ObjTrio`'s three
members, as `Field_State` and `Sprite_Current`, through
`Field_LeaderFrame` (the first) or `Field_MemberFrame` (the others) - unless
held by bit n of the low bytes of `Field_ScriptFlags2 | Field_ScriptFlags`.
The second runs only when `Field_MemberCount` is above 1, the third only when
it is exactly 3 (so a count of 4 would run two; the count never exceeds 3 in
anything seen, and the PSX tests the same).

**`Field_ObjectsScreen`**: for each present (`+0` bit 0) object of the 30:
the ordering slot `+0x29` - 4 with `+0x24` bit 4, else `Draw_OtSlot` - then
`Sprite_UpdateScreenA` for type (`+6`) 0x0A, `Sprite_UpdateScreen` for the
rest. **Type 9 is the exception**: skipped entirely under `Draw_OtSlot` 4, and
otherwise updated with whatever `+0x29` it already had.

**`Field_ObjectsFrame`**: nothing when `Field_ScriptFlags2` has bit 10.
Otherwise each present object of the 30 becomes `Sprite_Current` and
`Field_ActiveMember`, scratch byte 3 (`0x903853`, the PSX's `0x1F800003`) is
cleared, and then:

- no timed move (`+9` is 0) and context `+0x80` bit 5: with bit 4,
  `MoveScript_SetTurnTarget(context)` - and if that returns non-zero and
  `Sprite_Current +7` lacks bit 3, `+0x85` takes the direction's low three
  bits and the sprite turns: to its own direction (`+7` bit 1) or to face
  `ObjTrio`'s first member (`(ObjTrio[8] ^ 4) & 7`), through
  `Sprite_FaceDirection`, and `+0x85` gets bit 0 for a sprite whose `+1` is
  0x0A. Without bit 4, `Field_ObjectIdle(object)`.
- otherwise: context bit 0 - `Field_ObjectLinked()`, and **nothing else for
  that object**; `+0x9C` above 0x1E with no timed move -
  `Field_ObjectIdleLong(object)`; else the kind handler,
  `Field_ObjectHandlers[+1](object)` - entry 4 is `Field_ObjectUpdate`, 7
  `Field_ObjectFollow` ([`movement-script.md`](movement-script.md) §1b).
- then, unless `Sprite_Current` (read again) is type 0x0A: context bit 6 -
  one tick of the frame-animation script (`Sprite_ScriptTick` for flag bit 4,
  else `Sprite_ScriptTickOnce`), unless `+7` bit 5; context bit 3 -
  `Sprite_FaceDirection(+8)` and the bit cleared; else, unless `+7` bit 5,
  `max(+0x84 - 2, 0) + 1` ticks - `+0x84` is the byte `Field_ObjectUpdate`
  indexes `Field_MoveSpeeds` by, so the animation's pace follows the move
  speed's index (what that looks like in game is unobserved).

At the end `Field_EdgeBitsPrev` takes `Field_EdgeBits` - the PSX copies
`0x80143F22` to `0x80146874` there; `0x5187C0` XORs the two to find bits that
changed.

**`Party_ExtraScreens` / `Party_ExtraFrame`**: the same for the four
`Sprite_ObjectsExtra`. The screen pass skips an object with flag bit 6 and
calls only `Sprite_UpdateScreenA`. The frame pass is `Field_ObjectsFrame`'s
first half without the idle-long case, the scratch byte, the turn target's
result, or anything after the handler.

**`Sprite_UpdateScreenA` + `Sprite_ProjectA`**: a type-0x0A sprite's
screen update, where `Sprite_UpdateScreen` serves the rest. The slot `+0x29`
is 4 (`+0x24` bit 4), 6 (byte 3 of the record `+0x54` points at has bit 5)
or `Draw_OtSlot`. Then the draw key: the same 16-bit sum as
`Sprite_UpdateScreen`'s but with the record's byte 1 (signed) where that one
has `Sprite_KeyAdjust`, and **no range test** - only the low byte is kept,
shifted up, with the same low byte below it. `Sprite_InheritDrawKey` unless
record byte 3 has bit 4. The projection is the same vertex, through
`Gte_LoadVertex` / `Gte_Rtps` / `Gte_StoreScreenXY` / `Gte_StoreDepthQuarter`
(to `+0x60`) rather than `Gte_RotTransPers`; the float screen point to
`+0x74` / `+0x78` and through the CRT's `_ftol` to `+0x2E` / `+0x30`. Then a
cull `Sprite_UpdateScreen` does not have: outside x `-m1 .. 0x140 + m0`, y
`-m3 .. 0xF0 + m2`, with `m` entry 0 or 1 of `Sprite_ScreenMargins`
(`0x663B28`: 80 on every side, or 100 / 100 / 160 / 100) by record byte 3 bit
3, flag bit 7 is set; inside, it is cleared and the sprite goes on
`Sprite_DrawList` while there is room. No `Port_DroppedCall` here.

**`Field_DrawFrame`**: `AreaMap_HeaderPass`, `Sprite_DrawOverlays`,
`Sprite_DrawPass`, and both list counts to 0. The PSX `FUN_8014B948` also
calls `SetSp` / `GetSp` (the sibling's Psy-Q signature names) - the move to
the scratchpad stack the PC has no use for.

**`Sprite_DrawOverlays`**: with `Draw_PassFlags` bit 0, a bubble sort of
`Sprite_OverlayList` by the s16 height `+0x3E`, smallest first, then for each
entry: `Sprite_Current`, `+0x74` / `+0x78` rewritten as the float of the
integer screen point `+0x2E` / `+0x30`, and `Sprite_Draw`.
**`Sprite_SwapOverlays`** exchanges two entries by byte index.

## 3. The callees, and what is not ours

All of these are called through a `Callees` pointer struct, which the fuzz
swaps for recorders. Group B, C and D functions keep their provisional
names; the ones with no entry before got minimal typed entries (`# group A
callees` in `symbols.toml`), all `hypothesis`:

| PC | name | read |
|---|---|---|
| `0x533760` | `Field_PendingJump` | 7 instructions: `jmp [0x660B60 + 0x904EF1 * 4]` when `0x904EF0` is set |
| `0x52D8F0` | `Field_LeaderFrame` | `Sprite_RestoreClut`, a countdown byte, `Field_CopyInput`, `jmp [0x660918 + +1 * 4]` |
| `0x51AC50` | `Field_MemberFrame` | `Sprite_RestoreClut`, `jmp [0x65F960 + +1 * 4]` |
| `0x531B60` | `Party_UpdateScreens` | the members from the last, `0x588F00` under `Field_Request` 3 else `Sprite_UpdateScreen` |
| `0x494030` | `Effect_RunObjects` | the 20 `Effect_Objects` through `[0x655350 + +5 * 4]` |
| `0x455250` | `Field_RunSlots` | 8 records of 0x10 at `0x9035C0`, `0x455300(i)` for each with bit 0 |
| `0x454AD0` | `MoveScript_TintFrame` | walks `MoveScript_TintRecords`; its head only |
| `0x56D690` | `Field_ModeDispatch` | `call [[0x662C80 + Cond_ByteFA * 4]]`, tail `jmp 0x56D8B0` |
| `0x59E230` | `Field_RunTaskRecords` | 22 records of 0x24 at `0x803160`, three passes by `+0xF`, a local table of nine |

New data: `Field_ObjectHandlers` (`0x65F5F8`, 11 entries, then data),
`Sprite_ScreenMargins` (`0x663B28`), `Field_ScriptFlags2` (`0x905BA4`, the
PSX `0x80146256` beside `Field_ScriptFlags`' `0x80146254`), `Field_EdgeBits`
/ `Field_EdgeBitsPrev` (`0x905B80` / `0x8034E8`).

**`MoveScript_SetTurnTarget` returns a value.** `symbols.toml` types it
`void`, but it returns 0 in `al` when context `+8` is `0xFFFF` (`0x517EAA`)
and 1 after opening a message through `Msg_OpenScript` / `Msg_OpenSystem` and
setting `Field_Request` to 2 (`0x517F21`); `Field_ObjectsFrame` tests it. The
sibling names its twin `0x801A27A8` `Script_ShowMessage`. `field_frame.cpp`
calls it through a cast with the real type rather than retype it under
`move_groups.cpp`; retyping and renaming it is proposed in the report.

## 4. Quirks kept

Each is in the code's comment and each has a negative control (section 5):

- `Field_ObjectsFrame` re-reads `Sprite_Current` and the context byte after
  every callee - after the turn target, after the handler, after
  `Sprite_FaceDirection` (controls 14, 18, 19, 59). A callee that moves
  `Sprite_Current` redirects the rest of that object's turn.
- The tick count is `max(+0x84 - 2, 0) + 1` with `+0x84` read once (20, 21, 22).
- `Field_ObjectLinked` ends the object's turn with no ticks and no turn (15).
- Type 9 keeps its slot byte; skipped under `Draw_OtSlot` 4 (7, 8).
- `Field_MembersFrame` re-reads the flags and the count after each call (5).
- `Sprite_ProjectA`: 16-bit key, low byte only, no range test; every step
  reads `Sprite_Current` afresh (38); the height halved toward zero (29); the
  float stored as the x87 moves it (33) and truncated by `_ftol` (34); the
  margins' edges inclusive as the original compares them (30, 31, 32).
- `Sprite_DrawOverlays`: a stable sort (42) on signed heights (43); the count
  re-read after every exchange (44) and every draw (45); the floats rewritten
  from the integers (48).
- `Sprite_SwapOverlays` masks its arguments to bytes: its caller pushes whole
  registers with stale upper bytes (`0x592F54` loads a dword of which only the
  low byte was stored). Ours takes dwords and masks, so it is safe under
  `BOF3X_ORIGINAL=Sprite_DrawOverlays` too.

**Not reproduced, because unobservable:**

- `Field_ObjectsFrame` at `0x517518..0x517529` compares the sprite's direction
  with `ObjTrio`'s, `setne`s the answer, XORs it with 4 and branches to skip
  the turn if the result is 0 - which it never is (0 ^ 4 or 1 ^ 4). A dead
  test with no effect; ours has no branch. (The PSX `FUN_801A16E0` has no
  such test either.)
- The vertex's fourth word in `Sprite_ProjectA`: the original never writes
  it, so `Gte_LoadVertex` copies stale stack into the top half of
  `Gte_Vertices[3]`. Ours writes 0, as DIV-0023 decided for `MapCell_DrawQuads`'
  vertices - **DIV-0023's entry should name `0x57B860` too** (the ledger is
  the coordinator's to edit). Nothing reads that half word (DIV-0023's
  `pe_xref` measurement).

## 5. The fuzz, and what it cannot see

`BOF3X_SHADOW=field_frame`: twelve byte-copies (`CloneOriginal`), every call
out re-aimed at a recorder - the copies' calls of one another included, so
each function is tested alone - except `Sprite_ProjectA`'s two calls of the
CRT's `_ftol`, which the copy keeps. The handler table's eleven entries are
recorders for the test's duration, for both sides. `CloneOriginal` refused
entries that begin with `E8` as "already patched"; four of these begin with
their own call, so it now accepts an `E8` at offset 0 when the call list
re-aims offset 0 (our patches are `E9`, the tracer's `CC`).

One round: one of the twelve; every byte of the 30 objects, the 4 extra,
`ObjTrio`, both sprite lists, the flag words, counts and draw bytes random,
then each object's tested bytes seeded (present or not, types 9 / 0x0A,
handler index within the table, `+9` zero half the time, context bit 5 half
the time, `+0x84` mostly 0..4, `+0x9C` at 0x1D..0x20, heights from a few
values for ties and the s16 edges, `+0x54` at one of eight records); list
counts at 0, 1, 2 and near their limits. Recorders log their id, the current
sprite and their arguments, and disturb what the callers re-read (the current
sprite and its type, flags and direction; the loop object's context and
speed; the member flags and count; the list counts). The screen-point
recorder returns NaNs, infinities, values past 2^63, and points on and one
either side of the cull's edges with fractions of 0, 1/2 and 0.999 either
way. Theirs, then ours from the same state; everything above and the log
compared.

```
shadow      field_frame self-test: 24000 rounds (2000 per function), 301853 calls to the stand-ins, 48642 exchanges in the sort, Sprite_ProjectA 1129 shown / 871 culled, 0 MISMATCHES; ...
```

With `BOF3X_SHADOW='*'` every one of the 49 self-tests passes with this
module injected last (exit 0).

**Negative controls** - a bug planted in ours, the build re-run, the test
refused (exit 3), the bug removed - 62 of 63 refused:

| # | planted | mismatches |
|---|---|---|
| 0 / 1 | two of `Field_Frame`'s calls swapped / `Field_FrameScripted`'s task records dropped | 2000 / 2000 |
| 2 / 3 / 4 | members: `<= 1` as `< 1` / `!= 3` as `< 3` / the third tested by bit 1 | 201 / 190 / 120 |
| 5 / 6 / 53 / 57 | flags read once / second `Field_State` unset / leader for the second / pending after | 65 / 474 / 585 / 2000 |
| 7 / 8 / 9 | type 9 given a slot / not skipped under slot 4 / bit 4 slot 5 | 1137 / 858 / 1999 |
| 10 / 11 | flag `0x400` as `0x200` / scratch byte left | 1011 / 1830 |
| 12 / 13 / 50 / 54 | turn result ignored / face without `^ 4` / own direction unmasked / `+0x85` bit 1 | 1340 / 876 / 845 / 583 |
| 14 / 18 / 19 / 59 | a sprite or context not re-read after a callee (four places) | 134 / 1662 / 629 / 641 |
| 58 | the turn target given the object, not its context | 1824 |
| 15 / 16 / 17 | linked falls through / idle-long at `>= 0x1E` / idle-long with a timed move | 1830 / 232 / 1766 |
| 20 / 21 / 22 / 24 / 51 | one tick fewer / `+0x84` re-read per tick / clamp at -1 / tick kinds swapped / `+7` bit 5 ignored | 1342 / 624 / 305 / 1675 / 1699 |
| 23 | edge bits not copied | 1831 |
| 25 / 26 / 27 / 52 | extra: bit 6 ignored / active member unset / linked by bit 1 / turn target by bit 3 | 1701 / 1969 / 1183 / 1078 |
| 28 | slot 7 for record bit 5 | 471 |
| 29 / 60 | height halved by shift / not negated | 359 / 1569 |
| 30 / 31 / 32 / 62 | right edge inclusive / top margin from byte 2 / margins by bit 4 / shown keeps bit 7 | 106 / 50 / 340 / 558 |
| 33 / 34 | no x87 NaN quieting / `_ftol` rounding half away | 63 / 1432 |
| 35 / 39 / 61 | record byte 1 not added / x whole test dropped / slot-4 low byte from z always | 1994 / 675 / 817 |
| 36 / 37 / 38 | list limit `0x29` / inherit skipped by bit 5 / depth address not re-read | 160 / 977 / 764 |
| 41 | draw count not reset | 1985 |
| 42 / 43 / 55 / 56 | unstable / unsigned compare / inner start one lower / one pass fewer | 981 / 1232 / 1053 / 123 |
| 44 / 45 | count not re-read after an exchange / after a draw | 951 / 548 |
| 46 / 47 / 48 / 49 | pass flag bit 1 / exchange arguments swapped / y float from x / one exchange store dropped | 909 / 1244 / 1488 / 957 |
| **40** | **`Word(+0x32) << 8` for `byte(+0x32) << 8`** | **0 - not refused, expected** |

Control 40 changes nothing: the store is 16 bits, so the word shifted up
keeps exactly the low byte - no input can tell them apart
(HANDOFF's "a change that changes nothing").

**Control 59 was not refused at first**, and the reason was the fuzz: the turn
target's stand-in took its result from the same hash bits its disturbance
used, so every call that moved `Sprite_Current` returned 0 and nothing read
the moved sprite. The result now comes from the hash's top bit; 641 refused.
(The HANDOFF's quiet-stand-in trap in a new form: correlated side effects.)

**What the fuzz cannot see:**

- The callees themselves - every one is a recorder. Whether
  `Field_ObjectHandlers`, the GTE and `Sprite_Draw` do the right thing with
  what these hand them is theirs to test; that the calls, their order and
  arguments are the original's is what this test shows.
- A handler index past the table's 11 entries: the fuzz never makes one (the
  original would call whatever dword follows). Ours indexes the same memory.
- `_ftol` itself: the copy calls Capcom's, ours reproduces it
  (`Ftol16`, as `sprite_screen.cpp`'s).
- The real game's reach: the attract cycle's call counts are above, but
  **`Sprite_SwapOverlays` is not a traced entry**, so whether the attract
  sequence ever has two overlay sprites out of height order is unmeasured,
  and `Field_FrameScripted` was traced only in `hidden_b`.

## 6. Defects of the 2001 code

None found that changes behaviour. Observations, for the record:

- `Sprite_ProjectA` keeps only the low byte of its draw key and never
  range-tests it, where `Sprite_UpdateScreen` hides a sprite whose key is
  outside 0..0x36. A type-0x0A sprite far enough off the view's cells would
  get a wrapped layer. Unread on the PSX side (`FUN_8015B394` is not
  decompiled by the sibling), so whether that is the port's or the
  original's is open.
- `Field_MembersFrame` runs two members for a count of 4 or more; the PSX
  does the same, and nothing seen makes more than 3.
- The dead test of section 4 and the stale vertex word are the 2001
  compiler's and the 2001 code's, and harmless.

## 7. Open

- The batch check in game: frame hash, oracle, captures, with these twelve
  on the `--original` list.
- Read `FUN_8015B394` (MIPS, 744 bytes) for the key's range test.
- Retype `MoveScript_SetTurnTarget`'s return and consider the sibling's name.
